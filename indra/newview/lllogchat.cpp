/** 
 * @file lllogchat.cpp
 * @brief LLLogChat class implementation
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

#include "llviewerprecompiledheaders.h"

#include <ctime>
#include "lllogchat.h"
#include "llappviewer.h"
#include "llfloaterchat.h"


//static
std::string LLLogChat::makeLogFileName(std::string filename)
{
	if (gSavedPerAccountSettings.getBOOL("LogFileNamewithDate"))
	{
		time_t now; 
		time(&now); 
		char dbuffer[100];               /* Flawfinder: ignore */
		if (filename == "chat") 
		{ 
			static const LLCachedControl<std::string> local_chat_date_format(gSavedPerAccountSettings, "LogFileLocalChatDateFormat", "-%Y-%m-%d");
			strftime(dbuffer, 100, local_chat_date_format().c_str(), localtime(&now));
		} 
		else 
		{ 
			static const LLCachedControl<std::string> ims_date_format(gSavedPerAccountSettings, "LogFileIMsDateFormat", "-%Y-%m");
			strftime(dbuffer, 100, ims_date_format().c_str(), localtime(&now));
		} 
		filename += dbuffer; 
	}
	filename = cleanFileName(filename);
	filename = gDirUtilp->getExpandedFilename(LL_PATH_PER_ACCOUNT_CHAT_LOGS,filename);
	filename += ".txt";
	return filename;
}

std::string LLLogChat::cleanFileName(std::string filename)
{
	std::string invalidChars = "\"\'\\/?*:<>|[]{}~"; // Cannot match glob or illegal filename chars
	S32 position = filename.find_first_of(invalidChars);
	while (position != filename.npos)
	{
		filename[position] = '_';
		position = filename.find_first_of(invalidChars, position);
	}
	return filename;
}

std::string LLLogChat::timestamp(bool withdate)
{
	time_t utc_time;
	utc_time = time_corrected();

	// There's only one internal tm buffer.
	struct tm* timep;

	// Convert to Pacific, based on server's opinion of whether
	// it's daylight savings time there.
	timep = utc_to_pacific_time(utc_time, gPacificDaylightTime);

	static LLCachedControl<bool> withseconds("SecondsInLog");
	std::string text;
	if (withdate)
		if (withseconds)
			text = llformat("[%d/%02d/%02d %02d:%02d:%02d]  ", (timep->tm_year-100)+2000, timep->tm_mon+1, timep->tm_mday, timep->tm_hour, timep->tm_min, timep->tm_sec);
		else
			text = llformat("[%d/%02d/%02d %02d:%02d]  ", (timep->tm_year-100)+2000, timep->tm_mon+1, timep->tm_mday, timep->tm_hour, timep->tm_min);
	else
		if (withseconds)
			text = llformat("[%02d:%02d:%02d]  ", timep->tm_hour, timep->tm_min, timep->tm_sec);
		else
			text = llformat("[%02d:%02d]  ", timep->tm_hour, timep->tm_min);

	return text;
}


//static
void LLLogChat::saveHistory(std::string const& filename, std::string line)
{
	if(!filename.size())
	{
		llinfos << "Filename is Empty!" << llendl;
		return;
	}

	LLFILE* fp = LLFile::fopen(LLLogChat::makeLogFileName(filename), "a"); 		/*Flawfinder: ignore*/
	if (!fp)
	{
		llinfos << "Couldn't open chat history log!" << llendl;
	}
	else
	{
		fprintf(fp, "%s\n", line.c_str());
		
		fclose (fp);
	}
}

static long const LOG_RECALL_BUFSIZ = 2048;

void LLLogChat::loadHistory(std::string const& filename , void (*callback)(ELogLineType, std::string, void*), void* userdata)
{
	bool filename_empty = filename.empty();
	if (filename_empty)
	{
		llwarns << "filename is empty!" << llendl;
	}
	else while(1)	// So we can use break.
	{
		// The number of lines to return.
		static const LLCachedControl<U32> lines("LogShowHistoryLines", 32);
		if (lines == 0) break;

		// Open the log file.
		LLFILE* fptr = LLFile::fopen(makeLogFileName(filename), "rb");
		if (!fptr) break;

		// Set pos to point to the last character of the file, if any.
		if (fseek(fptr, 0, SEEK_END)) break;
		long pos = ftell(fptr) - 1;
		if (pos < 0) break;

		char buffer[LOG_RECALL_BUFSIZ];
		bool error = false;
		U32 nlines = 0;
		while (pos > 0 && nlines < lines)
		{
			// Read the LOG_RECALL_BUFSIZ characters before pos.
			size_t size = llmin(LOG_RECALL_BUFSIZ, pos);
			pos -= size;
			fseek(fptr, pos, SEEK_SET);
			size_t len = fread(buffer, 1, size, fptr);
			error = len != size;
			if (error) break;
			// Count the number of newlines in it and set pos to the beginning of the first line to return when we found enough.
			for (char const* p = buffer + size - 1; p >= buffer; --p)
			{
				if (*p == '\n')
				{
					if (++nlines == lines)
					{
						pos += p - buffer + 1;
						break;
					}
				}
			}
		}
		if (error)
		{
			fclose(fptr);
			break;
		}

		// Set the file pointer at the first line to return.
		fseek(fptr, pos, SEEK_SET);

		// Read lines from the file one by one until we reach the end of the file.
		while (fgets(buffer, LOG_RECALL_BUFSIZ, fptr))
		{
		  size_t len = strlen(buffer);
		  int i = len - 1;
		  while (i >= 0 && (buffer[i] == '\r' || buffer[i] == '\n')) // strip newline chars from the end of the string
		  {
			  buffer[i] = '\0';
			  i--;
		  }
		  callback(LOG_LINE, buffer, userdata);
		}

		fclose(fptr);
		callback(LOG_END, LLStringUtil::null, userdata);
		return;
	}
	callback(LOG_EMPTY, LLStringUtil::null, userdata);
}

