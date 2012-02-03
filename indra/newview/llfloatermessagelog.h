// <edit>
#include "llfloater.h"
#include "llmessagelog.h"
#include "llscrolllistctrl.h"
#include "lltemplatemessagereader.h"
#include "lleventtimer.h"
#include "llhash.h"
#include "lleasymessagereader.h"
#include "llthread.h"

class LLNetListItem
{
public:
	LLNetListItem(LLUUID id);
	LLUUID mID;
	BOOL mAutoName;
	std::string mName;
	std::string mPreviousRegionName;
	U64 mHandle;
	LLCircuitData* mCircuitData;
};


typedef std::list<LogPayload> LogPayloadList;

class LLEasyMessageLogEntry;

typedef LLEasyMessageLogEntry* FloaterMessageItem;
typedef std::list<LLEasyMessageLogEntry*> FloaterMessageList;

typedef LLHashMap<U64, FloaterMessageItem> HTTPConvoMap;

class LLMessageLogFilter
{
public:
	LLMessageLogFilter();
	LLMessageLogFilter(std::string filter);
	~LLMessageLogFilter();
	void set(std::string filter);

	std::string asString() {return mAsString;}

	//these should probably be unordered_sets
	std::list<std::string> mPositiveNames;
	std::list<std::string> mNegativeNames;

protected:
	std::string mAsString;
};


class LLFloaterMessageLog;

class LLMessageLogFilterApply : public LLEventTimer
{

public:
	LLMessageLogFilterApply(LLFloaterMessageLog* parent);
	void cancel();
	BOOL tick();
	S32 mProgress;
	BOOL mFinished;
	LLFloaterMessageLog* mParent;

private:
	LogPayloadList::iterator mIter;
};


class LLMessageLogNetMan : public LLEventTimer
{
public:
	LLMessageLogNetMan();
	~LLMessageLogNetMan();

	void cancel();

protected:
	bool mCancel;
	BOOL tick();
};


class LLFloaterMessageLog : public LLFloater
{
public:
	LLFloaterMessageLog();
	~LLFloaterMessageLog();
	static void show();
	static void onLog(LogPayload entry);

	static LLFloaterMessageLog* sInstance;

protected:
	BOOL postBuild();
	static void updateGlobalNetList(bool starting=false);
	static LLNetListItem* findNetListItem(LLHost host);
	static LLNetListItem* findNetListItem(LLUUID id);


	void refreshNetList();
	void refreshNetInfo(BOOL force);

	//the textbox(es) in the lower half of the floater can
	//display two types of information, information about
	//the circuit, or information about the selected message.

	//depending on which mode is set, certain UI elements may
	//be enabled or disabled.
	enum EInfoPaneMode { IPANE_NET, IPANE_TEMPLATE_LOG, IPANE_HTTP_LOG };
	void setInfoPaneMode(EInfoPaneMode mode);
	void wrapInfoPaneText(bool wrap);

	void conditionalLog(LogPayload entry);
	void pairHTTPResponse(LogPayload entry);

	void showSelectedMessage();
	void showMessage(FloaterMessageItem item);

	static void onCommitNetList(LLUICtrl* ctrl, void* user_data);
	static void onCommitMessageLog(LLUICtrl* ctrl, void* user_data);
	static void onClickClearLog(void* user_data);
	static void onCommitFilter(LLUICtrl* ctrl, void* user_data);
	static BOOL onClickCloseCircuit(void* user_data);
	static void onConfirmCloseCircuit(const LLSD& notification, const LLSD& response);
	static void onConfirmRemoveRegion(const LLSD& notification, const LLSD& response);
	static void onClickFilterMenu(void* user_data);
	static void onClickFilterApply(void* user_data);
	static void onClickFilterChoice(void* user_data);
	static void onCheckWrapNetInfo(LLUICtrl* ctrl, void* user_data);
	static void onCheckBeautifyMessages(LLUICtrl* ctrl, void* user_data);

public:
	void startApplyingFilter(std::string filter, BOOL force);

protected:
	void stopApplyingFilter(bool quitting=false);
	void updateFilterStatus();

	LLMessageLogFilter mMessageLogFilter;
	LLMessageLogFilterApply* mMessageLogFilterApply;

	static LLMessageLogNetMan* sNetListTimer;
	EInfoPaneMode mInfoPaneMode;

	bool mBeautifyMessages;

public:
	static std::list<LLNetListItem*> sNetListItems;

	static LogPayloadList sMessageLogEntries;
	FloaterMessageList mFloaterMessageLogItems;

	static LLMutex* sNetListMutex;
	static LLMutex* sMessageListMutex;

protected:
	HTTPConvoMap mIncompleteHTTPConvos;

	U32 mMessagesLogged;

	LLEasyMessageReader* mEasyMessageReader;

	void clearMessageLogEntries();
	void clearFloaterMessageItems(bool dying = false);

	//this needs to be able to look through the list of raw messages
	//to be able to create floater message items on a timer.
	friend class LLMessageLogFilterApply;
	friend class LLMessageLogNetMan;
};
// </edit>
