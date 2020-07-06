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
#include "llsdserialize.h"

static std::string get_log_dir_file(const std::string& filename)
{
	return gDirUtilp->getExpandedFilename(LL_PATH_PER_ACCOUNT_CHAT_LOGS, filename);
}

//static
std::string LLLogChat::makeLogFileNameInternal(std::string filename)
{
	static const LLCachedControl<bool> with_date(gSavedPerAccountSettings, "LogFileNamewithDate");
	if (with_date)
	{
		time_t now; 
		time(&now); 
		std::array<char, 100> dbuffer;
		static const LLCachedControl<std::string> local_chat_date_format(gSavedPerAccountSettings, "LogFileLocalChatDateFormat", "-%Y-%m-%d");
		static const LLCachedControl<std::string> ims_date_format(gSavedPerAccountSettings, "LogFileIMsDateFormat", "-%Y-%m");
		strftime(dbuffer.data(), dbuffer.size(), (filename == "chat" ? local_chat_date_format : ims_date_format)().c_str(), localtime(&now));
		filename += dbuffer.data();
	}
	cleanFileName(filename);
	return get_log_dir_file(filename + ".txt");
}

bool LLLogChat::migrateFile(const std::string& old_name, const std::string& filename)
{
	std::string oldfile = makeLogFileNameInternal(old_name);
	if (!LLFile::isfile(oldfile)) return false; // An old file by this name doesn't exist

	if (LLFile::isfile(filename)) // A file by the new name also exists, but wasn't being tracked yet
	{
		auto&& new_untracked_log = llifstream(filename);
		auto&& tracked_log = llofstream(oldfile, llofstream::out|llofstream::app);
		// Append new to old and find out if it failed
		bool failed = !(tracked_log << new_untracked_log.rdbuf());
		// Close streams
		new_untracked_log.close();
		tracked_log.close();
		if (failed || LLFile::remove(filename)) // Delete the untracked new file so that reclaiming its name won't fail
			return true; // We failed to remove it or update the old file, let's just use the new file and leave the old one alone
	}

	LLFile::rename(oldfile, filename); // Move the existing file to the new name
	return true; // Report success
}

static LLSD sIDMap;

static std::string get_ids_map_file() { return get_log_dir_file("ids_to_names.json"); }
void LLLogChat::initializeIDMap()
{
	const auto map_file = get_ids_map_file();
	bool write = true; // Do we want to write back to map_file?
	if (LLFile::isfile(map_file)) // If we've already made this file, load our map from it
	{
		if (auto&& fstr = llifstream(map_file))
		{
			LLSDSerialize::fromNotation(sIDMap, fstr, LLSDSerialize::SIZE_UNLIMITED);
			fstr.close();
		}
		write = false; // Don't write what we just read
	}

	if (gCacheName) // Load what we can from name cache to initialize or update the map and its file
	{
		bool empty = sIDMap.size() == 0; // Opt out of searching the map for IDs we added if we started with none
		for (const auto& r : gCacheName->getReverseMap()) // For every name id pair
		{
			const auto id = r.second.asString();
			const auto& name = r.first;
			const auto filename = makeLogFileNameInternal(name);
			bool id_known = !empty && sIDMap.has(id); // Is this ID known?
			if (id_known ? name != sIDMap[id].asStringRef() // If names don't match
					&& migrateFile(sIDMap[id].asStringRef(), filename) // Do we need to migrate an existing log?
				: LLFile::isfile(filename)) // Otherwise if there's a log file for them but they're not in the map yet
			{
				if (id_known) write = true; // We updated, write
				sIDMap[id] = name; // Add them to the map
			}
		}

		if (write)
		if (auto&& fstr = llofstream(map_file))
		{
			LLSDSerialize::toPrettyNotation(sIDMap, fstr);
			fstr.close();
		}
	}

}

//static
std::string LLLogChat::makeLogFileName(const std::string& username, const LLUUID& id)
{
	const auto name = username.empty() ? id.asString() : username; // Fall back on ID if the grid sucks and we have no name
	std::string filename = makeLogFileNameInternal(name);
	if (id.notNull() && !LLFile::isfile(filename)) // No existing file by this user's current name, check for possible file rename
	{
		auto& entry = sIDMap[id.asString()];
		const bool empty = !entry.size();
		if (empty || entry != name) // If we haven't seen this entry yet, or the name is different than we remember
		{
			if (empty) // We didn't see this entry on load
			{
				// Ideally, we would look up the old names here via server request
				// In lieu of that, our reverse cache has old names and new names that we've gained since our initialization of the ID map
				for (const auto& r : gCacheName->getReverseMap())
					if (r.second == id && migrateFile(r.first, filename))
						break;
			}
			else migrateFile(entry.asStringRef(), filename); // We've seen this entry before, migrate old file if it exists

			entry = name; // Update the entry to point to the new name

			if (auto&& fstr = llofstream(get_ids_map_file())) // Write back to our map file
			{
				LLSDSerialize::toPrettyNotation(sIDMap, fstr);
				fstr.close();
			}
		}
	}
	return filename;
}

void LLLogChat::cleanFileName(std::string& filename)
{
	std::string invalidChars = "\"\'\\/?*:<>|[]{}~"; // Cannot match glob or illegal filename chars
	S32 position = filename.find_first_of(invalidChars);
	while (position != filename.npos)
	{
		filename[position] = '_';
		position = filename.find_first_of(invalidChars, position);
	}
}

static void time_format(std::string& out, const char* fmt, const std::tm* time)
{
	typedef typename std::vector<char, boost::alignment::aligned_allocator<char, 1>> vec_t;
	static thread_local vec_t charvector(1024); // Evolves into charveleon
	#define format_the_time() std::strftime(charvector.data(), charvector.capacity(), fmt, time)
	const auto smallsize(charvector.capacity());
	const auto size = format_the_time();
	if (size < 0)
	{
		LL_ERRS() << "Formatting time failed, code " << size << ". String hint: " << out << '/' << fmt << LL_ENDL;
	}
	else if (static_cast<vec_t::size_type>(size) >= smallsize) // Resize if we need more space
	{
		charvector.resize(1+size); // Use the String Stone
		format_the_time();
	}
	#undef format_the_time
	out.assign(charvector.data());
}


std::string LLLogChat::timestamp(bool withdate)
{
	// Convert to Pacific, based on server's opinion of whether
	// it's daylight savings time there.
	auto time = utc_to_pacific_time(time_corrected(), gPacificDaylightTime);

	static const LLCachedControl<bool> withseconds("SecondsInLog");
	static const LLCachedControl<std::string> date("ShortDateFormat");
	static const LLCachedControl<std::string> shorttime("ShortTimeFormat");
	static const LLCachedControl<std::string> longtime("LongTimeFormat");
	std::string text = "[";
	if (withdate) text += date() + ' ';
	text += (withseconds ? longtime : shorttime)() + "]  ";

	time_format(text, text.data(), time);

	return text;
}


//static
void LLLogChat::saveHistory(const std::string& name, const LLUUID& id, const std::string& line)
{
	if(name.empty() && id.isNull())
	{
		LL_INFOS() << "Filename is Empty!" << LL_ENDL;
		return;
	}

	LLFILE* fp = LLFile::fopen(LLLogChat::makeLogFileName(name, id), "a"); 		/*Flawfinder: ignore*/
	if (!fp)
	{
		LL_INFOS() << "Couldn't open chat history log!" << LL_ENDL;
	}
	else
	{
		fprintf(fp, "%s\n", line.c_str());
		
		fclose (fp);
	}
}

static long const LOG_RECALL_BUFSIZ = 2048;

void LLLogChat::loadHistory(const std::string& name, const LLUUID& id, std::function<void (ELogLineType, const std::string&)> callback)
{
	if (name.empty() && id.isNull())
	{
		LL_WARNS() << "filename is empty!" << LL_ENDL;
	}
	else while(1)	// So we can use break.
	{
		// The number of lines to return.
		static const LLCachedControl<U32> lines("LogShowHistoryLines", 32);
		if (lines == 0) break;

		// Open the log file.
		LLFILE* fptr = LLFile::fopen(makeLogFileName(name, id), "rb");
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
			// strip newline chars from the end of the string
			for (S32 i = strlen(buffer) - 1; i >= 0 && (buffer[i] == '\r' || buffer[i] == '\n'); --i)
				buffer[i] = '\0';
			callback(LOG_LINE, buffer);
		}

		fclose(fptr);
		callback(LOG_END, LLStringUtil::null);
		return;
	}
	callback(LOG_EMPTY, LLStringUtil::null);
}
