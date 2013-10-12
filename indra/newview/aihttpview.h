/**
 * @file aihttpview.h
 * @brief Declaration for AIHTTPView.
 *
 * Copyright (c) 2013, Aleric Inglewood.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * There are special exceptions to the terms and conditions of the GPL as
 * it is applied to this Source Code. View the full text of the exception
 * in the file doc/FLOSS-exception.txt in this software distribution.
 *
 * CHANGELOG
 *   and additional copyright holders.
 *
 *   28/05/2013
 *   Initial version, written by Aleric Inglewood @ SL
 */

#ifndef AIHTTPVIEW_H
#define AIHTTPVIEW_H

#include "llcontainerview.h"
#include <vector>

class AIGLHTTPHeaderBar;
class AIServiceBar;
struct CreateServiceBar;

class AIHTTPView : public LLContainerView
{
	friend class AIGLHTTPHeaderBar;
	friend class AIServiceBar;
	friend struct CreateServiceBar;

  public:
	AIHTTPView(AIHTTPView::Params const& p);
	~AIHTTPView();

	/*virtual*/ void draw(void);
	/*virtual*/ BOOL handleMouseUp(S32 x, S32 y, MASK mask);
	/*virtual*/ BOOL handleKey(KEY key, MASK mask, BOOL called_from_parent);

	U32 updateColumn(U32 col, U32 start);
	void setWidth(S32 width) { mWidth = width; }

  private:
	AIGLHTTPHeaderBar* mGLHTTPHeaderBar;
	std::vector<AIServiceBar*> mServiceBars;
	std::vector<U32> mStartColumn;
	size_t mMaxBandwidthPerService;
	S32 mWidth;

	static U64 sTime_40ms;

  public:
	static U64 getTime_40ms(void) { return sTime_40ms; }
	static void toggle_visibility(void* user_data);
};

extern AIHTTPView *gHttpView;

#endif // AIHTTPVIEW_H
