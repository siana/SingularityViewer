/** 
 * @file lllogchat.h
 * @brief LLFloaterChat class definition
 *
 * $LicenseInfo:firstyear=2002&license=viewergpl$
 * 
 * Copyright (c) 2002-2009, Linden Research, Inc.
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


#ifndef LL_LLLOGCHAT_H
#define LL_LLLOGCHAT_H

#include <string>

class LLLogChat
{
public:
	// status values for callback function
	enum ELogLineType {
		LOG_EMPTY,
		LOG_LINE,
		LOG_END
	};
	static void initializeIDMap();
	static std::string timestamp(bool withdate = false);
	static std::string makeLogFileName(const std::string& name, const LLUUID& id);
	static void saveHistory(const std::string& name, const LLUUID& id, const std::string& line);
	static void loadHistory(const std::string& name, const LLUUID& id,
		                    std::function<void (ELogLineType, const std::string&)> callback);
private:
	static std::string makeLogFileNameInternal(std::string filename);
	static bool migrateFile(const std::string& old_name, const std::string& filename);
	static void cleanFileName(std::string& filename);
};

#endif
