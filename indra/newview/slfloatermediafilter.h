/** 
 * @file slfloatermediafilter.h
 * @brief The SLFloaterMediaFilter class declaration
 *
 * $LicenseInfo:firstyear=2011&license=viewergpl$
 * 
 * Copyright (c) 2011, Sione Lomu
 * with debugging and improvements by Henri Beauchamp
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

#ifndef LL_SLFLOATERMEDIAFILTER_H
#define LL_SLFLOATERMEDIAFILTER_H

#include "llfloater.h"

class LLScrollListCtrl;

class SLFloaterMediaFilter : public LLFloater, public LLFloaterSingleton<SLFloaterMediaFilter>
{
	friend class LLUISingleton<SLFloaterMediaFilter, VisibilityPolicy<LLFloater> >;
public:
	SLFloaterMediaFilter(const LLSD& key = LLSD());
	BOOL postBuild();
	virtual void draw();
	virtual ~SLFloaterMediaFilter();

	void setDirty();

	void onShowIPs();
	void onWhitelistAdd();
	void onWhitelistRemove();
	void onBlacklistAdd();
	void onBlacklistRemove();
	void onCommitDomain();

private:
	bool mIsWhitelist;
	bool mShowIPs;
	LLScrollListCtrl* mWhitelistSLC;
	LLScrollListCtrl* mBlacklistSLC;
	bool mIsDirty;
};
#endif
