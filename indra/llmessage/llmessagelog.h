// <edit>
#ifndef LL_LLMESSAGELOG_H
#define LL_LLMESSAGELOG_H
#include "stdtypes.h"
#include "linden_common.h"
#include "llhost.h"
#include <queue>
#include <string.h>
#include "lliopipe.h"
#include "llurlrequest.h"

class LLMessageSystem;

class LLMessageLogEntry;
typedef LLMessageLogEntry* LogPayload;

class LLMessageLogEntry
{
public:
	enum EType
	{
		NONE,
		TEMPLATE,
		HTTP_REQUEST,
		HTTP_RESPONSE,
		LOG_TYPE_NUM
	};
	LLMessageLogEntry(EType type, LLHost from_host, LLHost to_host, U8* data, S32 data_size);
	LLMessageLogEntry(EType type, const std::string& url, const LLChannelDescriptors& channels,
	                  const LLIOPipe::buffer_ptr_t& buffer, const LLSD& headers, U64 request_id,
	                  LLURLRequest::ERequestAction method = LLURLRequest::INVALID, U32 status_code = 0);
	LLMessageLogEntry(const LLMessageLogEntry& entry);
	LLMessageLogEntry();
	~LLMessageLogEntry();
	EType mType;
	LLHost mFromHost;
	LLHost mToHost;
	S32 mDataSize;
	U8* mData;

	//http-related things
	std::string mURL;
	U32 mStatusCode;
	LLURLRequest::ERequestAction mMethod;
	LLSD mHeaders;
	U64 mRequestID;
};

typedef void (*LogCallback) (LogPayload);

class LLMessageLog
{
public:
	static void setCallback(LogCallback callback);
	static void log(LLHost from_host, LLHost to_host, U8* data, S32 data_size);
	static void logHTTPRequest(const std::string& url, LLURLRequest::ERequestAction method, const LLChannelDescriptors& channels,
	                           const LLIOPipe::buffer_ptr_t& buffer, const LLSD& headers, U64 request_id);
	static void logHTTPResponse(U32 status_code, const LLChannelDescriptors& channels,
	                            const LLIOPipe::buffer_ptr_t& buffer, const LLSD& headers, U64 request_id);

	static bool haveLogger(){return sCallback != NULL;}

private:
	static LogCallback sCallback;
};
#endif
// </edit>
