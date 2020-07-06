/** 
 * @file llapr.cpp
 * @author Phoenix
 * @date 2004-11-28
 * @brief Helper functions for using the apache portable runtime library.
 *
 * $LicenseInfo:firstyear=2004&license=viewerlgpl$
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
#include "llapr.h"
#include "llscopedvolatileaprpool.h"
#include <limits>

bool ll_apr_warn_status(apr_status_t status)
{
	if(APR_SUCCESS == status) return false;
	char buf[MAX_STRING];	/* Flawfinder: ignore */
	apr_strerror(status, buf, sizeof(buf));
	LL_WARNS("APR") << "APR: " << buf << LL_ENDL;
	return true;
}

void ll_apr_assert_status(apr_status_t status)
{
	llassert(!ll_apr_warn_status(status));
}

//---------------------------------------------------------------------
//
// LLAPRFile functions
//
LLAPRFile::LLAPRFile()
	: mFile(NULL),
	  mVolatileFilePoolp(NULL),
	  mRegularFilePoolp(NULL)
{
}

LLAPRFile::LLAPRFile(const std::string& filename, apr_int32_t flags, S32* sizep, access_t access_type)
	: mFile(NULL),
	  mVolatileFilePoolp(NULL),
	  mRegularFilePoolp(NULL)
{
	open(filename, flags, access_type, sizep);
}

LLAPRFile::~LLAPRFile()
{
	close() ;
}

apr_status_t LLAPRFile::close() 
{
	apr_status_t ret = APR_SUCCESS ;
	if(mFile)
	{
		ret = apr_file_close(mFile);
		mFile = NULL ;
	}

	if (mVolatileFilePoolp)
	{
		mVolatileFilePoolp->clearVolatileAPRPool() ;
		mVolatileFilePoolp = NULL ;
	}

	if (mRegularFilePoolp)
	{
		delete mRegularFilePoolp;
		mRegularFilePoolp = NULL;
	}

	return ret ;
}

apr_status_t LLAPRFile::open(std::string const& filename, apr_int32_t flags, access_t access_type, S32* sizep)
{
	llassert_always(!mFile);
	llassert_always(!mVolatileFilePoolp && !mRegularFilePoolp);

	apr_status_t status;
	{
		apr_pool_t* apr_file_open_pool;	// The use of apr_pool_t is OK here.
										// This is a temporary variable for a pool that is passed directly to apr_file_open below.
		if (access_type == short_lived)
		{
			// Use a "volatile" thread-local pool.
			mVolatileFilePoolp = &LLThreadLocalData::tldata().mVolatileAPRPool;
			// Access the pool and increment it's reference count.
			// The reference count of LLVolatileAPRPool objects will be decremented
			// again in LLAPRFile::close by calling mVolatileFilePoolp->clearVolatileAPRPool().
			apr_file_open_pool = mVolatileFilePoolp->getVolatileAPRPool();
		}
		else
		{
			mRegularFilePoolp = new LLAPRPool(LLThreadLocalData::tldata().mRootPool);
			apr_file_open_pool = (*mRegularFilePoolp)();
		}
		status = apr_file_open(&mFile, filename.c_str(), flags, APR_OS_DEFAULT, apr_file_open_pool);
	}
	if (status != APR_SUCCESS || !mFile)
	{
		mFile = NULL ;
		close() ;
		if (sizep)
		{
			*sizep = 0;
		}
		return status;
	}

	if (sizep)
	{
		S32 file_size = 0;
		apr_off_t offset = 0;
		if (apr_file_seek(mFile, APR_END, &offset) == APR_SUCCESS)
		{
			llassert_always(offset <= 0x7fffffff);
			file_size = (S32)offset;
			offset = 0;
			apr_file_seek(mFile, APR_SET, &offset);
		}
		*sizep = file_size;
	}

	return status;
}

apr_status_t LLAPRFile::open(const std::string& filename, apr_int32_t flags, BOOL use_global_pool)
{
	return open(filename, flags, use_global_pool ? LLAPRFile::long_lived : LLAPRFile::short_lived);
}
// File I/O
S32 LLAPRFile::read(void *buf, U64 nbytes)
{
	if(!mFile) 
	{
		LL_WARNS() << "apr mFile is removed by somebody else. Can not read." << LL_ENDL ;
		return 0;
	}
	
	apr_size_t sz = nbytes;
	apr_status_t s = apr_file_read(mFile, buf, &sz);
	if (s != APR_SUCCESS)
	{
		ll_apr_warn_status(s);
		return 0;
	}
	else
	{
		llassert_always(sz <= std::numeric_limits<apr_size_t>::max());
		return (S32)sz;
	}
}

S32 LLAPRFile::write(const void *buf, U64 nbytes)
{
	if(!mFile) 
	{
		LL_WARNS() << "apr mFile is removed by somebody else. Can not write." << LL_ENDL ;
		return 0;
	}
	
	apr_size_t sz = nbytes;
	apr_status_t s = apr_file_write(mFile, buf, &sz);
	if (s != APR_SUCCESS)
	{
		ll_apr_warn_status(s);
		return 0;
	}
	else
	{
		llassert_always(sz <= std::numeric_limits<apr_size_t>::max());
		return (S32)sz;
	}
}

S32 LLAPRFile::seek(apr_seek_where_t where, S32 offset)
{
	return LLAPRFile::seek(mFile, where, offset) ;
}

//
// *******************************************************************************************************************************
//static components of LLAPRFile
//

//static
S32 LLAPRFile::seek(apr_file_t* file_handle, apr_seek_where_t where, S32 offset)
{
	if(!file_handle)
	{
		return -1 ;
	}

	apr_status_t s;
	apr_off_t apr_offset;
	if (offset >= 0)
	{
		apr_offset = (apr_off_t)offset;
		s = apr_file_seek(file_handle, where, &apr_offset);
	}
	else
	{
		apr_offset = 0;
		s = apr_file_seek(file_handle, APR_END, &apr_offset);
	}
	if (s != APR_SUCCESS)
	{
		ll_apr_warn_status(s);
		return -1;
	}
	else
	{
		llassert_always(apr_offset <= 0x7fffffff);
		return (S32)apr_offset;
	}
}

//static
S32 LLAPRFile::readEx(const std::string& filename, void *buf, S32 offset, S32 nbytes)
{
	apr_file_t* file_handle;
	LLScopedVolatileAPRPool pool;
	apr_status_t s = apr_file_open(&file_handle, filename.c_str(), APR_READ|APR_BINARY, APR_OS_DEFAULT, pool);
	if (s != APR_SUCCESS || !file_handle)
	{
		ll_apr_warn_status(s);
		LL_WARNS("APR") << " while attempting to open file \"" << filename << '"' << LL_ENDL;
		return 0;
	}

	S32 off;
	if (offset < 0)
		off = LLAPRFile::seek(file_handle, APR_END, 0);
	else
		off = LLAPRFile::seek(file_handle, APR_SET, offset);
	
	apr_size_t bytes_read;
	if (off < 0)
	{
		bytes_read = 0;
	}
	else
	{
		bytes_read = nbytes ;		
		apr_status_t s = apr_file_read(file_handle, buf, &bytes_read);
		if (s != APR_SUCCESS)
		{
			LL_WARNS("APR") << " Attempting to read filename: " << filename << LL_ENDL;
			ll_apr_warn_status(s);
			bytes_read = 0;
		}
		else
		{
			llassert_always(bytes_read <= 0x7fffffff);		
		}
	}
	
	apr_file_close(file_handle);

	return (S32)bytes_read;
}

//static
S32 LLAPRFile::writeEx(const std::string& filename, void *buf, S32 offset, S32 nbytes)
{
	apr_int32_t flags = APR_CREATE|APR_WRITE|APR_BINARY;
	if (offset < 0)
	{
		flags |= APR_APPEND;
		offset = 0;
	}
	
	apr_file_t* file_handle;
	LLScopedVolatileAPRPool pool;
	apr_status_t s = apr_file_open(&file_handle, filename.c_str(), flags, APR_OS_DEFAULT, pool);
	if (s != APR_SUCCESS || !file_handle)
	{
		ll_apr_warn_status(s);
		LL_WARNS("APR") << " while attempting to open file \"" << filename << '"' << LL_ENDL;
		return 0;
	}

	if (offset > 0)
	{
		offset = LLAPRFile::seek(file_handle, APR_SET, offset);
	}
	
	apr_size_t bytes_written;
	if (offset < 0)
	{
		bytes_written = 0;
	}
	else
	{
		bytes_written = nbytes ;		
		apr_status_t s = apr_file_write(file_handle, buf, &bytes_written);
		if (s != APR_SUCCESS)
		{
			LL_WARNS("APR") << " Attempting to write filename: " << filename << LL_ENDL;
			ll_apr_warn_status(s);
			bytes_written = 0;
		}
		else
		{
			llassert_always(bytes_written <= 0x7fffffff);
		}
	}

	apr_file_close(file_handle);

	return (S32)bytes_written;
}

//static
bool LLAPRFile::remove(const std::string& filename)
{
	apr_status_t s;

	LLScopedVolatileAPRPool pool;
	s = apr_file_remove(filename.c_str(), pool);

	if (s != APR_SUCCESS)
	{
		if (s != APR_ENOENT)	// Not an issue if the file did not exist in the first place...
		{
		ll_apr_warn_status(s);
		LL_WARNS("APR") << " Attempting to remove filename: " << filename << LL_ENDL;
		}
		return false;
	}
	return true;
}

//static
bool LLAPRFile::rename(const std::string& filename, const std::string& newname)
{
	apr_status_t s;

	LLScopedVolatileAPRPool pool;
	s = apr_file_rename(filename.c_str(), newname.c_str(), pool);
	
	if (s != APR_SUCCESS)
	{
		ll_apr_warn_status(s);
		LL_WARNS("APR") << " Attempting to rename filename: " << filename << LL_ENDL;
		return false;
	}
	return true;
}

//static
bool LLAPRFile::isExist(const std::string& filename, apr_int32_t flags)
{
	apr_file_t* file_handle;
	apr_status_t s;

	LLScopedVolatileAPRPool pool;
	s = apr_file_open(&file_handle, filename.c_str(), flags, APR_OS_DEFAULT, pool);

	if (s != APR_SUCCESS || !file_handle)
	{
		return false;
	}
	else
	{
		apr_file_close(file_handle);
		return true;
	}
}

//static
S32 LLAPRFile::size(const std::string& filename)
{
	apr_file_t* file_handle;
	apr_finfo_t info;
	apr_status_t s;
	
	LLScopedVolatileAPRPool pool;
	s = apr_file_open(&file_handle, filename.c_str(), APR_READ, APR_OS_DEFAULT, pool);
	
	if (s != APR_SUCCESS || !file_handle)
	{		
		return 0;
	}
	else
	{
		apr_status_t s = apr_file_info_get(&info, APR_FINFO_SIZE, file_handle);

		apr_file_close(file_handle) ;
		
		if (s == APR_SUCCESS)
		{
			return (S32)info.size;
		}
		else
		{
			return 0;
		}
	}
}

//static
bool LLAPRFile::makeDir(const std::string& dirname)
{
	apr_status_t s;

	LLScopedVolatileAPRPool pool;
	s = apr_dir_make(dirname.c_str(), APR_FPROT_OS_DEFAULT, pool);
		
	if (s != APR_SUCCESS)
	{
		ll_apr_warn_status(s);
		LL_WARNS("APR") << " while attempting to make directory: " << dirname << LL_ENDL;
		return false;
	}
	return true;
}

//static
bool LLAPRFile::removeDir(const std::string& dirname)
{
	apr_status_t s;

	LLScopedVolatileAPRPool pool;
	s = apr_file_remove(dirname.c_str(), pool);
	
	if (s != APR_SUCCESS)
	{
		ll_apr_warn_status(s);
		LL_WARNS("APR") << " Attempting to remove directory: " << dirname << LL_ENDL;
		return false;
	}
	return true;
}
//
//end of static components of LLAPRFile
// *******************************************************************************************************************************
//
