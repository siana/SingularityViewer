/** 
* @file llstatusbar.cpp
* @brief LLStatusBar class implementation
*
* $LicenseInfo:firstyear=2002&license=viewergpl$
* 
* Copyright (c) 2002-2009, Linden Research, Inc.
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

#include "llviewerprecompiledheaders.h"

#include "llstatusbar.h"

// viewer includes
#include "llagent.h"
#include "llbutton.h"
#include "llcommandhandler.h"
#include "llfloaterbuycurrency.h"
#include "llfloaterchat.h"
#include "llfloaterinventory.h"
#include "llfloaterland.h"
#include "llfloaterregioninfo.h"
#include "llfloaterscriptdebug.h"
#include "llfloatersearch.h"
#include "llfloaterstats.h"
#include "llhudicon.h"
#include "llkeyboard.h"
#include "lllineeditor.h"
#include "llmenugl.h"
#include "llmenuoptionpathfindingrebakenavmesh.h"
#include "llnotify.h"
#include "llpathfindingmanager.h"
#include "llpathfindingnavmeshstatus.h"
#include "llimview.h"
#include "lltextbox.h"
#include "lltrans.h"
#include "llui.h"
#include "llviewerparceloverlay.h"
#include "llviewerregion.h"
#include "llviewerstats.h"
#include "llviewerwindow.h"
#include "llframetimer.h"
#include "llvoavatarself.h"
#include "llresmgr.h"
#include "llworld.h"
#include "llstatgraph.h"
#include "llviewercontrol.h"
#include "llviewergenericmessage.h"
#include "llviewermenu.h"	// for gMenuBarView
#include "llviewerparcelmgr.h"
#include "llviewerthrottle.h"
#include "lluictrlfactory.h"
#include "llagentui.h"

#include "lltoolmgr.h"
#include "llfocusmgr.h"
#include "llappviewer.h"

// library includes
#include "imageids.h"
#include "llfontgl.h"
#include "llrect.h"
#include "llerror.h"
#include "llnotificationsutil.h"
#include "llparcel.h"
#include "llstring.h"
#include "message.h"

// system includes
#include <iomanip>

#include "hippogridmanager.h"
#include "lfsimfeaturehandler.h"

// [RLVa:KB]
#include "rlvactions.h"
#include "rlvcommon.h"
// [/RLVa:KB]

//
// Globals
//
LLStatusBar *gStatusBar = NULL;
S32 STATUS_BAR_HEIGHT = 0;
extern S32 MENU_BAR_HEIGHT;


// TODO: these values ought to be in the XML too
const S32 MENU_PARCEL_SPACING = 1;	// Distance from right of menu item to parcel information
const S32 SIM_STAT_WIDTH = 8;
const F32 SIM_WARN_FRACTION = 0.75f;
const F32 SIM_FULL_FRACTION = 0.98f;
const LLColor4 SIM_OK_COLOR(0.f, 1.f, 0.f, 1.f);
const LLColor4 SIM_WARN_COLOR(1.f, 1.f, 0.f, 1.f);
const LLColor4 SIM_FULL_COLOR(1.f, 0.f, 0.f, 1.f);
const F32 ICON_TIMER_EXPIRY		= 3.f; // How long the balance and health icons should flash after a change.
const F32 ICON_FLASH_FREQUENCY	= 2.f;
const S32 TEXT_HEIGHT = 18;

static void onClickParcelInfo();
static bool rebakeRegionCallback(const LLSD& n, const LLSD& r);
static void pf_dirty_click() { LLNotificationsUtil::add("PathfindingDirty", LLSD(), LLSD(), rebakeRegionCallback); }
static void onClickScripts();
static void onClickSearch(const std::string& query);

std::vector<std::string> LLStatusBar::sDays;
std::vector<std::string> LLStatusBar::sMonths;
const U32 LLStatusBar::MAX_DATE_STRING_LENGTH = 2000;

class LLDispatchUPCBalance : public LLDispatchHandler
{
public:
	virtual bool operator()(
		const LLDispatcher* dispatcher,
		const std::string& key,
		const LLUUID& invoice,
		const sparam_t& strings)
	{
		S32 upc = atoi(strings[0].c_str());
		gStatusBar->setUPC(upc);
		return true;
	}
};

static LLDispatchUPCBalance sDispatchUPCBalance;

static void toggle_time_value()
{
	LLControlVariable* control = gSavedSettings.getControl("LiruLocalTime");
	control->set(!control->get());
}

LLStatusBar::LLStatusBar(const std::string& name, const LLRect& rect)
:	LLPanel(name, LLRect(), FALSE),		// not mouse opaque
mBalance(0),
mUPC(0),
mHealth(100),
mSquareMetersCredit(0),
mSquareMetersCommitted(0),
mRegionCrossingSlot(),
mNavMeshSlot(),
mIsNavMeshDirty(false)
{
	mUPCSupported = gHippoGridManager->getConnectedGrid()->getUPCSupported();

	if (mUPCSupported)
		gGenericDispatcher.addHandler("upcbalance", &sDispatchUPCBalance);

	// status bar can possible overlay menus?
	setMouseOpaque(FALSE);
	setIsChrome(TRUE);

	// size of day of the weeks and year
	sDays.reserve(7);
	sMonths.reserve(12);

	mBalanceTimer = new LLFrameTimer();
	mHealthTimer = new LLFrameTimer();

	LLUICtrlFactory::getInstance()->buildPanel(this,"panel_status_bar.xml");

	// status bar can never get a tab
	setFocusRoot(FALSE);

	// build date necessary data (must do after panel built)
	setupDate();

	mTextParcelName = getChild<LLTextBox>("ParcelNameText" );
	mTextBalance = getChild<LLTextBox>("BalanceText" );
	mTextUPC = getChild<LLTextBox>("UPCText" );

	mTextHealth = getChild<LLTextBox>("HealthText" );
	mTextTime = getChild<LLTextBox>("TimeText" );
	mTextTime->setClickedCallback(boost::bind(toggle_time_value));

	if (!mUPCSupported)
		mTextUPC->setVisible(false);

	mScriptOut = getChild<LLUICtrl>("scriptout");
	mScriptOut->setCommitCallback(boost::bind(LLFloaterScriptDebug::show, LLUUID::null));
	mHealthV = getChild<LLUICtrl>("health");
	mHealthV->setCommitCallback(boost::bind(LLNotificationsUtil::add, "NotSafe"));
	mNoFly = getChild<LLUICtrl>("no_fly");
	mNoFly->setCommitCallback(boost::bind(LLNotificationsUtil::add, "NoFly"));
	mBuyLand = getChild<LLUICtrl>("buyland");
	mBuyLand->setCommitCallback(boost::bind(&LLViewerParcelMgr::startBuyLand, boost::bind(LLViewerParcelMgr::getInstance), 0));
	mBuyCurrency = getChild<LLUICtrl>("buycurrency");
	mBuyCurrency->setCommitCallback(boost::bind(LLFloaterBuyCurrency::buyCurrency));
	mNoBuild = getChild<LLUICtrl>("no_build");
	mNoBuild->setCommitCallback(boost::bind(LLNotificationsUtil::add, "NoBuild"));
	mPFDirty = getChild<LLUICtrl>("pf_dirty");
	mPFDirty->setCommitCallback(boost::bind(pf_dirty_click));
	mPFDisabled = getChild<LLUICtrl>("pf_disabled");
	mPFDisabled->setCommitCallback(boost::bind(LLNotificationsUtil::add, "DynamicPathfindingDisabled"));
	mStatusSeeAV = getChild<LLUICtrl>("status_SeeAV");
	mStatusSeeAV->setCommitCallback(boost::bind(LLNotificationsUtil::add, "SeeAvatars"));
	mNoScripts = getChild<LLUICtrl>("no_scripts");
	mNoScripts->setCommitCallback(boost::bind(onClickScripts));
	mRestrictPush = getChild<LLUICtrl>("restrictpush");
	mRestrictPush->setCommitCallback(boost::bind(LLNotificationsUtil::add, "PushRestricted"));
	mStatusNoVoice = getChild<LLUICtrl>("status_no_voice");
	mStatusNoVoice->setCommitCallback(boost::bind(LLNotificationsUtil::add, "NoVoice"));

	mSearchEditor = getChild<LLUICtrl>("search_editor");
	mSearchEditor->setCommitCallback(boost::bind(onClickSearch, _2));
	mSearchBtn = getChild<LLUICtrl>("search_btn");
	mSearchBtn->setCommitCallback(boost::bind(onClickSearch, boost::bind(&LLView::getValue, mSearchEditor)));

	bool show_search(gSavedSettings.getBOOL("ShowSearchBar"));
	mSearchEditor->setVisible(show_search);
	mSearchBtn->setVisible(show_search);
	mSearchBevel = getChildView("menubar_search_bevel_bg");
	mSearchBevel->setVisible(show_search);

	mTextParcelName->setClickedCallback(boost::bind(onClickParcelInfo));
	mTextBalance->setClickedCallback(boost::bind(LLStatusBar::sendMoneyBalanceRequest, true));

	// TODO: Disable buying currency when connected to non-SL grids
	// that don't support currency yet -- MC
	mBuyCurrency->setLabelArg("[CURRENCY]", gHippoGridManager->getConnectedGrid()->getCurrencySymbol());

	mRegionCrossingSlot = gAgent.addRegionChangedCallback(boost::bind(&LLStatusBar::createNavMeshStatusListenerForCurrentRegion, this));
	createNavMeshStatusListenerForCurrentRegion();

	// Adding Net Stat Graph
	S32 x = getRect().getWidth() - 2;
	S32 y = 0;
	LLRect r;
	r.set( x-SIM_STAT_WIDTH, y+MENU_BAR_HEIGHT-1, x, y+1);
	mSGBandwidth = new LLStatGraph("BandwidthGraph", r);
	mSGBandwidth->setFollows(FOLLOWS_BOTTOM | FOLLOWS_RIGHT);
	mSGBandwidth->setStat(&LLViewerStats::getInstance()->mKBitStat);
	mSGBandwidth->setLabel(getString("bandwidth_tooltip") + " ");
	mSGBandwidth->setUnits("Kbps");
	mSGBandwidth->setPrecision(0);
	mSGBandwidth->setMouseOpaque(FALSE);
	addChild(mSGBandwidth);
	x -= SIM_STAT_WIDTH + 2;

	r.set( x-SIM_STAT_WIDTH, y+MENU_BAR_HEIGHT-1, x, y+1);
	mSGPacketLoss = new LLStatGraph("PacketLossPercent", r);
	mSGPacketLoss->setFollows(FOLLOWS_BOTTOM | FOLLOWS_RIGHT);
	mSGPacketLoss->setStat(&LLViewerStats::getInstance()->mPacketsLostPercentStat);
	mSGPacketLoss->setLabel(getString("packet_loss_tooltip") + " ");
	mSGPacketLoss->setUnits("%");
	mSGPacketLoss->setMin(0.f);
	mSGPacketLoss->setMax(5.f);
	mSGPacketLoss->setThreshold(0, 0.5f);
	mSGPacketLoss->setThreshold(1, 1.f);
	mSGPacketLoss->setThreshold(2, 3.f);
	mSGPacketLoss->setPrecision(1);
	mSGPacketLoss->setMouseOpaque(FALSE);
	mSGPacketLoss->mPerSec = FALSE;
	addChild(mSGPacketLoss);

	mStatBtn = getChild<LLTextBox>("stat_btn");
	mStatBtn->setClickedCallback(boost::bind(LLFloaterStats::toggleInstance, LLSD()));
}

LLStatusBar::~LLStatusBar()
{
	delete mBalanceTimer;
	mBalanceTimer = NULL;

	delete mHealthTimer;
	mHealthTimer = NULL;

	mRegionCrossingSlot.disconnect();
	mNavMeshSlot.disconnect();

	// LLView destructor cleans up children
}

//-----------------------------------------------------------------------
// Overrides
//-----------------------------------------------------------------------

// virtual
void LLStatusBar::draw()
{
	refresh();

	if (isBackgroundVisible())
	{
		static const LLCachedControl<LLColor4> color_drop_shadow(*LLUI::sColorsGroup,"ColorDropShadow",LLColor4(LLColor4U(0,0,0,200)));
		static const LLCachedControl<S32> drop_shadow_floater(*LLUI::sConfigGroup,"DropShadowFloater");
		gl_drop_shadow(0, getRect().getHeight(), getRect().getWidth(), 0, 
			color_drop_shadow, 
			drop_shadow_floater );
	}
	LLPanel::draw();
}


// Per-frame updates of visibility
void LLStatusBar::refresh()
{
	if(gDisconnected)
	return; //or crash if the sim crashes; because: already ~LLMenuBarGL()

	// Adding Net Stat Meter back in
	F32 bwtotal = gViewerThrottle.getMaxBandwidth() / 1000.f;
	mSGBandwidth->setMin(0.f);
	mSGBandwidth->setMax(bwtotal*1.25f);
	mSGBandwidth->setThreshold(0, bwtotal*0.75f);
	mSGBandwidth->setThreshold(1, bwtotal);
	mSGBandwidth->setThreshold(2, bwtotal);

	// Singu Note: Use system's time if the user desires, otherwise use server time
	static const LLCachedControl<bool> show_local_time("LiruLocalTime");

	// Get current UTC time, adjusted for the user's clock
	// being off.
	time_t utc_time = show_local_time ? time(NULL) : time_corrected();

	// There's only one internal tm buffer.
	struct tm* internal_time;

	// Convert to Pacific, based on server's opinion of whether
	// it's daylight savings time there.
	internal_time = show_local_time ? std::localtime(&utc_time) : utc_to_pacific_time(utc_time, gPacificDaylightTime);

	static const LLCachedControl<std::string> short_time_fmt(gSavedSettings, "ShortTimeFormat");
	std::string t;
	timeStructToFormattedString(internal_time, short_time_fmt, t);
	if (show_local_time)
	{
		static const std::string local(" " + getString("Local"));
		t += local;
	}
	else if (gPacificDaylightTime)
	{
		t += " PDT";
	}
	else
	{
		t += " PST";
	}
	mTextTime->setText(t);

	static const LLCachedControl<std::string> long_date_fmt(gSavedSettings, "LongDateFormat");
	std::string date;
	timeStructToFormattedString(internal_time, long_date_fmt, date);
	mTextTime->setToolTip(date);

	LLRect r;
	const S32 MENU_RIGHT = gMenuBarView->getRightmostMenuEdge();
	S32 x = MENU_RIGHT + MENU_PARCEL_SPACING;
	S32 y = 0;

	// reshape menu bar to its content's width
	if (MENU_RIGHT != gMenuBarView->getRect().getWidth())
	{
		gMenuBarView->reshape(MENU_RIGHT, gMenuBarView->getRect().getHeight());
	}

	LLViewerRegion *region = gAgent.getRegion();
	LLParcel *parcel = LLViewerParcelMgr::getInstance()->getAgentParcel();

	if (LLHUDIcon::iconsNearby())
	{
		const LLRect& buttonRect = mScriptOut->getRect();
		r.setOriginAndSize( x, y, buttonRect.getWidth(), buttonRect.getHeight());
		mScriptOut->setRect(r);
		mScriptOut->setVisible(true);
		x += buttonRect.getWidth();
	}
	else
	{
		mScriptOut->setVisible(false);
	}

	if ((region && region->getAllowDamage()) ||
		(parcel && parcel->getAllowDamage()) )
	{
		// set visibility based on flashing
		mHealthV->setVisible(mHealthTimer->hasExpired() || S32(mHealthTimer->getElapsedSeconds() * ICON_FLASH_FREQUENCY) & 1);
		mTextHealth->setVisible(TRUE);

		// Health
		const LLRect& buttonRect = mHealthV->getRect();
		r.setOriginAndSize( x, y, buttonRect.getWidth(), buttonRect.getHeight());
		mHealthV->setRect(r);
		x += buttonRect.getWidth();

		const S32 health_width = S32( LLFontGL::getFontSansSerifSmall()->getWidth(std::string("100%")) );
		r.set(x, y+TEXT_HEIGHT - 2, x+health_width, y);
		mTextHealth->setRect(r);
		x += health_width;
	}
	else
	{
		// invisible if region doesn't allow damage
		mHealthV->setVisible(false);
		mTextHealth->setVisible(FALSE);
	}

	if ((region && region->getBlockFly()) ||
		(parcel && !parcel->getAllowFly()) )
	{
		// No Fly Zone
		mNoFly->setVisible(true);
		const LLRect& buttonRect(mNoFly->getRect());
		r.setOriginAndSize( x, y, buttonRect.getWidth(), buttonRect.getHeight());
		mNoFly->setRect(r);
		x += buttonRect.getWidth();
	}
	else
	{
		// Fly Zone
		mNoFly->setVisible(false);
	}

	if (parcel && !parcel->getAllowModify())
	{
		// No Build Zone
		mNoBuild->setVisible(true);
		const LLRect& buttonRect(mNoBuild->getRect());
		r.setOriginAndSize( x, y, buttonRect.getWidth(), buttonRect.getHeight());
		mNoBuild->setRect(r);
		x += buttonRect.getWidth();
	}
	else
	{
		mNoBuild->setVisible(false);
	}

	if ((region
		&& (region->getRegionFlag(REGION_FLAGS_SKIP_SCRIPTS)
		|| region->getRegionFlag(REGION_FLAGS_ESTATE_SKIP_SCRIPTS)))
		|| (parcel && !parcel->getAllowOtherScripts()))
	{
		// No scripts
		mNoScripts->setVisible(true);
		const LLRect& buttonRect(mNoScripts->getRect());
		r.setOriginAndSize( x, y, buttonRect.getWidth(), buttonRect.getHeight());
		mNoScripts->setRect(r);
		x += buttonRect.getWidth();
	}
	else
	{
		// Yes scripts
		mNoScripts->setVisible(false);
	}

	if ((region && region->getRestrictPushObject()) || (parcel && parcel->getRestrictPushObject()))
	{
		mRestrictPush->setVisible(true);
		const LLRect& buttonRect(mRestrictPush->getRect());
		r.setOriginAndSize( x, y, buttonRect.getWidth(), buttonRect.getHeight());
		mRestrictPush->setRect(r);
		x += buttonRect.getWidth();
	}
	else
	{
		mRestrictPush->setVisible(false);
	}

	if (parcel && parcel->getParcelFlagAllowVoice())
	{
		mStatusNoVoice->setVisible(false);
	}
	else
	{
		mStatusNoVoice->setVisible(true);
		const LLRect& buttonRect(mStatusNoVoice->getRect());
		r.setOriginAndSize( x, y, buttonRect.getWidth(), buttonRect.getHeight());
		mStatusNoVoice->setRect(r);
		x += buttonRect.getWidth();
	}

	if (parcel && !parcel->getSeeAVs())
	{
		mStatusSeeAV->setVisible(true);
		const LLRect& buttonRect(mStatusSeeAV->getRect());
		r.setOriginAndSize( x, y, buttonRect.getWidth(), buttonRect.getHeight());
		mStatusSeeAV->setRect(r);
		x += buttonRect.getWidth();
	}
	else mStatusSeeAV->setVisible(false);

	if (region)
	{
		bool pf_disabled = !region->dynamicPathfindingEnabled();
		mPFDirty->setVisible(!pf_disabled && mIsNavMeshDirty);
		mPFDisabled->setVisible(pf_disabled);
		if (LLView* pf_icon = pf_disabled ? mPFDisabled : mIsNavMeshDirty ? mPFDirty : NULL)
		{
			x += 6;
			const LLRect& buttonRect(pf_icon->getRect());
			r.setOriginAndSize(x, y, buttonRect.getWidth(), buttonRect.getHeight());
			pf_icon->setRect(r);
			x += buttonRect.getWidth();
		}
	}

	if (parcel && !parcel->isPublic() && LLViewerParcelMgr::getInstance()->canAgentBuyParcel(parcel, false))
	{
		mBuyLand->setVisible(true);
		//HACK: layout tweak until this is all xml
		x += 9;
		const LLRect& buttonRect(mBuyLand->getRect());
		r.setOriginAndSize( x, y, buttonRect.getWidth(), buttonRect.getHeight());
		mBuyLand->setRect(r);
		x += buttonRect.getWidth();
	}
	else mBuyLand->setVisible(false);

	std::string location_name;
	if (region && parcel)
	{
// [RLVa:KB] - Checked: 2009-07-04 (RLVa-1.0.0a) | Modified: RLVa-1.0.0a
		if (RlvActions::hasBehaviour(RLV_BHVR_SHOWLOC))
		{
			location_name = llformat("%s (%s) - %s",
					RlvStrings::getString(RLV_STRING_HIDDEN_REGION).c_str(), region->getSimAccessString().c_str(), 
					RlvStrings::getString(RLV_STRING_HIDDEN).c_str());
		}
		else
// [/RLVa:KB]
		if (!LLAgentUI::buildLocationString(location_name, LLAgentUI::LOCATION_FORMAT_FULL)) 
			location_name = "???";
		else
		{
			const std::string& grid(LFSimFeatureHandler::instance().gridName());
			if (!grid.empty()) location_name += ", " + grid;
		}

		static const LLCachedControl<bool> show_channel("ShowSimChannel");
		if (show_channel && !gLastVersionChannel.empty()) location_name += " - " + gLastVersionChannel;
	}
	else
	{
		// no region
		location_name = "(Unknown)";
	}

	mTextParcelName->setText(location_name);

	// x = right edge
	// loop through: stat graphs, search btn, search text editor, money, buy money, clock
	// adjust rect
	// finally adjust parcel name rect

	S32 new_right = getRect().getWidth();
	static const LLCachedControl<bool> search_visible(gSavedSettings, "ShowSearchBar");
	if (search_visible)
	{
		new_right -= mSearchBtn->getRect().getWidth();
		new_right -= mSearchEditor->getRect().getWidth() + 6;
	}
	else
	{
		r = mStatBtn->getRect();
		r.translate( new_right - r.mRight, 0);
		mStatBtn->setRect(r);
		new_right -= r.getWidth() + 6;
	}

	// Set search bar visibility
	mSearchEditor->setVisible(search_visible);
	mSearchBtn->setVisible(search_visible);
	mSearchBevel->setVisible(search_visible);
	mSGBandwidth->setVisible(! search_visible);
	mSGPacketLoss->setVisible(! search_visible);
	mStatBtn->setEnabled(!search_visible);

	// Set rects of money, buy money, time
	if (mUPCSupported)
	{
		r = mTextUPC->getRect();
		r.translate( new_right - r.mRight, 0);
		mTextUPC->setRect(r);
		new_right -= r.getWidth() - 18;
	}

	r = mTextBalance->getRect();
	r.translate( new_right - r.mRight, 0);
	mTextBalance->setRect(r);
	new_right -= r.getWidth() - 18;

	r = mBuyCurrency->getRect();
	r.translate( new_right - r.mRight, 0);
	mBuyCurrency->setRect(r);
	new_right -= r.getWidth() + 6;

	r = mTextTime->getRect();
	r.translate( new_right - r.mRight, 0);
	mTextTime->setRect(r);

	// Adjust region name and parcel name
	x += 8;

	const S32 PARCEL_RIGHT =  llmin(mTextTime->getRect().mLeft, mTextParcelName->getTextPixelWidth() + x + 5);
	r.set(x+4, getRect().getHeight() - 2, PARCEL_RIGHT, 0);
	mTextParcelName->setRect(r);
}

void LLStatusBar::setVisibleForMouselook(bool visible)
{
	mTextBalance->setVisible(visible);
	if (mUPCSupported)
		mTextUPC->setVisible(visible);
	mTextTime->setVisible(visible);
	mBuyCurrency->setVisible(visible);
	mSearchEditor->setVisible(visible);
	mSearchBtn->setVisible(visible);
	mSGBandwidth->setVisible(visible);
	mSGPacketLoss->setVisible(visible);
	setBackgroundVisible(visible);
}

void LLStatusBar::debitBalance(S32 debit)
{
	setBalance(getBalance() - debit);
}

void LLStatusBar::creditBalance(S32 credit)
{
	setBalance(getBalance() + credit);
}

void LLStatusBar::setBalance(S32 balance)
{
	mTextBalance->setText(gHippoGridManager->getConnectedGrid()->getCurrencySymbol().c_str() +
		LLResMgr::getInstance()->getMonetaryString(balance - mUPC));

	if (mBalance && (fabs((F32)(mBalance - balance)) > gSavedSettings.getF32("UISndMoneyChangeThreshold")))
	{
		if (mBalance > balance)
			make_ui_sound("UISndMoneyChangeDown");
		else
			make_ui_sound("UISndMoneyChangeUp");
	}

	if( balance != mBalance )
	{
		mBalanceTimer->reset(ICON_TIMER_EXPIRY);
		mBalance = balance;
	}
}

void LLStatusBar::setUPC(S32 upc)
{
	mTextUPC->setText("UPC " + LLResMgr::getInstance()->getMonetaryString(upc));

	mUPC = upc;

	setBalance(mBalance);
}

// static
void LLStatusBar::sendMoneyBalanceRequest(bool from_user)
{
	void cmdline_printchat(const std::string& message);
	if (from_user) cmdline_printchat(LLTrans::getString("refreshing balance"));
	LLMessageSystem* msg = gMessageSystem;
	msg->newMessageFast(_PREHASH_MoneyBalanceRequest);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
	msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
	msg->nextBlockFast(_PREHASH_MoneyData);
	msg->addUUIDFast(_PREHASH_TransactionID, LLUUID::null );
	gAgent.sendReliableMessage();
}


void LLStatusBar::setHealth(S32 health)
{
	//LL_INFOS() << "Setting health to: " << buffer << LL_ENDL;
	mTextHealth->setText(llformat("%d%%", health));

	if( mHealth > health )
	{
		if (mHealth > (health + gSavedSettings.getF32("UISndHealthReductionThreshold")))
		{
			if (isAgentAvatarValid())
			{
				if (gAgentAvatarp->getSex() == SEX_FEMALE)
				{
					make_ui_sound("UISndHealthReductionF");
				}
				else
				{
					make_ui_sound("UISndHealthReductionM");
				}
			}
		}

		mHealthTimer->reset(ICON_TIMER_EXPIRY);
	}

	mHealth = health;
}

S32 LLStatusBar::getBalance() const
{
	return mBalance;
}


S32 LLStatusBar::getHealth() const
{
	return mHealth;
}

void LLStatusBar::setLandCredit(S32 credit)
{
	mSquareMetersCredit = credit;
}
void LLStatusBar::setLandCommitted(S32 committed)
{
	mSquareMetersCommitted = committed;
}

BOOL LLStatusBar::isUserTiered() const
{
	return (mSquareMetersCredit > 0);
}

S32 LLStatusBar::getSquareMetersCredit() const
{
	return mSquareMetersCredit;
}

S32 LLStatusBar::getSquareMetersCommitted() const
{
	return mSquareMetersCommitted;
}

S32 LLStatusBar::getSquareMetersLeft() const
{
	return mSquareMetersCredit - mSquareMetersCommitted;
}

static void onClickParcelInfo()
{
	LLViewerParcelMgr::getInstance()->selectParcelAt(gAgent.getPositionGlobal());
	LLFloaterLand::showInstance();
}

static bool rebakeRegionCallback(const LLSD& n, const LLSD& r)
{
	if(!LLNotificationsUtil::getSelectedOption(n, r)) //0 is Yes
	{
		LLMenuOptionPathfindingRebakeNavmesh::getInstance()->sendRequestRebakeNavmesh();
		return true;
	}
	return false;
}

void LLStatusBar::createNavMeshStatusListenerForCurrentRegion()
{
	if (mNavMeshSlot.connected())
	{
		mNavMeshSlot.disconnect();
	}

	LLViewerRegion *currentRegion = gAgent.getRegion();
	if (currentRegion != NULL)
	{
		mNavMeshSlot = LLPathfindingManager::getInstance()->registerNavMeshListenerForRegion(currentRegion, boost::bind(&LLStatusBar::onNavMeshStatusChange, this, _2));
		LLPathfindingManager::getInstance()->requestGetNavMeshForRegion(currentRegion, true);
	}
}

void LLStatusBar::onNavMeshStatusChange(const LLPathfindingNavMeshStatus &pNavMeshStatus)
{
	mIsNavMeshDirty = pNavMeshStatus.isValid() && (pNavMeshStatus.getStatus() != LLPathfindingNavMeshStatus::kComplete);
	refresh();
}

static void onClickScripts()
{
	LLViewerRegion* region = gAgent.getRegion();
	if(region && region->getRegionFlag(REGION_FLAGS_ESTATE_SKIP_SCRIPTS))
	{
		LLNotificationsUtil::add("ScriptsStopped");
	}
	else if(region && region->getRegionFlag(REGION_FLAGS_SKIP_SCRIPTS))
	{
		LLNotificationsUtil::add("ScriptsNotRunning");
	}
	else
	{
		LLNotificationsUtil::add("NoOutsideScripts");
	}
}

// sets the static variables necessary for the date
void LLStatusBar::setupDate()
{
	// fill the day array with what's in the xui
	std::string day_list = getString("StatBarDaysOfWeek");
	size_t length = day_list.size();
	
	// quick input check
	if(length < MAX_DATE_STRING_LENGTH)
	{
		// tokenize it and put it in the array
		std::string cur_word;
		for(size_t i = 0; i < length; ++i)
		{
			if(day_list[i] == ':')
			{
				sDays.push_back(cur_word);
				cur_word.clear();
			}
			else
			{
				cur_word.append(1, day_list[i]);
			}
		}
		sDays.push_back(cur_word);
	}
	
	// fill the day array with what's in the xui	
	std::string month_list = getString( "StatBarMonthsOfYear" );
	length = month_list.size();
	
	// quick input check
	if(length < MAX_DATE_STRING_LENGTH)
	{
		// tokenize it and put it in the array
		std::string cur_word;
		for(size_t i = 0; i < length; ++i)
		{
			if(month_list[i] == ':')
			{
				sMonths.push_back(cur_word);
				cur_word.clear();
			}
			else
			{
				cur_word.append(1, month_list[i]);
			}
		}
		sMonths.push_back(cur_word);
	}
	
	// make sure we have at least 7 days and 12 months
	if(sDays.size() < 7)
	{
		sDays.resize(7);
	}
	
	if(sMonths.size() < 12)
	{
		sMonths.resize(12);
	}
}

static void onClickSearch(const std::string& query)
{
	LLFloaterSearch::SearchQuery search;
	search.query = query;
	LLFloaterSearch::showInstance(search);
}

BOOL can_afford_transaction(S32 cost)
{
	return((cost <= 0)||((gStatusBar) && (gStatusBar->getBalance() >=cost)));
}


// Implements secondlife:///app/balance/request to request a L$ balance
// update via UDP message system. JC
class LLBalanceHandler : public LLCommandHandler
{
public:
	// Requires "trusted" browser/URL source
	LLBalanceHandler() : LLCommandHandler("balance", UNTRUSTED_BLOCK) { }
	bool handle(const LLSD& tokens, const LLSD& query_map, LLMediaCtrl* web)
	{
		if (tokens.size() == 1
			&& tokens[0].asString() == "request")
		{
			LLStatusBar::sendMoneyBalanceRequest(true);
			return true;
		}
		return false;
	}
};
// register with command dispatch system
LLBalanceHandler gBalanceHandler;
