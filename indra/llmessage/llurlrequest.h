/** 
 * @file llurlrequest.h
 * @author Phoenix
 * @date 2005-04-21
 * @brief Declaration of url based requests on pipes.
 *
 * $LicenseInfo:firstyear=2005&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 * Copyright (C) 2012, Aleric Inglewood.
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

#ifndef LL_LLURLREQUEST_H
#define LL_LLURLREQUEST_H

/** 
 * This file holds the declaration of useful classes for dealing with
 * url based client requests. 
 */

#include <string>
#include "aicurleasyrequeststatemachine.h"
#include "aihttpheaders.h"

class Injector
{
  public:
	typedef LLHTTPClient::ResponderBase::buffer_ptr_t buffer_ptr_t;
	virtual char const* contentType(void) const = 0;
	virtual U32 get_body(LLChannelDescriptors const& channels, buffer_ptr_t& buffer) = 0;
	// To avoid compiler warning.
	virtual ~Injector() { }
};

class LLURLRequest : public AICurlEasyRequestStateMachine {
  public:
	typedef LLHTTPClient::ERequestAction ERequestAction;

	/**
	 * @brief Turn the request action into an http verb.
	 */
	static std::string actionAsVerb(ERequestAction action);

	/** 
	 * @brief Constructor.
	 *
	 * @param action One of the ERequestAction enumerations.
	 * @param url The url of the request. It should already be encoded.
	 */
	LLURLRequest(ERequestAction action, std::string const& url, Injector* body,
				 LLHTTPClient::ResponderPtr responder, AIHTTPHeaders& headers,
				 AIPerService::Approvement* approved,
				 bool keepalive, bool is_auth, bool no_compression);

	/**
	 * @brief Cached value of responder->getName() as passed to the constructor.
	 */
	char const* getResponderName(void) const { return mResponderNameCache; }

  protected:
	// Call abort(), not delete.
	/*virtual*/ ~LLURLRequest() { }

  public:
    /**
     * @ brief Turn off (or on) the CURLOPT_PROXY header.
     */
    void useProxy(bool use_proxy);

	/** 
	 * @brief Add a header to the http post.
	 *
	 * The header must be correctly formatted for HTTP requests. This
	 * provides a raw interface if you know what kind of request you
	 * will be making during construction of this instance. All
	 * required headers will be automatically constructed, so this is
	 * usually useful for encoding parameters.
	 */
	void addHeader(char const* header);

  private:
	/** 
	 * @brief Handle action specific url request configuration.
	 *
	 * @return Returns true if this is configured.
	 */
	bool configure(AICurlEasyRequest_wat const& curlEasyRequest_w);

  private:
	ERequestAction mAction;
	std::string mURL;
	bool mKeepAlive;					// Set to false only when explicitely requested.
	bool mIsAuth;						// Set for authentication messages (login, buy land, buy currency).
	bool mNoCompression;				// Set to disable using gzip.
	Injector* mBody;					// Non-zero iff the action is HTTP_POST and HTTP_PUT.
	U32 mBodySize;
	LLHTTPClient::ResponderPtr mResponder;
	AIHTTPHeaders mHeaders;
	char const* mResponderNameCache;

  protected:
	// Handle initializing the object.
	/*virtual*/ void initialize_impl(void);
};

#endif // LL_LLURLREQUEST_H
