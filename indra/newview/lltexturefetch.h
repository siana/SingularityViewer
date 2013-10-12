/** 
 * @file lltexturefetch.h
 * @brief Object for managing texture fetches.
 *
 * $LicenseInfo:firstyear=2000&license=viewergpl$
 * 
 * Copyright (c) 2000-2009, Linden Research, Inc.
 * 
 * Second Life Viewer Source Code
 * The source code in this file ("Source Code") is provided by Linden Lab
 * to you under the terms of the GNU General Public License, version 2.0
 * ("GPL"), unless you have obtained a separate licensing agreement
 * ("Other License"), formally executed by you and Linden Lab.  Terms of
 * the GPL can be found in doc/GPL-license.txt in this distribution, or
 * online at http://secondlifegrid.net/programs/open_source/licensing/gplv2
 * 
 * There are special exceptions to the terms and conditions of the GPL as
 * it is applied to this Source Code. View the full text of the exception
 * in the file doc/FLOSS-exception.txt in this software distribution, or
 * online at
 * http://secondlifegrid.net/programs/open_source/licensing/flossexception
 * 
 * By copying, modifying or distributing this software, you acknowledge
 * that you have read and understood your obligations described above,
 * and agree to abide by those obligations.
 * 
 * ALL LINDEN LAB SOURCE CODE IS PROVIDED "AS IS." LINDEN LAB MAKES NO
 * WARRANTIES, EXPRESS, IMPLIED OR OTHERWISE, REGARDING ITS ACCURACY,
 * COMPLETENESS OR PERFORMANCE.
 * $/LicenseInfo$
 */

#ifndef LL_LLTEXTUREFETCH_H
#define LL_LLTEXTUREFETCH_H

#include <vector>
#include <map>

#include "lldir.h"
#include "llimage.h"
#include "lluuid.h"
#include "llworkerthread.h"
#include "lltextureinfo.h"
#include "llapr.h"
#include "llstat.h"

class LLViewerTexture;
class LLTextureFetchWorker;
class HTTPGetResponder;
class LLImageDecodeThread;
class LLHost;
class LLViewerAssetStats;
class LLTextureCache;

// Interface class
class LLTextureFetch : public LLWorkerThread
{
	friend class LLTextureFetchWorker;
	friend class HTTPGetResponder;
	
public:
	LLTextureFetch(LLTextureCache* cache, LLImageDecodeThread* imagedecodethread, bool threaded, bool qa_mode = false);
	~LLTextureFetch();

	class TFRequest;
	
	/*virtual*/ S32 update(F32 max_time_ms);	
	void shutDownTextureCacheThread() ; //called in the main thread after the TextureCacheThread shuts down.
	void shutDownImageDecodeThread() ;  //called in the main thread after the ImageDecodeThread shuts down.

	bool createRequest(const std::string& url, const LLUUID& id, const LLHost& host, F32 priority,
					   S32 w, S32 h, S32 c, S32 discard, bool needs_aux, bool can_use_http);
	void deleteRequest(const LLUUID& id, bool cancel);
	void deleteAllRequests();
	bool getRequestFinished(const LLUUID& id, S32& discard_level,
							LLPointer<LLImageRaw>& raw, LLPointer<LLImageRaw>& aux);
	bool updateRequestPriority(const LLUUID& id, F32 priority);

	bool receiveImageHeader(const LLHost& host, const LLUUID& id, U8 codec, U16 packets, U32 totalbytes, U16 data_size, U8* data);
	bool receiveImagePacket(const LLHost& host, const LLUUID& id, U16 packet_num, U16 data_size, U8* data);

	// Debug
	BOOL isFromLocalCache(const LLUUID& id);
	S32 getFetchState(const LLUUID& id, F32& decode_progress_p, F32& requested_priority_p,
					  U32& fetch_priority_p, F32& fetch_dtime_p, F32& request_dtime_p, bool& can_use_http);
	void dump();
	// Threads:  T*
	S32 getNumRequests();

	// Threads:  T*
	S32 getNumHTTPRequests();

	// Threads:  T*
	U32 getTotalNumHTTPRequests();
	
	// Public for access by callbacks
    S32 getPending();
	void lockQueue() { mQueueMutex.lock(); }
	void unlockQueue() { mQueueMutex.unlock(); }
	LLTextureFetchWorker* getWorker(const LLUUID& id);
	LLTextureFetchWorker* getWorkerAfterLock(const LLUUID& id);

	// Commands available to other threads to control metrics gathering operations.

	// Threads:  T*
	void commandSetRegion(U64 region_handle);

	// Threads:  T*
	void commandSendMetrics(const std::string & caps_url,
							const LLUUID & session_id,
							const LLUUID & agent_id,
							LLViewerAssetStats * main_stats);

	// Threads:  T*
	void commandDataBreak();

	bool isQAMode() const				{ return mQAMode; }
	void updateStateStats(U32 cache_read, U32 cache_write);
	void getStateStats(U32 * cache_read, U32 * cache_write);
protected:
	void addToNetworkQueue(LLTextureFetchWorker* worker);
	void removeFromNetworkQueue(LLTextureFetchWorker* worker, bool cancel);
	void addToHTTPQueue(const LLUUID& id);
	void removeFromHTTPQueue(const LLUUID& id, S32 received_size = 0);
	void removeRequest(LLTextureFetchWorker* worker, bool cancel, bool bNeedsLock = true);

	// Overrides from the LLThread tree
	bool runCondition();

private:
	void sendRequestListToSimulators();
	/*virtual*/ void startThread(void);
	/*virtual*/ void endThread(void);
	/*virtual*/ void threadedUpdate(void);
	void commonUpdate();

	void cmdEnqueue(TFRequest *);
	TFRequest * cmdDequeue();
	void cmdDoWork();
	
public:
	LLUUID mDebugID;
	S32 mDebugCount;
	BOOL mDebugPause;
	S32 mPacketCount;
	S32 mBadPacketCount;
	
private:
	LLMutex mQueueMutex;        //to protect mRequestMap only
	LLMutex mNetworkQueueMutex; //to protect mNetworkQueue, mHTTPTextureQueue and mCancelQueue.

public:
	static LLStat sCacheHitRate;
	static LLStat sCacheReadLatency;
private:

	LLTextureCache* mTextureCache;
	LLImageDecodeThread* mImageDecodeThread;
	
	// Map of all requests by UUID
	typedef std::map<LLUUID,LLTextureFetchWorker*> map_t;
	map_t mRequestMap;

	// Set of requests that require network data
	typedef std::set<LLUUID> queue_t;
	queue_t mNetworkQueue;
	queue_t mHTTPTextureQueue;
	typedef std::map<LLHost,std::set<LLUUID> > cancel_queue_t;
	cancel_queue_t mCancelQueue;
	LLTextureInfo mTextureInfo;

	//debug use
	U32 mTotalHTTPRequests ;

	// Out-of-band cross-thread command queue.  This command queue
	// is logically tied to LLQueuedThread's list of
	// QueuedRequest instances and so must be covered by the
	// same locks.
	typedef std::vector<TFRequest *> command_queue_t;
	command_queue_t mCommands;

	// If true, modifies some behaviors that help with QA tasks.
	const bool mQAMode;
	// Cumulative stats on the states/requests issued by
	// textures running through here.
	U32 mTotalCacheReadCount;
	U32 mTotalCacheWriteCount;

public:
	// A probabilistically-correct indicator that the current
	// attempt to log metrics follows a break in the metrics stream
	// reporting due to either startup or a problem POSTing data.
	static volatile bool svMetricsDataBreak;
};

#endif // LL_LLTEXTUREFETCH_H

