/**
 * @file llpaneldirfind.cpp
 * @brief The "All" panel in the Search directory.
 *
 * $LicenseInfo:firstyear=2001&license=viewergpl$
 * 
 * Copyright (c) 2001-2009, Linden Research, Inc.
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

#include "llpaneldirfind.h"

// linden library includes
#include "llclassifiedflags.h"
#include "llfontgl.h"
#include "llparcel.h"
#include "llqueryflags.h"
#include "message.h"

// viewer project includes
#include "llagent.h"
#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "lllineeditor.h"
#include "llcombobox.h"
#include "llviewercontrol.h"
#include "llmenucommands.h"
#include "llmenugl.h"
#include "llnotificationsutil.h"
#include "llpluginclassmedia.h"
#include "lltextbox.h"
#include "lluiconstants.h"
#include "llviewertexturelist.h"
#include "llviewermessage.h"
#include "llfloateravatarinfo.h"
#include "lldir.h"
#include "llviewercontrol.h"
#include "llviewerregion.h"		// for region name for search urls
#include "lluictrlfactory.h"
#include "llfloaterdirectory.h"
#include "llpaneldirbrowser.h"

#include "hippogridmanager.h"
#include "lfsimfeaturehandler.h"

#if LL_MSVC
// disable boost::lexical_cast warning
#pragma warning (disable:4702)
#endif
#include <boost/tokenizer.hpp>
#include <boost/lexical_cast.hpp>

//---------------------------------------------------------------------------
// LLPanelDirFindAll - Google search appliance based search
//---------------------------------------------------------------------------
namespace
{
	std::string getSearchUrl()
	{
		return LFSimFeatureHandler::instance().searchURL();
	}
	enum SearchType
	{
		SEARCH_ALL_EMPTY,
		SEARCH_ALL_QUERY,
		SEARCH_ALL_TEMPLATE
	};
	std::string getSearchUrl(SearchType ty, bool is_web)
	{
		const std::string mSearchUrl(getSearchUrl());
		if (is_web)
		{
			if (gHippoGridManager->getConnectedGrid()->isSecondLife())
			{
				// Second Life defaults
				if (ty == SEARCH_ALL_EMPTY)
				{
					return gSavedSettings.getString("SearchURLDefault");
				}
				else if (ty == SEARCH_ALL_QUERY)
				{
					return gSavedSettings.getString("SearchURLQuery");
				}
				else if (ty == SEARCH_ALL_TEMPLATE)
				{
					return gSavedSettings.getString("SearchURLSuffix2");
				}
			}
			else if (!mSearchUrl.empty())
			{
				// Search url sent to us in the login response
				if (ty == SEARCH_ALL_EMPTY)
				{
					return mSearchUrl;
				}
				else if (ty == SEARCH_ALL_QUERY)
				{
					return mSearchUrl + "q=[QUERY]&s=[COLLECTION]&";
				}
				else if (ty == SEARCH_ALL_TEMPLATE)
				{
					return "lang=[LANG]&mat=[MATURITY]&t=[TEEN]&region=[REGION]&x=[X]&y=[Y]&z=[Z]&session=[SESSION]&dice=[DICE]";
				}
			}
			else
			{
				// OpenSim and other web search defaults
				if (ty == SEARCH_ALL_EMPTY)
				{
					return gSavedSettings.getString("SearchURLDefaultOpenSim");
				}
				else if (ty == SEARCH_ALL_QUERY)
				{
					return gSavedSettings.getString("SearchURLQueryOpenSim");
				}
				else if (ty == SEARCH_ALL_TEMPLATE)
				{
					return gSavedSettings.getString("SearchURLSuffixOpenSim");
				}
			}
		}
		else
		{
			// Use the old search all
			if (ty == SEARCH_ALL_EMPTY)
			{
				return mSearchUrl + "panel=All&";
			}
			else if (ty == SEARCH_ALL_QUERY)
			{
				return mSearchUrl + "q=[QUERY]&s=[COLLECTION]&";
			}
			else if (ty == SEARCH_ALL_TEMPLATE)
			{
				return "lang=[LANG]&m=[MATURITY]&t=[TEEN]&region=[REGION]&x=[X]&y=[Y]&z=[Z]&session=[SESSION]&dice=[DICE]";
			}
		}
		LL_INFOS() << "Illegal search URL type " << ty << LL_ENDL;
		return "";
	}
}

class LLPanelDirFindAll
:	public LLPanelDirFind
{
public:
	LLPanelDirFindAll(const std::string& name, LLFloaterDirectory* floater);

	/*virtual*/ void reshape(S32 width, S32 height, BOOL called_from_parent);
	/*virtual*/ void search(const std::string& search_text);
};

LLPanelDirFindAll::LLPanelDirFindAll(const std::string& name, LLFloaterDirectory* floater)
:	LLPanelDirFind(name, floater, "find_browser")
{
	LFSimFeatureHandler::getInstance()->setSearchURLCallback(boost::bind(&LLPanelDirFindAll::navigateToDefaultPage, this));
}

//---------------------------------------------------------------------------
// LLPanelDirFind - Base class for all browser-based search tabs
//---------------------------------------------------------------------------

LLPanelDirFind::LLPanelDirFind(const std::string& name, LLFloaterDirectory* floater, const std::string& browser_name)
:	LLPanelDirBrowser(name, floater),
	mWebBrowser(NULL),
	mBrowserName(browser_name)
{
}

BOOL LLPanelDirFind::postBuild()
{
	LLPanelDirBrowser::postBuild();

	getChild<LLButton>("back_btn")->setCommitCallback(boost::bind(&LLPanelDirFind::onClickBack,this));
	if (hasChild("home_btn"))
		getChild<LLButton>("home_btn")->setCommitCallback(boost::bind(&LLPanelDirFind::onClickHome,this));
	getChild<LLButton>("forward_btn")->setCommitCallback(boost::bind(&LLPanelDirFind::onClickForward,this));
	getChild<LLButton>("reload_btn")->setCommitCallback(boost::bind(&LLPanelDirFind::onClickRefresh,this));
	if (hasChild("search_editor"))
		getChild<LLButton>("search_editor")->setCommitCallback(boost::bind(&LLPanelDirFind::onClickSearch,this));
	if (hasChild("search_btn"))
		getChild<LLButton>("search_btn")->setCommitCallback(boost::bind(&LLPanelDirFind::onClickSearch,this));
	if (hasChild("?"))
		getChild<LLButton>("?")->setCommitCallback(boost::bind(&LLPanelDirFind::onClickHelp,this));

	// showcase doesn't have maturity flags -- it's all PG
	if (hasChild("incmature"))
	{
		// Teens don't get mature checkbox
		if (gAgent.wantsPGOnly())
		{
			childSetValue("incmature", FALSE);
			childSetValue("incadult", FALSE);
			childHide("incmature");
			childHide("incadult");
			childSetValue("incpg", TRUE);
			childDisable("incpg");
		}		
		
		if (!gAgent.canAccessMature())
		{
			childSetValue("incmature", FALSE);
			childDisable("incmature");
		}
		
		if (!gAgent.canAccessAdult())
		{
			childSetValue("incadult", FALSE);
			childDisable("incadult");
		}
	}
	
	
	mWebBrowser = getChild<LLMediaCtrl>(mBrowserName);
	if (mWebBrowser)
	{
		mWebBrowser->addObserver(this);

		// need to handle secondlife:///app/ URLs for direct teleports
		mWebBrowser->setTrustedContent( true );

		navigateToDefaultPage();
	}

	if (LLUICtrl* ctrl = findChild<LLUICtrl>("filter_gaming"))
	{
		const LLViewerRegion* region(gAgent.getRegion());
		ctrl->setVisible(region && (region->getGamingFlags() & REGION_GAMING_PRESENT) && !(region->getGamingFlags() & REGION_GAMING_HIDE_FIND_ALL));
	}

	return TRUE;
}

LLPanelDirFind::~LLPanelDirFind()
{
}

// virtual
void LLPanelDirFind::draw()
{
	// enable/disable buttons depending on state
	if ( mWebBrowser )
	{
		bool enable_back = mWebBrowser->canNavigateBack();	
		childSetEnabled( "back_btn", enable_back );

		bool enable_forward = mWebBrowser->canNavigateForward();	
		childSetEnabled( "forward_btn", enable_forward );
		childSetEnabled( "reload_btn", TRUE );
	}

	// showcase doesn't have maturity flags -- it's all PG
	if (hasChild("incmature"))
	{
		updateMaturityCheckbox();
	}

	LLPanelDirBrowser::draw();
}

// When we show any browser-based view, we want to hide all
// the right-side XUI detail panels.
// virtual
void LLPanelDirFind::handleVisibilityChange(BOOL new_visibility)
{
	if (new_visibility)
	{
		mFloaterDirectory->hideAllDetailPanels();
	}
	LLPanel::handleVisibilityChange(new_visibility);
}

// virtual
void LLPanelDirFindAll::reshape(S32 width, S32 height, BOOL called_from_parent = TRUE)
{
	if ( mWebBrowser )
		mWebBrowser->navigateTo( mWebBrowser->getCurrentNavUrl() );

	LLUICtrl::reshape( width, height, called_from_parent );
}

void LLPanelDirFindAll::search(const std::string& search_text)
{
	BOOL inc_pg = childGetValue("incpg").asBoolean();
	BOOL inc_mature = childGetValue("incmature").asBoolean();
	BOOL inc_adult = childGetValue("incadult").asBoolean();
	if (!(inc_pg || inc_mature || inc_adult))
	{
		LLNotificationsUtil::add("NoContentToSearch");
		return;
	}
	
	if (!search_text.empty())
	{
		// Check whether or not we're on the old or web search All -- MC
		bool is_web = false;
		LLPanel* tabs_panel = mFloaterDirectory->getChild<LLTabContainer>("Directory Tabs")->getCurrentPanel();
		if (tabs_panel)
		{
			is_web = tabs_panel->getName() == "find_all_panel";
		}
		else
		{
			LL_WARNS() << "search panel not found! How can this be?!" << LL_ENDL;
		}

		std::string selected_collection = childGetValue( "Category" ).asString();
		std::string url = buildSearchURL(search_text, selected_collection, inc_pg, inc_mature, inc_adult, is_web);
		if (mWebBrowser)
		{
			mWebBrowser->navigateTo(url);
		}
	}
	else
	{
		// empty search text
		navigateToDefaultPage();
	}

	childSetText("search_editor", search_text);
}

void LLPanelDirFind::focus()
{
	if (hasChild("search_editor"))
		childSetFocus("search_editor");
}

void LLPanelDirFind::navigateToDefaultPage()
{
	bool showcase(mBrowserName == "showcase_browser");
	std::string start_url = showcase ? LLWeb::expandURLSubstitutions(LFSimFeatureHandler::instance().destinationGuideURL(), LLSD()) : getSearchUrl();
	bool secondlife(gHippoGridManager->getConnectedGrid()->isSecondLife());
	// Note: we use the web panel in OpenSim as well as Second Life -- MC
	if (start_url.empty() && !secondlife)
	{
		// OS-based but doesn't have its own web search url -- MC
		start_url = gSavedSettings.getString("SearchURLDefaultOpenSim");
	}
	else
	{
		if (!showcase)
		{
			if (secondlife) // Legacy Web Search
				start_url = gSavedSettings.getString("SearchURLDefault");
			else // OS-based but has its own web search url -- MC
				start_url += "panel=" + getName() + "&";

			if (hasChild("incmature"))
			{
				bool inc_pg = getChildView("incpg")->getValue().asBoolean();
				bool inc_mature = getChildView("incmature")->getValue().asBoolean();
				bool inc_adult = getChildView("incadult")->getValue().asBoolean();
				if (!(inc_pg || inc_mature || inc_adult))
				{
					// if nothing's checked, just go for pg; we don't notify in
					// this case because it's a default page.
					inc_pg = true;
				}

				start_url += getSearchURLSuffix(inc_pg, inc_mature, inc_adult, true);
			}
		}
	}

	LL_INFOS() << "default web search url: "  << start_url << LL_ENDL;

	if (mWebBrowser)
	{
		mWebBrowser->navigateTo( start_url );
	}
}

const std::string LLPanelDirFind::buildSearchURL(const std::string& search_text, const std::string& collection,
										   bool inc_pg, bool inc_mature, bool inc_adult, bool is_web) const
{
	std::string url;
	if (search_text.empty()) 
	{
		url = getSearchUrl(SEARCH_ALL_EMPTY, is_web);
	} 
	else 
	{
		// Replace spaces with "+" for use by Google search appliance
		// Yes, this actually works for double-spaces
		// " foo  bar" becomes "+foo++bar" and works fine. JC
		std::string search_text_with_plus = search_text;
		std::string::iterator it = search_text_with_plus.begin();
		for ( ; it != search_text_with_plus.end(); ++it )
		{
			if ( std::isspace( *it ) )
			{
				*it = '+';
			}
		}

		// Our own special set of allowed chars (RFC1738 http://www.ietf.org/rfc/rfc1738.txt)
		// Note that "+" is one of them, so we can do "+" addition first.
		const char* allowed =   
			"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
			"0123456789"
			"-._~$+!*'()";
		std::string query = LLURI::escape(search_text_with_plus, allowed);

		url = getSearchUrl(SEARCH_ALL_QUERY, is_web);
		std::string substring = "[QUERY]";
		std::string::size_type where = url.find(substring);
		if (where != std::string::npos)
		{
			url.replace(where, substring.length(), query);
		}

		// replace the collection name with the one selected from the combo box
		// std::string selected_collection = childGetValue( "Category" ).asString();
		substring = "[COLLECTION]";
		where = url.find(substring);
		if (where != std::string::npos)
		{
			url.replace(where, substring.length(), collection);
		}

	}
	url += getSearchURLSuffix(inc_pg, inc_mature, inc_adult, is_web);
	LL_INFOS() << "web search url " << url << LL_ENDL;
	return url;
}

const std::string LLPanelDirFind::getSearchURLSuffix(bool inc_pg, bool inc_mature, bool inc_adult, bool is_web) const
{
	std::string url = getSearchUrl(SEARCH_ALL_TEMPLATE, is_web);
	LL_INFOS() << "Suffix template " << url << LL_ENDL;

	if (!url.empty())
	{
		// Note: opensim's default template (SearchURLSuffixOpenSim) is currently empty -- MC
		if (gHippoGridManager->getConnectedGrid()->isSecondLife() || !getSearchUrl().empty())
		{
			// if the mature checkbox is unchecked, modify query to remove 
			// terms with given phrase from the result set
			// This builds a value from 1-7 by or-ing together the flags, and then converts
			// it to a string. 
			std::string substring="[MATURITY]";
			S32 maturityFlag = 
				(inc_pg ? SEARCH_PG : SEARCH_NONE) |
				(inc_mature ? SEARCH_MATURE : SEARCH_NONE) |
				(inc_adult ? SEARCH_ADULT : SEARCH_NONE);
			url.replace(url.find(substring), substring.length(), boost::lexical_cast<std::string>(maturityFlag));
			
			// Include region and x/y position, not for the GSA, but
			// just to get logs on the web server for search_proxy.php
			// showing where people were standing when they searched.
			std::string region_name;
			LLViewerRegion* region = gAgent.getRegion();
			if (region)
			{
				region_name = region->getName();
			}
			// take care of spaces in names
			region_name = LLURI::escape(region_name);
			substring = "[REGION]";
			url.replace(url.find(substring), substring.length(), region_name);

			LLVector3 pos_region = gAgent.getPositionAgent();

			std::string x = llformat("%.0f", pos_region.mV[VX]);
			substring = "[X]";
			url.replace(url.find(substring), substring.length(), x);
			std::string y = llformat("%.0f", pos_region.mV[VY]);
			substring = "[Y]";
			url.replace(url.find(substring), substring.length(), y);
			std::string z = llformat("%.0f", pos_region.mV[VZ]);
			substring = "[Z]";
			url.replace(url.find(substring), substring.length(), z);

			LLUUID session_id = gAgent.getSessionID();
			std::string session_string = session_id.getString();
			substring = "[SESSION]";
			url.replace(url.find(substring), substring.length(), session_string);

			// set the currently selected language by asking the pref setting
			std::string language_string = LLUI::getLanguage();
			std::string language_tag = "[LANG]";
			url.replace( url.find( language_tag ), language_tag.length(), language_string );

			// and set the flag for the teen grid
			std::string teen_string = gAgent.isTeen() ? "y" : "n";
			std::string teen_tag = "[TEEN]";
			url.replace( url.find( teen_tag ), teen_tag.length(), teen_string );

			// and set the flag for gaming areas if not on SL Grid.
			if (!gHippoGridManager->getConnectedGrid()->isSecondLife())
			{
				substring = "[DICE]";
				url.replace(url.find(substring), substring.length(), (hasChild("filter_gaming") && childGetValue("filter_gaming").asBoolean()) ? "y" : "n");
			}
		}	
	}
	
	return url;
}

void LLPanelDirFind::onClickBack()
{
	if ( mWebBrowser )
	{
		mWebBrowser->navigateBack();
	}
}

void LLPanelDirFind::onClickHelp()
{
	LLNotificationsUtil::add("ClickSearchHelpAll");
}

void LLPanelDirFind::onClickForward()
{
	if ( mWebBrowser )
	{
		mWebBrowser->navigateForward();
	}
}

void LLPanelDirFind::onClickHome()
{
	if ( mWebBrowser )
	{
		mWebBrowser->navigateHome();
	}
}

void LLPanelDirFind::onClickRefresh()
{
	if ( mWebBrowser )
	{
		mWebBrowser->navigateTo(mWebBrowser->getCurrentNavUrl());
	}
}

void LLPanelDirFind::onClickSearch()
{
	std::string search_text = childGetText("search_editor");
	search(search_text);

	LLFloaterDirectory::sNewSearchCount++;
}

void LLPanelDirFind::handleMediaEvent(LLPluginClassMedia* self, EMediaEvent event)
{
	switch(event)
	{
		case MEDIA_EVENT_NAVIGATE_BEGIN:
			childSetText("status_text", getString("loading_text"));
		break;
		
		case MEDIA_EVENT_NAVIGATE_COMPLETE:
			childSetText("status_text", getString("done_text"));
		break;
		
		case MEDIA_EVENT_LOCATION_CHANGED:
			// Debugging info to console
			LL_INFOS() << self->getLocation() << LL_ENDL;
		break;

		default:
			// Having a default case makes the compiler happy.
		break;
	}
}

//---------------------------------------------------------------------------
// LLPanelDirFindAllInterface
//---------------------------------------------------------------------------

static LLPanelDirFindAll* sFindAll = NULL;
// static
LLPanelDirFindAll* LLPanelDirFindAllInterface::create(LLFloaterDirectory* floater)
{
	return new LLPanelDirFindAll("find_all_panel", floater);
}

static LLPanelDirFindAllOld* sFindAllOld = NULL;
// static
void LLPanelDirFindAllInterface::search(LLFloaterDirectory* inst,
										const LLFloaterSearch::SearchQuery& search, bool show)
{
	bool secondlife(gHippoGridManager->getConnectedGrid()->isSecondLife());
	LLPanelDirFind* find_panel(secondlife ? inst->findChild<LLPanelDirFind>("web_panel") : sFindAll);
	LLPanel* panel(find_panel);
	if (secondlife)
		LLFloaterSearch::search(search, find_panel->mWebBrowser);
	else
	{
		bool has_url(!getSearchUrl().empty());
		if (has_url) find_panel->search(search.query);
		if (sFindAllOld)
		{
			sFindAllOld->search(search.query);
			if (!has_url) panel = sFindAllOld;
		}
	}
	if (show && panel)
	{
		inst->findChild<LLTabContainer>("Directory Tabs")->selectTabPanel(panel);
		panel->setFocus(true);
	}
}

// static
void LLPanelDirFindAllInterface::focus(LLPanelDirFindAll* panel)
{
	panel->focus();
}

//---------------------------------------------------------------------------
// LLPanelDirFindAllOld - deprecated if new Google search works out. JC
//---------------------------------------------------------------------------

LLPanelDirFindAllOld::LLPanelDirFindAllOld(const std::string& name, LLFloaterDirectory* floater)
	:	LLPanelDirBrowser(name, floater)
{
	sFindAllOld = this;
	mMinSearchChars = 3;
}

BOOL LLPanelDirFindAllOld::postBuild()
{
	LLPanelDirBrowser::postBuild();

	getChild<LLLineEditor>("name")->setKeystrokeCallback(boost::bind(&LLPanelDirBrowser::onKeystrokeName,this,_1));

	getChild<LLButton>("Search")->setCommitCallback(boost::bind(&LLPanelDirFindAllOld::onClickSearch,this));
	childDisable("Search");
	setDefaultBtn( "Search" );

	if (LLUICtrl* ctrl = findChild<LLUICtrl>("filter_gaming"))
		ctrl->setVisible(gAgent.getRegion() && (gAgent.getRegion()->getGamingFlags() & REGION_GAMING_PRESENT) && !(gAgent.getRegion()->getGamingFlags() & REGION_GAMING_HIDE_FIND_ALL_CLASSIC));

	return TRUE;
}

LLPanelDirFindAllOld::~LLPanelDirFindAllOld()
{
	sFindAllOld = NULL;
	// Children all cleaned up by default view destructor.
}

// virtual
void LLPanelDirFindAllOld::draw()
{
	updateMaturityCheckbox();
	LLPanelDirBrowser::draw();
}

void LLPanelDirFindAllOld::search(const std::string& query)
{
	getChildView("name")->setValue(query);
	onClickSearch();
}

void LLPanelDirFindAllOld::onClickSearch()
{
	if (childGetValue("name").asString().length() < mMinSearchChars)
	{
		return;
	}

	BOOL inc_pg = childGetValue("incpg").asBoolean();
	BOOL inc_mature = childGetValue("incmature").asBoolean();
	BOOL inc_adult = childGetValue("incadult").asBoolean();
	if (!(inc_pg || inc_mature || inc_adult))
	{
		LLNotificationsUtil::add("NoContentToSearch");
		return;
	}

	setupNewSearch();

	// Figure out scope
	U32 scope = 0x0;
	scope |= DFQ_PEOPLE;	// people (not just online = 0x01 | 0x02)
	// places handled below
	scope |= DFQ_EVENTS;	// events
	scope |= DFQ_GROUPS;	// groups
	if (inc_pg)
	{
		scope |= DFQ_INC_PG;
	}
	if (inc_mature)
	{
		scope |= DFQ_INC_MATURE;
	}
	if (inc_adult)
	{
		scope |= DFQ_INC_ADULT;
	}

	if (hasChild("filter_gaming") && childGetValue("filter_gaming").asBoolean())
	{
		scope |= DFQ_FILTER_GAMING;
	}

	// send the message
	LLMessageSystem *msg = gMessageSystem;
	S32 start_row = 0;
	sendDirFindQuery(msg, mSearchID, childGetValue("name").asString(), scope, start_row);

	// Also look up classified ads. JC 12/2005
	BOOL filter_auto_renew = FALSE;
	U32 classified_flags = pack_classified_flags_request(filter_auto_renew, inc_pg, inc_mature, inc_adult);
	msg->newMessage("DirClassifiedQuery");
	msg->nextBlock("AgentData");
	msg->addUUID("AgentID", gAgent.getID());
	msg->addUUID("SessionID", gAgent.getSessionID());
	msg->nextBlock("QueryData");
	msg->addUUID("QueryID", mSearchID);
	msg->addString("QueryText", childGetValue("name").asString());
	msg->addU32("QueryFlags", classified_flags);
	msg->addU32("Category", 0);	// all categories
	msg->addS32("QueryStart", 0);
	gAgent.sendReliableMessage();

	// Need to use separate find places query because places are
	// sent using the more compact DirPlacesReply message.
	U32 query_flags = DFQ_DWELL_SORT;
	if (inc_pg)
	{
		query_flags |= DFQ_INC_PG;
	}
	if (inc_mature)
	{
		query_flags |= DFQ_INC_MATURE;
	}
	if (inc_adult)
	{
		query_flags |= DFQ_INC_ADULT;
	}
	msg->newMessage("DirPlacesQuery");
	msg->nextBlock("AgentData");
	msg->addUUID("AgentID", gAgent.getID() );
	msg->addUUID("SessionID", gAgent.getSessionID());
	msg->nextBlock("QueryData");
	msg->addUUID("QueryID", mSearchID );
	msg->addString("QueryText", childGetValue("name").asString());
	msg->addU32("QueryFlags", query_flags );
	msg->addS32("QueryStart", 0 ); // Always get the first 100 when using find ALL
	msg->addS8("Category", LLParcel::C_ANY);
	msg->addString("SimName", NULL);
	gAgent.sendReliableMessage();
}
