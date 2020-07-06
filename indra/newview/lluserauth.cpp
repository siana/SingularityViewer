/** 
 * @file lluserauth.cpp
 * @brief LLUserAuth class implementation
 *
 * $LicenseInfo:firstyear=2003&license=viewergpl$
 * 
 * Copyright (c) 2003-2009, Linden Research, Inc.
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

#include "lluserauth.h"

#include <sstream>
#include <iterator>

#include "lldir.h"
#include "llversioninfo.h"
#include "llappviewer.h"
#include "llviewercontrol.h"
#include "llxmlrpcresponder.h"
#include "llsdutil.h"
#include "llhttpclient.h"
#include "stringize.h"

// NOTE: MUST include these after otherincludes since queue gets redefined!?!!
#include <xmlrpc-epi/xmlrpc.h>

#include <curl/curl.h>
#ifdef DEBUG_CURLIO
#include "debug_libcurl.h"
#endif

// Don't define PLATFORM_STRING for unknown platforms - they need
// to get added to the login cgi script, so we want this to cause an
// error if we get compiled for a different platform.
#if LL_WINDOWS
static const char* PLATFORM_STRING = "win";
#elif LL_DARWIN
static const char* PLATFORM_STRING = "mac";
#elif LL_LINUX
static const char* PLATFORM_STRING = "lnx";
#elif LL_SOLARIS
static const char* PLATFORM_STRING = "sol";
#else
#error("Unknown platform defined!")
#endif


LLUserAuth::LLUserAuth() :
	mLastTransferRateBPS(0),
	mResult(LLSD())
{
	mAuthResponse = E_NO_RESPONSE_YET;
}

LLUserAuth::~LLUserAuth()
{
	reset();
}

void LLUserAuth::reset()
{
	mResponder = NULL;
	mResponses.clear();
	mResult.clear();
}

void LLUserAuth::authenticate(
	const std::string& auth_uri,
	const std::string& method,
	const std::string& firstname,
	const std::string& lastname,
	LLUUID web_login_key,
	const std::string& start,
	BOOL skip_optional,
	BOOL accept_tos,
	BOOL accept_critical_message,
	BOOL last_exec_froze, 
	const std::vector<const char*>& requested_options,
	const std::string& hashed_mac,
	const std::string& hashed_volume_serial)
{
	LL_INFOS("AppInit", "Authentication") << "Authenticating: " << firstname << " " << lastname << ", "
			<< /*dpasswd.c_str() <<*/ LL_ENDL;
	std::ostringstream option_str;
	option_str << "Options: ";
	std::ostream_iterator<const char*> appender(option_str, ", ");
	std::copy(requested_options.begin(), requested_options.end(), appender);
	option_str << "END";
	
	LL_INFOS("AppInit", "Authentication") << option_str.str() << LL_ENDL;

	mAuthResponse = E_NO_RESPONSE_YET;
	//mDownloadTimer.reset();
	
	// create the request
	XMLRPC_REQUEST request = XMLRPC_RequestNew();
	XMLRPC_RequestSetMethodName(request, method.c_str());
	XMLRPC_RequestSetRequestType(request, xmlrpc_request_call);

	// stuff the parameters
	XMLRPC_VALUE params = XMLRPC_CreateVector(NULL, xmlrpc_vector_struct);
	XMLRPC_VectorAppendString(params, "first", firstname.c_str(), 0);
	XMLRPC_VectorAppendString(params, "last", lastname.c_str(), 0);
	XMLRPC_VectorAppendString(params, "web_login_key", web_login_key.getString().c_str(), 0);
	XMLRPC_VectorAppendString(params, "start", start.c_str(), 0);
	XMLRPC_VectorAppendString(params, "version", LLVersionInfo::getChannelAndVersion().c_str(), 0); // Includes channel name
	XMLRPC_VectorAppendString(params, "channel", LLVersionInfo::getChannel().c_str(), 0);
	XMLRPC_VectorAppendString(params, "platform", PLATFORM_STRING, 0);
	XMLRPC_VectorAppendString(params, "platform_version", LLAppViewer::instance()->getOSInfo().getOSVersionString().c_str(), 0);

	XMLRPC_VectorAppendString(params, "mac", hashed_mac.c_str(), 0);
	// A bit of security through obscurity: id0 is volume_serial

	XMLRPC_VectorAppendString(params, "id0", hashed_volume_serial.c_str(), 0);
	if (skip_optional)
	{
		XMLRPC_VectorAppendString(params, "skipoptional", "true", 0);
	}
	if (accept_tos)
	{
		XMLRPC_VectorAppendString(params, "agree_to_tos", "true", 0);
	}
	if (accept_critical_message)
	{
		XMLRPC_VectorAppendString(params, "read_critical", "true", 0);
	}
	XMLRPC_VectorAppendInt(params, "last_exec_event", (int) last_exec_froze);

	// append optional requests in an array
	XMLRPC_VALUE options = XMLRPC_CreateVector("options", xmlrpc_vector_array);
	std::vector<const char*>::const_iterator it = requested_options.begin();
	std::vector<const char*>::const_iterator end = requested_options.end();
	for( ; it < end; ++it)
	{
		XMLRPC_VectorAppendString(options, NULL, (*it), 0);
	}
	XMLRPC_AddValueToVector(params, options);

	// put the parameters on the request
	XMLRPC_RequestSetData(request, params);

	mResponder = new XMLRPCResponder;
	LLHTTPClient::postXMLRPC(auth_uri, request, mResponder);

	LL_INFOS("AppInit", "Authentication") << "LLUserAuth::authenticate: uri=" << auth_uri << LL_ENDL;
}


// Legacy version of constructor

// passwd is already MD5 hashed by the time we get to it.
void LLUserAuth::authenticate(
	const std::string& auth_uri,
	const std::string& method,
	const std::string& firstname,
	const std::string& lastname,
	const std::string& passwd,
	const std::string& start,
	BOOL skip_optional,
	BOOL accept_tos,
	BOOL accept_critical_message,
	BOOL last_exec_froze, 
	const std::vector<const char*>& requested_options,
	const std::string& hashed_mac,
	const std::string& hashed_volume_serial)
{
	std::string dpasswd("$1$");
	dpasswd.append(passwd);
	LL_INFOS("AppInit", "Authentication") << "Authenticating: " << firstname << " " << lastname << ", "
			<< /*dpasswd.c_str() <<*/ LL_ENDL;
	std::ostringstream option_str;
	option_str << "Options: ";
	std::ostream_iterator<const char*> appender(option_str, ", ");
	std::copy(requested_options.begin(), requested_options.end(), appender);
	option_str << "END";

	LL_INFOS("AppInit", "Authentication") << option_str.str().c_str() << LL_ENDL;

	mAuthResponse = E_NO_RESPONSE_YET;
	//mDownloadTimer.reset();
	
	// create the request
	XMLRPC_REQUEST request = XMLRPC_RequestNew();
	XMLRPC_RequestSetMethodName(request, method.c_str());
	XMLRPC_RequestSetRequestType(request, xmlrpc_request_call);

	// stuff the parameters
	XMLRPC_VALUE params = XMLRPC_CreateVector(NULL, xmlrpc_vector_struct);
	XMLRPC_VectorAppendString(params, "first", firstname.c_str(), 0);
	XMLRPC_VectorAppendString(params, "last", lastname.c_str(), 0);
	XMLRPC_VectorAppendString(params, "passwd", dpasswd.c_str(), 0);
	XMLRPC_VectorAppendString(params, "start", start.c_str(), 0);
	XMLRPC_VectorAppendString(params, "version", LLVersionInfo::getVersion().c_str(), 0);
	// Singu Note: At the request of Linden Lab we change channel sent to the login server in the following way:
	// * If channel is "Singularity" we change it to "Singularity Release", due to their statistics system
	//   not being able to distinguish just the release version
	// * We append "64" to channel name on 64-bit for systems for the LL stats system to be able to produce independent
	//   crash statistics depending on the architecture
	std::string chan(LLVersionInfo::getChannel());
#if defined(_WIN64) || defined(__x86_64__)
	chan += " 64";
#endif
	XMLRPC_VectorAppendString(params, "channel", chan.c_str(), 0);
	XMLRPC_VectorAppendString(params, "platform", PLATFORM_STRING, 0);
	XMLRPC_VectorAppendString(params, "platform_version", LLAppViewer::instance()->getOSInfo().getOSVersionString().c_str(), 0);

	XMLRPC_VectorAppendString(params, "mac", hashed_mac.c_str(), 0);
	// A bit of security through obscurity: id0 is volume_serial
	// ^^^^^^^^^^^^^^^^^^^^
	// you fucking idiot - charbl

	XMLRPC_VectorAppendString(params, "id0", hashed_volume_serial.c_str(), 0);
	if (skip_optional)
	{
		XMLRPC_VectorAppendString(params, "skipoptional", "true", 0);
	}
	if (accept_tos)
	{
		XMLRPC_VectorAppendString(params, "agree_to_tos", "true", 0);
	}
	if (accept_critical_message)
	{
		XMLRPC_VectorAppendString(params, "read_critical", "true", 0);
	}
	XMLRPC_VectorAppendInt(params, "last_exec_event", (int) last_exec_froze);

	// append optional requests in an array
	XMLRPC_VALUE options = XMLRPC_CreateVector("options", xmlrpc_vector_array);
	std::vector<const char*>::const_iterator it = requested_options.begin();
	std::vector<const char*>::const_iterator end = requested_options.end();
	for( ; it < end; ++it)
	{
		XMLRPC_VectorAppendString(options, NULL, (*it), 0);
	}
	XMLRPC_AddValueToVector(params, options);

	// put the parameters on the request
	XMLRPC_RequestSetData(request, params);

	// Post the XML RPC.
	mResponder = new XMLRPCResponder;
	LLHTTPClient::postXMLRPC(auth_uri, request, mResponder);
	
	LL_INFOS("AppInit", "Authentication") << "LLUserAuth::authenticate: uri=" << auth_uri << LL_ENDL;
}


LLUserAuth::UserAuthcode LLUserAuth::authResponse()
{
	if (!mResponder)
	{
		return mAuthResponse;
	}
	
	bool done = mResponder->is_finished();

	if (!done) {
		if (mResponder->is_downloading())
		{
			mAuthResponse = E_DOWNLOADING;
		}
		
		return mAuthResponse;
	}
	
	mLastTransferRateBPS = mResponder->transferRate();
	mErrorMessage = mResponder->getReason();

	// if curl was ok, parse the download area.
	CURLcode result = mResponder->result_code();
	if (is_internal_http_error(mResponder->getStatus()))
	{
		// result can be a meaningless CURLE_OK in the case of an internal error.
		result = CURLE_FAILED_INIT;		// Just some random error to get the default case below.
	}
	switch (result)
	{
	case CURLE_OK:
		mAuthResponse = parseResponse();
		break;
	case CURLE_COULDNT_RESOLVE_HOST:
		mAuthResponse = E_COULDNT_RESOLVE_HOST;
		break;
	case CURLE_SSL_PEER_CERTIFICATE:
		mAuthResponse = E_SSL_PEER_CERTIFICATE;
		break;
	case CURLE_SSL_CACERT:
		mAuthResponse = E_SSL_CACERT;
		break;
	case CURLE_SSL_CONNECT_ERROR:
		mAuthResponse = E_SSL_CONNECT_ERROR;
		break;
	default:
		mAuthResponse = E_UNHANDLED_ERROR;
		break;
	}

	LL_INFOS("AppInit", "Authentication") << "Processed response: " << result << LL_ENDL;

	// We're done with this data.
	mResponder = NULL;

	return mAuthResponse;
}

LLUserAuth::UserAuthcode LLUserAuth::parseResponse()
{
	// The job of this function is to parse sCurlDownloadArea and
	// extract every member into either the mResponses or
	// mOptions. For now, we will only be looking at mResponses, which
	// will all be string => string pairs.
	UserAuthcode rv = E_UNHANDLED_ERROR;
	XMLRPC_REQUEST response = mResponder->response();
	if(!response)
	{
		U32 status = mResponder->getStatus();
		// Is it an HTTP error?
		if (!(200 <= status && status < 400))
		{
			rv = E_HTTP_SERVER_ERROR;
		}
		return rv;
	}

	// clear out any old parsing
	mResponses.clear();

	// Now, parse everything
    XMLRPC_VALUE param = XMLRPC_RequestGetData(response);
    if (! param)
	{
		LL_DEBUGS() << "Response contains no data" << LL_ENDL;
		return rv;
	}

	// Now, parse everything
	mResponses = parseValues(rv, "", param);
	return rv;
}

LLSD LLUserAuth::parseValues(UserAuthcode &auth_code, const std::string& key_pfx, XMLRPC_VALUE param)
{
	auth_code = E_OK;
	LLSD responses;
	for(XMLRPC_VALUE current = XMLRPC_VectorRewind(param); current;
		current = XMLRPC_VectorNext(param))
	{
		std::string key(XMLRPC_GetValueID(current));
		LL_DEBUGS() << "key: " << key_pfx << key << LL_ENDL;
		XMLRPC_VALUE_TYPE_EASY type = XMLRPC_GetValueTypeEasy(current);
		if(xmlrpc_type_string == type)
		{
			LLSD::String val(XMLRPC_GetValueString(current));
			LL_DEBUGS() << "val: " << val << LL_ENDL;
			responses.insert(key,val);
		}
		else if(xmlrpc_type_int == type)
		{
			LLSD::Integer val(XMLRPC_GetValueInt(current));
			LL_DEBUGS() << "val: " << val << LL_ENDL;
			responses.insert(key,val);
		}
		else if (xmlrpc_type_double == type)
        {
			LLSD::Real val(XMLRPC_GetValueDouble(current));
            LL_DEBUGS() << "val: " << val << LL_ENDL;
			responses.insert(key,val);
		}
		else if(xmlrpc_type_array == type)
		{
			// We expect this to be an array of submaps. Walk the array,
			// recursively parsing each submap and collecting them.
			LLSD array;
			int i = 0;          // for descriptive purposes
			for (XMLRPC_VALUE row = XMLRPC_VectorRewind(current); row;
				row = XMLRPC_VectorNext(current), ++i)
			{
				// Recursive call. For the lower-level key_pfx, if 'key'
				// is "foo", pass "foo[0]:", then "foo[1]:", etc. In the
				// nested call, a subkey "bar" will then be logged as
				// "foo[0]:bar", and so forth.
				// Parse the scalar subkey/value pairs from this array
				// entry into a temp submap. Collect such submaps in 'array'.
				std::string key_prefix = key_pfx;
				array.append(parseValues(auth_code,
									STRINGIZE(key_pfx << key << '[' << i << "]:"),
									row));
			}
			// Having collected an 'array' of 'submap's, insert that whole
			// 'array' as the value of this 'key'.
			responses.insert(key, array);
		}
		else if (xmlrpc_type_struct == type)
    	{
    		LLSD submap = parseValues(auth_code,
            						STRINGIZE(key_pfx << key << ':'),
            						current);
            responses.insert(key, submap);
        }
        else
        {
        	// whoops - unrecognized type
            LL_WARNS() << "Unhandled xmlrpc type " << type << " for key "
                                        << key_pfx << key << LL_ENDL;
            responses.insert(key, STRINGIZE("<bad XMLRPC type " << type << '>'));
            auth_code = E_UNHANDLED_ERROR;
        }
    }
    return responses;
}
		


