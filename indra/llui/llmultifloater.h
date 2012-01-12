/** 
 * @file llmultifloater.h
 * @brief LLFloater that hosts other floaters
 *
 * $LicenseInfo:firstyear=2002&license=viewerlgpl$
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

// Floating "windows" within the GL display, like the inventory floater,
// mini-map floater, etc.


#ifndef LL_MULTI_FLOATER_H
#define LL_MULTI_FLOATER_H

#include "llfloater.h"
#include "lltabcontainer.h" // for LLTabContainer::eInsertionPoint

// https://wiki.lindenlab.com/mediawiki/index.php?title=LLMultiFloater&oldid=81376
class LLMultiFloater : public LLFloater
{
public:
	LLMultiFloater();
	LLMultiFloater(LLTabContainer::TabPosition tab_pos);
	LLMultiFloater(const std::string& name);
	LLMultiFloater(const std::string& name, const LLRect& rect, LLTabContainer::TabPosition tab_pos = LLTabContainer::TOP, BOOL auto_resize = TRUE);
	LLMultiFloater(const std::string& name, const std::string& rect_control, LLTabContainer::TabPosition tab_pos = LLTabContainer::TOP, BOOL auto_resize = TRUE);
	virtual ~LLMultiFloater() {};

	virtual BOOL postBuild();
	virtual LLXMLNodePtr getXML(bool save_children = true) const;
	/*virtual*/ void open();	/* Flawfinder: ignore */
	/*virtual*/ void onClose(bool app_quitting);
	/*virtual*/ void draw();
	/*virtual*/ void setVisible(BOOL visible);

	virtual void setCanResize(BOOL can_resize);
	virtual void growToFit(S32 content_width, S32 content_height);
	virtual void addFloater(LLFloater* floaterp, BOOL select_added_floater, LLTabContainer::eInsertionPoint insertion_point = LLTabContainer::END);

	virtual void showFloater(LLFloater* floaterp);
	virtual void removeFloater(LLFloater* floaterp);

	virtual void tabOpen(LLFloater* opened_floater, bool from_click);
	virtual void tabClose();

	virtual BOOL selectFloater(LLFloater* floaterp);
	virtual void selectNextFloater();
	virtual void selectPrevFloater();

	virtual LLFloater*	getActiveFloater();
	virtual BOOL		isFloaterFlashing(LLFloater* floaterp);
	virtual S32			getFloaterCount();

	virtual void setFloaterFlashing(LLFloater* floaterp, BOOL flashing);
	virtual BOOL closeAllFloaters();	//Returns FALSE if the floater could not be closed due to pending confirmation dialogs
	void setTabContainer(LLTabContainer* tab_container) { if (!mTabContainer) mTabContainer = tab_container; }
	static void onTabSelected(void* userdata, bool);

	virtual void updateResizeLimits();

protected:
	struct LLFloaterData
	{
		S32		mWidth;
		S32		mHeight;
		BOOL	mCanMinimize;
		BOOL	mCanResize;
	};

	LLTabContainer*		mTabContainer;
	
	typedef std::map<LLHandle<LLFloater>, LLFloaterData> floater_data_map_t;
	floater_data_map_t	mFloaterDataMap;
	
	LLTabContainer::TabPosition mTabPos;
	BOOL				mAutoResize;
	S32					mOrigMinWidth, mOrigMinHeight;  // logically const but initialized late
};

#endif  // LL_MULTI_FLOATER_H



