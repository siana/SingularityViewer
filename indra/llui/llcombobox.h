/** 
 * @file llcombobox.h
 * @brief LLComboBox base class
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

// A control that displays the name of the chosen item, which when clicked
// shows a scrolling box of choices.

#ifndef LL_LLCOMBOBOX_H
#define LL_LLCOMBOBOX_H

#include "llbutton.h"
#include "lluictrl.h"
#include "llctrlselectioninterface.h"
#include "llrect.h"
#include "llscrolllistctrl.h"

// Classes

class LLFontGL;
class LLLineEditor;
class LLViewBorder;

extern S32 LLCOMBOBOX_HEIGHT;
extern S32 LLCOMBOBOX_WIDTH;

class LLComboBox
:	public LLUICtrl, public LLCtrlListInterface
{
public:
	typedef enum e_preferred_position
	{
		ABOVE,
		BELOW
	} EPreferredPosition;

	virtual ~LLComboBox(); 
protected:
	friend class LLFloaterTestImpl;
	friend class LLUICtrlFactory;
	LLComboBox(const std::string& name, const LLRect& rect, const std::string& label, commit_callback_t commit_callback = NULL);
	void	prearrangeList(std::string filter = "");

public:
	// LLView interface

	virtual LLXMLNodePtr getXML(bool save_children = true) const;
	static LLView* fromXML(LLXMLNodePtr node, LLView *parent, LLUICtrlFactory *factory);

	virtual void	draw();
	virtual void	onFocusLost();

	virtual void	setEnabled(BOOL enabled);

	virtual BOOL	handleToolTip(S32 x, S32 y, std::string& msg, LLRect* sticky_rect);
	virtual BOOL	handleKeyHere(KEY key, MASK mask);
	virtual BOOL	handleUnicodeCharHere(llwchar uni_char);

	// LLUICtrl interface
	virtual void	clear();					// select nothing
	virtual void	onCommit();
	virtual BOOL	acceptsTextInput() const		{ return mAllowTextEntry; }
	virtual BOOL	isDirty() const;			// Returns TRUE if the user has modified this control.
	virtual void	resetDirty();				// Clear dirty state

	virtual void	setFocus(BOOL b);

	// Allow prevalidation of text input field
	void			setPrevalidate( BOOL (*func)(const LLWString &) );

	// Selects item by underlying LLSD value, using LLSD::asString() matching.
	// For simple items, this is just the name of the label.
	virtual void	setValue(const LLSD& value );

	// Gets underlying LLSD value for currently selected items.  For simple
	// items, this is just the label.
	virtual LLSD	getValue() const;

	void			setAllowTextEntry(BOOL allow, S32 max_chars = 50, BOOL make_tentative = TRUE);
	void			setTextEntry(const LLStringExplicit& text);
	const std::string	getTextEntry() const;
	void			setFocusText(BOOL b);	// Sets focus to the text input area instead of the list
	BOOL			isTextDirty() const;	// Returns TRUE if the user has modified the text input area
	void			resetTextDirty();		// Resets the dirty flag on the input field

	LLScrollListItem*	add(const std::string& name, EAddPosition pos = ADD_BOTTOM, BOOL enabled = TRUE);	// add item "name" to menu
	LLScrollListItem*	add(const std::string& name, const LLUUID& id, EAddPosition pos = ADD_BOTTOM, BOOL enabled = TRUE);
	LLScrollListItem*	add(const std::string& name, void* userdata, EAddPosition pos = ADD_BOTTOM, BOOL enabled = TRUE);
	LLScrollListItem*	add(const std::string& name, LLSD value, EAddPosition pos = ADD_BOTTOM, BOOL enabled = TRUE);
	LLScrollListItem*	addSeparator(EAddPosition pos = ADD_BOTTOM);
	BOOL			remove( S32 index );	// remove item by index, return TRUE if found and removed
	void			removeall() { clearRows(); }
	bool			itemExists(const std::string& name);

	void			sortByName(BOOL ascending = TRUE); // Sort the entries in the combobox by name

	// Select current item by name using selectItemByLabel.  Returns FALSE if not found.
	BOOL			setSimple(const LLStringExplicit& name);
	// Get name of current item. Returns an empty string if not found.
	const std::string	getSimple() const;
	// Get contents of column x of selected row
	virtual const std::string getSelectedItemLabel(S32 column = 0) const;

	// Sets the label, which doesn't have to exist in the label.
	// This is probably a UI abuse.
	void			setLabel(const LLStringExplicit& name);

	// Updates the combobox label to match the selected list item.
	void			updateLabel();

	BOOL			remove(const std::string& name);	// remove item "name", return TRUE if found and removed
	
	BOOL			setCurrentByIndex( S32 index );
	S32				getCurrentIndex() const;

	virtual void	updateLayout();

	//========================================================================
	LLCtrlSelectionInterface* getSelectionInterface()	{ return (LLCtrlSelectionInterface*)this; };
	LLCtrlListInterface* getListInterface()				{ return (LLCtrlListInterface*)this; };

	// LLCtrlListInterface functions
	// See llscrolllistctrl.h
	virtual S32		getItemCount() const;
	// Overwrites the default column (See LLScrollListCtrl for format)
	virtual void 	addColumn(const LLSD& column, EAddPosition pos = ADD_BOTTOM);
	virtual void 	clearColumns();
	virtual void	setColumnLabel(const std::string& column, const std::string& label);
	virtual LLScrollListItem* addElement(const LLSD& value, EAddPosition pos = ADD_BOTTOM, void* userdata = NULL);
	virtual LLScrollListItem* addSimpleElement(const std::string& value, EAddPosition pos = ADD_BOTTOM, const LLSD& id = LLSD());
	virtual void 	clearRows();
	virtual void 	sortByColumn(const std::string& name, BOOL ascending);

	// LLCtrlSelectionInterface functions
	virtual BOOL	getCanSelect() const				{ return TRUE; }
	virtual BOOL	selectFirstItem()					{ return setCurrentByIndex(0); }
	virtual BOOL	selectNthItem( S32 index )			{ return setCurrentByIndex(index); }
	virtual BOOL	selectItemRange( S32 first, S32 last );
	virtual S32		getFirstSelectedIndex() const		{ return getCurrentIndex(); }
	virtual BOOL	setCurrentByID( const LLUUID& id );
	virtual LLUUID	getCurrentID() const;				// LLUUID::null if no items in menu
	virtual BOOL	setSelectedByValue(const LLSD& value, BOOL selected);
	virtual LLSD	getSelectedValue();
	virtual BOOL	isSelected(const LLSD& value) const;
	virtual BOOL	operateOnSelection(EOperation op);
	virtual BOOL	operateOnAll(EOperation op);

	//========================================================================
	
	void*			getCurrentUserdata();

	void			setPrearrangeCallback( commit_callback_t cb ) { mPrearrangeCallback = cb; }
	void			setTextEntryCallback( commit_callback_t cb ) { mTextEntryCallback = cb; }


	void			setButtonVisible(BOOL visible);

	void			onButtonMouseDown();
	void			onListMouseUp();
	void			onItemSelected(const LLSD& data);
	void			onTextCommit(const LLSD& data);

	void			setSuppressTentative(bool suppress);
	void			setSuppressAutoComplete(bool suppress);

	void			updateSelection();
	virtual void	showList();
	virtual void	hideList();

	virtual void	onTextEntry(LLLineEditor* line_editor);
	
protected:
	LLButton*			mButton;
	LLLineEditor*		mTextEntry;
	LLScrollListCtrl*	mList;
	EPreferredPosition	mListPosition;
	LLPointer<LLUIImage>	mArrowImage;
	std::string			mLabel;
	BOOL				mHasAutocompletedText;
	LLColor4				mListColor;

private:
	BOOL				mAllowTextEntry;
	BOOL				mAllowNewValues;
	S32					mMaxChars;
	BOOL				mTextEntryTentative;
	bool				mSuppressAutoComplete;
	bool				mSuppressTentative;
	commit_callback_t	mPrearrangeCallback;
	commit_callback_t	mTextEntryCallback;
	boost::signals2::connection mTopLostSignalConnection;
	S32                 mLastSelectedIndex;
};

#endif
