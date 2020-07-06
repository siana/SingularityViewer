/** 
 * @file llscrollcontainer.h
 * @brief LLScrollContainer class header file.
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

#ifndef LL_LLSCROLLCONTAINER_H
#define LL_LLSCROLLCONTAINER_H

#include "lluictrl.h"
#ifndef LL_V4COLOR_H
#include "v4color.h"
#endif
#include "stdenums.h"
#include "llcoord.h"
#include "llscrollbar.h"


class LLViewBorder;
class LLUICtrlFactory;

/*****************************************************************************
 * 
 * A decorator view class meant to encapsulate a clipped region which is
 * scrollable. It automatically takes care of pixel perfect scrolling
 * and cliipping, as well as turning the scrollbars on or off based on
 * the width and height of the view you're scrolling.
 *
 *****************************************************************************/
class LLScrollContainer : public LLUICtrl
{
public:
	// Note: vertical comes before horizontal because vertical
	// scrollbars have priority for mouse and keyboard events.
	enum SCROLL_ORIENTATION { VERTICAL, HORIZONTAL, SCROLLBAR_COUNT };

	LLScrollContainer( const std::string& name, const LLRect& rect,
							   LLView* scrolled_view, BOOL is_opaque = FALSE,
							   const LLColor4& bg_color = LLColor4(0,0,0,0) );
	virtual ~LLScrollContainer( void );

	void setScrolledView(LLView* view) { mScrolledView = view; }

	virtual void setValue(const LLSD& value) { mInnerRect.setValue(value); }

	void			setBorderVisible( BOOL b );
	void			setPassBackToChildren(bool b) { mPassBackToChildren = b; }

	void			scrollToShowRect( const LLRect& rect, const LLRect& constraint);
	void			scrollToShowRect( const LLRect& rect) { scrollToShowRect(rect, LLRect(0, mInnerRect.getHeight(), mInnerRect.getWidth(), 0)); }

	void			setReserveScrollCorner( BOOL b ) { mReserveScrollCorner = b; }
	LLRect			getVisibleContentRect();
	LLRect			getContentWindowRect();
	virtual const LLRect&	getScrolledViewRect() const { return mScrolledView ? mScrolledView->getRect() : LLRect::null; }
	void			pageUp(S32 overlap = 0);
	void			pageDown(S32 overlap = 0);
	void			goToTop();
	void			goToBottom();
	bool			isAtTop() { return mScrollbar[VERTICAL]->isAtBeginning(); }
	bool			isAtBottom() { return mScrollbar[VERTICAL]->isAtEnd(); }
	S32				getBorderWidth() const;

	// LLView functionality
	virtual void	reshape(S32 width, S32 height, BOOL called_from_parent = TRUE);
	virtual BOOL	handleKeyHere(KEY key, MASK mask);
	virtual BOOL	handleUnicodeCharHere(llwchar uni_char);
	virtual BOOL	handleScrollWheel( S32 x, S32 y, S32 clicks );
	virtual BOOL	handleDragAndDrop(S32 x, S32 y, MASK mask, BOOL drop,
								   EDragAndDropType cargo_type,
								   void* cargo_data,
								   EAcceptance* accept,
								   std::string& tooltip_msg);

	virtual BOOL	handleToolTip(S32 x, S32 y, std::string& msg, LLRect* sticky_rect);
	virtual void	draw();
	virtual bool	addChild(LLView* view, S32 tab_group = 0);
	
	bool autoScroll(S32 x, S32 y);
	virtual	LLXMLNodePtr getXML(bool save_children) const;
	static LLView* fromXML(LLXMLNodePtr node, LLView *parent, LLUICtrlFactory *factory);

	S32 getSize() const { return mSize; }
	void setSize(S32 thickness);

protected:
	LLView*		mScrolledView;

private:
	void init();
	// internal scrollbar handlers
	virtual void scrollHorizontal( S32 new_pos );
	virtual void scrollVertical( S32 new_pos );
	void updateScroll();
public:
	void calcVisibleSize( S32 *visible_width, S32 *visible_height, BOOL* show_h_scrollbar, BOOL* show_v_scrollbar ) const;
private:

	LLScrollbar* mScrollbar[SCROLLBAR_COUNT];
	S32			mSize;
	BOOL		mIsOpaque;
	LLColor4	mBackgroundColor;
	LLRect		mInnerRect;
	LLViewBorder* mBorder;
	BOOL		mReserveScrollCorner;
	BOOL		mAutoScrolling;
	F32			mAutoScrollRate;
	F32			mMinAutoScrollRate;
	F32			mMaxAutoScrollRate;
	bool		mHideScrollbar;
	bool		mPassBackToChildren;
};


#endif // LL_LLSCROLLCONTAINER_H
