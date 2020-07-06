/** 
* @file lllocalcliprect.cpp
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

#include "lllocalcliprect.h"

#include "llfontgl.h"
#include "llui.h"

/*static*/ std::stack<LLRect> LLScreenClipRect::sClipRectStack;


LLScreenClipRect::LLScreenClipRect(const LLRect& rect, BOOL enabled)
:	mScissorState(enabled),
	mEnabled(enabled),
	mRootScissorRect(gGL.getScissor())
{
	if (mEnabled)
	{
		pushClipRect(rect);
	}
}

LLScreenClipRect::~LLScreenClipRect()
{
	if (mEnabled)
	{
		popClipRect();
	}
}

void LLScreenClipRect::pushClipRect(const LLRect& rect)
{
	LLRect combined_clip_rect = rect;
	if (!sClipRectStack.empty())
	{
		LLRect top = sClipRectStack.top();
		combined_clip_rect.intersectWith(top);

		if(combined_clip_rect.isEmpty())
		{
			// avoid artifacts where zero area rects show up as lines
			combined_clip_rect = LLRect::null;
		}
	}
	
	sClipRectStack.push(combined_clip_rect);
	updateScissorRegion();
}

void LLScreenClipRect::popClipRect()
{
	sClipRectStack.pop();
	if (!sClipRectStack.empty())
	{
		updateScissorRegion();
	}
	else
	{
		gGL.setScissor(mRootScissorRect);
	}
}

// static
void LLScreenClipRect::updateScissorRegion()
{
	LLRect rect = sClipRectStack.top();
	stop_glerror();
	S32 x,y,w,h;
	x = llfloor(rect.mLeft * LLUI::getScaleFactor().mV[VX]);
	y = llfloor(rect.mBottom * LLUI::getScaleFactor().mV[VY]);
	w = llmax(0, llceil(rect.getWidth() * LLUI::getScaleFactor().mV[VX])) + 1;
	h = llmax(0, llceil(rect.getHeight() * LLUI::getScaleFactor().mV[VY])) + 1;
	gGL.setScissor( x,y,w,h );
	stop_glerror();
}

//---------------------------------------------------------------------------
// LLLocalClipRect
//---------------------------------------------------------------------------
LLLocalClipRect::LLLocalClipRect(const LLRect& rect, BOOL enabled /* = TRUE */)
:	LLScreenClipRect(LLRect(rect.mLeft + LLFontGL::sCurOrigin.mX, 
					rect.mTop + LLFontGL::sCurOrigin.mY, 
					rect.mRight + LLFontGL::sCurOrigin.mX, 
					rect.mBottom + LLFontGL::sCurOrigin.mY), enabled)
{}

LLLocalClipRect::~LLLocalClipRect()
{}
