// <edit>
#include "llmessagelog.h"

LLMessageLogEntry::LLMessageLogEntry(EType type, LLHost from_host, LLHost to_host, U8* data, S32 data_size)
:	mType(type),
	mFromHost(from_host),
	mToHost(to_host),
	mDataSize(data_size),
	mData(NULL)
{
	if(data)
	{
		mData = new U8[data_size];
		memcpy(mData, data, data_size);
	}
}

LLMessageLogEntry::LLMessageLogEntry()
:	mType(NONE),
	mFromHost(LLHost()),
	mToHost(LLHost()),
	mDataSize(0),
	mData(NULL)
{
}

LLMessageLogEntry::LLMessageLogEntry(EType type, const std::string& url, const LLChannelDescriptors& channels,
                                     const LLIOPipe::buffer_ptr_t& buffer, const LLSD& headers, U64 request_id,
                                     LLURLRequest::ERequestAction method, U32 status_code)
:	mType(type),
      mURL(url),
      mHeaders(headers),
      mRequestID(request_id),
      mMethod(method),
      mStatusCode(status_code),
      mDataSize(0),
      mData(NULL)
{
	if(buffer.get())
	{
		S32 channel = type == HTTP_REQUEST ? channels.out() : channels.in();
		mDataSize = buffer->countAfter(channel, NULL);
		if (mDataSize > 0)
		{
			mData = new U8[mDataSize + 1];
			buffer->readAfter(channel, NULL, mData, mDataSize);

			//make sure this is null terminated, since it's going to be used stringified
			mData[mDataSize] = '\0';
			++mDataSize;
		}
	}
}

LLMessageLogEntry::LLMessageLogEntry(const LLMessageLogEntry& entry)
    : mDataSize(entry.mDataSize),
      mType(entry.mType),
      mToHost(entry.mToHost),
      mFromHost(entry.mFromHost),
      mURL(entry.mURL),
      mHeaders(entry.mHeaders),
      mRequestID(entry.mRequestID),
      mMethod(entry.mMethod),
      mStatusCode(entry.mStatusCode)
{
	mData = new U8[mDataSize];
	memcpy(mData, entry.mData, mDataSize);
}

LLMessageLogEntry::~LLMessageLogEntry()
{
	delete mData;
}

LogCallback LLMessageLog::sCallback = NULL;

void LLMessageLog::setCallback(LogCallback callback)
{
	sCallback = callback;
}

void LLMessageLog::log(LLHost from_host, LLHost to_host, U8* data, S32 data_size)
{
	//we don't store anything locally anymore, don't bother creating a
	//log entry if it's not even going to be logged.
	if(!sCallback) return;

	LogPayload payload = new LLMessageLogEntry(LLMessageLogEntry::TEMPLATE, from_host, to_host, data, data_size);
	if(!payload->mDataSize) return;

	sCallback(payload);
}

void LLMessageLog::logHTTPRequest(const std::string& url, LLURLRequest::ERequestAction method, const LLChannelDescriptors& channels,
                                  const LLIOPipe::buffer_ptr_t& buffer, const LLSD& headers, U64 request_id)
{
	if(!sCallback) return;

	LogPayload payload = new LLMessageLogEntry(LLMessageLogEntry::HTTP_REQUEST, url, channels, buffer,
	                                         headers, request_id, method);

	sCallback(payload);
}

void LLMessageLog::logHTTPResponse(U32 status_code, const LLChannelDescriptors& channels,
                                   const LLIOPipe::buffer_ptr_t& buffer, const LLSD& headers, U64 request_id)
{
	if(!sCallback) return;

	LogPayload payload = new LLMessageLogEntry(LLMessageLogEntry::HTTP_RESPONSE, "", channels, buffer,
	                                         headers, request_id, LLURLRequest::INVALID, status_code);

	sCallback(payload);
}
// </edit>
