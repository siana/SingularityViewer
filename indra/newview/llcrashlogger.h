/** 
* @file llcrashlogger.h
* @brief Crash Logger Definition
*
* $LicenseInfo:firstyear=2003&license=viewerlgpl$
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
#ifndef LLCRASHLOGGER_H
#define LLCRASHLOGGER_H

#include <vector>

#include "linden_common.h"

#include "llapp.h"
#include "llsd.h"
#include "llcontrol.h"

class LLCrashLogger
{
public:
	LLCrashLogger();
	virtual ~LLCrashLogger();
	S32 loadCrashBehaviorSetting();
    bool readDebugFromXML(LLSD& dest, const std::string& filename );
	void gatherFiles();
    void mergeLogs( LLSD src_sd );

	virtual void gatherPlatformSpecificFiles() {}
	bool saveCrashBehaviorSetting(S32 crash_behavior);
    bool sendCrashLog(std::string dump_dir);
	LLSD constructPostData();
	void setUserText(const std::string& text) { mCrashInfo["UserNotes"] = text; }
	S32 getCrashBehavior() { return mCrashBehavior; }
	bool readMinidump(std::string minidump_path);
	void checkCrashDump();

protected:
	S32 mCrashBehavior;
	BOOL mCrashInPreviousExec;
	std::map<std::string, std::string> mFileMap;
	std::string mGridName;
	std::string mProductName;
	LLSD mCrashInfo;
	std::string mCrashHost;
	LLSD mDebugLog;
};

#endif //LLCRASHLOGGER_H
