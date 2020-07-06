
/** 
 * @file llfolderview.h
 * @brief Definition of the folder view collection of classes.
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

/**
 *
 * The folder view collection of classes provides an interface for
 * making a 'folder view' similar to the way the a single pane file
 * folder interface works.
 *
 */

#ifndef LL_LLFOLDERVIEW_H
#define LL_LLFOLDERVIEW_H

#include "llfolderviewitem.h"	// because LLFolderView is-a LLFolderViewFolder

#include "lluictrl.h"
#include "v4color.h"
#include "stdenums.h"
#include "lldepthstack.h"
#include "lleditmenuhandler.h"
#include "llfontgl.h"
#include "llinventoryfilter.h"
#include "lltooldraganddrop.h"
#include "llviewertexture.h"

#include <boost/unordered_map.hpp>

class LLFolderViewEventListener;
class LLFolderViewGroupedItemModel;
class LLFolderViewFolder;
class LLFolderViewItem;
class LLInventoryModel;
class LLPanel;
class LLLineEditor;
class LLMenuGL;
class LLScrollContainer;
class LLUICtrl;
class LLTextBox;
class LLSaveFolderState;

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLFolderView
//
// The LLFolderView represents the root level folder view object.
// It manages the screen region of the folder view.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

class LLFolderView : public LLFolderViewFolder, public LLEditMenuHandler
{
public:
	typedef void (*SelectCallback)(const std::deque<LLFolderViewItem*> &items, BOOL user_action, void* data);


	LLFolderView( const std::string& name, const LLRect& rect, 
					const LLUUID& source_id, LLPanel *parent_view, LLFolderViewEventListener* listener, LLFolderViewGroupedItemModel* group_model = NULL );

	typedef folder_view_item_deque selected_items_t;

	virtual ~LLFolderView( void );

	virtual BOOL canFocusChildren() const;

	virtual const LLFolderView*	getRoot() const { return this; }
	virtual LLFolderView*	getRoot() { return this; }

	LLFolderViewGroupedItemModel* getFolderViewGroupedItemModel() { return mGroupedItemModel; }
	const LLFolderViewGroupedItemModel* getFolderViewGroupedItemModel() const { return mGroupedItemModel; }

	// FolderViews default to sort by name.  This will change that,
	// and resort the items if necessary.
	void setSortOrder(U32 order);
	void setFilterPermMask(PermissionMask filter_perm_mask);

	typedef boost::signals2::signal<void (const std::deque<LLFolderViewItem*>& items, BOOL user_action)> signal_t;
	void setSelectCallback(const signal_t::slot_type& cb) { mSelectSignal.connect(cb); }
	void setReshapeCallback(const signal_t::slot_type& cb) { mReshapeSignal.connect(cb); }
	void setAllowMultiSelect(BOOL allow) { mAllowMultiSelect = allow; }
	void setShowEmptyMessage(bool show) { mShowEmptyMessage = show; }
	/*void setShowItemLinkOverlays(bool show) { mShowItemLinkOverlays = show; }
	void setAllowDropOnRoot(bool show) { mAllowDropOnRoot = show; }*/

	LLInventoryFilter& getFilter() { return mFilter; }
	const std::string getFilterSubString(BOOL trim = FALSE);
	U32 getFilterObjectTypes() const;
	PermissionMask getFilterPermissions() const;
	// *NOTE: use getFilter().getShowFolderState();
	//LLInventoryFilter::EFolderShow getShowFolderState();
	U32 getSortOrder() const;
	BOOL isFilterModified();

	bool getAllowMultiSelect() { return mAllowMultiSelect; }

	U32 toggleSearchType(std::string toggle);
	U32 getSearchType() const;

	// Close all folders in the view
	void closeAllFolders();
	void openTopLevelFolders();

	virtual void toggleOpen() {};
	virtual void setOpenArrangeRecursively(BOOL openitem, ERecurseType recurse);
	virtual BOOL addFolder( LLFolderViewFolder* folder);

	// Find width and height of this object and its children. Also
	// makes sure that this view and its children are the right size.
	virtual S32 arrange( S32* width, S32* height, S32 filter_generation );

	void arrangeAll() { mArrangeGeneration++; }
	S32 getArrangeGeneration() { return mArrangeGeneration; }

	// applies filters to control visibility of items
	virtual void filter( LLInventoryFilter& filter);

	// Get the last selected item
	virtual LLFolderViewItem* getCurSelectedItem( void );
	selected_items_t& getSelectedItems( void );

	// Record the selected item and pass it down the hierarchy.
	virtual BOOL setSelection(LLFolderViewItem* selection, BOOL openitem,
		BOOL take_keyboard_focus = TRUE);

	// Used by menu callbacks
	void setSelectionByID(const LLUUID& obj_id, BOOL take_keyboard_focus);

	// Called once a frame to update the selection if mSelectThisID has been set
	void updateSelection();

	// This method is used to toggle the selection of an item. Walks
	// children, and keeps track of selected objects.
	virtual BOOL changeSelection(LLFolderViewItem* selection, BOOL selected);

	virtual uuid_set_t getSelectionList() const;

	// Make sure if ancestor is selected, descendants are not
	void sanitizeSelection();
	virtual void clearSelection();
	void addToSelectionList(LLFolderViewItem* item);
	void removeFromSelectionList(LLFolderViewItem* item);

	bool startDrag(LLToolDragAndDrop::ESource source);
	void setDragAndDropThisFrame() { mDragAndDropThisFrame = TRUE; }
	void setDraggingOverItem(LLFolderViewItem* item) { mDraggingOverItem = item; }
	LLFolderViewItem* getDraggingOverItem() { return mDraggingOverItem; }

	// Deletion functionality
 	void removeSelectedItems();
	static void removeCutItems();

	// Open the selected item
	void openSelectedItems( void );
	void propertiesSelectedItems( void );

	// Change the folder type
	void changeType(LLInventoryModel *model, LLFolderType::EType new_folder_type);

	void autoOpenItem(LLFolderViewFolder* item);
	void closeAutoOpenedFolders();
	BOOL autoOpenTest(LLFolderViewFolder* item);
	BOOL isOpen() const { return TRUE; } // root folder always open

	// Copy & paste
	virtual BOOL	canCopy() const;
	virtual void	copy() const override final;

	virtual BOOL	canCut() const;
	virtual void	cut();

	virtual BOOL	canPaste() const;
	virtual void	paste();

	virtual BOOL	canDoDelete() const;
	virtual void	doDelete();

	// Public rename functionality - can only start the process
	void startRenamingSelectedItem( void );

	// LLView functionality
	///*virtual*/ BOOL handleKey( KEY key, MASK mask, BOOL called_from_parent );
	/*virtual*/ BOOL handleKeyHere( KEY key, MASK mask );
	/*virtual*/ BOOL handleUnicodeCharHere(llwchar uni_char);
	/*virtual*/ BOOL handleMouseDown( S32 x, S32 y, MASK mask );
	/*virtual*/ BOOL handleDoubleClick( S32 x, S32 y, MASK mask );
	/*virtual*/ BOOL handleRightMouseDown( S32 x, S32 y, MASK mask );
	/*virtual*/ BOOL handleHover( S32 x, S32 y, MASK mask );
	/*virtual*/ BOOL handleDragAndDrop(S32 x, S32 y, MASK mask, BOOL drop,
								   EDragAndDropType cargo_type,
								   void* cargo_data,
								   EAcceptance* accept,
								   std::string& tooltip_msg);
	/*virtual*/ void reshape(S32 width, S32 height, BOOL called_from_parent = TRUE);
	/*virtual*/ void onMouseLeave(S32 x, S32 y, MASK mask) { setShowSelectionContext(FALSE); }
	virtual BOOL handleScrollWheel(S32 x, S32 y, S32 clicks);
	virtual void draw();
	virtual void deleteAllChildren();

	void scrollToShowSelection();
	void scrollToShowItem(LLFolderViewItem* item, const LLRect& constraint_rect);
	void setScrollContainer(LLScrollContainer* parent);
	LLRect getVisibleRect();

	BOOL search(LLFolderViewItem* first_item, const std::string &search_string, BOOL backward);
	void setShowSelectionContext(bool show) { mShowSelectionContext = show; }
	BOOL getShowSelectionContext();
	void setShowSingleSelection(bool show);
	BOOL getShowSingleSelection() { return mShowSingleSelection; }
	F32  getSelectionFadeElapsedTime() { return mMultiSelectionFadeTimer.getElapsedTimeF32(); }
	bool getUseEllipses() { return mUseEllipses; }
	S32 getSelectedCount() { return (S32)mSelectedItems.size(); }

	void addItemID(const LLUUID& id, LLFolderViewItem* itemp);
	void removeItemID(const LLUUID& id);
	LLFolderViewItem* getItemByID(const LLUUID& id);
	LLFolderViewFolder* getFolderByID(const LLUUID& id);

	void	doIdle();						// Real idle routine
	static void idle(void* user_data);		// static glue to doIdle()

	BOOL needsAutoSelect() { return mNeedsAutoSelect && !mAutoSelectOverride; }
	BOOL needsAutoRename() { return mNeedsAutoRename; }
	void setNeedsAutoRename(BOOL val) { mNeedsAutoRename = val; }
	void setPinningSelectedItem(BOOL val) { mPinningSelectedItem = val; }
	void setAutoSelectOverride(BOOL val) { mAutoSelectOverride = val; }
	bool getAutoSelectOverride() const { return mAutoSelectOverride; }

	BOOL getDebugFilters() { return mDebugFilters; }

	LLPanel* getParentPanel() { return mParentPanel.get(); }
	// DEBUG only
	void dumpSelectionInformation();

	virtual S32	notify(const LLSD& info) ;
	
	bool useLabelSuffix() { return mUseLabelSuffix; }
	virtual void updateMenu();
	void saveFolderState();
	void restoreFolderState();

	// Note: We may eventually have to move that method up the hierarchy to LLFolderViewItem.
	LLHandle<LLFolderView>	getHandle() const { return getDerivedHandle<LLFolderView>(); }

private:
	void updateMenuOptions(LLMenuGL* menu);
	void updateRenamerPosition();

protected:
	LLScrollContainer* mScrollContainer;  // NULL if this is not a child of a scroll container.

	void commitRename( );
	void onRenamerLost();

	void finishRenamingItem( void );
	void closeRenamer( void );

	bool selectFirstItem();
	bool selectLastItem();
	
	BOOL addNoOptions(LLMenuGL* menu) const;

private:
	std::unique_ptr<LLSaveFolderState> mSavedFolderState;
protected:
	LLHandle<LLView>					mPopupMenuHandle;
	
	selected_items_t				mSelectedItems;
	bool							mKeyboardSelection,
									mAllowMultiSelect,
									mShowEmptyMessage,
									mShowFolderHierarchy,
									mNeedsScroll,
									mPinningSelectedItem,
									mNeedsAutoSelect,
									mAutoSelectOverride,
									mNeedsAutoRename,
									mUseLabelSuffix,
									mDragAndDropThisFrame,
									mShowSelectionContext,
									mShowSingleSelection;

	LLUUID							mSourceID;

	// Renaming variables and methods
	LLFolderViewItem*				mRenameItem;  // The item currently being renamed
	LLLineEditor*					mRenamer;

	LLRect							mScrollConstraintRect;

	bool							mDebugFilters;
	U32								mSortOrder;
	U32								mSearchType;
	LLDepthStack<LLFolderViewFolder>	mAutoOpenItems;
	LLFolderViewFolder*				mAutoOpenCandidate;
	LLFrameTimer					mAutoOpenTimer;
	LLFrameTimer					mSearchTimer;
	LLWString						mSearchString;
	LLInventoryFilter				mFilter;
	LLFrameTimer					mMultiSelectionFadeTimer;
	S32								mArrangeGeneration;

	signal_t						mSelectSignal;
	signal_t						mReshapeSignal;
	S32								mSignalSelectCallback;
	S32								mMinWidth;
	S32								mRunningHeight;
	boost::unordered_map<LLUUID, LLFolderViewItem*> mItemMap;
	LLUUID							mSelectThisID; // if non null, select this item
	
	LLHandle<LLPanel>				mParentPanel;

	LLFolderViewGroupedItemModel*	mGroupedItemModel;

	/**
	 * Is used to determine if we need to cut text In LLFolderViewItem to avoid horizontal scroll.
	 * NOTE: For now it's used only to cut LLFolderViewItem::mLabel text for Landmarks in Places Panel.
	 */
	bool							mUseEllipses; // See EXT-719

	/**
	 * Contains item under mouse pointer while dragging
	 */
	LLFolderViewItem*				mDraggingOverItem; // See EXT-719

public:
	static F32 sAutoOpenTime;
	LLTextBox*						mStatusTextBox;

};


// Flags for buildContextMenu()
const U32 SUPPRESS_OPEN_ITEM = 0x1;
const U32 FIRST_SELECTED_ITEM = 0x2;
const U32 ITEM_IN_MULTI_SELECTION = 0x4;

#endif // LL_LLFOLDERVIEW_H
