/* 
 * @file llinventorypanel.cpp
 * @brief Implementation of the inventory panel and associated stuff.
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
#include "llinventorypanel.h"

#include <utility> // for std::pair<>

#include "llagent.h"
#include "llagentwearables.h"
#include "llappearancemgr.h"
#include "lluictrlfactory.h"
#include "llfloatercustomize.h"
#include "llfolderview.h"
#include "llimview.h"
#include "llinventorybridge.h"
#include "llinventoryfunctions.h"
#include "llinventorymodelbackgroundfetch.h"
#include "llviewerfoldertype.h"
#include "llvoavatarself.h"
#include "llscrollcontainer.h"
#include "llviewerassettype.h"
#include "llpanelmaininventory.h"
#include "llpanelmarketplaceoutboxinventory.h"

#include "llsdserialize.h"

// [RLVa:KB]
#include "rlvhandler.h"
// [/RLVa:KB]

static LLRegisterWidget<LLInventoryPanel> r("inventory_panel");

const std::string LLInventoryPanel::DEFAULT_SORT_ORDER = std::string("InventorySortOrder");
const std::string LLInventoryPanel::RECENTITEMS_SORT_ORDER = std::string("RecentItemsSortOrder");
const std::string LLInventoryPanel::WORNITEMS_SORT_ORDER = std::string("WornItemsSortOrder");
const std::string LLInventoryPanel::INHERIT_SORT_ORDER = std::string("");
static const LLInventoryFVBridgeBuilder INVENTORY_BRIDGE_BUILDER;


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLInventoryPanelObserver
//
// Bridge to support knowing when the inventory has changed.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

class LLInventoryPanelObserver : public LLInventoryObserver
{
public:
	LLInventoryPanelObserver(LLInventoryPanel* ip) : mIP(ip) {}
	virtual ~LLInventoryPanelObserver() {}
	virtual void changed(U32 mask)
	{
		mIP->modelChanged(mask);
	}
protected:
	LLInventoryPanel* mIP;
};







//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLInvPanelComplObserver
//
// Calls specified callback when all specified items become complete.
//
// Usage:
// observer = new LLInvPanelComplObserver(boost::bind(onComplete));
// inventory->addObserver(observer);
// observer->reset(); // (optional)
// observer->watchItem(incomplete_item1_id);
// observer->watchItem(incomplete_item2_id);
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

class LLInvPanelComplObserver : public LLInventoryCompletionObserver
{
public:
	typedef boost::function<void()> callback_t;

	LLInvPanelComplObserver(callback_t cb)
	:	mCallback(cb)
	{
	}

	void reset();

private:
	/*virtual*/ void done();

	/// Called when all the items are complete.
	callback_t	mCallback;
};

void LLInvPanelComplObserver::reset()
{
	mIncomplete.clear();
	mComplete.clear();
}

void LLInvPanelComplObserver::done()
{
	mCallback();
}

LLInventoryPanel::LLInventoryPanel(const std::string& name,
								    const std::string& sort_order_setting,
									const std::string& start_folder,
									const LLRect& rect,
									LLInventoryModel* inventory,
									BOOL allow_multi_select,
									LLView *parent_view) :
	LLPanel(name, rect, TRUE),
	mInventoryObserver(NULL),
	mCompletionObserver(NULL),
	mFolderRoot(NULL),
	mScroller(NULL),
	mSortOrderSetting(sort_order_setting),
	mStartFolder(start_folder),
	mInventory(inventory),
	mAllowMultiSelect(allow_multi_select),
	mViewsInitialized(false),
	mInvFVBridgeBuilder(NULL)
{
	mInvFVBridgeBuilder = &INVENTORY_BRIDGE_BUILDER;

	setBackgroundColor(gColors.getColor("InventoryBackgroundColor"));
	setBackgroundVisible(TRUE);
	setBackgroundOpaque(TRUE);
}

void LLInventoryPanel::buildFolderView()
{
	// Determine the root folder in case specified, and
	// build the views starting with that folder.

	//std::string start_folder_name(params.start_folder());

	const LLFolderType::EType preferred_type = LLViewerFolderType::lookupTypeFromNewCategoryName(mStartFolder);

	LLUUID root_id;

	if ("LIBRARY" == mStartFolder)
	{
		root_id = gInventory.getLibraryRootFolderID();
	}
	else
	{
		root_id = (preferred_type != LLFolderType::FT_NONE)
				? gInventory.findCategoryUUIDForType(preferred_type, false, false)
				: gInventory.getCategory(static_cast<LLUUID>(mStartFolder)) ? static_cast<LLUUID>(mStartFolder) // Singu Note: if start folder is an id of a folder, use it
				: LLUUID::null;
	}

	if ((root_id == LLUUID::null) && !mStartFolder.empty())
	{
		llwarns << "No category found that matches start_folder: " << mStartFolder << llendl;
		root_id = LLUUID::generateNewID();
	}

	LLInvFVBridge* new_listener = mInvFVBridgeBuilder->createBridge(LLAssetType::AT_CATEGORY,
													LLAssetType::AT_CATEGORY,
													LLInventoryType::IT_CATEGORY,
													this,
													NULL,
													root_id);
													
	mFolderRoot = createFolderView(new_listener, true/*params.use_label_suffix()*/);
}
BOOL LLInventoryPanel::postBuild()
{
	init_inventory_panel_actions(this);

	buildFolderView();
	{
		// scroller
		LLRect scroller_view_rect = getRect();
		scroller_view_rect.translate(-scroller_view_rect.mLeft, -scroller_view_rect.mBottom);
		mScroller = new LLScrollContainer(std::string("Inventory Scroller"),
												   scroller_view_rect,
												  mFolderRoot);
		mScroller->setFollowsAll();
		mScroller->setReserveScrollCorner(TRUE);
		addChild(mScroller);
		mFolderRoot->setScrollContainer(mScroller);
	}

	// Set up the callbacks from the inventory we're viewing, and then build everything.
	mInventoryObserver = new LLInventoryPanelObserver(this);
	mInventory->addObserver(mInventoryObserver);

	mCompletionObserver = new LLInvPanelComplObserver(boost::bind(&LLInventoryPanel::onItemsCompletion, this));
	mInventory->addObserver(mCompletionObserver);

	// Build view of inventory if we need default full hierarchy and inventory ready,
	// otherwise wait for idle callback.
	if (mInventory->isInventoryUsable() && !mViewsInitialized)
	{
		initializeViews();
	}
	gIdleCallbacks.addFunction(onIdle, (void*)this);

	if (mSortOrderSetting != INHERIT_SORT_ORDER)
	{
		setSortOrder(gSavedSettings.getU32(mSortOrderSetting));
	}
	else
	{
		setSortOrder(gSavedSettings.getU32(DEFAULT_SORT_ORDER));
	}
	getFilter()->setFilterEmptySystemFolders();
	return LLPanel::postBuild();
}

LLInventoryPanel::~LLInventoryPanel()
{
	if (mFolderRoot)
	{
		U32 sort_order = mFolderRoot->getSortOrder();
		if (mSortOrderSetting != INHERIT_SORT_ORDER)
		{
			gSavedSettings.setU32(mSortOrderSetting, sort_order);
		}
	}

	gIdleCallbacks.deleteFunction(onIdle, this);

	// LLView destructor will take care of the sub-views.
	mInventory->removeObserver(mInventoryObserver);
	mInventory->removeObserver(mCompletionObserver);
	delete mInventoryObserver;
	delete mCompletionObserver;

	mScroller = NULL;
}

// virtual
LLXMLNodePtr LLInventoryPanel::getXML(bool save_children) const
{
	LLXMLNodePtr node = LLPanel::getXML(false); // Do not print out children

	node->setName(LL_INVENTORY_PANEL_TAG);

		node->createChild("allow_multi_select", TRUE)->setBoolValue(mFolderRoot->getAllowMultiSelect());

	return node;
}

class LLInventoryRecentItemsPanel : public LLInventoryPanel
{
public:
	LLInventoryRecentItemsPanel(const std::string& name,
								    const std::string& sort_order_setting,
								    const std::string& start_folder,
									const LLRect& rect,
									LLInventoryModel* inventory,
									BOOL allow_multi_select,
									LLView *parent_view);

};

LLView* LLInventoryPanel::fromXML(LLXMLNodePtr node, LLView *parent, LLUICtrlFactory *factory)
{
	LLInventoryPanel* panel;

	std::string name("inventory_panel");
	node->getAttributeString("name", name);

	BOOL allow_multi_select = TRUE;
	node->getAttributeBOOL("allow_multi_select", allow_multi_select);

	LLRect rect;
	createRect(node, rect, parent, LLRect());

	std::string sort_order(INHERIT_SORT_ORDER);
	node->getAttributeString("sort_order", sort_order);

	std::string start_folder;
	node->getAttributeString("start_folder", start_folder);

	if(name == "Recent Items")
		panel = new LLInventoryRecentItemsPanel(name, sort_order, start_folder,
								 rect, &gInventory,
								 allow_multi_select, parent);
	else if(name == "panel_outbox_inventory")
		panel = new LLOutboxInventoryPanel(name, sort_order, start_folder,
								 rect, &gInventory,
								 allow_multi_select, parent);
	else
		panel = new LLInventoryPanel(name, sort_order, start_folder,
								 rect, &gInventory,
								 allow_multi_select, parent);

	panel->initFromXML(node, parent);

	panel->postBuild();

	return panel;
}

void LLInventoryPanel::draw()
{
	// Select the desired item (in case it wasn't loaded when the selection was requested)
	mFolderRoot->updateSelection();
	LLPanel::draw();
}

LLInventoryFilter* LLInventoryPanel::getFilter()
{
	if (mFolderRoot)
	{
		return mFolderRoot->getFilter();
	}
	return NULL;
}

const LLInventoryFilter* LLInventoryPanel::getFilter() const
{
	if (mFolderRoot)
	{
		return mFolderRoot->getFilter();
	}
	return NULL;
}

void LLInventoryPanel::setFilterTypes(U64 types, LLInventoryFilter::EFilterType filter_type)
{
	if (filter_type == LLInventoryFilter::FILTERTYPE_OBJECT)
		getFilter()->setFilterObjectTypes(types);
	if (filter_type == LLInventoryFilter::FILTERTYPE_CATEGORY)
		getFilter()->setFilterCategoryTypes(types);
}

U32 LLInventoryPanel::getFilterObjectTypes() const 
{ 
	return mFolderRoot->getFilterObjectTypes(); 
}

U32 LLInventoryPanel::getFilterPermMask() const 
{ 
	return mFolderRoot->getFilterPermissions(); 
}


void LLInventoryPanel::setFilterPermMask(PermissionMask filter_perm_mask)
{
	getFilter()->setFilterPermissions(filter_perm_mask);
}

void LLInventoryPanel::setFilterWearableTypes(U64 types)
{
	getFilter()->setFilterWearableTypes(types);
}

void LLInventoryPanel::setFilterWorn(bool worn)
{
	getFilter()->setFilterWorn(worn);
}

void LLInventoryPanel::setFilterSubString(const std::string& string)
{
	getFilter()->setFilterSubString(string);
}

const std::string LLInventoryPanel::getFilterSubString() 
{ 
	return mFolderRoot->getFilterSubString(); 
}


void LLInventoryPanel::setSortOrder(U32 order)
{
	getFilter()->setSortOrder(order);
	if (getFilter()->isModified())
	{
		mFolderRoot->setSortOrder(order);
		// try to keep selection onscreen, even if it wasn't to start with
		mFolderRoot->scrollToShowSelection();
	}
}

U32 LLInventoryPanel::getSortOrder() const 
{ 
	return mFolderRoot->getSortOrder(); 
}

void LLInventoryPanel::requestSort()
{
	mFolderRoot->requestSort();
}

void LLInventoryPanel::setSinceLogoff(BOOL sl)
{
	getFilter()->setDateRangeLastLogoff(sl);
}

void LLInventoryPanel::setHoursAgo(U32 hours)
{
	getFilter()->setHoursAgo(hours);
}

void LLInventoryPanel::setFilterLinks(U64 filter_links)
{
	getFilter()->setFilterLinks(filter_links);
}

void LLInventoryPanel::setShowFolderState(LLInventoryFilter::EFolderShow show)
{
	getFilter()->setShowFolderState(show);
}

LLInventoryFilter::EFolderShow LLInventoryPanel::getShowFolderState()
{
	return getFilter()->getShowFolderState();
}

void LLInventoryPanel::modelChanged(U32 mask)
{
	static LLFastTimer::DeclareTimer FTM_REFRESH("Inventory Refresh");
	LLFastTimer t2(FTM_REFRESH);

	if (!mViewsInitialized) return;
	
	const LLInventoryModel* model = getModel();
	if (!model) return;

	const LLInventoryModel::changed_items_t& changed_items = model->getChangedIDs();
	if (changed_items.empty()) return;

	for (LLInventoryModel::changed_items_t::const_iterator items_iter = changed_items.begin();
		 items_iter != changed_items.end();
		 ++items_iter)
	{
		const LLUUID& item_id = (*items_iter);
		const LLInventoryObject* model_item = model->getObject(item_id);
		LLFolderViewItem* view_item = mFolderRoot->getItemByID(item_id);

		// LLFolderViewFolder is derived from LLFolderViewItem so dynamic_cast from item
		// to folder is the fast way to get a folder without searching through folders tree.
		LLFolderViewFolder* view_folder = dynamic_cast<LLFolderViewFolder*>(view_item);

		//////////////////////////////
		// LABEL Operation
		// Empty out the display name for relabel.
		if (mask & LLInventoryObserver::LABEL)
		{
			if (view_item)
			{
				// Request refresh on this item (also flags for filtering)
				LLInvFVBridge* bridge = (LLInvFVBridge*)view_item->getListener();
				if(bridge)
				{	// Clear the display name first, so it gets properly re-built during refresh()
					bridge->clearDisplayName();

					view_item->refresh();
				}
				// Singu note: Needed to propagate name change to wearables.
				view_item->nameOrDescriptionChanged();
			}
		}

		//////////////////////////////
		// DESCRIPTION Operation (singu only)
		// Alert listener.
		if ((mask & LLInventoryObserver::DESCRIPTION))
		{
			if (view_item)
			{
				view_item->nameOrDescriptionChanged();
			}
		}

		//////////////////////////////
		// REBUILD Operation
		// Destroy and regenerate the UI.
		if (mask & LLInventoryObserver::REBUILD)
		{
			if (model_item && view_item)
			{
				view_item->destroyView();
			}
			view_item = buildNewViews(item_id);
			view_folder = dynamic_cast<LLFolderViewFolder *>(view_item);
		}

		//////////////////////////////
		// INTERNAL Operation
		// This could be anything.  For now, just refresh the item.
		if (mask & LLInventoryObserver::INTERNAL)
		{
			if (view_item)
			{
				view_item->refresh();
			}
		}

		//////////////////////////////
		// SORT Operation
		// Sort the folder.
		if (mask & LLInventoryObserver::SORT)
		{
			if (view_folder)
			{
				view_folder->requestSort();
			}
		}	

		// We don't typically care which of these masks the item is actually flagged with, since the masks
		// may not be accurate (e.g. in the main inventory panel, I move an item from My Inventory into
		// Landmarks; this is a STRUCTURE change for that panel but is an ADD change for the Landmarks
		// panel).  What's relevant is that the item and UI are probably out of sync and thus need to be
		// resynchronized.
		if (mask & (LLInventoryObserver::STRUCTURE |
					LLInventoryObserver::ADD |
					LLInventoryObserver::REMOVE))
		{
			//////////////////////////////
			// ADD Operation
			// Item exists in memory but a UI element hasn't been created for it.
			if (model_item && !view_item)
			{
				// Add the UI element for this item.
				buildNewViews(item_id);
				// Select any newly created object that has the auto rename at top of folder root set.
				if(mFolderRoot->getRoot()->needsAutoRename())
				{
					setSelection(item_id, FALSE);
				}
			}

			//////////////////////////////
			// STRUCTURE Operation
			// This item already exists in both memory and UI.  It was probably reparented.
			else if (model_item && view_item)
			{
				// Don't process the item if it is the root
				if (view_item->getRoot() != view_item)
				{
					LLFolderViewFolder* new_parent = (LLFolderViewFolder*)mFolderRoot->getItemByID(model_item->getParentUUID());
					// Item has been moved.
					if (view_item->getParentFolder() != new_parent)
					{
						if (new_parent != NULL)
						{
							// Item is to be moved and we found its new parent in the panel's directory, so move the item's UI.
							view_item->getParentFolder()->extractItem(view_item);
							view_item->addToFolder(new_parent, mFolderRoot);
						}
						else
						{
							// Item is to be moved outside the panel's directory (e.g. moved to trash for a panel that
							// doesn't include trash).  Just remove the item's UI.
							view_item->destroyView();
						}
					}
				}
			}

			//////////////////////////////
			// REMOVE Operation
			// This item has been removed from memory, but its associated UI element still exists.
			else if (!model_item && view_item)
			{
				// Remove the item's UI.
				view_item->destroyView();
			}
		}
	}
}

LLFolderView* LLInventoryPanel::getRootFolder() 
{ 
	return mFolderRoot; 
}


// static
void LLInventoryPanel::onIdle(void *userdata)
{
	if (!gInventory.isInventoryUsable())
		return;

	LLInventoryPanel *self = (LLInventoryPanel*)userdata;
	// Inventory just initialized, do complete build
	if (!self->mViewsInitialized)
	{
		self->initializeViews();
	}
	if (self->mViewsInitialized)
	{
		gIdleCallbacks.deleteFunction(onIdle, (void*)self);
	}
}

const LLUUID& LLInventoryPanel::getRootFolderID() const
{
	return mFolderRoot->getListener()->getUUID();
}

void LLInventoryPanel::initializeViews()
{
	if (!gInventory.isInventoryUsable()) return;

	rebuildViewsFor(getRootFolderID());

	mViewsInitialized = true;
	
	openStartFolderOrMyInventory();
	
	// Special case for new user login
	if (gAgent.isFirstLogin())
	{
		// Auto open the user's library
		LLFolderViewFolder* lib_folder = mFolderRoot->getFolderByID(gInventory.getLibraryRootFolderID());
		if (lib_folder)
		{
			lib_folder->setOpen(TRUE);
		}
		
		// Auto close the user's my inventory folder
		LLFolderViewFolder* my_inv_folder = mFolderRoot->getFolderByID(gInventory.getRootFolderID());
		if (my_inv_folder)
		{
			my_inv_folder->setOpenArrangeRecursively(FALSE, LLFolderViewFolder::RECURSE_DOWN);
		}
	}
}
LLFolderViewItem* LLInventoryPanel::rebuildViewsFor(const LLUUID& id)
{
	// Destroy the old view for this ID so we can rebuild it.
	LLFolderViewItem* old_view = mFolderRoot->getItemByID(id);
	if (old_view)
	{
		old_view->destroyView();
	}

	return buildNewViews(id);
}

LLFolderView * LLInventoryPanel::createFolderView(LLInvFVBridge * bridge, bool useLabelSuffix)
{
	LLRect folder_rect(0,
					   0,
					   getRect().getWidth(),
					   0);

	LLFolderView* ret = new LLFolderView(
	getName(), 
	folder_rect, 
	LLUUID::null, 
	this, 
	bridge);
	ret->setAllowMultiSelect(mAllowMultiSelect);
	return ret;
}

LLFolderViewFolder * LLInventoryPanel::createFolderViewFolder(LLInvFVBridge * bridge)
{
	return new LLFolderViewFolder(
		bridge->getDisplayName(),
		bridge->getIcon(),
		bridge->getOpenIcon(),
		LLUI::getUIImage("inv_link_overlay.tga"),
		mFolderRoot,
		bridge);
}

LLFolderViewItem * LLInventoryPanel::createFolderViewItem(LLInvFVBridge * bridge)
{
	return new LLFolderViewItem(
		bridge->getDisplayName(),
		bridge->getIcon(),
		bridge->getOpenIcon(),
		LLUI::getUIImage("inv_link_overlay.tga"),
		bridge->getCreationDate(),
		mFolderRoot,
		bridge);
}

LLFolderViewItem* LLInventoryPanel::buildNewViews(const LLUUID& id)
{
 	LLInventoryObject const* objectp = gInventory.getObject(id);
 	LLUUID root_id = mFolderRoot->getListener()->getUUID();
 	LLFolderViewFolder* parent_folder = NULL;
	LLFolderViewItem* itemp = NULL;
	
 	if (id == root_id)
 	{
 		parent_folder = mFolderRoot;
 	}
 	else if (objectp)
 	{
 		const LLUUID &parent_id = objectp->getParentUUID();
 		parent_folder = (LLFolderViewFolder*)mFolderRoot->getItemByID(parent_id);
  		
  		if (parent_folder)
  		{
  			if (objectp->getType() <= LLAssetType::AT_NONE ||
  				objectp->getType() >= LLAssetType::AT_COUNT)
  			{
  				llwarns << "LLInventoryPanel::buildNewViews called with invalid objectp->mType : "
  						<< ((S32) objectp->getType()) << " name " << objectp->getName() << " UUID " << objectp->getUUID()
  						<< llendl;
  				return NULL;
  			}
  		
  			if ((objectp->getType() == LLAssetType::AT_CATEGORY) &&
  				(objectp->getActualType() != LLAssetType::AT_LINK_FOLDER))
			{
  				LLInvFVBridge* new_listener = mInvFVBridgeBuilder->createBridge(objectp->getType(),
													objectp->getType(),
													LLInventoryType::IT_CATEGORY,
													this,
													mFolderRoot,
													objectp->getUUID());
  				if (new_listener)
  				{
					LLFolderViewFolder* folderp = createFolderViewFolder(new_listener);
					if (folderp)
					{
						folderp->setItemSortOrder(mFolderRoot->getSortOrder());
					}
  					itemp = folderp;
  				}
  			}
  			else
  			{
  				// Build new view for item.
  				LLInventoryItem* item = (LLInventoryItem*)objectp;
  				LLInvFVBridge* new_listener = mInvFVBridgeBuilder->createBridge(item->getType(),
				item->getActualType(),
				item->getInventoryType(),
  																				this,
  																				mFolderRoot,
  																				item->getUUID(),
  																				item->getFlags());
 
  				if (new_listener)
  				{
					itemp = createFolderViewItem(new_listener);
  				}
  			}
 
  			if (itemp)
  			{
  				itemp->addToFolder(parent_folder, mFolderRoot);
   			}
		}
	}

	// If this is a folder, add the children of the folder and recursively add any 
	// child folders.
	if (id.isNull()
		||	(objectp
			&& objectp->getType() == LLAssetType::AT_CATEGORY))
	{
		LLViewerInventoryCategory::cat_array_t* categories;
		LLViewerInventoryItem::item_array_t* items;
		mInventory->lockDirectDescendentArrays(id, categories, items);
		
		if(categories)
		{
			for (LLViewerInventoryCategory::cat_array_t::const_iterator cat_iter = categories->begin();
				 cat_iter != categories->end();
				 ++cat_iter)
			{
				const LLViewerInventoryCategory* cat = (*cat_iter);
				buildNewViews(cat->getUUID());
			}
		}
		
		if(items && parent_folder)
		{
			for (LLViewerInventoryItem::item_array_t::const_iterator item_iter = items->begin();
				 item_iter != items->end();
				 ++item_iter)
			{
				const LLViewerInventoryItem* item = (*item_iter);
				buildNewViews(item->getUUID());
			}
		}
		mInventory->unlockDirectDescendentArrays(id);
	}
	
	return itemp;
}

// bit of a hack to make sure the inventory is open.
void LLInventoryPanel::openStartFolderOrMyInventory()
{
	// Find My Inventory folder and open it up by name
	for (LLView *child = mFolderRoot->getFirstChild(); child; child = mFolderRoot->findNextSibling(child))
	{
		LLFolderViewFolder *fchild = dynamic_cast<LLFolderViewFolder*>(child);
		if (fchild
			&& fchild->getListener()
				&& fchild->getListener()->getUUID() == gInventory.getRootFolderID())
		{
			fchild->setOpen(TRUE);
			break;
		}
	}
}

struct LLConfirmPurgeData
{
	LLUUID mID;
	LLInventoryModel* mModel;
};

class LLIsNotWorn : public LLInventoryCollectFunctor
{
public:
	LLIsNotWorn() {}
	virtual ~LLIsNotWorn() {}
	virtual bool operator()(LLInventoryCategory* cat,
							LLInventoryItem* item)
	{
		return !gAgentWearables.isWearingItem(item->getUUID());
	}
};

class LLOpenFolderByID : public LLFolderViewFunctor
{
public:
	LLOpenFolderByID(const LLUUID& id) : mID(id) {}
	virtual ~LLOpenFolderByID() {}
	virtual void doFolder(LLFolderViewFolder* folder)
		{
			if (folder->getListener() && folder->getListener()->getUUID() == mID) folder->setOpenArrangeRecursively(TRUE, LLFolderViewFolder::RECURSE_UP);
		}
	virtual void doItem(LLFolderViewItem* item) {}
protected:
	const LLUUID& mID;
};

void LLInventoryPanel::onItemsCompletion()
{
	if (mFolderRoot) mFolderRoot->updateMenu();
}

void LLInventoryPanel::openSelected()
{
	LLFolderViewItem* folder_item = mFolderRoot->getCurSelectedItem();
	if(!folder_item) return;
	LLInvFVBridge* bridge = (LLInvFVBridge*)folder_item->getListener();
	if(!bridge) return;
	bridge->openItem();
}

void LLInventoryPanel::unSelectAll()	
{ 
	mFolderRoot->setSelection(NULL, FALSE, FALSE); 
}


BOOL LLInventoryPanel::handleHover(S32 x, S32 y, MASK mask)
{
	BOOL handled = LLView::handleHover(x, y, mask);
	if(handled)
	{
		if (LLInventoryModelBackgroundFetch::instance().folderFetchActive())
		{
			// replace arrow cursor with arrow and hourglass cursor
			getWindow()->setCursor(UI_CURSOR_WORKING);
		}
	}
	else
	{
		getWindow()->setCursor(UI_CURSOR_ARROW);
	}
	return TRUE;
}

BOOL LLInventoryPanel::handleDragAndDrop(S32 x, S32 y, MASK mask, BOOL drop,
								   EDragAndDropType cargo_type,
								   void* cargo_data,
								   EAcceptance* accept,
								   std::string& tooltip_msg)
{
	BOOL handled = FALSE;

	//if (mAcceptsDragAndDrop)
	{
		handled = LLPanel::handleDragAndDrop(x, y, mask, drop, cargo_type, cargo_data, accept, tooltip_msg);

		// If folder view is empty the (x, y) point won't be in its rect
		// so the handler must be called explicitly.
		// but only if was not handled before. See EXT-6746.
		if (!handled && !mFolderRoot->hasVisibleChildren())
		{
			handled = mFolderRoot->handleDragAndDrop(x, y, mask, drop, cargo_type, cargo_data, accept, tooltip_msg);
		}

		if (handled)
		{
			mFolderRoot->setDragAndDropThisFrame();
		}
	}

	return handled;
}

void LLInventoryPanel::onFocusLost()
{
	// inventory no longer handles cut/copy/paste/delete
	if (LLEditMenuHandler::gEditMenuHandler == mFolderRoot)
	{
		LLEditMenuHandler::gEditMenuHandler = NULL;
	}

	LLPanel::onFocusLost();
}

void LLInventoryPanel::onFocusReceived()
{
	// inventory now handles cut/copy/paste/delete
	LLEditMenuHandler::gEditMenuHandler = mFolderRoot;

	LLPanel::onFocusReceived();
}

void LLInventoryPanel::openAllFolders()
{
	mFolderRoot->setOpenArrangeRecursively(TRUE, LLFolderViewFolder::RECURSE_DOWN);
	mFolderRoot->arrangeAll();
}

void LLInventoryPanel::closeAllFolders()
{
	mFolderRoot->setOpenArrangeRecursively(FALSE, LLFolderViewFolder::RECURSE_DOWN);
	mFolderRoot->arrangeAll();
}

void LLInventoryPanel::openDefaultFolderForType(LLAssetType::EType type)
{
	LLUUID category_id = mInventory->findCategoryUUIDForType(LLFolderType::assetTypeToFolderType(type));
	LLOpenFolderByID opener(category_id);
	mFolderRoot->applyFunctorRecursively(opener);
}

void LLInventoryPanel::setSelection(const LLUUID& obj_id, BOOL take_keyboard_focus)
{
	// Don't select objects in COF (e.g. to prevent refocus when items are worn).
	const LLInventoryObject *obj = gInventory.getObject(obj_id);
	if (obj && obj->getParentUUID() == LLAppearanceMgr::instance().getCOF())
	{
		return;
	}
	mFolderRoot->setSelectionByID(obj_id, take_keyboard_focus);
}

void LLInventoryPanel::setSelectCallback(const boost::function<void (const std::deque<LLFolderViewItem*>& items, BOOL user_action)>& cb) 
{ 
	if (mFolderRoot) 
	{
		mFolderRoot->setSelectCallback(cb);
	}
}

void LLInventoryPanel::clearSelection()
{
	mFolderRoot->clearSelection();
}

void LLInventoryPanel::onSelectionChange(const std::deque<LLFolderViewItem*>& items, BOOL user_action)
{
	// Schedule updating the folder view context menu when all selected items become complete (STORM-373).
	mCompletionObserver->reset();
	for (std::deque<LLFolderViewItem*>::const_iterator it = items.begin(); it != items.end(); ++it)
	{
		LLUUID id = (*it)->getListener()->getUUID();
		LLViewerInventoryItem* inv_item = mInventory->getItem(id);

		if (inv_item && !inv_item->isFinished())
		{
			mCompletionObserver->watchItem(id);
		}
	}

	LLFolderView* fv = getRootFolder();
	if (fv->needsAutoRename()) // auto-selecting a new user-created asset and preparing to rename
	{
		fv->setNeedsAutoRename(FALSE);
		if (items.size()) // new asset is visible and selected
		{
			fv->startRenamingSelectedItem();
		}
	}
}

void LLInventoryPanel::createNewItem(const std::string& name,
									const LLUUID& parent_id,
									LLAssetType::EType asset_type,
									LLInventoryType::EType inv_type,
									U32 next_owner_perm)
{
	std::string desc;
	LLViewerAssetType::generateDescriptionFor(asset_type, desc);
	next_owner_perm = (next_owner_perm) ? next_owner_perm : PERM_MOVE | PERM_TRANSFER;

	
	if (inv_type == LLInventoryType::IT_GESTURE)
	{
		LLPointer<LLInventoryCallback> cb =  new LLBoostFuncInventoryCallback(create_gesture_cb);
		create_inventory_item(gAgent.getID(), gAgent.getSessionID(),
							  parent_id, LLTransactionID::tnull, name, desc, asset_type, inv_type,
							  NOT_WEARABLE, next_owner_perm, cb);
	}
	else
	{
		LLPointer<LLInventoryCallback> cb = NULL;
		create_inventory_item(gAgent.getID(), gAgent.getSessionID(),
							  parent_id, LLTransactionID::tnull, name, desc, asset_type, inv_type,
							  NOT_WEARABLE, next_owner_perm, cb);
	}
	
}	

BOOL LLInventoryPanel::getSinceLogoff()
{
	return getFilter()->isSinceLogoff();
}

// DEBUG ONLY
// static 
void LLInventoryPanel::dumpSelectionInformation(void* user_data)
{
	LLInventoryPanel* iv = (LLInventoryPanel*)user_data;
	iv->mFolderRoot->dumpSelectionInformation();
}
// static
LLInventoryPanel* LLInventoryPanel::getActiveInventoryPanel(BOOL auto_open)
{
	LLInventoryPanel* res = NULL;
	LLInventoryView* floater_inventory = LLInventoryView::getActiveInventory();
	if (!floater_inventory)
	{
		llwarns << "Could not find My Inventory floater" << llendl;
		return FALSE;
	}
	res = floater_inventory ? floater_inventory->getActivePanel() : NULL;
	if (res)
	{
		// Make sure the floater is not minimized (STORM-438).
		if (floater_inventory && floater_inventory->isMinimized())
		{
			floater_inventory->setMinimized(FALSE);
		}
	}	
	else if (auto_open)
	{
		floater_inventory->open();

		res = floater_inventory->getActivePanel();
	}
	return res;
}
void LLInventoryPanel::addHideFolderType(LLFolderType::EType folder_type)
{
	getFilter()->setFilterCategoryTypes(getFilter()->getFilterCategoryTypes() & ~(1ULL << folder_type));
}

BOOL LLInventoryPanel::getIsHiddenFolderType(LLFolderType::EType folder_type) const
{
	return !(getFilter()->getFilterCategoryTypes() & (1ULL << folder_type));
}


/************************************************************************/
/* Recent Inventory Panel related class                                 */
/************************************************************************/
class LLInventoryRecentItemsPanel;
static const LLRecentInventoryBridgeBuilder RECENT_ITEMS_BUILDER;


LLInventoryRecentItemsPanel:: LLInventoryRecentItemsPanel(const std::string& name,
						    		const std::string& sort_order_setting,
									const std::string& start_folder,
									const LLRect& rect,
									LLInventoryModel* inventory,
									BOOL allow_multi_select,
									LLView *parent_view) :
									LLInventoryPanel(name, sort_order_setting, start_folder,rect,inventory,allow_multi_select,parent_view)
{
	mInvFVBridgeBuilder = &RECENT_ITEMS_BUILDER;	
}


