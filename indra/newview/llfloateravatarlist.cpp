/**
 * @file llfloateravatarlist.cpp
 * @brief Avatar list/radar floater
 *
 * @author Dale Glass <dale@daleglass.net>, (C) 2007
 */

/**
 * Rewritten by jcool410
 * Removed usage of globals
 * Removed TrustNET
 * Added utilization of "minimap" data
 * Heavily modified by Henri Beauchamp (the laggy spying tool becomes a true,
 * low lag radar)
 */

#include "llviewerprecompiledheaders.h"

#include "llavatarconstants.h"
#include "llavatarnamecache.h"
#include "llfloateravatarlist.h"

#include "llnotificationsutil.h"
#include "llradiogroup.h"
#include "llscrolllistcolumn.h"
#include "llscrolllistctrl.h"
#include "llscrolllistitem.h"
#include "lluictrlfactory.h"
#include "llviewerwindow.h"
#include "llwindow.h"

#include "llvoavatar.h"
#include "llimview.h"
#include "llfloaterreporter.h"
#include "llagent.h"
#include "llagentcamera.h"
#include "llavataractions.h"
#include "llfloaterregioninfo.h"
#include "llviewerregion.h"
#include "lltracker.h"
#include "llchat.h"
#include "llfloaterchat.h"
#include "llviewermessage.h"
#include "llweb.h"
#include "llviewerobjectlist.h"
#include "llmutelist.h"
#include "llcallbacklist.h"

#include <time.h>
#include <string.h>

#include <map>
#include <boost/date_time.hpp>
#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>

#include "llworld.h"
#include "llsdutil.h"
#include "llaudioengine.h"
#include "llstartup.h"
#include "llviewermenu.h"

#include "hippogridmanager.h"
#include "lfsimfeaturehandler.h"

// [RLVa:KB]
#include "rlvhandler.h"
// [/RLVa:KB]

/**
 * @brief How long to keep people who are gone in the list and in memory.
 */
const F32 DEAD_KEEP_TIME = 0.5f;

extern U32 gFrameCount;

namespace
{
void chat_avatar_status(std::string name, LLUUID key, ERadarStatType type, bool entering)
{
	if(gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES)) return; //RLVa:LF Don't announce people are around when blind, that cheats the system.
	static LLCachedControl<bool> radar_chat_alerts(gSavedSettings, "RadarChatAlerts");
	if (!radar_chat_alerts) return;
	static LLCachedControl<bool> radar_alert_sim(gSavedSettings, "RadarAlertSim");
	static LLCachedControl<bool> radar_alert_draw(gSavedSettings, "RadarAlertDraw");
	static LLCachedControl<bool> radar_alert_shout_range(gSavedSettings, "RadarAlertShoutRange");
	static LLCachedControl<bool> radar_alert_chat_range(gSavedSettings, "RadarAlertChatRange");
	static LLCachedControl<bool> radar_alert_age(gSavedSettings, "RadarAlertAge");

	LLFloaterAvatarList* self = LLFloaterAvatarList::getInstance();
	LLStringUtil::format_map_t args;
	args["[NAME]"] = name;
	switch(type)
	{
		case STAT_TYPE_SIM:
			if (radar_alert_sim)
			{
				args["[RANGE]"] = self->getString("the_sim");
			}
			break;

		case STAT_TYPE_DRAW:
			if (radar_alert_draw)
			{
				args["[RANGE]"] = self->getString("draw_distance");
			}
			break;

		case STAT_TYPE_SHOUTRANGE:
			if (radar_alert_shout_range)
			{
				args["[RANGE]"] = self->getString("shout_range");
			}
			break;

		case STAT_TYPE_CHATRANGE:
			if (radar_alert_chat_range)
			{
				args["[RANGE]"] = self->getString("chat_range");
			}
			break;

		case STAT_TYPE_AGE:
			if (radar_alert_age)
			{
				LLChat chat;
				chat.mFromName = name;
				chat.mText = name + " " + self->getString("has_triggered_your_avatar_age_alert") + ".";
				chat.mURL = llformat("secondlife:///app/agent/%s/about",key.asString().c_str());
				chat.mSourceType = CHAT_SOURCE_SYSTEM;
				LLFloaterChat::addChat(chat);
			}
			break;
		default:
			llassert(type);
			break;
	}
	if (args.find("[RANGE]") != args.end())
	{
		args["[ACTION]"] = self->getString(entering ? "has_entered" : "has_left");
		LLChat chat;
		chat.mText = self->getString("template", args);
		chat.mFromName = name;
		chat.mURL = llformat("secondlife:///app/agent/%s/about",key.asString().c_str());
		chat.mSourceType = CHAT_SOURCE_SYSTEM;
		LLFloaterChat::addChat(chat);
	}
}

	void send_keys_message(const int transact_num, const int num_ids, const std::string ids)
	{
		gMessageSystem->newMessage("ScriptDialogReply");
		gMessageSystem->nextBlock("AgentData");
		gMessageSystem->addUUID("AgentID", gAgent.getID());
		gMessageSystem->addUUID("SessionID", gAgent.getSessionID());
		gMessageSystem->nextBlock("Data");
		gMessageSystem->addUUID("ObjectID", gAgent.getID());
		gMessageSystem->addS32("ChatChannel", -777777777);
		gMessageSystem->addS32("ButtonIndex", 1);
		gMessageSystem->addString("ButtonLabel", llformat("%d,%d", transact_num, num_ids) + ids);
		gAgent.sendReliableMessage();
	}
} //namespace

LLAvatarListEntry::LLAvatarListEntry(const LLUUID& id, const std::string &name, const LLVector3d &position) :
		mID(id), mName(name), mPosition(position), mMarked(false), mFocused(false),
		mUpdateTimer(), mFrame(gFrameCount), mStats(),
		mActivityType(ACTIVITY_NEW), mActivityTimer(),
		mIsInList(false), mAge(-1), mTime(time(NULL))
{
	LLAvatarPropertiesProcessor::getInstance()->addObserver(mID, this);
	LLAvatarPropertiesProcessor::getInstance()->sendAvatarPropertiesRequest(mID);
}

LLAvatarListEntry::~LLAvatarListEntry()
{
	LLAvatarPropertiesProcessor::getInstance()->removeObserver(mID, this);
}

// virtual
void LLAvatarListEntry::processProperties(void* data, EAvatarProcessorType type)
{
	if(type == APT_PROPERTIES)
	{
		LLAvatarPropertiesProcessor::getInstance()->removeObserver(mID, this);
		const LLAvatarData* pAvatarData = static_cast<const LLAvatarData*>(data);
		if (pAvatarData && (pAvatarData->avatar_id != LLUUID::null))
		{
			using namespace boost::gregorian;
			int year, month, day;
			sscanf(pAvatarData->born_on.c_str(),"%d/%d/%d",&month,&day,&year);
			mAge = (day_clock::local_day() - date(year, month, day)).days();
			if (!mStats[STAT_TYPE_AGE] && mAge >= 0) //Only announce age once per entry.
			{
				static const LLCachedControl<U32> sAvatarAgeAlertDays(gSavedSettings, "AvatarAgeAlertDays");
				if ((U32)mAge < sAvatarAgeAlertDays)
				{
					chat_avatar_status(mName, mID, STAT_TYPE_AGE, mStats[STAT_TYPE_AGE] = true);
				}
			}
			// If one wanted more information that gets displayed on profiles to be displayed, here would be the place to do it.
		}
	}
}

void LLAvatarListEntry::setPosition(LLVector3d position, bool this_sim, bool drawn, bool chatrange, bool shoutrange)
{
	mPosition = position;

	mFrame = gFrameCount;

	if (this_sim != mStats[STAT_TYPE_SIM])
	{
		chat_avatar_status(mName, mID, STAT_TYPE_SIM, mStats[STAT_TYPE_SIM] = this_sim);
	}
	if (drawn != mStats[STAT_TYPE_DRAW])
	{
		chat_avatar_status(mName, mID, STAT_TYPE_DRAW, mStats[STAT_TYPE_DRAW] = drawn);
	}
	if (shoutrange != mStats[STAT_TYPE_SHOUTRANGE])
	{
		chat_avatar_status(mName, mID, STAT_TYPE_SHOUTRANGE, mStats[STAT_TYPE_SHOUTRANGE] = shoutrange);
	}
	if (chatrange != mStats[STAT_TYPE_CHATRANGE])
	{
		chat_avatar_status(mName, mID, STAT_TYPE_CHATRANGE, mStats[STAT_TYPE_CHATRANGE] = chatrange);
	}

	mUpdateTimer.start();
}

bool LLAvatarListEntry::getAlive()
{
	U32 current = gFrameCount;
	return ((current - mFrame) <= 2);
}

F32 LLAvatarListEntry::getEntryAgeSeconds() const
{
	return mUpdateTimer.getElapsedTimeF32();
}

bool LLAvatarListEntry::isDead() const
{
	return getEntryAgeSeconds() > DEAD_KEEP_TIME;
}
const F32 ACTIVITY_TIMEOUT = 1.0f;
void LLAvatarListEntry::setActivity(ACTIVITY_TYPE activity)
{
	if ( activity >= mActivityType || mActivityTimer.getElapsedTimeF32() > ACTIVITY_TIMEOUT )
	{
		mActivityType = activity;
		mActivityTimer.start();
	}
}

const LLAvatarListEntry::ACTIVITY_TYPE LLAvatarListEntry::getActivity()
{
	if ( mActivityTimer.getElapsedTimeF32() > ACTIVITY_TIMEOUT )
	{
		mActivityType = ACTIVITY_NONE;
	}
	if(isDead())return ACTIVITY_DEAD;

	return mActivityType;
}

LLFloaterAvatarList::LLFloaterAvatarList() :  LLFloater(std::string("radar")), 
	mTracking(false),
	mUpdate("RadarUpdateEnabled"),
	mDirtyAvatarSorting(false),
	mUpdateRate(gSavedSettings.getU32("RadarUpdateRate") * 3 + 3),
	mAvatarList(NULL)
{
	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_radar.xml");
}

LLFloaterAvatarList::~LLFloaterAvatarList()
{
	gIdleCallbacks.deleteFunction(LLFloaterAvatarList::callbackIdle);
}

//static
void LLFloaterAvatarList::toggle(void*)
{
// [RLVa:KB]
	if(gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES))
	{
		if(instanceExists())
			getInstance()->close();
	}
	else
// [/RLVa:KB]
	if(!instanceExists() || !getInstance()->getVisible())
	{
		showInstance();
	}
	else
	{
		getInstance()->close();
	}
}

//static
void LLFloaterAvatarList::showInstance()
{
// [RLVa:KB]
	if(gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES))
		return;
// [/RLVa:KB]
	getInstance()->open();
}

void LLFloaterAvatarList::draw()
{
	LLFloater::draw();
}

void LLFloaterAvatarList::onOpen()
{
	gSavedSettings.setBOOL("ShowRadar", TRUE);
}

void LLFloaterAvatarList::onClose(bool app_quitting)
{
	setVisible(FALSE);
	if (!app_quitting)
	{
		gSavedSettings.setBOOL("ShowRadar", FALSE);
	}
	if (!gSavedSettings.getBOOL("RadarKeepOpen") || app_quitting)
	{
		destroy();
	}
}

static void cmd_profile(const LLAvatarListEntry* entry);
static void cmd_toggle_mark(LLAvatarListEntry* entry);
static void cmd_ar(const LLAvatarListEntry* entry);
static void cmd_teleport(const LLAvatarListEntry* entry);

namespace
{
	typedef LLMemberListener<LLView> view_listener_t;
	class RadarTrack : public view_listener_t
	{
		bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
		{
			LLFloaterAvatarList::instance().onClickTrack();
			return true;
		}
	};

	class RadarMark : public view_listener_t
	{
		bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
		{
			LLFloaterAvatarList::instance().doCommand(cmd_toggle_mark);
			return true;
		}
	};

	class RadarFocus : public view_listener_t
	{
		bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
		{
			LLFloaterAvatarList::instance().onClickFocus();
			return true;
		}
	};

	class RadarFocusPrev : public view_listener_t
	{
		bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
		{
			LLFloaterAvatarList::instance().focusOnPrev(userdata.asInteger());
			return true;
		}
	};

	class RadarFocusNext : public view_listener_t
	{
		bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
		{
			LLFloaterAvatarList::instance().focusOnNext(userdata.asInteger());
			return true;
		}
	};

	class RadarTeleportTo : public view_listener_t
	{
		bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
		{
			LLFloaterAvatarList::instance().doCommand(cmd_teleport, true);
			return true;
		}
	};

	class RadarAnnounceKeys : public view_listener_t
	{
		bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
		{
			LLFloaterAvatarList::instance().sendKeys();
			return true;
		}
	};
}

void addMenu(view_listener_t* menu, const std::string& name);

void add_radar_listeners()
{
	addMenu(new RadarTrack(), "Radar.Track");
	addMenu(new RadarMark(), "Radar.Mark");
	addMenu(new RadarFocus(), "Radar.Focus");
	addMenu(new RadarFocusPrev(), "Radar.FocusPrev");
	addMenu(new RadarFocusNext(), "Radar.FocusNext");
	addMenu(new RadarTeleportTo(), "Radar.TeleportTo");
	addMenu(new RadarAnnounceKeys(), "Radar.AnnounceKeys");
}

BOOL LLFloaterAvatarList::postBuild()
{
	// Set callbacks
	childSetAction("profile_btn", boost::bind(&LLFloaterAvatarList::doCommand, this, &cmd_profile, false));
	childSetAction("im_btn", boost::bind(&LLFloaterAvatarList::onClickIM, this));
	childSetAction("offer_btn", boost::bind(&LLFloaterAvatarList::onClickTeleportOffer, this));
	childSetAction("track_btn", boost::bind(&LLFloaterAvatarList::onClickTrack, this));
	childSetAction("mark_btn", boost::bind(&LLFloaterAvatarList::doCommand, this, &cmd_toggle_mark, false));
	childSetAction("focus_btn", boost::bind(&LLFloaterAvatarList::onClickFocus, this));
	childSetAction("prev_in_list_btn", boost::bind(&LLFloaterAvatarList::focusOnPrev, this, FALSE));
	childSetAction("next_in_list_btn", boost::bind(&LLFloaterAvatarList::focusOnNext, this, FALSE));
	childSetAction("prev_marked_btn", boost::bind(&LLFloaterAvatarList::focusOnPrev, this, TRUE));
	childSetAction("next_marked_btn", boost::bind(&LLFloaterAvatarList::focusOnNext, this, TRUE));

	childSetAction("get_key_btn", boost::bind(&LLFloaterAvatarList::onClickGetKey, this));

	childSetAction("freeze_btn", boost::bind(&LLFloaterAvatarList::onClickFreeze, this));
	childSetAction("eject_btn", boost::bind(&LLFloaterAvatarList::onClickEject, this));
	childSetAction("mute_btn", boost::bind(&LLFloaterAvatarList::onClickMute, this));
	childSetAction("ar_btn", boost::bind(&LLFloaterAvatarList::doCommand, this, &cmd_ar, true));
	childSetAction("teleport_btn", boost::bind(&LLFloaterAvatarList::doCommand, this, &cmd_teleport, true));
	childSetAction("estate_eject_btn", boost::bind(&LLFloaterAvatarList::onClickEjectFromEstate, this));
	childSetAction("estate_ban_btn",boost::bind(&LLFloaterAvatarList:: onClickBanFromEstate, this));

	childSetAction("send_keys_btn", boost::bind(&LLFloaterAvatarList::sendKeys, this));

	getChild<LLRadioGroup>("update_rate")->setSelectedIndex(gSavedSettings.getU32("RadarUpdateRate"));
	getChild<LLRadioGroup>("update_rate")->setCommitCallback(boost::bind(&LLFloaterAvatarList::onCommitUpdateRate, this));

	gSavedSettings.getControl("RadarColumnMarkHidden")->getSignal()->connect(boost::bind(&LLFloaterAvatarList::assessColumns, this));
	gSavedSettings.getControl("RadarColumnPositionHidden")->getSignal()->connect(boost::bind(&LLFloaterAvatarList::assessColumns, this));
	gSavedSettings.getControl("RadarColumnAltitudeHidden")->getSignal()->connect(boost::bind(&LLFloaterAvatarList::assessColumns, this));
	gSavedSettings.getControl("RadarColumnActivityHidden")->getSignal()->connect(boost::bind(&LLFloaterAvatarList::assessColumns, this));
	gSavedSettings.getControl("RadarColumnAgeHidden")->getSignal()->connect(boost::bind(&LLFloaterAvatarList::assessColumns, this));
	gSavedSettings.getControl("RadarColumnTimeHidden")->getSignal()->connect(boost::bind(&LLFloaterAvatarList::assessColumns, this));

	// Get a pointer to the scroll list from the interface
	mAvatarList = getChild<LLScrollListCtrl>("avatar_list");
	mAvatarList->sortByColumn("distance", TRUE);
	mAvatarList->setCommitOnSelectionChange(TRUE);
	mAvatarList->setCommitCallback(boost::bind(&LLFloaterAvatarList::onSelectName,this));
	mAvatarList->setDoubleClickCallback(boost::bind(&LLFloaterAvatarList::onClickFocus,this));
	mAvatarList->setSortChangedCallback(boost::bind(&LLFloaterAvatarList::onAvatarSortingChanged,this));
	refreshAvatarList();

	gIdleCallbacks.addFunction(LLFloaterAvatarList::callbackIdle);

	assessColumns();

	if(gHippoGridManager->getConnectedGrid()->isSecondLife())
		childSetVisible("hide_client", false);
	else
		gSavedSettings.getControl("RadarColumnClientHidden")->getSignal()->connect(boost::bind(&LLFloaterAvatarList::assessColumns, this));

	return TRUE;
}

void col_helper(const bool hide, LLCachedControl<S32> &setting, LLScrollListColumn* col)
{
	// Brief Explanation:
	// Check if we want the column hidden, and if it's still showing. If so, hide it, but save its width.
	// Otherwise, if we don't want it hidden, but it is, unhide it to the saved width.
	// We only store width of columns when hiding here for the purpose of hiding and unhiding.
	const int width = col->getWidth();

	if (hide && width)
	{
		setting = width;
		col->setWidth(0);
	}
	else if(!hide && !width)
	{
		col->setWidth(setting);
	}
}

//Macro to reduce redundant lines. Preprocessor concatenation and stringizing avoids bloat that
//wrapping in a class would create.
#define BIND_COLUMN_TO_SETTINGS(col, name)\
	static const LLCachedControl<bool> hide_##name(gSavedSettings, "RadarColumn"#name"Hidden");\
	static LLCachedControl<S32> width_##name(gSavedSettings, "RadarColumn"#name"Width");\
	col_helper(hide_##name, width_##name, mAvatarList->getColumn(col));

void LLFloaterAvatarList::assessColumns()
{
	BIND_COLUMN_TO_SETTINGS(LIST_MARK,Mark);
	BIND_COLUMN_TO_SETTINGS(LIST_POSITION,Position);
	BIND_COLUMN_TO_SETTINGS(LIST_ALTITUDE,Altitude);
	BIND_COLUMN_TO_SETTINGS(LIST_ACTIVITY,Activity);
	BIND_COLUMN_TO_SETTINGS(LIST_AGE,Age);
	BIND_COLUMN_TO_SETTINGS(LIST_TIME,Time);

	static const LLCachedControl<bool> hide_client(gSavedSettings, "RadarColumnClientHidden");
	static LLCachedControl<S32> width_name(gSavedSettings, "RadarColumnNameWidth");
	bool client_hidden = hide_client || gHippoGridManager->getConnectedGrid()->isSecondLife();
	LLScrollListColumn* name_col = mAvatarList->getColumn(LIST_AVATAR_NAME);
	LLScrollListColumn* client_col = mAvatarList->getColumn(LIST_CLIENT);

	if (client_hidden != !!name_col->mDynamicWidth)
	{
		//Don't save if its being hidden because of detected grid. Not that it really matters, as this setting(along with the other RadarColumn*Width settings)
		//isn't handled in a way that allows it to carry across sessions, but I assume that may want to be fixed in the future..
		if(client_hidden && !gHippoGridManager->getConnectedGrid()->isSecondLife() && name_col->getWidth() > 0)
			width_name = name_col->getWidth();

		//MUST call setWidth(0) first to clear out mTotalStaticColumnWidth accumulation in parent before changing the mDynamicWidth value
		client_col->setWidth(0);
		name_col->setWidth(0);

		client_col->mDynamicWidth =	!client_hidden;
		name_col->mDynamicWidth =	 client_hidden;
		mAvatarList->setNumDynamicColumns(1); // Dynamic width is set on only one column, be sure to let the list know, otherwise we may divide by zero

		if(!client_hidden)
		{
			name_col->setWidth(llmax(width_name.get(),10));
		}
	}

	mAvatarList->updateLayout();
}

void updateParticleActivity(LLDrawable *drawablep)
{
	if (LLFloaterAvatarList::instanceExists())
	{
		LLViewerObject *vobj = drawablep->getVObj();
		if (vobj && vobj->isParticleSource())
		{
			LLUUID id = vobj->mPartSourcep->getOwnerUUID();
			LLAvatarListEntry *ent = LLFloaterAvatarList::getInstance()->getAvatarEntry(id);
			if ( NULL != ent )
			{
				ent->setActivity(LLAvatarListEntry::ACTIVITY_PARTICLES);
			}
		}
	}
}

void LLFloaterAvatarList::updateAvatarList()
{
	//llinfos << "radar refresh: updating map" << llendl;

	// Check whether updates are enabled
	if (!mUpdate)
	{
		refreshTracker();
		return;
	}
	//moved to pipeline to prevent a crash
	//gPipeline.forAllVisibleDrawables(updateParticleActivity);


	//todo: make this less of a hacked up copypasta from dales 1.18.

	{
		std::vector<LLUUID> avatar_ids;
		std::vector<LLUUID> sorted_avatar_ids;
		std::vector<LLVector3d> positions;

		LLVector3d mypos = gAgent.getPositionGlobal();
		static const LLCachedControl<F32> radar_range_radius("RadarRangeRadius", 0);
		LLWorld::instance().getAvatars(&avatar_ids, &positions, mypos, radar_range_radius ? radar_range_radius : F32_MAX);

		size_t i;
		size_t count = avatar_ids.size();

		static LLCachedControl<bool> announce(gSavedSettings, "RadarChatKeys");
		std::queue<LLUUID> announce_keys;

		for (i = 0; i < count; ++i)
		{
			const LLUUID &avid = avatar_ids[i];

			std::string name;
			if (!LLAvatarNameCache::getPNSName(avid, name))
				continue; //prevent (Loading...)

			LLAvatarListEntry* entry = getAvatarEntry(avid);

			LLVector3d position = positions[i];

			LLVOAvatar* avatarp = gObjectList.findAvatar(avid);
			if (avatarp)
			{
				// Get avatar data
				position = gAgent.getPosGlobalFromAgent(avatarp->getCharacterPosition());
			}

			if (!entry)
			{
				// Avatar not there yet, add it
				if(announce && gAgent.getRegion()->pointInRegionGlobal(position))
					announce_keys.push(avid);
				mAvatars.push_back(LLAvatarListEntryPtr(entry = new LLAvatarListEntry(avid, name, position)));
			}

			// Announce position
			F32 dist = (F32)(position - mypos).magVec();
			entry->setPosition(position, gAgent.getRegion()->pointInRegionGlobal(position), avatarp, dist < LFSimFeatureHandler::getInstance()->sayRange(), dist < LFSimFeatureHandler::getInstance()->shoutRange());

			// Mark as typing if they are typing
			if (avatarp && avatarp->isTyping()) entry->setActivity(LLAvatarListEntry::ACTIVITY_TYPING);
		}

		// Set activity for anyone making sounds
		if (gAudiop)
		{
			for (LLAudioEngine::source_map::iterator iter = gAudiop->mAllSources.begin(); iter != gAudiop->mAllSources.end(); ++iter)
			{
				LLAudioSource* sourcep = iter->second;
				if (LLAvatarListEntry* entry = getAvatarEntry(sourcep->getOwnerID()))
				{
					entry->setActivity(LLAvatarListEntry::ACTIVITY_SOUND);
				}
			}
		}

		//let us send the keys in a more timely fashion
		if (announce && !announce_keys.empty())
		{
			// NOTE: This fragment is repeated in sendKey
			std::ostringstream ids;
			int transact_num = (int)gFrameCount;
			int num_ids = 0;
			while(!announce_keys.empty())
			{
				LLUUID id = announce_keys.front();
				announce_keys.pop();

				ids << "," << id.asString();
				++num_ids;

				if (ids.tellp() > 200)
				{
					send_keys_message(transact_num, num_ids, ids.str());

					num_ids = 0;
					ids.seekp(0);
					ids.str("");
				}
			}
			if (num_ids > 0)
				send_keys_message(transact_num, num_ids, ids.str());
		}
	}

//	llinfos << "radar refresh: done" << llendl;

	expireAvatarList();

	if (mAvatars.empty())
		setTitle(getString("Title"));
	else if (mAvatars.size() == 1)
		setTitle(getString("TitleOneAvatar"));
	else
	{
		LLStringUtil::format_map_t args;
		args["[COUNT]"] = boost::lexical_cast<std::string>(mAvatars.size());
		setTitle(getString("TitleWithCount", args));
	}

	refreshAvatarList();
	refreshTracker();
}

void LLFloaterAvatarList::expireAvatarList()
{
//	llinfos << "radar: expiring" << llendl;
	for(av_list_t::iterator it = mAvatars.begin(); it != mAvatars.end();)
	{
		LLAvatarListEntry* entry = it->get();
		if(!entry->isDead())
		{
			entry->getAlive();
			++it;
		}
		else
		{
			entry->setPosition(entry->getPosition(), false, false, false, false); // Dead and gone
			it = mAvatars.erase(it);
		}
	}
}

void LLFloaterAvatarList::updateAvatarSorting()
{
	if(mDirtyAvatarSorting)
	{
		mDirtyAvatarSorting = false;
		if(mAvatars.size() <= 1) //Nothing to sort.
			return;
		const std::vector<LLScrollListItem*> list = mAvatarList->getAllData();
		av_list_t::iterator insert_it = mAvatars.begin();
		for(std::vector<LLScrollListItem*>::const_iterator it=list.begin();it!=list.end();++it)
		{
			av_list_t::iterator av_it = std::find_if(mAvatars.begin(),mAvatars.end(),LLAvatarListEntry::uuidMatch((*it)->getUUID()));
			if(av_it!=mAvatars.end())
			{
				std::iter_swap(insert_it++,av_it);
				if(insert_it+1 == mAvatars.end()) //We've ran out of elements to sort
					return;
			}
		}
	}
}

/**
 * Redraws the avatar list
 * Only does anything if the avatar list is visible.
 * @author Dale Glass
 */
void LLFloaterAvatarList::refreshAvatarList() 
{
	// Don't update list when interface is hidden
	if (!getVisible()) return;

	// We rebuild the list fully each time it's refreshed
	// The assumption is that it's faster to refill it and sort than
	// to rebuild the whole list.
	uuid_vec_t selected = mAvatarList->getSelectedIDs();
	S32 scrollpos = mAvatarList->getScrollPos();

	mAvatarList->deleteAllItems();

	LLVector3d mypos = gAgent.getPositionGlobal();
	LLVector3d posagent;
	posagent.setVec(gAgent.getPositionAgent());
	LLVector3d simpos = mypos - posagent;
	const S32 width(gAgent.getRegion() ? gAgent.getRegion()->getWidth() : 256);

	BOOST_FOREACH(av_list_t::value_type& entry, mAvatars)
	{
		LLScrollListItem::Params element;
		LLUUID av_id;
		std::string av_name;

		// Skip if avatar hasn't been around
		if (entry->isDead())
		{
			continue;
		}

		entry->setInList();

		av_id = entry->getID();
		av_name = entry->getName().c_str();

		LLVector3d position = entry->getPosition();
		BOOL UnknownAltitude = false;

		LLVector3d delta = position - mypos;
		F32 distance = (F32)delta.magVec();
		F32 unknownAlt = (gHippoGridManager->getConnectedGrid()->isSecondLife()) ? 1020.f : 0.f;
		if (position.mdV[VZ] == unknownAlt)
		{
			UnknownAltitude = true;
			distance = 9000.0;
		}
		delta.mdV[2] = 0.0f;
		F32 side_distance = (F32)delta.magVec();

		// HACK: Workaround for an apparent bug:
		// sometimes avatar entries get stuck, and are registered
		// by the client as perpetually moving in the same direction.
		// this makes sure they get removed from the visible list eventually

		//jcool410 -- this fucks up seeing dueds thru minimap data > 1024m away, so, lets just say > 2048m to the side is bad
		//aka 8 sims
		if (side_distance > 2048.0f)
		{
			continue;
		}

		element.value = av_id;

		LLScrollListCell::Params mark;
		mark.column = "marked";
		mark.type = "text";
		if (entry->isMarked())
		{
			mark.value = "X";
			mark.color = LLColor4::blue;
			mark.font_style = "BOLD";
		}

		LLScrollListCell::Params name;
		name.column = "avatar_name";
		name.type = "text";
		name.value = av_name;
		if (entry->isFocused())
		{
			name.font_style = "BOLD";
		}

		//<edit> custom colors for certain types of avatars!
		//Changed a bit so people can modify them in settings. And since they're colors, again it's possibly account-based. Starting to think I need a function just to determine that. - HgB
		//name.color = gColors.getColor( "MapAvatar" );
		LLViewerRegion* parent_estate = LLWorld::getInstance()->getRegionFromPosGlobal(entry->getPosition());
		LLUUID estate_owner = LLUUID::null;
		if(parent_estate && parent_estate->isAlive())
		{
			estate_owner = parent_estate->getOwner();
		}

		static const LLCachedControl<LLColor4> unselected_color(gColors, "ScrollUnselectedColor",LLColor4(0.f, 0.f, 0.f, 0.8f));

		static LLCachedControl<LLColor4> sDefaultListText(gColors, "DefaultListText");
		static LLCachedControl<LLColor4> sRadarTextChatRange(gColors, "RadarTextChatRange");
		static LLCachedControl<LLColor4> sRadarTextShoutRange(gColors, "RadarTextShoutRange");
		static LLCachedControl<LLColor4> sRadarTextDrawDist(gColors, "RadarTextDrawDist");
		static LLCachedControl<LLColor4> sRadarTextYoung(gColors, "RadarTextYoung");
		LLColor4 name_color = sDefaultListText;

		//Lindens are always more Linden than your friend, make that take precedence
		if(LLMuteList::getInstance()->isLinden(av_id))
		{
			static const LLCachedControl<LLColor4> ascent_linden_color("AscentLindenColor",LLColor4(0.f,0.f,1.f,1.f));
			name_color = ascent_linden_color;
		}
		//check if they are an estate owner at their current position
		else if(estate_owner.notNull() && av_id == estate_owner)
		{
			static const LLCachedControl<LLColor4> ascent_estate_owner_color("AscentEstateOwnerColor",LLColor4(1.f,0.6f,1.f,1.f));
			name_color = ascent_estate_owner_color;
		}
		//without these dots, SL would suck.
		else if(LLAvatarActions::isFriend(av_id))
		{
			static const LLCachedControl<LLColor4> ascent_friend_color("AscentFriendColor",LLColor4(1.f,1.f,0.f,1.f));
			name_color = ascent_friend_color;
		}
		//big fat jerkface who is probably a jerk, display them as such.
		else if(LLMuteList::getInstance()->isMuted(av_id))
		{
			static const LLCachedControl<LLColor4> ascent_muted_color("AscentMutedColor",LLColor4(0.7f,0.7f,0.7f,1.f));
			name_color = ascent_muted_color;
		}

		name_color = name_color*0.5f + unselected_color*0.5f;

		name.color = name_color;

		char temp[32];
		LLColor4 color = sDefaultListText;
		LLScrollListCell::Params dist;
		dist.column = "distance";
		dist.type = "text";
		if (UnknownAltitude)
		{
			strcpy(temp, "?");
			if (entry->mStats[STAT_TYPE_DRAW])
			{
				color = sRadarTextDrawDist;
			}
		}
		else
		{
			if (distance <= LFSimFeatureHandler::getInstance()->shoutRange())
			{
				snprintf(temp, sizeof(temp), "%.1f", distance);
				if (distance > LFSimFeatureHandler::getInstance()->sayRange())
				{
					color = sRadarTextShoutRange;
				}
				else
				{
					color = sRadarTextChatRange;
				}
			}
			else
			{
				if (entry->mStats[STAT_TYPE_DRAW])
				{
					color = sRadarTextDrawDist;
				}
				snprintf(temp, sizeof(temp), "%d", (S32)distance);
			}
		}
		dist.value = temp;
		dist.color = color;

		LLScrollListCell::Params pos;
		position = position - simpos;

		S32 x = (S32)position.mdV[VX];
		S32 y = (S32)position.mdV[VY];
		if (x >= 0 && x <= width && y >= 0 && y <= width)
		{
			snprintf(temp, sizeof(temp), "%d, %d", x, y);
		}
		else
		{
			temp[0] = '\0';
			if (y < 0)
			{
				strcat(temp, "S");
			}
			else if (y > width)
			{
				strcat(temp, "N");
			}
			if (x < 0)
			{
				strcat(temp, "W");
			}
			else if (x > width)
			{
				strcat(temp, "E");
			}
		}
		pos.column = "position";
		pos.type = "text";
		pos.value = temp;

		LLScrollListCell::Params alt;
		alt.column = "altitude";
		alt.type = "text";
		if (UnknownAltitude)
		{
			strcpy(temp, "?");
		}
		else
		{
			snprintf(temp, sizeof(temp), "%d", (S32)position.mdV[VZ]);
		}
		alt.value = temp;

		LLScrollListCell::Params act;
		act.column = "activity";
		act.type = "icon";

		std::string activity_icon = "";
		std::string activity_tip = "";
		switch(entry->getActivity())
		{
		case LLAvatarListEntry::ACTIVITY_MOVING:
			{
				activity_icon = "inv_item_animation.tga";
				activity_tip = getString("Moving");
			}
			break;
		case LLAvatarListEntry::ACTIVITY_GESTURING:
			{
				activity_icon = "inv_item_gesture.tga";
				activity_tip = getString("Playing a gesture");
			}
			break;
		case LLAvatarListEntry::ACTIVITY_SOUND:
			{
				activity_icon = "inv_item_sound.tga";
				activity_tip = getString("Playing a sound");
			}
			break;
		case LLAvatarListEntry::ACTIVITY_REZZING:
			{
				activity_icon = "ff_edit_theirs.tga";
				activity_tip = getString("Rezzing objects");
			}
			break;
		case LLAvatarListEntry::ACTIVITY_PARTICLES:
			{
				activity_icon = "particles_scan.tga";
				activity_tip = getString("Creating particles");
			}
			break;
		case LLAvatarListEntry::ACTIVITY_NEW:
			{
				activity_icon = "avatar_new.tga";
				activity_tip = getString("Just arrived");
			}
			break;
		case LLAvatarListEntry::ACTIVITY_TYPING:
			{
				activity_icon = "avatar_typing.tga";
				activity_tip = getString("Typing");
			}
			break;
		default:
			break;
		}

		act.value = activity_icon;//icon_image_id; //"icn_active-speakers-dot-lvl0.tga";
		//act.color = icon_color;
		act.tool_tip = activity_tip;

		LLScrollListCell::Params agep;
		agep.column = "age";
		agep.type = "text";
		color = sDefaultListText;
		std::string age = boost::lexical_cast<std::string>(entry->mAge);
		if (entry->mAge > -1)
		{
			static const LLCachedControl<U32> sAvatarAgeAlertDays(gSavedSettings, "AvatarAgeAlertDays");
			if ((U32)entry->mAge < sAvatarAgeAlertDays)
				color = sRadarTextYoung;
		}
		else
		{
			age = "?";
		}
		agep.value = age;
		agep.color = color;

		int dur = difftime(time(NULL), entry->getTime());
		int hours = dur / 3600;
		int mins = (dur % 3600) / 60;
		int secs = (dur % 3600) % 60;

		LLScrollListCell::Params time;
		time.column = "time";
		time.type = "text";
		time.value = llformat("%d:%02d:%02d", hours, mins, secs);

		LLScrollListCell::Params viewer;
		viewer.column = "client";
		viewer.type = "text";

		static const LLCachedControl<LLColor4> avatar_name_color(gColors, "AvatarNameColor",LLColor4(0.98f, 0.69f, 0.36f, 1.f));
		LLColor4 client_color(avatar_name_color);
		LLVOAvatar* avatarp = gObjectList.findAvatar(av_id);
		if(avatarp)
		{
			std::string client = SHClientTagMgr::instance().getClientName(avatarp, false);
			SHClientTagMgr::instance().getClientColor(avatarp, false, client_color);
			if(client == "")
			{
				client_color = unselected_color;
				client = "?";
			}
			viewer.value = client.c_str();
		}
		else
		{
			viewer.value = getString("Out Of Range");
		}
		//Blend to make the color show up better
		client_color = client_color *.5f + unselected_color * .5f;

		viewer.color = client_color;

		// Add individual column cell params to the item param
		element.columns.add(mark);
		element.columns.add(name);
		element.columns.add(dist);
		element.columns.add(pos);
		element.columns.add(alt);
		element.columns.add(act);
		element.columns.add(agep);
		element.columns.add(time);
		element.columns.add(viewer);

		// Add to list
		mAvatarList->addRow(element);
	}

	// finish
	mAvatarList->updateSort();
	mAvatarList->selectMultiple(selected);
	mAvatarList->setScrollPos(scrollpos);
	
	mDirtyAvatarSorting = true;

//	llinfos << "radar refresh: done" << llendl;

}

void LLFloaterAvatarList::onClickIM()
{
	//llinfos << "LLFloaterFriends::onClickIM()" << llendl;
	const uuid_vec_t ids = mAvatarList->getSelectedIDs();
	if (!ids.empty())
	{
		if (ids.size() == 1)
		{
			// Single avatar
			LLAvatarActions::startIM(ids[0]);
		}
		else
		{
			// Group IM
			LLAvatarActions::startConference(ids);
		}
	}
}

void LLFloaterAvatarList::onClickTeleportOffer()
{
	uuid_vec_t ids = mAvatarList->getSelectedIDs();
	if (ids.size() > 0)
	{
		handle_lure(ids);
	}
}

void LLFloaterAvatarList::onClickTrack()
{
	LLScrollListItem* item = mAvatarList->getFirstSelected();
	if (!item) return;

	LLUUID agent_id = item->getUUID();

	if (mTracking && mTrackedAvatar == agent_id)
	{
		LLTracker::stopTracking(false);
		mTracking = FALSE;
	}
	else
	{
		mTracking = TRUE;
		mTrackedAvatar = agent_id;
//		trackAvatar only works for friends allowing you to see them on map...
//		LLTracker::trackAvatar(agent_id, self->mAvatars[agent_id].getName());
		trackAvatar(getAvatarEntry(mTrackedAvatar));
	}
}

void LLFloaterAvatarList::refreshTracker()
{
	if (!mTracking) return;

	if (LLTracker::isTracking())
	{
		if(LLAvatarListEntry* entry = getAvatarEntry(mTrackedAvatar))
		{
			if (entry->getPosition() != LLTracker::getTrackedPositionGlobal())
			{
				trackAvatar(entry);
			}
		}
	}
	else
	{	// Tracker stopped.
		LLTracker::stopTracking(false);
		mTracking = FALSE;
//		llinfos << "Tracking stopped." << llendl;
	}
}

void LLFloaterAvatarList::trackAvatar(const LLAvatarListEntry* entry)
{
	if(!entry) return;
	std::string name = entry->getName();
	if (!mUpdate)
	{
		name += "\n(last known position)";
	}
	LLTracker::trackLocation(entry->getPosition(), name, name);
}

LLAvatarListEntry * LLFloaterAvatarList::getAvatarEntry(LLUUID avatar)
{
	if (avatar.isNull())
	{
		return NULL;
	}

	av_list_t::iterator iter = std::find_if(mAvatars.begin(),mAvatars.end(),LLAvatarListEntry::uuidMatch(avatar));
	if(iter != mAvatars.end())
		return iter->get();
	else
		return NULL;
}

BOOL LLFloaterAvatarList::handleKeyHere(KEY key, MASK mask)
{
	LLScrollListItem* item = mAvatarList->getFirstSelected();
	if(item)
	{
		LLUUID agent_id = item->getUUID();
		if (( KEY_RETURN == key ) && (MASK_NONE == mask))
		{
			setFocusAvatar(agent_id);
			return TRUE;
		}
		else if (( KEY_RETURN == key ) && (MASK_CONTROL == mask))
		{
			const LLAvatarListEntry* entry = getAvatarEntry(agent_id);
			if (entry)
			{
//				llinfos << "Trying to teleport to " << entry->getName() << " at " << entry->getPosition() << llendl;
				gAgent.teleportViaLocation(entry->getPosition());
			}
			return TRUE;
		}
	}

	if (( KEY_RETURN == key ) && (MASK_SHIFT == mask))
	{
		onClickIM();
	}
	return LLPanel::handleKeyHere(key, mask);
}

void LLFloaterAvatarList::onClickFocus()
{
	LLScrollListItem* item = mAvatarList->getFirstSelected();
	if (item)
	{
		setFocusAvatar(item->getUUID());
	}
}

void LLFloaterAvatarList::removeFocusFromAll()
{
	BOOST_FOREACH(av_list_t::value_type& entry, mAvatars)
	{
		entry->setFocus(FALSE);
	}
}

void LLFloaterAvatarList::setFocusAvatar(const LLUUID& id)
{
	av_list_t::iterator iter = std::find_if(mAvatars.begin(),mAvatars.end(),LLAvatarListEntry::uuidMatch(id));
	if(iter != mAvatars.end())
	{
		if(!gAgentCamera.lookAtObject(id, false))
			return;
		removeFocusFromAll();
		(*iter)->setFocus(TRUE);
	}
}

template<typename T>
void decrement_focus_target(T begin, T end, BOOL marked_only)
{
	T iter = begin;
	while(iter != end && !(*iter)->isFocused()) ++iter;
	if(iter == end)
		return;
	T prev_iter = iter;
	while(prev_iter != begin)
	{
		const LLAvatarListEntry& entry = *((--prev_iter)->get());
		if(entry.isInList() && (entry.isMarked() || !marked_only) && gAgentCamera.lookAtObject(entry.getID(), false))
		{
			(*iter)->setFocus(FALSE);
			(*prev_iter)->setFocus(TRUE);
			gAgentCamera.lookAtObject((*prev_iter)->getID(), false);
			return;
		}
	}
	gAgentCamera.lookAtObject((*iter)->getID(), false);
}

void LLFloaterAvatarList::focusOnPrev(BOOL marked_only)
{
	updateAvatarSorting();
	decrement_focus_target(mAvatars.begin(), mAvatars.end(), marked_only);
}

void LLFloaterAvatarList::focusOnNext(BOOL marked_only)
{
	updateAvatarSorting();
	decrement_focus_target(mAvatars.rbegin(), mAvatars.rend(), marked_only);
}

/*static*/
void LLFloaterAvatarList::lookAtAvatar(LLUUID &uuid)
{ // twisted laws
	LLVOAvatar* voavatar = gObjectList.findAvatar(uuid);
	if(voavatar && voavatar->isAvatar())
	{
		gAgentCamera.setFocusOnAvatar(FALSE, FALSE);
		gAgentCamera.changeCameraToThirdPerson();
		gAgentCamera.setFocusGlobal(voavatar->getPositionGlobal(),uuid);
		gAgentCamera.setCameraPosAndFocusGlobal(voavatar->getPositionGlobal()
				+ LLVector3d(3.5,1.35,0.75) * voavatar->getRotation(),
												voavatar->getPositionGlobal(),
												uuid );
	}
}

void LLFloaterAvatarList::onClickGetKey()
{
	LLScrollListItem* item = mAvatarList->getFirstSelected();

	if (NULL == item) return;

	gViewerWindow->getWindow()->copyTextToClipboard(utf8str_to_wstring(item->getUUID().asString()));
}

void LLFloaterAvatarList::sendKeys()
{
	// This would break for send_keys_btn callback, check this beforehand, if it matters.
	//static LLCachedControl<bool> radar_chat_keys(gSavedSettings, "RadarChatKeys");
	//if (radar_chat_keys) return;

	LLViewerRegion* regionp = gAgent.getRegion();
	if (!regionp) return;//ALWAYS VALIDATE DATA

	static int last_transact_num = 0;
	int transact_num = (int)gFrameCount;

	if (transact_num > last_transact_num)
	{
		last_transact_num = transact_num;
	}
	else
	{
		//on purpose, avatar IDs on map don't change until the next frame.
		//no need to send more than once a frame.
		return;
	}

	std::ostringstream ids;
	int num_ids = 0;

	for (int i = 0; i < regionp->mMapAvatarIDs.count(); ++i)
	{
		const LLUUID &id = regionp->mMapAvatarIDs.get(i);

		ids << "," << id;
		++num_ids;


		if (ids.tellp() > 200)
		{
			send_keys_message(transact_num, num_ids, ids.str());

			num_ids = 0;
			ids.seekp(0);
			ids.str("");
		}
	}
	if (num_ids > 0)
		send_keys_message(transact_num, num_ids, ids.str());
}
//static
void LLFloaterAvatarList::sound_trigger_hook(LLMessageSystem* msg,void **)
{
	if (!LLFloaterAvatarList::instanceExists()) return; // Don't bother if we're closed.

	LLUUID  sound_id,owner_id;
	msg->getUUIDFast(_PREHASH_SoundData, _PREHASH_SoundID, sound_id);
	msg->getUUIDFast(_PREHASH_SoundData, _PREHASH_OwnerID, owner_id);
	if (owner_id == gAgent.getID() && sound_id == LLUUID("76c78607-93f9-f55a-5238-e19b1a181389"))
	{
		static LLCachedControl<bool> on("RadarChatKeys");
		static LLCachedControl<bool> do_not_ask("RadarChatKeysStopAsking");
		if (on)
			LLFloaterAvatarList::getInstance()->sendKeys();
		else if (!do_not_ask) // Let's ask if they want to turn it on, but not pester them.
			LLNotificationsUtil::add("RadarChatKeysRequest", LLSD(), LLSD(), onConfirmRadarChatKeys);
	}
}
// static
bool LLFloaterAvatarList::onConfirmRadarChatKeys(const LLSD& notification, const LLSD& response )
{
	S32 option = LLNotification::getSelectedOption(notification, response);
	if (option == 1) // no
	{
		return false;
	}
	else if (option == 0) // yes
	{
		gSavedSettings.setBOOL("RadarChatKeys", true);
		LLFloaterAvatarList::getInstance()->sendKeys();
	}
	else if (option == 2) // No, and stop asking!!
	{
		gSavedSettings.setBOOL("RadarChatKeysStopAsking", true);
	}

	return false;
}

void send_freeze(const LLUUID& avatar_id, bool freeze)
{
	U32 flags = 0x0;
	if (!freeze)
	{
		// unfreeze
		flags |= 0x1;
	}

	LLMessageSystem* msg = gMessageSystem;
	LLVOAvatar* avatarp = gObjectList.findAvatar(avatar_id);

	if (avatarp && avatarp->getRegion())
	{
		msg->newMessage("FreezeUser");
		msg->nextBlock("AgentData");
		msg->addUUID("AgentID", gAgent.getID());
		msg->addUUID("SessionID", gAgent.getSessionID());
		msg->nextBlock("Data");
		msg->addUUID("TargetID", avatar_id);
		msg->addU32("Flags", flags);
		msg->sendReliable( avatarp->getRegion()->getHost());
	}
}

void send_eject(const LLUUID& avatar_id, bool ban)
{	
	LLMessageSystem* msg = gMessageSystem;
	LLVOAvatar* avatarp = gObjectList.findAvatar(avatar_id);

	if (avatarp && avatarp->getRegion())
	{
		U32 flags = 0x0;
		if (ban)
		{
			// eject and add to ban list
			flags |= 0x1;
		}

		msg->newMessage("EjectUser");
		msg->nextBlock("AgentData");
		msg->addUUID("AgentID", gAgent.getID());
		msg->addUUID("SessionID", gAgent.getSessionID());
		msg->nextBlock("Data");
		msg->addUUID("TargetID", avatar_id);
		msg->addU32("Flags", flags);
		msg->sendReliable(avatarp->getRegion()->getHost());
	}
}

static void send_estate_message(
	const char* request,
	const LLUUID& target)
{

	LLMessageSystem* msg = gMessageSystem;
	LLUUID invoice;

	// This seems to provide an ID so that the sim can say which request it's
	// replying to. I think this can be ignored for now.
	invoice.generate();

	llinfos << "Sending estate request '" << request << "'" << llendl;
	msg->newMessage("EstateOwnerMessage");
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
	msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
	msg->addUUIDFast(_PREHASH_TransactionID, LLUUID::null); //not used
	msg->nextBlock("MethodData");
	msg->addString("Method", request);
	msg->addUUID("Invoice", invoice);

	// Agent id
	msg->nextBlock("ParamList");
	msg->addString("Parameter", gAgent.getID().asString().c_str());

	// Target
	msg->nextBlock("ParamList");
	msg->addString("Parameter", target.asString().c_str());

	msg->sendReliable(gAgent.getRegion()->getHost());
}

static void cmd_append_names(const LLAvatarListEntry* entry, std::string &str, std::string &sep)
															{ if(!str.empty())str.append(sep);str.append(entry->getName()); }
static void cmd_toggle_mark(LLAvatarListEntry* entry)		{ entry->toggleMark(); }
static void cmd_ar(const LLAvatarListEntry* entry)			{ LLFloaterReporter::showFromObject(entry->getID()); }
static void cmd_profile(const LLAvatarListEntry* entry)		{ LLAvatarActions::showProfile(entry->getID()); }
static void cmd_teleport(const LLAvatarListEntry* entry)	{ gAgent.teleportViaLocation(entry->getPosition()); }
static void cmd_freeze(const LLAvatarListEntry* entry)		{ send_freeze(entry->getID(), true); }
static void cmd_unfreeze(const LLAvatarListEntry* entry)	{ send_freeze(entry->getID(), false); }
static void cmd_eject(const LLAvatarListEntry* entry)		{ send_eject(entry->getID(), false); }
static void cmd_ban(const LLAvatarListEntry* entry)			{ send_eject(entry->getID(), true); }
static void cmd_estate_eject(const LLAvatarListEntry* entry){ send_estate_message("teleporthomeuser", entry->getID()); }
static void cmd_estate_ban(const LLAvatarListEntry* entry)	{ LLPanelEstateInfo::sendEstateAccessDelta(ESTATE_ACCESS_BANNED_AGENT_ADD | ESTATE_ACCESS_ALLOWED_AGENT_REMOVE | ESTATE_ACCESS_NO_REPLY, entry->getID()); }

void LLFloaterAvatarList::doCommand(avlist_command_t func, bool single/*=false*/)
{
	uuid_vec_t ids;
	if(!single)
		ids = mAvatarList->getSelectedIDs();
	else
		ids.push_back(getSelectedID());
	for (uuid_vec_t::iterator itr = ids.begin(); itr != ids.end(); ++itr)
	{
		LLUUID& avid = *itr;
		if(avid.isNull())
			continue;
		LLAvatarListEntry* entry = getAvatarEntry(avid);
		if (entry != NULL)
		{
			llinfos << "Executing command on " << entry->getName() << llendl;
			func(entry);
		}
	}
}

std::string LLFloaterAvatarList::getSelectedNames(const std::string& separator)
{
	std::string ret;
	doCommand(boost::bind(&cmd_append_names,_1,boost::ref(ret),separator));
	return ret;
}

std::string LLFloaterAvatarList::getSelectedName()
{
	LLUUID id = getSelectedID();
	LLAvatarListEntry* entry = getAvatarEntry(id);
	if (entry)
	{
		return entry->getName();
	}
	return "";
}

LLUUID LLFloaterAvatarList::getSelectedID()
{
	LLScrollListItem* item = mAvatarList->getFirstSelected();
	if (item) return item->getUUID();
	return LLUUID::null;
}

//static 
void LLFloaterAvatarList::callbackFreeze(const LLSD& notification, const LLSD& response)
{
	if(!instanceExists())
		return;

	S32 option = LLNotification::getSelectedOption(notification, response);

	if (option == 0)
	{
		getInstance()->doCommand(cmd_freeze);
	}
	else if (option == 1)
	{
		getInstance()->doCommand(cmd_unfreeze);
	}
}

//static 
void LLFloaterAvatarList::callbackEject(const LLSD& notification, const LLSD& response)
{
	if(!instanceExists())
		return;

	S32 option = LLNotification::getSelectedOption(notification, response);

	if (option == 0)
	{
		getInstance()->doCommand(cmd_eject);
	}
	else if (option == 1)
	{
		getInstance()->doCommand(cmd_ban);
	}
}

//static 
void LLFloaterAvatarList::callbackEjectFromEstate(const LLSD& notification, const LLSD& response)
{
	if(!instanceExists())
		return;

	S32 option = LLNotification::getSelectedOption(notification, response);

	if (option == 0)
	{
		getInstance()->doCommand(cmd_estate_eject);
	}
}

//static
void LLFloaterAvatarList::callbackBanFromEstate(const LLSD& notification, const LLSD& response)
{
	if(!instanceExists())
		return;

	S32 option = LLNotification::getSelectedOption(notification, response);

	if (option == 0)
	{
		getInstance()->doCommand(cmd_estate_eject); //Eject first, just in case.
		getInstance()->doCommand(cmd_estate_ban);
	}
}

//static
void LLFloaterAvatarList::callbackIdle(void* userdata)
{
	if (instanceExists())
	{
		// Do not update at every frame: this would be insane!
		if (gFrameCount % getInstance()->mUpdateRate == 0)
		{
			getInstance()->updateAvatarList();
		}
	}
}

void LLFloaterAvatarList::onClickFreeze()
{
	LLSD args;
	LLSD payload;
	args["AVATAR_NAME"] = getSelectedNames();
	LLNotificationsUtil::add("FreezeAvatarFullname", args, payload, callbackFreeze);
}

void LLFloaterAvatarList::onClickEject()
{
	LLSD args;
	LLSD payload;
	args["AVATAR_NAME"] = getSelectedNames();
	LLNotificationsUtil::add("EjectAvatarFullname", args, payload, callbackEject);
}

void LLFloaterAvatarList::onClickMute()
{
	uuid_vec_t ids = mAvatarList->getSelectedIDs();
	if (ids.size() > 0)
	{
		for (uuid_vec_t::iterator itr = ids.begin(); itr != ids.end(); ++itr)
		{
			LLUUID agent_id = *itr;

			std::string agent_name;
			if (gCacheName->getFullName(agent_id, agent_name))
			{
				if (LLMuteList::getInstance()->isMuted(agent_id))
				{
					LLMute mute(agent_id, agent_name, LLMute::AGENT);
					LLMuteList::getInstance()->remove(mute);	
				}
				else
				{
					LLMute mute(agent_id, agent_name, LLMute::AGENT);
					LLMuteList::getInstance()->add(mute);
				}
			}
		}
	}
}

void LLFloaterAvatarList::onClickEjectFromEstate()
{
	LLSD args;
	LLSD payload;
	args["EVIL_USER"] = getSelectedNames();
	LLNotificationsUtil::add("EstateKickUser", args, payload, callbackEjectFromEstate);
}

void LLFloaterAvatarList::onClickBanFromEstate()
{
	LLSD args;
	LLSD payload;
	args["EVIL_USER"] = getSelectedNames();
	LLNotificationsUtil::add("EstateBanUser", args, payload, callbackBanFromEstate);
}

void LLFloaterAvatarList::onAvatarSortingChanged()
{
	mDirtyAvatarSorting = true;
}

void LLFloaterAvatarList::onSelectName()
{
	LLScrollListItem* item = mAvatarList->getFirstSelected();
	if (item)
	{
		LLUUID agent_id = item->getUUID();
		LLAvatarListEntry* entry = getAvatarEntry(agent_id);
		if (entry)
		{
			BOOL enabled = entry->mStats[STAT_TYPE_DRAW];
			childSetEnabled("focus_btn", enabled);
			childSetEnabled("prev_in_list_btn", enabled);
			childSetEnabled("next_in_list_btn", enabled);
		}
	}
}

void LLFloaterAvatarList::onCommitUpdateRate()
{
	mUpdateRate = gSavedSettings.getU32("RadarUpdateRate") * 3 + 3;
}
