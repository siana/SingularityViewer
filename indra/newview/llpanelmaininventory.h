/** 
 * @file llpanelmaininventory.h
 * @brief llpanelmaininventory.h
 * class definition
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

#ifndef LL_LLPANELMAININVENTORY_H
#define LL_LLPANELMAININVENTORY_H

#include "llfloater.h"
#include "llinventoryobserver.h"

#include "llfolderview.h"

class LLFolderViewItem;
class LLInventoryPanel;
class LLSaveFolderState;
class LLFilterEditor;
class LLTabContainer;
class LLMenuButton;
class LLMenuGL;
class LLToggleableMenu;
class LLFloater;
class LLFilterEditor;
class LLComboBox;
class LLFloaterInventoryFinder;

class LLInventoryView : public LLFloater, LLInventoryObserver
{
friend class LLFloaterInventoryFinder;

public:
	LLInventoryView(const std::string& name, const std::string& rect,
			LLInventoryModel* inventory);
	LLInventoryView(const std::string& name, const LLRect& rect,
					LLInventoryModel* inventory);
	~LLInventoryView();

	 BOOL postBuild();
	
//TODO: Move these statics.
	static LLInventoryView* showAgentInventory(BOOL take_keyboard_focus=FALSE);
	static LLInventoryView* getActiveInventory();
	static void toggleVisibility();
	static void toggleVisibility(void*) { toggleVisibility(); }
	// Final cleanup, destroy all open inventory views.
	static void cleanup();


	// LLView & LLFloater functionality
	virtual void onClose(bool app_quitting);
	virtual void setVisible(BOOL visible);

	virtual BOOL handleKeyHere(KEY key, MASK mask);

	// Inherited functionality
	BOOL handleDragAndDrop(S32 x, S32 y, MASK mask, BOOL drop,
		EDragAndDropType cargo_type,
		void* cargo_data,
		EAcceptance* accept,
		std::string& tooltip_msg);
	/*virtual*/ void changed(U32 mask);
	/*virtual*/ void draw();
	

	LLInventoryPanel* getPanel() { return mActivePanel; }
	LLInventoryPanel* getActivePanel() { return mActivePanel; }
	const LLInventoryPanel* getActivePanel() const { return mActivePanel; }

	const std::string& getFilterText() const { return mFilterText; }

	void setSelectCallback(const LLFolderView::signal_t::slot_type& cb);
	void onFilterEdit(const std::string& search_string);
	//
	// Misc functions
	//
	void setFilterTextFromFilter();
	void startSearch();

	void toggleFindOptions();
	void onSelectionChange(LLInventoryPanel *panel, const std::deque<LLFolderViewItem*>& items, BOOL user_action);

	static BOOL filtersVisible(void* user_data);
	void onClearSearch();
	static void onFoldersByName(void *user_data);
	static BOOL checkFoldersByName(void *user_data);
	
	
	void onFilterSelected();
	
	const std::string getFilterSubString();
	void setFilterSubString(const std::string& string);
		


	static void onQuickFilterCommit(LLUICtrl* ctrl, void* user_data);
	static void refreshQuickFilter(LLUICtrl* ctrl);
	
	static void onResetAll(void* userdata);
	static void onExpandAll(void* userdata);
    static void onCollapseAll(void* userdata);
	
	void updateSortControls();

	void resetFilters();
	void updateItemcountText();

// [RLVa:KB] - Checked: 2009-07-10 (RLVa-1.0.0g)
	static void closeAll() 
	{
		// If there are mulitple inventory floaters open then clicking the "Inventory" button will close
		// them one by one (see LLToolBar::onClickInventory() => toggleVisibility() ) until we get to the
		// last one which will just be hidden instead of closed/destroyed (see LLInventoryView::onClose)
		//
		// However the view isn't removed from sActiveViews until its destructor is called and since
		// 'LLMortician::sDestroyImmediate == FALSE' while the viewer is running the destructor won't be 
		// called right away
		//
		// Result: we can't call close() on the last (sActiveViews.count() will still be > 1) because
		//         onClose() would take the wrong branch and destroy() it as well
		//
		// Workaround: "fix" onClose() to count only views that aren't marked as "dead"

		LLInventoryView* pView; U8 flagsSound;
		for (S32 idx = sActiveViews.count() - 1; idx >= 0; idx--)
		{
			pView = sActiveViews.get(idx);
			flagsSound = pView->getSoundFlags();
			pView->setSoundFlags(LLView::SILENT);	// Suppress the window close sound
			pView->close();							// onClose() protects against closing the last inventory floater
			pView->setSoundFlags(flagsSound);		// One view won't be destroy()'ed so it needs its sound flags restored
		}
	}
// [/RLVa:KB]


protected:
	// internal initialization code
	void init(LLInventoryModel* inventory);

protected:
	LLFloaterInventoryFinder* getFinder();
	LLFilterEditor*				mFilterEditor;
	LLComboBox*					mQuickFilterCombo;
	LLTabContainer*				mFilterTabs;
	LLHandle<LLFloater>			mFinderHandle;
	LLInventoryPanel*			mActivePanel;
	bool						mResortActivePanel;
	LLSaveFolderState*			mSavedFolderState;
	std::string					mFilterText;
	//std::string					mFilterSubString;


	// This container is used to hold all active inventory views. This
	// is here to support the inventory toggle show button.
	static LLDynamicArray<LLInventoryView*> sActiveViews;
};


#endif // LL_LLPANELMAININVENTORY_H



