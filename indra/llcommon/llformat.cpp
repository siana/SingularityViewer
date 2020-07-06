/** 
 * @file llformat.cpp
 * @date   January 2007
 * @brief string formatting utility
 *
 * $LicenseInfo:firstyear=2007&license=viewergpl$
 * 
 * Copyright (c) 2007-2009, Linden Research, Inc.
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

#include "linden_common.h"

#include "llformat.h"

#include <cstdarg>
#include <boost/align/aligned_allocator.hpp>

// common used function with va_list argument
// wrapper for vsnprintf to be called from llformatXXX functions.
static void va_format(std::string& out, const char *fmt, va_list& va)
{
	if (!fmt || !fmt[0]) return; // Don't bother if null or empty c_str

	typedef typename std::vector<char, boost::alignment::aligned_allocator<char, 1>> vec_t;
	static thread_local vec_t charvector(1024); // Evolves into charveleon
	#define vsnprintf(va) std::vsnprintf(charvector.data(), charvector.capacity(), fmt, va)
#ifdef LL_WINDOWS // We don't have to copy on windows
	#define va2 va
#else
	va_list va2;
	va_copy(va2, va);
#endif
	const auto smallsize(charvector.capacity());
	const auto size = vsnprintf(va);
	if (size < 0)
	{
		LL_ERRS() << "Encoding failed, code " << size << ". String hint: " << out << '/' << fmt << LL_ENDL;
	}
	else if (static_cast<vec_t::size_type>(size) >= smallsize) // Resize if we need more space
	{
		charvector.resize(1+size); // Use the String Stone
		vsnprintf(va2);
	}
#ifndef LL_WINDOWS
	va_end(va2);
#endif
	out.assign(charvector.data());
}

std::string llformat(const char *fmt, ...)
{
	std::string res;
	va_list va;
	va_start(va, fmt);
	va_format(res, fmt, va);
	va_end(va);
	return res;
}

std::string llformat_to_utf8(const char *fmt, ...)
{
	std::string res;
	va_list va;
	va_start(va, fmt);
	va_format(res, fmt, va);
	va_end(va);

#if LL_WINDOWS
	// made converting to utf8. See EXT-8318.
	res = ll_convert_string_to_utf8_string(res);
#endif
	return res;
}

