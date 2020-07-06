/** 
 * @file llbase64.cpp
 * @brief Wrapper for apr base64 encoding that returns a std::string
 * @author James Cook
 *
 * $LicenseInfo:firstyear=2007&license=viewerlgpl$
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

#include "llbase64.h"
#include <openssl/evp.h>

// static
std::string LLBase64::encode(const U8* input, size_t input_size)
{
	if (!(input && input_size > 0)) return LLStringUtil::null;
	
	BIO *b64 = BIO_new(BIO_f_base64());
	BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
	
	BIO *bio = BIO_new(BIO_s_mem());
	BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
	bio = BIO_push(b64, bio);
	BIO_write(bio, input, input_size);
	
	(void)BIO_flush(bio);
	
	char *new_data;
	size_t bytes_written = BIO_get_mem_data(bio, &new_data);
	std::string result(new_data, bytes_written);
	BIO_free_all(bio);
	
	return result;
}

// static
std::string LLBase64::encode(const std::string& in_str)
{
	size_t data_size = in_str.size();
	std::vector<U8> data;
	data.resize(data_size);
	memcpy(&data[0], in_str.c_str(), data_size);
	return encode(&data[0], data_size);
}

// static
size_t LLBase64::decode(const std::string& input, U8 * buffer, size_t buffer_size)
{
	if (input.empty()) return 0;
	
	BIO *b64 = BIO_new(BIO_f_base64());
	BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
	
	// BIO_new_mem_buf is not const aware, but it doesn't modify the buffer
	BIO *bio = BIO_new_mem_buf(const_cast<char*>(input.c_str()), input.length());
	bio = BIO_push(b64, bio);
	size_t bytes_written = BIO_read(bio, buffer, buffer_size);
	BIO_free_all(bio);
	
	return bytes_written;
}

std::string LLBase64::decode(const std::string& input)
{
	U32 buffer_len = LLBase64::requiredDecryptionSpace(input);
	std::vector<U8> buffer(buffer_len);
	buffer_len = LLBase64::decode(input, &buffer[0], buffer_len);
	buffer.resize(buffer_len);
	return std::string(reinterpret_cast<const char*>(&buffer[0]), buffer_len);
}

// static
size_t LLBase64::requiredDecryptionSpace(const std::string& str)
{
	size_t len = str.length();
	U32 padding = 0;
 
	if (str[len - 1] == '=' && str[len - 2] == '=') //last two chars are =
		padding = 2;
	else if (str[len - 1] == '=') //last char is =
		padding = 1;

	return len * 0.75 - padding;
}

