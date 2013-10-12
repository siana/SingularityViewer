/** 
 * @file llhttpclient.cpp
 * @brief Implementation of classes for making HTTP requests.
 *
 * $LicenseInfo:firstyear=2006&license=viewerlgpl$
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

#include "linden_common.h"

#include <boost/shared_ptr.hpp>
#include <xmlrpc-epi/xmlrpc.h>

#include "llhttpclient.h"
#include "llbufferstream.h"
#include "llsdserialize.h"
#include "llvfile.h"
#include "llurlrequest.h"
#include "llxmltree.h"
#include "aihttptimeoutpolicy.h"

class AIHTTPTimeoutPolicy;
extern AIHTTPTimeoutPolicy blockingLLSDPost_timeout;
extern AIHTTPTimeoutPolicy blockingLLSDGet_timeout;
extern AIHTTPTimeoutPolicy blockingRawGet_timeout;

////////////////////////////////////////////////////////////////////////////

class XMLRPCInjector : public Injector {
public:
	XMLRPCInjector(XMLRPC_REQUEST request) : mRequest(request), mRequestText(NULL) { }
	~XMLRPCInjector() { XMLRPC_RequestFree(mRequest, 1); XMLRPC_Free(const_cast<char*>(mRequestText)); }

	/*virtual*/ char const* contentType(void) const { return "text/xml"; }
	/*virtual*/ U32 get_body(LLChannelDescriptors const& channels, buffer_ptr_t& buffer)
	{
		int requestTextSize;
		mRequestText = XMLRPC_REQUEST_ToXML(mRequest, &requestTextSize);
		if (!mRequestText)
			throw AICurlNoBody("XMLRPC_REQUEST_ToXML returned NULL.");
		LLBufferStream ostream(channels, buffer.get());
		ostream.write(mRequestText, requestTextSize);
		ostream << std::flush;          // Always flush a LLBufferStream when done writing to it.
		return requestTextSize;
	}

private:
	XMLRPC_REQUEST const mRequest;
	char const* mRequestText;
};

class XmlTreeInjector : public Injector
{
	public:
		XmlTreeInjector(LLXmlTree const* tree) : mTree(tree) { }
		~XmlTreeInjector() { delete const_cast<LLXmlTree*>(mTree); }

		/*virtual*/ char const* contentType(void) const { return "application/xml"; }
		/*virtual*/ U32 get_body(LLChannelDescriptors const& channels, buffer_ptr_t& buffer)
		{
			std::string data;
			mTree->write(data);
			LLBufferStream ostream(channels, buffer.get());
			ostream.write(data.data(), data.size());
			ostream << std::flush;          // Always flush a LLBufferStream when done writing to it.
			return data.size();
		}

	private:
		LLXmlTree const* mTree;
};

class LLSDInjector : public Injector
{
  public:
	LLSDInjector(LLSD const& sd) : mSD(sd) { }

	/*virtual*/ char const* contentType(void) const { return "application/llsd+xml"; }

	/*virtual*/ U32 get_body(LLChannelDescriptors const& channels, buffer_ptr_t& buffer)
	{
		LLBufferStream ostream(channels, buffer.get());
		LLSDSerialize::toXML(mSD, ostream);
		// Need to flush the LLBufferStream or count_out() returns more than the written data.
		ostream << std::flush;
		return ostream.count_out();
	}

	LLSD const mSD;
};

class RawInjector : public Injector
{
  public:
	RawInjector(char const* data, U32 size) : mData(data), mSize(size) { }
	/*virtual*/ ~RawInjector() { delete [] mData; }

	/*virtual*/ char const* contentType(void) const { return "application/octet-stream"; }

	/*virtual*/ U32 get_body(LLChannelDescriptors const& channels, buffer_ptr_t& buffer)
	{
		LLBufferStream ostream(channels, buffer.get());
		ostream.write(mData, mSize);
		ostream << std::flush;			// Always flush a LLBufferStream when done writing to it.
		return mSize;
	}

	char const* mData;
	U32 mSize;
};

class FileInjector : public Injector
{
  public:
	FileInjector(std::string const& filename) : mFilename(filename) { }

	char const* contentType(void) const { return "application/octet-stream"; }

	/*virtual*/ U32 get_body(LLChannelDescriptors const& channels, buffer_ptr_t& buffer)
	{
		llifstream fstream(mFilename, std::ios::binary);
		if (!fstream.is_open())
		  throw AICurlNoBody(llformat("Failed to open \"%s\".", mFilename.c_str()));
		LLBufferStream ostream(channels, buffer.get());
		char tmpbuf[4096];
#ifdef SHOW_ASSERT
		size_t total_len = 0;
		fstream.seekg(0, std::ios::end);
		size_t file_size = fstream.tellg();
		fstream.seekg(0, std::ios::beg);
#endif
		while (fstream)
		{
			fstream.read(tmpbuf, sizeof(tmpbuf));
			std::streamsize len = fstream.gcount();
			if (len > 0)
			{
				ostream.write(tmpbuf, len);
#ifdef SHOW_ASSERT
				total_len += len;
#endif
			}
		}
		if (fstream.bad())
		  throw AICurlNoBody(llformat("An error occured while reading \"%s\".", mFilename.c_str()));
		fstream.close();
		ostream << std::flush;
		llassert(total_len == file_size && total_len == ostream.count_out());
		return ostream.count_out();
	}

	std::string const mFilename;
};

class VFileInjector : public Injector
{
public:
	VFileInjector(LLUUID const& uuid, LLAssetType::EType asset_type) : mUUID(uuid), mAssetType(asset_type) { }

	/*virtual*/ char const* contentType(void) const { return "application/octet-stream"; }

	/*virtual*/ U32 get_body(LLChannelDescriptors const& channels, buffer_ptr_t& buffer)
	{
		LLBufferStream ostream(channels, buffer.get());
		
		LLVFile vfile(gVFS, mUUID, mAssetType, LLVFile::READ);
		S32 fileSize = vfile.getSize();
		std::vector<U8> fileBuffer(fileSize);
		vfile.read(&fileBuffer[0], fileSize);
		ostream.write((char*)&fileBuffer[0], fileSize);
		ostream << std::flush;
		
		return fileSize;
	}

	LLUUID const mUUID;
	LLAssetType::EType mAssetType;
};

//static
void LLHTTPClient::request(
	std::string const& url,
	LLURLRequest::ERequestAction method,
	Injector* body_injector,
	LLHTTPClient::ResponderPtr responder,
	AIHTTPHeaders& headers,
	AIPerService::Approvement* approved/*,*/
	DEBUG_CURLIO_PARAM(EDebugCurl debug),
	EKeepAlive keepalive,
	EDoesAuthentication does_auth,
	EAllowCompressedReply allow_compression,
	AIStateMachine* parent,
	AIStateMachine::state_type new_parent_state,
	AIEngine* default_engine)
{
	llassert(responder);

	if (!responder) {
		responder = new LLHTTPClient::ResponderIgnore;
	}
	// For possible debug output from within the responder.
	responder->setURL(url);

	LLURLRequest* req;
	try
	{
		req = new LLURLRequest(method, url, body_injector, responder, headers, approved, keepalive, does_auth, allow_compression);
#ifdef DEBUG_CURLIO
		req->mCurlEasyRequest.debug(debug);
#endif
	}
	catch(AICurlNoEasyHandle& error)
	{
		llwarns << "Failed to create LLURLRequest: " << error.what() << llendl;
		// This is what the old LL code did: no recovery whatsoever (but also no leaks or crash).
		return ;
	}

	req->run(parent, new_parent_state, parent != NULL, true, default_engine);
}

void LLHTTPClient::getByteRange(std::string const& url, S32 offset, S32 bytes, ResponderPtr responder, AIHTTPHeaders& headers/*,*/ DEBUG_CURLIO_PARAM(EDebugCurl debug))
{
	if(offset > 0 || bytes > 0)
	{
		headers.addHeader("Range", llformat("bytes=%d-%d", offset, offset + bytes - 1));
	}
    request(url, HTTP_GET, NULL, responder, headers, NULL/*,*/ DEBUG_CURLIO_PARAM(debug));
}

void LLHTTPClient::head(std::string const& url, ResponderHeadersOnly* responder, AIHTTPHeaders& headers/*,*/ DEBUG_CURLIO_PARAM(EDebugCurl debug))
{
	request(url, HTTP_HEAD, NULL, responder, headers, NULL/*,*/ DEBUG_CURLIO_PARAM(debug));
}

void LLHTTPClient::get(std::string const& url, ResponderPtr responder, AIHTTPHeaders& headers/*,*/ DEBUG_CURLIO_PARAM(EDebugCurl debug))
{
	request(url, HTTP_GET, NULL, responder, headers, NULL/*,*/ DEBUG_CURLIO_PARAM(debug));
}

void LLHTTPClient::getHeaderOnly(std::string const& url, ResponderHeadersOnly* responder, AIHTTPHeaders& headers/*,*/ DEBUG_CURLIO_PARAM(EDebugCurl debug))
{
	request(url, HTTP_HEAD, NULL, responder, headers, NULL/*,*/ DEBUG_CURLIO_PARAM(debug));
}

void LLHTTPClient::get(std::string const& url, LLSD const& query, ResponderPtr responder, AIHTTPHeaders& headers/*,*/ DEBUG_CURLIO_PARAM(EDebugCurl debug))
{
	LLURI uri;
	
	uri = LLURI::buildHTTP(url, LLSD::emptyArray(), query);
	get(uri.asString(), responder, headers/*,*/ DEBUG_CURLIO_PARAM(debug));
}

//=============================================================================
// Responders base classes.
//

//-----------------------------------------------------------------------------
// class LLHTTPClient::ResponderBase
//

LLHTTPClient::ResponderBase::ResponderBase(void) : mReferenceCount(0), mCode(CURLE_FAILED_INIT), mFinished(false)
{
	DoutEntering(dc::curl, "AICurlInterface::Responder() with this = " << (void*)this);
	AICurlInterface::Stats::ResponderBase_count++;
}

LLHTTPClient::ResponderBase::~ResponderBase()
{
	DoutEntering(dc::curl, "AICurlInterface::ResponderBase::~ResponderBase() with this = " << (void*)this << "; mReferenceCount = " << mReferenceCount);
	llassert(mReferenceCount == 0);
	--AICurlInterface::Stats::ResponderBase_count;
}

void LLHTTPClient::ResponderBase::setURL(std::string const& url)
{
	// setURL is called from llhttpclient.cpp (request()), before calling any of the below (of course).
	// We don't need locking here therefore; it's a case of initializing before use.
	mURL = url;
}

AIHTTPTimeoutPolicy const& LLHTTPClient::ResponderBase::getHTTPTimeoutPolicy(void) const
{
	return AIHTTPTimeoutPolicy::getDebugSettingsCurlTimeout();
}

void LLHTTPClient::ResponderBase::decode_llsd_body(U32 status, std::string const& reason, LLChannelDescriptors const& channels, buffer_ptr_t const& buffer, LLSD& content)
{
	AICurlInterface::Stats::llsd_body_count++;
	if (is_internal_http_error(status))
	{
		// In case of an internal error (ie, a curl error), a description of the (curl) error is the best we can do.
		// In any case, the body if anything was received at all, can not be relied upon.
	    content = reason;
		return;
	}
	// If the status indicates success (and we get here) then we expect the body to be LLSD.
	bool const should_be_llsd = (200 <= status && status < 300);
	if (should_be_llsd)
	{
		LLBufferStream istr(channels, buffer.get());
		if (LLSDSerialize::fromXML(content, istr) == LLSDParser::PARSE_FAILURE)
		{
			// Unfortunately we can't show the body of the message... I think this is a pretty serious error
			// though, so if this ever happens it has to be investigated by making a copy of the buffer
			// before serializing it, as is done below.
			llwarns << "Failed to deserialize LLSD. " << mURL << " [" << status << "]: " << reason << llendl;
			AICurlInterface::Stats::llsd_body_parse_error++;
		}
		// LLSDSerialize::fromXML destructed buffer, we can't initialize content now.
		return;
	}
	// Put the body in content as-is.
	std::stringstream ss;
	buffer->writeChannelTo(ss, channels.in());
	content = ss.str();
#ifdef SHOW_ASSERT
	if (!should_be_llsd)
	{
		char const* str = ss.str().c_str();
		// Make sure that the server indeed never returns LLSD as body when the http status is an error.
		LLSD dummy;
		bool server_sent_llsd_with_http_error =
			strncmp(str, "<!DOCTYPE", 9) &&				// LLSD does never start with "<!"; short circuits 97% of the replies.
			strncmp(str, "cap not found:", 14) &&		// Most of the other 3%.
			str[0] &&									// Empty happens too and aint LLSD either.
			strncmp(str, "Not Found", 9) &&
			LLSDSerialize::fromXML(dummy, ss) > 0;
		if (server_sent_llsd_with_http_error)
		{
			llwarns << "The server sent us a response with http status " << status << " and LLSD(!) body: \"" << ss.str() << "\"!" << llendl;
		}
		// This is not really an error, and it has been known to happen before. It just normally never happens (at the moment)
		// and therefore warrants an investigation. Linden Lab (or other grids) might start to send error messages
		// as LLSD in the body in future, but until that happens frequently we might want to leave this assert in.
		// Note that an HTTP error code means an error at the transport level in most cases-- so I'm highly suspicious
		// when there is any additional information in the body in LLSD format: it is not unlikely to be a server
		// bug, where the returned HTTP status code should have been 200 instead.
		llassert(!server_sent_llsd_with_http_error);
	}
#endif
}

void LLHTTPClient::ResponderBase::decode_raw_body(U32 status, std::string const& reason, LLChannelDescriptors const& channels, buffer_ptr_t const& buffer, std::string& content)
{
	AICurlInterface::Stats::raw_body_count++;
	if (is_internal_http_error(status))
	{
		// In case of an internal error (ie, a curl error), a description of the (curl) error is the best we can do.
		// In any case, the body if anything was received at all, can not be relied upon.
	    content = reason;
		return;
	}
	LLMutexLock lock(buffer->getMutex());
	LLBufferArray::const_segment_iterator_t const end = buffer->endSegment();
	for (LLBufferArray::const_segment_iterator_t iter = buffer->beginSegment(); iter != end; ++iter)
	{
		if (iter->isOnChannel(channels.in()))
		{
			content.append((char*)iter->data(), iter->size());
		}
	}
}

std::string const& LLHTTPClient::ResponderBase::get_cookie(std::string const& key)
{
	AIHTTPReceivedHeaders::range_type cookies;
	if (mReceivedHeaders.getValues("set-cookie", cookies))
	{
		for (AIHTTPReceivedHeaders::iterator_type cookie = cookies.first; cookie != cookies.second; ++cookie)
		{
			if (key == cookie->second.substr(0, cookie->second.find('=')))
			{
				return cookie->second;
			}
		}
	}
	// Not found.
	static std::string empty_dummy;
	return empty_dummy;
}

// Called with HTML body.
// virtual
void LLHTTPClient::ResponderWithCompleted::completedRaw(U32 status, std::string const& reason, LLChannelDescriptors const& channels, buffer_ptr_t const& buffer)
{
  LLSD content;
  decode_llsd_body(status, reason, channels, buffer, content);

  // Allow derived class to override at this point.
  completed(status, reason, content);
}

// virtual
void LLHTTPClient::ResponderWithCompleted::completed(U32 status, std::string const& reason, LLSD const& content)
{
  // Either completedRaw() or this method must be overridden by the derived class. Hence, we should never get here.
  llassert_always(false);
}

// virtual
void LLHTTPClient::ResponderWithResult::finished(CURLcode code, U32 http_status, std::string const& reason, LLChannelDescriptors const& channels, buffer_ptr_t const& buffer)
{
  mCode = code;

  LLSD content;
  decode_llsd_body(http_status, reason, channels, buffer, content);

  // HTTP status good?
  if (200 <= http_status && http_status < 300)
  {
	// Allow derived class to override at this point.
	result(content);
  }
  else
  {
	// Allow derived class to override at this point.
	errorWithContent(http_status, reason, content);
  }

  mFinished = true;
}

// virtual
void LLHTTPClient::ResponderWithResult::errorWithContent(U32 status, std::string const& reason, LLSD const&)
{
  // Allow derived class to override at this point.
  error(status, reason);
}

// virtual
void LLHTTPClient::ResponderWithResult::error(U32 status, std::string const& reason)
{
  llinfos << mURL << " [" << status << "]: " << reason << llendl;
}

// Friend functions.

void intrusive_ptr_add_ref(LLHTTPClient::ResponderBase* responder)
{
  responder->mReferenceCount++;
}

void intrusive_ptr_release(LLHTTPClient::ResponderBase* responder)
{
  if (--responder->mReferenceCount == 0)
  {
	delete responder;
  }
}

//-----------------------------------------------------------------------------
// Blocking Responders.
//

class BlockingResponder : public LLHTTPClient::LegacyPolledResponder {
private:
	LLCondition mSignal;	// Wait condition to wait till mFinished is true.
	static LLSD LLSD_dummy;
	static std::string Raw_dummy;

public:
	void wait(void);		// Blocks until mFinished is true.
	virtual LLSD const& getLLSD(void) const { llassert(false); return LLSD_dummy; }
	virtual std::string const& getRaw(void) const { llassert(false); return Raw_dummy; }

protected:
	void wakeup(void);		// Call this at the end of completedRaw.
};

LLSD BlockingResponder::LLSD_dummy;
std::string BlockingResponder::Raw_dummy;

void BlockingResponder::wait(void)
{
  if (AIThreadID::in_main_thread())
  {
	// We're the main thread, so we have to give AIStateMachine CPU cycles.
	while (!mFinished)
	{
	  // AIFIXME: this can probably be removed once curl is detached from the main thread.
	  gMainThreadEngine.mainloop();
	  ms_sleep(10);
	}
  }
  else	// Hopefully not the curl thread :p
  {
	mSignal.lock();
	while (!mFinished)
	  mSignal.wait();
	mSignal.unlock();
  }
}

void BlockingResponder::wakeup(void)
{
  // Normally mFinished is set immediately after returning from this function,
  // but we do it here, because we need to set it before calling mSignal.signal().
  mSignal.lock();
  mFinished = true;
  mSignal.unlock();
  mSignal.signal();
}

class BlockingLLSDResponder : public BlockingResponder {
private:
	LLSD mResponse;

protected:
	/*virtual*/ LLSD const& getLLSD(void) const { llassert(mFinished && mCode == CURLE_OK && mStatus == HTTP_OK); return mResponse; }
	/*virtual*/ void completedRaw(U32 status, std::string const& reason, LLChannelDescriptors const& channels, buffer_ptr_t const& buffer)
	{
		decode_llsd_body(status, reason, channels, buffer, mResponse);		// This puts the body asString() in mResponse in case of http error.
		wakeup();
	}
};

class BlockingRawResponder : public BlockingResponder {
private:
	std::string mResponse;

protected:
	/*virtual*/ std::string const& getRaw(void) const { llassert(mFinished && mCode == CURLE_OK && mStatus == HTTP_OK); return mResponse; }
	/*virtual*/ void completedRaw(U32 status, std::string const& reason, LLChannelDescriptors const& channels, buffer_ptr_t const& buffer)
	{
		decode_raw_body(mCode, reason, channels, buffer, mResponse);
		wakeup();
	}
};

class BlockingLLSDPostResponder : public BlockingLLSDResponder {
public:
	/*virtual*/ AIHTTPTimeoutPolicy const& getHTTPTimeoutPolicy(void) const { return blockingLLSDPost_timeout; }
	/*virtual*/ char const* getName(void) const { return "BlockingLLSDPostResponder"; }
};

class BlockingLLSDGetResponder : public BlockingLLSDResponder {
public:
	/*virtual*/ AIHTTPTimeoutPolicy const& getHTTPTimeoutPolicy(void) const { return blockingLLSDGet_timeout; }
	/*virtual*/ char const* getName(void) const { return "BlockingLLSDGetResponder"; }
};

class BlockingRawGetResponder : public BlockingRawResponder {
public:
	/*virtual*/ AIHTTPTimeoutPolicy const& getHTTPTimeoutPolicy(void) const { return blockingRawGet_timeout; }
	/*virtual*/ char const* getName(void) const { return "BlockingRawGetResponder"; }
};

// End (blocking) responders.
//=============================================================================

// These calls are blocking! This is usually bad, unless you're a dataserver. Then it's awesome.

enum EBlockingRequestAction {
  HTTP_LLSD_POST,
  HTTP_LLSD_GET,
  HTTP_RAW_GET
};

/**
	@brief does a blocking request on the url, returning the data or bad status.

	@param url URI to verb on.
	@param method the verb to hit the URI with.
	@param body the body of the call (if needed - for instance not used for GET and DELETE, but is for POST and PUT)
	@param headers HTTP headers to use for the request.
	@param timeout Curl timeout to use. Defaults to 5. Rationale:
	Without this timeout, blockingGet() calls have been observed to take
	up to 90 seconds to complete.  Users of blockingGet() already must 
	check the HTTP return code for validity, so this will not introduce
	new errors.  A 5 second timeout will succeed > 95% of the time (and 
	probably > 99% of the time) based on my statistics. JC

	@returns an LLSD map: {status: integer, body: map}
  */
static LLSD blocking_request(
	std::string const& url,
	EBlockingRequestAction method,
	LLSD const& body/*,*/				// Only used for HTTP_LLSD_POST
	DEBUG_CURLIO_PARAM(EDebugCurl debug))
{
	lldebugs << "blockingRequest of " << url << llendl;

	AIHTTPHeaders headers;
	boost::intrusive_ptr<BlockingResponder> responder;
	if (method == HTTP_LLSD_POST)
	{
		responder = new BlockingLLSDPostResponder;
		LLHTTPClient::post(url, body, responder, headers/*,*/ DEBUG_CURLIO_PARAM(debug));
	}
	else if (method == HTTP_LLSD_GET)
	{
		responder = new BlockingLLSDGetResponder;
		LLHTTPClient::get(url, responder, headers/*,*/ DEBUG_CURLIO_PARAM(debug));
	}
	else // method == HTTP_RAW_GET
	{
		responder = new BlockingRawGetResponder;
		LLHTTPClient::get(url, responder, headers/*,*/ DEBUG_CURLIO_PARAM(debug));
	}

	responder->wait();

	LLSD response = LLSD::emptyMap();
	CURLcode result = responder->result_code();
	S32 http_status = responder->http_status();

	bool http_success = http_status >= 200 && http_status < 300;
	if (result == CURLE_OK && http_success)
	{
		if (method == HTTP_RAW_GET)
		{
			response["body"] = responder->getRaw();
		}
		else
		{
			response["body"] = responder->getLLSD();
		}
	}
	else if (result == CURLE_OK)
	{
		// We expect 404s, don't spam for them.
		if (http_status != 404)
		{
			llwarns << "CURL REQ URL: " << url << llendl;
			llwarns << "CURL REQ METHOD TYPE: " << method << llendl;
			llwarns << "CURL REQ HEADERS: " << headers << llendl;
			if (method == HTTP_LLSD_POST)
			{
				llwarns << "CURL REQ BODY: " << body.asString() << llendl;
			}
			llwarns << "CURL HTTP_STATUS: " << http_status << llendl;
			if (method == HTTP_RAW_GET)
			{
				llwarns << "CURL ERROR BODY: " << responder->getRaw() << llendl;
			}
			else
			{
				llwarns << "CURL ERROR BODY: " << responder->getLLSD().asString() << llendl;
			}
		}
		if (method == HTTP_RAW_GET)
		{
			response["body"] = responder->getRaw();
		}
		else
		{
			response["body"] = responder->getLLSD().asString();
		}
	}
	else
	{
		response["body"] = responder->reason();
	}

	response["status"] = http_status;
	return response;
}

LLSD LLHTTPClient::blockingPost(const std::string& url, const LLSD& body/*,*/ DEBUG_CURLIO_PARAM(EDebugCurl debug))
{
	return blocking_request(url, HTTP_LLSD_POST, body/*,*/ DEBUG_CURLIO_PARAM(debug));
}

LLSD LLHTTPClient::blockingGet(const std::string& url/*,*/ DEBUG_CURLIO_PARAM(EDebugCurl debug))
{
	return blocking_request(url, HTTP_LLSD_GET, LLSD()/*,*/ DEBUG_CURLIO_PARAM(debug));
}

U32 LLHTTPClient::blockingGetRaw(const std::string& url, std::string& body/*,*/ DEBUG_CURLIO_PARAM(EDebugCurl debug))
{
	LLSD result = blocking_request(url, HTTP_RAW_GET, LLSD()/*,*/ DEBUG_CURLIO_PARAM(debug));
	body = result["body"].asString();
	return result["status"].asInteger();
}

void LLHTTPClient::put(std::string const& url, LLSD const& body, ResponderPtr responder, AIHTTPHeaders& headers/*,*/ DEBUG_CURLIO_PARAM(EDebugCurl debug))
{
	request(url, HTTP_PUT, new LLSDInjector(body), responder, headers, NULL/*,*/ DEBUG_CURLIO_PARAM(debug), no_keep_alive, no_does_authentication, no_allow_compressed_reply);
}

void LLHTTPClient::post(std::string const& url, LLSD const& body, ResponderPtr responder, AIHTTPHeaders& headers/*,*/ DEBUG_CURLIO_PARAM(EDebugCurl debug), EKeepAlive keepalive, AIStateMachine* parent, AIStateMachine::state_type new_parent_state)
{
	request(url, HTTP_POST, new LLSDInjector(body), responder, headers, NULL/*,*/ DEBUG_CURLIO_PARAM(debug), keepalive, no_does_authentication, allow_compressed_reply, parent, new_parent_state);
}

void LLHTTPClient::post_approved(std::string const& url, LLSD const& body, ResponderPtr responder, AIHTTPHeaders& headers, AIPerService::Approvement* approved/*,*/ DEBUG_CURLIO_PARAM(EDebugCurl debug), EKeepAlive keepalive, AIStateMachine* parent, AIStateMachine::state_type new_parent_state)
{
	request(url, HTTP_POST, new LLSDInjector(body), responder, headers, approved/*,*/ DEBUG_CURLIO_PARAM(debug), keepalive, no_does_authentication, allow_compressed_reply, parent, new_parent_state, &gMainThreadEngine);
}

void LLHTTPClient::postXMLRPC(std::string const& url, XMLRPC_REQUEST xmlrpc_request, ResponderPtr responder, AIHTTPHeaders& headers/*,*/ DEBUG_CURLIO_PARAM(EDebugCurl debug), EKeepAlive keepalive)
{
  	request(url, HTTP_POST, new XMLRPCInjector(xmlrpc_request), responder, headers, NULL/*,*/ DEBUG_CURLIO_PARAM(debug), keepalive, does_authentication, allow_compressed_reply);
}

void LLHTTPClient::postXMLRPC(std::string const& url, char const* method, XMLRPC_VALUE value, ResponderPtr responder, AIHTTPHeaders& headers/*,*/ DEBUG_CURLIO_PARAM(EDebugCurl debug), EKeepAlive keepalive)
{
	XMLRPC_REQUEST xmlrpc_request = XMLRPC_RequestNew();
	XMLRPC_RequestSetMethodName(xmlrpc_request, method);
	XMLRPC_RequestSetRequestType(xmlrpc_request, xmlrpc_request_call);
	XMLRPC_RequestSetData(xmlrpc_request, value);
	// XMLRPCInjector takes ownership of xmlrpc_request and will free it when done.
	// LLURLRequest takes ownership of the XMLRPCInjector object and will free it when done.
  	request(url, HTTP_POST, new XMLRPCInjector(xmlrpc_request), responder, headers, NULL/*,*/ DEBUG_CURLIO_PARAM(debug), keepalive, does_authentication, no_allow_compressed_reply);
}

void LLHTTPClient::postRaw(std::string const& url, char const* data, S32 size, ResponderPtr responder, AIHTTPHeaders& headers/*,*/ DEBUG_CURLIO_PARAM(EDebugCurl debug), EKeepAlive keepalive)
{
	request(url, HTTP_POST, new RawInjector(data, size), responder, headers, NULL/*,*/ DEBUG_CURLIO_PARAM(debug), keepalive);
}

void LLHTTPClient::postFile(std::string const& url, std::string const& filename, ResponderPtr responder, AIHTTPHeaders& headers/*,*/ DEBUG_CURLIO_PARAM(EDebugCurl debug), EKeepAlive keepalive)
{
	request(url, HTTP_POST, new FileInjector(filename), responder, headers, NULL/*,*/ DEBUG_CURLIO_PARAM(debug), keepalive);
}

void LLHTTPClient::postFile(std::string const& url, LLUUID const& uuid, LLAssetType::EType asset_type, ResponderPtr responder, AIHTTPHeaders& headers/*,*/ DEBUG_CURLIO_PARAM(EDebugCurl debug), EKeepAlive keepalive)
{
	request(url, HTTP_POST, new VFileInjector(uuid, asset_type), responder, headers, NULL/*,*/ DEBUG_CURLIO_PARAM(debug), keepalive);
}

// static
void LLHTTPClient::del(std::string const& url, ResponderPtr responder, AIHTTPHeaders& headers/*,*/ DEBUG_CURLIO_PARAM(EDebugCurl debug))
{
	request(url, HTTP_DELETE, NULL, responder, headers, NULL/*,*/ DEBUG_CURLIO_PARAM(debug));
}

// static
void LLHTTPClient::move(std::string const& url, std::string const& destination, ResponderPtr responder, AIHTTPHeaders& headers/*,*/ DEBUG_CURLIO_PARAM(EDebugCurl debug))
{
	headers.addHeader("Destination", destination);
	request(url, HTTP_MOVE, NULL, responder, headers, NULL/*,*/ DEBUG_CURLIO_PARAM(debug));
}
