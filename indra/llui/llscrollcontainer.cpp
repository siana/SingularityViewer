/** 
 * @file llscrollcontainer.cpp
 * @brief LLScrollContainer base class
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


#include "linden_common.h"

#include "llscrollcontainer.h"

#include "llrender.h"
#include "llcontainerview.h"
#include "lllocalcliprect.h"
#include "llscrollbar.h"
#include "llui.h"
#include "llkeyboard.h"
#include "lluiimage.h"
#include "llviewborder.h"
#include "llfocusmgr.h"
#include "llframetimer.h"
#include "lluictrlfactory.h"
#include "llpanel.h"
#include "llfontgl.h"

///----------------------------------------------------------------------------
/// Local function declarations, constants, enums, and typedefs
///----------------------------------------------------------------------------

static const S32 HORIZONTAL_MULTIPLE = 8;
static const S32 VERTICAL_MULTIPLE = 16;
static const F32 MIN_AUTO_SCROLL_RATE = 120.f;
static const F32 MAX_AUTO_SCROLL_RATE = 500.f;
static const F32 AUTO_SCROLL_RATE_ACCEL = 120.f;

///----------------------------------------------------------------------------
/// Class LLScrollContainer
///----------------------------------------------------------------------------

static LLRegisterWidget<LLScrollContainer> r("scroll_container");

// Default constructor
LLScrollContainer::LLScrollContainer( const std::string& name,
													  const LLRect& rect,
													  LLView* scrolled_view,
													  BOOL is_opaque,
													  const LLColor4& bg_color ) :
	LLUICtrl( name, rect, FALSE ),
	mAutoScrolling( FALSE ),
	mAutoScrollRate( 0.f ),
	mBackgroundColor( bg_color ),
	mIsOpaque( is_opaque ),
	mHideScrollbar( FALSE ),
	mReserveScrollCorner( FALSE ),
	mMinAutoScrollRate( MIN_AUTO_SCROLL_RATE ),
	mMaxAutoScrollRate( MAX_AUTO_SCROLL_RATE ),
	mScrolledView( scrolled_view ),
	mPassBackToChildren(true)
{
	if( mScrolledView )
	{
		LLView::addChild( mScrolledView );
	}

	S32 scrollbar_size = SCROLLBAR_SIZE;
	LLRect border_rect( 0, getRect().getHeight(), getRect().getWidth(), 0 );
	mBorder = new LLViewBorder( std::string("scroll border"), border_rect, LLViewBorder::BEVEL_IN );
	LLView::addChild( mBorder );

	mInnerRect.set( 0, getRect().getHeight(), getRect().getWidth(), 0 );
	mInnerRect.stretch( -getBorderWidth()  );

	LLRect vertical_scroll_rect = mInnerRect;
	vertical_scroll_rect.mLeft = vertical_scroll_rect.mRight - scrollbar_size;
	mScrollbar[VERTICAL] = new LLScrollbar( std::string("scrollable vertical"),
											vertical_scroll_rect,
											LLScrollbar::VERTICAL,
											mInnerRect.getHeight(), 
											0,
											mInnerRect.getHeight(),
											NULL,
											VERTICAL_MULTIPLE);
	LLView::addChild( mScrollbar[VERTICAL] );
	mScrollbar[VERTICAL]->setVisible( FALSE );
	mScrollbar[VERTICAL]->setFollowsRight();
	mScrollbar[VERTICAL]->setFollowsTop();
	mScrollbar[VERTICAL]->setFollowsBottom();
	
	LLRect horizontal_scroll_rect = mInnerRect;
	horizontal_scroll_rect.mTop = horizontal_scroll_rect.mBottom + scrollbar_size;
	mScrollbar[HORIZONTAL] = new LLScrollbar( std::string("scrollable horizontal"),
											  horizontal_scroll_rect,
											  LLScrollbar::HORIZONTAL,
											  mInnerRect.getWidth(),
											  0,
											  mInnerRect.getWidth(),
											  NULL,
											  HORIZONTAL_MULTIPLE);
	LLView::addChild( mScrollbar[HORIZONTAL] );
	mScrollbar[HORIZONTAL]->setVisible( FALSE );
	mScrollbar[HORIZONTAL]->setFollowsLeft();
	mScrollbar[HORIZONTAL]->setFollowsRight();

	setTabStop(FALSE);
}

// Destroys the object
LLScrollContainer::~LLScrollContainer( void )
{
	// mScrolledView and mScrollbar are child views, so the LLView
	// destructor takes care of memory deallocation.
	for( S32 i = 0; i < SCROLLBAR_COUNT; i++ )
	{
		mScrollbar[i] = NULL;
	}
	mScrolledView = NULL;
}

// internal scrollbar handlers
// virtual
void LLScrollContainer::scrollHorizontal( S32 new_pos )
{
	//LL_INFOS() << "LLScrollContainer::scrollHorizontal()" << LL_ENDL;
	if( mScrolledView )
	{
		LLRect doc_rect = mScrolledView->getRect();
		S32 old_pos = -(doc_rect.mLeft - mInnerRect.mLeft);
		mScrolledView->translate( -(new_pos - old_pos), 0 );
	}
}

// virtual
void LLScrollContainer::scrollVertical( S32 new_pos )
{
	// LL_INFOS() << "LLScrollContainer::scrollVertical() " << new_pos << LL_ENDL;
	if( mScrolledView )
	{
		LLRect doc_rect = mScrolledView->getRect();
		S32 old_pos = doc_rect.mTop - mInnerRect.mTop;
		mScrolledView->translate( 0, new_pos - old_pos );
	}
}

// LLView functionality
void LLScrollContainer::reshape(S32 width, S32 height,
										BOOL called_from_parent)
{
	LLUICtrl::reshape( width, height, called_from_parent );

	mInnerRect = getLocalRect();
	mInnerRect.stretch( -getBorderWidth() );

	if (mScrolledView)
	{
		const LLRect& scrolled_rect = mScrolledView->getRect();

		S32 visible_width = 0;
		S32 visible_height = 0;
		BOOL show_v_scrollbar = FALSE;
		BOOL show_h_scrollbar = FALSE;
		calcVisibleSize( &visible_width, &visible_height, &show_h_scrollbar, &show_v_scrollbar );

		mScrollbar[VERTICAL]->setDocSize( scrolled_rect.getHeight() );
		mScrollbar[VERTICAL]->setPageSize( visible_height );

		mScrollbar[HORIZONTAL]->setDocSize( scrolled_rect.getWidth() );
		mScrollbar[HORIZONTAL]->setPageSize( visible_width );
		updateScroll();
	}
}

BOOL LLScrollContainer::handleKeyHere(KEY key, MASK mask)
{
	// allow scrolled view to handle keystrokes in case it delegated keyboard focus
	// to the scroll container.  
	// NOTE: this should not recurse indefinitely as handleKeyHere
	// should not propagate to parent controls, so mScrolledView should *not*
	// call LLScrollContainer::handleKeyHere in turn
	if (mScrolledView && mScrolledView->handleKeyHere(key, mask))
	{
		return TRUE;
	}
	for( S32 i = 0; i < SCROLLBAR_COUNT; i++ )
	{
		if( mScrollbar[i]->handleKeyHere(key, mask) )
		{
			updateScroll();
			return TRUE;
		}
	}	

	return FALSE;
}

BOOL LLScrollContainer::handleUnicodeCharHere(llwchar uni_char)
{
	if (mScrolledView && mScrolledView->handleUnicodeCharHere(uni_char))
	{
		return TRUE;
	}
	return FALSE;
}

BOOL LLScrollContainer::handleScrollWheel( S32 x, S32 y, S32 clicks )
{
	// Give event to my child views - they may have scroll bars
	// (Bad UI design, but technically possible.)
	if (mPassBackToChildren && LLUICtrl::handleScrollWheel(x,y,clicks))
		return TRUE;

	// When the vertical scrollbar is visible, scroll wheel
	// only affects vertical scrolling.  It's confusing to have
	// scroll wheel perform both vertical and horizontal in a
	// single container.
	LLScrollbar* vertical = mScrollbar[VERTICAL];
	if (vertical->getVisible()
		&& vertical->getEnabled())
	{
		// Pretend the mouse is over the scrollbar
		if (vertical->handleScrollWheel( 0, 0, clicks ) )
		{
			updateScroll();
		}
		// Always eat the event
		return TRUE;
	}

	LLScrollbar* horizontal = mScrollbar[HORIZONTAL];
	// Test enablement and visibility for consistency with
	// LLView::childrenHandleScrollWheel().
	if (horizontal->getVisible()
		&& horizontal->getEnabled()
		&& horizontal->handleScrollWheel( 0, 0, clicks ) )
	{
		updateScroll();
		return TRUE;
	}
	return FALSE;
}

BOOL LLScrollContainer::handleDragAndDrop(S32 x, S32 y, MASK mask,
												  BOOL drop,
												  EDragAndDropType cargo_type,
												  void* cargo_data,
												  EAcceptance* accept,
												  std::string& tooltip_msg)
{
	// Scroll folder view if needed.  Never accepts a drag or drop.
	*accept = ACCEPT_NO;
	BOOL handled = autoScroll(x, y);

	if( !handled )
	{
		handled = childrenHandleDragAndDrop(x, y, mask, drop, cargo_type,
											cargo_data, accept, tooltip_msg) != NULL;
	}

	return TRUE;
}

bool LLScrollContainer::autoScroll(S32 x, S32 y)
{
	S32 scrollbar_size = SCROLLBAR_SIZE;

	bool scrolling = false;
	if( mScrollbar[HORIZONTAL]->getVisible() || mScrollbar[VERTICAL]->getVisible() )
	{
		LLRect screen_local_extents;
		screenRectToLocal(getRootView()->getLocalRect(), &screen_local_extents);

		LLRect inner_rect_local( 0, mInnerRect.getHeight(), mInnerRect.getWidth(), 0 );
		if(	mScrollbar[HORIZONTAL]->getVisible() )
		{
			inner_rect_local.mBottom += scrollbar_size;
		}
		if(	mScrollbar[VERTICAL]->getVisible() )
		{
			inner_rect_local.mRight -= scrollbar_size;
		}

		// clip rect against root view
		inner_rect_local.intersectWith(screen_local_extents);

		S32 auto_scroll_speed = ll_round(mAutoScrollRate * LLFrameTimer::getFrameDeltaTimeF32());
		// autoscroll region should take up no more than one third of visible scroller area
		S32 auto_scroll_region_width = llmin(inner_rect_local.getWidth() / 3, 10); 
		S32 auto_scroll_region_height = llmin(inner_rect_local.getHeight() / 3, 10); 

		if(	mScrollbar[HORIZONTAL]->getVisible() )
		{
			LLRect left_scroll_rect = screen_local_extents;
			left_scroll_rect.mRight = inner_rect_local.mLeft + auto_scroll_region_width;
			if( left_scroll_rect.pointInRect( x, y ) && (mScrollbar[HORIZONTAL]->getDocPos() > 0) )
			{
				mScrollbar[HORIZONTAL]->setDocPos( mScrollbar[HORIZONTAL]->getDocPos() - auto_scroll_speed );
				mAutoScrolling = TRUE;
				scrolling = true;
			}

			LLRect right_scroll_rect = screen_local_extents;
			right_scroll_rect.mLeft = inner_rect_local.mRight - auto_scroll_region_width;
			if( right_scroll_rect.pointInRect( x, y ) && (mScrollbar[HORIZONTAL]->getDocPos() < mScrollbar[HORIZONTAL]->getDocPosMax()) )
			{
				mScrollbar[HORIZONTAL]->setDocPos( mScrollbar[HORIZONTAL]->getDocPos() + auto_scroll_speed );
				mAutoScrolling = TRUE;
				scrolling = true;
			}
		}
		if(	mScrollbar[VERTICAL]->getVisible() )
		{
			LLRect bottom_scroll_rect = screen_local_extents;
			bottom_scroll_rect.mTop = inner_rect_local.mBottom + auto_scroll_region_height;
			if( bottom_scroll_rect.pointInRect( x, y ) && (mScrollbar[VERTICAL]->getDocPos() < mScrollbar[VERTICAL]->getDocPosMax()) )
			{
				mScrollbar[VERTICAL]->setDocPos( mScrollbar[VERTICAL]->getDocPos() + auto_scroll_speed );
				mAutoScrolling = TRUE;
				scrolling = true;
			}

			LLRect top_scroll_rect = screen_local_extents;
			top_scroll_rect.mBottom = inner_rect_local.mTop - auto_scroll_region_height;
			if( top_scroll_rect.pointInRect( x, y ) && (mScrollbar[VERTICAL]->getDocPos() > 0) )
			{
				mScrollbar[VERTICAL]->setDocPos( mScrollbar[VERTICAL]->getDocPos() - auto_scroll_speed );
				mAutoScrolling = TRUE;
				scrolling = true;
			}
		}
	}
	return scrolling;
}


BOOL LLScrollContainer::handleToolTip(S32 x, S32 y, std::string& msg, LLRect* sticky_rect)
{
	S32 local_x, local_y;
	for( S32 i = 0; i < SCROLLBAR_COUNT; i++ )
	{
		local_x = x - mScrollbar[i]->getRect().mLeft;
		local_y = y - mScrollbar[i]->getRect().mBottom;
		if( mScrollbar[i]->handleToolTip(local_x, local_y, msg, sticky_rect) )
		{
			return TRUE;
		}
	}
	// Handle 'child' view.
	if( mScrolledView )
	{
		local_x = x - mScrolledView->getRect().mLeft;
		local_y = y - mScrolledView->getRect().mBottom;
		if( mScrolledView->handleToolTip(local_x, local_y, msg, sticky_rect) )
		{
			return TRUE;
		}
	}

	// Opaque
	return TRUE;
}

void LLScrollContainer::calcVisibleSize( S32 *visible_width, S32 *visible_height, BOOL* show_h_scrollbar, BOOL* show_v_scrollbar ) const
{
	const LLRect& doc_rect = getScrolledViewRect();
	S32 scrollbar_size = SCROLLBAR_SIZE;
	S32 doc_width = doc_rect.getWidth();
	S32 doc_height = doc_rect.getHeight();

	S32 border_width = getBorderWidth();
	*visible_width = getRect().getWidth() - 2 * border_width;
	*visible_height = getRect().getHeight() - 2 * border_width;
	
	*show_v_scrollbar = FALSE;
	*show_h_scrollbar = FALSE;

	if (!mHideScrollbar)
	{
		if( *visible_height < doc_height )
		{
			*show_v_scrollbar = TRUE;
			*visible_width -= scrollbar_size;
		}

		if( *visible_width < doc_width )
		{
			*show_h_scrollbar = TRUE;
			*visible_height -= scrollbar_size;

			// Must retest now that visible_height has changed
			if( !*show_v_scrollbar && (*visible_height < doc_height) )
			{
				*show_v_scrollbar = TRUE;
				*visible_width -= scrollbar_size;
			}
		}
	}
}


void LLScrollContainer::draw()
{
	S32 scrollbar_size = SCROLLBAR_SIZE;
	if (mAutoScrolling)
	{
		// add acceleration to autoscroll
		mAutoScrollRate = llmin(mAutoScrollRate + (LLFrameTimer::getFrameDeltaTimeF32() * AUTO_SCROLL_RATE_ACCEL), MAX_AUTO_SCROLL_RATE);
	}
	else
	{
		// reset to minimum for next time
		mAutoScrollRate = mMinAutoScrollRate;
	}
	// clear this flag to be set on next call to autoScroll
	mAutoScrolling = FALSE;

	// auto-focus when scrollbar active
	// this allows us to capture user intent (i.e. stop automatically scrolling the view/etc)
	if (!hasFocus() 
		&& (mScrollbar[VERTICAL]->hasMouseCapture() || mScrollbar[HORIZONTAL]->hasMouseCapture()))
	{
		focusFirstItem();
	}

	if (getRect().isValid()) 
	{
		// Draw background
		if( mIsOpaque )
		{
			gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
			gGL.color4fv( mBackgroundColor.mV );
			gl_rect_2d( mInnerRect );
		}
	
		// Draw mScrolledViews and update scroll bars.
		// get a scissor region ready, and draw the scrolling view. The
		// scissor region ensures that we don't draw outside of the bounds
		// of the rectangle.
		if( mScrolledView )
		{
			updateScroll();

			// Draw the scrolled area.
			{
				S32 visible_width = 0;
				S32 visible_height = 0;
				BOOL show_v_scrollbar = FALSE;
				BOOL show_h_scrollbar = FALSE;
				calcVisibleSize( &visible_width, &visible_height, &show_h_scrollbar, &show_v_scrollbar );

				LLLocalClipRect clip(LLRect(mInnerRect.mLeft, 
						mInnerRect.mBottom + (show_h_scrollbar ? scrollbar_size : 0) + visible_height,
						mInnerRect.mRight - (show_v_scrollbar ? scrollbar_size: 0),
						mInnerRect.mBottom + (show_h_scrollbar ? scrollbar_size : 0)
						));
				drawChild(mScrolledView);
			}
		}

		// Highlight border if a child of this container has keyboard focus
		if( mBorder->getVisible() )
		{
			mBorder->setKeyboardFocusHighlight( gFocusMgr.childHasKeyboardFocus(this) );
		}

		// Draw all children except mScrolledView
		// Note: scrollbars have been adjusted by above drawing code
		for (child_list_const_reverse_iter_t child_iter = getChildList()->rbegin();
			 child_iter != getChildList()->rend(); ++child_iter)
		{
			LLView *viewp = *child_iter;
			if( sDebugRects )
			{
				sDepth++;
			}
			if( (viewp != mScrolledView) && viewp->getVisible() )
			{
				drawChild(viewp);
			}
			if( sDebugRects )
			{
				sDepth--;
			}
		}
	}

	if (sDebugRects)
	{
		drawDebugRect();
	}

} // end draw

bool LLScrollContainer::addChild(LLView* view, S32 tab_group)
{
	if (!mScrolledView)
	{
		// Use the first panel or container as the scrollable view (bit of a hack)
		mScrolledView = view;
	}

	bool ret_val = LLView::addChild(view, tab_group);

	//bring the scrollbars to the front
	sendChildToFront( mScrollbar[HORIZONTAL] );
	sendChildToFront( mScrollbar[VERTICAL] );

	return ret_val;
}

void LLScrollContainer::updateScroll()
{
	if (!getVisible() || !mScrolledView)
	{
		return;
	}
	S32 scrollbar_size = SCROLLBAR_SIZE;
	LLRect doc_rect = mScrolledView->getRect();
	S32 doc_width = doc_rect.getWidth();
	S32 doc_height = doc_rect.getHeight();
	S32 visible_width = 0;
	S32 visible_height = 0;
	BOOL show_v_scrollbar = FALSE;
	BOOL show_h_scrollbar = FALSE;
	calcVisibleSize( &visible_width, &visible_height, &show_h_scrollbar, &show_v_scrollbar );

	S32 border_width = getBorderWidth();
	if( show_v_scrollbar )
	{
		if( doc_rect.mTop < getRect().getHeight() - border_width )
		{
			mScrolledView->translate( 0, getRect().getHeight() - border_width - doc_rect.mTop );
		}

		scrollVertical(	mScrollbar[VERTICAL]->getDocPos() );
		mScrollbar[VERTICAL]->setVisible( TRUE );

		S32 v_scrollbar_height = visible_height;
		if( !show_h_scrollbar && mReserveScrollCorner )
		{
			v_scrollbar_height -= scrollbar_size;
		}
		mScrollbar[VERTICAL]->reshape( scrollbar_size, v_scrollbar_height, TRUE );

		// Make room for the horizontal scrollbar (or not)
		S32 v_scrollbar_offset = 0;
		if( show_h_scrollbar || mReserveScrollCorner )
		{
			v_scrollbar_offset = scrollbar_size;
		}
		LLRect r = mScrollbar[VERTICAL]->getRect();
		r.translate( 0, mInnerRect.mBottom - r.mBottom + v_scrollbar_offset );
		mScrollbar[VERTICAL]->setRect( r );
	}
	else
	{
		mScrolledView->translate( 0, getRect().getHeight() - border_width - doc_rect.mTop );

		mScrollbar[VERTICAL]->setVisible( FALSE );
		mScrollbar[VERTICAL]->setDocPos( 0 );
	}
		
	if( show_h_scrollbar )
	{
		if( doc_rect.mLeft > border_width )
		{
			mScrolledView->translate( border_width - doc_rect.mLeft, 0 );
			mScrollbar[HORIZONTAL]->setDocPos( 0 );
		}
		else
		{
			scrollHorizontal( mScrollbar[HORIZONTAL]->getDocPos() );
		}
	
		mScrollbar[HORIZONTAL]->setVisible( TRUE );
		S32 h_scrollbar_width = visible_width;
		if( !show_v_scrollbar && mReserveScrollCorner )
		{
			h_scrollbar_width -= scrollbar_size;
		}
		mScrollbar[HORIZONTAL]->reshape( h_scrollbar_width, scrollbar_size, TRUE );
	}
	else
	{
		mScrolledView->translate( border_width - doc_rect.mLeft, 0 );
		
		mScrollbar[HORIZONTAL]->setVisible( FALSE );
		mScrollbar[HORIZONTAL]->setDocPos( 0 );
	}

	mScrollbar[HORIZONTAL]->setDocSize( doc_width );
	mScrollbar[HORIZONTAL]->setPageSize( visible_width );

	mScrollbar[VERTICAL]->setDocSize( doc_height );
	mScrollbar[VERTICAL]->setPageSize( visible_height );
} // end updateScroll

void LLScrollContainer::setBorderVisible(BOOL b)
{
	mBorder->setVisible( b );
	// Recompute inner rect, as border visibility changes it
	mInnerRect = getLocalRect();
	mInnerRect.stretch( -getBorderWidth() );
}

LLRect LLScrollContainer::getVisibleContentRect()
{
	updateScroll();
	LLRect visible_rect = getContentWindowRect();
	LLRect contents_rect = mScrolledView->getRect();
	visible_rect.translate(-contents_rect.mLeft, -contents_rect.mBottom);
	return visible_rect;
}

LLRect LLScrollContainer::getContentWindowRect()
{
	updateScroll();
	LLRect scroller_view_rect;
	S32 visible_width = 0;
	S32 visible_height = 0;
	BOOL show_h_scrollbar = FALSE;
	BOOL show_v_scrollbar = FALSE;
	calcVisibleSize( &visible_width, &visible_height, &show_h_scrollbar, &show_v_scrollbar );
	S32 border_width = getBorderWidth();
	scroller_view_rect.setOriginAndSize(border_width, 
										show_h_scrollbar ? mScrollbar[HORIZONTAL]->getRect().mTop : border_width, 
										visible_width, 
										visible_height);
	return scroller_view_rect;
}

// rect is in document coordinates, constraint is in display coordinates relative to content window rect
void LLScrollContainer::scrollToShowRect(const LLRect& rect, const LLRect& constraint)
{
	if (!mScrolledView)
	{
		LL_WARNS() << "LLScrollContainer::scrollToShowRect with no view!" << LL_ENDL;
		return;
	}

	LLRect content_window_rect = getContentWindowRect();
	// get document rect
	LLRect scrolled_rect = mScrolledView->getRect();

	// shrink target rect to fit within constraint region, biasing towards top left
	LLRect rect_to_constrain = rect;
	rect_to_constrain.mBottom = llmax(rect_to_constrain.mBottom, rect_to_constrain.mTop - constraint.getHeight());
	rect_to_constrain.mRight = llmin(rect_to_constrain.mRight, rect_to_constrain.mLeft + constraint.getWidth());

	// calculate allowable positions for scroller window in document coordinates
	LLRect allowable_scroll_rect(rect_to_constrain.mRight - constraint.mRight,
								rect_to_constrain.mBottom - constraint.mBottom,
								rect_to_constrain.mLeft - constraint.mLeft,
								rect_to_constrain.mTop - constraint.mTop);

	// translate from allowable region for lower left corner to upper left corner
	allowable_scroll_rect.translate(0, content_window_rect.getHeight());

	S32 vert_pos = llclamp(mScrollbar[VERTICAL]->getDocPos(), 
					mScrollbar[VERTICAL]->getDocSize() - allowable_scroll_rect.mTop, // min vertical scroll
					mScrollbar[VERTICAL]->getDocSize() - allowable_scroll_rect.mBottom); // max vertical scroll	

	mScrollbar[VERTICAL]->setDocSize( scrolled_rect.getHeight() );
	mScrollbar[VERTICAL]->setPageSize( content_window_rect.getHeight() );
	mScrollbar[VERTICAL]->setDocPos( vert_pos );

	S32 horizontal_pos = llclamp(mScrollbar[HORIZONTAL]->getDocPos(), 
								allowable_scroll_rect.mLeft,
								allowable_scroll_rect.mRight);

	mScrollbar[HORIZONTAL]->setDocSize( scrolled_rect.getWidth() );
	mScrollbar[HORIZONTAL]->setPageSize( content_window_rect.getWidth() );
	mScrollbar[HORIZONTAL]->setDocPos( horizontal_pos );

	// propagate scroll to document
	updateScroll();

	// In case we are in accordion tab notify parent to show selected rectangle
	LLRect screen_rc;
	localRectToScreen(rect_to_constrain, &screen_rc);
	notifyParent(LLSD().with("scrollToShowRect",screen_rc.getValue()));
}

void LLScrollContainer::pageUp(S32 overlap)
{
	mScrollbar[VERTICAL]->pageUp(overlap);
	updateScroll();
}

void LLScrollContainer::pageDown(S32 overlap)
{
	mScrollbar[VERTICAL]->pageDown(overlap);
	updateScroll();
}

void LLScrollContainer::goToTop()
{
	mScrollbar[VERTICAL]->setDocPos(0);
	updateScroll();
}

void LLScrollContainer::goToBottom()
{
	mScrollbar[VERTICAL]->setDocPos(mScrollbar[VERTICAL]->getDocSize());
	updateScroll();
}

S32 LLScrollContainer::getBorderWidth() const
{
	if (mBorder && mBorder->getVisible())
	{
		return mBorder->getBorderWidth();
	}

	return 0;
}

void LLScrollContainer::setSize(S32 size)
{
	mSize = size;
	mScrollbar[VERTICAL]->setThickness(size);
	mScrollbar[HORIZONTAL]->setThickness(size);
}

// virtual
LLXMLNodePtr LLScrollContainer::getXML(bool save_children) const
{
	LLXMLNodePtr node = LLUICtrl::getXML();

	node->setName(LL_SCROLLABLE_CONTAINER_VIEW_TAG);

	// Attributes

	node->createChild("opaque", TRUE)->setBoolValue(mIsOpaque);

	if (mIsOpaque)
	{
		node->createChild("color", TRUE)->setFloatValue(4, mBackgroundColor.mV);
	}

	// Contents

	LLXMLNodePtr child_node = mScrolledView->getXML();

	node->addChild(child_node);

	return node;
}

LLView* LLScrollContainer::fromXML(LLXMLNodePtr node, LLView *parent, LLUICtrlFactory *factory)
{
	std::string name("scroll_container");
	node->getAttributeString("name", name);

	LLRect rect;
	U32 follows_flags = createRect(node, rect, parent, LLRect());
	
	BOOL opaque = FALSE;
	node->getAttributeBOOL("opaque", opaque);

	LLColor4 color(0,0,0,0);
	LLUICtrlFactory::getAttributeColor(node,"color", color);

	// Create the scroll view
	LLScrollContainer *ret = new LLScrollContainer(name, rect, (LLPanel*)NULL, opaque, color);

	// Obey xml follows
	ret->setFollows(follows_flags);
	ret->parseFollowsFlags(node);

	LLPanel* panelp = NULL;

	// Find a child panel to add
	LLXMLNodePtr child;
	for (child = node->getFirstChild(); child.notNull(); child = child->getNextSibling())
	{
		LLView *control = factory->createCtrlWidget(panelp, child);
		if (control && control->isPanel())
		{
			if (panelp)
			{
				LL_INFOS() << "Warning! Attempting to put multiple panels into a scrollable container view!" << LL_ENDL;
				delete control;
			}
			else
			{
				panelp = (LLPanel*)control;
			}
		}
	}

	if (panelp == NULL)
	{
		panelp = new LLPanel(std::string("dummy"), LLRect::null, FALSE);
	}

	ret->addChild(panelp);

	return ret;
}
