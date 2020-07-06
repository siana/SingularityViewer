/** 
 * @file llinventorypanel.h
 * @brief LLInventoryPanel
 * class definition
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

#ifndef LL_LLINVENTORYPANEL_H
#define LL_LLINVENTORYPANEL_H

#include "llassetstorage.h"
#include "llfloater.h"
#include "llinventory.h"
#include "llfolderview.h"
#include "llinventoryfilter.h"
#include "llinventorymodel.h"
#include "llinventoryobserver.h"
#include "llmemberlistener.h"
#include "llcombobox.h"
#include "lluictrlfactory.h"
#include <set>

class LLFolderView;
class LLFolderViewFolder;
class LLFolderViewItem;
class LLInventoryFilter;
class LLInventoryModel;
class LLInvFVBridge;
class LLInventoryFolderViewModelBuilder;
class LLMenuBarGL;
class LLCheckBoxCtrl;
class LLSpinCtrl;
class LLScrollContainer;
class LLTextBox;
class LLIconCtrl;
class LLSaveFolderState;
class LLInvPanelComplObserver;
class LLFolderViewGroupedItemBridge;

class LLInventoryPanel : public LLPanel
{
	//--------------------------------------------------------------------
	// Data
	//--------------------------------------------------------------------
public:
	struct Filter : public LLInitParam::Block<Filter>
	{
		Optional<U32>			sort_order;
		Optional<U32>			types;
		Optional<std::string>	search_string;

		Filter()
		:	sort_order("sort_order"),
			types("types", 0xffffffff),
			search_string("search_string")
		{}
	};

	struct InventoryState : public LLInitParam::Block<InventoryState>
	{
		Mandatory<LLInventoryFilter::Params> filter;
		//Mandatory<LLInventorySort::Params> sort;
	};

	//--------------------------------------------------------------------
	// Initialization
	//--------------------------------------------------------------------
protected:
	friend class LFFloaterInvPanel;
	LLInventoryPanel(const std::string& name,
			const std::string& sort_order_setting,
			const LLSD& start_folder,
			const LLRect& rect,
			LLInventoryModel* inventory,
			BOOL allow_multi_select,
			LLView *parent_view = NULL);
public:
	virtual ~LLInventoryPanel();

public:
	LLInventoryModel* getModel() { return mInventory; }

	BOOL postBuild();

	virtual LLXMLNodePtr getXML(bool save_children = true) const;
	static LLView* fromXML(LLXMLNodePtr node, LLView *parent, LLUICtrlFactory *factory);

	// LLView methods
	void draw();
	/*virtual*/ BOOL handleKeyHere( KEY key, MASK mask );
	BOOL handleHover(S32 x, S32 y, MASK mask);
	BOOL handleDragAndDrop(S32 x, S32 y, MASK mask, BOOL drop,
								   EDragAndDropType cargo_type,
								   void* cargo_data,
								   EAcceptance* accept,
								   std::string& tooltip_msg);
	// LLUICtrl methods
	 /*virtual*/ void onFocusLost();
	 /*virtual*/ void onFocusReceived();

	// Call this method to set the selection.
	void openAllFolders();
	void closeAllFolders();
	void openDefaultFolderForType(LLAssetType::EType);
	void setSelection(const LLUUID& obj_id, BOOL take_keyboard_focus);
	void setSelectCallback(const std::function<void (const std::deque<LLFolderViewItem*>& items, BOOL user_action)>& cb);
	void clearSelection();
	bool isSelectionRemovable();
	LLInventoryFilter& getFilter();
	const LLInventoryFilter& getFilter() const;
	void setFilterTypes(U64 filter, LLInventoryFilter::EFilterType = LLInventoryFilter::FILTERTYPE_OBJECT);
	U32 getFilterObjectTypes() const;
	void setFilterPermMask(PermissionMask filter_perm_mask);
	U32 getFilterPermMask() const;
	void setFilterWearableTypes(U64 filter);
	void setFilterSubString(const std::string& string);
	const std::string getFilterSubString();
	void setFilterWornItems();

	void setSinceLogoff(BOOL sl);
	void setHoursAgo(U32 hours);
	void setDateSearchDirection(U32 direction);
	BOOL getSinceLogoff();
	void setFilterLinks(LLInventoryFilter::EFilterLink filter_links);

	void setShowFolderState(LLInventoryFilter::EFolderShow show);
	LLInventoryFilter::EFolderShow getShowFolderState();
	void setAllowMultiSelect(BOOL allow) { mFolderRoot.get()->setAllowMultiSelect(allow); }
	// This method is called when something has changed about the inventory.
	void modelChanged(U32 mask);
	LLFolderView* getRootFolder() { return mFolderRoot.get(); }
	LLScrollContainer* getScrollableContainer() { return mScroller; }
	bool getAllowDropOnRoot() const { return mAllowDropOnRoot; }
	
	void onSelectionChange(const std::deque<LLFolderViewItem*> &items, BOOL user_action);
	
	LLHandle<LLInventoryPanel> getInventoryPanelHandle() const { return getDerivedHandle<LLInventoryPanel>(); }

	// Callbacks
	void doToSelected(const LLSD& userdata);
	// DEBUG ONLY:
	static void dumpSelectionInformation(void* user_data);

	void openSelected();
	void unSelectAll();
	
	static void onIdle(void* user_data);

	// Find whichever inventory panel is active / on top.
	// "Auto_open" determines if we open an inventory panel if none are open.
	static LLInventoryPanel *getActiveInventoryPanel(BOOL auto_open = TRUE);

public:
	// TomY TODO: Move this elsewhere?
	// helper method which creates an item with a good description,
	// updates the inventory, updates the server, and pushes the
	// inventory update out to other observers.
	void createNewItem(const std::string& name,
					   const LLUUID& parent_id,
					   LLAssetType::EType asset_type,
					   LLInventoryType::EType inv_type,
					   U32 next_owner_perm = 0);

	void addItemID(const LLUUID& id, LLFolderViewItem* itemp);
	void removeItemID(const LLUUID& id);
	LLFolderViewItem* getItemByID(const LLUUID& id);
	LLFolderViewFolder* getFolderByID(const LLUUID& id);
	void setSelectionByID(const LLUUID& obj_id, BOOL take_keyboard_focus);
	void updateSelection();

	// Clean up stuff when the folder root gets deleted
	void clearFolderRoot();

protected:
	void openStartFolderOrMyInventory(); // open the first level of inventory
	void onItemsCompletion();			// called when selected items are complete

	LLUUID						mSelectThisID;	
	LLInventoryModel*			mInventory;
	LLInventoryObserver*		mInventoryObserver;
	LLInvPanelComplObserver*	mCompletionObserver;
	bool 						mAllowMultiSelect;

	LLHandle<LLFolderView>				mFolderRoot;
	LLScrollContainer*	mScroller;

    LLPointer<LLFolderViewGroupedItemBridge> mGroupedItemBridge;
	boost::unordered_map<LLUUID, LLFolderViewItem*> mItemMap;
	/**
	 * Pointer to LLInventoryFolderViewModelBuilder.
	 *
	 * It is set in LLInventoryPanel's constructor and can be overridden in derived classes with 
	 * another implementation.
	 * Take into account it will not be deleted by LLInventoryPanel itself.
	 */
	const LLInventoryFolderViewModelBuilder* mInvFVBridgeBuilder;


	//--------------------------------------------------------------------
	// Sorting
	//--------------------------------------------------------------------
public:
	static const std::string DEFAULT_SORT_ORDER;
	static const std::string RECENTITEMS_SORT_ORDER;
	static const std::string WORNITEMS_SORT_ORDER;
	static const std::string INHERIT_SORT_ORDER;
	
	void setSortOrder(U32 order);
	U32 getSortOrder() const;
	void requestSort();

private:
	LLSD					mStartFolder;
	bool					mShowRootFolder;
	bool					mShowEmptyMessage;
	//bool					mShowItemLinkOverlays;
	bool					mAllowDropOnRoot;
	bool					mAllowWear;
	bool					mUseMarketplaceFolders;
	const std::string			mSortOrderSetting;

	//--------------------------------------------------------------------
	// Hidden folders
	//--------------------------------------------------------------------
public:
	void addHideFolderType(LLFolderType::EType folder_type);

public:
	BOOL 				getIsViewsInitialized() const { return mViewsInitialized; }
	const LLUUID		getRootFolderID() const;
protected:
	// Builds the UI.  Call this once the inventory is usable.
	void 				initializeViews();
	LLFolderViewItem*	rebuildViewsFor(const LLUUID& id); // Given the id and the parent, build all of the folder views.

	virtual void		buildFolderView();
	LLFolderViewItem*	buildNewViews(const LLUUID& id);
	BOOL				getIsHiddenFolderType(LLFolderType::EType folder_type) const;
	
	virtual LLFolderView*		createFolderView(LLInvFVBridge * bridge, bool useLabelSuffix);
	virtual LLFolderViewFolder*	createFolderViewFolder(LLInvFVBridge * bridge);
	virtual LLFolderViewItem*	createFolderViewItem(LLInvFVBridge * bridge);
	BOOL				mViewsInitialized; // Views have been generated
};

class LLPanelMainInventory;

///----------------------------------------------------------------------------
/// Function declarations, constants, enums, and typedefs
///----------------------------------------------------------------------------

// useful functions with the inventory view
// *FIX: move these methods.

void init_inventory_actions(LLPanelMainInventory *floater);
void init_inventory_panel_actions(LLInventoryPanel *panel);

class LLInventoryCategory;
class LLInventoryItem;

// These methods can open items without the inventory being visible
void open_notecard(LLViewerInventoryItem* inv_item, const std::string& title, const LLUUID& object_id, BOOL show_keep_discard, const LLUUID& source_id = LLUUID::null, BOOL take_focus = TRUE);
void open_landmark(LLViewerInventoryItem* inv_item, const std::string& title,                          BOOL show_keep_discard, const LLUUID& source_id = LLUUID::null, BOOL take_focus = TRUE);
void open_texture(const LLUUID& item_id, const std::string& title, BOOL show_keep_discard, const LLUUID& source_id = LLUUID::null, BOOL take_focus = TRUE);

const BOOL TAKE_FOCUS_YES = TRUE;
const BOOL TAKE_FOCUS_NO  = FALSE;

#endif // LL_LLINVENTORYPANEL_H
