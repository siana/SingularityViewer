/** 
 * @file lldarray.h
 * @brief Wrapped std::vector for backward compatibility.
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
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

#ifndef LL_LLDARRAY_H
#define LL_LLDARRAY_H

#include "llerror.h"

#include <vector>
#include <map>

//--------------------------------------------------------
// LLIndexedVector
//--------------------------------------------------------

template <typename Type, typename Key, int BlockSize = 32>
class LLIndexedVector1
{
public:
	typedef typename std::vector<std::pair<Key, Type> > vec_type_t;
	typedef typename vec_type_t::iterator iterator;
	typedef typename vec_type_t::const_iterator const_iterator;
	typedef typename vec_type_t::reverse_iterator reverse_iterator;
	typedef typename vec_type_t::const_reverse_iterator const_reverse_iterator;
	typedef typename vec_type_t::size_type size_type;
protected:
	std::vector<std::pair<Key, Type> > mVector;
	
public:
	LLIndexedVector1() { mVector.reserve(BlockSize); }

	const Type& toValue(const_iterator& iter) const { return iter->second; }
	Type& toValue(iterator& iter) { return iter->second; }
	
	iterator begin() { return mVector.begin(); }
	const_iterator begin() const { return mVector.begin(); }
	iterator end() { return mVector.end(); }
	const_iterator end() const { return mVector.end(); }

	reverse_iterator rbegin() { return mVector.rbegin(); }
	const_reverse_iterator rbegin() const { return mVector.rbegin(); }
	reverse_iterator rend() { return mVector.rend(); }
	const_reverse_iterator rend() const { return mVector.rend(); }

	void reset() { mVector.resize(0); }
	bool empty() const { return mVector.empty(); }
	size_type size() const { return mVector.size(); }
	
	Type& operator[](const Key& k)
	{
		return get_val_in_pair_vec(mVector, k);
	}

	const_iterator find(const Key& k) const
	{
		return std::find_if(mVector.begin(), mVector.end(), [&k](const typename vec_type_t::value_type& e) { return e.first == k; });
	}

	using DeletePointer = ::DeletePairedPointer;
};

template <typename Type, typename Key, int BlockSize = 32>
class LLIndexedVector2
{
public:
	typedef typename std::vector<Type>::iterator iterator;
	typedef typename std::vector<Type>::const_iterator const_iterator;
	typedef typename std::vector<Type>::reverse_iterator reverse_iterator;
	typedef typename std::vector<Type>::const_reverse_iterator const_reverse_iterator;
	typedef typename std::vector<Type>::size_type size_type;
protected:
	std::vector<Type> mVector;
	std::map<Key, U32> mIndexMap;
#ifdef DEBUG_LLINDEXEDVECTOR
	LLIndexedVector1<Type*, Key, BlockSize> mCopy;
#endif

public:
	LLIndexedVector2() { mVector.reserve(BlockSize); }

	const Type& toValue(const_iterator& iter) const { return *iter; }
	Type& toValue(iterator& iter) { return *iter; }

	iterator begin() { return mVector.begin(); }
	const_iterator begin() const { return mVector.begin(); }
	iterator end() { return mVector.end(); }
	const_iterator end() const { return mVector.end(); }

	reverse_iterator rbegin() { return mVector.rbegin(); }
	const_reverse_iterator rbegin() const { return mVector.rbegin(); }
	reverse_iterator rend() { return mVector.rend(); }
	const_reverse_iterator rend() const { return mVector.rend(); }

	void reset() {
#ifdef DEBUG_LLINDEXEDVECTOR
		mCopy.reset();
#endif
		mVector.resize(0);
		mIndexMap.resize(0);
	}
	bool empty() const { return mVector.empty(); }
	size_type size() const { return mVector.size(); }

	void verify() const
	{
#ifdef DEBUG_LLINDEXEDVECTOR
		llassert_always(mCopy.empty() == empty());
		llassert_always(mCopy.size() == size());
		auto it2 = mCopy.begin();
		int index = 0;
		for (auto it1 = begin(); it1 != end(); ++it1)
		{
			llassert_always(it2->second == &*it1);
			++it2;
			++index;
		}
		for (auto it = mIndexMap.begin(); it != mIndexMap.end(); ++it)
		{
			auto i = it->second;
			llassert_always((mCopy.begin() + i)->first == it->first);
			llassert_always((mCopy.begin() + i)->second == &mVector[i]);
		}
#endif
	}

	Type& operator[](const Key& k)
	{
		verify();
		typename std::map<Key, U32>::const_iterator iter = mIndexMap.find(k);

		if (iter == mIndexMap.end())
		{
			U32 n = mVector.size();
			mIndexMap[k] = n;
			mVector.push_back(Type());
			llassert(mVector.size() == mIndexMap.size());
#ifdef DEBUG_LLINDEXEDVECTOR
			mCopy[k] = &mVector[n];
			auto it2 = mCopy.begin();
			for (auto it1 = begin(); it1 != end(); ++it1)
			{
				it2->second = &(*it1);
				++it2;
			}
#endif
			return mVector[n];
		}
		else
		{
			return mVector[iter->second];
		}
	}

	const_iterator find(const Key& k) const
	{
		verify();
		typename std::map<Key, U32>::const_iterator iter = mIndexMap.find(k);

		if(iter == mIndexMap.end())
		{
			return mVector.end();
		}
		else
		{
			return mVector.begin() + iter->second;
		}
	}

	using DeletePointer = ::DeletePointer;
};

template <typename Type, typename Key, int BlockSize = 32>
#ifdef DEBUG_LLINDEXEDVECTOR
using LLIndexedVector = LLIndexedVector2<Type, Key, BlockSize>;
#else
using LLIndexedVector = LLIndexedVector1<Type, Key, BlockSize>;
#endif

#endif
