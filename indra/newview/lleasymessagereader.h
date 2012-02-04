#ifndef EASY_MESSAGE_READER_H
#define EASY_MESSAGE_READER_H

#include "llmessagelog.h"
#include "linden_common.h"
#include "message.h"
#include "lltemplatemessagereader.h"
#include "llmessagetemplate.h"

class LLEasyMessageReader
{
public:
	LLEasyMessageReader();
	~LLEasyMessageReader();

	LLMessageTemplate* decodeTemplateMessage(U8* data, S32 data_len, LLHost from_host);
	LLMessageTemplate* decodeTemplateMessage(U8* data, S32 data_len, LLHost from_host, U32& sequence_id);

	S32 getNumberOfBlocks(const char *blockname);

	std::string var2Str(const char* block_name, S32 block_num, LLMessageVariable* variable, BOOL &returned_hex, BOOL summary_mode=FALSE);

private:
	U8 mDecodeBuffer[NET_BUFFER_SIZE];
	LLTemplateMessageReader mTemplateMessageReader;
};

class LLEasyMessageLogEntry : public LLMessageLogEntry
{
public:
	LLEasyMessageLogEntry(LogPayload entry, LLEasyMessageReader* message_reader = NULL);
	LLEasyMessageLogEntry(LLEasyMessageReader* message_reader = NULL);
	~LLEasyMessageLogEntry();

	std::string getFull(BOOL beautify = FALSE, BOOL show_header = FALSE);
	std::string getName();
	std::string getResponseFull(BOOL beautify = FALSE, BOOL show_header = FALSE);
	BOOL isOutgoing();

	void setResponseMessage(LogPayload entry);

	LLUUID mID;
	U32 mSequenceID;
	//depending on how the server is configured, two cap handlers
	//may have the exact same URI, meaning there may be multiple possible
	//cap names for each message.
	std::set<std::string> mNames;
	std::string mSummary;
	U32 mFlags;

private:
	LLEasyMessageLogEntry* mResponseMsg;
	LLEasyMessageReader* mEasyMessageReader;
};

#endif
