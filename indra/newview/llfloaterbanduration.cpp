/**
* @file llfloaterbanduration.cpp
*
* $LicenseInfo:firstyear=2004&license=viewerlgpl$
* Second Life Viewer Source Code
* Copyright (C) 2018, Linden Research, Inc.
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

#include "llviewerprecompiledheaders.h"
#include "llfloaterbanduration.h"

#include "llcombobox.h"
#include "llspinctrl.h"
#include "lluictrlfactory.h"

LLFloaterBanDuration::LLFloaterBanDuration(const LLSD& target) : LLFloater()
{
	LLUICtrlFactory::instance().buildFloater(this, "floater_ban_duration.xml");
}

BOOL LLFloaterBanDuration::postBuild()
{
    childSetAction("ok_btn", boost::bind(&LLFloaterBanDuration::onClickBan, this));
    childSetAction("cancel_btn", boost::bind(&LLFloaterBanDuration::close, this, false));

    auto combo = getChild<LLUICtrl>("ban_duration_combo");
    combo->setCommitCallback(boost::bind(&LLFloaterBanDuration::onClickCombo, this, _2, getChild<LLUICtrl>("ban_duration")));
    combo->onCommit();

    return TRUE;
}

LLFloaterBanDuration* LLFloaterBanDuration::show(select_callback_t callback, const uuid_vec_t& ids)
{
    LLFloaterBanDuration* floater = showInstance();
    if (!floater)
    {
        LL_WARNS() << "Cannot instantiate ban duration floater" << LL_ENDL;
        return nullptr;
    }

    floater->mSelectionCallback = callback;
    floater->mAvatar_ids = ids;

    return floater;
}

void LLFloaterBanDuration::onClickCombo(const LLSD& val, LLUICtrl* duration)
{
    duration->setEnabled(val.asInteger() != 0);
}

void LLFloaterBanDuration::onClickBan()
{
    if (mSelectionCallback)
    {
        S32 time = 0;
	if (auto type = getChild<LLUICtrl>("ban_duration_combo")->getValue().asInteger())
        {
            LLSpinCtrl* duration_spin = getChild<LLSpinCtrl>("ban_duration");
            if (duration_spin)
            {
                time = (duration_spin->getValue().asInteger() * 3600);
		if (type > 1)
		{
			time *= type == 2 ? 24 : type == 3 ? 168 : 730;
		}
		time += LLDate::now().secondsSinceEpoch();
            }
        }
        mSelectionCallback(mAvatar_ids, time);
    }
    close();
}

