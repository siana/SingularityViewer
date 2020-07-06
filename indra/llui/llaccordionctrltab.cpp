/** 
 * @file LLAccordionCtrlTab.cpp
 * @brief Collapsible control implementation
 *
 * $LicenseInfo:firstyear=2009&license=viewerlgpl$
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

#include "linden_common.h"

#include "llaccordionctrltab.h"
#include "llaccordionctrl.h"

#include "lllocalcliprect.h"
#include "llscrollbar.h"
#include "lltextbox.h"
//#include "lltextutil.h"
#include "lluictrl.h"
#include "lluictrlfactory.h"

static const std::string DD_BUTTON_NAME = "dd_button";
static const std::string DD_TEXTBOX_NAME = "dd_textbox";
static const std::string DD_HEADER_NAME = "dd_header";

static const S32 HEADER_HEIGHT = 23;
static const S32 HEADER_IMAGE_LEFT_OFFSET = 5;
static const S32 HEADER_TEXT_LEFT_OFFSET = 30;
static const F32 AUTO_OPEN_TIME = 1.f;
static const S32 VERTICAL_MULTIPLE = 16;
static const S32 PARENT_BORDER_MARGIN = 5;

//static LLDefaultChildRegistry::Register<LLAccordionCtrlTab> t1("accordion_tab");
static LLRegisterWidget<LLAccordionCtrlTab> t1("accordion_tab");

class LLAccordionCtrlTab::LLAccordionCtrlTabHeader : public LLUICtrl
{
public:
	friend class LLUICtrlFactory;

	struct Params : public LLAccordionCtrlTab::Params //LLInitParam::Block<Params, LLAccordionCtrlTab::Params>
	{
		Params();
		Params(const LLAccordionCtrlTab::Params& p) : LLAccordionCtrlTab::Params(p) {}
	};

	LLAccordionCtrlTabHeader(const LLAccordionCtrlTabHeader::Params& p);
	
	virtual ~LLAccordionCtrlTabHeader();

	virtual void draw();

	virtual void reshape(S32 width, S32 height, BOOL called_from_parent = TRUE);

	virtual BOOL postBuild();

	std::string getTitle();
	void	setTitle(const std::string& title, const std::string& hl);

	void	setTitleFontStyle(std::string style);

	void	setTitleColor(LLUIColor);

	void	setSelected(bool is_selected) { mIsSelected = is_selected; }

	virtual void onMouseEnter(S32 x, S32 y, MASK mask);
	virtual void onMouseLeave(S32 x, S32 y, MASK mask);
	virtual BOOL handleKey(KEY key, MASK mask, BOOL called_from_parent);
	virtual BOOL handleDragAndDrop(S32 x, S32 y, MASK mask, BOOL drop,
								   EDragAndDropType cargo_type,
								   void* cargo_data,
								   EAcceptance* accept,
								   std::string& tooltip_msg);
private:

	LLTextBox* mHeaderTextbox;

	// Overlay images (arrows)
	LLPointer<LLUIImage> mImageCollapsed;
	LLPointer<LLUIImage> mImageExpanded;
	LLPointer<LLUIImage> mImageCollapsedPressed;
	LLPointer<LLUIImage> mImageExpandedPressed;

	// Background images
	LLPointer<LLUIImage> mImageHeader;
	LLPointer<LLUIImage> mImageHeaderOver;
	LLPointer<LLUIImage> mImageHeaderPressed;
	LLPointer<LLUIImage> mImageHeaderFocused;

	// style saved when applying it in setTitleFontStyle
	//U8/*LLStyle::Params*/			mStyleParams;

	LLUIColor mHeaderBGColor;

	bool mNeedsHighlight;
	bool mIsSelected;

	LLFrameTimer mAutoOpenTimer;
};

LLAccordionCtrlTab::LLAccordionCtrlTabHeader::Params::Params()
{
}

LLAccordionCtrlTab::LLAccordionCtrlTabHeader::LLAccordionCtrlTabHeader(
	const LLAccordionCtrlTabHeader::Params& p)
: LLUICtrl(p)
, mHeaderBGColor(p.header_bg_color())
, mNeedsHighlight(false)
, mIsSelected(false),
	mImageCollapsed(p.header_collapse_img),
	mImageCollapsedPressed(p.header_collapse_img_pressed),
	mImageExpanded(p.header_expand_img),
	mImageExpandedPressed(p.header_expand_img_pressed),
	mImageHeader(p.header_image),
	mImageHeaderOver(p.header_image_over),
	mImageHeaderPressed(p.header_image_pressed),
	mImageHeaderFocused(p.header_image_focused)
{
	mHeaderTextbox = new LLTextBox(DD_TEXTBOX_NAME, p.title(), 200, p.font(), false);
	mHeaderTextbox->setColor(p.header_text_color());
	mHeaderTextbox->setFollows(FOLLOWS_NONE);
	mHeaderTextbox->setFontShadow(LLFontGL::NO_SHADOW);
	mHeaderTextbox->setUseEllipses(true);
	mHeaderTextbox->setBackgroundVisible(false);
	addChild(mHeaderTextbox);
}

LLAccordionCtrlTab::LLAccordionCtrlTabHeader::~LLAccordionCtrlTabHeader()
{
}

BOOL LLAccordionCtrlTab::LLAccordionCtrlTabHeader::postBuild()
{
	return TRUE;
}

std::string LLAccordionCtrlTab::LLAccordionCtrlTabHeader::getTitle()
{
	if(mHeaderTextbox)
	{
		return mHeaderTextbox->getText();
	}
	else
	{
		return LLStringUtil::null;
	}
}

void LLAccordionCtrlTab::LLAccordionCtrlTabHeader::setTitle(const std::string& title, const std::string& hl)
{
	if(mHeaderTextbox)
	{
		mHeaderTextbox->setText(title);
		/*LLTextUtil::textboxSetHighlightedVal(
			mHeaderTextbox,
			mStyleParams,
			title,
			hl);*/
	}
}

void LLAccordionCtrlTab::LLAccordionCtrlTabHeader::setTitleFontStyle(std::string style)
{
	if (mHeaderTextbox)
	{
		mHeaderTextbox->setFontStyle(/*mStyleParams =*/ LLFontGL::getStyleFromString(style));
	}
}

void LLAccordionCtrlTab::LLAccordionCtrlTabHeader::setTitleColor(LLUIColor color)
{
	if(mHeaderTextbox)
	{
		mHeaderTextbox->setColor(color);
	}
}

void LLAccordionCtrlTab::LLAccordionCtrlTabHeader::draw()
{
	S32 width = getRect().getWidth();
	S32 height = getRect().getHeight();

	//F32 alpha = getCurrentTransparency(); // Singu TODO
	gl_rect_2d(0,0,width - 1 ,height - 1,mHeaderBGColor.get() /*% alpha*/,true);

	LLAccordionCtrlTab* parent = dynamic_cast<LLAccordionCtrlTab*>(getParent());
	bool collapsible = (parent && parent->getCollapsible());
	bool expanded = (parent && parent->getDisplayChildren());

	// Handle overlay images, if needed
	// Only show green "focus" background image if the accordion is open,
	// because the user's mental model of focus is that it goes away after
	// the accordion is closed.
	if (getParent()->hasFocus() || mIsSelected
		/*&& !(collapsible && !expanded)*/ // WHY??
		)
	{
		mImageHeaderFocused->draw(0,0,width,height);
	}
	else
	{
		mImageHeader->draw(0,0,width,height);
	}

	if(mNeedsHighlight)
	{
		mImageHeaderOver->draw(0,0,width,height);
	}
	

	if(collapsible)
	{
		LLPointer<LLUIImage> overlay_image;
		if(expanded)
		{
			overlay_image = mImageExpanded;
		}
		else
		{
			overlay_image = mImageCollapsed;
		}
		overlay_image->draw(HEADER_IMAGE_LEFT_OFFSET,
							(height - overlay_image->getHeight()) / 2);
	}
	
	LLUICtrl::draw();
}

void LLAccordionCtrlTab::LLAccordionCtrlTabHeader::reshape(S32 width, S32 height, BOOL called_from_parent /* = TRUE */)
{
	S32 header_height = mHeaderTextbox->getTextPixelHeight();

	LLRect textboxRect(HEADER_TEXT_LEFT_OFFSET,(height+header_height)/2 ,width,(height-header_height)/2);
	mHeaderTextbox->reshape(textboxRect.getWidth(), textboxRect.getHeight());
	mHeaderTextbox->setRect(textboxRect);

	if (mHeaderTextbox->getTextPixelWidth() > mHeaderTextbox->getRect().getWidth())
	{
		setToolTip(mHeaderTextbox->getText());
	}
	else
	{
		setToolTip(LLStringUtil::null);
	}
}

void LLAccordionCtrlTab::LLAccordionCtrlTabHeader::onMouseEnter(S32 x, S32 y, MASK mask)
{
	LLUICtrl::onMouseEnter(x, y, mask);
	mNeedsHighlight = true;
}
void LLAccordionCtrlTab::LLAccordionCtrlTabHeader::onMouseLeave(S32 x, S32 y, MASK mask)
{
	LLUICtrl::onMouseLeave(x, y, mask);
	mNeedsHighlight = false;
	mAutoOpenTimer.stop();
}
BOOL LLAccordionCtrlTab::LLAccordionCtrlTabHeader::handleKey(KEY key, MASK mask, BOOL called_from_parent)
{
	if ( ( key == KEY_LEFT || key == KEY_RIGHT) && mask == MASK_NONE)
	{
		return getParent()->handleKey(key, mask, called_from_parent);
	}
	return LLUICtrl::handleKey(key, mask, called_from_parent);
}
BOOL LLAccordionCtrlTab::LLAccordionCtrlTabHeader::handleDragAndDrop(S32 x, S32 y, MASK mask,
																	 BOOL drop,
																	 EDragAndDropType cargo_type,
																	 void* cargo_data,
																	 EAcceptance* accept,
																	 std::string& tooltip_msg)
{
	LLAccordionCtrlTab* parent = dynamic_cast<LLAccordionCtrlTab*>(getParent());

	if ( parent && !parent->getDisplayChildren() && parent->getCollapsible() && parent->canOpenClose() )
	{
		if (mAutoOpenTimer.getStarted())
		{
			if (mAutoOpenTimer.getElapsedTimeF32() > AUTO_OPEN_TIME)
			{
				parent->changeOpenClose(false);
				mAutoOpenTimer.stop();
				return TRUE;
			}
		}
		else
			mAutoOpenTimer.start();
	}

	return LLUICtrl::handleDragAndDrop(x, y, mask, drop, cargo_type,
									   cargo_data, accept, tooltip_msg);
}
LLAccordionCtrlTab::Params::Params()
	: title("title")
	,display_children("expanded", true)
	,header_height("header_height", HEADER_HEIGHT),
	min_width("min_width", 0),
	min_height("min_height", 0)
	,collapsible("collapsible", true)
	,header_bg_color("header_bg_color")
	,dropdown_bg_color("dropdown_bg_color")
	,header_visible("header_visible",true)
	,padding_left("padding_left",2)
	,padding_right("padding_right",2)
	,padding_top("padding_top",2)
	,padding_bottom("padding_bottom",2)
	,header_expand_img("header_expand_img")
	,header_expand_img_pressed("header_expand_img_pressed")
	,header_collapse_img("header_collapse_img")
	,header_collapse_img_pressed("header_collapse_img_pressed")
	,header_image("header_image")
	,header_image_over("header_image_over")
	,header_image_pressed("header_image_pressed")
	,header_image_focused("header_image_focused")
	,header_text_color("header_text_color")
	,fit_panel("fit_panel",true)
	,selection_enabled("selection_enabled", false)
{
	changeDefault(mouse_opaque, false);
}

LLAccordionCtrlTab::LLAccordionCtrlTab(const LLAccordionCtrlTab::Params&p)
	: LLUICtrl(p)
	,mDisplayChildren(p.display_children)
	,mCollapsible(p.collapsible)
	,mExpandedHeight(0)
	,mDropdownBGColor(p.dropdown_bg_color())
	,mHeaderVisible(p.header_visible)
	,mPaddingLeft(p.padding_left)
	,mPaddingRight(p.padding_right)
	,mPaddingTop(p.padding_top)
	,mPaddingBottom(p.padding_bottom)
	,mCanOpenClose(true)
	,mFitPanel(p.fit_panel)
	,mSelectionEnabled(p.selection_enabled)
	,mContainerPanel(NULL)
	,mScrollbar(NULL)
{
	mStoredOpenCloseState = false;
	mWasStateStored = false;
	
	mDropdownBGColor = LLColor4::white;
	LLAccordionCtrlTabHeader::Params headerParams(p);
	headerParams.name(DD_HEADER_NAME);
	headerParams.title(p.title);
	mHeader = LLUICtrlFactory::create<LLAccordionCtrlTabHeader>(headerParams);
	addChild(mHeader, 1);

	LLFocusableElement::setFocusReceivedCallback(boost::bind(&LLAccordionCtrlTab::selectOnFocusReceived, this));

	if (!p.selection_enabled)
	{
		LLFocusableElement::setFocusLostCallback(boost::bind(&LLAccordionCtrlTab::deselectOnFocusLost, this));
	}

	reshape(100, 200,FALSE);
}

LLAccordionCtrlTab::~LLAccordionCtrlTab()
{
}


void LLAccordionCtrlTab::setDisplayChildren(bool display)
{
	mDisplayChildren = display;
	LLRect rect = getRect();

	rect.mBottom = rect.mTop - (getDisplayChildren() ? 
		mExpandedHeight : HEADER_HEIGHT);
	setRect(rect);

	if(mContainerPanel)
		mContainerPanel->setVisible(getDisplayChildren());

	if(mDisplayChildren)
	{
		adjustContainerPanel();
	}
	else
	{
		if(mScrollbar)
			mScrollbar->setVisible(false);
	}

}

void LLAccordionCtrlTab::reshape(S32 width, S32 height, BOOL called_from_parent /* = TRUE */)
{
	LLRect headerRect;

	headerRect.setLeftTopAndSize(
		0,height,width,HEADER_HEIGHT);
	mHeader->setRect(headerRect);
	mHeader->reshape(headerRect.getWidth(), headerRect.getHeight());

	if(!mDisplayChildren)
		return;

	LLRect childRect;

	childRect.setLeftTopAndSize(
		getPaddingLeft(),
		height - getHeaderHeight() - getPaddingTop(),
		width - getPaddingLeft() - getPaddingRight(), 
		height - getHeaderHeight() - getPaddingTop() - getPaddingBottom() );

	adjustContainerPanel(childRect);
}

void LLAccordionCtrlTab::changeOpenClose(bool is_open)
{
	if(is_open)
		mExpandedHeight = getRect().getHeight();

	setDisplayChildren(!is_open);
	reshape(getRect().getWidth(), getRect().getHeight(), FALSE);
	if (mCommitSignal)
	{
		(*mCommitSignal)(this, getDisplayChildren());
	}
}

/*void LLAccordionCtrlTab::onVisibilityChange(BOOL new_visibility)
{
	LLUICtrl::onVisibilityChange(new_visibility);

	notifyParent(LLSD().with("child_visibility_change", new_visibility));
}*/

BOOL LLAccordionCtrlTab::handleMouseDown(S32 x, S32 y, MASK mask)
{
	if(mCollapsible && mHeaderVisible && mCanOpenClose)
	{
		if(y >= (getRect().getHeight() - HEADER_HEIGHT) )
		{
			mHeader->setFocus(true);
			changeOpenClose(getDisplayChildren());

			//reset stored state
			mWasStateStored = false;
			return TRUE;
		}
	}
	return LLUICtrl::handleMouseDown(x,y,mask);
}

BOOL LLAccordionCtrlTab::handleMouseUp(S32 x, S32 y, MASK mask)
{
	return LLUICtrl::handleMouseUp(x,y,mask);
}

boost::signals2::connection LLAccordionCtrlTab::setDropDownStateChangedCallback(commit_callback_t cb)
{
	return setCommitCallback(cb);
}

bool LLAccordionCtrlTab::addChild(LLView* child, S32 tab_group)
{
	if(DD_HEADER_NAME != child->getName())
	{
		reshape(child->getRect().getWidth() , child->getRect().getHeight() + HEADER_HEIGHT );
		mExpandedHeight = getRect().getHeight();
	}

	bool res = LLUICtrl::addChild(child, tab_group);

	if(DD_HEADER_NAME != child->getName())
	{
		if(!mCollapsible)
			setDisplayChildren(true);
		else
			setDisplayChildren(getDisplayChildren());	
	}

	if (!mContainerPanel)
		mContainerPanel = findContainerView();

	return res;
}

void LLAccordionCtrlTab::setAccordionView(LLView* panel)
{
	addChild(panel,0);
}

std::string LLAccordionCtrlTab::getTitle() const
{
	if (mHeader)
	{
		return mHeader->getTitle();
	}
	else
	{
		return LLStringUtil::null;
	}
}

void LLAccordionCtrlTab::setTitle(const std::string& title, const std::string& hl)
{
	if (mHeader)
	{
		mHeader->setTitle(title, hl);
	}
}

void LLAccordionCtrlTab::setTitleFontStyle(std::string style)
{
	if (mHeader)
	{
		mHeader->setTitleFontStyle(style);
	}
}

void LLAccordionCtrlTab::setTitleColor(LLUIColor color)
{
	if (mHeader)
	{
		mHeader->setTitleColor(color);
	}
}

boost::signals2::connection LLAccordionCtrlTab::setFocusReceivedCallback(const focus_signal_t::slot_type& cb)
{
	if (mHeader)
	{
		return mHeader->setFocusReceivedCallback(cb);
	}
	return boost::signals2::connection();
}

boost::signals2::connection LLAccordionCtrlTab::setFocusLostCallback(const focus_signal_t::slot_type& cb)
{
	if (mHeader)
	{
		return mHeader->setFocusLostCallback(cb);
	}
	return boost::signals2::connection();
}

void LLAccordionCtrlTab::setSelected(bool is_selected)
{
	if (mHeader)
	{
		mHeader->setSelected(is_selected);
	}
}

LLView*	LLAccordionCtrlTab::findContainerView()
{
	for(child_list_const_iter_t it = getChildList()->begin(); 
		getChildList()->end() != it; ++it)
	{
		LLView* child = *it;
		if(DD_HEADER_NAME == child->getName())
			continue;
		if(!child->getVisible())
			continue;
		return child;
	}
	return NULL;
}

void LLAccordionCtrlTab::selectOnFocusReceived()
{
	if (getParent()) // A parent may not be set if tabs are added dynamically.
		getParent()->notifyParent(LLSD().with("action", "select_current"));
}

void LLAccordionCtrlTab::deselectOnFocusLost()
{
	if(getParent()) // A parent may not be set if tabs are added dynamically.
	{
		getParent()->notifyParent(LLSD().with("action", "deselect_current"));
	}

}

S32 LLAccordionCtrlTab::getHeaderHeight()
{
	return mHeaderVisible?HEADER_HEIGHT:0; 
}

void LLAccordionCtrlTab::setHeaderVisible(bool value) 
{
	if(mHeaderVisible == value)
		return;
	mHeaderVisible = value;
	if(mHeader)
		mHeader->setVisible(value);
	reshape(getRect().getWidth(), getRect().getHeight(), FALSE);
};

//virtual
BOOL LLAccordionCtrlTab::postBuild()
{
	if(mHeader)
		mHeader->setVisible(mHeaderVisible);
	
	static LLUICachedControl<S32> scrollbar_size ("UIScrollbarSize", 0);

	LLRect scroll_rect;
	scroll_rect.setOriginAndSize( 
		getRect().getWidth() - scrollbar_size,
		1,
		scrollbar_size,
		getRect().getHeight() - 1);

	mContainerPanel = findContainerView();

	if(!mFitPanel)
	{
		mScrollbar = new LLScrollbar("scrollable vertical", scroll_rect, LLScrollbar::VERTICAL, getRect().getHeight(), 0, getRect().getHeight(), boost::bind(&LLAccordionCtrlTab::onScrollPosChangeCallback, this, _1, _2), VERTICAL_MULTIPLE);
		mScrollbar->setFollows(FOLLOWS_RIGHT | FOLLOWS_TOP | FOLLOWS_BOTTOM);
		LLView::addChild( mScrollbar );
		mScrollbar->setFollows(FOLLOWS_RIGHT|FOLLOWS_TOP|FOLLOWS_BOTTOM);

		mScrollbar->setVisible(false);
	}

	if(mContainerPanel)
		mContainerPanel->setVisible(mDisplayChildren);

	return LLUICtrl::postBuild();
}
bool	LLAccordionCtrlTab::notifyChildren	(const LLSD& info)
{
	if(info.has("action"))
	{
		std::string str_action = info["action"];
		if(str_action == "store_state")
		{
			storeOpenCloseState();
			return true;
		}
		if(str_action == "restore_state")
		{
			restoreOpenCloseState();
			return true;
		}
	}	
	return LLUICtrl::notifyChildren(info);
}

S32	LLAccordionCtrlTab::notifyParent(const LLSD& info)
{
	if(info.has("action"))
	{
		std::string str_action = info["action"];
		if(str_action == "size_changes")
		{
			//
			S32 height = info["height"];
			height = llmax(height,10) + HEADER_HEIGHT + getPaddingTop() + getPaddingBottom();
			
			mExpandedHeight = height;
			
			if(isExpanded())
			{
				LLRect panel_rect = getRect();
				panel_rect.setLeftTopAndSize( panel_rect.mLeft, panel_rect.mTop, panel_rect.getWidth(), height);
				reshape(getRect().getWidth(),height);
				setRect(panel_rect);
			}
			
			//LLAccordionCtrl should rearrange accordion tab if one of accordion change its size
			if (getParent()) // A parent may not be set if tabs are added dynamically.
				getParent()->notifyParent(info);
			return 1;
		}
		else if(str_action == "select_prev") 
		{
			showAndFocusHeader();
			return 1;
		}
	}
	else if (info.has("scrollToShowRect"))
	{
		LLAccordionCtrl* parent = dynamic_cast<LLAccordionCtrl*>(getParent());
		if (parent && parent->getFitParent())
		{
			//	EXT-8285 ('No attachments worn' text appears at the bottom of blank 'Attachments' accordion)
			//	The problem was in passing message "scrollToShowRect" IN LLAccordionCtrlTab::notifyParent
			//	FROM child LLScrollContainer TO parent LLAccordionCtrl with "it_parent" set to true.

			//	It is wrong notification for parent accordion which leads to recursive call of adjustContainerPanel
			//	As the result of recursive call of adjustContainerPanel we got LLAccordionCtrlTab
			//	that reshaped and re-sized with different rectangles.

			//	LLAccordionCtrl has own scrollContainer and LLAccordionCtrlTab has own scrollContainer
			//	both should handle own scroll container's event.
			//	So, if parent accordion "fit_parent" accordion tab should handle its scroll container events itself.

			return 1;
		}

		if (!getDisplayChildren())
		{
			// Don't pass scrolling event further if our contents are invisible (STORM-298).
			return 1;
		}
	}

	return LLUICtrl::notifyParent(info);
}

S32 LLAccordionCtrlTab::notify(const LLSD& info)
{
	if(info.has("action"))
	{
		std::string str_action = info["action"];
		if(str_action == "select_first")
		{
			showAndFocusHeader();
			return 1;
		}
		else if( str_action == "select_last" )
		{
			if(getDisplayChildren() == false)
			{
				showAndFocusHeader();
			}
			else
			{
				LLView* view = getAccordionView();
				if(view)
					view->notify(LLSD().with("action","select_last"));
			}
		}
	}
	return 0;
}

BOOL LLAccordionCtrlTab::handleKey(KEY key, MASK mask, BOOL called_from_parent)
{
	if( !mHeader->hasFocus() )
		return LLUICtrl::handleKey(key, mask, called_from_parent);

	if ( (key == KEY_RETURN )&& mask == MASK_NONE)
	{
		changeOpenClose(getDisplayChildren());
		return TRUE;
	}

	if ( (key == KEY_ADD || key == KEY_RIGHT)&& mask == MASK_NONE)
	{
		if(getDisplayChildren() == false)
		{
			changeOpenClose(getDisplayChildren());
			return TRUE;
		}
	}
	if ( (key == KEY_SUBTRACT || key == KEY_LEFT)&& mask == MASK_NONE)
	{
		if(getDisplayChildren() == true)
		{
			changeOpenClose(getDisplayChildren());
			return TRUE;
		}
	}

	if ( key == KEY_DOWN && mask == MASK_NONE)
	{
		//if collapsed go to the next accordion
		if(getDisplayChildren() == false)
			//we processing notifyParent so let call parent directly
			getParent()->notifyParent(LLSD().with("action","select_next"));
		else
		{
			getAccordionView()->notify(LLSD().with("action","select_first"));
		}
		return TRUE;
	}

	if ( key == KEY_UP && mask == MASK_NONE)
	{
		//go to the previous accordion

		//we processing notifyParent so let call parent directly
		getParent()->notifyParent(LLSD().with("action","select_prev"));
		return TRUE;
	}

	return LLUICtrl::handleKey(key, mask, called_from_parent);
}

void LLAccordionCtrlTab::showAndFocusHeader()
{
	mHeader->setFocus(true);
	mHeader->setSelected(mSelectionEnabled);

	LLRect screen_rc;
	LLRect selected_rc = mHeader->getRect();
	localRectToScreen(selected_rc, &screen_rc);

	// This call to notifyParent() is intended to deliver "scrollToShowRect" command
	// to the parent LLAccordionCtrl so by calling it from the direct parent of this
	// accordion tab (assuming that the parent is an LLAccordionCtrl) the calls chain
	// is shortened and messages from inside the collapsed tabs are avoided.
	// See STORM-536.
	getParent()->notifyParent(LLSD().with("scrollToShowRect",screen_rc.getValue()));
}
void    LLAccordionCtrlTab::storeOpenCloseState()
{
	if(mWasStateStored)
		return;
	mStoredOpenCloseState = getDisplayChildren();
	mWasStateStored = true;
}

void   LLAccordionCtrlTab::restoreOpenCloseState()
{
	if(!mWasStateStored)
		return;
	if(getDisplayChildren() != mStoredOpenCloseState)
	{
		changeOpenClose(getDisplayChildren());
	}
	mWasStateStored = false;
}

void LLAccordionCtrlTab::adjustContainerPanel	()
{
	S32 width = getRect().getWidth();
	S32 height = getRect().getHeight();

	LLRect child_rect;
	child_rect.setLeftTopAndSize(
		getPaddingLeft(),
		height - getHeaderHeight() - getPaddingTop(),
		width - getPaddingLeft() - getPaddingRight(), 
		height - getHeaderHeight() - getPaddingTop() - getPaddingBottom() );

	adjustContainerPanel(child_rect);
}

void LLAccordionCtrlTab::adjustContainerPanel(const LLRect& child_rect)
{
	if(!mContainerPanel)
		return; 

	if(!mFitPanel)
	{
		show_hide_scrollbar(child_rect);
		updateLayout(child_rect);
	}
	else
	{
		mContainerPanel->reshape(child_rect.getWidth(),child_rect.getHeight());
		mContainerPanel->setRect(child_rect);
	}
}

S32 LLAccordionCtrlTab::getChildViewHeight()
{
	if(!mContainerPanel)
		return 0;
	return mContainerPanel->getRect().getHeight();
}

void LLAccordionCtrlTab::show_hide_scrollbar(const LLRect& child_rect)
{
	if(getChildViewHeight() > child_rect.getHeight() )
		showScrollbar(child_rect);
	else
		hideScrollbar(child_rect);
}
void LLAccordionCtrlTab::showScrollbar(const LLRect& child_rect)
{
	if(!mContainerPanel || !mScrollbar)
		return;
	bool was_visible = mScrollbar->getVisible();
	mScrollbar->setVisible(true);
	
	static LLUICachedControl<S32> scrollbar_size ("UIScrollbarSize", 0);

	{
		ctrlSetLeftTopAndSize(mScrollbar,child_rect.getWidth()-scrollbar_size, 
			child_rect.getHeight()-PARENT_BORDER_MARGIN, 
			scrollbar_size, 
			child_rect.getHeight()-2*PARENT_BORDER_MARGIN);
	}

	LLRect orig_rect = mContainerPanel->getRect();

	mScrollbar->setPageSize(child_rect.getHeight());
	mScrollbar->setDocParams(orig_rect.getHeight(),mScrollbar->getDocPos());
	
	if(was_visible)
	{
		S32 scroll_pos = llmin(mScrollbar->getDocPos(), orig_rect.getHeight() - child_rect.getHeight() - 1);
		mScrollbar->setDocPos(scroll_pos);
	}
	else//shrink child panel
	{
		updateLayout(child_rect);
	}
	
}

void	LLAccordionCtrlTab::hideScrollbar( const LLRect& child_rect )
{
	if(!mContainerPanel || !mScrollbar)
		return;

	if(mScrollbar->getVisible() == false)
		return;
	mScrollbar->setVisible(false);
	mScrollbar->setDocPos(0);

	//shrink child panel
	updateLayout(child_rect);
}

void	LLAccordionCtrlTab::onScrollPosChangeCallback(S32, LLScrollbar*)
{
	LLRect child_rect;

	S32 width = getRect().getWidth();
	S32 height = getRect().getHeight();

	child_rect.setLeftTopAndSize(
		getPaddingLeft(),
		height - getHeaderHeight() - getPaddingTop(),
		width - getPaddingLeft() - getPaddingRight(), 
		height - getHeaderHeight() - getPaddingTop() - getPaddingBottom() );

	updateLayout(child_rect);
}

void LLAccordionCtrlTab::drawChild(const LLRect& root_rect,LLView* child)
{
	if (child && child->getVisible() && child->getRect().isValid())
	{
		LLRect screen_rect;
		localRectToScreen(child->getRect(),&screen_rect);
		
		if ( root_rect.overlaps(screen_rect)  /*&& LLUI::sDirtyRect.overlaps(screen_rect)*/)// Singu TODO: LLUI::sDirtyRect
		{
			gGL.matrixMode(LLRender::MM_MODELVIEW);
			LLUI::pushMatrix();
			{
				LLUI::translate((F32)child->getRect().mLeft, (F32)child->getRect().mBottom);
				child->draw();

			}
			LLUI::popMatrix();
		}
	}
}

void LLAccordionCtrlTab::draw()
{
	if(mFitPanel)
		LLUICtrl::draw();
	else
	{
		LLRect root_rect = getRootView()->getRect();
		drawChild(root_rect,mHeader);
		drawChild(root_rect,mScrollbar );
		{
			LLRect child_rect;

			S32 width = getRect().getWidth();
			S32 height = getRect().getHeight();

			child_rect.setLeftTopAndSize(
				getPaddingLeft(),
				height - getHeaderHeight() - getPaddingTop(),
				width - getPaddingLeft() - getPaddingRight(), 
				height - getHeaderHeight() - getPaddingTop() - getPaddingBottom() );

			LLLocalClipRect clip(child_rect);
			drawChild(root_rect,mContainerPanel);
		}
	}
}

void	LLAccordionCtrlTab::updateLayout	( const LLRect& child_rect )
{
	LLView*	child = getAccordionView();
	if(!mContainerPanel)
		return;

	S32 panel_top = child_rect.getHeight();
	S32 panel_width = child_rect.getWidth();

	static LLUICachedControl<S32> scrollbar_size ("UIScrollbarSize", 0);
	if(mScrollbar && mScrollbar->getVisible() != false)
	{
		panel_top+=mScrollbar->getDocPos();
		panel_width-=scrollbar_size;
	}

	//set sizes for first panels and dragbars
	LLRect panel_rect = child->getRect();
	ctrlSetLeftTopAndSize(mContainerPanel,child_rect.mLeft,panel_top,panel_width,panel_rect.getHeight());
}
void LLAccordionCtrlTab::ctrlSetLeftTopAndSize(LLView* panel, S32 left, S32 top, S32 width, S32 height)
{
	if(!panel)
		return;
	LLRect panel_rect = panel->getRect();
	panel_rect.setLeftTopAndSize( left, top, width, height);
	panel->reshape( width, height, 1);
	panel->setRect(panel_rect);
}
BOOL LLAccordionCtrlTab::handleToolTip(S32 x, S32 y, std::string& msg, LLRect* sticky_rect)
{
	//header may be not the first child but we need to process it first
	if(y >= (getRect().getHeight() - HEADER_HEIGHT - HEADER_HEIGHT/2) )
	{
		//inside tab header
		//fix for EXT-6619
		mHeader->handleToolTip(x, y, msg, sticky_rect);
		return TRUE;
	}
	return LLUICtrl::handleToolTip(x, y, msg, sticky_rect);
}
BOOL LLAccordionCtrlTab::handleScrollWheel		( S32 x, S32 y, S32 clicks )
{
	if( LLUICtrl::handleScrollWheel(x,y,clicks))
	{
		return TRUE;
	}
	if( mScrollbar && mScrollbar->getVisible() && mScrollbar->handleScrollWheel( 0, 0, clicks ) )
	{
		return TRUE;
	}
	return FALSE;
}

//static 
LLView* LLAccordionCtrlTab::fromXML(LLXMLNodePtr node, LLView* parent, LLUICtrlFactory* factory)
{
	Params p;
	// Singu TODO: Widgets folder for defaults instead of shoving into params here where noted
	if (node->hasAttribute("title"))
	{
		std::string title;
		node->getAttributeString("title", title);
		p.title(title);
	}

	if (node->hasAttribute("expanded"))
	{
		bool display_children;
		node->getAttribute_bool("expanded", display_children);
		p.display_children(display_children);
	}

	if (node->hasAttribute("header_height"))
	{
		S32 header_height;
		node->getAttributeS32("header_height", header_height);
		p.header_height(header_height);
	}

	if (node->hasAttribute("min_width"))
	{
		S32 min_width;
		node->getAttributeS32("min_width", min_width);
		p.min_width(min_width);
	}

	if (node->hasAttribute("min_width"))
	{
		S32 min_height;
		node->getAttributeS32("min_height", min_height);
		p.min_height(min_height);
	}

	if (node->hasAttribute("collapsible"))
	{
		bool collapsible;
		node->getAttribute_bool("collapsible", collapsible);
		p.collapsible(collapsible);
	}

	if (node->hasAttribute("header_bg_color"))
	{
		LLColor4 color;
		node->getAttributeColor("header_bg_color", color);
		p.header_bg_color(color);
	}
	else // widget
	{
		p.header_bg_color(LLUI::sColorsGroup->getColor("ButtonUnselectedBgColor")); // was DkGray2
	}

	if (node->hasAttribute("dropdown_bg_color"))
	{
		LLColor4 color;
		node->getAttributeColor("dropdown_bg_color", color);
		p.dropdown_bg_color(color);
	}

	if (node->hasAttribute("header_visible"))
	{
		bool header_visible;
		node->getAttribute_bool("header_visible", header_visible);
		p.header_visible(header_visible);
	}

	if (node->hasAttribute("padding_left"))
	{
		S32 padding_left;
		node->getAttributeS32("padding_left", padding_left);
		p.padding_left(padding_left);
	}

	if (node->hasAttribute("padding_right"))
	{
		S32 padding_right;
		node->getAttributeS32("padding_right", padding_right);
		p.padding_right(padding_right);
	}

	if (node->hasAttribute("padding_top"))
	{
		S32 padding_top;
		node->getAttributeS32("padding_top", padding_top);
		p.padding_top(padding_top);
	}

	if (node->hasAttribute("padding_bottom"))
	{
		S32 padding_bottom;
		node->getAttributeS32("padding_bottom", padding_bottom);
		p.padding_bottom(padding_bottom);
	}

	if (node->hasAttribute("header_expand_img"))
	{
		std::string image;
		node->getAttributeString("header_expand_img", image);
		p.header_expand_img.name(image);
	}
	else // widget
	{
		p.header_expand_img.name("Accordion_ArrowOpened_Off");
	}

	if (node->hasAttribute("header_expand_img_pressed"))
	{
		std::string image;
		node->getAttributeString("header_expand_img_pressed", image);
		p.header_expand_img_pressed.name(image);
	}
	else // widget
	{
		p.header_expand_img_pressed.name("Accordion_ArrowOpened_Press");
	}

	if (node->hasAttribute("header_collapse_img"))
	{
		std::string image;
		node->getAttributeString("header_collapse_img", image);
		p.header_collapse_img.name(image);
	}
	else // widget
	{
		p.header_collapse_img.name("Accordion_ArrowClosed_Off");
	}

	if (node->hasAttribute("header_collapse_img_pressed"))
	{
		std::string image;
		node->getAttributeString("header_collapse_img_pressed", image);
		p.header_collapse_img_pressed.name(image);
	}
	else // widget
	{
		p.header_collapse_img_pressed.name("Accordion_ArrowClosed_Press");
	}

	if (node->hasAttribute("header_image"))
	{
		std::string image;
		node->getAttributeString("header_image", image);
		p.header_image.name(image);
	}
	else // widget
	{
		p.header_image.name("Accordion_Off");
	}

	if (node->hasAttribute("header_image_over"))
	{
		std::string image;
		node->getAttributeString("header_image_over", image);
		p.header_image_over.name(image);
	}
	else // widget
	{
		p.header_image_over.name("Accordion_Over");
	}

	if (node->hasAttribute("header_image_pressed"))
	{
		std::string image;
		node->getAttributeString("header_image_pressed", image);
		p.header_image_pressed.name(image);
	}
	else // widget
	{
		p.header_image_pressed.name("Accordion_Press");
	}

	if (node->hasAttribute("header_image_focused"))
	{
		std::string image;
		node->getAttributeString("header_image_focused", image);
		p.header_image_focused.name(image);
	}
	else // widget
	{
		p.header_image_focused.name("Accordion_Selected");
	}

	if (node->hasAttribute("header_text_color"))
	{
		LLColor4 color;
		node->getAttributeColor("header_text_color", color);
		p.header_text_color(color);
	}
	else // widget
	{
		p.header_text_color(LLUI::sColorsGroup->getColor("ButtonLabelColor")); // AccordionHeaderTextColor
	}

	if (node->hasAttribute("fit_panel"))
	{
		bool fit_panel;
		node->getAttribute_bool("fit_panel", fit_panel);
		p.fit_panel(fit_panel);
	}

	if (node->hasAttribute("selection_enabled"))
	{
		bool selection_enabled;
		node->getAttribute_bool("selection_enabled", selection_enabled);
		p.selection_enabled(selection_enabled);
	}

	if (node->hasAttribute("font"))
	{
		std::string font;
		node->getAttributeString("font", font);
		p.font.name(font);
	}
	else // widget
	{
		p.font.name("SansSerif");
	}

	LLAccordionCtrlTab* ctrl = new LLAccordionCtrlTab(p);
	ctrl->initFromXML(node, parent);
	return ctrl;
}
