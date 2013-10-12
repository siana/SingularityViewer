/**
 * @file llflyoutbutton.h
 * @brief LLFlyoutButton base class
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

#ifndef LL_LLFLYOUTBUTTON_H
#define LL_LLFLYOUTBUTTON_H

#include "llcombobox.h"

// Classes

class LLFlyoutButton : public LLComboBox
{
protected:
	LLFlyoutButton(const std::string& name, const LLRect& rect, const std::string& label);
	friend class LLUICtrlFactory;
public:
	virtual void	updateLayout();
	virtual void	draw();
	virtual void	setEnabled(BOOL enabled);

	void setToggleState(BOOL state);

	virtual LLXMLNodePtr getXML(bool save_children = true) const;
	static LLView* fromXML(LLXMLNodePtr node, LLView* parent, LLUICtrlFactory* factory);
	void		onActionButtonClick(const LLSD& data);
	void		onSelectAction(LLUICtrl* ctrl);

protected:
	LLButton*				mActionButton;
	BOOL					mToggleState;
};

#endif // LL_LLFLYOUTBUTTON_H
