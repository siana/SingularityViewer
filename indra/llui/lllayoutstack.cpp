/** 
 * @file lllayoutstack.cpp
 * @brief LLLayout class - dynamic stacking of UI elements
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

// Opaque view with a background and a border.  Can contain LLUICtrls.

#include "linden_common.h"

#include "lllayoutstack.h"

#include "lllocalcliprect.h"
#include "llpanel.h"
#include "lluictrlfactory.h"
#include "llcriticaldamp.h"

#include <boost/range/adaptor/reversed.hpp>

static const F32 MIN_FRACTIONAL_SIZE = 0.00001f;
static const F32 MAX_FRACTIONAL_SIZE = 1.f;
static const S32 RESIZE_BAR_OVERLAP = 1;
static const S32 RESIZE_BAR_HEIGHT = 3;

static LLRegisterWidget<LLLayoutPanel> r1("layout_panel");


//
// LLLayoutPanel
//
LLLayoutPanel::LLLayoutPanel(S32 min_dim, BOOL auto_resize, BOOL user_resize, LLRect rect)
:	LLPanel("",rect,false), 
	mMinDim(min_dim), 
	mAutoResize(auto_resize),
	mUserResize(user_resize),
	mCollapsed(FALSE),
	mCollapseAmt(0.f),
	mVisibleAmt(1.f), // default to fully visible
	mResizeBar(NULL),
	mFractionalSize(0.f),
	mTargetDim(0),
	mIgnoreReshape(false),
	mOrientation(LLLayoutStack::HORIZONTAL),
	mExpandedMinDim(-1)
{

}

BOOL LLLayoutPanel::initPanelXML(LLXMLNodePtr node, LLView *parent, LLUICtrlFactory *factory)
{
	std::string orientation_string;
	node->getAttributeString("orientation", orientation_string);
	if (orientation_string == "horizontal")
	{
		mOrientation = LLLayoutStack::HORIZONTAL;
	}
	else if (orientation_string == "vertical")
	{
		mOrientation = LLLayoutStack::VERTICAL;
	}

	if(node->hasAttribute("min_dim"))
		node->getAttributeS32("min_dim", mMinDim);
	else if(mOrientation == LLLayoutStack::HORIZONTAL)
		node->getAttributeS32("min_width", mMinDim);
	else if(mOrientation == LLLayoutStack::VERTICAL)
		node->getAttributeS32("min_height", mMinDim);
	node->getAttributeS32("expanded_min_dim", mExpandedMinDim);

	BOOL auto_resize = mAutoResize;
	BOOL user_resize = mUserResize;
	node->getAttributeBOOL("auto_resize", auto_resize);
	node->getAttributeBOOL("user_resize", user_resize);
	mAutoResize = auto_resize;
	mUserResize = user_resize;

	bool ret = LLPanel::initPanelXML(node,parent,factory);
	// panels initialized as hidden should not start out partially visible
	if (!getVisible())
	{
		mVisibleAmt = 0.f;
	}
	setFollowsNone();
	return ret;
}

LLLayoutPanel::~LLLayoutPanel()
{
	// probably not necessary, but...
	delete mResizeBar;
	mResizeBar = NULL;
}

F32 LLLayoutPanel::getAutoResizeFactor() const
{
	return mVisibleAmt * (1.f - mCollapseAmt);
}
 
F32 LLLayoutPanel::getVisibleAmount() const
{
	return mVisibleAmt;
}

S32 LLLayoutPanel::getLayoutDim() const
{
	return ll_round((F32)((mOrientation == LLLayoutStack::HORIZONTAL)
					? getRect().getWidth()
					: getRect().getHeight()));
}

S32 LLLayoutPanel::getTargetDim() const
{
	return mTargetDim;
}

void LLLayoutPanel::setTargetDim(S32 value)
{
	LLRect new_rect(getRect());
	if (mOrientation == LLLayoutStack::HORIZONTAL)
	{
		new_rect.mRight = new_rect.mLeft + value;
	}
	else
	{
		new_rect.mTop = new_rect.mBottom + value;
	}
	setShape(new_rect, true);
}

S32 LLLayoutPanel::getVisibleDim() const
{
	F32 min_dim = getRelevantMinDim();
	return ll_round(mVisibleAmt
					* (min_dim
						+ (((F32)mTargetDim - min_dim) * (1.f - mCollapseAmt))));
}
 
void LLLayoutPanel::setOrientation( LLLayoutStack::ELayoutOrientation orientation )
{
	mOrientation = orientation;
	S32 layout_dim = ll_round((F32)((mOrientation == LLLayoutStack::HORIZONTAL)
		? getRect().getWidth()
		: getRect().getHeight()));

	if (mAutoResize == FALSE 
		&& mUserResize == TRUE 
		&& mMinDim == -1 )
	{
		setMinDim(layout_dim);
	}
	mTargetDim = llmax(layout_dim, getMinDim());
}
 
void LLLayoutPanel::setVisible( BOOL visible )
{
	if (visible != getVisible())
	{
		LLLayoutStack* stackp = dynamic_cast<LLLayoutStack*>(getParent());
		if (stackp)
		{
			stackp->mNeedsLayout = true;
		}
	}
	LLPanel::setVisible(visible);
}

void LLLayoutPanel::reshape( S32 width, S32 height, BOOL called_from_parent /*= TRUE*/ )
{
	if (width == getRect().getWidth() && height == getRect().getHeight()) return;

	if (!mIgnoreReshape && mAutoResize == false)
	{
		mTargetDim = (mOrientation == LLLayoutStack::HORIZONTAL) ? width : height;
		LLLayoutStack* stackp = dynamic_cast<LLLayoutStack*>(getParent());
		if (stackp)
		{
			stackp->mNeedsLayout = true;
		}
	}
	LLPanel::reshape(width, height, called_from_parent);
}

void LLLayoutPanel::handleReshape(const LLRect& new_rect, bool by_user)
{
	LLLayoutStack* stackp = dynamic_cast<LLLayoutStack*>(getParent());
	if (stackp)
	{
		if (by_user)
		{	// tell layout stack to account for new shape
			
			// make sure that panels have already been auto resized
			stackp->updateLayout();
			// now apply requested size to panel
			stackp->updatePanelRect(this, new_rect);
		}
		stackp->mNeedsLayout = true;
	}
	LLPanel::handleReshape(new_rect, by_user);
}

static LLRegisterWidget<LLLayoutStack> r2("layout_stack");

LLLayoutStack::LLLayoutStack(ELayoutOrientation orientation, S32 border_size, bool animate, bool clip, F32 open_time_constant, F32 close_time_constant, F32 resize_bar_overlap)
:	LLView(),
	mPanelSpacing(border_size),
	mOrientation(orientation),
	mAnimate(animate),
	mAnimatedThisFrame(false),
	mNeedsLayout(true),
	mClip(clip),
	mOpenTimeConstant(open_time_constant),
	mCloseTimeConstant(close_time_constant),
	mResizeBarOverlap(resize_bar_overlap)
{}

LLLayoutStack::~LLLayoutStack()
{
	e_panel_list_t panels = mPanels; // copy list of panel pointers
	mPanels.clear(); // clear so that removeChild() calls don't cause trouble
	std::for_each(panels.begin(), panels.end(), DeletePointer());
}

void LLLayoutStack::draw()
{
	updateLayout();

	// always clip to stack itself
	LLLocalClipRect clip(getLocalRect());
	for (LLLayoutPanel* panelp : mPanels)
	{
		// clip to layout rectangle, not bounding rectangle
		LLRect clip_rect = panelp->getRect();
		// scale clipping rectangle by visible amount
		if (mOrientation == HORIZONTAL)
		{
			clip_rect.mRight = clip_rect.mLeft + panelp->getVisibleDim();
		}
		else
		{
			clip_rect.mBottom = clip_rect.mTop - panelp->getVisibleDim();
		}

		{LLLocalClipRect clip(clip_rect, mClip);
			// only force drawing invisible children if visible amount is non-zero
			drawChild(panelp, 0, 0, !clip_rect.isEmpty());
			
			if (sDebugRects)
			{
				LLUI::pushMatrix();
				{
					// drawing solids requires texturing be disabled
					gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);

					LLUI::translate((F32)panelp->getRect().mLeft, (F32)panelp->getRect().mBottom, 0.f);
					panelp->drawDebugRect();

					// Check for bogus rectangle
					if (!panelp->getRect().isValid())
					{
						LL_WARNS() << "Bogus rectangle for " << panelp->getName() << " with " << panelp->getRect() << LL_ENDL;
					}
				}
				LLUI::popMatrix();
			}
		}
	}
	if (sDebugRects)
	{
		drawDebugRect();

		// Check for bogus rectangle
		if (!getRect().isValid())
		{
			LL_WARNS() << "Bogus rectangle for " << getName() << " with " << getRect() << LL_ENDL;
		}
	}
}

void LLLayoutStack::removeChild(LLView* view)
{
	LLLayoutPanel* embedded_panelp = findEmbeddedPanel(dynamic_cast<LLPanel*>(view));

	if (embedded_panelp)
	{
		mPanels.erase(std::find(mPanels.begin(), mPanels.end(), embedded_panelp));
		delete embedded_panelp;
		updateFractionalSizes();
		mNeedsLayout = true;
	}

	LLView::removeChild(view);
}

BOOL LLLayoutStack::postBuild()
{
	updateLayout();
	return TRUE;
}

bool LLLayoutStack::addChild(LLView* child, S32 tab_group)
{
	LLLayoutPanel* panelp = dynamic_cast<LLLayoutPanel*>(child);
	if (panelp)
	{
		panelp->setOrientation(mOrientation);
		mPanels.push_back(panelp);
		createResizeBar(panelp);
		mNeedsLayout = true;
	}
	BOOL result = LLView::addChild(child, tab_group);

	updateFractionalSizes();
	return result;
}

void LLLayoutStack::addPanel(LLLayoutPanel* panel, EAnimate animate)
{
	addChild(panel);

	// panel starts off invisible (collapsed)
	if (animate == ANIMATE)
	{
		panel->mVisibleAmt = 0.f;
		panel->setVisible(TRUE);
	}
}

void LLLayoutStack::collapsePanel(LLPanel* panel, BOOL collapsed)
{
	LLLayoutPanel* panel_container = findEmbeddedPanel(panel);
	if (!panel_container) return;
	panel_container->mCollapsed = collapsed;
	mNeedsLayout = true;
}

static LLTrace::BlockTimerStatHandle FTM_UPDATE_LAYOUT("Update LayoutStacks");

void LLLayoutStack::updateLayout()
{	
	LL_RECORD_BLOCK_TIME(FTM_UPDATE_LAYOUT);

	if (!mNeedsLayout) return;

	bool continue_animating = animatePanels();
	F32 total_visible_fraction = 0.f;
	S32 space_to_distribute = (mOrientation == HORIZONTAL)
							? getRect().getWidth()
							: getRect().getHeight();

	// first, assign minimum dimensions
	LLLayoutPanel* panelp = mPanels.size() ? *mPanels.rbegin() : nullptr;
	for (auto* panelp : mPanels)
	{
		if (panelp->mAutoResize)
		{
			panelp->mTargetDim = panelp->getRelevantMinDim();
		}
		space_to_distribute -= panelp->getVisibleDim() + ll_round((F32)mPanelSpacing * panelp->getVisibleAmount());
		total_visible_fraction += panelp->mFractionalSize * panelp->getAutoResizeFactor();
	}

	llassert(total_visible_fraction < 1.05f);

	// don't need spacing after last panel
	space_to_distribute += panelp ? ll_round((F32)mPanelSpacing * panelp->getVisibleAmount()) : 0;

	S32 remaining_space = space_to_distribute;
	F32 fraction_distributed = 0.f;
	if (space_to_distribute > 0 && total_visible_fraction > 0.f)
	{	// give space proportionally to visible auto resize panels
		for (LLLayoutPanel* panelp : mPanels)
		{
			if (panelp->mAutoResize)
			{
				F32 fraction_to_distribute = (panelp->mFractionalSize * panelp->getAutoResizeFactor()) / (total_visible_fraction);
				S32 delta = ll_round((F32)space_to_distribute * fraction_to_distribute);
				fraction_distributed += fraction_to_distribute;
				panelp->mTargetDim += delta;
				remaining_space -= delta;
			}
		}
	}

	// distribute any left over pixels to non-collapsed, visible panels
	for (LLLayoutPanel* panelp : mPanels)
	{
		if (remaining_space == 0) break;

		if (panelp->mAutoResize 
			&& !panelp->mCollapsed 
			&& panelp->getVisible())
		{
			S32 space_for_panel = remaining_space > 0 ? 1 : -1;
			panelp->mTargetDim += space_for_panel;
			remaining_space -= space_for_panel;
		}
	}

	F32 cur_pos = (mOrientation == HORIZONTAL) ? 0.f : (F32)getRect().getHeight();

	for (LLLayoutPanel* panelp : mPanels)
	{
		F32 panel_dim = llmax(panelp->getExpandedMinDim(), panelp->mTargetDim);
		F32 panel_visible_dim = panelp->getVisibleDim();

		LLRect panel_rect;
		if (mOrientation == HORIZONTAL)
		{
			panel_rect.setLeftTopAndSize(ll_round(cur_pos),
										getRect().getHeight(),
										ll_round(panel_dim),
										getRect().getHeight());
		}
		else
		{
			panel_rect.setLeftTopAndSize(0,
										ll_round(cur_pos),
										getRect().getWidth(),
										ll_round(panel_dim));
		}
		panelp->setIgnoreReshape(true);
		panelp->setShape(panel_rect);
		panelp->setIgnoreReshape(false);

		LLRect resize_bar_rect(panel_rect);

		F32 panel_spacing = (F32)mPanelSpacing * panelp->getVisibleAmount();
		if (mOrientation == HORIZONTAL)
		{
			resize_bar_rect.mLeft = panel_rect.mRight - mResizeBarOverlap;
			resize_bar_rect.mRight = panel_rect.mRight + (S32)(ll_round(panel_spacing)) + mResizeBarOverlap;

			cur_pos += panel_visible_dim + panel_spacing;
		}
		else //VERTICAL
		{
			resize_bar_rect.mTop = panel_rect.mBottom + mResizeBarOverlap;
			resize_bar_rect.mBottom = panel_rect.mBottom - (S32)(ll_round(panel_spacing)) - mResizeBarOverlap;

			cur_pos -= panel_visible_dim + panel_spacing;
		}
		panelp->mResizeBar->setShape(resize_bar_rect);
	}

	updateResizeBarLimits();

	// clear animation flag at end, since panel resizes will set it
	// and leave it set if there is any animation in progress
	mNeedsLayout = continue_animating;
} // end LLLayoutStack::updateLayout

LLLayoutPanel* LLLayoutStack::findEmbeddedPanel(LLPanel* panelp) const
{
	if (!panelp) return NULL;

	e_panel_list_t::const_iterator panel_it;
	for (LLLayoutPanel* p : mPanels)
	{
		if (p == panelp)
		{
			return p;
		}
	}
	return NULL;
}

LLLayoutPanel* LLLayoutStack::findEmbeddedPanelByName(const std::string& name) const
{
	LLLayoutPanel* result = NULL;

	for (LLLayoutPanel* p : mPanels)
	{
		if (p->getName() == name)
		{
			result = p;
			break;
		}
	}

	return result;
}

void LLLayoutStack::createResizeBar(LLLayoutPanel* panelp)
{
	for (LLLayoutPanel* lp : mPanels)
	{
		if (lp->mResizeBar == NULL)
		{
			LLResizeBar::Side side = (mOrientation == HORIZONTAL) ? LLResizeBar::RIGHT : LLResizeBar::BOTTOM;

			LLResizeBar::Params resize_params;
			resize_params.name("resize");
			resize_params.resizing_view(lp);
			resize_params.min_size(lp->getRelevantMinDim());
			resize_params.side(side);
			resize_params.snapping_enabled(false);
			LLResizeBar* resize_bar = LLUICtrlFactory::create<LLResizeBar>(resize_params);
			lp->mResizeBar = resize_bar;
			LLView::addChild(resize_bar, 0);
		}
	}
	// bring all resize bars to the front so that they are clickable even over the panels
	// with a bit of overlap
	for (e_panel_list_t::iterator panel_it = mPanels.begin(); panel_it != mPanels.end(); ++panel_it)
	{
		LLResizeBar* resize_barp = (*panel_it)->mResizeBar;
		sendChildToFront(resize_barp);
	}
}

// update layout stack animations, etc. once per frame
// NOTE: we use this to size world view based on animating UI, *before* we draw the UI
// we might still need to call updateLayout during UI draw phase, in case UI elements
// are resizing themselves dynamically
//static 
void LLLayoutStack::updateClass()
{
	for (instance_iter it = beginInstances(), it_end(endInstances()); it != it_end; ++it)
	{
		it->updateLayout();
		it->mAnimatedThisFrame = false;
	}
}

void LLLayoutStack::updateFractionalSizes()
{
	F32 total_resizable_dim = 0.f;

	for (LLLayoutPanel* panelp : mPanels)
	{
		if (panelp->mAutoResize)
		{
			total_resizable_dim += llmax(MIN_FRACTIONAL_SIZE, (F32)(panelp->getLayoutDim() - panelp->getRelevantMinDim()));
		}
	}

	for (LLLayoutPanel* panelp : mPanels)
	{
		if (panelp->mAutoResize)
		{
			F32 panel_resizable_dim = llmax(MIN_FRACTIONAL_SIZE, (F32)(panelp->getLayoutDim() - panelp->getRelevantMinDim()));
			panelp->mFractionalSize = panel_resizable_dim > 0.f 
				? llclamp(panel_resizable_dim / total_resizable_dim, MIN_FRACTIONAL_SIZE, MAX_FRACTIONAL_SIZE)
				: MIN_FRACTIONAL_SIZE;
			llassert(!std::isnan(panelp->mFractionalSize));
		}
	}

	normalizeFractionalSizes();
}


void LLLayoutStack::normalizeFractionalSizes()
{
	S32 num_auto_resize_panels = 0;
	F32 total_fractional_size = 0.f;
	
	for (LLLayoutPanel* panelp : mPanels)
	{
		if (panelp->mAutoResize)
		{
			total_fractional_size += panelp->mFractionalSize;
			num_auto_resize_panels++;
		}
	}

	if (total_fractional_size == 0.f)
	{ // equal distribution
		for (LLLayoutPanel* panelp : mPanels)
		{
			if (panelp->mAutoResize)
			{
				panelp->mFractionalSize = MAX_FRACTIONAL_SIZE / (F32)num_auto_resize_panels;
			}
		}
	}
	else
	{ // renormalize
		for (LLLayoutPanel* panelp : mPanels)
		{
			if (panelp->mAutoResize)
			{
				panelp->mFractionalSize /= total_fractional_size;
			}
		}
	}
}

bool LLLayoutStack::animatePanels()
{
	bool continue_animating = false;
	
	//
	// animate visibility
	//
	for (LLLayoutPanel* panelp : mPanels)
	{
		if (panelp->getVisible())
		{
			if (mAnimate && panelp->mVisibleAmt < 1.f)
			{
				if (!mAnimatedThisFrame)
				{
					panelp->mVisibleAmt = lerp(panelp->mVisibleAmt, 1.f, LLSmoothInterpolation::getInterpolant(mOpenTimeConstant));
					if (panelp->mVisibleAmt > 0.99f)
					{
						panelp->mVisibleAmt = 1.f;
					}
				}
				
				mAnimatedThisFrame = true;
				continue_animating = true;
			}
			else
			{
				if (panelp->mVisibleAmt != 1.f)
				{
					panelp->mVisibleAmt = 1.f;
					mAnimatedThisFrame = true;
				}
			}
		}
		else // not visible
		{
			if (mAnimate && panelp->mVisibleAmt > 0.f)
			{
				if (!mAnimatedThisFrame)
				{
					panelp->mVisibleAmt = lerp(panelp->mVisibleAmt, 0.f, LLSmoothInterpolation::getInterpolant(mCloseTimeConstant));
					if (panelp->mVisibleAmt < 0.001f)
					{
						panelp->mVisibleAmt = 0.f;
					}
				}

				continue_animating = true;
				mAnimatedThisFrame = true;
			}
			else
			{
				if (panelp->mVisibleAmt != 0.f)
				{
					panelp->mVisibleAmt = 0.f;
					mAnimatedThisFrame = true;
				}
			}
		}

		F32 collapse_state = panelp->mCollapsed ? 1.f : 0.f;
		if (panelp->mCollapseAmt != collapse_state)
		{
			if (mAnimate)
			{
				if (!mAnimatedThisFrame)
				{
					panelp->mCollapseAmt = lerp(panelp->mCollapseAmt, collapse_state, LLSmoothInterpolation::getInterpolant(mCloseTimeConstant));
				}
			
				if (llabs(panelp->mCollapseAmt - collapse_state) < 0.001f)
				{
					panelp->mCollapseAmt = collapse_state;
				}

				mAnimatedThisFrame = true;
				continue_animating = true;
			}
			else
			{
				panelp->mCollapseAmt = collapse_state;
				mAnimatedThisFrame = true;
			}
		}
	}

	if (mAnimatedThisFrame) mNeedsLayout = true;
	return continue_animating;
}

void LLLayoutStack::updatePanelRect( LLLayoutPanel* resized_panel, const LLRect& new_rect )
{
	S32 new_dim = (mOrientation == HORIZONTAL)
					? new_rect.getWidth()
					: new_rect.getHeight();
	S32 delta_panel_dim = new_dim - resized_panel->getVisibleDim();
	if (delta_panel_dim == 0) return;

	F32 total_visible_fraction = 0.f;
	F32 delta_auto_resize_headroom = 0.f;
	F32 old_auto_resize_headroom = 0.f;

	LLLayoutPanel* other_resize_panel = NULL;
	LLLayoutPanel* following_panel = NULL;

	for(LLLayoutPanel* panelp : boost::adaptors::reverse(mPanels))
	{
		if (panelp->mAutoResize)
		{
			old_auto_resize_headroom += (F32)(panelp->mTargetDim - panelp->getRelevantMinDim());
			if (panelp->getVisible() && !panelp->mCollapsed)
			{
				total_visible_fraction += panelp->mFractionalSize;
			}
		}

		if (panelp == resized_panel)
		{
			other_resize_panel = following_panel;
		}

		if (panelp->getVisible() && !panelp->mCollapsed)
		{
			following_panel = panelp;
		}
	}

	if (resized_panel->mAutoResize)
	{
		if (!other_resize_panel || !other_resize_panel->mAutoResize)
		{
			delta_auto_resize_headroom += delta_panel_dim;	
		}
	}
	else 
	{
		if (!other_resize_panel || other_resize_panel->mAutoResize)
		{
			delta_auto_resize_headroom -= delta_panel_dim;
		}
	}

	F32 fraction_given_up = 0.f;
	F32 fraction_remaining = 1.f;
	F32 new_auto_resize_headroom = old_auto_resize_headroom + delta_auto_resize_headroom;

	enum
	{
		BEFORE_RESIZED_PANEL,
		RESIZED_PANEL,
		NEXT_PANEL,
		AFTER_RESIZED_PANEL
	} which_panel = BEFORE_RESIZED_PANEL;

	for (LLLayoutPanel* panelp : mPanels)
	{
		if (!panelp->getVisible() || panelp->mCollapsed) 
		{
			if (panelp->mAutoResize) 
			{
				fraction_remaining -= panelp->mFractionalSize;
			}
			continue;
		}

		if (panelp == resized_panel)
		{
			which_panel = RESIZED_PANEL;
		}

		switch(which_panel)
		{
		case BEFORE_RESIZED_PANEL:
			if (panelp->mAutoResize)
			{	// freeze current size as fraction of overall auto_resize space
				F32 fractional_adjustment_factor = new_auto_resize_headroom == 0.f
													? 1.f
													: old_auto_resize_headroom / new_auto_resize_headroom;
				F32 new_fractional_size = llclamp(panelp->mFractionalSize * fractional_adjustment_factor,
													MIN_FRACTIONAL_SIZE,
													MAX_FRACTIONAL_SIZE);
				fraction_given_up -= new_fractional_size - panelp->mFractionalSize;
				fraction_remaining -= panelp->mFractionalSize;
				panelp->mFractionalSize = new_fractional_size;
				llassert(!std::isnan(panelp->mFractionalSize));
			}
			else
			{
				// leave non auto-resize panels alone
			}
			break;
		case RESIZED_PANEL:
			if (panelp->mAutoResize)
			{	// freeze new size as fraction
				F32 new_fractional_size = (new_auto_resize_headroom == 0.f)
					? MAX_FRACTIONAL_SIZE
					: llclamp(total_visible_fraction * (F32)(new_dim - panelp->getRelevantMinDim()) / new_auto_resize_headroom, MIN_FRACTIONAL_SIZE, MAX_FRACTIONAL_SIZE);
				fraction_given_up -= new_fractional_size - panelp->mFractionalSize;
				fraction_remaining -= panelp->mFractionalSize;
				panelp->mFractionalSize = new_fractional_size;
				llassert(!std::isnan(panelp->mFractionalSize));
			}
			else
			{	// freeze new size as original size
				panelp->mTargetDim = new_dim;
			}
			which_panel = NEXT_PANEL;
			break;
		case NEXT_PANEL:
			if (panelp->mAutoResize)
			{
				fraction_remaining -= panelp->mFractionalSize;
				if (resized_panel->mAutoResize)
				{
					panelp->mFractionalSize = llclamp(panelp->mFractionalSize + fraction_given_up, MIN_FRACTIONAL_SIZE, MAX_FRACTIONAL_SIZE);
					fraction_given_up = 0.f;
				}
				else
				{
					if (new_auto_resize_headroom < 1.f)
					{
						new_auto_resize_headroom = 1.f;
					}

					F32 new_fractional_size = llclamp(total_visible_fraction * (F32)(panelp->mTargetDim - panelp->getRelevantMinDim() + delta_auto_resize_headroom) 
														/ new_auto_resize_headroom,
													MIN_FRACTIONAL_SIZE,
													MAX_FRACTIONAL_SIZE);
					fraction_given_up -= new_fractional_size - panelp->mFractionalSize;
					panelp->mFractionalSize = new_fractional_size;
				}
			}
			else
			{
				panelp->mTargetDim -= delta_panel_dim;
			}
			which_panel = AFTER_RESIZED_PANEL;
			break;
		case AFTER_RESIZED_PANEL:
			if (panelp->mAutoResize && fraction_given_up != 0.f)
			{
				panelp->mFractionalSize = llclamp(panelp->mFractionalSize + (panelp->mFractionalSize / fraction_remaining) * fraction_given_up,
												MIN_FRACTIONAL_SIZE,
												MAX_FRACTIONAL_SIZE);
			}
		default:
			break;
		}
	}
	updateLayout();
	//normalizeFractionalSizes();
}

void LLLayoutStack::reshape(S32 width, S32 height, BOOL called_from_parent)
{
	mNeedsLayout = true;
	LLView::reshape(width, height, called_from_parent);
}

void LLLayoutStack::updateResizeBarLimits()
{
	LLLayoutPanel* previous_visible_panelp = NULL;
	for(LLLayoutPanel* visible_panelp : boost::adaptors::reverse(mPanels))
	{
		if (!visible_panelp->getVisible() || visible_panelp->mCollapsed)
		{
			visible_panelp->mResizeBar->setVisible(FALSE);
			continue;
		}

		// toggle resize bars based on panel visibility, resizability, etc
		if (previous_visible_panelp
			&& (visible_panelp->mUserResize || previous_visible_panelp->mUserResize)				// one of the pair is user resizable
			&& (visible_panelp->mAutoResize || visible_panelp->mUserResize)							// current panel is resizable
			&& (previous_visible_panelp->mAutoResize || previous_visible_panelp->mUserResize))		// previous panel is resizable
		{
			visible_panelp->mResizeBar->setVisible(TRUE);
			S32 previous_panel_headroom = previous_visible_panelp->getVisibleDim() - previous_visible_panelp->getRelevantMinDim();
			visible_panelp->mResizeBar->setResizeLimits(visible_panelp->getRelevantMinDim(), 
														visible_panelp->getVisibleDim() + previous_panel_headroom);
		}
		else
		{
			visible_panelp->mResizeBar->setVisible(FALSE);
		}

		previous_visible_panelp = visible_panelp;
	}
}

LLXMLNodePtr LLLayoutStack::getXML(bool save_children) const
{
	LLXMLNodePtr node = LLView::getXML();
	node->setName(LL_LAYOUT_STACK_TAG);

	if (mOrientation == HORIZONTAL)
	{
		node->createChild("orientation", TRUE)->setStringValue("horizontal");
	}
	else
	{
		node->createChild("orientation", TRUE)->setStringValue("vertical");
	}

	if (save_children)
	{
		LLView::child_list_const_reverse_iter_t rit;
		for (rit = getChildList()->rbegin(); rit != getChildList()->rend(); ++rit)
		{
			LLView* childp = *rit;

			if (childp->getSaveToXML())
			{
				LLXMLNodePtr xml_node = childp->getXML();

				if (xml_node->hasName(LL_PANEL_TAG))
				{
					xml_node->setName(LL_LAYOUT_PANEL_TAG);
				}

				node->addChild(xml_node);
			}
		}
	}

	return node;
}

//static 
LLView* LLLayoutStack::fromXML(LLXMLNodePtr node, LLView *parent, LLUICtrlFactory *factory)
{
	std::string orientation_string("vertical");
	node->getAttributeString("orientation", orientation_string);

	ELayoutOrientation orientation = VERTICAL;

	if (orientation_string == "horizontal")
	{
		orientation = HORIZONTAL;
	}
	else if (orientation_string == "vertical")
	{
		orientation = VERTICAL;
	}
	else
	{
		LL_WARNS() << "Unknown orientation " << orientation_string << ", using vertical" << LL_ENDL;
	}
	
	BOOL clip = false;
	BOOL animate = true;
	S32 border_size = RESIZE_BAR_HEIGHT;
	F32 open_time_constant = 0.02f;
	F32 close_time_constant = 0.02f;

	node->getAttributeBOOL("animate", animate);
	node->getAttributeBOOL("clip", clip);
	node->getAttributeS32("border_size", border_size);
	node->getAttributeF32("over_time_constant", open_time_constant);
	node->getAttributeF32("close_time_constant", close_time_constant);

	border_size = llmax(border_size,0);

	LLLayoutStack* layout_stackp = new LLLayoutStack(orientation,border_size,animate,clip,open_time_constant,close_time_constant,RESIZE_BAR_OVERLAP);

	std::string name("stack");
	node->getAttributeString("name", name);

	layout_stackp->setName(name);
	layout_stackp->initFromXML(node, parent);

	LLXMLNodePtr child;
	for (child = node->getFirstChild(); child.notNull(); child = child->getNextSibling())
	{
		std::string ctrl_type = child->getName()->mString;
		LLStringUtil::toLower(ctrl_type);
		child->createChild("orientation", TRUE)->setStringValue(orientation == HORIZONTAL ? "horizontal" : "vertical");
		LLWidgetClassRegistry::factory_func_t func = LLWidgetClassRegistry::getInstance()->getCreatorFunc(ctrl_type);

		if(func)
		{
			LLView* widget = func(child,layout_stackp,factory);
			if(widget)
				layout_stackp->addChild(widget);
		}
	}
	layout_stackp->updateLayout();

	return layout_stackp;
}

LLView* LLLayoutPanel::fromXML(LLXMLNodePtr node, LLView* parent, LLUICtrlFactory *factory)
{
	llassert_always(dynamic_cast<LLLayoutStack*>(parent)!=NULL);

	std::string name("layout panel");
	node->getAttributeString("name", name);

	LLLayoutPanel* panelp = NULL;
	LLPanel* factory_panelp = factory->createFactoryPanel(name);
	if(factory_panelp)
		llassert_always((panelp = dynamic_cast<LLLayoutPanel*>(factory_panelp)) != NULL);
	// Fall back on a default panel, if there was no special factory.
	if (!panelp)
	{
		// create a new panel without a border, by default
		panelp = new LLLayoutPanel(-1,TRUE,TRUE,LLRect());
		// for local registry callbacks; define in constructor, referenced in XUI or postBuild
		panelp->mCommitCallbackRegistrar.pushScope(); 
		panelp->mEnableCallbackRegistrar.pushScope();
		panelp->initPanelXML(node, parent, factory);
		panelp->mCommitCallbackRegistrar.popScope();
		panelp->mEnableCallbackRegistrar.popScope();
		
	}
	else
	{
		if(!factory->builtPanel(panelp))
		{
			// for local registry callbacks; define in constructor, referenced in XUI or postBuild
			panelp->mCommitCallbackRegistrar.pushScope(); 
			panelp->mEnableCallbackRegistrar.pushScope();
			panelp->initPanelXML(node, parent, factory);
			panelp->mCommitCallbackRegistrar.popScope();
			panelp->mEnableCallbackRegistrar.popScope();
		}
	}
	panelp->setOrigin(0, 0);

	return panelp;
}
