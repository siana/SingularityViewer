/**
 * @file llcororesponder.h
 * @brief A responder purposed to call coro functions, to ease transition to LLCoro
 *
 * $LicenseInfo:firstyear=2006&license=viewerlgpl$
 * Second Life Viewer Source Code
 * 
 * Copyright (C) 2020, Liru Færs
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

#include <functional>
#include "llhttpclient.h"

struct LLCoroResponderBase : public LLHTTPClient::ResponderWithCompleted
{
	const AIHTTPReceivedHeaders& getHeaders() const { return mReceivedHeaders; }
	const LLSD& getContent() const { return mContent; }

	char const* getName() const override final { return "LLCoroResponder"; }
protected:
    LLCoroResponderBase() {}
};

struct LLCoroResponder final : public LLCoroResponderBase
{
	typedef std::function<void(const LLCoroResponder&)> cb_t;
	LLCoroResponder(const cb_t& cb) : mCB(cb) {}
	void httpCompleted() override { mCB(*this); }
private:
	const cb_t mCB;
};

struct LLCoroResponderRaw final : public LLCoroResponderBase
{
	typedef std::function<void(const LLCoroResponderRaw&, const std::string&)> cb_t;
	LLCoroResponderRaw(const cb_t& cb) : mCB(cb) {}
	void completedRaw(const LLChannelDescriptors& channels, const buffer_ptr_t& buffer) override
	{
		std::string content;
		decode_raw_body(channels, buffer, content);
		mCB(*this, content);
	}
private:
	const cb_t mCB;
};

