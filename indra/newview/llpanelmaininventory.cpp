/** 
 * @file llpanelmaininventory.cpp
 * @brief Implementation of llpanelmaininventory.
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
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

#include "llviewerprecompiledheaders.h"
#include "llpanelmaininventory.h"

#include "llagent.h"
#include "llagentcamera.h"
#include "llfirstuse.h"
#include "llfiltereditor.h"
#include "llinventorybridge.h"
#include "llinventoryfunctions.h"
#include "llinventorymodelbackgroundfetch.h"
#include "llinventorypanel.h"
#include "llpreviewtexture.h"
#include "llresmgr.h"
#include "llscrollcontainer.h"
#include "llsdserialize.h"
#include "llsdparam.h"
#include "llspinctrl.h"
#include "lltooldraganddrop.h"
#include "llviewermenu.h"
#include "llviewertexturelist.h"
#include "llpanelobjectinventory.h"
#include "llappviewer.h"

#include "llradiogroup.h"

#include "rlvhandler.h"

const std::string FILTERS_FILENAME("filters.xml");

std::vector<LLPanelMainInventory*> LLPanelMainInventory::sActiveViews;
const S32 INV_MIN_WIDTH = 240;
const S32 INV_MIN_HEIGHT = 150;
const S32 INV_FINDER_WIDTH = 160;
const S32 INV_FINDER_HEIGHT = 408;

class LLFloaterInventoryFinder : public LLFloater
{
public:
	LLFloaterInventoryFinder(const std::string& name,
						const LLRect& rect,
						LLPanelMainInventory* inventory_view);
	virtual void draw();
	/*virtual*/	BOOL	postBuild();
	virtual void onClose(bool app_quitting);
	void changeFilter(LLInventoryFilter* filter);
	void updateElementsFromFilter();
	BOOL getCheckShowLinks();
	BOOL getCheckShowEmpty();
	BOOL getCheckSinceLogoff();
	U32 getDateSearchDirection();

	void onLinks(const LLSD& val);
	static void onTimeAgo(LLUICtrl*, void *);
	static void onCloseBtn(void* user_data);
	static void selectAllTypes(void* user_data);
	static void selectNoTypes(void* user_data);

protected:
	LLPanelMainInventory*	mPanelMainInventory;
	LLSpinCtrl*			mSpinSinceDays;
	LLSpinCtrl*			mSpinSinceHours;
	LLUICtrl*			mRadioLinks;
	LLInventoryFilter*	mFilter;
};

///----------------------------------------------------------------------------
/// LLPanelMainInventory
///----------------------------------------------------------------------------
// Default constructor
LLPanelMainInventory::LLPanelMainInventory(const std::string& name,
								 const std::string& rect,
								 LLInventoryModel* inventory) :
	LLFloater(name, rect, std::string("Inventory"), RESIZE_YES,
			  INV_MIN_WIDTH, INV_MIN_HEIGHT, DRAG_ON_TOP,
			  MINIMIZE_NO, CLOSE_YES),
	mFilterEditor(nullptr),
	mFilterTabs(nullptr),
	mActivePanel(nullptr),
	mResortActivePanel(true),
	mFilterText("")
	//LLHandle<LLFloater> mFinderHandle takes care of its own initialization
{
	init(inventory);
}

LLPanelMainInventory::LLPanelMainInventory(const std::string& name,
								 const LLRect& rect,
								 LLInventoryModel* inventory) :
	LLFloater(name, rect, std::string("Inventory"), RESIZE_YES,
			  INV_MIN_WIDTH, INV_MIN_HEIGHT, DRAG_ON_TOP,
			  MINIMIZE_NO, CLOSE_YES),
	mFilterEditor(nullptr),
	mFilterTabs(nullptr),
	mActivePanel(nullptr),
	mResortActivePanel(true),
	mFilterText("")
	//LLHandle<LLFloater> mFinderHandle takes care of its own initialization
{
	init(inventory);
	setRect(rect); // override XML
}


void LLPanelMainInventory::init(LLInventoryModel* inventory)
{
	// Callbacks
	init_inventory_actions(this);

	// Controls
	addBoolControl("Inventory.ShowFilters", FALSE);
	addBoolControl("Inventory.SortByName", FALSE);
	addBoolControl("Inventory.SortByDate", TRUE);
	addBoolControl("Inventory.FoldersAlwaysByName", TRUE);
	addBoolControl("Inventory.SystemFoldersToTop", TRUE);
	updateSortControls();

	addBoolControl("Inventory.SearchName", TRUE);
	addBoolControl("Inventory.SearchDesc", FALSE);
	addBoolControl("Inventory.SearchCreator", FALSE);

	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_inventory.xml", NULL);
}

BOOL LLPanelMainInventory::postBuild()
{
	gInventory.addObserver(this);
	
	mFilterTabs = (LLTabContainer*)getChild<LLTabContainer>("inventory filter tabs");

	// Set up the default inv. panel/filter settings.
	mActivePanel = getChild<LLInventoryPanel>("All Items");
	if (mActivePanel)
	{
		// "All Items" is the previous only view, so it gets the InventorySortOrder
		mActivePanel->setSortOrder(gSavedSettings.getU32(LLInventoryPanel::DEFAULT_SORT_ORDER));
		mActivePanel->getFilter().markDefault();
		mActivePanel->setSelectCallback(boost::bind(&LLPanelMainInventory::onSelectionChange, this, mActivePanel, _1, _2));
		mResortActivePanel = true;
	}
	LLInventoryPanel* recent_items_panel = getChild<LLInventoryPanel>("Recent Items");
	if (recent_items_panel)
	{
		recent_items_panel->setSinceLogoff(TRUE);
		recent_items_panel->setSortOrder(gSavedSettings.getU32(LLInventoryPanel::RECENTITEMS_SORT_ORDER));
		recent_items_panel->setShowFolderState(LLInventoryFilter::SHOW_NON_EMPTY_FOLDERS);
		LLInventoryFilter& recent_filter = recent_items_panel->getFilter();
		recent_filter.setFilterObjectTypes(recent_filter.getFilterObjectTypes() & ~(0x1 << LLInventoryType::IT_CATEGORY));
		recent_filter.markDefault();
		recent_items_panel->setSelectCallback(boost::bind(&LLPanelMainInventory::onSelectionChange, this, recent_items_panel, _1, _2));
	}
	LLInventoryPanel* worn_items_panel = getChild<LLInventoryPanel>("Worn Items");
	if (worn_items_panel)
	{
		worn_items_panel->setSortOrder(gSavedSettings.getU32(LLInventoryPanel::WORNITEMS_SORT_ORDER));
		worn_items_panel->setShowFolderState(LLInventoryFilter::SHOW_NON_EMPTY_FOLDERS);
		worn_items_panel->getFilter().markDefault();
		worn_items_panel->setFilterWornItems();
		worn_items_panel->setFilterLinks(LLInventoryFilter::FILTERLINK_EXCLUDE_LINKS);
		worn_items_panel->setSelectCallback(boost::bind(&LLPanelMainInventory::onSelectionChange, this, worn_items_panel, _1, _2));
	}

	// Now load the stored settings from disk, if available.
	std::ostringstream filterSaveName;
	filterSaveName << gDirUtilp->getExpandedFilename(LL_PATH_PER_SL_ACCOUNT, FILTERS_FILENAME);
	LL_INFOS() << "LLPanelMainInventory::init: reading from " << filterSaveName.str() << LL_ENDL;
	llifstream file(filterSaveName.str());
	LLSD savedFilterState;
	if (file.is_open())
	{
		LLSDSerialize::fromXML(savedFilterState, file);
		file.close();

		// Load the persistent "Recent Items" settings.
		// Note that the "All Items" settings do not persist.
		if(recent_items_panel)
		{
			if(savedFilterState.has(recent_items_panel->getFilter().getName()))
			{
				LLSD recent_items = savedFilterState.get(
					recent_items_panel->getFilter().getName());
				LLInventoryFilter::Params p;
				LLParamSDParser parser;
				parser.readSD(recent_items, p);
				recent_items_panel->getFilter().fromParams(p);
			}
		}
		if(worn_items_panel)
		{
			if(savedFilterState.has(worn_items_panel->getFilter().getName()))
			{
				LLSD worn_items = savedFilterState.get(
					worn_items_panel->getFilter().getName());
				LLInventoryFilter::Params p;
				LLParamSDParser parser;
				parser.readSD(worn_items, p);
				worn_items_panel->getFilter().fromParams(p);
			}
		}
	}

	mFilterEditor = getChild<LLFilterEditor>("inventory search editor");
	if (mFilterEditor)
	{
		mFilterEditor->setCommitCallback(boost::bind(&LLPanelMainInventory::onFilterEdit, this, _2));
	}

	mQuickFilterCombo = getChild<LLComboBox>("Quick Filter");

	if (mQuickFilterCombo)
	{
		mQuickFilterCombo->setCommitCallback(boost::bind(LLPanelMainInventory::onQuickFilterCommit, _1, this));
	}


	sActiveViews.push_back(this);

	getChild<LLTabContainer>("inventory filter tabs")->setCommitCallback(boost::bind(&LLPanelMainInventory::onFilterSelected,this));

	childSetAction("Inventory.ResetAll",onResetAll,this);
	childSetAction("Inventory.ExpandAll",onExpandAll,this);
	childSetAction("collapse_btn", onCollapseAll, this);
	return TRUE;
}

// Destroys the object
LLPanelMainInventory::~LLPanelMainInventory( void )
{
	// Save the filters state.
	LLSD filterRoot;
	LLInventoryPanel* all_items_panel = getChild<LLInventoryPanel>("All Items");
	if (all_items_panel)
	{
		LLSD filterState;
		LLInventoryPanel::InventoryState p;
		all_items_panel->getFilter().toParams(p.filter);
		if (p.validateBlock(false))
		{
			LLParamSDParser().writeSD(filterState, p);
			filterRoot[all_items_panel->getName()] = filterState;
		}
	}

	LLInventoryPanel* recent_panel = findChild<LLInventoryPanel>("Recent Items");
	if (recent_panel)
	{
		LLSD filterState;
		LLInventoryPanel::InventoryState p;
		recent_panel->getFilter().toParams(p.filter);
		if (p.validateBlock(false))
		{
			LLParamSDParser().writeSD(filterState, p);
			filterRoot[recent_panel->getName()] = filterState;
		}
	}
	
	LLInventoryPanel* worn_panel = findChild<LLInventoryPanel>("Worn Items");
	if (worn_panel)
	{
		LLSD filterState;
		LLInventoryPanel::InventoryState p;
		worn_panel->getFilter().toParams(p.filter);
		if (p.validateBlock(false))
		{
			LLParamSDParser().writeSD(filterState, p);
			filterRoot[worn_panel->getName()] = filterState;
		}
	}

	std::string filterSaveName(gDirUtilp->getExpandedFilename(LL_PATH_PER_SL_ACCOUNT, FILTERS_FILENAME));
	llofstream filtersFile(filterSaveName.c_str());
	if(!LLSDSerialize::toPrettyXML(filterRoot, filtersFile))
	{
		LL_WARNS() << "Could not write to filters save file " << filterSaveName.c_str() << LL_ENDL;
	}
	else
	{
		filtersFile.close();
	}

	vector_replace_with_last(sActiveViews, this);
	gInventory.removeObserver(this);
}

void LLPanelMainInventory::startSearch()
{
	// this forces focus to line editor portion of search editor
	if (mFilterEditor)
	{
		mFilterEditor->focusFirstItem(TRUE);
	}
}

// virtual, from LLView
void LLPanelMainInventory::setVisible( BOOL visible )
{
	gSavedSettings.setBOOL("ShowInventory", visible);
	LLFloater::setVisible(visible);
}

// Destroy all but the last floater, which is made invisible.
void LLPanelMainInventory::onClose(bool app_quitting)
{
//	S32 count = sActiveViews.count();
// [RLVa:KB] - Checked: 2009-07-10 (RLVa-1.0.0g)
	// See LLPanelMainInventory::closeAll() on why we're doing it this way
	S32 count = 0;
	for (S32 idx = 0, cnt = sActiveViews.size(); idx < cnt; idx++)
	{
		if (!sActiveViews.at(idx)->isDead())
			count++;
	}
// [/RLVa:KB]

	if (count > 1)
	{
		destroy();
	}
	else
	{
		if (!app_quitting)
		{
			gSavedSettings.setBOOL("ShowInventory", FALSE);
		}

		// pass up
		LLFloater::setVisible(FALSE);
	}
}

BOOL LLPanelMainInventory::handleKeyHere(KEY key, MASK mask)
{
	LLFolderView* root_folder = mActivePanel ? mActivePanel->getRootFolder() : NULL;
	if (root_folder)
	{
		// first check for user accepting current search results
		if (mFilterEditor 
			&& mFilterEditor->hasFocus()
		    && (key == KEY_RETURN 
		    	|| key == KEY_DOWN)
		    && mask == MASK_NONE)
		{
			// move focus to inventory proper
			mActivePanel->setFocus(TRUE);
			root_folder->scrollToShowSelection();
			return TRUE;
		}

		if (mActivePanel->hasFocus() && key == KEY_UP)
		{
			startSearch();
		}
	}

	return LLFloater::handleKeyHere(key, mask);

}



// static
// *TODO: remove take_keyboard_focus param
LLPanelMainInventory* LLPanelMainInventory::showAgentInventory(BOOL take_keyboard_focus)
{
	if (gDisconnected) return NULL;

// [RLVa:KB] - Checked: 2009-07-10 (RLVa-1.0.0g)
	if (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWINV)) return NULL;
// [/RLVa:KB]

	LLPanelMainInventory* iv = LLPanelMainInventory::getActiveInventory();
#if 0 && !LL_RELEASE_FOR_DOWNLOAD
	if (sActiveViews.count() == 1)
	{
		delete iv;
		iv = NULL;
	}
#endif
	if(!iv && !gAgentCamera.cameraMouselook())
	{
		// create one.
		iv = new LLPanelMainInventory(std::string("Inventory"),
								 std::string("FloaterInventoryRect"),
								 &gInventory);
		iv->open();
		// keep onscreen
		gFloaterView->adjustToFitScreen(iv, FALSE);

		gSavedSettings.setBOOL("ShowInventory", TRUE);
	}
	if(iv)
	{
		// Make sure it's in front and it makes a noise
		iv->setTitle(std::string("Inventory"));
		iv->open();		/*Flawfinder: ignore*/
	}
	return iv;
}

// static
LLPanelMainInventory* LLPanelMainInventory::getActiveInventory()
{
	LLPanelMainInventory* iv = NULL;
	S32 count = sActiveViews.size();
	if(count > 0)
	{
		iv = sActiveViews.front();
		S32 z_order = gFloaterView->getZOrder(iv);
		S32 z_next = 0;
		LLPanelMainInventory* next_iv = NULL;
		for(S32 i = 1; i < count; ++i)
		{
			next_iv = sActiveViews[i];
			z_next = gFloaterView->getZOrder(next_iv);
			if(z_next < z_order)
			{
				iv = next_iv;
				z_order = z_next;
			}
		}
	}
	return iv;
}

// static
void LLPanelMainInventory::toggleVisibility()
{
	S32 count = sActiveViews.size();
	if (0 == count)
	{
		// We're using the inventory, possibly for the first time.
		LLFirstUse::useInventory();
		showAgentInventory(TRUE);
	}
	else if (1 == count)
	{
		if (sActiveViews.front()->getVisible())
		{
			sActiveViews.front()->close();
			gSavedSettings.setBOOL("ShowInventory", FALSE);
		}
		else
		{
			showAgentInventory(TRUE);
		}
	}
	else
	{
		// With more than one open, we know at least one
		// is visible.

		// Close all the last one spawned.
		sActiveViews.back()->close();
	}
}

// static
void LLPanelMainInventory::cleanup()
{
	S32 count = sActiveViews.size();
	for (S32 i = 0; i < count; i++)
	{
		sActiveViews.at(i)->destroy();
	}
	sActiveViews.clear();
}


void LLPanelMainInventory::updateSortControls()
{
	U32 order = mActivePanel ? mActivePanel->getSortOrder() : gSavedSettings.getU32("InventorySortOrder");
	bool sort_by_date = order & LLInventoryFilter::SO_DATE;
	bool folders_by_name = order & LLInventoryFilter::SO_FOLDERS_BY_NAME;
	bool sys_folders_on_top = order & LLInventoryFilter::SO_SYSTEM_FOLDERS_TO_TOP;

	getControl("Inventory.SortByDate")->setValue(sort_by_date);
	getControl("Inventory.SortByName")->setValue(!sort_by_date);
	getControl("Inventory.FoldersAlwaysByName")->setValue(folders_by_name);
	getControl("Inventory.SystemFoldersToTop")->setValue(sys_folders_on_top);
}

void LLPanelMainInventory::resetFilters()
{
	getActivePanel()->getFilter().resetDefault();
	if (LLFloaterInventoryFinder* finder = getFinder())
	{
		finder->updateElementsFromFilter();
	}

	setFilterTextFromFilter();
}

// static
BOOL LLPanelMainInventory::filtersVisible(void* user_data)
{
	LLPanelMainInventory* self = (LLPanelMainInventory*)user_data;
	if(!self) return FALSE;

	return self->getFinder() != NULL;
}

void LLPanelMainInventory::onFilterEdit(const std::string& search_string )
{
	if (!mActivePanel)
	{
		return;
	}

	/* Singu Note: This is the old behavior, desired by SV-2078, but we'd prefer the new behavior.
	 * I've left this here in case we want to add it back in some way.
	if (search_string.empty())
	{
		mActivePanel->setFilterTypes(0xffffffffffffffffULL);
		if (auto* finder = getFinder())
			LLFloaterInventoryFinder::selectAllTypes(finder);
	}
	 */

	// set new filter string
	// Internally handles saving/restoring folder states.
	mActivePanel->setFilterSubString(search_string);
}

struct FilterEntry : public LLDictionaryEntry
{
	FilterEntry(const std::string &filter_name)	:
		LLDictionaryEntry(filter_name){}
};

class LLFilterDictionary : public LLSingleton<LLFilterDictionary>,
						   public LLDictionary<U32, FilterEntry>
{
public:
	LLFilterDictionary()
	{}
	void init(LLPanelMainInventory *view)
	{
		addEntry(0x1 << LLInventoryType::IT_ANIMATION,		new FilterEntry(view->getString("filter_type_animation")));
		addEntry(0x1 << LLInventoryType::IT_CALLINGCARD,	new FilterEntry(view->getString("filter_type_callingcard")));
		addEntry(0x1 << LLInventoryType::IT_WEARABLE,		new FilterEntry(view->getString("filter_type_wearable")));
		addEntry(0x1 << LLInventoryType::IT_GESTURE,		new FilterEntry(view->getString("filter_type_gesture")));
		addEntry(0x1 << LLInventoryType::IT_LANDMARK,		new FilterEntry(view->getString("filter_type_landmark")));
		addEntry(0x1 << LLInventoryType::IT_NOTECARD,		new FilterEntry(view->getString("filter_type_notecard")));
		addEntry(0x1 << LLInventoryType::IT_OBJECT,			new FilterEntry(view->getString("filter_type_object")));
		addEntry(0x1 << LLInventoryType::IT_LSL,			new FilterEntry(view->getString("filter_type_script")));
		addEntry(0x1 << LLInventoryType::IT_SOUND,			new FilterEntry(view->getString("filter_type_sound")));
		addEntry(0x1 << LLInventoryType::IT_TEXTURE,		new FilterEntry(view->getString("filter_type_texture")));
		addEntry(0x1 << LLInventoryType::IT_SNAPSHOT,		new FilterEntry(view->getString("filter_type_snapshot")));
		addEntry(0xffffffff,								new FilterEntry(view->getString("filter_type_all")));
	}
	virtual U32 notFound() const
	{
		return 0;
	}
};

//static
void LLPanelMainInventory::onQuickFilterCommit(LLUICtrl* ctrl, void* user_data)
{

	LLComboBox* quickfilter = (LLComboBox*)ctrl;

	LLPanelMainInventory* view = (LLPanelMainInventory*)(quickfilter->getParent());
	if (!view->mActivePanel)
	{
		return;
	}

	std::string item_type = quickfilter->getSimple();

	if (view->getString("filter_type_custom") == item_type)
	{
		// When they select custom, show the floater then return
		if( !(view->filtersVisible(view)) )
		{
			view->toggleFindOptions();
		}
		return;
	}
	else 
	{
		if(!LLFilterDictionary::instanceExists())
			LLFilterDictionary::instance().init(view);

		U32 filter_type = LLFilterDictionary::instance().lookup(item_type);
		if(!filter_type)
		{
			LL_WARNS() << "Ignoring unknown filter: " << item_type << LL_ENDL;
			return;
		}
		else
		{
			view->mActivePanel->setFilterTypes( filter_type );

			// Force the filters window to update itself, if it's open.
			if (LLFloaterInventoryFinder* finder = view->getFinder())
				finder->updateElementsFromFilter();
		}
	}
}



//static
void LLPanelMainInventory::refreshQuickFilter(LLUICtrl* ctrl)
{
	LLPanelMainInventory* view = (LLPanelMainInventory*)(ctrl->getParent());
	if (!view->mActivePanel)
	{
		return;
	}

	LLComboBox* quickfilter = view->getChild<LLComboBox>("Quick Filter");
	if (!quickfilter)
	{
		return;
	}

	U32 filter_type = view->mActivePanel->getFilterObjectTypes();

	if(!LLFilterDictionary::instanceExists())
		LLFilterDictionary::instance().init(view);

	// Mask to extract only the bit fields we care about.
	// *TODO: There's probably a cleaner way to construct this mask.
	U32 filter_mask = 0;
	for (LLFilterDictionary::const_iterator_t dictionary_iter =  LLFilterDictionary::instance().map_t::begin(); 
		dictionary_iter != LLFilterDictionary::instance().map_t::end(); dictionary_iter++)
	{
		if(dictionary_iter->first != 0xffffffff)
			filter_mask |= dictionary_iter->first;
	}
 
	filter_type &= filter_mask;

	//LL_INFOS() << "filter_type: " << filter_type << LL_ENDL;
	std::string selection;

	if (filter_type == filter_mask)
	{
		selection = view->getString("filter_type_all");
	}
	else
	{
		if (const FilterEntry *entry = LLFilterDictionary::instance().lookup(filter_type))
			selection = entry->mName;
		else
			selection = view->getString("filter_type_custom");
	}

	// Select the chosen item by label text
	if (!quickfilter->setSimple(selection))
	{
		LL_INFOS() << "The item didn't exist: " << selection << LL_ENDL;
	}
}

void LLPanelMainInventory::onResetAll(void* userdata)
{
	LLPanelMainInventory* self = (LLPanelMainInventory*) userdata;
	self->mActivePanel = (LLInventoryPanel*)self->childGetVisibleTab("inventory filter tabs");

	if (!self->mActivePanel)
	{
		return;
	}
	if (self->mActivePanel && self->mFilterEditor)
	{
		self->mFilterEditor->setText(LLStringUtil::null);
	}
	self->onFilterEdit(LLStringUtil::null);
	self->mActivePanel->setFilterTypes(0xffffffffffffffffULL);
	if (auto* finder = self->getFinder())
		LLFloaterInventoryFinder::selectAllTypes(finder);
	self->mActivePanel->closeAllFolders();
}

//static
void LLPanelMainInventory::onExpandAll(void* userdata)
{
	LLPanelMainInventory* self = (LLPanelMainInventory*) userdata;
	self->mActivePanel = (LLInventoryPanel*)self->childGetVisibleTab("inventory filter tabs");

	if (!self->mActivePanel)
	{
		return;
	}
	self->mActivePanel->openAllFolders();
}


//static
void LLPanelMainInventory::onCollapseAll(void* userdata)
{
	LLPanelMainInventory* self = (LLPanelMainInventory*) userdata;
	self->mActivePanel = (LLInventoryPanel*)self->childGetVisibleTab("inventory filter tabs");

	if (!self->mActivePanel)
	{
		return;
	}
	self->mActivePanel->closeAllFolders();
}

void LLPanelMainInventory::onFilterSelected()
{
	// Find my index
	mActivePanel = (LLInventoryPanel*)getChild<LLTabContainer>("inventory filter tabs")->getCurrentPanel();

	if (!mActivePanel)
	{
		return;
	}

	LLInventoryFilter& filter = mActivePanel->getFilter();
	LLFloaterInventoryFinder *finder = getFinder();
	if (finder)
	{
		finder->changeFilter(&filter);
	}
	if (filter.isActive())
	{
		// If our filter is active we may be the first thing requiring a fetch so we better start it here.
		LLInventoryModelBackgroundFetch::instance().start();
	}
	setFilterTextFromFilter();
	updateSortControls();
}

const std::string LLPanelMainInventory::getFilterSubString() 
{ 
	return mActivePanel->getFilterSubString(); 
}

void LLPanelMainInventory::setFilterSubString(const std::string& string) 
{ 
	mActivePanel->setFilterSubString(string); 
}


BOOL LLPanelMainInventory::handleDragAndDrop(S32 x, S32 y, MASK mask, BOOL drop,
										 EDragAndDropType cargo_type,
										 void* cargo_data,
										 EAcceptance* accept,
										 std::string& tooltip_msg)
{
	// Check to see if we are auto scrolling from the last frame
	LLInventoryPanel* panel = (LLInventoryPanel*)this->getActivePanel();
	BOOL needsToScroll = panel->getScrollableContainer()->autoScroll(x, y);
	if(mFilterTabs)
	{
		if(needsToScroll)
		{
			mFilterTabs->startDragAndDropDelayTimer();
		}
	}
	
	BOOL handled = LLFloater::handleDragAndDrop(x, y, mask, drop, cargo_type, cargo_data, accept, tooltip_msg);

	return handled;
}

void LLPanelMainInventory::changed(U32 mask)
{
	// Singu note: only if there's a change we're interested in.
	if ((mask & (LLInventoryObserver::ADD | LLInventoryObserver::REMOVE)) != 0)
	{
		updateItemcountText();
	}
}

void LLPanelMainInventory::draw()
{
	if (mActivePanel && mFilterEditor)
	{
		mFilterEditor->setText(mActivePanel->getFilterSubString());
	}

	if (mActivePanel && mQuickFilterCombo)
	{
		refreshQuickFilter( mQuickFilterCombo );
	}
	if (mActivePanel && mResortActivePanel)
	{
		// EXP-756: Force resorting of the list the first time we draw the list: 
		// In the case of date sorting, we don't have enough information at initialization time
		// to correctly sort the folders. Later manual resort doesn't do anything as the order value is 
		// set correctly. The workaround is to reset the order to alphabetical (or anything) then to the correct order.
		U32 order = mActivePanel->getSortOrder();
		mActivePanel->setSortOrder(LLInventoryFilter::SO_NAME);
		mActivePanel->setSortOrder(order);
		mResortActivePanel = false;
	}
	updateItemcountText();
	LLFloater::draw();
}

void LLPanelMainInventory::updateItemcountText()
{
	std::ostringstream title;
	title << "Inventory";
 	if (LLInventoryModelBackgroundFetch::instance().folderFetchActive() || LLInventoryModelBackgroundFetch::instance().isEverythingFetched())
	{
		LLLocale locale(LLLocale::USER_LOCALE);
		std::string item_count_string;
		LLResMgr::getInstance()->getIntegerString(item_count_string, gInventory.getItemCount());
		if(LLInventoryModelBackgroundFetch::instance().folderFetchActive())
			title << " (Fetched " << item_count_string << " items...)";
		else
			title << " ("<< item_count_string << " items)";
	}
	title << mFilterText;
	setTitle(title.str());

}
void LLPanelMainInventory::setFilterTextFromFilter() 
{ 
	mFilterText = mActivePanel->getFilter().getFilterText(); 
}

void LLPanelMainInventory::toggleFindOptions()
{
	LLFloater *floater = getFinder();
	if (!floater)
	{
		LLFloaterInventoryFinder * finder = new LLFloaterInventoryFinder(std::string("Inventory Finder"),
										LLRect(getRect().mLeft - INV_FINDER_WIDTH, getRect().mTop, getRect().mLeft, getRect().mTop - INV_FINDER_HEIGHT),
										this);
		mFinderHandle = finder->getHandle();
		finder->open();		/*Flawfinder: ignore*/
		addDependentFloater(mFinderHandle);

		// start background fetch of folders
		LLInventoryModelBackgroundFetch::instance().start();

		mFloaterControls[std::string("Inventory.ShowFilters")]->setValue(TRUE);
	}
	else
	{
		floater->close();

		mFloaterControls[std::string("Inventory.ShowFilters")]->setValue(FALSE);
	}
}

LLFolderView* LLPanelMainInventory::getRootFolder() const
{
	return mActivePanel ? (mActivePanel->getRootFolder()) : NULL;
}

void LLPanelMainInventory::setSelectCallback(const LLFolderView::signal_t::slot_type& cb)
{
	getChild<LLInventoryPanel>("All Items")->setSelectCallback(cb);
	getChild<LLInventoryPanel>("Recent Items")->setSelectCallback(cb);
	getChild<LLInventoryPanel>("Worn Items")->setSelectCallback(cb);
}

void LLPanelMainInventory::onSelectionChange(LLInventoryPanel *panel, const std::deque<LLFolderViewItem*>& items, BOOL user_action)
{
	panel->onSelectionChange(items, user_action);
}

///----------------------------------------------------------------------------
/// LLFloaterInventoryFinder
///----------------------------------------------------------------------------

LLFloaterInventoryFinder* LLPanelMainInventory::getFinder() 
{ 
	return (LLFloaterInventoryFinder*)mFinderHandle.get();
}


LLFloaterInventoryFinder::LLFloaterInventoryFinder(const std::string& name,
						const LLRect& rect,
						LLPanelMainInventory* inventory_view) :
	LLFloater(name, rect, std::string("Filters"), RESIZE_NO,
				INV_FINDER_WIDTH, INV_FINDER_HEIGHT, DRAG_ON_TOP,
				MINIMIZE_NO, CLOSE_YES),
	mPanelMainInventory(inventory_view),
	mFilter(&inventory_view->getPanel()->getFilter())
{

	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_inventory_view_finder.xml");
	updateElementsFromFilter();
}

BOOL LLFloaterInventoryFinder::postBuild()
{
	const LLRect& viewrect = mPanelMainInventory->getRect();
	setRect(LLRect(viewrect.mLeft - getRect().getWidth(), viewrect.mTop, viewrect.mLeft, viewrect.mTop - getRect().getHeight()));

	childSetAction("All", selectAllTypes, this);
	childSetAction("None", selectNoTypes, this);

	mSpinSinceHours = getChild<LLSpinCtrl>("spin_hours_ago");
	childSetCommitCallback("spin_hours_ago", onTimeAgo, this);

	mSpinSinceDays = getChild<LLSpinCtrl>("spin_days_ago");
	childSetCommitCallback("spin_days_ago", onTimeAgo, this);

	mRadioLinks = getChild<LLUICtrl>("radio_links");
	mRadioLinks->setCommitCallback(std::bind(&LLFloaterInventoryFinder::onLinks, this, std::placeholders::_2));

	childSetAction("Close", onCloseBtn, this);

	updateElementsFromFilter();
	return TRUE;
}

void LLFloaterInventoryFinder::onLinks(const LLSD& val)
{
	auto value = val.asInteger();
	mFilter->setFilterLinks(value == 0 ? LLInventoryFilter::FILTERLINK_INCLUDE_LINKS : value == 1 ? LLInventoryFilter::FILTERLINK_EXCLUDE_LINKS : LLInventoryFilter::FILTERLINK_ONLY_LINKS);
}

void LLFloaterInventoryFinder::onTimeAgo(LLUICtrl *ctrl, void *user_data)
{
	LLFloaterInventoryFinder *self = (LLFloaterInventoryFinder *)user_data;
	if (!self) return;
	
	if ( self->mSpinSinceDays->get() ||  self->mSpinSinceHours->get() )
	{
		self->getChild<LLUICtrl>("check_since_logoff")->setValue(false);

		U32 days = (U32)self->mSpinSinceDays->get();
		U32 hours = (U32)self->mSpinSinceHours->get();
		if (hours >= 24)
		{
			// Try to handle both cases of spinner clicking and text input in a sensible fashion as best as possible.
			// There is no way to tell if someone has clicked the spinner to get to 24 or input 24 manually, so in
			// this case add to days.  Any value > 24 means they have input the hours manually, so do not add to the
			// current day value.
			if (24 == hours)  // Got to 24 via spinner clicking or text input of 24
			{
				days = days + hours / 24;
			}
			else	// Text input, so do not add to days
			{ 
				days = hours / 24;
			}
			hours = (U32)hours % 24;
			self->mSpinSinceHours->setFocus(false);
			self->mSpinSinceDays->setFocus(false);
			self->mSpinSinceDays->set((F32)days);
			self->mSpinSinceHours->set((F32)hours);
			self->mSpinSinceHours->setFocus(true);
		}
	}
}

void LLFloaterInventoryFinder::changeFilter(LLInventoryFilter* filter)
{
	mFilter = filter;
	updateElementsFromFilter();
}

void LLFloaterInventoryFinder::updateElementsFromFilter()
{
	if (!mFilter)
		return;

	// Get data needed for filter display
	U32 filter_types = mFilter->getFilterObjectTypes();
	std::string filter_string = mFilter->getFilterSubString();
	LLInventoryFilter::EFilterLink show_links = mFilter->getFilterLinks();
	LLInventoryFilter::EFolderShow show_folders = mFilter->getShowFolderState();
	U32 hours = mFilter->getHoursAgo();
	U32 date_search_direction = mFilter->getDateSearchDirection();

	// update the ui elements
	setTitle(mFilter->getName());

	getChild<LLUICtrl>("check_animation")->setValue((S32) (filter_types & 0x1 << LLInventoryType::IT_ANIMATION));

	getChild<LLUICtrl>("check_calling_card")->setValue((S32) (filter_types & 0x1 << LLInventoryType::IT_CALLINGCARD));
	getChild<LLUICtrl>("check_clothing")->setValue((S32) (filter_types & 0x1 << LLInventoryType::IT_WEARABLE));
	getChild<LLUICtrl>("check_gesture")->setValue((S32) (filter_types & 0x1 << LLInventoryType::IT_GESTURE));
	getChild<LLUICtrl>("check_landmark")->setValue((S32) (filter_types & 0x1 << LLInventoryType::IT_LANDMARK));
	getChild<LLUICtrl>("check_notecard")->setValue((S32) (filter_types & 0x1 << LLInventoryType::IT_NOTECARD));
	getChild<LLUICtrl>("check_object")->setValue((S32) (filter_types & 0x1 << LLInventoryType::IT_OBJECT));
	getChild<LLUICtrl>("check_script")->setValue((S32) (filter_types & 0x1 << LLInventoryType::IT_LSL));
	getChild<LLUICtrl>("check_sound")->setValue((S32) (filter_types & 0x1 << LLInventoryType::IT_SOUND));
	getChild<LLUICtrl>("check_texture")->setValue((S32) (filter_types & 0x1 << LLInventoryType::IT_TEXTURE));
	getChild<LLUICtrl>("check_snapshot")->setValue((S32) (filter_types & 0x1 << LLInventoryType::IT_SNAPSHOT));
	getChild<LLUICtrl>("check_show_links")->setValue(show_links == LLInventoryFilter::FILTERLINK_INCLUDE_LINKS);
	getChild<LLUICtrl>("check_show_empty")->setValue(show_folders == LLInventoryFilter::SHOW_ALL_FOLDERS);
	getChild<LLUICtrl>("check_since_logoff")->setValue(mFilter->isSinceLogoff());
	mSpinSinceHours->set((F32)(hours % 24));
	mSpinSinceDays->set((F32)(hours / 24));
	auto value = mFilter->getFilterLinks();
	mRadioLinks->setValue(value == LLInventoryFilter::FILTERLINK_INCLUDE_LINKS ? 0 : value == LLInventoryFilter::FILTERLINK_EXCLUDE_LINKS ? 1 : 2);
	getChild<LLRadioGroup>("date_search_direction")->setSelectedIndex(date_search_direction);
}

void LLFloaterInventoryFinder::draw()
{
	U64 filter = 0xffffffffffffffffULL;
	BOOL filtered_by_all_types = TRUE;

	if (!getChild<LLUICtrl>("check_animation")->getValue())
	{
		filter &= ~(0x1 << LLInventoryType::IT_ANIMATION);
		filtered_by_all_types = FALSE;
	}


	if (!getChild<LLUICtrl>("check_calling_card")->getValue())
	{
		filter &= ~(0x1 << LLInventoryType::IT_CALLINGCARD);
		filtered_by_all_types = FALSE;
	}

	if (!getChild<LLUICtrl>("check_clothing")->getValue())
	{
		filter &= ~(0x1 << LLInventoryType::IT_WEARABLE);
		filtered_by_all_types = FALSE;
	}

	if (!getChild<LLUICtrl>("check_gesture")->getValue())
	{
		filter &= ~(0x1 << LLInventoryType::IT_GESTURE);
		filtered_by_all_types = FALSE;
	}

	if (!getChild<LLUICtrl>("check_landmark")->getValue())


	{
		filter &= ~(0x1 << LLInventoryType::IT_LANDMARK);
		filtered_by_all_types = FALSE;
	}


	if (!getChild<LLUICtrl>("check_notecard")->getValue())
	{
		filter &= ~(0x1 << LLInventoryType::IT_NOTECARD);
		filtered_by_all_types = FALSE;
	}

	if (!getChild<LLUICtrl>("check_object")->getValue())
	{
		filter &= ~(0x1 << LLInventoryType::IT_OBJECT);
		filter &= ~(0x1 << LLInventoryType::IT_ATTACHMENT);
		filtered_by_all_types = FALSE;
	}

	if (!getChild<LLUICtrl>("check_script")->getValue())
	{
		filter &= ~(0x1 << LLInventoryType::IT_LSL);
		filtered_by_all_types = FALSE;
	}

	if (!getChild<LLUICtrl>("check_sound")->getValue())
	{
		filter &= ~(0x1 << LLInventoryType::IT_SOUND);
		filtered_by_all_types = FALSE;
	}

	if (!getChild<LLUICtrl>("check_texture")->getValue())
	{
		filter &= ~(0x1 << LLInventoryType::IT_TEXTURE);
		filtered_by_all_types = FALSE;
	}

	if (!getChild<LLUICtrl>("check_snapshot")->getValue())
	{
		filter &= ~(0x1 << LLInventoryType::IT_SNAPSHOT);
		filtered_by_all_types = FALSE;
	}

	if (!filtered_by_all_types || (mPanelMainInventory->getPanel()->getFilter().getFilterTypes() & LLInventoryFilter::FILTERTYPE_DATE))
	{
		// don't include folders in filter, unless I've selected everything or filtering by date
		filter &= ~(0x1 << LLInventoryType::IT_CATEGORY);
	}

	// update the panel, panel will update the filter
	mPanelMainInventory->getPanel()->setFilterLinks(getCheckShowLinks() ?
		LLInventoryFilter::FILTERLINK_INCLUDE_LINKS : LLInventoryFilter::FILTERLINK_EXCLUDE_LINKS);

	mPanelMainInventory->getPanel()->setShowFolderState(getCheckShowEmpty() ?
		LLInventoryFilter::SHOW_ALL_FOLDERS : LLInventoryFilter::SHOW_NON_EMPTY_FOLDERS);
	mPanelMainInventory->getPanel()->setFilterTypes(filter);
	if (getCheckSinceLogoff())
	{
		mSpinSinceDays->set(0);
		mSpinSinceHours->set(0);
	}
	U32 days = (U32)mSpinSinceDays->get();
	U32 hours = (U32)mSpinSinceHours->get();
	if (hours >= 24)
	{
		days += hours / 24;
		hours = (U32)hours % 24;
		mSpinSinceDays->set((F32)days);
		mSpinSinceHours->set((F32)hours);
	}
	hours += days * 24;
	mPanelMainInventory->getPanel()->setHoursAgo(hours);
	mPanelMainInventory->getPanel()->setSinceLogoff(getCheckSinceLogoff());
	mPanelMainInventory->setFilterTextFromFilter();
	mPanelMainInventory->getPanel()->setDateSearchDirection(getDateSearchDirection());

	LLFloater::draw();
}

void  LLFloaterInventoryFinder::onClose(bool app_quitting)
{
	if (mPanelMainInventory) mPanelMainInventory->getControl("Inventory.ShowFilters")->setValue(FALSE);
	// If you want to reset the filter on close, do it here.  This functionality was
	// hotly debated - Paulm
#if 0
	if (mPanelMainInventory)
	{
		LLPanelMainInventory::onResetFilter((void *)mPanelMainInventory);
	}
#endif
	destroy();
}

BOOL LLFloaterInventoryFinder::getCheckShowLinks()
{
	return getChild<LLUICtrl>("check_show_links")->getValue();
}

BOOL LLFloaterInventoryFinder::getCheckShowEmpty()
{
	return getChild<LLUICtrl>("check_show_empty")->getValue();
}


BOOL LLFloaterInventoryFinder::getCheckSinceLogoff()
{
	return getChild<LLUICtrl>("check_since_logoff")->getValue();
}

U32 LLFloaterInventoryFinder::getDateSearchDirection()
{
	return getChild<LLRadioGroup>("date_search_direction")->getSelectedIndex();
}

void LLFloaterInventoryFinder::onCloseBtn(void* user_data)
{
	LLFloaterInventoryFinder* finderp = (LLFloaterInventoryFinder*)user_data;
	finderp->close();
}

// static
void LLFloaterInventoryFinder::selectAllTypes(void* user_data)
{
	LLFloaterInventoryFinder* self = (LLFloaterInventoryFinder*)user_data;
	if(!self) return;

	self->getChild<LLUICtrl>("check_animation")->setValue(TRUE);
	self->getChild<LLUICtrl>("check_calling_card")->setValue(TRUE);
	self->getChild<LLUICtrl>("check_clothing")->setValue(TRUE);
	self->getChild<LLUICtrl>("check_gesture")->setValue(TRUE);
	self->getChild<LLUICtrl>("check_landmark")->setValue(TRUE);
	self->getChild<LLUICtrl>("check_notecard")->setValue(TRUE);
	self->getChild<LLUICtrl>("check_object")->setValue(TRUE);
	self->getChild<LLUICtrl>("check_script")->setValue(TRUE);
	self->getChild<LLUICtrl>("check_sound")->setValue(TRUE);
	self->getChild<LLUICtrl>("check_texture")->setValue(TRUE);
	self->getChild<LLUICtrl>("check_snapshot")->setValue(TRUE);
}

//static
void LLFloaterInventoryFinder::selectNoTypes(void* user_data)
{
	LLFloaterInventoryFinder* self = (LLFloaterInventoryFinder*)user_data;
	if(!self) return;

	self->getChild<LLUICtrl>("check_animation")->setValue(FALSE);
	self->getChild<LLUICtrl>("check_calling_card")->setValue(FALSE);
	self->getChild<LLUICtrl>("check_clothing")->setValue(FALSE);
	self->getChild<LLUICtrl>("check_gesture")->setValue(FALSE);
	self->getChild<LLUICtrl>("check_landmark")->setValue(FALSE);
	self->getChild<LLUICtrl>("check_notecard")->setValue(FALSE);
	self->getChild<LLUICtrl>("check_object")->setValue(FALSE);
	self->getChild<LLUICtrl>("check_script")->setValue(FALSE);
	self->getChild<LLUICtrl>("check_sound")->setValue(FALSE);
	self->getChild<LLUICtrl>("check_texture")->setValue(FALSE);
	self->getChild<LLUICtrl>("check_snapshot")->setValue(FALSE);
}
