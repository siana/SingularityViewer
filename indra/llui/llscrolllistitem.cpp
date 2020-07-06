/**
 * @file llscrolllistitem.cpp
 * @brief Scroll lists are composed of rows (items), each of which
 * contains columns (cells).
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

#include "linden_common.h"

#include "llscrolllistitem.h"
#include "llview.h"


//---------------------------------------------------------------------------
// LLScrollListItem
//---------------------------------------------------------------------------

LLScrollListItem::LLScrollListItem( const Params& p )
:	mSelected(FALSE),
	mEnabled(p.enabled),
	mFiltered(false),
	mUserdata(p.userdata),
	mItemValue(p.value),
	mColumns()
{
}


LLScrollListItem::~LLScrollListItem()
{
	std::for_each(mColumns.begin(), mColumns.end(), DeletePointer());
}

void LLScrollListItem::addColumn(const LLScrollListCell::Params& p)
{
	mColumns.push_back(LLScrollListCell::create(p));
}

void LLScrollListItem::setNumColumns(S32 columns)
{
	S32 prev_columns = mColumns.size();
	if (columns < prev_columns)
	{
		std::for_each(mColumns.begin()+columns, mColumns.end(), DeletePointer());
	}

	mColumns.resize(columns);

	for (S32 col = prev_columns; col < columns; ++col)
	{
		mColumns[col] = NULL;
	}
}

void LLScrollListItem::setColumn( S32 column, LLScrollListCell *cell )
{
	if (column < (S32)mColumns.size())
	{
		delete mColumns[column];
		mColumns[column] = cell;
	}
	else
	{
		LL_ERRS() << "LLScrollListItem::setColumn: bad column: " << column << LL_ENDL;
	}
}


S32 LLScrollListItem::getNumColumns() const
{
	return mColumns.size();
}

LLScrollListCell* LLScrollListItem::getColumn(const S32 i) const
{
	if (0 <= i && i < (S32)mColumns.size())
	{
		return mColumns[i];
	}
	return NULL;
}

std::string LLScrollListItem::getContentsCSV() const
{
	std::string ret;

	S32 count = getNumColumns();
	for (S32 i=0; i<count; ++i)
	{
		ret += getColumn(i)->getValue().asString();
		if (i < count-1)
		{
			ret += ", ";
		}
	}

	return ret;
}


bool LLScrollListItem::draw(const U32 pass, const LLRect& rect, const LLColor4& fg_color, const LLColor4& bg_color, const LLColor4& highlight_color, S32 column_padding)
{
	const U32 num_cols = (U32)getNumColumns();

	// draw background rect
	if (pass == 0)
	{
		gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
		gl_rect_2d(rect, bg_color);
		return true;
	}
	// Draw column (pass - 1)
	else if (pass <= num_cols)
	{
		LLScrollListCell* cur_cell = getColumn(pass - 1);
		if (!cur_cell)
		{
			return false;
		}
		// Two ways a cell could be hidden
		else if (cur_cell->getWidth() && cur_cell->getVisible())
		{
			// Iterate over cells to the left and calculate offset.
			S32 offset = rect.mLeft;
			for (U32 i = 0; i < (pass - 1); ++i)
			{
				LLScrollListCell* cell = getColumn(i);
				if (!cell)
				{
					return false; // Shouldn't happen.
				}
				if (cell->getVisible() && cell->getWidth())
				{
					// Only apply padding if cell is visible and has width.
					offset += cell->getWidth() + column_padding;
				}
			}
			// Do not draw if not on screen.
			// Only care about horizontal bounds. This draw call wont even occur if this row is entirely off screen.
			LLRect clip_rect = LLUI::getRootView()->getRect();
			if (LLGLState<GL_SCISSOR_TEST>::isEnabled())
			{
				LLRect scissor = gGL.getScissor();
				scissor.mLeft /= LLUI::getScaleFactor().mV[VX];
				scissor.mTop /= LLUI::getScaleFactor().mV[VY];
				scissor.mRight /= LLUI::getScaleFactor().mV[VX];
				scissor.mBottom /= LLUI::getScaleFactor().mV[VY];
				clip_rect.intersectWith(scissor);
			}
			if (offset + LLFontGL::sCurOrigin.mX >= clip_rect.mRight)
			{
				// Went off the right edge of the screen. Don't bother with any more columns.
				return false;
			}
			// Draw if not off the left edge of screen. If it is off the left edge, still return true if other colums remain.
			if (offset + LLFontGL::sCurOrigin.mX + cur_cell->getWidth() > clip_rect.mLeft)
			{
				LLUI::pushMatrix();
				{
					LLUI::translate((F32)offset, (F32)rect.mBottom);
					cur_cell->draw(fg_color, highlight_color);
				}
				LLUI::popMatrix();
			}
		}
		return pass != getNumColumns();
	}
	return false;
}

