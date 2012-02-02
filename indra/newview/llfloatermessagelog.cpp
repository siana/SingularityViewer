// <edit>
#include "llviewerprecompiledheaders.h"
#include "llfloatermessagelog.h"
#include "lluictrlfactory.h"
#include "llworld.h"
#include "llviewerregion.h"
#include "llscrolllistctrl.h"
#include "llcheckboxctrl.h"
#include "lltexteditor.h"
#include "llviewerwindow.h"
#include "llmessagetemplate.h"
#include <boost/tokenizer.hpp>
#include "llmenugl.h"

#include "llagent.h"
#include "llnotificationsutil.h"

////////////////////////////////
// LLNetListItem
////////////////////////////////
LLNetListItem::LLNetListItem(LLUUID id)
:	mID(id),
	mAutoName(TRUE),
	mName("No name"),
	mPreviousRegionName(""),
	mCircuitData(NULL),
	mHandle(0)
{
}

//TODO: replace all filtering code, esp start/stopApplyingFilter

LLMessageLogFilter::LLMessageLogFilter()
				{
		}
LLMessageLogFilter::LLMessageLogFilter(std::string filter)
{
	set(filter);
}
LLMessageLogFilter::~LLMessageLogFilter()
{
}
void LLMessageLogFilter::set(std::string filter)
{
	mAsString = filter;
	mPositiveNames.clear();
	mNegativeNames.clear();
	typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
	boost::char_separator<char> sep(" ","");
	tokenizer tokens(filter, sep);
	tokenizer::iterator end = tokens.end();
	for(tokenizer::iterator iter = tokens.begin(); iter != end; ++iter)
	{
		std::string token = (*iter);
		LLStringUtil::trim(token);
		if(token.length())
		{
			LLStringUtil::toLower(token);

			BOOL negative = token.find("!") == 0;
			if(negative)
			{
				token = token.substr(1);
				mNegativeNames.push_back(token);
			}
			else
				mPositiveNames.push_back(token);
		}
	}
}

////////////////////////////////
// LLMessageLogFilterApply
////////////////////////////////
LLMessageLogFilterApply::LLMessageLogFilterApply(LLFloaterMessageLog* parent)
:	LLEventTimer(0.1f),
	mFinished(FALSE),
	mProgress(0),
	mParent(parent)
{
	mIter = mParent->sMessageLogEntries.begin();
}
void LLMessageLogFilterApply::cancel()
{
	mFinished = TRUE;
}
BOOL LLMessageLogFilterApply::tick()
{
	//we shouldn't even exist anymore, bail out
	if(mFinished)
		return TRUE;

	LLMutexLock lock(LLFloaterMessageLog::sMessageListMutex);

	LogPayloadList::iterator end = mParent->sMessageLogEntries.end();
	for(S32 i = 0; i < 256; i++)
	{
		if(mIter == end)
		{
			mFinished = TRUE;

			if(mParent->mMessageLogFilterApply == this)
			{
				mParent->stopApplyingFilter();
			}

			return TRUE;
		}

		mParent->conditionalLog(*mIter);

		++mIter;
		++mProgress;
	}

	mParent->updateFilterStatus();
	return FALSE;
}

LLMessageLogNetMan::LLMessageLogNetMan()
    : LLEventTimer(1.0f),
      mCancel(false)
{
}
		
void LLMessageLogNetMan::cancel()
{
	mCancel = true;
	}

BOOL LLMessageLogNetMan::tick()
{
	if(mCancel)
		return TRUE;

	LLFloaterMessageLog::updateGlobalNetList();
	return FALSE;
}

LLMessageLogNetMan::~LLMessageLogNetMan()
{
}

////////////////////////////////
// LLFloaterMessageLog
////////////////////////////////
LLFloaterMessageLog* LLFloaterMessageLog::sInstance;
std::list<LLNetListItem*> LLFloaterMessageLog::sNetListItems;
LogPayloadList LLFloaterMessageLog::sMessageLogEntries;
LLMessageLogNetMan* LLFloaterMessageLog::sNetListTimer = NULL;
LLMutex* LLFloaterMessageLog::sNetListMutex = NULL;
LLMutex* LLFloaterMessageLog::sMessageListMutex = NULL;

LLFloaterMessageLog::LLFloaterMessageLog()
    : LLFloater("message_log"),
      mInfoPaneMode(IPANE_NET),
      mMessageLogFilterApply(NULL),
      mMessagesLogged(0),
      mBeautifyMessages(false),
      mMessageLogFilter("!StartPingCheck !CompletePingCheck !PacketAck !SimulatorViewerTimeMessage !SimStats !AgentUpdate !AgentAnimation !AvatarAnimation !ViewerEffect !CoarseLocationUpdate !LayerData !CameraConstraint !ObjectUpdateCached !RequestMultipleObjects !ObjectUpdate !ObjectUpdateCompressed !ImprovedTerseObjectUpdate !KillObject !ImagePacket !SendXferPacket !ConfirmXferPacket !TransferPacket !SoundTrigger !AttachedSound !PreloadSound"),
      mEasyMessageReader(new LLEasyMessageReader())
{
	if(!sNetListMutex)
		sNetListMutex = new LLMutex();
	if(!sMessageListMutex)
		sMessageListMutex = new LLMutex();

	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_message_log.xml");
}
LLFloaterMessageLog::~LLFloaterMessageLog()
{
	stopApplyingFilter();
	clearFloaterMessageItems(true);

	delete mEasyMessageReader;

	sInstance = NULL;

	//prepare for when we might have multiple instances
	if(!sInstance)
	{
		LLMessageLog::setCallback(NULL);

		sNetListTimer->cancel();
		sNetListTimer = NULL;

		sNetListMutex->lock();
		sNetListItems.clear();
		sNetListMutex->unlock();

		clearMessageLogEntries();

		delete sNetListMutex;
		delete sMessageListMutex;
		sNetListMutex = NULL;
		sMessageListMutex = NULL;
	}
}
// static
void LLFloaterMessageLog::show()
{
	if(!sInstance) sInstance = new LLFloaterMessageLog();
	sInstance->open();
}

BOOL LLFloaterMessageLog::postBuild()
{
	childSetCommitCallback("net_list", onCommitNetList, this);
	childSetCommitCallback("message_log", onCommitMessageLog, this);
	childSetAction("filter_choice_btn", onClickFilterChoice, this);
	childSetAction("filter_apply_btn", onClickFilterApply, this);
	childSetCommitCallback("filter_edit", onCommitFilter, this);
	childSetCommitCallback("wrap_net_info", onCheckWrapNetInfo, this);
	childSetCommitCallback("beautify_messages", onCheckBeautifyMessages, this);
	childSetAction("clear_log_btn", onClickClearLog, this);
	childSetText("filter_edit", mMessageLogFilter.asString());

	startApplyingFilter(mMessageLogFilter.asString(), TRUE);

	if(!sInstance)
	{
		updateGlobalNetList(true);
		sNetListTimer = new LLMessageLogNetMan();
	}

	sInstance = this;

	setInfoPaneMode(IPANE_NET);
	wrapInfoPaneText(true);

	LLMessageLog::setCallback(onLog);

	return TRUE;
}
void LLFloaterMessageLog::clearFloaterMessageItems(bool dying)
{
	if(!dying)
	{
		childSetEnabled("msg_builder_send_btn", false);
		getChild<LLScrollListCtrl>("message_log")->clearRows();
}

	mIncompleteHTTPConvos.clear();

	FloaterMessageList::iterator iter = mFloaterMessageLogItems.begin();
	FloaterMessageList::const_iterator end = mFloaterMessageLogItems.end();
	for (;iter != end; ++iter)
	{
	       delete *iter;
	}

	mFloaterMessageLogItems.clear();
}

void LLFloaterMessageLog::clearMessageLogEntries()
{
	LLMutexLock lock(sMessageListMutex);
	//make sure to delete the objects referenced by these pointers first
	LogPayloadList::iterator iter = sMessageLogEntries.begin();
	LogPayloadList::const_iterator end = sMessageLogEntries.end();
	for (;iter != end; ++iter)
    {
       delete *iter;
}

	sMessageLogEntries.clear();
}

//static
void LLFloaterMessageLog::updateGlobalNetList(bool starting)
{
	//something tells me things aren't deallocated properly here, but
	//valgrind isn't complaining

	LLMutexLock lock(sNetListMutex);

	// Update circuit data of net list items
	std::vector<LLCircuitData*> circuits = gMessageSystem->getCircuit()->getCircuitDataList();
	std::vector<LLCircuitData*>::iterator circuits_end = circuits.end();
	for(std::vector<LLCircuitData*>::iterator iter = circuits.begin(); iter != circuits_end; ++iter)
	{
		LLNetListItem* itemp = findNetListItem((*iter)->getHost());
		if(!itemp)
		{
			LLUUID id; id.generate();
			itemp = new LLNetListItem(id);
			sNetListItems.push_back(itemp);
		}
		itemp->mCircuitData = (*iter);
	}
	// Clear circuit data of items whose circuits are gone
	std::list<LLNetListItem*>::iterator items_end = sNetListItems.end();
	for(std::list<LLNetListItem*>::iterator iter = sNetListItems.begin(); iter != items_end; ++iter)
	{
		if(std::find(circuits.begin(), circuits.end(), (*iter)->mCircuitData) == circuits.end())
			(*iter)->mCircuitData = NULL;
	}
	// Remove net list items that are totally useless now
	for(std::list<LLNetListItem*>::iterator iter = sNetListItems.begin(); iter != sNetListItems.end();)
	{
		if((*iter)->mCircuitData == NULL)
		{
			delete *iter;
			iter = sNetListItems.erase(iter);
		}
		else ++iter;
	}

	if(!starting)
	{
		sInstance->refreshNetList();
		sInstance->refreshNetInfo(FALSE);
	}
}

LLNetListItem* LLFloaterMessageLog::findNetListItem(LLHost host)
{
	std::list<LLNetListItem*>::iterator end = sNetListItems.end();
	for(std::list<LLNetListItem*>::iterator iter = sNetListItems.begin(); iter != end; ++iter)
		if((*iter)->mCircuitData && (*iter)->mCircuitData->getHost() == host)
			return (*iter);
	return NULL;
}
LLNetListItem* LLFloaterMessageLog::findNetListItem(LLUUID id)
{
	std::list<LLNetListItem*>::iterator end = sNetListItems.end();
	for(std::list<LLNetListItem*>::iterator iter = sNetListItems.begin(); iter != end; ++iter)
		if((*iter)->mID == id)
			return (*iter);
	return NULL;
}

void LLFloaterMessageLog::refreshNetList()
{
	LLScrollListCtrl* scrollp = getChild<LLScrollListCtrl>("net_list");

	// Update names of net list items
	std::list<LLNetListItem*>::iterator items_end = sNetListItems.end();
	for(std::list<LLNetListItem*>::iterator iter = sNetListItems.begin(); iter != items_end; ++iter)
	{
		LLNetListItem* itemp = (*iter);
		if(itemp->mAutoName)
		{
			if(itemp->mCircuitData)
			{
				LLViewerRegion* regionp = LLWorld::getInstance()->getRegion(itemp->mCircuitData->getHost());
				if(regionp)
				{
					std::string name = regionp->getName();
					if(name == "") name = llformat("%s (awaiting region name)", itemp->mCircuitData->getHost().getString().c_str());
					itemp->mName = name;
					itemp->mPreviousRegionName = name;
					itemp->mHandle = regionp->getHandle();
				}
				else
				{
					itemp->mName = itemp->mCircuitData->getHost().getString();
					if(itemp->mPreviousRegionName != "")
						itemp->mName.append(llformat(" (was %s)", itemp->mPreviousRegionName.c_str()));
				}
			}
			else
			{
				// an item just for an event queue, not handled yet
				itemp->mName = "Something else";
			}
		}
	}
	// Rebuild scroll list from scratch
	LLUUID selected_id = scrollp->getFirstSelected() ? scrollp->getFirstSelected()->getUUID() : LLUUID::null;
	S32 scroll_pos = scrollp->getScrollPos();
	scrollp->clearRows();
	for(std::list<LLNetListItem*>::iterator iter = sNetListItems.begin(); iter != items_end; ++iter)
	{
		LLNetListItem* itemp = (*iter);
		LLSD element;
		element["id"] = itemp->mID;
		LLSD& text_column = element["columns"][0];
		text_column["column"] = "text";
		text_column["value"] = itemp->mName + (itemp->mCircuitData->getHost() == gAgent.getRegionHost() ? " (main)" : "");
		for(int i = 0; i < 2; i++)
		{
			LLSD& icon_column = element["columns"][i + 1];
			icon_column["column"] = llformat("icon%d", i);
			icon_column["type"] = "icon";
			icon_column["value"] = "";
		}
		LLScrollListItem* scroll_itemp = scrollp->addElement(element);
		BOOL has_live_circuit = itemp->mCircuitData && itemp->mCircuitData->isAlive();
		if(has_live_circuit)
		{
			LLScrollListIcon* icon = (LLScrollListIcon*)scroll_itemp->getColumn(1);
			icon->setValue("icon_net_close_circuit.tga");
			icon->setClickCallback(onClickCloseCircuit, itemp);
		}
		else
		{
			LLScrollListIcon* icon = (LLScrollListIcon*)scroll_itemp->getColumn(1);
			icon->setValue("icon_net_close_circuit_gray.tga");
			icon->setClickCallback(NULL, NULL);
		}
		// Event queue isn't even supported yet... FIXME
		LLScrollListIcon* icon = (LLScrollListIcon*)scroll_itemp->getColumn(2);
		icon->setValue("icon_net_close_eventpoll_gray.tga");
		icon->setClickCallback(NULL, NULL);
	}
	if(selected_id.notNull()) scrollp->selectByID(selected_id);
	if(scroll_pos < scrollp->getItemCount()) scrollp->setScrollPos(scroll_pos);
}
void LLFloaterMessageLog::refreshNetInfo(BOOL force)
{
	if(mInfoPaneMode != IPANE_NET) return;
	LLScrollListCtrl* scrollp = getChild<LLScrollListCtrl>("net_list");
	LLScrollListItem* selected_itemp = scrollp->getFirstSelected();
	if(selected_itemp)
	{
		LLTextEditor* net_info = getChild<LLTextEditor>("net_info");
		if(!force && (net_info->hasSelection() || net_info->hasFocus())) return;
		LLNetListItem* itemp = findNetListItem(selected_itemp->getUUID());
		if(itemp)
		{
			std::string info(llformat("%s, %d\n--------------------------------\n\n", itemp->mName.c_str(), itemp->mHandle));
			if(itemp->mCircuitData)
			{
				LLCircuitData* cdp = itemp->mCircuitData;
				info.append("Circuit\n--------------------------------\n");
				info.append(llformat(" * Host: %s\n", cdp->getHost().getString().c_str()));
				S32 seconds = (S32)cdp->getAgeInSeconds();
				S32 minutes = seconds / 60;
				seconds = seconds % 60;
				S32 hours = minutes / 60;
				minutes = minutes % 60;
				info.append(llformat(" * Age: %dh %dm %ds\n", hours, minutes, seconds));
				info.append(llformat(" * Alive: %s\n", cdp->isAlive() ? "yes" : "no"));
				info.append(llformat(" * Blocked: %s\n", cdp->isBlocked() ? "yes" : "no"));
				info.append(llformat(" * Allow timeout: %s\n", cdp->getAllowTimeout() ? "yes" : "no"));
				info.append(llformat(" * Trusted: %s\n", cdp->getTrusted() ? "yes" : "no"));
				info.append(llformat(" * Ping delay: %d\n", cdp->getPingDelay()));
				info.append(llformat(" * Packets out: %d\n", cdp->getPacketsOut()));
				info.append(llformat(" * Bytes out: %d\n", cdp->getBytesOut()));
				info.append(llformat(" * Packets in: %d\n", cdp->getPacketsIn()));
				info.append(llformat(" * Bytes in: %d\n", cdp->getBytesIn()));
				info.append(llformat(" * Endpoint ID: %s\n", cdp->getLocalEndPointID().asString().c_str()));
				info.append(llformat(" * Remote ID: %s\n", cdp->getRemoteID().asString().c_str()));
				info.append(llformat(" * Remote session ID: %s\n", cdp->getRemoteSessionID().asString().c_str()));
				info.append("\n");
			}

			childSetText("net_info", info);
		}
		else childSetText("net_info", std::string(""));
	}
	else childSetText("net_info", std::string(""));
}
void LLFloaterMessageLog::setInfoPaneMode(EInfoPaneMode mode)
{
	mInfoPaneMode = mode;
	if(mInfoPaneMode == IPANE_NET)
		refreshNetInfo(TRUE);

	//we hide the regular net_info editor and show two panes for http log mode
	bool http_mode = mode == IPANE_HTTP_LOG;

	childSetVisible("net_info", !http_mode);

	childSetVisible("conv_stack", http_mode);

	childSetEnabled("msg_builder_send_btn", mInfoPaneMode == IPANE_TEMPLATE_LOG);
}
// static
void LLFloaterMessageLog::onLog(LogPayload entry)
{
	//we shouldn't even be able to get here without a proper instance, but make sure
	if(!sInstance)
	{
		delete entry;
		return;
	}

	if(entry->mType != LLMessageLogEntry::HTTP_RESPONSE)
	{
		sMessageListMutex->lock();
		while(!sInstance->mMessageLogFilterApply && sMessageLogEntries.size() > 4096)
		{
			//delete the raw message we're getting rid of
			delete sMessageLogEntries.front();
			sMessageLogEntries.pop_front();
		}

		++sInstance->mMessagesLogged;

		sMessageLogEntries.push_back(entry);

		sMessageListMutex->unlock();

		sInstance->conditionalLog(entry);
	}
	//this is a response, try to add it to the relevant request
	else
	{
		sInstance->pairHTTPResponse(entry);
	}
}

void LLFloaterMessageLog::conditionalLog(LogPayload entry)
{	
	if(!mMessageLogFilterApply)
		childSetText("log_status_text", llformat("Showing %d messages of %d", mFloaterMessageLogItems.size(), mMessagesLogged));

	FloaterMessageItem item = new LLEasyMessageLogEntry(entry, mEasyMessageReader);


	std::set<std::string>::const_iterator end_msg_name = item->mNames.end();
	std::set<std::string>::iterator iter_msg_name = item->mNames.begin();

	bool have_positive = false;

	for(; iter_msg_name != end_msg_name; ++iter_msg_name)
	{
		std::string find_name = *iter_msg_name;
	LLStringUtil::toLower(find_name);

		//keep the message if we allowed its name so long as one of its other names hasn't been blacklisted
		if(!have_positive && !mMessageLogFilter.mPositiveNames.empty())
		{
			if(std::find(mMessageLogFilter.mPositiveNames.begin(), mMessageLogFilter.mPositiveNames.end(), find_name) != mMessageLogFilter.mPositiveNames.end())
				have_positive = true;
		}
		if(!mMessageLogFilter.mNegativeNames.empty())
		{
			if(std::find(mMessageLogFilter.mNegativeNames.begin(), mMessageLogFilter.mNegativeNames.end(), find_name) != mMessageLogFilter.mNegativeNames.end())
			{
				delete item;
			return;
			}
		}
		//we don't have any negative filters and we have a positive match
		else if(have_positive)
			break;
	}

	//we had a positive filter but no positive matches
	if(!mMessageLogFilter.mPositiveNames.empty() && !have_positive)
	{
		delete item;
		return;
	}

	mFloaterMessageLogItems.push_back(item); // moved from beginning...

	if(item->mType == LLEasyMessageLogEntry::HTTP_REQUEST)
	{
		mIncompleteHTTPConvos.insert(HTTPConvoMap::value_type(item->mRequestID, item));
	}

	std::string net_name("\?\?\?");
	BOOL outgoing = item->isOutgoing();
	if(item->mType == LLEasyMessageLogEntry::TEMPLATE)
	{
		LLHost find_host = outgoing ? item->mToHost : item->mFromHost;
		net_name = find_host.getIPandPort();
		std::list<LLNetListItem*>::iterator end = sNetListItems.end();
		for(std::list<LLNetListItem*>::iterator iter = sNetListItems.begin(); iter != end; ++iter)
		{
			if((*iter)->mCircuitData->getHost() == find_host)
			{
				net_name = (*iter)->mName;
				break;
			}
		}
	}

	//add the message to the messagelog scroller
	LLSD element;
	element["id"] = item->mID;
	LLSD& sequence_column = element["columns"][0];
	sequence_column["column"] = "sequence";
	sequence_column["value"] = llformat("%u", item->mSequenceID);

	LLSD& type_column = element["columns"][1];
	type_column["column"] = "type";
	switch(item->mType)
	{
	case LLEasyMessageLogEntry::TEMPLATE:
		type_column["value"] = "UDP";
		break;
	case LLEasyMessageLogEntry::HTTP_REQUEST:
		type_column["value"] = "HTTP";
		break;
	default:
		type_column["value"] = "\?\?\?";
	}

	LLSD& direction_column = element["columns"][2];
	direction_column["column"] = "direction";
	if(item->mType == LLEasyMessageLogEntry::TEMPLATE)
		direction_column["value"] = outgoing ? "to" : "from";
	else if(item->mType == LLEasyMessageLogEntry::HTTP_REQUEST)
		direction_column["value"] = "both";

	LLSD& net_column = element["columns"][3];
	net_column["column"] = "net";
	net_column["value"] = net_name;

	LLSD& name_column = element["columns"][4];
	name_column["column"] = "name";
	name_column["value"] = item->getName();

	LLSD& summary_column = element["columns"][5];
	summary_column["column"] = "summary";
	summary_column["value"] = item->mSummary;
	LLScrollListCtrl* scrollp = getChild<LLScrollListCtrl>("message_log");

	S32 scroll_pos = scrollp->getScrollPos();
	scrollp->addElement(element, ADD_BOTTOM);

	if(scroll_pos > scrollp->getItemCount() - scrollp->getPageLines() - 4)
		scrollp->setScrollPos(scrollp->getItemCount());
}
void LLFloaterMessageLog::pairHTTPResponse(LogPayload entry)
{
	HTTPConvoMap::iterator iter = mIncompleteHTTPConvos.find(entry->mRequestID);

	if(iter != mIncompleteHTTPConvos.end())
	{
		iter->second->setResponseMessage(entry);
		mIncompleteHTTPConvos.erase(iter);

		//if this message was already selected in the message log,
		//redisplay it to show the response as well.
		LLScrollListItem* itemp = getChild<LLScrollListCtrl>("message_log")->getFirstSelected();

		if(!itemp) return;

		if(itemp->getUUID() == iter->second->mID)
		{
			showMessage(iter->second);
		}
	}
	else
		delete entry;
}

// static
void LLFloaterMessageLog::onCommitNetList(LLUICtrl* ctrl, void* user_data)
{
	LLFloaterMessageLog* floaterp = (LLFloaterMessageLog*)user_data;
	floaterp->setInfoPaneMode(IPANE_NET);
	floaterp->refreshNetInfo(TRUE);
}
// static
void LLFloaterMessageLog::onCommitMessageLog(LLUICtrl* ctrl, void* user_data)
{
	LLFloaterMessageLog* floaterp = (LLFloaterMessageLog*)user_data;
	floaterp->showSelectedMessage();
}

void LLFloaterMessageLog::showSelectedMessage()
{
	LLScrollListItem* selected_itemp = getChild<LLScrollListCtrl>("message_log")->getFirstSelected();
	if(!selected_itemp) return;
	LLUUID id = selected_itemp->getUUID();
	FloaterMessageList::iterator end = mFloaterMessageLogItems.end();
	for(FloaterMessageList::iterator iter = mFloaterMessageLogItems.begin(); iter != end; ++iter)
	{
		if((*iter)->mID == id)
		{
			showMessage((*iter));
			break;
		}
	}
}

void LLFloaterMessageLog::showMessage(FloaterMessageItem item)
{
	if(item->mType == LLMessageLogEntry::TEMPLATE)
	{
		setInfoPaneMode(IPANE_TEMPLATE_LOG);
		childSetText("net_info", item->getFull(mBeautifyMessages));
	}
	else if(item->mType == LLMessageLogEntry::HTTP_REQUEST)
	{
		setInfoPaneMode(IPANE_HTTP_LOG);
		childSetText("conv_request", item->getFull(mBeautifyMessages));
		childSetText("conv_response", item->getResponseFull(mBeautifyMessages));
	}
}

// static
BOOL LLFloaterMessageLog::onClickCloseCircuit(void* user_data)
{
	LLNetListItem* itemp = (LLNetListItem*)user_data;
	LLCircuitData* cdp = (LLCircuitData*)itemp->mCircuitData;
	if(!cdp) return FALSE;
	LLHost myhost = cdp->getHost();
	LLSD args;
	args["MESSAGE"] = "This will delete local circuit data.\nDo you want to tell the remote host to close the circuit too?";
	LLSD payload;
	payload["circuittoclose"] = myhost.getString(); 
	LLNotificationsUtil::add("GenericAlertYesCancel", args, payload, onConfirmCloseCircuit);
	return TRUE;
}
// static
void LLFloaterMessageLog::onConfirmCloseCircuit(S32 option, LLSD payload)
{
	LLCircuitData* cdp = gMessageSystem->mCircuitInfo.findCircuit(LLHost(payload["circuittoclose"].asString()));
	if(!cdp) return;
	LLViewerRegion* regionp = LLWorld::getInstance()->getRegion(cdp->getHost());
	switch(option)
	{
	case 0: // yes
		gMessageSystem->newMessageFast(_PREHASH_CloseCircuit);
		gMessageSystem->sendReliable(cdp->getHost());
		break;
	case 2: // cancel
		return;
		break;
	case 1: // no
	default:
		break;
	}
	if(gMessageSystem->findCircuitCode(cdp->getHost()))
		gMessageSystem->disableCircuit(cdp->getHost());
	else
		gMessageSystem->getCircuit()->removeCircuitData(cdp->getHost());
	if(regionp)
	{
		LLHost myhost = regionp->getHost();
		LLSD args;
		args["MESSAGE"] = "That host had a region associated with it.\nDo you want to clean that up?";
		LLSD payload;
		payload["regionhost"] = myhost.getString();
		LLNotificationsUtil::add("GenericAlertYesCancel", args, payload, onConfirmRemoveRegion);
	}
}
// static
void LLFloaterMessageLog::onConfirmRemoveRegion(S32 option, LLSD payload)
{
	if(option == 0) // yes
		LLWorld::getInstance()->removeRegion(LLHost(payload["regionhost"].asString()));
}
// static
void LLFloaterMessageLog::onClickFilterApply(void* user_data)
{
	LLFloaterMessageLog* floaterp = (LLFloaterMessageLog*)user_data;
	floaterp->startApplyingFilter(floaterp->childGetValue("filter_edit"), TRUE);
}
void LLFloaterMessageLog::startApplyingFilter(std::string filter, BOOL force)
{
	LLMessageLogFilter new_filter(filter);
	if(force
		|| (new_filter.mNegativeNames != mMessageLogFilter.mNegativeNames)
		|| (new_filter.mPositiveNames != mMessageLogFilter.mPositiveNames))
	{
		stopApplyingFilter();
		mMessageLogFilter = new_filter;

		mMessagesLogged = sMessageLogEntries.size();
		clearFloaterMessageItems();

		childSetVisible("message_log", false);
		mMessageLogFilterApply = new LLMessageLogFilterApply(this);
	}
}
void LLFloaterMessageLog::stopApplyingFilter(bool quitting)
{
	if(mMessageLogFilterApply)
	{
			mMessageLogFilterApply->cancel();

		if(!quitting)
		{
		childSetVisible("message_log", true);
			childSetText("log_status_text", llformat("Showing %d messages from %d", mFloaterMessageLogItems.size(), mMessagesLogged));
		}
	}

	mMessageLogFilterApply = NULL;
}
void LLFloaterMessageLog::updateFilterStatus()
{
	if(!mMessageLogFilterApply) return;

	S32 progress = mMessageLogFilterApply->mProgress;
	S32 packets = sMessageLogEntries.size();
	S32 matches = mFloaterMessageLogItems.size();
	std::string text = llformat("Applying filter ( %d / %d ), %d matches ...", progress, packets, matches);
	childSetText("log_status_text", text);
}
// static
void LLFloaterMessageLog::onCommitFilter(LLUICtrl* ctrl, void* user_data)
{
	LLFloaterMessageLog* floaterp = (LLFloaterMessageLog*)user_data;
	floaterp->startApplyingFilter(floaterp->childGetValue("filter_edit"), FALSE);
}
// static
void LLFloaterMessageLog::onClickClearLog(void* user_data)
{
	LLFloaterMessageLog* floaterp = (LLFloaterMessageLog*)user_data;
	floaterp->stopApplyingFilter();
	floaterp->getChild<LLScrollListCtrl>("message_log")->clearRows();
	floaterp->setInfoPaneMode(IPANE_NET);
	floaterp->clearMessageLogEntries();
	floaterp->clearFloaterMessageItems();
	floaterp->mMessagesLogged = 0;
}
// static
void LLFloaterMessageLog::onClickFilterChoice(void* user_data)
{
	LLMenuGL* menu = new LLMenuGL(LLStringUtil::null);
	menu->append(new LLMenuItemCallGL("No filter", onClickFilterMenu, NULL, (void*)""));
	menu->append(new LLMenuItemCallGL("Fewer spammy messages", onClickFilterMenu, NULL, (void*)"!StartPingCheck !CompletePingCheck !PacketAck !SimulatorViewerTimeMessage !SimStats !AgentUpdate !AgentAnimation !AvatarAnimation !ViewerEffect !CoarseLocationUpdate !LayerData !CameraConstraint !ObjectUpdateCached !RequestMultipleObjects !ObjectUpdate !ObjectUpdateCompressed !ImprovedTerseObjectUpdate !KillObject !ImagePacket !SendXferPacket !ConfirmXferPacket !TransferPacket"));
	menu->append(new LLMenuItemCallGL("Fewer spammy messages (minus sound crap)", onClickFilterMenu, NULL, (void*)"!StartPingCheck !CompletePingCheck !PacketAck !SimulatorViewerTimeMessage !SimStats !AgentUpdate !AgentAnimation !AvatarAnimation !ViewerEffect !CoarseLocationUpdate !LayerData !CameraConstraint !ObjectUpdateCached !RequestMultipleObjects !ObjectUpdate !ObjectUpdateCompressed !ImprovedTerseObjectUpdate !KillObject !ImagePacket !SendXferPacket !ConfirmXferPacket !TransferPacket !SoundTrigger !AttachedSound !PreloadSound"));
	menu->append(new LLMenuItemCallGL("Object updates", onClickFilterMenu, NULL, (void*)"ObjectUpdateCached ObjectUpdate ObjectUpdateCompressed ImprovedTerseObjectUpdate KillObject RequestMultipleObjects"));
	menu->append(new LLMenuItemCallGL("Abnormal", onClickFilterMenu, NULL, (void*)"Invalid TestMessage AddCircuitCode NeighborList AvatarTextureUpdate SimulatorMapUpdate SimulatorSetMap SubscribeLoad UnsubscribeLoad SimulatorReady SimulatorPresentAtLocation SimulatorLoad SimulatorShutdownRequest RegionPresenceRequestByRegionID RegionPresenceRequestByHandle RegionPresenceResponse UpdateSimulator LogDwellTime FeatureDisabled LogFailedMoneyTransaction UserReportInternal SetSimStatusInDatabase SetSimPresenceInDatabase OpenCircuit CloseCircuit DirFindQueryBackend DirPlacesQueryBackend DirClassifiedQueryBackend DirLandQueryBackend DirPopularQueryBackend GroupNoticeAdd DataHomeLocationRequest DataHomeLocationReply DerezContainer ObjectCategory ObjectExportSelected StateSave ReportAutosaveCrash AgentAlertMessage NearestLandingRegionRequest NearestLandingRegionReply NearestLandingRegionUpdated TeleportLandingStatusChanged ConfirmEnableSimulator KickUserAck SystemKickUser AvatarPropertiesRequestBackend UpdateParcel RemoveParcel MergeParcel LogParcelChanges CheckParcelSales ParcelSales StartAuction ConfirmAuctionStart CompleteAuction CancelAuction CheckParcelAuctions ParcelAuctions ChatPass EdgeDataPacket SimStatus ChildAgentUpdate ChildAgentAlive ChildAgentPositionUpdate ChildAgentDying ChildAgentUnknown AtomicPassObject KillChildAgents ScriptSensorRequest ScriptSensorReply DataServerLogout RequestInventoryAsset InventoryAssetResponse TransferInventory TransferInventoryAck EventLocationRequest EventLocationReply MoneyTransferBackend RoutedMoneyBalanceReply SetStartLocation NetTest SetCPURatio SimCrashed NameValuePair RemoveNameValuePair UpdateAttachment RemoveAttachment EmailMessageRequest EmailMessageReply InternalScriptMail ScriptDataRequest ScriptDataReply InviteGroupResponse TallyVotes LiveHelpGroupRequest LiveHelpGroupReply GroupDataUpdate LogTextMessage CreateTrustedCircuit ParcelRename SystemMessage RpcChannelRequest RpcChannelReply RpcScriptRequestInbound RpcScriptRequestInboundForward RpcScriptReplyInbound ScriptMailRegistration Error"));
	menu->updateParent(LLMenuGL::sMenuContainer);
	menu->setCanTearOff(FALSE);
	LLView* buttonp = sInstance->getChild<LLView>("filter_choice_btn");
	S32 x = buttonp->getRect().mLeft;
	S32 y = buttonp->getRect().mBottom;
	LLMenuGL::showPopup(sInstance, menu, x, y);
}
// static
void LLFloaterMessageLog::onClickFilterMenu(void* user_data)
{
	if(sInstance)
	{
		std::string filter = std::string((char*)user_data);
		sInstance->childSetText("filter_edit", filter);
		sInstance->startApplyingFilter(filter, FALSE);
	}
}

// static
void LLFloaterMessageLog::onCheckWrapNetInfo(LLUICtrl* ctrl, void* user_data)
{
	LLFloaterMessageLog* floaterp = (LLFloaterMessageLog*)user_data;
	floaterp->wrapInfoPaneText(((LLCheckBoxCtrl*)ctrl)->getValue());
}

//static
void LLFloaterMessageLog::onCheckBeautifyMessages(LLUICtrl* ctrl, void* user_data)
{
	LLFloaterMessageLog* floaterp = (LLFloaterMessageLog*)user_data;
	floaterp->mBeautifyMessages = ((LLCheckBoxCtrl*)ctrl)->getValue();

	//if we already had a message selected, we need to set the full
	//text of the message again
	floaterp->showSelectedMessage();
}

void LLFloaterMessageLog::wrapInfoPaneText(bool wrap)
{
	getChild<LLTextEditor>("net_info")->setWordWrap(wrap);
	getChild<LLTextEditor>("conv_request")->setWordWrap(wrap);
	getChild<LLTextEditor>("conv_response")->setWordWrap(wrap);
}
// </edit>
