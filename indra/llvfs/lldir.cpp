
/** 
 * @file lldir.cpp
 * @brief implementation of directory utilities base class
 *
 * $LicenseInfo:firstyear=2002&license=viewerlgpl$
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

#if !LL_WINDOWS
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#else
#include <direct.h>
#endif

#include "lldir.h"

#include "llerror.h"
#include "lltimer.h"	// ms_sleep()
#include "lluuid.h"

#include "lldiriterator.h"
#include "stringize.h"
#include <boost/filesystem.hpp>
#include <boost/foreach.hpp>
#include <boost/range/begin.hpp>
#include <boost/range/end.hpp>
#include <boost/assign/list_of.hpp>
#include <boost/bind.hpp>
#include <boost/ref.hpp>
#include <algorithm>

using boost::assign::list_of;
using boost::assign::map_list_of;

#if LL_WINDOWS
#include "lldir_win32.h"
LLDir_Win32 gDirUtil;
#elif LL_DARWIN
#include "lldir_mac.h"
LLDir_Mac gDirUtil;
#elif LL_SOLARIS
#include "lldir_solaris.h"
LLDir_Solaris gDirUtil;
#else
#include "lldir_linux.h"
LLDir_Linux gDirUtil;
#endif

LLDir *gDirUtilp = (LLDir *)&gDirUtil;

static const char* const empty = "";
std::string LLDir::sDumpDir = "";
LLDir::LLDir()
:	mAppName(""),
	mExecutablePathAndName(""),
	mExecutableFilename(""),
	mExecutableDir(""),
	mAppRODataDir(""),
	mOSUserDir(""),
	mOSUserAppDir(""),
	mLindenUserDir(""),
	mOSCacheDir(""),
	mCAFile(""),
	mTempDir(""),
	mDirDelimiter("/") // fallback to forward slash if not overridden
{
}

LLDir::~LLDir()
{
}

std::vector<std::string> LLDir::getFilesInDir(const std::string &dirname)
{
    //Returns a vector of fullpath filenames.
    
    boost::filesystem::path p (dirname);
    std::vector<std::string> v;
    
    if (exists(p))
    {
        if (is_directory(p))
        {
            boost::filesystem::directory_iterator end_iter;
            for (boost::filesystem::directory_iterator dir_itr(p);
                 dir_itr != end_iter;
                 ++dir_itr)
            {
                if (boost::filesystem::is_regular_file(dir_itr->status()))
                {
                    v.push_back(dir_itr->path().filename().string());
                }
            }
        }
    }
    return v;
}   
            
S32 LLDir::deleteFilesInDir(const std::string &dirname, const std::string &mask)
{
	S32 count = 0;
	std::string filename; 
	std::string fullpath;
	S32 result;

	// File masks starting with "/" will match nothing, so we consider them invalid.
	if (LLStringUtil::startsWith(mask, getDirDelimiter()))
	{
		llwarns << "Invalid file mask: " << mask << llendl;
		llassert(!"Invalid file mask");
	}

	try
	{
		LLDirIterator iter(dirname, mask);
		while (iter.next(filename))
		{
			fullpath = add(dirname, filename);

			if(LLFile::isdir(fullpath))
			{
				// skipping directory traversal filenames
				count++;
				continue;
			}

			S32 retry_count = 0;
			while (retry_count < 5)
			{
				if (0 != LLFile::remove(fullpath))
				{
					retry_count++;
					result = errno;
					llwarns << "Problem removing " << fullpath << " - errorcode: "
						<< result << " attempt " << retry_count << llendl;

					if(retry_count >= 5)
					{
						llwarns << "Failed to remove " << fullpath << llendl ;
						return count ;
					}

					ms_sleep(100);
				}
				else
				{
					if (retry_count)
					{
						llwarns << "Successfully removed " << fullpath << llendl;
					}
					break;
				}			
			}
			count++;
		}
	}
	catch(...)
	{
		llwarns << "Unable to remove some files from " + dirname  << llendl;
	}

	return count;
}

U32 LLDir::deleteDirAndContents(const std::string& dir_name)
{
	//Removes the directory and its contents.  Returns number of files removed.
	// Singu Note: boost::filesystem throws exceptions
	S32 res = 0;

	try 
	{
		res = boost::filesystem::remove_all(dir_name);
	}
	catch(const boost::filesystem::filesystem_error& e)
	{
		llwarns << "boost::filesystem::remove_all(\"" + dir_name + "\") failed: '" + e.code().message() + "'" << llendl;
	}

	return res;
}

const std::string LLDir::findFile(const std::string &filename, 
						   const std::string& searchPath1, 
						   const std::string& searchPath2, 
						   const std::string& searchPath3) const
{
	std::vector<std::string> search_paths;
	search_paths.push_back(searchPath1);
	search_paths.push_back(searchPath2);
	search_paths.push_back(searchPath3);
	return findFile(filename, search_paths);
}

const std::string LLDir::findFile(const std::string& filename, const std::vector<std::string> search_paths) const
{
	std::vector<std::string>::const_iterator search_path_iter;
	for (search_path_iter = search_paths.begin();
		search_path_iter != search_paths.end();
		++search_path_iter)
	{
		if (!search_path_iter->empty())
		{
			std::string filename_and_path = (*search_path_iter);
			if (!filename.empty())
			{
				filename_and_path += getDirDelimiter() + filename;
			}
			if (fileExists(filename_and_path))
			{
				return filename_and_path;
			}
		}
	}
	return "";
}


const std::string &LLDir::getExecutablePathAndName() const
{
	return mExecutablePathAndName;
}

const std::string &LLDir::getExecutableFilename() const
{
	return mExecutableFilename;
}

const std::string &LLDir::getExecutableDir() const
{
	return mExecutableDir;
}

const std::string &LLDir::getWorkingDir() const
{
	return mWorkingDir;
}

const std::string &LLDir::getAppName() const
{
	return mAppName;
}

const std::string &LLDir::getAppRODataDir() const
{
	return mAppRODataDir;
}

const std::string &LLDir::getOSUserDir() const
{
	return mOSUserDir;
}

const std::string &LLDir::getOSUserAppDir() const
{
	return mOSUserAppDir;
}

const std::string &LLDir::getLindenUserDir(bool empty_ok) const
{
	llassert(empty_ok || !mLindenUserDir.empty());
	return mLindenUserDir;
}

const std::string &LLDir::getChatLogsDir() const
{
	return mChatLogsDir;
}

void LLDir::setDumpDir( const std::string& path )
{
    LLDir::sDumpDir = path;
    if (! sDumpDir.empty() && sDumpDir.rbegin() == mDirDelimiter.rbegin() )
    {
        sDumpDir.erase(sDumpDir.size() -1);
    }
}

const std::string &LLDir::getDumpDir() const
{
    if (sDumpDir.empty() )
    {
		/* Singu Note: don't generate a different dump dir each time
        LLUUID uid;
        uid.generate();
        
        sDumpDir = gDirUtilp->getExpandedFilename(LL_PATH_LOGS, "")
                    + "dump-" + uid.asString();
		*/

		sDumpDir = gDirUtilp->getExpandedFilename(LL_PATH_LOGS, "") + "singularity-debug";
        dir_exists_or_crash(sDumpDir);  
    }

	return LLDir::sDumpDir;
}

const std::string &LLDir::getPerAccountChatLogsDir() const
{
	return mPerAccountChatLogsDir;
}

const std::string &LLDir::getTempDir() const
{
	return mTempDir;
}

const std::string  LLDir::getCacheDir(bool get_default) const
{
	if (mCacheDir.empty() || get_default)
	{
		if (!mDefaultCacheDir.empty())
		{	// Set at startup - can't set here due to const API
			return mDefaultCacheDir;
		}
		
		std::string res = buildSLOSCacheDir();
		return res;
	}
	else
	{
		return mCacheDir;
	}
}

// Return the default cache directory
std::string LLDir::buildSLOSCacheDir() const
{
	std::string res;
	if (getOSCacheDir().empty())
	{
		if (getOSUserAppDir().empty())
		{
			res = "data";
		}
		else
		{
			res = add(getOSUserAppDir(), "cache_sg1");
		}
	}
	else
	{
		res = add(getOSCacheDir(), "SingularityViewer");
	}
	return res;
}



const std::string &LLDir::getOSCacheDir() const
{
	return mOSCacheDir;
}


const std::string &LLDir::getCAFile() const
{
	return mCAFile;
}

const std::string &LLDir::getDirDelimiter() const
{
	return mDirDelimiter;
}

const std::string &LLDir::getSkinDir() const
{
	return mSkinDir;
}

const std::string &LLDir::getUserSkinDir() const
{
	return mUserSkinDir;
}

const std::string& LLDir::getDefaultSkinDir() const
{
	return mDefaultSkinDir;
}

const std::string LLDir::getSkinBaseDir() const
{
	return mSkinBaseDir;
}

const std::string &LLDir::getLLPluginDir() const
{
	return mLLPluginDir;
}

static std::string ELLPathToString(ELLPath location)
{
	typedef std::map<ELLPath, const char*> ELLPathMap;
#define ENT(symbol) (symbol, #symbol)
	static const ELLPathMap sMap = map_list_of
		ENT(LL_PATH_NONE)
		ENT(LL_PATH_USER_SETTINGS)
		ENT(LL_PATH_APP_SETTINGS)
		ENT(LL_PATH_PER_SL_ACCOUNT) // returns/expands to blank string if we don't know the account name yet
		ENT(LL_PATH_CACHE)
		ENT(LL_PATH_CHARACTER)
		ENT(LL_PATH_HELP)
		ENT(LL_PATH_LOGS)
		ENT(LL_PATH_TEMP)
		ENT(LL_PATH_SKINS)
		ENT(LL_PATH_TOP_SKIN)
		ENT(LL_PATH_CHAT_LOGS)
		ENT(LL_PATH_PER_ACCOUNT_CHAT_LOGS)
		ENT(LL_PATH_USER_SKIN)
		ENT(LL_PATH_LOCAL_ASSETS)
		ENT(LL_PATH_EXECUTABLE)
		ENT(LL_PATH_DEFAULT_SKIN)
		ENT(LL_PATH_FONTS)
		ENT(LL_PATH_LAST)
	;
#undef ENT

	ELLPathMap::const_iterator found = sMap.find(location);
	if (found != sMap.end())
		return found->second;
	return STRINGIZE("Invalid ELLPath value " << location);
}

std::string LLDir::getExpandedFilename(ELLPath location, const std::string& filename) const
{
	return getExpandedFilename(location, "", filename);
}

std::string LLDir::getExpandedFilename(ELLPath location, const std::string& subdir, const std::string& filename) const
{
	return getExpandedFilename(location, "", subdir, filename);
}

std::string LLDir::getExpandedFilename(ELLPath location, const std::string& subdir1, const std::string& subdir2, const std::string& in_filename) const
{
	std::string prefix;
	switch (location)
	{
	case LL_PATH_NONE:
		// Do nothing
		break;

	case LL_PATH_APP_SETTINGS:
		prefix = add(getAppRODataDir(), "app_settings");
		break;
	
	case LL_PATH_CHARACTER:
		prefix = add(getAppRODataDir(), "character");
		break;
		
	case LL_PATH_HELP:
		prefix = "help";
		break;
		
	case LL_PATH_CACHE:
		prefix = getCacheDir();
		break;
		
    case LL_PATH_DUMP:
        prefix=getDumpDir();
        break;
            
	case LL_PATH_USER_SETTINGS:
		prefix = add(getOSUserAppDir(), "user_settings");
		break;

	case LL_PATH_PER_SL_ACCOUNT:
		prefix = getLindenUserDir();
		if (prefix.empty())
		{
			// if we're asking for the per-SL-account directory but we haven't
			// logged in yet (or otherwise don't know the account name from
			// which to build this string), then intentionally return a blank
			// string to the caller and skip the below warning about a blank
			// prefix.
			LL_DEBUGS("LLDir") << "getLindenUserDir() not yet set: "
							   << ELLPathToString(location)
							   << ", '" << subdir1 << "', '" << subdir2 << "', '" << in_filename
							   << "' => ''" << LL_ENDL;
			return std::string();
		}
		break;
		
	case LL_PATH_CHAT_LOGS:
		prefix = getChatLogsDir();
		break;
		
	case LL_PATH_PER_ACCOUNT_CHAT_LOGS:
		prefix = getPerAccountChatLogsDir();
		break;

	case LL_PATH_LOGS:
		prefix = add(getOSUserAppDir(), "logs");
		break;

	case LL_PATH_TEMP:
		prefix = getTempDir();
		break;

	case LL_PATH_TOP_SKIN:
		prefix = getSkinDir();
		break;

	case LL_PATH_DEFAULT_SKIN:
		prefix = getDefaultSkinDir();
		break;

	case LL_PATH_USER_SKIN:
		prefix = getUserSkinDir();
		break;

	case LL_PATH_SKINS:
		prefix = getSkinBaseDir();
		break;

	case LL_PATH_LOCAL_ASSETS:
		prefix = add(getAppRODataDir(), "local_assets");
		break;

	case LL_PATH_EXECUTABLE:
		prefix = getExecutableDir();
		break;
		
	case LL_PATH_FONTS:
		prefix = add(getAppRODataDir(), "fonts");
		break;
		
	default:
		llassert(0);
	}

	if (prefix.empty())
	{
		llwarns << ELLPathToString(location)
				<< ", '" << subdir1 << "', '" << subdir2 << "', '" << in_filename
				<< "': prefix is empty, possible bad filename" << llendl;
	}

	std::string expanded_filename = add(add(prefix, subdir1), subdir2);
	if (expanded_filename.empty() && in_filename.empty())
	{
		return "";
	}
	// Use explicit concatenation here instead of another add() call. Callers
	// passing in_filename as "" expect to obtain a pathname ending with
	// mDirSeparator so they can later directly concatenate with a specific
	// filename. A caller using add() doesn't care, but there's still code
	// loose in the system that uses std::string::operator+().
	expanded_filename += mDirDelimiter;
	expanded_filename += in_filename;

	LL_DEBUGS("LLDir") << ELLPathToString(location)
					   << ", '" << subdir1 << "', '" << subdir2 << "', '" << in_filename
					   << "' => '" << expanded_filename << "'" << LL_ENDL;
	return expanded_filename;
}

std::string LLDir::getBaseFileName(const std::string& filepath, bool strip_exten) const
{
	std::size_t offset = filepath.find_last_of(getDirDelimiter());
	offset = (offset == std::string::npos) ? 0 : offset+1;
	std::string res = filepath.substr(offset, std::string::npos);
	if (strip_exten)
	{
		offset = res.find_last_of('.');
		if (offset != std::string::npos &&
		    offset != 0) // if basename STARTS with '.', don't strip
		{
			res = res.substr(0, offset);
		}
	}
	return res;
}

std::string LLDir::getDirName(const std::string& filepath) const
{
	std::size_t offset = filepath.find_last_of(getDirDelimiter());
	S32 len = (offset == std::string::npos) ? 0 : offset;
	std::string dirname = filepath.substr(0, len);
	return dirname;
}

std::string LLDir::getExtension(const std::string& filepath) const
{
	if (filepath.empty())
		return std::string();
	std::string basename = getBaseFileName(filepath, false);
	std::size_t offset = basename.find_last_of('.');
	std::string exten = (offset == std::string::npos || offset == 0) ? "" : basename.substr(offset+1);
	LLStringUtil::toLower(exten);
	return exten;
}

std::string LLDir::findSkinnedFilename(const std::string &filename) const
{
	return findSkinnedFilename("", "", filename);
}

std::string LLDir::findSkinnedFilename(const std::string &subdir, const std::string &filename) const
{
	return findSkinnedFilename("", subdir, filename);
}

std::string LLDir::findSkinnedFilename(const std::string &subdir1, const std::string &subdir2, const std::string &filename) const
{
	// generate subdirectory path fragment, e.g. "/foo/bar", "/foo", ""
	std::string subdirs = ((subdir1.empty() ? "" : mDirDelimiter) + subdir1)
						 + ((subdir2.empty() ? "" : mDirDelimiter) + subdir2);

	std::vector<std::string> search_paths;
	
	search_paths.push_back(getUserSkinDir() + subdirs);		// first look in user skin override
	search_paths.push_back(getSkinDir() + subdirs);			// then in current skin
	search_paths.push_back(getDefaultSkinDir() + subdirs);  // then default skin
	search_paths.push_back(getCacheDir() + subdirs);		// and last in preload directory

	std::string found_file = findFile(filename, search_paths);
	return found_file;
}

std::string LLDir::getTempFilename() const
{
	LLUUID random_uuid;
	std::string uuid_str;

	random_uuid.generate();
	random_uuid.toString(uuid_str);

	return add(getTempDir(), uuid_str + ".tmp");
}

// static
std::string LLDir::getScrubbedFileName(const std::string uncleanFileName)
{
	std::string name(uncleanFileName);
	std::string illegalChars(getForbiddenFileChars());
#if LL_LINUX || LL_SOLARIS
	// Spaces in filenames are REALLY annoying on UNIX.
	illegalChars += ' ';
#endif
	// replace any illegal file chars with and underscore '_'
	for( unsigned int i = 0; i < illegalChars.length(); i++ )
	{
		int j = -1;
		while((j = name.find(illegalChars[i])) > -1)
		{
			name[j] = '_';
		}
	}
	return name;
}

// static
std::string LLDir::getForbiddenFileChars()
{
	return "\\/:*?\"<>|";
}

void LLDir::setLindenUserDir(const std::string &grid, const std::string &first, const std::string &last)
{
	// if both first and last aren't set, assume we're grabbing the cached dir
	if (!first.empty() && !last.empty())
	{
		// some platforms have case-sensitive filesystems, so be
		// utterly consistent with our firstname/lastname case.
		std::string userlower(first+"_"+last);
		LLStringUtil::toLower(userlower);
		mLindenUserDir = add(getOSUserAppDir(), userlower);
		
		if (!grid.empty())
		{
			std::string gridlower(grid);
			LLStringUtil::toLower(gridlower);
			mLindenUserDir += "@";
			mLindenUserDir += gridlower;
		}		
	}
	else
	{
		llerrs << "Invalid name for LLDir::setLindenUserDir" << llendl;
	}

	dumpCurrentDirectories();	
}

void LLDir::setChatLogsDir(const std::string &path)
{
	if (!path.empty() )
	{
		mChatLogsDir = path;
	}
	else
	{
		llwarns << "Invalid name for LLDir::setChatLogsDir" << llendl;
	}
}

void LLDir::setPerAccountChatLogsDir(const std::string &grid, const std::string &first, const std::string &last)
{
	// if both first and last aren't set, assume we're grabbing the cached dir
	if (!first.empty() && !last.empty())
	{
		// some platforms have case-sensitive filesystems, so be
		// utterly consistent with our firstname/lastname case.
		std::string userlower(first+"_"+last);
		LLStringUtil::toLower(userlower);
		mPerAccountChatLogsDir = add(getChatLogsDir(), userlower);
		if (!grid.empty())
		{
			std::string gridlower(grid);
			LLStringUtil::toLower(gridlower);
			mPerAccountChatLogsDir += "@";
			mPerAccountChatLogsDir += gridlower;
		}
	}
	else
	{
		llwarns << "Invalid name for LLDir::setPerAccountChatLogsDir" << llendl;
	}
}

void LLDir::setSkinFolder(const std::string &skin_folder)
{
	mSkinDir = getSkinBaseDir();
	append(mSkinDir, skin_folder);

	// user modifications to current skin
	// e.g. c:\documents and settings\users\username\application data\second life\skins\dazzle
	mUserSkinDir = getOSUserAppDir();
	append(mUserSkinDir, "skins_sg1");
	append(mUserSkinDir, skin_folder);

	// base skin which is used as fallback for all skinned files
	// e.g. c:\program files\secondlife\skins\default
	mDefaultSkinDir = getSkinBaseDir();
	append(mDefaultSkinDir, "default");
}

bool LLDir::setCacheDir(const std::string &path)
{
	if (path.empty() )
	{
		// reset to default
		mCacheDir = "";
		return true;
	}
	else
	{
		LLFile::mkdir(path);
		std::string tempname = add(path, "temp");
		LLFILE* file = LLFile::fopen(tempname,"wt");
		if (file)
		{
			fclose(file);
			LLFile::remove(tempname);
			mCacheDir = path;
			return true;
		}
		return false;
	}
}

void LLDir::dumpCurrentDirectories()
{
	LL_DEBUGS2("AppInit","Directories") << "Current Directories:" << LL_ENDL;

	LL_DEBUGS2("AppInit","Directories") << "  CurPath:               " << getCurPath() << LL_ENDL;
	LL_DEBUGS2("AppInit","Directories") << "  AppName:               " << getAppName() << LL_ENDL;
	LL_DEBUGS2("AppInit","Directories") << "  ExecutableFilename:    " << getExecutableFilename() << LL_ENDL;
	LL_DEBUGS2("AppInit","Directories") << "  ExecutableDir:         " << getExecutableDir() << LL_ENDL;
	LL_DEBUGS2("AppInit","Directories") << "  ExecutablePathAndName: " << getExecutablePathAndName() << LL_ENDL;
	LL_DEBUGS2("AppInit","Directories") << "  WorkingDir:            " << getWorkingDir() << LL_ENDL;
	LL_DEBUGS2("AppInit","Directories") << "  AppRODataDir:          " << getAppRODataDir() << LL_ENDL;
	LL_DEBUGS2("AppInit","Directories") << "  OSUserDir:             " << getOSUserDir() << LL_ENDL;
	LL_DEBUGS2("AppInit","Directories") << "  OSUserAppDir:          " << getOSUserAppDir() << LL_ENDL;
	LL_DEBUGS2("AppInit","Directories") << "  LindenUserDir:         " << getLindenUserDir() << LL_ENDL;
	LL_DEBUGS2("AppInit","Directories") << "  TempDir:               " << getTempDir() << LL_ENDL;
	LL_DEBUGS2("AppInit","Directories") << "  CAFile:				 " << getCAFile() << LL_ENDL;
	LL_DEBUGS2("AppInit","Directories") << "  SkinBaseDir:           " << getSkinBaseDir() << LL_ENDL;
	LL_DEBUGS2("AppInit","Directories") << "  SkinDir:               " << getSkinDir() << LL_ENDL;
}

std::string LLDir::add(const std::string& path, const std::string& name) const
{
	std::string destpath(path);
	append(destpath, name);
	return destpath;
}

void LLDir::append(std::string& destpath, const std::string& name) const
{
	// Delegate question of whether we need a separator to helper method.
	SepOff sepoff(needSep(destpath, name));
	if (sepoff.first)               // do we need a separator?
	{
		destpath += mDirDelimiter;
	}
	// If destpath ends with a separator, AND name starts with one, skip
	// name's leading separator.
	destpath += name.substr(sepoff.second);
}

LLDir::SepOff LLDir::needSep(const std::string& path, const std::string& name) const
{
	if (path.empty() || name.empty())
	{
		// If either path or name are empty, we do not need a separator
		// between them.
		return SepOff(false, 0);
	}
	// Here we know path and name are both non-empty. But if path already ends
	// with a separator, or if name already starts with a separator, we need
	// not add one.
	std::string::size_type seplen(mDirDelimiter.length());
	bool path_ends_sep(path.substr(path.length() - seplen) == mDirDelimiter);
	bool name_starts_sep(name.substr(0, seplen) == mDirDelimiter);
	if ((! path_ends_sep) && (! name_starts_sep))
	{
		// If neither path nor name brings a separator to the junction, then
		// we need one.
		return SepOff(true, 0);
	}
	if (path_ends_sep && name_starts_sep)
	{
		// But if BOTH path and name bring a separator, we need not add one.
		// Moreover, we should actually skip the leading separator of 'name'.
		return SepOff(false, seplen);
	}
	// Here we know that either path_ends_sep or name_starts_sep is true --
	// but not both. So don't add a separator, and don't skip any characters:
	// simple concatenation will do the trick.
	return SepOff(false, 0);
}

void dir_exists_or_crash(const std::string &dir_name)
{
#if LL_WINDOWS
	// *FIX: lame - it doesn't do the same thing on windows. not so
	// important since we don't deploy simulator to windows boxes.
	LLFile::mkdir(dir_name, 0700);
#else
	struct stat dir_stat;
	if(0 != LLFile::stat(dir_name, &dir_stat))
	{
		S32 stat_rv = errno;
		if(ENOENT == stat_rv)
		{
		   if(0 != LLFile::mkdir(dir_name, 0700))		// octal
		   {
			   llerrs << "Unable to create directory: " << dir_name << llendl;
		   }
		}
		else
		{
			llerrs << "Unable to stat: " << dir_name << " errno = " << stat_rv
				   << llendl;
		}
	}
	else
	{
		// data_dir exists, make sure it's a directory.
		if(!S_ISDIR(dir_stat.st_mode))
		{
			llerrs << "Data directory collision: " << dir_name << llendl;
		}
	}
#endif
}
