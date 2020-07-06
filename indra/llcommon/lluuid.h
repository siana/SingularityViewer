/** 
 * @file lluuid.h
 *
 * $LicenseInfo:firstyear=2000&license=viewerlgpl$
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

#ifndef LL_LLUUID_H
#define LL_LLUUID_H

#include <iostream>
#include <set>
#include <vector>
#include <functional>
#include <boost/functional/hash.hpp>
#include <boost/unordered_set.hpp>
#include "stdtypes.h"
#include "llpreprocessor.h"
#include <absl/hash/hash.h>

class LLMutex;

const S32 UUID_BYTES = 16;
const S32 UUID_WORDS = 4;
const S32 UUID_STR_LENGTH = 37;	// actually wrong, should be 36 and use size below
const S32 UUID_STR_SIZE = 37;
const S32 UUID_BASE85_LENGTH = 21; // including the trailing NULL.

struct uuid_time_t {
	U32 high;
	U32 low;
		};

class LL_COMMON_API LLUUID
{
public:
	//
	// CREATORS
	//
	LLUUID();
	explicit LLUUID(const char *in_string); // Convert from string.
	explicit LLUUID(const std::string& in_string); // Convert from string.
	LLUUID(const LLUUID &in) = default;
	LLUUID &operator=(const LLUUID &rhs) = default;

	~LLUUID() = default;

	//
	// MANIPULATORS
	//
	void	generate();					// Generate a new UUID
	void	generate(const std::string& stream); //Generate a new UUID based on hash of input stream

	static LLUUID generateNewID(std::string stream = "");	//static version of above for use in initializer expressions such as constructor params, etc. 

	BOOL	set(const char *in_string, BOOL emit = TRUE);	// Convert from string, if emit is FALSE, do not emit warnings
	BOOL	set(const std::string& in_string, BOOL emit = TRUE);	// Convert from string, if emit is FALSE, do not emit warnings
	void	setNull();					// Faster than setting to LLUUID::null.

	S32     cmpTime(uuid_time_t *t1, uuid_time_t *t2);
	static void    getSystemTime(uuid_time_t *timestamp);
	void    getCurrentTime(uuid_time_t *timestamp);

	//
	// ACCESSORS
	//
	BOOL	isNull() const;			// Faster than comparing to LLUUID::null.
	BOOL	notNull() const;		// Faster than comparing to LLUUID::null.
	// JC: This is dangerous.  It allows UUIDs to be cast automatically
	// to integers, among other things.  Use isNull() or notNull().
	//		operator bool() const;

	// JC: These must return real bool's (not BOOLs) or else use of the STL
	// will generate bool-to-int performance warnings.
	bool	operator==(const LLUUID &rhs) const;
	bool	operator!=(const LLUUID &rhs) const;
	bool	operator<(const LLUUID &rhs) const;
	bool	operator>(const LLUUID &rhs) const;

	template <typename H>
	friend H AbslHashValue(H h, const LLUUID& id) {
		return H::combine_contiguous(std::move(h), id.mData, UUID_BYTES);
	}

	// xor functions. Useful since any two random uuids xored together
	// will yield a determinate third random unique id that can be
	// used as a key in a single uuid that represents 2.
	const LLUUID& operator^=(const LLUUID& rhs);
	LLUUID operator^(const LLUUID& rhs) const;

	// similar to functions above, but not invertible
	// yields a third random UUID that can be reproduced from the two inputs
	// but which, given the result and one of the inputs can't be used to
	// deduce the other input
	LLUUID combine(const LLUUID& other) const;
	void combine(const LLUUID& other, LLUUID& result) const;  

	friend LL_COMMON_API std::ostream&	 operator<<(std::ostream& s, const LLUUID &uuid);
	friend LL_COMMON_API std::istream&	 operator>>(std::istream& s, LLUUID &uuid);

	void toString(std::string& out) const;
	void toCompressedString(std::string& out) const;

	std::string asString() const;
	std::string getString() const;

	U16 getCRC16() const;
	U32 getCRC32() const;

	inline size_t hash() const
	{
		size_t seed = 0;
		for (U8 i = 0; i < 4; ++i)
		{
			seed ^= static_cast<size_t>(mData[i * 4]) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
			seed ^= static_cast<size_t>(mData[i * 4 + 1]) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
			seed ^= static_cast<size_t>(mData[i * 4 + 2]) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
			seed ^= static_cast<size_t>(mData[i * 4 + 3]) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
		}
		return seed;
	}

	static BOOL validate(const std::string& in_string); // Validate that the UUID string is legal.

	static const LLUUID null;
	static LLMutex * mMutex;

	static U32 getRandomSeed();
	static S32 getNodeID(unsigned char * node_id);

	static BOOL parseUUID(const std::string& buf, LLUUID* value);

	U8 mData[UUID_BYTES];
};

// Construct
inline LLUUID::LLUUID()
{
	setNull();
}


// Faster than copying from memory
inline void LLUUID::setNull()
{
	memset(mData, 0, sizeof(mData)); // <alchemy/>
}


// Compare
inline bool LLUUID::operator==(const LLUUID& rhs) const
{
	return !memcmp(mData, rhs.mData, sizeof(mData)); // <alchemy/>
}


inline bool LLUUID::operator!=(const LLUUID& rhs) const
{
	return !!memcmp(mData, rhs.mData, sizeof(mData)); // <alchemy/>
}

/*
// JC: This is dangerous.  It allows UUIDs to be cast automatically
// to integers, among other things.  Use isNull() or notNull().
inline LLUUID::operator bool() const
{
	U32 *word = (U32 *)mData;
	return (word[0] | word[1] | word[2] | word[3]) > 0;
}
*/

inline BOOL LLUUID::notNull() const
{
	return !!memcmp(mData, null.mData, sizeof(mData)); // <alchemy/>
}

// Faster than == LLUUID::null because doesn't require
// as much memory access.
inline BOOL LLUUID::isNull() const
{
	return !memcmp(mData, null.mData, sizeof(mData)); // <alchemy/>
}

inline LLUUID::LLUUID(const char *in_string)
{
	if (!in_string || in_string[0] == 0)
	{
		setNull();
		return;
	}
 
	set(in_string);
}

inline LLUUID::LLUUID(const std::string& in_string)
{
	if (in_string.empty())
	{
		setNull();
		return;
	}

	set(in_string);
}

// IW: DON'T "optimize" these w/ U32s or you'll scoogie the sort order
// IW: this will make me very sad
inline bool LLUUID::operator<(const LLUUID &rhs) const
{
	U32 i;
	for( i = 0; i < (UUID_BYTES - 1); i++ )
	{
		if( mData[i] != rhs.mData[i] )
		{
			return (mData[i] < rhs.mData[i]);
		}
	}
	return (mData[UUID_BYTES - 1] < rhs.mData[UUID_BYTES - 1]);
}

inline bool LLUUID::operator>(const LLUUID &rhs) const
{
	U32 i;
	for( i = 0; i < (UUID_BYTES - 1); i++ )
	{
		if( mData[i] != rhs.mData[i] )
		{
			return (mData[i] > rhs.mData[i]);
		}
	}
	return (mData[UUID_BYTES - 1] > rhs.mData[UUID_BYTES - 1]);
}

inline U16 LLUUID::getCRC16() const
{
	// A UUID is 16 bytes, or 8 shorts.
	U16 *short_data = (U16*)mData;
	U16 out = 0;
	out += short_data[0];
	out += short_data[1];
	out += short_data[2];
	out += short_data[3];
	out += short_data[4];
	out += short_data[5];
	out += short_data[6];
	out += short_data[7];
	return out;
}

inline U32 LLUUID::getCRC32() const
{
	// <alchemy/>
	U32 ret = 0;
	for(U32 i = 0;i < 4;++i)
	{
		ret += (mData[i*4]) | (mData[i*4+1]) << 8 | (mData[i*4+2]) << 16 | (mData[i*4+3]) << 24;
	}
	return ret;
	// </alchemy>
}

static_assert(std::is_trivially_copyable<LLUUID>::value, "LLUUID must be a trivially copyable type");

typedef std::vector<LLUUID> uuid_vec_t;
typedef boost::unordered_set<LLUUID> uuid_set_t;

// Helper structure for ordering lluuids in stl containers.  eg:
// std::map<LLUUID, LLWidget*, lluuid_less> widget_map;
//
// (isn't this the default behavior anyway? I think we could
// everywhere replace these with uuid_set_t, but someone should
// verify.)
struct lluuid_less
{
	bool operator()(const LLUUID& lhs, const LLUUID& rhs) const
	{
		return (lhs < rhs) ? true : false;
	}
};

typedef std::set<LLUUID, lluuid_less> uuid_list_t;

namespace std {
	template <> struct hash<LLUUID>
	{
		size_t operator()(const LLUUID & id) const
		{
			return absl::Hash<LLUUID>{}(id);
		}
	};
}

namespace boost {
	template<> struct hash<LLUUID>
	{
		size_t operator()(const LLUUID& id) const
		{
			return absl::Hash<LLUUID>{}(id);
		}
	};
}

/*
 * Sub-classes for keeping transaction IDs and asset IDs
 * straight.
 */
typedef LLUUID LLAssetID;

class LL_COMMON_API LLTransactionID : public LLUUID
{
public:
	LLTransactionID() : LLUUID() { }
	
	static const LLTransactionID tnull;
	LLAssetID makeAssetID(const LLUUID& session) const;
};

#endif


