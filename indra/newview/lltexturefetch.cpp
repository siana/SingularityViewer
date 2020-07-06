/**
 * @file lltexturefetch.cpp
 * @brief Object which fetches textures from the cache and/or network
 *
 * $LicenseInfo:firstyear=2000&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 * 
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include <iostream>

#include <boost/regex.hpp>
#include <boost/lexical_cast.hpp>

#include "llstl.h"
#include "message.h"

#include "lltexturefetch.h"

#include "aicurl.h"
#include "lldir.h"
#include "llhttpclient.h"
#include "llhttpstatuscodes.h"
#include "llimage.h"
#include "llimagej2c.h"
#include "llimageworker.h"
#include "llworkerthread.h"
#include "message.h"

#include "llagent.h"
#include "lltexturecache.h"
#include "llviewercontrol.h"
#include "llviewertexturelist.h"
#include "llviewertexture.h"
#include "llviewerregion.h"
#include "llviewerstats.h"
#include "llviewerstatsrecorder.h"
#include "llviewerassetstats.h"
#include "llworld.h"
#include "llsdutil.h"
#include "llstartup.h"
#include "llsdserialize.h"
#include "llbuffer.h"
#include "llhttpretrypolicy.h"
#include "hippogridmanager.h"

#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>

class AIHTTPTimeoutPolicy;
extern AIHTTPTimeoutPolicy HTTPGetResponder_timeout;
extern AIHTTPTimeoutPolicy lcl_responder_timeout;
extern AIHTTPTimeoutPolicy assetReportHandler_timeout;

LLStat LLTextureFetch::sCacheHitRate("texture_cache_hits", 128);
LLStat LLTextureFetch::sCacheReadLatency("texture_cache_read_latency", 128);

//////////////////////////////////////////////////////////////////////////////
// Log scope
static const char * const LOG_TXT = "Texture";

class LLTextureFetchWorker : public LLWorkerClass
{
	friend class LLTextureFetch;
	friend class HTTPGetResponder;
	
private:
	class CacheReadResponder : public LLTextureCache::ReadResponder
	{
	public:
		CacheReadResponder(LLTextureFetch* fetcher, const LLUUID& id, LLImageFormatted* image)
			: mFetcher(fetcher), mID(id)
		{
			setImage(image);
		}
		virtual void completed(bool success)
		{
			LLTextureFetchWorker* worker = mFetcher->getWorker(mID);
			if (worker)
			{
 				worker->callbackCacheRead(success, mFormattedImage, mImageSize, mImageLocal);
			}
		}
	private:
		LLTextureFetch* mFetcher;
		LLUUID mID;
	};

	class CacheWriteResponder : public LLTextureCache::WriteResponder
	{
	public:
		CacheWriteResponder(LLTextureFetch* fetcher, const LLUUID& id)
			: mFetcher(fetcher), mID(id)
		{
		}
		virtual void completed(bool success)
		{
			LLTextureFetchWorker* worker = mFetcher->getWorker(mID);
			if (worker)
			{
				worker->callbackCacheWrite(success);
			}
		}
	private:
		LLTextureFetch* mFetcher;
		LLUUID mID;
	};
	
	class DecodeResponder : public LLImageDecodeThread::Responder
	{
	public:
		DecodeResponder(LLTextureFetch* fetcher, const LLUUID& id, LLTextureFetchWorker* worker)
			: mFetcher(fetcher), mID(id), mWorker(worker)
		{
		}
		virtual void completed(bool success, LLImageRaw* raw, LLImageRaw* aux)
		{
			LLTextureFetchWorker* worker = mFetcher->getWorker(mID);
			if (worker)
			{
 				worker->callbackDecoded(success, raw, aux);
			}
		}
	private:
		LLTextureFetch* mFetcher;
		LLUUID mID;
		LLTextureFetchWorker* mWorker; // debug only (may get deleted from under us, use mFetcher/mID)
	};

	struct Compare
	{
		// lhs < rhs
		bool operator()(const LLTextureFetchWorker* lhs, const LLTextureFetchWorker* rhs) const
		{
			// greater priority is "less"
			const F32 lpriority = lhs->mImagePriority;
			const F32 rpriority = rhs->mImagePriority;
			if (lpriority > rpriority) // higher priority
				return true;
			else if (lpriority < rpriority)
				return false;
			else
				return lhs < rhs;
		}
	};
	
public:
	/*virtual*/ bool doWork(S32 param); // Called from LLWorkerThread::processRequest()
	/*virtual*/ void finishWork(S32 param, bool completed); // called from finishRequest() (WORK THREAD)
	/*virtual*/ bool deleteOK(); // called from update() (WORK THREAD)

	~LLTextureFetchWorker();
	// void relese() { --mActiveCount; }

	S32 callbackHttpGet(U32 offset, U32 length,
						 const LLChannelDescriptors& channels,
						 const LLHTTPClient::ResponderBase::buffer_ptr_t& buffer,
						 bool partial, bool success);
	void callbackCacheRead(bool success, LLImageFormatted* image,
						   S32 imagesize, BOOL islocal);
	void callbackCacheWrite(bool success);
	void callbackDecoded(bool success, LLImageRaw* raw, LLImageRaw* aux);
	
	void setGetStatus(U32 status, const std::string& reason)
	{
		LLMutexLock lock(&mWorkMutex);

		mGetStatus = status;
		mGetReason = reason;
	}

	void setCanUseHTTP(bool can_use_http) { mCanUseHTTP = can_use_http; }
	bool getCanUseHTTP() const { return mCanUseHTTP; }

	LLTextureFetch & getFetcher() { return *mFetcher; }
	
protected:
	LLTextureFetchWorker(LLTextureFetch* fetcher, FTType f_type,
						 const std::string& url, const LLUUID& id, const LLHost& host,
						 F32 priority, S32 discard, S32 size);

private:
	/*virtual*/ void startWork(S32 param); // called from addWork() (MAIN THREAD)
	/*virtual*/ void endWork(S32 param, bool aborted); // called from doWork() (MAIN THREAD)

	void resetFormattedData();
	
	void setImagePriority(F32 priority);
	void setDesiredDiscard(S32 discard, S32 size);
	bool insertPacket(S32 index, U8* data, S32 size);
	void clearPackets();
	void setupPacketData();
	U32 calcWorkPriority();
	void removeFromCache();
	bool processSimulatorPackets();
	bool writeToCacheComplete();
	
	// Threads:  Ttf
	void recordTextureStart(bool is_http);

	// Threads:  Ttf
	void recordTextureDone(bool is_http);

	void lockWorkMutex() { mWorkMutex.lock(); }
	void unlockWorkMutex() { mWorkMutex.unlock(); }

private:
	enum e_state // mState
	{
		// NOTE: Affects LLTextureBar::draw in lltextureview.cpp (debug hack)
		INVALID = 0,
		INIT,
		LOAD_FROM_TEXTURE_CACHE,
		CACHE_POST,
		LOAD_FROM_NETWORK,
		LOAD_FROM_SIMULATOR,
		SEND_UDP_REQ,
		WAIT_UDP_REQ,
		SEND_HTTP_REQ,
		WAIT_HTTP_REQ,
		DECODE_IMAGE,
		DECODE_IMAGE_UPDATE,
		WRITE_TO_CACHE,
		WAIT_ON_WRITE,
		DONE
	};
	enum e_request_state // mSentRequest
	{
		UNSENT = 0,
		QUEUED = 1,
		SENT_SIM = 2
	};
	enum e_write_to_cache_state //mWriteToCacheState
	{
		NOT_WRITE = 0,
		CAN_WRITE = 1,
		SHOULD_WRITE = 2
	};
	static const char* sStateDescs[];
	e_state mState;
	void setState(e_state new_state);
	e_write_to_cache_state mWriteToCacheState;
	LLTextureFetch* mFetcher;
	LLPointer<LLImageFormatted> mFormattedImage;
	LLPointer<LLImageRaw>       mRawImage,
								mAuxImage;
	const FTType mFTType;
	LLUUID mID;
	LLHost mHost;
	std::string mUrl;
	AIPerServicePtr mPerServicePtr;		// Pointer to the AIPerService corresponding to the host of mUrl.
	U8 mType;
	F32 mImagePriority;
	U32 mWorkPriority;
	F32 mRequestedPriority;
	S32                         mDesiredDiscard,
								mSimRequestedDiscard,
								mRequestedDiscard,
								mLoadedDiscard,
								mDecodedDiscard;
	LLFrameTimer                mRequestedTimer,
								mFetchTimer;
	LLTimer			mCacheReadTimer;
	F32				mCacheReadTime;
	LLTextureCache::handle_t    mCacheReadHandle,
								mCacheWriteHandle;
	std::vector<U8> mHttpBuffer;
	S32                         mRequestedSize,
								mRequestedOffset,
								mDesiredSize,
								mFileSize,
								mCachedSize;
	e_request_state mSentRequest;
	handle_t mDecodeHandle;
	BOOL mLoaded;
	BOOL mDecoded;
	BOOL mWritten;
	BOOL mNeedsAux;
	BOOL mHaveAllData;
	BOOL mInLocalCache;
	bool                        mCanUseHTTP,
								mCanUseNET ; //can get from asset server.
	S32 mHTTPFailCount;
	S32 mRetryAttempt;
	S32 mActiveCount;
	U32 mGetStatus;
	std::string mGetReason;
	
	// Work Data
	LLMutex mWorkMutex;
	struct PacketData
	{
		PacketData(U8* data, S32 size) 
		:	mData(data), mSize(size) 
		{}
		~PacketData() { clearData(); }
		void clearData() { delete[] mData; mData = NULL; }

		U8* mData;
		U32 mSize;
	};
	std::vector<PacketData*> mPackets;
	S32 mFirstPacket;
	S32 mLastPacket;
	U16 mTotalPackets;
	U8 mImageCodec;

	LLViewerAssetStats::duration_t mMetricsStartTime;
	U32						mHttpReplySize,				// Actual received data size
							mHttpReplyOffset;			// Actual received data offset
	// State history
	U32						mCacheReadCount,
							mCacheWriteCount;
};

//////////////////////////////////////////////////////////////////////////////
class HTTPGetResponder : public LLHTTPClient::ResponderWithCompleted
{
	LOG_CLASS(HTTPGetResponder);
public:
	HTTPGetResponder( FTType f_type, LLTextureFetch* fetcher, const LLUUID& id, U64 startTime, S32 requestedSize, U32 offset)
		: mFetcher(fetcher)
		, mID(id)
		, mMetricsStartTime(startTime)
		, mRequestedSize(requestedSize)
		, mRequestedOffset(offset)
		, mReplyOffset(0)
		, mReplyLength(0)
		, mReplyFullLength(0)
		, mFTType(f_type)
	{
		mFetchRetryPolicy = new LLAdaptiveRetryPolicy(10.0,3600.0,2.0,10);
	}
	~HTTPGetResponder()
	{
	}

	/*virtual*/ bool needsHeaders(void) const { return true; }

	/*virtual*/ void completedHeaders(void) {
		LL_DEBUGS("Texture") << "HTTP HEADERS COMPLETE: " << mID << LL_ENDL;

		std::string	rangehdr;
		if (mReceivedHeaders.getFirstValue("content-range", rangehdr))
		{
			std::vector<std::string> tokens;
			boost::split(tokens,rangehdr,boost::is_any_of(" -/"));
			if(tokens.size() == 4 && !stricmp(tokens[0].c_str(),"bytes"))
			{
				U32 first(0), last(0), len(0);
				try
				{
					first = boost::lexical_cast<U32>(tokens[1].c_str());
					last = boost::lexical_cast<U32>(tokens[2].c_str());
				}
				catch( boost::bad_lexical_cast& )
				{
					return;
				}
				if(tokens[3] != "*")
				{
					try
					{
						len = boost::lexical_cast<U32>(tokens[3].c_str());
					}
					catch( boost::bad_lexical_cast& )
					{
						len = 0;
					}
				}
				if(first <= last && (!len || last < len))
				{
					mReplyOffset = first;
					mReplyLength = last - first + 1;
					mReplyFullLength = len;
					LL_DEBUGS("Texture") << " mReplyOffset=" << mReplyOffset << " mReplyLength=" << mReplyLength << " mReplyFullLength=" << mReplyFullLength << LL_ENDL;
				}
			}
		}
	}

	/*virtual*/ void completedRaw(LLChannelDescriptors const& channels,
								  buffer_ptr_t const& buffer)
	{
		static LLCachedControl<bool> log_to_viewer_log(gSavedSettings,"LogTextureDownloadsToViewerLog");
		static LLCachedControl<bool> log_to_sim(gSavedSettings,"LogTextureDownloadsToSimulator");
		static LLCachedControl<bool> log_texture_traffic(gSavedSettings,"LogTextureNetworkTraffic") ;
		
		if (log_to_viewer_log || log_to_sim)
		{
			mFetcher->mTextureInfo.setRequestStartTime(mID, mMetricsStartTime);
			mFetcher->mTextureInfo.setRequestType(mID, LLTextureInfoDetails::REQUEST_TYPE_HTTP);
			mFetcher->mTextureInfo.setRequestSize(mID, mRequestedSize);
			mFetcher->mTextureInfo.setRequestOffset(mID, mRequestedOffset);
			mFetcher->mTextureInfo.setRequestCompleteTimeAndLog(mID, LLTimer::getTotalTime());
		}

		LL_DEBUGS("Texture") << "HTTP COMPLETE: " << mID << LL_ENDL;
		LLTextureFetchWorker* worker = mFetcher->getWorker(mID);
		if (worker)
		{
			worker->lockWorkMutex();
			bool success = false;
			bool partial = false;
			if (HTTP_OK <= mStatus &&  mStatus < HTTP_MULTIPLE_CHOICES)
			{
				mFetchRetryPolicy->onSuccess();
				success = true;
				if (HTTP_PARTIAL_CONTENT == mStatus) // partial information
				{
					partial = true;
				}
			}
			if (!success)
			{
				if(mFTType == FTT_SERVER_BAKE)
				{
					mFetchRetryPolicy->onFailure(getStatus(), getResponseHeaders());
					F32 retry_after;
					if (mFetchRetryPolicy->shouldRetry(retry_after))
					{
						LL_INFOS(LOG_TXT) << mID << " will retry after " << retry_after << " seconds, resetting state to LOAD_FROM_NETWORK" << LL_ENDL;
						mFetcher->removeFromHTTPQueue(mID, 0);
						worker->setGetStatus(mStatus, mReason);
						worker->setState(LLTextureFetchWorker::LOAD_FROM_NETWORK);
						worker->unlockWorkMutex();
						return;
					}
				}
				worker->setGetStatus(mStatus, mReason);
				if (mFTType != FTT_MAP_TILE) // missing map tiles are normal, don't complain about them.
				{
					LL_WARNS(LOG_TXT) << "CURL GET FAILED, status:" << mStatus
									  << " reason: " << mReason << LL_ENDL;
				}
			}
			S32BytesImplicit data_size = worker->callbackHttpGet(mReplyOffset, mReplyLength, channels, buffer, partial, success);
			
			if(log_texture_traffic && data_size > 0)
			{
				// one worker per multiple textures
				std::vector<LLViewerTexture*> textures;
				LLViewerTextureManager::findTextures(mID, textures);
				std::vector<LLViewerTexture*>::iterator iter = textures.begin();
				while (iter != textures.end())
				{
					LLViewerTexture* tex = *iter++;
					if (tex)
					{
						gTotalTextureBytesPerBoostLevel[tex->getBoostLevel()] += data_size;
					}
				}
			}

			mFetcher->removeFromHTTPQueue(mID, data_size);
			worker->recordTextureDone(true);
			worker->unlockWorkMutex();
		}
		else
		{
			mFetcher->removeFromHTTPQueue(mID);
 			LL_WARNS() << "Worker not found: " << mID << LL_ENDL;
		}
	}
	
	/*virtual*/ AICapabilityType capability_type(void) const { return cap_texture; }
	/*virtual*/ AIHTTPTimeoutPolicy const& getHTTPTimeoutPolicy(void) const { return HTTPGetResponder_timeout; }
	/*virtual*/ char const* getName(void) const { return "HTTPGetResponder"; }

private:

	LLTextureFetch* mFetcher;
	LLUUID mID;
	const FTType mFTType;
	LLPointer<LLHTTPRetryPolicy> mFetchRetryPolicy;
	U64 mMetricsStartTime;
	S32 mRequestedSize;
	U32 mRequestedOffset;
	U32 mReplyOffset;
	U32 mReplyLength;
	U32 mReplyFullLength;

};

//////////////////////////////////////////////////////////////////////////////

class SGHostBlackList{
	static const int MAX_ERRORCOUNT = 20;
	struct BlackListEntry {
		std::string host;
		U64 timeUntil;
		U32 reason;
		int errorCount;
	};

	typedef std::vector<BlackListEntry> blacklist_t;
	//Why is it a vector? because using std::map for just 1-2 values is insane-ish.
	typedef blacklist_t::iterator iter;
	static blacklist_t blacklist;

	static bool is_obsolete(BlackListEntry entry) {
			U64 now = LLTimer::getTotalTime();
			return (now > entry.timeUntil);
	} //should make a functor. if i cared.

	static void cleanup() {
		(void)std::remove_if(blacklist.begin(), blacklist.end(), is_obsolete);
	}

	static iter find(std::string host) {
		cleanup();
		for(blacklist_t::iterator i = blacklist.begin(); i != blacklist.end(); ++i) {
			if (i->host.find(host) == 0) return i;
		}
		return blacklist.end();
	}

public:
	static bool isBlacklisted(std::string url) {
		iter found = find(url);
		bool r = (found != blacklist.end()) && (found->errorCount > MAX_ERRORCOUNT);
		return r;
	}

	static void add(std::string url, float timeout, U32 reason) {
		LL_WARNS() << "Requested adding to blacklist: " << url << LL_ENDL;
		BlackListEntry entry;
		entry.host = url.substr(0, url.rfind("/"));
		if (entry.host.empty()) return;
		entry.timeUntil = LLTimer::getTotalTime() + timeout*1000;
		entry.reason = reason;
		entry.errorCount = 0;
		iter found = find(entry.host);
		if(found != blacklist.end()) {
			entry.errorCount = found->errorCount + 1;
			*found = entry;
			if (entry.errorCount > MAX_ERRORCOUNT) {
				std::string s;
				microsecondsToTimecodeString(entry.timeUntil, s);
				LL_WARNS() << "Blacklisting address " << entry.host
					<< "is blacklisted for " << timeout 
					<< " seconds because of error " << reason
					<< LL_ENDL;
			}
		}
		else blacklist.push_back(entry);
	}
};

SGHostBlackList::blacklist_t SGHostBlackList::blacklist;

//////////////////////////////////////////////////////////////////////////////

// Cross-thread messaging for asset metrics.

/**
 * @brief Base class for cross-thread requests made of the fetcher
 *
 * I believe the intent of the LLQueuedThread class was to
 * have these operations derived from LLQueuedThread::QueuedRequest
 * but the texture fetcher has elected to manage the queue
 * in its own manner.  So these are free-standing objects which are
 * managed in simple FIFO order on the mCommands queue of the
 * LLTextureFetch object.
 *
 * What each represents is a simple command sent from an
 * outside thread into the TextureFetch thread to be processed
 * in order and in a timely fashion (though not an absolute
 * higher priority than other operations of the thread).
 * Each operation derives a new class from the base customizing
 * members, constructors and the doWork() method to effect
 * the command.
 *
 * The flow is one-directional.  There are two global instances
 * of the LLViewerAssetStats collector, one for the main program's
 * thread pointed to by gViewerAssetStatsMain and one for the
 * TextureFetch thread pointed to by gViewerAssetStatsThread1.
 * Common operations has each thread recording metrics events
 * into the respective collector unconcerned with locking and
 * the state of any other thread.  But when the agent moves into
 * a different region or the metrics timer expires and a report
 * needs to be sent back to the grid, messaging across threads
 * is required to distribute data and perform global actions.
 * In pseudo-UML, it looks like:
 *
 *                       Main                 Thread1
 *                        .                      .
 *                        .                      .
 *                     +-----+                   .
 *                     | AM  |                   .
 *                     +--+--+                   .
 *      +-------+         |                      .
 *      | Main  |      +--+--+                   .
 *      |       |      | SRE |---.               .
 *      | Stats |      +-----+    \              .
 *      |       |         |        \  (uuid)  +-----+
 *      | Coll. |      +--+--+      `-------->| SR  |
 *      +-------+      | MSC |                +--+--+
 *         | ^         +-----+                   |
 *         | |  (uuid)  / .                   +-----+ (uuid)
 *         |  `--------'  .                   | MSC |---------.
 *         |              .                   +-----+         |
 *         |           +-----+                   .            v
 *         |           | TE  |                   .        +-------+
 *         |           +--+--+                   .        | Thd1  |
 *         |              |                      .        |       |
 *         |           +-----+                   .        | Stats |
 *          `--------->| RSC |                   .        |       |
 *                     +--+--+                   .        | Coll. |
 *                        |                      .        +-------+
 *                     +--+--+                   .            |
 *                     | SME |---.               .            |
 *                     +-----+    \              .            |
 *                        .        \ (clone)  +-----+         |
 *                        .         `-------->| SM  |         |
 *                        .                   +--+--+         |
 *                        .                      |            |
 *                        .                   +-----+         |
 *                        .                   | RSC |<--------'
 *                        .                   +-----+
 *                        .                      |
 *                        .                   +-----+
 *                        .                   | CP  |--> HTTP POST
 *                        .                   +-----+
 *                        .                      .
 *                        .                      .
 *
 *
 * Key:
 *
 * SRE - Set Region Enqueued.  Enqueue a 'Set Region' command in
 *       the other thread providing the new UUID of the region.
 *       TFReqSetRegion carries the data.
 * SR  - Set Region.  New region UUID is sent to the thread-local
 *       collector.
 * SME - Send Metrics Enqueued.  Enqueue a 'Send Metrics' command
 *       including an ownership transfer of a cloned LLViewerAssetStats.
 *       TFReqSendMetrics carries the data.
 * SM  - Send Metrics.  Global metrics reporting operation.  Takes
 *       the cloned stats from the command, merges it with the
 *       thread's local stats, converts to LLSD and sends it on
 *       to the grid.
 * AM  - Agent Moved.  Agent has completed some sort of move to a
 *       new region.
 * TE  - Timer Expired.  Metrics timer has expired (on the order
 *       of 10 minutes).
 * CP  - CURL Post
 * MSC - Modify Stats Collector.  State change in the thread-local
 *       collector.  Typically a region change which affects the
 *       global pointers used to find the 'current stats'.
 * RSC - Read Stats Collector.  Extract collector data cloning it
 *       (i.e. deep copy) when necessary.
 *
 */
class LLTextureFetch::TFRequest // : public LLQueuedThread::QueuedRequest
{
public:
	// Default ctors and assignment operator are correct.

	virtual ~TFRequest()
		{}

	// Patterned after QueuedRequest's method but expected behavior
	// is different.  Always expected to complete on the first call
	// and work dispatcher will assume the same and delete the
	// request after invocation.
	virtual bool doWork(LLTextureFetch * fetcher) = 0;
};

namespace 
{

/**
 * @brief Implements a 'Set Region' cross-thread command.
 *
 * When an agent moves to a new region, subsequent metrics need
 * to be binned into a new or existing stats collection in 1:1
 * relationship with the region.  We communicate this region
 * change across the threads involved in the communication with
 * this message.
 *
 * Corresponds to LLTextureFetch::commandSetRegion()
 */
class TFReqSetRegion : public LLTextureFetch::TFRequest
{
public:
	TFReqSetRegion(U64 region_handle)
		: LLTextureFetch::TFRequest(),
		  mRegionHandle(region_handle)
		{}
	TFReqSetRegion & operator=(const TFReqSetRegion &);	// Not defined

	virtual ~TFReqSetRegion()
		{}

	virtual bool doWork(LLTextureFetch * fetcher);
		
public:
	const U64 mRegionHandle;
};


/**
 * @brief Implements a 'Send Metrics' cross-thread command.
 *
 * This is the big operation.  The main thread gathers metrics
 * for a period of minutes into LLViewerAssetStats and other
 * objects then makes a snapshot of the data by cloning the
 * collector.  This command transfers the clone, along with a few
 * additional arguments (UUIDs), handing ownership to the
 * TextureFetch thread.  It then merges its own data into the
 * cloned copy, converts to LLSD and kicks off an HTTP POST of
 * the resulting data to the currently active metrics collector.
 *
 * Corresponds to LLTextureFetch::commandSendMetrics()
 */
class TFReqSendMetrics : public LLTextureFetch::TFRequest
{
public:
    /**
	 * Construct the 'Send Metrics' command to have the TextureFetch
	 * thread add and log metrics data.
	 *
	 * @param	caps_url		URL of a "ViewerMetrics" Caps target
	 *							to receive the data.  Does not have to
	 *							be associated with a particular region.
	 *
	 * @param	session_id		UUID of the agent's session.
	 *
	 * @param	agent_id		UUID of the agent.  (Being pure here...)
	 *
	 * @param	main_stats		Pointer to a clone of the main thread's
	 *							LLViewerAssetStats data.  Thread1 takes
	 *							ownership of the copy and disposes of it
	 *							when done.
	 */
	TFReqSendMetrics(const std::string & caps_url,
					 const LLUUID & session_id,
					 const LLUUID & agent_id,
					 LLViewerAssetStats * main_stats)
		: LLTextureFetch::TFRequest(),
		  mCapsURL(caps_url),
		  mSessionID(session_id),
		  mAgentID(agent_id),
		  mMainStats(main_stats)
		{}
	TFReqSendMetrics & operator=(const TFReqSendMetrics &);	// Not defined

	virtual ~TFReqSendMetrics();

	virtual bool doWork(LLTextureFetch * fetcher);
		
public:
	const std::string mCapsURL;
	const LLUUID mSessionID;
	const LLUUID mAgentID;
	LLViewerAssetStats * mMainStats;
};

/*
 * Examines the merged viewer metrics report and if found to be too long,
 * will attempt to truncate it in some reasonable fashion.
 *
 * @param		max_regions		Limit of regions allowed in report.
 *
 * @param		metrics			Full, merged viewer metrics report.
 *
 * @returns		If data was truncated, returns true.
 */
bool truncate_viewer_metrics(int max_regions, LLSD & metrics);

} // end of anonymous namespace


//////////////////////////////////////////////////////////////////////////////

//static
const char* LLTextureFetchWorker::sStateDescs[] = {
	"INVALID",
	"INIT",
	"LOAD_FROM_TEXTURE_CACHE",
	"CACHE_POST",
	"LOAD_FROM_NETWORK",
	"LOAD_FROM_SIMULATOR",
	"SEND_UDP_REQ",
	"WAIT_UDP_REQ",
	"SEND_HTTP_REQ",
	"WAIT_HTTP_REQ",
	"DECODE_IMAGE",
	"DECODE_IMAGE_UPDATE",
	"WRITE_TO_CACHE",
	"WAIT_ON_WRITE",
	"DONE"
};

// static
volatile bool LLTextureFetch::svMetricsDataBreak(true);	// Start with a data break

// called from MAIN THREAD

LLTextureFetchWorker::LLTextureFetchWorker(LLTextureFetch* fetcher,
										   FTType f_type, // Fetched image type
										   const std::string& url, // Optional URL
										   const LLUUID& id,	// Image UUID
										   const LLHost& host,	// Simulator host
										   F32 priority,		// Priority
										   S32 discard,			// Desired discard
										   S32 size)			// Desired size
	: LLWorkerClass(fetcher, "TextureFetch"),
	  mState(INIT),
	  mWriteToCacheState(NOT_WRITE),
	  mFetcher(fetcher),
	  mFTType(f_type),
	  mID(id),
	  mHost(host),
	  mUrl(url),
	  mImagePriority(priority),
	  mWorkPriority(0),
	  mRequestedPriority(0.f),
	  mDesiredDiscard(-1),
	  mSimRequestedDiscard(-1),
	  mRequestedDiscard(-1),
	  mLoadedDiscard(-1),
	  mDecodedDiscard(-1),
	  mCacheReadTime(0.f),
	  mCacheReadHandle(LLTextureCache::nullHandle()),
	  mCacheWriteHandle(LLTextureCache::nullHandle()),
	  mRequestedSize(0),
	  mRequestedOffset(0),
	  mDesiredSize(TEXTURE_CACHE_ENTRY_SIZE),
	  mFileSize(0),
	  mCachedSize(0),
	  mLoaded(FALSE),
	  mSentRequest(UNSENT),
	  mDecodeHandle(0),
	  mDecoded(FALSE),
	  mWritten(FALSE),
	  mNeedsAux(FALSE),
	  mHaveAllData(FALSE),
	  mInLocalCache(FALSE),
	  mCanUseHTTP(true),
	  mHTTPFailCount(0),
	  mRetryAttempt(0),
	  mActiveCount(0),
	  mGetStatus(0),
	  mFirstPacket(0),
	  mLastPacket(-1),
	  mTotalPackets(0),
	  mImageCodec(IMG_CODEC_INVALID),
	  mMetricsStartTime(0),
	  mHttpReplySize(0U),
	  mHttpReplyOffset(0U),
	  mCacheReadCount(0U),
	  mCacheWriteCount(0U)
{
	mCanUseNET = mUrl.empty(); // Necessary for precached UUID textures, regardless of grid.

	if (!mCanUseNET)
	{
	  // Probably a file://, but well; in that case servicename will be empty.
	  std::string servicename = AIPerService::extract_canonical_servicename(mUrl);
	  if (!servicename.empty())
	  {
		// Make sure mPerServicePtr is up to date with mUrl.
		mPerServicePtr = AIPerService::instance(servicename);
	  }
	}

	calcWorkPriority();
	mType = host.isOk() ? LLImageBase::TYPE_AVATAR_BAKE : LLImageBase::TYPE_NORMAL;
 	//LL_INFOS() << "Create: " << mID << " mHost:" << host << " Discard=" << discard << " URL:"<< mUrl << LL_ENDL;
	if (!mFetcher->mDebugPause)
	{
		U32 work_priority = mWorkPriority | LLWorkerThread::PRIORITY_HIGH;
		addWork(0, work_priority );
	}
	setDesiredDiscard(discard, size);
}

LLTextureFetchWorker::~LLTextureFetchWorker()
{
// 	LL_INFOS() << "Destroy: " << mID
// 			<< " Decoded=" << mDecodedDiscard
// 			<< " Requested=" << mRequestedDiscard
// 			<< " Desired=" << mDesiredDiscard << LL_ENDL;
	llassert_always(!haveWork());
	lockWorkMutex();
	if (mCacheReadHandle != LLTextureCache::nullHandle() && mFetcher->mTextureCache)
	{
		mFetcher->mTextureCache->readComplete(mCacheReadHandle, true);
	}
	if (mCacheWriteHandle != LLTextureCache::nullHandle() && mFetcher->mTextureCache)
	{
		mFetcher->mTextureCache->writeComplete(mCacheWriteHandle, true);
	}
	mFormattedImage = NULL;
	clearPackets();
	unlockWorkMutex();
	mFetcher->removeFromHTTPQueue(mID);
	mFetcher->updateStateStats(mCacheReadCount, mCacheWriteCount);
}

void LLTextureFetchWorker::clearPackets()
{
	for_each(mPackets.begin(), mPackets.end(), DeletePointer());
	mPackets.clear();
	mTotalPackets = 0;
	mLastPacket = -1;
	mFirstPacket = 0;
}

void LLTextureFetchWorker::setupPacketData()
{
	S32 data_size = 0;
	if (mFormattedImage.notNull())
	{
		data_size = mFormattedImage->getDataSize();
	}
	if (data_size > 0)
	{
		// Only used for simulator requests
		mFirstPacket = (data_size - FIRST_PACKET_SIZE) / MAX_IMG_PACKET_SIZE + 1;
		if (FIRST_PACKET_SIZE + (mFirstPacket-1) * MAX_IMG_PACKET_SIZE != data_size)
		{
			LL_WARNS(LOG_TXT) << "Bad CACHED TEXTURE size: " << data_size << " removing." << LL_ENDL;
			removeFromCache();
			resetFormattedData();
			clearPackets();
		}
		else if (mFileSize > 0)
		{
			mLastPacket = mFirstPacket-1;
			mTotalPackets = (mFileSize - FIRST_PACKET_SIZE + MAX_IMG_PACKET_SIZE-1) / MAX_IMG_PACKET_SIZE + 1;
		}
		else
		{
			// This file was cached using HTTP so we have to refetch the first packet
			resetFormattedData();
			clearPackets();
		}
	}
}

U32 LLTextureFetchWorker::calcWorkPriority()
{
// 	llassert_always(mImagePriority >= 0 && mImagePriority <= LLViewerTexture::maxDecodePriority());
	static const F32 PRIORITY_SCALE = (F32)LLWorkerThread::PRIORITY_LOWBITS / LLViewerFetchedTexture::maxDecodePriority();
	mWorkPriority = llmin((U32)LLWorkerThread::PRIORITY_LOWBITS, (U32)(mImagePriority * PRIORITY_SCALE));
	return mWorkPriority;
}

// mWorkMutex is locked
void LLTextureFetchWorker::setDesiredDiscard(S32 discard, S32 size)
{
	bool prioritize = false;
	if (mDesiredDiscard != discard)
	{
		if (!haveWork())
		{
			calcWorkPriority();
			if (!mFetcher->mDebugPause)
			{
				U32 work_priority = mWorkPriority | LLWorkerThread::PRIORITY_HIGH;
				addWork(0, work_priority);
			}
		}
		else if (mDesiredDiscard < discard)
		{
			prioritize = true;
		}
		mDesiredDiscard = discard;
		mDesiredSize = size;
	}
	else if (size > mDesiredSize)
	{
		mDesiredSize = size;
		prioritize = true;
	}
	mDesiredSize = llmax(mDesiredSize, TEXTURE_CACHE_ENTRY_SIZE);
	if ((prioritize && mState == INIT) || mState == DONE)
	{
		setState(INIT);
		U32 work_priority = mWorkPriority | LLWorkerThread::PRIORITY_HIGH;
		setPriority(work_priority);
	}
}

void LLTextureFetchWorker::setImagePriority(F32 priority)
{
// 	llassert_always(priority >= 0 && priority <= LLViewerTexture::maxDecodePriority());
	F32 delta = fabs(priority - mImagePriority);
	if (delta > (mImagePriority * .05f) || mState == DONE)
	{
		mImagePriority = priority;
		calcWorkPriority();
		U32 work_priority = mWorkPriority | (getPriority() & LLWorkerThread::PRIORITY_HIGHBITS);
		setPriority(work_priority);
	}
}

void LLTextureFetchWorker::resetFormattedData()
{
	std::vector<U8>().swap(mHttpBuffer);
	if (mFormattedImage.notNull())
	{
		mFormattedImage->deleteData();
	}
	mHttpReplySize = 0;
	mHttpReplyOffset = 0;
	mHaveAllData = FALSE;
}

// Called from MAIN thread
void LLTextureFetchWorker::startWork(S32 param)
{
	llassert(mFormattedImage.isNull());
}

#include "llviewertexturelist.h" // debug

// Called from LLWorkerThread::processRequest()
bool LLTextureFetchWorker::doWork(S32 param)
{
	LLMutexLock lock(&mWorkMutex);

	if ((mFetcher->isQuitting() || getFlags(LLWorkerClass::WCF_DELETE_REQUESTED)))
	{
		if (mState < DECODE_IMAGE)
	{
			return true; // abort
		}
	}
	if(mImagePriority < F_ALMOST_ZERO)
	{
		if (mState == INIT || mState == LOAD_FROM_NETWORK/* || mState == LOAD_FROM_SIMULATOR*/) //If we've already sent out requests.. might as well continue.
		{
			LL_DEBUGS(LOG_TXT) << mID << " abort: mImagePriority < F_ALMOST_ZERO" << LL_ENDL;
			return true; // abort
		}
	}
	if(mState > CACHE_POST && !mCanUseNET && !mCanUseHTTP)
	{
		//nowhere to get data, abort.
		LL_WARNS(LOG_TXT) << mID << " abort, nowhere to get data" << LL_ENDL;
		return true ;
	}
	
	if (mFetcher->mDebugPause)
	{
		return false; // debug: don't do any work
	}
	if (mID == mFetcher->mDebugID)
	{
		mFetcher->mDebugCount++; // for setting breakpoints
	}

	if (mState != DONE)
	{
		mFetchTimer.reset();
	}

	if (mState == INIT)
	{
		if(gAssetStorage && std::find(gAssetStorage->mBlackListedAsset.begin(),
			gAssetStorage->mBlackListedAsset.end(),mID) != gAssetStorage->mBlackListedAsset.end())
		{
			LL_INFOS() << "Blacklisted asset " << mID.asString() << " was trying to be accessed!!!!!!" << LL_ENDL;
			setState(DONE);
			return true;
		}

		mRawImage = NULL ;
		mRequestedDiscard = -1;
		mLoadedDiscard = -1;
		mDecodedDiscard = -1;
		mRequestedSize = 0;
		mRequestedOffset = 0;
		mFileSize = 0;
		mCachedSize = 0;
		mLoaded = FALSE;
		mSentRequest = UNSENT;
		mDecoded  = FALSE;
		mWritten  = FALSE;
		std::vector<U8>().swap(mHttpBuffer);
		mHttpReplySize = 0;
		mHttpReplyOffset = 0;
		mHaveAllData = FALSE;
		clearPackets(); // TODO: Shouldn't be necessary
		mCacheReadHandle = LLTextureCache::nullHandle();
		mCacheWriteHandle = LLTextureCache::nullHandle();
		setState(LOAD_FROM_TEXTURE_CACHE);
		mDesiredSize = llmax(mDesiredSize, TEXTURE_CACHE_ENTRY_SIZE); // min desired size is TEXTURE_CACHE_ENTRY_SIZE
		LL_DEBUGS(LOG_TXT) << mID << ": Priority: " << llformat("%8.0f",mImagePriority)
							 << " Desired Discard: " << mDesiredDiscard << " Desired Size: " << mDesiredSize << LL_ENDL;
		// fall through
	}

	if (mState == LOAD_FROM_TEXTURE_CACHE)
	{
		if (mCacheReadHandle == LLTextureCache::nullHandle())
		{
			U32 cache_priority = mWorkPriority;
			S32 offset = mFormattedImage.notNull() ? mFormattedImage->getDataSize() : 0;
			S32 size = mDesiredSize - offset;
			if (size <= 0)
			{
				setState(CACHE_POST);
				return false;
			}
			mFileSize = 0;
			mLoaded = FALSE;

			if (mUrl.compare(0, 7, "file://") == 0)
			{
				setPriority(LLWorkerThread::PRIORITY_LOW | mWorkPriority); // Set priority first since Responder may change it

				// read file from local disk
				++mCacheReadCount;
				std::string filename = mUrl.substr(7, std::string::npos);
				CacheReadResponder* responder = new CacheReadResponder(mFetcher, mID, mFormattedImage);
				mCacheReadHandle = mFetcher->mTextureCache->readFromCache(filename, mID, cache_priority,
																		  offset, size, responder);
				mCacheReadTimer.reset();
			}
			else if ((mUrl.empty() || mFTType==FTT_SERVER_BAKE))
			{
				setPriority(LLWorkerThread::PRIORITY_LOW | mWorkPriority); // Set priority first since Responder may change it

				++mCacheReadCount;
				CacheReadResponder* responder = new CacheReadResponder(mFetcher, mID, mFormattedImage);
				mCacheReadHandle = mFetcher->mTextureCache->readFromCache(mID, cache_priority,
																		  offset, size, responder);
				mCacheReadTimer.reset();
			}
			else if(!mUrl.empty() && mCanUseHTTP)
			{
				if (!(mUrl.compare(0, 7, "http://") == 0))
				{
					// *TODO:?remove this warning
					LL_WARNS() << "Unknown URL Type: " << mUrl << LL_ENDL;
				}
				setPriority(LLWorkerThread::PRIORITY_HIGH | mWorkPriority);
				setState(SEND_HTTP_REQ);
			}
			else
			{
				setPriority(LLWorkerThread::PRIORITY_HIGH | mWorkPriority);
				setState(LOAD_FROM_NETWORK);
			}
		}

		if (mLoaded)
		{
			// Make sure request is complete. *TODO: make this auto-complete
			if (mFetcher->mTextureCache->readComplete(mCacheReadHandle, false))
			{
				mCacheReadHandle = LLTextureCache::nullHandle();
				setState(CACHE_POST);
				// fall through
			}
			else
			{
				//
				//This should never happen
				//
				LL_DEBUGS(LOG_TXT) << mID << " this should never happen" << LL_ENDL;
				return false;
			}
		}
		else
		{
			return false;
		}
	}

	if (mState == CACHE_POST)
	{
		mCachedSize = mFormattedImage.notNull() ? mFormattedImage->getDataSize() : 0;
		// Successfully loaded
		if ((mCachedSize >= mDesiredSize) || mHaveAllData)
		{
			// we have enough data, decode it
			llassert_always(mFormattedImage->getDataSize() > 0);
			mLoadedDiscard = mDesiredDiscard;
			if (mLoadedDiscard < 0)
			{
				LL_WARNS(LOG_TXT) << mID << " mLoadedDiscard is " << mLoadedDiscard
									<< ", should be >=0" << LL_ENDL;
			}
			setState(DECODE_IMAGE);
			mWriteToCacheState = NOT_WRITE ;
			LL_DEBUGS(LOG_TXT) << mID << ": Cached. Bytes: " << mFormattedImage->getDataSize()
								 << " Size: " << llformat("%dx%d",mFormattedImage->getWidth(),mFormattedImage->getHeight())
								 << " Desired Discard: " << mDesiredDiscard << " Desired Size: " << mDesiredSize << LL_ENDL;
			LLTextureFetch::sCacheHitRate.addValue(100.f);
		}
		else
		{
			if (mUrl.compare(0, 7, "file://") == 0)
			{
				// failed to load local file, we're done.
				LL_WARNS(LOG_TXT) << mID << ": abort, failed to load local file " << mUrl << LL_ENDL;
				return true;
			}
			// need more data
			else
			{
				LL_DEBUGS(LOG_TXT) << mID << ": Not in Cache" << LL_ENDL;
				setState(LOAD_FROM_NETWORK);
			}
			
			// fall through
			LLTextureFetch::sCacheHitRate.addValue(0.f);
		}
	}

	if (mState == LOAD_FROM_NETWORK)
	{
		static LLCachedControl<bool> use_http(gSavedSettings,"ImagePipelineUseHTTP");
		bool is_sl = gHippoGridManager->getConnectedGrid()->isSecondLife();

// 		if (mHost != LLHost::invalid) use_http = false;
		if ((is_sl || use_http) && mCanUseHTTP && mUrl.empty())	// get http url.
		{
			LLViewerRegion* region = NULL;
			if (mHost == LLHost::invalid)
				region = gAgent.getRegion();
			else
				region = LLWorld::getInstance()->getRegion(mHost);

			if (region)
			{
				std::string http_url = region->getViewerAssetUrl();
				if (http_url.empty()) http_url = region->getCapability("GetTexture");
				if (!http_url.empty())
				{
					if (mFTType != FTT_DEFAULT)
					{
						LL_WARNS(LOG_TXT) << "trying to seek a non-default texture on the sim. Bad! mFTType: " << mFTType << LL_ENDL;
					}
					mUrl = http_url + "/?texture_id=" + mID.asString().c_str();
					LL_DEBUGS(LOG_TXT) << "Texture URL: " << mUrl << LL_ENDL;
					mWriteToCacheState = CAN_WRITE ; //because this texture has a fixed texture id.
					mPerServicePtr = AIPerService::instance(AIPerService::extract_canonical_servicename(http_url));
				}
				else
				{
					mCanUseHTTP = false ;
					LL_DEBUGS(LOG_TXT) << "Texture not available via HTTP: empty URL." << LL_ENDL;
				}
			}
			else
			{
				// This will happen if not logged in or if a region does not have HTTP Texture enabled
				//LL_WARNS() << "Region not found for host: " << mHost << LL_ENDL;
				LL_DEBUGS(LOG_TXT) << "Texture not available via HTTP: no region " << mUrl << LL_ENDL;
				mCanUseHTTP = false;
			}
		}
		else if (mFTType == FTT_SERVER_BAKE)
		{
			mWriteToCacheState = CAN_WRITE;
		}
		if (!mUrl.empty() && SGHostBlackList::isBlacklisted(mUrl)){
			LL_DEBUGS("Texture") << mID << "Blacklisted" << LL_ENDL;
			mCanUseHTTP = false;
		}
		if (mCanUseHTTP && !mUrl.empty())
		{
			setState(SEND_HTTP_REQ);
			setPriority(LLWorkerThread::PRIORITY_HIGH | mWorkPriority);
			if(mWriteToCacheState != NOT_WRITE)
			{
				mWriteToCacheState = CAN_WRITE ;
			}
			// don't return, fall through to next state
		}
		else if (!mCanUseNET)
		{
			LL_WARNS(LOG_TXT) << mID << "Unable to retrieve texture via HTTP and UDP unavailable (probable 404): " << mUrl << LL_ENDL;
			return true;
		}
		else if (mSentRequest == UNSENT)
		{
			LL_DEBUGS("Texture") << mID << " moving to UDP fetch. mSentRequest=" << mSentRequest << " mCanUseNET = " << mCanUseNET << LL_ENDL;
			setState(SEND_UDP_REQ);
			setPriority(LLWorkerThread::PRIORITY_HIGH | mWorkPriority);
			if(mWriteToCacheState != NOT_WRITE)
			{
				mWriteToCacheState = CAN_WRITE ;
			}
		}
		else
		{
			// Shouldn't need to do anything here
			//llassert_always(mFetcher->mNetworkQueue.find(mID) != mFetcher->mNetworkQueue.end());
			// Make certain this is in the network queue
			//mFetcher->addToNetworkQueue(this);
			//recordTextureStart(false);
			//setPriority(LLWorkerThread::PRIORITY_LOW | mWorkPriority);

			LL_WARNS("Texture") << mID << " does this happen? mSentRequest=" << mSentRequest << " mCanUseNET = " << mCanUseNET << LL_ENDL;
			return false;
		}
	}
	
	if (mState == LOAD_FROM_SIMULATOR)	//UDP. From LLTextureFetch::receiveImageHeader or LLTextureFetch::receiveImagePacket
	{
		if (mFormattedImage.isNull())
		{
			mFormattedImage = new LLImageJ2C;
		}
		if (processSimulatorPackets())
		{
			LL_DEBUGS(LOG_TXT) << mID << ": Loaded from Sim. Bytes: " << mFormattedImage->getDataSize() << LL_ENDL;
			mFetcher->removeFromNetworkQueue(this, false);
			if (mFormattedImage.isNull() || !mFormattedImage->getDataSize())
			{
				// processSimulatorPackets() failed
// 				LL_WARNS() << "processSimulatorPackets() failed to load buffer" << LL_ENDL;
				LL_WARNS(LOG_TXT) << mID << " processSimulatorPackets() failed to load buffer" << LL_ENDL;
				return true; // failed
			}
			setPriority(LLWorkerThread::PRIORITY_HIGH | mWorkPriority);
			if (mLoadedDiscard < 0)
			{
				LL_WARNS(LOG_TXT) << mID << " mLoadedDiscard is " << mLoadedDiscard
									<< ", should be >=0" << LL_ENDL;
			}
			setState(DECODE_IMAGE);
			llassert_always(strstr(mUrl.c_str(), "map.secondlife") == NULL);
			mWriteToCacheState = SHOULD_WRITE;
			recordTextureDone(false);
		}
		else
		{
			llassert(mFetcher->mNetworkQueue.find(mID) != mFetcher->mNetworkQueue.end());
			//mFetcher->addToNetworkQueue(this); // failsafe
			//setPriority(LLWorkerThread::PRIORITY_LOW | mWorkPriority);
			//recordTextureStart(false);
		}
		return false;
	}

	if (mState == SEND_UDP_REQ)
	{
		if (! mCanUseNET)
		{
			LL_WARNS("Texture") << mID << " abort: SEND_UDP_REQ but !mCanUseNet" << LL_ENDL;
			return true ; //abort
		}

		LL_DEBUGS("Texture") << mID << " sending to UDP fetch. mSentRequest=" << mSentRequest << " mCanUseNET = " << mCanUseNET << LL_ENDL;

		mRequestedSize = mDesiredSize;
		mRequestedDiscard = mDesiredDiscard;
		mSentRequest = QUEUED;
		mFetcher->addToNetworkQueue(this);
		recordTextureStart(false);
		setPriority(LLWorkerThread::PRIORITY_LOW | mWorkPriority);
		setState(WAIT_UDP_REQ);	
	}

	if (mState == WAIT_UDP_REQ)
	{
		//do nothing.
		if (! mCanUseNET)
		{
			LL_WARNS("Texture") << mID << " abort: SEND_UDP_REQ but !mCanUseNet" << LL_ENDL;
			return true ; //abort
		}
		llassert(mFetcher->mNetworkQueue.find(mID) != mFetcher->mNetworkQueue.end());
		//Should migrate to LOAD_FROM_SIMULATOR upon receipt of data.
	}
	
	if (mState == SEND_HTTP_REQ)
	{
		if (! mCanUseHTTP)
		{
			LL_WARNS(LOG_TXT) << mID << " abort: SEND_HTTP_REQ but !mCanUseHTTP" << LL_ENDL;
			return true ; //abort
		}
		S32 cur_size = 0;
		if (mFormattedImage.notNull())
		{
			cur_size = mFormattedImage->getDataSize(); // amount of data we already have
			if (mFormattedImage->getDiscardLevel() == 0)
			{
				// Already have all data.
				mFetcher->removeFromNetworkQueue(this, false);	// Note sure this is necessary, but it's what the old did --Aleric
				if (cur_size > 0)
				{
					// We already have all the data, just decode it
					mLoadedDiscard = mFormattedImage->getDiscardLevel();
					setPriority(LLWorkerThread::PRIORITY_HIGH | mWorkPriority);
					if (mLoadedDiscard < 0)
					{
						LL_WARNS(LOG_TXT) << mID << " mLoadedDiscard is " << mLoadedDiscard
											<< ", should be >=0" << LL_ENDL;
					}
					setState(DECODE_IMAGE);
					return false;
				}
				else
				{
					LL_WARNS(LOG_TXT) << mID << " SEND_HTTP_REQ abort: cur_size " << cur_size << " <=0" << LL_ENDL;
						return true ; //abort.
				}
			}
		}

		// Let AICurl decide if we can process more HTTP requests at the moment or not.

		// AIPerService::approveHTTPRequestFor returns approvement for ONE request.
		// This object keeps track of whether or not that is honored.
		LLPointer<AIPerService::Approvement> approved = AIPerService::approveHTTPRequestFor(mPerServicePtr, cap_texture);
		if (!approved)
		{
			return false ; //wait.
		}

		mFetcher->removeFromNetworkQueue(this, false);

		mRequestedSize = mDesiredSize;
		mRequestedDiscard = mDesiredDiscard;
		mRequestedSize -= cur_size;
		mRequestedOffset = cur_size;

		if (mRequestedOffset)
		{
			// Texture fetching often issues 'speculative' loads that
			// start beyond the end of the actual asset.  Some cache/web
			// systems, e.g. Varnish, will respond to this not with a
			// 416 but with a 200 and the entire asset in the response
			// body.  By ensuring that we always have a partially
			// satisfiable Range request, we avoid that hit to the network.
			// We just have to deal with the overlapping data which is made
			// somewhat harder by the fact that grid services don't necessarily
			// return the Content-Range header on 206 responses.  *Sigh*
			mRequestedOffset -= 1;
			mRequestedSize += 1;
		}
		
		if (mUrl.empty())
		{
			LL_WARNS() << "HTTP GET request failed for " << mID << LL_ENDL;
			resetFormattedData();
			++mHTTPFailCount;
			return true; // failed
		}
		
		mRequestedTimer.reset();
		mLoaded = FALSE;
		mGetStatus = 0;
		mGetReason.clear();
		LL_DEBUGS(LOG_TXT) << "HTTP GET: " << mID << " Offset: " << mRequestedOffset
									 << " Bytes: " << mRequestedSize
									 << LL_ENDL;
		// Will call callbackHttpGet when curl request completes
		AIHTTPHeaders headers("Accept", "image/x-j2c");
		// Call LLHTTPClient::request directly instead of LLHTTPClient::getByteRange, because we want to pass a NULL AIEngine.
		if (mRequestedOffset > 0 || mRequestedSize > 0)
		{
			int const range_end = mRequestedOffset + mRequestedSize - 1;
			char const* const range_format = (range_end >= HTTP_REQUESTS_RANGE_END_MAX) ? "bytes=%d-" : "bytes=%d-%d";
			headers.addHeader("Range", llformat(range_format, mRequestedOffset, range_end));
		}
		LLHTTPClient::request(mUrl, LLHTTPClient::HTTP_GET, NULL,
			new HTTPGetResponder( mFTType, mFetcher, mID, LLTimer::getTotalTime(), mRequestedSize, mRequestedOffset),
			headers, approved/*,*/ DEBUG_CURLIO_PARAM(debug_off), keep_alive, no_does_authentication, allow_compressed_reply, NULL, 0, NULL);

		mFetcher->addToHTTPQueue(mID);
		recordTextureStart(true);
		setPriority(LLWorkerThread::PRIORITY_LOW | mWorkPriority);
		setState(WAIT_HTTP_REQ);	
		
		// fall through
	}
	
	if (mState == WAIT_HTTP_REQ)
	{
		if (mLoaded)
		{
			S32 cur_size = mFormattedImage.notNull() ? mFormattedImage->getDataSize() : 0;
			if (mRequestedSize < 0)
			{
				S32 max_attempts;
				switch(mGetStatus)
				{
#define HTTP_CASE(name) case name: LL_DEBUGS("TexDebug") << mID << " status = " << mGetStatus << " (" << #name << ")" << " Failcount = " << mHTTPFailCount << LL_ENDL; break; 
				HTTP_CASE(HTTP_CONTINUE)
				HTTP_CASE(HTTP_SWITCHING_PROTOCOLS)
				HTTP_CASE(HTTP_OK)
				HTTP_CASE(HTTP_CREATED)
				HTTP_CASE(HTTP_ACCEPTED)
				HTTP_CASE(HTTP_NON_AUTHORITATIVE_INFORMATION)
				HTTP_CASE(HTTP_NO_CONTENT)
				HTTP_CASE(HTTP_RESET_CONTENT)
				HTTP_CASE(HTTP_PARTIAL_CONTENT)
				HTTP_CASE(HTTP_MULTIPLE_CHOICES)
				HTTP_CASE(HTTP_MOVED_PERMANENTLY)
				HTTP_CASE(HTTP_FOUND)
				HTTP_CASE(HTTP_SEE_OTHER)
				HTTP_CASE(HTTP_NOT_MODIFIED)
				HTTP_CASE(HTTP_USE_PROXY)
				HTTP_CASE(HTTP_TEMPORARY_REDIRECT)
				HTTP_CASE(HTTP_BAD_REQUEST)
				HTTP_CASE(HTTP_UNAUTHORIZED)
				HTTP_CASE(HTTP_PAYMENT_REQUIRED)
				HTTP_CASE(HTTP_FORBIDDEN)
				HTTP_CASE(HTTP_NOT_FOUND)
				HTTP_CASE(HTTP_METHOD_NOT_ALLOWED)
				HTTP_CASE(HTTP_NOT_ACCEPTABLE)
				HTTP_CASE(HTTP_PROXY_AUTHENTICATION_REQUIRED)
				HTTP_CASE(HTTP_REQUEST_TIME_OUT)
				HTTP_CASE(HTTP_CONFLICT)
				HTTP_CASE(HTTP_GONE)
				HTTP_CASE(HTTP_LENGTH_REQUIRED)
				HTTP_CASE(HTTP_PRECONDITION_FAILED)
				HTTP_CASE(HTTP_REQUEST_ENTITY_TOO_LARGE)
				HTTP_CASE(HTTP_REQUEST_URI_TOO_LARGE)
				HTTP_CASE(HTTP_UNSUPPORTED_MEDIA_TYPE)
				HTTP_CASE(HTTP_REQUESTED_RANGE_NOT_SATISFIABLE)
				HTTP_CASE(HTTP_EXPECTATION_FAILED)
				HTTP_CASE(HTTP_INTERNAL_SERVER_ERROR)
				HTTP_CASE(HTTP_NOT_IMPLEMENTED)
				HTTP_CASE(HTTP_BAD_GATEWAY)
				HTTP_CASE(HTTP_SERVICE_UNAVAILABLE)
				HTTP_CASE(HTTP_GATEWAY_TIME_OUT)
				HTTP_CASE(HTTP_VERSION_NOT_SUPPORTED)
				HTTP_CASE(HTTP_INTERNAL_ERROR_LOW_SPEED)
				HTTP_CASE(HTTP_INTERNAL_ERROR_CURL_LOCKUP)
				HTTP_CASE(HTTP_INTERNAL_ERROR_CURL_BADSOCKET)
				HTTP_CASE(HTTP_INTERNAL_ERROR_CURL_TIMEOUT)
				HTTP_CASE(HTTP_INTERNAL_ERROR_CURL_OTHER)
				HTTP_CASE(HTTP_INTERNAL_ERROR_OTHER)
				default:
					LL_DEBUGS("TexDebug") << mID << " status = " << mGetStatus << " (?)" << " Failcount = " << mHTTPFailCount << LL_ENDL; break;
				}
				
				if (mGetStatus == HTTP_NOT_FOUND || mGetStatus == HTTP_INTERNAL_ERROR_CURL_TIMEOUT || mGetStatus == HTTP_INTERNAL_ERROR_LOW_SPEED)
				{
					mHTTPFailCount = max_attempts = 1; // Don't retry
					if(mGetStatus == HTTP_NOT_FOUND)
					{
						if (mFTType != FTT_MAP_TILE)
						{
							LL_WARNS(LOG_TXT) << "Texture missing from server (404): " << mUrl << LL_ENDL;
						}

						if(mWriteToCacheState == NOT_WRITE) //map tiles
						{
							resetFormattedData();
							setState(DONE);
							if (mFTType != FTT_MAP_TILE)
							{
								LL_WARNS(LOG_TXT) << mID << " abort: WAIT_HTTP_REQ not found" << LL_ENDL;
							}
							return true; // failed, means no map tile on the empty region.
						}
					}
					else if (mGetStatus == HTTP_INTERNAL_ERROR_CURL_TIMEOUT || mGetStatus == HTTP_INTERNAL_ERROR_LOW_SPEED)
					{
						if (mGetStatus == HTTP_INTERNAL_ERROR_CURL_TIMEOUT)
						{
							LL_WARNS() << "No response from server (HTTP_INTERNAL_ERROR_CURL_TIMEOUT): " << mUrl << LL_ENDL;
						}
						else
						{
							LL_WARNS() << "Slow response from server (HTTP_INTERNAL_ERROR_LOW_SPEED): " << mUrl << LL_ENDL;
						}
						SGHostBlackList::add(mUrl, 60.0, mGetStatus);
					}
					//roll back to try UDP
					if(mCanUseNET)
					{
						LL_DEBUGS("TexDebug") << mID << " falling back to udp mSentRequest=" << mSentRequest << " mCanUseNET = " << mCanUseNET << LL_ENDL;
						resetFormattedData();
						setState(INIT);
						mCanUseHTTP = false ;
						mUrl.clear();
						setPriority(LLWorkerThread::PRIORITY_HIGH | mWorkPriority);
						LL_DEBUGS("TexDebug") << mID << " .. mSentRequest=" << mSentRequest << " mCanUseNET = " << mCanUseNET << LL_ENDL;
						return false ;
					}
					else
					{
						LL_INFOS("Texture") << mID << " aborted. no udp fallback" << LL_ENDL;
						// UDP is not an option, we are dead
						resetFormattedData();
						return true; // failed
					}
				}
				else if (mGetStatus == HTTP_SERVICE_UNAVAILABLE)
				{
					// *TODO: Should probably introduce a timer here to delay future HTTP requsts
					// for a short time (~1s) to ease server load? Ideally the server would queue
					// requests instead of returning 503... we already limit the number pending.
					++mHTTPFailCount;
					max_attempts = mHTTPFailCount+1; // Keep retrying
					LL_INFOS_ONCE("Texture") << "Texture server busy (503): " << mUrl << LL_ENDL;
				}
				else if (mGetStatus == HTTP_REQUESTED_RANGE_NOT_SATISFIABLE)
				{
					// Allowed, we'll accept whatever data we have as complete.
					mHaveAllData = TRUE;
					max_attempts = mHTTPFailCount+1;
				}
				else
				{
					const S32 HTTP_MAX_RETRY_COUNT = 3;
					max_attempts = HTTP_MAX_RETRY_COUNT + 1;
					++mHTTPFailCount;
					LL_INFOS() << "HTTP GET failed for: " << mUrl
							<< " Status: " << mGetStatus << " Reason: '" << mGetReason << "'"
							<< " Attempt:" << mHTTPFailCount+1 << "/" << max_attempts << LL_ENDL;
				}

				if (mHTTPFailCount >= max_attempts)
				{
					if (mFTType != FTT_SERVER_BAKE)
					{
						mUrl.clear();
					}
					// Make max_attempts attempt at decoding what data we have,
                    // then bail forever on this image
					if (cur_size > 0 && (mHTTPFailCount < (max_attempts+1)) )
					{
						// Use available data
						mLoadedDiscard = mFormattedImage->getDiscardLevel();
						setPriority(LLWorkerThread::PRIORITY_HIGH | mWorkPriority);
						if (mLoadedDiscard < 0)
						{
							LL_WARNS(LOG_TXT) << mID << " mLoadedDiscard is " << mLoadedDiscard
											  << ", should be >=0" << LL_ENDL;
						}
						setState(DECODE_IMAGE);
						return false; 
					}
					else
					{
    					//roll back to try UDP
    					if(mCanUseNET)
    					{
							LL_DEBUGS("TexDebug") << mID << " falling back to udp (2)" << LL_ENDL;
							resetFormattedData();
							setState(INIT);
    						mCanUseHTTP = false ;
    						setPriority(LLWorkerThread::PRIORITY_HIGH | mWorkPriority);
    						return false ;
    					}
    					else
    					{
    						// UDP is not an option, we are dead
    						resetFormattedData();
                            setState(DONE);
							LL_WARNS(LOG_TXT) <<  mID << " abort: fail harder" << LL_ENDL;
    						return true; // failed
    					}
					}
				}
				else
				{
					setState(SEND_HTTP_REQ);
					return false; // retry
				}
			}

			// Clear the url since we're done with the fetch
			// Note: mUrl is used to check is fetching is required so failure to clear it will force an http fetch
			// next time the texture is requested, even if the data have already been fetched.
			if(mWriteToCacheState != NOT_WRITE && mFTType != FTT_SERVER_BAKE)
			{
				// Why do we want to keep url if NOT_WRITE - is this a proxy for map tiles?
				mUrl.clear();
			}
			
			if(mHttpBuffer.empty())//no data received.
			{
				//abort.
				setState(DONE);
				LL_WARNS(LOG_TXT) << mID << " abort: no data received" << LL_ENDL;
				return true;
			}

			S32 append_size(mHttpBuffer.size());
			S32 total_size(cur_size + append_size);
			S32 src_offset(0);
			llassert_always(append_size == mRequestedSize);
			if (mHttpReplyOffset && mHttpReplyOffset != cur_size)
			{
				// In case of a partial response, our offset may
				// not be trivially contiguous with the data we have.
				// Get back into alignment.
				if ((S32)mHttpReplyOffset > cur_size)
				{
					LL_WARNS(LOG_TXT) << "Partial HTTP response produces break in image data for texture "
										<< mID << ".  Aborting load."  << LL_ENDL;
					setState(DONE);
					return true;
				}
				src_offset = cur_size - mHttpReplyOffset;
				append_size -= src_offset;
				total_size -= src_offset;
				mRequestedSize -= src_offset;			// Make requested values reflect useful part
				mRequestedOffset += src_offset;

			}

			if (mFormattedImage.isNull())
			{
				// For now, create formatted image based on extension
				std::string extension = gDirUtilp->getExtension(mUrl);
				mFormattedImage = LLImageFormatted::createFromType(LLImageBase::getCodecFromExtension(extension));
				if (mFormattedImage.isNull())
				{
					mFormattedImage = new LLImageJ2C; // default
				}
			}
						
			if (mHaveAllData) //the image file is fully loaded.
			{
				mFileSize = total_size;
			}
			else //the file size is unknown.
			{
				mFileSize = total_size + 1 ; //flag the file is not fully loaded.
			}
			
			U8* buffer = (U8*)ALLOCATE_MEM(LLImageBase::getPrivatePool(), total_size);
			if (cur_size > 0)
			{
				memcpy(buffer, mFormattedImage->getData(), cur_size);
			}
			if (append_size > 0)
			{
				memcpy(buffer + cur_size, &mHttpBuffer[src_offset], append_size);
			}
			// NOTE: setData releases current data and owns new data (buffer)
			mFormattedImage->setData(buffer, total_size);
			// delete temp data
			std::vector<U8>().swap(mHttpBuffer);
			mHttpReplySize = 0;
			mHttpReplyOffset = 0;

			mLoadedDiscard = mRequestedDiscard;
			if (mLoadedDiscard < 0)
			{
				LL_WARNS(LOG_TXT) << mID << " mLoadedDiscard is " << mLoadedDiscard
									<< ", should be >=0" << LL_ENDL;
			}
			setState(DECODE_IMAGE);
			if(mWriteToCacheState != NOT_WRITE)
			{
				mWriteToCacheState = SHOULD_WRITE ;
			}
			setPriority(LLWorkerThread::PRIORITY_HIGH | mWorkPriority);
			return false;
		}
		else
		{
			setPriority(LLWorkerThread::PRIORITY_LOW | mWorkPriority);
			return false;
		}
	}
	
	if (mState == DECODE_IMAGE)
	{
		static LLCachedControl<bool> textures_decode_disabled(gSavedSettings,"TextureDecodeDisabled");

		setPriority(LLWorkerThread::PRIORITY_LOW | mWorkPriority); // Set priority first since Responder may change it
		if (textures_decode_disabled)
		{
			// for debug use, don't decode
			setState(DONE);
			return true;
		}

		if (mDesiredDiscard < 0)
		{
			// We aborted, don't decode
			setState(DONE);
			LL_DEBUGS(LOG_TXT) << mID << " DECODE_IMAGE abort: desired discard " << mDesiredDiscard << "<0" << LL_ENDL;
			return true;
		}
		
		if (mFormattedImage->getDataSize() <= 0)
		{
			LL_WARNS(LOG_TXT) << "Decode entered with invalid mFormattedImage. ID = " << mID << LL_ENDL;
			
			//abort, don't decode
			setState(DONE);
			LL_DEBUGS(LOG_TXT) << mID << " DECODE_IMAGE abort: (mFormattedImage->getDataSize() <= 0)" << LL_ENDL;
			return true;
		}
		if (mLoadedDiscard < 0)
		{
			LL_WARNS(LOG_TXT) << "Decode entered with invalid mLoadedDiscard. ID = " << mID << LL_ENDL;

			//abort, don't decode
			setState(DONE);
			LL_DEBUGS(LOG_TXT) << mID << " DECODE_IMAGE abort: mLoadedDiscard < 0" << LL_ENDL;
			return true;
		}

		mRawImage = NULL;
		mAuxImage = NULL;
		llassert_always(mFormattedImage.notNull());
		S32 discard = mHaveAllData ? 0 : mLoadedDiscard;
		U32 image_priority = LLWorkerThread::PRIORITY_NORMAL | mWorkPriority;
		mDecoded  = FALSE;
		setPriority(LLWorkerThread::PRIORITY_LOW | mWorkPriority);
		setState(DECODE_IMAGE_UPDATE);
		LL_DEBUGS(LOG_TXT) << mID << ": Decoding. Bytes: " << mFormattedImage->getDataSize() << " Discard: " << discard
				<< " All Data: " << mHaveAllData << LL_ENDL;
		mDecodeHandle = mFetcher->mImageDecodeThread->decodeImage(mFormattedImage, image_priority, discard, mNeedsAux,
																  new DecodeResponder(mFetcher, mID, this));
		// fall though
	}
	
	if (mState == DECODE_IMAGE_UPDATE)
	{
		if (mDecoded)
		{
			if (mDecodedDiscard < 0)
			{
				LL_DEBUGS(LOG_TXT) << mID << ": Failed to Decode." << LL_ENDL;
				if (mCachedSize > 0 && !mInLocalCache && mRetryAttempt == 0)
				{
					// Cache file should be deleted, try again
 					LL_WARNS(LOG_TXT) << mID << ": Decode of cached file failed (removed), retrying" << LL_ENDL;
					llassert_always(mDecodeHandle == 0);
					mFormattedImage = NULL;
					++mRetryAttempt;
					setPriority(LLWorkerThread::PRIORITY_HIGH | mWorkPriority);
					setState(INIT);
					return false;
				}
				else
				{
// 					LL_WARNS() << "UNABLE TO LOAD TEXTURE: " << mID << " RETRIES: " << mRetryAttempt << LL_ENDL;
					setState(DONE); // failed
				}
			}
			else
			{
				llassert_always(mRawImage.notNull());
				LL_DEBUGS(LOG_TXT) << mID << ": Decoded. Discard: " << mDecodedDiscard
						<< " Raw Image: " << llformat("%dx%d",mRawImage->getWidth(),mRawImage->getHeight()) << LL_ENDL;
				setPriority(LLWorkerThread::PRIORITY_HIGH | mWorkPriority);
				setState(WRITE_TO_CACHE);
			}
			// fall through
		}
		else
		{
			return false;
		}
	}

	if (mState == WRITE_TO_CACHE)
	{
		if (mWriteToCacheState != SHOULD_WRITE || mFormattedImage.isNull())
		{
			// If we're in a local cache or we didn't actually receive any new data,
			// or we failed to load anything, skip
			setState(DONE);
			return false;
		}
		S32 datasize = mFormattedImage->getDataSize();
		if (mFileSize < datasize)	// This could happen when http fetching and sim fetching mixed.
		{
			if (mHaveAllData)
			{
				mFileSize = datasize;
			}
			else
			{
				mFileSize = datasize + 1; // flag not fully loaded.
			}
		}
		llassert_always(datasize);
		setPriority(LLWorkerThread::PRIORITY_LOW | mWorkPriority); // Set priority first since Responder may change it
		U32 cache_priority = mWorkPriority;
		mWritten = FALSE;
		setState(WAIT_ON_WRITE);
		++mCacheWriteCount;
		CacheWriteResponder* responder = new CacheWriteResponder(mFetcher, mID);
		mCacheWriteHandle = mFetcher->mTextureCache->writeToCache(mID, cache_priority,
																  mFormattedImage->getData(), datasize,
																  mFileSize, responder);
		// fall through
	}
	
	if (mState == WAIT_ON_WRITE)
	{
		if (writeToCacheComplete())
		{
			setState(DONE);
			// fall through
		}
		else
		{
			if (mDesiredDiscard < mDecodedDiscard)
			{
				// We're waiting for this write to complete before we can receive more data
				// (we can't touch mFormattedImage until the write completes)
				// Prioritize the write
				mFetcher->mTextureCache->prioritizeWrite(mCacheWriteHandle);
			}
			return false;
		}
	}

	if (mState == DONE)
	{
		if (mDecodedDiscard >= 0 && mDesiredDiscard < mDecodedDiscard)
		{
			// More data was requested, return to INIT
			setState(INIT);
			LL_DEBUGS(LOG_TXT) << mID << " more data requested, returning to INIT: " 
							   << " mDecodedDiscard " << mDecodedDiscard << ">= 0 && mDesiredDiscard " << mDesiredDiscard
							   << "<" << " mDecodedDiscard " << mDecodedDiscard << LL_ENDL;
			setPriority(LLWorkerThread::PRIORITY_HIGH | mWorkPriority);
			return false;
		}
		else
		{
			setPriority(LLWorkerThread::PRIORITY_LOW | mWorkPriority);
			return true;
		}
	}
	
	return false;
}

// Called from MAIN thread
void LLTextureFetchWorker::endWork(S32 param, bool aborted)
{
	if (mDecodeHandle != 0)
	{
		mFetcher->mImageDecodeThread->abortRequest(mDecodeHandle, false);
		mDecodeHandle = 0;
	}
	mFormattedImage = NULL;
}

//////////////////////////////////////////////////////////////////////////////

// virtual
void LLTextureFetchWorker::finishWork(S32 param, bool completed)
{
	// The following are required in case the work was aborted
	if (mCacheReadHandle != LLTextureCache::nullHandle())
	{
		mFetcher->mTextureCache->readComplete(mCacheReadHandle, true);
		mCacheReadHandle = LLTextureCache::nullHandle();
	}
	if (mCacheWriteHandle != LLTextureCache::nullHandle())
	{
		mFetcher->mTextureCache->writeComplete(mCacheWriteHandle, true);
		mCacheWriteHandle = LLTextureCache::nullHandle();
	}
}

// virtual
bool LLTextureFetchWorker::deleteOK()
{
	bool delete_ok = true;
	// Allow any pending reads or writes to complete
	if (mCacheReadHandle != LLTextureCache::nullHandle())
	{
		if (mFetcher->mTextureCache->readComplete(mCacheReadHandle, true))
		{
			mCacheReadHandle = LLTextureCache::nullHandle();
		}
		else
		{
			delete_ok = false;
		}
	}
	if (mCacheWriteHandle != LLTextureCache::nullHandle())
	{
		if (mFetcher->mTextureCache->writeComplete(mCacheWriteHandle))
		{
			mCacheWriteHandle = LLTextureCache::nullHandle();
		}
		else
		{
			delete_ok = false;
		}
	}

	if ((haveWork() &&
		 // not ok to delete from these states
		 ((mState >= WRITE_TO_CACHE && mState <= WAIT_ON_WRITE))))
	{
		delete_ok = false;
	}
	
	return delete_ok;
}

void LLTextureFetchWorker::removeFromCache()
{
	if (!mInLocalCache)
	{
		mFetcher->mTextureCache->removeFromCache(mID);
	}
}


//////////////////////////////////////////////////////////////////////////////

bool LLTextureFetchWorker::processSimulatorPackets()
{
	if (mFormattedImage.isNull() || mRequestedSize < 0)
	{
		// not sure how we got here, but not a valid state, abort!
		llassert_always(mDecodeHandle == 0);
		mFormattedImage = NULL;
		return true;
	}
	
	if (mLastPacket >= mFirstPacket)
	{
		S32 buffer_size = mFormattedImage->getDataSize();
		for (S32 i = mFirstPacket; i<=mLastPacket; i++)
		{
			llassert_always(mPackets[i]);
			buffer_size += mPackets[i]->mSize;
		}
		bool have_all_data = mLastPacket >= mTotalPackets-1;
		if (mRequestedSize <= 0)
		{
			// We received a packed but haven't requested anything yet (edge case)
			// Return true (we're "done") since we didn't request anything
			return true;
		}
		if (buffer_size >= mRequestedSize || have_all_data)
		{
			/// We have enough (or all) data
			if (have_all_data)
			{
				mHaveAllData = TRUE;
			}
			S32 cur_size = mFormattedImage->getDataSize();
			if (buffer_size > cur_size)
			{
				/// We have new data
				U8* buffer = (U8*)ALLOCATE_MEM(LLImageBase::getPrivatePool(), buffer_size);
				S32 offset = 0;
				if (cur_size > 0 && mFirstPacket > 0)
				{
					memcpy(buffer, mFormattedImage->getData(), cur_size);
					offset = cur_size;
				}
				for (S32 i=mFirstPacket; i<=mLastPacket; i++)
				{
					memcpy(buffer + offset, mPackets[i]->mData, mPackets[i]->mSize);
					offset += mPackets[i]->mSize;
				}
				// NOTE: setData releases current data
				mFormattedImage->setData(buffer, buffer_size);
			}
			mLoadedDiscard = mRequestedDiscard;
			return true;
		}
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////////

S32 LLTextureFetchWorker::callbackHttpGet(U32 offset, U32 length,
										   const LLChannelDescriptors& channels,
										   const LLHTTPClient::ResponderBase::buffer_ptr_t& buffer,
										   bool partial, bool success)
{
	S32 data_size = 0 ;

	if (mState != WAIT_HTTP_REQ)
	{
		LL_WARNS(LOG_TXT) << "callbackHttpGet for unrequested fetch worker: " << mID
				<< " req=" << mSentRequest << " state= " << mState << LL_ENDL;
		return data_size;
	}
	if (mLoaded)
	{
		LL_WARNS(LOG_TXT) << "Duplicate callback for " << mID.asString() << LL_ENDL;
		return data_size; // ignore duplicate callback
	}
	if (success)
	{
		// get length of stream:
		data_size = buffer->countAfter(channels.in(), NULL);

		LL_DEBUGS(LOG_TXT) << "HTTP RECEIVED: " << mID.asString() << " Bytes: " << data_size << LL_ENDL;
		if (data_size > 0)
		{
			LLViewerStatsRecorder::instance().textureFetch(data_size);
			// *TODO: set the formatted image data here directly to avoid the copy
			llassert(mHttpBuffer.empty());
			mHttpBuffer.resize(data_size);
			buffer->readAfter(channels.in(), NULL, &mHttpBuffer[0], data_size);

			if (partial)
			{
				if (! offset && ! length)
				{
					// This is the case where we receive a 206 status but
					// there wasn't a useful Content-Range header in the response.
					// This could be because it was badly formatted but is more
					// likely due to capabilities services which scrub headers
					// from responses.  Assume we got what we asked for...
					mHttpReplySize = data_size;
					mHttpReplyOffset = mRequestedOffset;
				}
				else
				{
					mHttpReplySize = length;
					mHttpReplyOffset = offset;
				}
			}

			if (! partial)
			{
				// Response indicates this is the entire asset regardless
				// of our asking for a byte range.  Mark it so and drop
				// any partial data we might have so that the current
				// response body becomes the entire dataset.
				if (data_size <= mRequestedOffset)
				{
					LL_WARNS(LOG_TXT) << "Fetched entire texture " << mID
										<< " when it was expected to be marked complete.  mImageSize:  "
										<< mFileSize << " datasize:  " << mFormattedImage->getDataSize()
										<< LL_ENDL;
				}
				mHaveAllData = TRUE;
				llassert_always(mDecodeHandle == 0);
				mFormattedImage = NULL; // discard any previous data we had
			}
			else if (data_size < mRequestedSize/* && mRequestedDiscard == 0*/)
			{
				mHaveAllData = TRUE;
			}
			else if (data_size > mRequestedSize)
			{
				// *TODO: This shouldn't be happening any more
				LL_WARNS(LOG_TXT) << "data_size = " << data_size << " > requested: " << mRequestedSize << LL_ENDL;
				mHaveAllData = TRUE;
				llassert_always(mDecodeHandle == 0);
				mFormattedImage = NULL; // discard any previous data we had
			}
		}
		else
		{
			// We requested data but received none (and no error),
			// so presumably we have all of it
			mHaveAllData = TRUE;
		}
		mRequestedSize = data_size;
	}
	else
	{
		mRequestedSize = -1; // error
	}
	mLoaded = TRUE;
	setPriority(LLWorkerThread::PRIORITY_HIGH | mWorkPriority);

	LLViewerStatsRecorder::instance().log(0.2f);
	return data_size ;
}

//////////////////////////////////////////////////////////////////////////////

void LLTextureFetchWorker::callbackCacheRead(bool success, LLImageFormatted* image,
											 S32 imagesize, BOOL islocal)
{
	LLMutexLock lock(&mWorkMutex);
	if (mState != LOAD_FROM_TEXTURE_CACHE)
	{
// 		LL_WARNS() << "Read callback for " << mID << " with state = " << mState << LL_ENDL;
		return;
	}
	if (success)
	{
		llassert_always(imagesize >= 0);
		mFileSize = imagesize;
		mFormattedImage = image;
		mImageCodec = image->getCodec();
		mInLocalCache = islocal;
		if (mFileSize != 0 && mFormattedImage->getDataSize() >= mFileSize)
		{
			mHaveAllData = TRUE;
		}
	}
	mLoaded = TRUE;
	setPriority(LLWorkerThread::PRIORITY_HIGH | mWorkPriority);
}

void LLTextureFetchWorker::callbackCacheWrite(bool success)
{
	LLMutexLock lock(&mWorkMutex);
	if (mState != WAIT_ON_WRITE)
	{
// 		LL_WARNS() << "Write callback for " << mID << " with state = " << mState << LL_ENDL;
		return;
	}
	mWritten = TRUE;
	setPriority(LLWorkerThread::PRIORITY_HIGH | mWorkPriority);
}

//////////////////////////////////////////////////////////////////////////////

void LLTextureFetchWorker::callbackDecoded(bool success, LLImageRaw* raw, LLImageRaw* aux)
{
	LLMutexLock lock(&mWorkMutex);
	if (mDecodeHandle == 0)
	{
		return; // aborted, ignore
	}
	if (mState != DECODE_IMAGE_UPDATE)
	{
// 		LL_WARNS() << "Decode callback for " << mID << " with state = " << mState << LL_ENDL;
		mDecodeHandle = 0;
		return;
	}
	llassert_always(mFormattedImage.notNull());
	
	mDecodeHandle = 0;
	if (success)
	{
		llassert_always(raw);
		mRawImage = raw;
		mAuxImage = aux;
		mDecodedDiscard = mFormattedImage->getDiscardLevel();
 		LL_DEBUGS(LOG_TXT) << mID << ": Decode Finished. Discard: " << mDecodedDiscard
							 << " Raw Image: " << llformat("%dx%d",mRawImage->getWidth(),mRawImage->getHeight()) << LL_ENDL;
	}
	else
	{
		if (mFormattedImage.notNull())
		{
			LL_WARNS(LOG_TXT) << "DECODE FAILED: id = " << mID << ", Discard = " << (S32)mFormattedImage->getDiscardLevel() << LL_ENDL;
		}
		else
		{
			LL_WARNS(LOG_TXT) << "DECODE FAILED: id = " << mID << ", mFormattedImage is Null!" << LL_ENDL;
		}
		removeFromCache();
		mDecodedDiscard = -1; // Redundant, here for clarity and paranoia
	}
	mDecoded = TRUE;
// 	LL_INFOS() << mID << " : DECODE COMPLETE " << LL_ENDL;
	setPriority(LLWorkerThread::PRIORITY_HIGH | mWorkPriority);
	mCacheReadTime = mCacheReadTimer.getElapsedTimeF32();
}

//////////////////////////////////////////////////////////////////////////////

bool LLTextureFetchWorker::writeToCacheComplete()
{
	// Complete write to cache
	if (mCacheWriteHandle != LLTextureCache::nullHandle())
	{
		if (!mWritten)
		{
			return false;
		}
		if (mFetcher->mTextureCache->writeComplete(mCacheWriteHandle))
		{
			mCacheWriteHandle = LLTextureCache::nullHandle();
		}
		else
		{
			return false;
		}
	}
	return true;
}


// Threads:  Ttf
void LLTextureFetchWorker::recordTextureStart(bool is_http)
{
	if (! mMetricsStartTime.value())
	{
		mMetricsStartTime = LLViewerAssetStatsFF::get_timestamp();
	}
	LLViewerAssetStatsFF::record_enqueue_thread1(LLViewerAssetType::AT_TEXTURE,
												 is_http,
												 LLImageBase::TYPE_AVATAR_BAKE == mType);
}


// Threads:  Ttf
void LLTextureFetchWorker::recordTextureDone(bool is_http)
{
	if (mMetricsStartTime.value())
	{
		LLViewerAssetStatsFF::record_response_thread1(LLViewerAssetType::AT_TEXTURE,
													  is_http,
													  LLImageBase::TYPE_AVATAR_BAKE == mType,
													  LLViewerAssetStatsFF::get_timestamp() - mMetricsStartTime);
		mMetricsStartTime = (U32Seconds)0;
	}
	LLViewerAssetStatsFF::record_dequeue_thread1(LLViewerAssetType::AT_TEXTURE,
												 is_http,
												 LLImageBase::TYPE_AVATAR_BAKE == mType);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
// public

LLTextureFetch::LLTextureFetch(LLTextureCache* cache, LLImageDecodeThread* imagedecodethread, bool threaded, bool qa_mode)
	: LLWorkerThread("TextureFetch", threaded, true),
	  mDebugCount(0),
	  mDebugPause(FALSE),
	  mPacketCount(0),
	  mBadPacketCount(0),
	  mTextureCache(cache),
	  mImageDecodeThread(imagedecodethread),
	  mTotalHTTPRequests(0),
	  mQAMode(qa_mode),
	  mTotalCacheReadCount(0U),
	  mTotalCacheWriteCount(0U)
{
	mTextureInfo.setUpLogging(gSavedSettings.getBOOL("LogTextureDownloadsToViewerLog"), gSavedSettings.getBOOL("LogTextureDownloadsToSimulator"), U32Bytes(gSavedSettings.getU32("TextureLoggingThreshold")));
}

LLTextureFetch::~LLTextureFetch()
{
	clearDeleteList() ;

	while (! mCommands.empty())
	{
		TFRequest * req(mCommands.front());
		mCommands.pop_front();
		delete req;
	}
	
	// ~LLQueuedThread() called here
}

bool LLTextureFetch::createRequest(FTType f_type, const std::string& url, const LLUUID& id, const LLHost& host, F32 priority,
								   S32 w, S32 h, S32 c, S32 desired_discard, bool needs_aux, bool can_use_http)
{
	if (mDebugPause)
	{
		return false;
	}
	if (f_type == FTT_SERVER_BAKE)
	{
		LL_DEBUGS("Avatar") << " requesting " << id << " " << w << "x" << h << " discard " << desired_discard << " type " << f_type << LL_ENDL;
	}
	LLTextureFetchWorker* worker = getWorker(id) ;
	if (worker)
	{
		if (worker->mHost != host)
		{
			LL_WARNS(LOG_TXT) << "LLTextureFetch::createRequest " << id << " called with multiple hosts: "
					<< host << " != " << worker->mHost << LL_ENDL;
			removeRequest(worker, true);
			worker = NULL;
			return false;
		}
	}

	S32 desired_size;
	std::string exten = gDirUtilp->getExtension(url);
	//if (f_type == FTT_SERVER_BAKE)
    if ((f_type == FTT_SERVER_BAKE) && !url.empty() && !exten.empty() && (LLImageBase::getCodecFromExtension(exten) != IMG_CODEC_J2C))
	{
		// SH-4030: This case should be redundant with the following one, just
		// breaking it out here to clarify that it's intended behavior.
		llassert(!url.empty() && (!exten.empty() && LLImageBase::getCodecFromExtension(exten) != IMG_CODEC_J2C));

		// Do full requests for baked textures to reduce interim blurring.
		LL_DEBUGS(LOG_TXT) << "full request for " << id << " texture is FTT_SERVER_BAKE" << LL_ENDL;
		desired_size = MAX_IMAGE_DATA_SIZE;
		desired_discard = 0;
	}
	else if (!url.empty() && (!exten.empty() && LLImageBase::getCodecFromExtension(exten) != IMG_CODEC_J2C))
	{
		LL_DEBUGS(LOG_TXT) << "full request for " << id << " exten is not J2C: " << exten << LL_ENDL;
		// Only do partial requests for J2C at the moment
		//LL_INFOS() << "Merov : LLTextureFetch::createRequest(), blocking fetch on " << url << LL_ENDL; 
		desired_size = MAX_IMAGE_DATA_SIZE;
		desired_discard = 0;
	}
	else if (desired_discard == 0)
	{
		// if we want the entire image, and we know its size, then get it all
		// (calcDataSizeJ2C() below makes assumptions about how the image
		// was compressed - this code ensures that when we request the entire image,
		// we really do get it.)
		desired_size = MAX_IMAGE_DATA_SIZE;
	}
	else if (w*h*c > 0)
	{
		// If the requester knows the dimensions of the image,
		// this will calculate how much data we need without having to parse the header

		desired_size = LLImageJ2C::calcDataSizeJ2C(w, h, c, desired_discard);
	}
	else
	{
		desired_size = TEXTURE_CACHE_ENTRY_SIZE;
		desired_discard = MAX_DISCARD_LEVEL;
	}

	
	if (worker)
	{
		if (worker->wasAborted())
		{
			return false; // need to wait for previous aborted request to complete
		}
		worker->lockWorkMutex();
		worker->mActiveCount++;
		worker->mNeedsAux = needs_aux;
		worker->setImagePriority(priority);
		worker->setDesiredDiscard(desired_discard, desired_size);
		worker->setCanUseHTTP(can_use_http) ;
		if (!worker->haveWork())
		{
			worker->setState(LLTextureFetchWorker::INIT);
			worker->unlockWorkMutex();									// -Mw
			worker->addWork(0, LLWorkerThread::PRIORITY_HIGH | worker->mWorkPriority);
		}
		else
		{
			worker->unlockWorkMutex();
		}
	}
	else
	{
		worker = new LLTextureFetchWorker(this, f_type, url, id, host, priority, desired_discard, desired_size);
		lockQueue() ;
		mRequestMap[id] = worker;
		unlockQueue() ;

		worker->lockWorkMutex();
		worker->mActiveCount++;
		worker->mNeedsAux = needs_aux;
		worker->setCanUseHTTP(can_use_http) ;
		worker->unlockWorkMutex();
	}
	
 	LL_DEBUGS(LOG_TXT) << "REQUESTED: " << id << " f_type " << fttype_to_string(f_type)
					   << " Discard: " << desired_discard << " size " << desired_size << LL_ENDL;
	return true;
}

// protected
void LLTextureFetch::addToNetworkQueue(LLTextureFetchWorker* worker)
{
	lockQueue();
	bool in_request_map = (mRequestMap.find(worker->mID) != mRequestMap.end());
	unlockQueue();
	LLMutexLock lock(&mNetworkQueueMutex);
	if (in_request_map)
	{
		// only add to the queue if in the request map
		// i.e. a delete has not been requested
		mNetworkQueue.insert(worker->mID);
	}
	for (cancel_queue_t::iterator iter1 = mCancelQueue.begin();
		 iter1 != mCancelQueue.end(); ++iter1)
	{
		iter1->second.erase(worker->mID);
	}
}

void LLTextureFetch::removeFromNetworkQueue(LLTextureFetchWorker* worker, bool cancel)
{
	LLMutexLock lock(&mNetworkQueueMutex);
	size_t erased = mNetworkQueue.erase(worker->mID);
	if (cancel && erased > 0)
	{
		mCancelQueue[worker->mHost].insert(worker->mID);
	}
}

// protected
void LLTextureFetch::addToHTTPQueue(const LLUUID& id)
{
	LLMutexLock lock(&mNetworkQueueMutex);
	mHTTPTextureQueue.insert(id);
	mTotalHTTPRequests++;
}

void LLTextureFetch::removeFromHTTPQueue(const LLUUID& id, S32 received_size)
{
	LLMutexLock lock(&mNetworkQueueMutex);
	mHTTPTextureQueue.erase(id);
}

void LLTextureFetch::deleteRequest(const LLUUID& id, bool cancel)
{
	lockQueue() ;
	LLTextureFetchWorker* worker = getWorkerAfterLock(id);

	removeRequest(worker, cancel, false);
}

void LLTextureFetch::removeRequest(LLTextureFetchWorker* worker, bool cancel, bool bNeedsLock)
{
	if(!worker)
	{
		if(!bNeedsLock)
			unlockQueue() ;
		return;
	}
	if(bNeedsLock)
		lockQueue() ;

	size_t erased_1 = mRequestMap.erase(worker->mID);
	unlockQueue() ;

	llassert_always(erased_1 > 0) ;
	removeFromNetworkQueue(worker, cancel);
	llassert_always(!(worker->getFlags(LLWorkerClass::WCF_DELETE_REQUESTED))) ;

	worker->scheduleDelete();	
}

void LLTextureFetch::deleteAllRequests()
{
	while(1)
	{
		lockQueue();
		if(mRequestMap.empty())
		{
			unlockQueue() ;
			break;
		}

		LLTextureFetchWorker* worker = mRequestMap.begin()->second;

		removeRequest(worker, true, false);
	}
}

S32 LLTextureFetch::getNumRequests() 
{ 
	lockQueue() ;
	S32 size = (S32)mRequestMap.size(); 
	unlockQueue() ;

	return size ;
}

// Threads:  T*
S32 LLTextureFetch::getNumHTTPRequests() 
{ 
	mNetworkQueueMutex.lock();											// +Mfq
	S32 size = (S32)mHTTPTextureQueue.size(); 
	mNetworkQueueMutex.unlock();										// -Mfq

	return size;
}

// Threads:  T*
U32 LLTextureFetch::getTotalNumHTTPRequests()
{
	mNetworkQueueMutex.lock() ;
	U32 size = mTotalHTTPRequests ;
	mNetworkQueueMutex.unlock() ;

	return size ;
}

// call lockQueue() first!
LLTextureFetchWorker* LLTextureFetch::getWorkerAfterLock(const LLUUID& id)
{
	LLTextureFetchWorker* res = NULL;
	map_t::iterator iter = mRequestMap.find(id);
	if (iter != mRequestMap.end())
	{
		res = iter->second;
	}
	return res;
}

LLTextureFetchWorker* LLTextureFetch::getWorker(const LLUUID& id)
{
	LLMutexLock lock(&mQueueMutex) ;

	return getWorkerAfterLock(id) ;
}


bool LLTextureFetch::getRequestFinished(const LLUUID& id, S32& discard_level,
										LLPointer<LLImageRaw>& raw, LLPointer<LLImageRaw>& aux)
{
	bool res = false;
	LLTextureFetchWorker* worker = getWorker(id);
	if (worker)
	{
		if (worker->wasAborted())
		{
			res = true;
		}
		else if (!worker->haveWork())
		{
			// Should only happen if we set mDebugPause...
			if (!mDebugPause)
			{
// 				LL_WARNS() << "Adding work for inactive worker: " << id << LL_ENDL;
				worker->addWork(0, LLWorkerThread::PRIORITY_HIGH | worker->mWorkPriority);
			}
		}
		else if (worker->checkWork())
		{
			worker->lockWorkMutex();
			discard_level = worker->mDecodedDiscard;
			raw = worker->mRawImage;
			aux = worker->mAuxImage;
			F32 cache_read_time = worker->mCacheReadTime;
			if (cache_read_time != 0.f)
			{
				sCacheReadLatency.addValue(cache_read_time * 1000.f);
			}
			res = true;
			LL_DEBUGS("Texture") << id << ": Request Finished. State: " << worker->mState << " Discard: " << discard_level << LL_ENDL;
			worker->unlockWorkMutex();
		}
		else
		{
			worker->lockWorkMutex();
			if ((worker->mDecodedDiscard >= 0) &&
				(worker->mDecodedDiscard < discard_level || discard_level < 0) &&
				(worker->mState >= LLTextureFetchWorker::WAIT_ON_WRITE))
			{
				// Not finished, but data is ready
				discard_level = worker->mDecodedDiscard;
				raw = worker->mRawImage;
				aux = worker->mAuxImage;
			}
			worker->unlockWorkMutex();
		}
	}
	else
	{
		res = true;
	}
	return res;
}

bool LLTextureFetch::updateRequestPriority(const LLUUID& id, F32 priority)
{
	bool res = false;
	LLTextureFetchWorker* worker = getWorker(id);
	if (worker)
	{
		worker->lockWorkMutex();
		worker->setImagePriority(priority);
		worker->unlockWorkMutex();
		res = true;
	}
	return res;
}

//
// May be called from any thread

//virtual
S32 LLTextureFetch::getPending()
{
	S32 res;
	lockData();
    {
        LLMutexLock lock(&mQueueMutex);
        
        res = mRequestQueue.size();
        res += mCommands.size();
    }																	// -Mfq
	unlockData();														// -Ct
	return res;
}

// Locks:  Ct
// virtual
bool LLTextureFetch::runCondition()
{
	// Caller is holding the lock on LLThread's condition variable.
	
	// LLQueuedThread, unlike its base class LLThread, makes this a
	// private method which is unfortunate.  I want to use it directly
	// but I'm going to have to re-implement the logic here (or change
	// declarations, which I don't want to do right now).
	//
	// Changes here may need to be reflected in getPending().
	
	bool have_no_commands(false);
	{
		LLMutexLock lock(&mQueueMutex);									// +Mfq
		
		have_no_commands = mCommands.empty();
	}																	// -Mfq
	
	return ! (have_no_commands
			  && (mRequestQueue.empty() && mIdleThread));		// From base class
}

//////////////////////////////////////////////////////////////////////////////

// MAIN THREAD (unthreaded envs), WORKER THREAD (threaded envs)
void LLTextureFetch::commonUpdate()
{
	// Run a cross-thread command, if any.
	cmdDoWork();
}


// MAIN THREAD
//virtual
S32 LLTextureFetch::update(F32 max_time_ms)
{
	S32 res = LLWorkerThread::update(max_time_ms);
	
	if (!mDebugPause)
	{
		// this is the startup state when send_complete_agent_movement() message is sent.
		// Before this, the RequestImages message sent by sendRequestListToSimulators 
		// won't work so don't bother trying
		if (LLStartUp::getStartupState() > STATE_AGENT_SEND)
		{
			sendRequestListToSimulators();
		}
	}

	if (!mThreaded)
	{
		commonUpdate();
	}

	return res;
}

//called in the MAIN thread after the TextureCacheThread shuts down.
void LLTextureFetch::shutDownTextureCacheThread() 
{
	if(mTextureCache)
	{
		llassert_always(mTextureCache->isQuitting() || mTextureCache->isStopped()) ;
		mTextureCache = NULL ;
	}
}
	
//called in the MAIN thread after the ImageDecodeThread shuts down.
void LLTextureFetch::shutDownImageDecodeThread() 
{
	if(mImageDecodeThread)
	{
		llassert_always(mImageDecodeThread->isQuitting() || mImageDecodeThread->isStopped()) ;
		mImageDecodeThread = NULL ;
	}
}

// Threads:  Ttf
void LLTextureFetch::startThread()
{
}

// Threads:  Ttf
void LLTextureFetch::endThread()
{
	LL_INFOS(LOG_TXT) << "CacheReads:  " << mTotalCacheReadCount
					  << ", CacheWrites:  " << mTotalCacheWriteCount
					  << ", TotalHTTPReq:  " << getTotalNumHTTPRequests()
					  << LL_ENDL;
}

// Threads:  Ttf
void LLTextureFetch::threadedUpdate()
{
	// Limit update frequency
	const F32 PROCESS_TIME = 0.05f; 
	static LLFrameTimer process_timer;
	if (process_timer.getElapsedTimeF32() < PROCESS_TIME)
	{
		return;
	}
	process_timer.reset();
	
	commonUpdate();

#if 0
	const F32 INFO_TIME = 1.0f; 
	static LLFrameTimer info_timer;
	if (info_timer.getElapsedTimeF32() >= INFO_TIME)
	{
		S32 q = mCurlGetRequest->getQueued();
		if (q > 0)
		{
			LL_INFOS(LOG_TXT) << "Queued gets: " << q << LL_ENDL;
			info_timer.reset();
		}
	}
#endif
	
}

//////////////////////////////////////////////////////////////////////////////

void LLTextureFetch::sendRequestListToSimulators()
{
	// All requests
	const F32 REQUEST_DELTA_TIME = 0.10f; // 10 fps
	
	// Sim requests
	const S32 IMAGES_PER_REQUEST = 50;
	const F32 SIM_LAZY_FLUSH_TIMEOUT = 10.0f; // temp
	const F32 MIN_REQUEST_TIME = 1.0f;
	const F32 MIN_DELTA_PRIORITY = 1000.f;

	// Periodically, gather the list of textures that need data from the network
	// And send the requests out to the simulators
	static LLFrameTimer timer;
	if (timer.getElapsedTimeF32() < REQUEST_DELTA_TIME)
	{
		return;
	}
	timer.reset();
	
	// Send requests
	typedef std::set<LLTextureFetchWorker*,LLTextureFetchWorker::Compare> request_list_t;
	typedef std::map< LLHost, request_list_t > work_request_map_t;
	work_request_map_t requests;
	{
	LLMutexLock lock2(&mNetworkQueueMutex);
	for (queue_t::iterator iter = mNetworkQueue.begin(); iter != mNetworkQueue.end(); )
	{
		queue_t::iterator curiter = iter++;
		LLTextureFetchWorker* req = getWorker(*curiter);
		if (!req)
		{
			// This happens when a request was removed from mRequestMap in a race
			// with adding it to mNetworkQueue by doWork (see SNOW-196).
			mNetworkQueue.erase(curiter);
			continue;
		}
		if ((req->mState != LLTextureFetchWorker::SEND_UDP_REQ) &&
			(req->mState != LLTextureFetchWorker::WAIT_UDP_REQ) &&	//Workers remain in the queue. May be re-requested upon timeout.
			(req->mState != LLTextureFetchWorker::LOAD_FROM_SIMULATOR))
		{
			// We already received our URL, remove from the queue
				LL_WARNS(LOG_TXT) << "Worker: " << req->mID << " in mNetworkQueue but in wrong state: " << req->mState << LL_ENDL;
			mNetworkQueue.erase(curiter);
			continue;
		}
		if (req->mID == mDebugID)
		{
			mDebugCount++; // for setting breakpoints
		}
		if (req->mSentRequest == LLTextureFetchWorker::SENT_SIM &&
			req->mTotalPackets > 0 &&
			req->mLastPacket >= req->mTotalPackets-1)
		{
			// We have all the packets... make sure this is high priority
// 			req->setPriority(LLWorkerThread::PRIORITY_HIGH | req->mWorkPriority);
			continue;
		}
		F32 elapsed = req->mRequestedTimer.getElapsedTimeF32();
		{
			F32 delta_priority = llabs(req->mRequestedPriority - req->mImagePriority);
			if ((req->mSimRequestedDiscard != req->mDesiredDiscard) ||
				(delta_priority > MIN_DELTA_PRIORITY && elapsed >= MIN_REQUEST_TIME) ||
				(elapsed >= SIM_LAZY_FLUSH_TIMEOUT))
			{
				requests[req->mHost].insert(req);
			}
		}
	}
	}

	for (work_request_map_t::iterator iter1 = requests.begin();
		 iter1 != requests.end(); ++iter1)
	{
		LLHost host = iter1->first;
		// invalid host = use agent host
		if (host == LLHost::invalid)
		{
			host = gAgent.getRegionHost();
		}

		S32 sim_request_count = 0;
		
		for (request_list_t::iterator iter2 = iter1->second.begin();
			 iter2 != iter1->second.end(); ++iter2)
		{
			LLTextureFetchWorker* req = *iter2;
			if (gMessageSystem)
			{
				if (req->mSentRequest != LLTextureFetchWorker::SENT_SIM)
				{
					// Initialize packet data based on data read from cache
					req->lockWorkMutex();
					req->setupPacketData();
					req->unlockWorkMutex();
				}
				if (0 == sim_request_count)
				{
					gMessageSystem->newMessageFast(_PREHASH_RequestImage);
					gMessageSystem->nextBlockFast(_PREHASH_AgentData);
					gMessageSystem->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
					gMessageSystem->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
				}
				S32 packet = req->mLastPacket + 1;
				gMessageSystem->nextBlockFast(_PREHASH_RequestImage);
				gMessageSystem->addUUIDFast(_PREHASH_Image, req->mID);
				gMessageSystem->addS8Fast(_PREHASH_DiscardLevel, (S8)req->mDesiredDiscard);
				gMessageSystem->addF32Fast(_PREHASH_DownloadPriority, req->mImagePriority);
				gMessageSystem->addU32Fast(_PREHASH_Packet, packet);
				gMessageSystem->addU8Fast(_PREHASH_Type, req->mType);
// 				LL_INFOS() << "IMAGE REQUEST: " << req->mID << " Discard: " << req->mDesiredDiscard
// 						<< " Packet: " << packet << " Priority: " << req->mImagePriority << LL_ENDL;

				static LLCachedControl<bool> log_to_viewer_log(gSavedSettings,"LogTextureDownloadsToViewerLog");
				static LLCachedControl<bool> log_to_sim(gSavedSettings,"LogTextureDownloadsToSimulator");
				if (log_to_viewer_log || log_to_sim)
				{
					mTextureInfo.setRequestStartTime(req->mID, LLTimer::getTotalTime());
					mTextureInfo.setRequestOffset(req->mID, 0);
					mTextureInfo.setRequestSize(req->mID, 0);
					mTextureInfo.setRequestType(req->mID, LLTextureInfoDetails::REQUEST_TYPE_UDP);
				}

				req->lockWorkMutex();
				req->mSentRequest = LLTextureFetchWorker::SENT_SIM;
				req->mSimRequestedDiscard = req->mDesiredDiscard;
				req->mRequestedPriority = req->mImagePriority;
				req->mRequestedTimer.reset();
				req->unlockWorkMutex();
				sim_request_count++;
				if (sim_request_count >= IMAGES_PER_REQUEST)
				{
// 					LL_INFOS() << "REQUESTING " << sim_request_count << " IMAGES FROM HOST: " << host.getIPString() << LL_ENDL;

					gMessageSystem->sendSemiReliable(host, NULL, NULL);
					sim_request_count = 0;
				}
			}
		}
		if (gMessageSystem && sim_request_count > 0 && sim_request_count < IMAGES_PER_REQUEST)
		{
// 			LL_INFOS() << "REQUESTING " << sim_request_count << " IMAGES FROM HOST: " << host.getIPString() << LL_ENDL;
			gMessageSystem->sendSemiReliable(host, NULL, NULL);
			sim_request_count = 0;
		}
	}
	
	// Send cancelations
	{
	LLMutexLock lock2(&mNetworkQueueMutex);
	if (gMessageSystem && !mCancelQueue.empty())
	{
		for (cancel_queue_t::iterator iter1 = mCancelQueue.begin();
			 iter1 != mCancelQueue.end(); ++iter1)
		{
			LLHost host = iter1->first;
			if (host == LLHost::invalid)
			{
				host = gAgent.getRegionHost();
			}
			S32 request_count = 0;
			for (queue_t::iterator iter2 = iter1->second.begin();
				 iter2 != iter1->second.end(); ++iter2)
			{
				if (0 == request_count)
				{
					gMessageSystem->newMessageFast(_PREHASH_RequestImage);
					gMessageSystem->nextBlockFast(_PREHASH_AgentData);
					gMessageSystem->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
					gMessageSystem->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
				}
				gMessageSystem->nextBlockFast(_PREHASH_RequestImage);
				gMessageSystem->addUUIDFast(_PREHASH_Image, *iter2);
				gMessageSystem->addS8Fast(_PREHASH_DiscardLevel, -1);
				gMessageSystem->addF32Fast(_PREHASH_DownloadPriority, 0);
				gMessageSystem->addU32Fast(_PREHASH_Packet, 0);
				gMessageSystem->addU8Fast(_PREHASH_Type, 0);
// 				LL_INFOS() << "CANCELING IMAGE REQUEST: " << (*iter2) << LL_ENDL;

				request_count++;
				if (request_count >= IMAGES_PER_REQUEST)
				{
					gMessageSystem->sendSemiReliable(host, NULL, NULL);
					request_count = 0;
				}
			}
			if (request_count > 0 && request_count < IMAGES_PER_REQUEST)
			{
				gMessageSystem->sendSemiReliable(host, NULL, NULL);
			}
		}
		mCancelQueue.clear();
	}
	}
}

//////////////////////////////////////////////////////////////////////////////

bool LLTextureFetchWorker::insertPacket(S32 index, U8* data, S32 size)
{
	mRequestedTimer.reset();
	if (index >= mTotalPackets)
	{
// 		LL_WARNS() << "Received Image Packet " << index << " > max: " << mTotalPackets << " for image: " << mID << LL_ENDL;
		return false;
	}
	if (index > 0 && index < mTotalPackets-1 && size != MAX_IMG_PACKET_SIZE)
	{
// 		LL_WARNS() << "Received bad sized packet: " << index << ", " << size << " != " << MAX_IMG_PACKET_SIZE << " for image: " << mID << LL_ENDL;
		return false;
	}
	
	if (index >= (S32)mPackets.size())
	{
		mPackets.resize(index+1, (PacketData*)NULL); // initializes v to NULL pointers
	}
	else if (mPackets[index] != NULL)
	{
// 		LL_WARNS() << "Received duplicate packet: " << index << " for image: " << mID << LL_ENDL;
		return false;
	}

	mPackets[index] = new PacketData(data, size);
	while (mLastPacket+1 < (S32)mPackets.size() && mPackets[mLastPacket+1] != NULL)
	{
		++mLastPacket;
	}
	return true;
}

void LLTextureFetchWorker::setState(e_state new_state)
{
	/*
	static const char* e_state_name[] =
	{
		"INVALID",
		"INIT",
		"LOAD_FROM_TEXTURE_CACHE",
		"CACHE_POST",
		"LOAD_FROM_NETWORK",
		"LOAD_FROM_SIMULATOR",
		"SEND_UDP_REQ",
		"WAIT_UDP_REQ",
		"SEND_HTTP_REQ",
		"WAIT_HTTP_REQ",
		"DECODE_IMAGE",
		"DECODE_IMAGE_UPDATE",
		"WRITE_TO_CACHE",
		"WAIT_ON_WRITE",
		"DONE"
	};
	*/
	//if(mState != new_state)
	//	LL_INFOS("Texture") << "id: " << mID << " disc: " << mDesiredDiscard << " sz: " << mDesiredSize << " state: " << e_state_name[mState] << " => " << e_state_name[new_state] << LL_ENDL;
	mState = new_state;
}

// Threads:  T*
bool LLTextureFetch::receiveImageHeader(const LLHost& host, const LLUUID& id, U8 codec, U16 packets, U32 totalbytes,
										U16 data_size, U8* data)
{
	LLTextureFetchWorker* worker = getWorker(id);
	bool res = true;

	++mPacketCount;
	
	if (!worker)
	{
// 		LL_WARNS() << "Received header for non active worker: " << id << LL_ENDL;
		res = false;
	}
	else if (worker->mState != LLTextureFetchWorker::WAIT_UDP_REQ ||
			 worker->mSentRequest != LLTextureFetchWorker::SENT_SIM)
	{
// 		LL_WARNS() << "receiveImageHeader for worker: " << id
// 				<< " in state: " << LLTextureFetchWorker::sStateDescs[worker->mState]
// 				<< " sent: " << worker->mSentRequest << LL_ENDL;
		res = false;
	}
	else if (worker->mLastPacket != -1)
	{
		// check to see if we've gotten this packet before
// 		LL_WARNS() << "Received duplicate header for: " << id << LL_ENDL;
		res = false;
	}
	else if (!data_size)
	{
// 		LL_WARNS() << "Img: " << id << ":" << " Empty Image Header" << LL_ENDL;
		res = false;
	}
	if (!res)
	{
		mNetworkQueueMutex.lock();										// +Mfnq
		++mBadPacketCount;
		mCancelQueue[host].insert(id);
		mNetworkQueueMutex.unlock();									// -Mfnq 
		return false;
	}

	LLViewerStatsRecorder::instance().textureFetch(data_size);
	LLViewerStatsRecorder::instance().log(0.1f);

	worker->lockWorkMutex();


	//	Copy header data into image object
	worker->mImageCodec = codec;
	worker->mTotalPackets = packets;
	worker->mFileSize = (S32)totalbytes;	
	llassert_always(totalbytes > 0);
	llassert_always(data_size == FIRST_PACKET_SIZE || data_size == worker->mFileSize);
	res = worker->insertPacket(0, data, data_size);
	worker->setPriority(LLWorkerThread::PRIORITY_HIGH | worker->mWorkPriority);
	worker->setState(LLTextureFetchWorker::LOAD_FROM_SIMULATOR);
	worker->unlockWorkMutex();											// -Mw
	return res;
}

bool LLTextureFetch::receiveImagePacket(const LLHost& host, const LLUUID& id, U16 packet_num, U16 data_size, U8* data)
{
	LLTextureFetchWorker* worker = getWorker(id);
	bool res = true;

	++mPacketCount;
	
	if (!worker)
	{
// 		LL_WARNS() << "Received packet " << packet_num << " for non active worker: " << id << LL_ENDL;
		res = false;
	}
	else if (worker->mLastPacket == -1)
	{
// 		LL_WARNS() << "Received packet " << packet_num << " before header for: " << id << LL_ENDL;
		res = false;
	}
	else if (!data_size)
	{
// 		LL_WARNS() << "Img: " << id << ":" << " Empty Image Header" << LL_ENDL;
		res = false;
	}
	if (!res)
	{
		mNetworkQueueMutex.lock();										// +Mfnq
		++mBadPacketCount;
		mCancelQueue[host].insert(id);
		mNetworkQueueMutex.unlock();									// -Mfnq
		return false;
	}
	
	LLViewerStatsRecorder::instance().textureFetch(data_size);
	LLViewerStatsRecorder::instance().log(0.1f);

	worker->lockWorkMutex();

	
	res = worker->insertPacket(packet_num, data, data_size);
	
	if ((worker->mState == LLTextureFetchWorker::LOAD_FROM_SIMULATOR) ||
		(worker->mState == LLTextureFetchWorker::WAIT_UDP_REQ))
	{
		worker->setPriority(LLWorkerThread::PRIORITY_HIGH | worker->mWorkPriority);
		worker->setState(LLTextureFetchWorker::LOAD_FROM_SIMULATOR);
	}
	else
	{
// 		LL_WARNS() << "receiveImagePacket " << packet_num << "/" << worker->mLastPacket << " for worker: " << id
// 				<< " in state: " << LLTextureFetchWorker::sStateDescs[worker->mState] << LL_ENDL;
		removeFromNetworkQueue(worker, true); // failsafe
	}

	if(packet_num >= (worker->mTotalPackets - 1))
	{
		static LLCachedControl<bool> log_to_viewer_log(gSavedSettings,"LogTextureDownloadsToViewerLog");
		static LLCachedControl<bool> log_to_sim(gSavedSettings,"LogTextureDownloadsToSimulator");

		if (log_to_viewer_log || log_to_sim)
		{
			U64Microseconds timeNow = LLTimer::getTotalTime();
			mTextureInfo.setRequestSize(id, worker->mFileSize);
			mTextureInfo.setRequestCompleteTimeAndLog(id, timeNow);
		}
	}
	worker->unlockWorkMutex();

	return res;
}

//////////////////////////////////////////////////////////////////////////////
BOOL LLTextureFetch::isFromLocalCache(const LLUUID& id)
{
	BOOL from_cache = FALSE;

	LLTextureFetchWorker* worker = getWorker(id);
	if (worker)
	{
		worker->lockWorkMutex();
		from_cache = worker->mInLocalCache;
		worker->unlockWorkMutex();
	}

	return from_cache;
}

S32 LLTextureFetch::getFetchState(const LLUUID& id, F32& data_progress_p, F32& requested_priority_p,
								  U32& fetch_priority_p, F32& fetch_dtime_p, F32& request_dtime_p, bool& can_use_http)
{
	S32 state = LLTextureFetchWorker::INVALID;
	F32 data_progress = 0.0f;
	F32 requested_priority = 0.0f;
	F32 fetch_dtime = 999999.f;
	F32 request_dtime = 999999.f;
	U32 fetch_priority = 0;
	
	LLTextureFetchWorker* worker = getWorker(id);
	if (worker && worker->haveWork())
	{
		worker->lockWorkMutex();
		state = worker->mState;
		fetch_dtime = worker->mFetchTimer.getElapsedTimeF32();
		request_dtime = worker->mRequestedTimer.getElapsedTimeF32();
		if (worker->mFileSize > 0)
		{
			if (state == LLTextureFetchWorker::LOAD_FROM_SIMULATOR)
			{
				S32 data_size = FIRST_PACKET_SIZE + (worker->mLastPacket-1) * MAX_IMG_PACKET_SIZE;
				data_size = llmax(data_size, 0);
				data_progress = (F32)data_size / (F32)worker->mFileSize;
			}
			else if (worker->mFormattedImage.notNull())
			{
				data_progress = (F32)worker->mFormattedImage->getDataSize() / (F32)worker->mFileSize;
			}
		}
		if (state >= LLTextureFetchWorker::LOAD_FROM_NETWORK && state <= LLTextureFetchWorker::WAIT_HTTP_REQ)
		{
			requested_priority = worker->mRequestedPriority;
		}
		else
		{
			requested_priority = worker->mImagePriority;
		}
		fetch_priority = worker->getPriority();
		can_use_http = worker->getCanUseHTTP() ;
		worker->unlockWorkMutex();
	}
	data_progress_p = data_progress;
	requested_priority_p = requested_priority;
	fetch_priority_p = fetch_priority;
	fetch_dtime_p = fetch_dtime;
	request_dtime_p = request_dtime;
	return state;
}

void LLTextureFetch::dump()
{
	LL_INFOS(LOG_TXT) << "LLTextureFetch REQUESTS:" << LL_ENDL;
	for (request_queue_t::iterator iter = mRequestQueue.begin();
		 iter != mRequestQueue.end(); ++iter)
	{
		LLQueuedThread::QueuedRequest* qreq = *iter;
		LLWorkerThread::WorkRequest* wreq = (LLWorkerThread::WorkRequest*)qreq;
		LLTextureFetchWorker* worker = (LLTextureFetchWorker*)wreq->getWorkerClass();
		LL_INFOS(LOG_TXT) << " ID: " << worker->mID
				<< " PRI: " << llformat("0x%08x",wreq->getPriority())
				<< " STATE: " << worker->sStateDescs[worker->mState]
				<< LL_ENDL;
	}

	LL_INFOS(LOG_TXT) << "LLTextureFetch ACTIVE_HTTP:" << LL_ENDL;
	for (queue_t::const_iterator iter(mHTTPTextureQueue.begin());
		 mHTTPTextureQueue.end() != iter;
		 ++iter)
	{
		LL_INFOS(LOG_TXT) << " ID: " << (*iter) << LL_ENDL;
	}
}

// Threads:  T*
void LLTextureFetch::updateStateStats(U32 cache_read, U32 cache_write)
{
	LLMutexLock lock(&mQueueMutex);										// +Mfq

	mTotalCacheReadCount += cache_read;
	mTotalCacheWriteCount += cache_write;
}																		// -Mfq


// Threads:  T*
void LLTextureFetch::getStateStats(U32 * cache_read, U32 * cache_write)
{
	U32 ret1(0U), ret2(0U);
	
	{
		LLMutexLock lock(&mQueueMutex);									// +Mfq
		ret1 = mTotalCacheReadCount;
		ret2 = mTotalCacheWriteCount;
	}																	// -Mfq
	
	*cache_read = ret1;
	*cache_write = ret2;
}
// Threads:  T*
void LLTextureFetch::commandSetRegion(U64 region_handle)
{
	TFReqSetRegion * req = new TFReqSetRegion(region_handle);

	cmdEnqueue(req);
}

// Threads:  T*
void LLTextureFetch::commandSendMetrics(const std::string & caps_url,
										const LLUUID & session_id,
										const LLUUID & agent_id,
										LLViewerAssetStats * main_stats)
{
	TFReqSendMetrics * req = new TFReqSendMetrics(caps_url, session_id, agent_id, main_stats);

	cmdEnqueue(req);
}
// Threads:  T*
void LLTextureFetch::commandDataBreak()
{
	// The pedantically correct way to implement this is to create a command
	// request object in the above fashion and enqueue it.  However, this is
	// simple data of an advisorial not operational nature and this case
	// of shared-write access is tolerable.

	LLTextureFetch::svMetricsDataBreak = true;
}

// Threads:  T*
void LLTextureFetch::cmdEnqueue(TFRequest * req)
{
	lockQueue();														// +Mfq
	mCommands.push_back(req);
	unlockQueue();														// -Mfq

	unpause();
}

// Threads:  T*
LLTextureFetch::TFRequest * LLTextureFetch::cmdDequeue()
{
	TFRequest * ret = 0;
	
	lockQueue();														// +Mfq
	if (! mCommands.empty())
	{
		ret = mCommands.front();
		mCommands.pop_front();
	}
	unlockQueue();														// -Mfq

	return ret;
}

// Threads:  Ttf
void LLTextureFetch::cmdDoWork()
{
	if (mDebugPause)
	{
		return;  // debug: don't do any work
	}

	TFRequest * req = cmdDequeue();
	if (req)
	{
		// One request per pass should really be enough for this.
		req->doWork(this);
		delete req;
	}
}

//////////////////////////////////////////////////////////////////////////////

// Private (anonymous) class methods implementing the command scheme.

namespace
{


// Example of a simple notification handler for metrics
// delivery notification.  Earlier versions of the code used
// a Responder that tried harder to detect delivery breaks
// but it really isn't that important.  If someone wants to
// revisit that effort, here is a place to start.
class AssetReportHandler : public LLHTTPClient::ResponderWithCompleted
{
public:

	// Threads:  Ttf
	/*virtual*/ virtual void httpCompleted(void)
	{
		if (mStatus)
		{
			LL_DEBUGS("Texture") << "Successfully delivered asset metrics to grid."
								<< LL_ENDL;
		}
		else
		{
			LL_WARNS("Texture") << "Error delivering asset metrics to grid.  Reason:  "
								<< mStatus << LL_ENDL;
		}
	}
	
	/*virtual*/ AIHTTPTimeoutPolicy const& getHTTPTimeoutPolicy(void) const { return assetReportHandler_timeout; }
	/*virtual*/ char const* getName(void) const { return "AssetReportHandler"; }
}; // end class AssetReportHandler


/**
 * Implements the 'Set Region' command.
 *
 * Thread:  Thread1 (TextureFetch)
 */
bool
TFReqSetRegion::doWork(LLTextureFetch *)
{
	LLViewerAssetStatsFF::set_region_thread1(mRegionHandle);

	return true;
}


TFReqSendMetrics::~TFReqSendMetrics()
{
	delete mMainStats;
	mMainStats = 0;
}


/**
 * Implements the 'Send Metrics' command.  Takes over
 * ownership of the passed LLViewerAssetStats pointer.
 *
 * Thread:  Thread1 (TextureFetch)
 */
bool
TFReqSendMetrics::doWork(LLTextureFetch * fetcher)
{
	if (! gViewerAssetStatsThread1)
		return true;

	static volatile bool reporting_started(false);
	static volatile S32 report_sequence(0);
    
	// We've taken over ownership of the stats copy at this
	// point.  Get a working reference to it for merging here
	// but leave it in 'this'.  Destructor will rid us of it.
	LLViewerAssetStats & main_stats = *mMainStats;

	// Merge existing stats into those from main, convert to LLSD
	main_stats.merge(*gViewerAssetStatsThread1);
	LLSD merged_llsd = main_stats.asLLSD(true);

	// Add some additional meta fields to the content
	merged_llsd["session_id"] = mSessionID;
	merged_llsd["agent_id"] = mAgentID;
	merged_llsd["message"] = "ViewerAssetMetrics";					// Identifies the type of metrics
	merged_llsd["sequence"] = report_sequence;						// Sequence number
	merged_llsd["initial"] = ! reporting_started;					// Initial data from viewer
	merged_llsd["break"] = LLTextureFetch::svMetricsDataBreak;		// Break in data prior to this report
		
	// Update sequence number
	if (S32_MAX == ++report_sequence)
		report_sequence = 0;
	reporting_started = true;
	
	// Limit the size of the stats report if necessary.
	merged_llsd["truncated"] = truncate_viewer_metrics(10, merged_llsd);

	if (! mCapsURL.empty())
	{
		if(fetcher->isQAMode() || true)	//Assuming the 'true' will vanish eventually.
			LLHTTPClient::post(mCapsURL, merged_llsd, new AssetReportHandler());
		else
			LLHTTPClient::post(mCapsURL, merged_llsd, new LLHTTPClient::ResponderIgnore());
		LLTextureFetch::svMetricsDataBreak = false;
	}
	else
	{
		LLTextureFetch::svMetricsDataBreak = true;
	}

	// In QA mode, Metrics submode, log the result for ease of testing
	if (fetcher->isQAMode())
	{
		LL_INFOS("Textures") << ll_pretty_print_sd(merged_llsd) << LL_ENDL;
	}

	gViewerAssetStatsThread1->reset();

	return true;
}


bool
truncate_viewer_metrics(int max_regions, LLSD & metrics)
{
	static const LLSD::String reg_tag("regions");
	static const LLSD::String duration_tag("duration");
	
	LLSD & reg_map(metrics[reg_tag]);
	if (reg_map.size() <= max_regions)
	{
		return false;
	}

	// Build map of region hashes ordered by duration
	typedef std::multimap<LLSD::Real, int> reg_ordered_list_t;
	reg_ordered_list_t regions_by_duration;

	int ind(0);
	LLSD::array_const_iterator it_end(reg_map.endArray());
	for (LLSD::array_const_iterator it(reg_map.beginArray()); it_end != it; ++it, ++ind)
	{
		LLSD::Real duration = (*it)[duration_tag].asReal();
		regions_by_duration.insert(reg_ordered_list_t::value_type(duration, ind));
	}

	// Build a replacement regions array with the longest-persistence regions
	LLSD new_region(LLSD::emptyArray());
	reg_ordered_list_t::const_reverse_iterator it2_end(regions_by_duration.rend());
	reg_ordered_list_t::const_reverse_iterator it2(regions_by_duration.rbegin());
	for (int i(0); i < max_regions && it2_end != it2; ++i, ++it2)
	{
		new_region.append(reg_map[it2->second]);
	}
	reg_map = new_region;
	
	return true;
}

} // end of anonymous namespace

