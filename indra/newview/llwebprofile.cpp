/** 
 * @file llwebprofile.cpp
 * @brief Web profile access.
 *
 * $LicenseInfo:firstyear=2011&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2011, Linden Research, Inc.
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

#include "llviewerprecompiledheaders.h"

#include "llwebprofile.h"

// libs
#include "llbufferstream.h"
#include "llhttpclient.h"
#include "llimagepng.h"
//#include "llplugincookiestore.h"

// newview
#include "llpanelprofile.h"		// <edit>getProfileURL (this is the original location LL put it).</edit>
#include "llviewermedia.h" // FIXME: don't use LLViewerMedia internals

#include "llsdjson.h"

/*
 * Workflow:
 * 1. LLViewerMedia::setOpenIDCookie()
 *    -> GET https://my-demo.secondlife.com/ via LLViewerMediaWebProfileResponder
 *    -> LLWebProfile::setAuthCookie()
 * 2. LLWebProfile::uploadImage()
 *    -> GET "https://my-demo.secondlife.com/snapshots/s3_upload_config" via ConfigResponder
 * 3. LLWebProfile::post()
 *    -> POST <config_url> via PostImageResponder
 *    -> redirect
 *    -> GET <redirect_url> via PostImageRedirectResponder
 */

///////////////////////////////////////////////////////////////////////////////
// LLWebProfileResponders::ConfigResponder

extern AIHTTPTimeoutPolicy webProfileResponders_timeout;

class LLWebProfileResponders::ConfigResponder : public LLHTTPClient::ResponderWithCompleted
{
	LOG_CLASS(LLWebProfileResponders::ConfigResponder);

public:
	ConfigResponder(LLPointer<LLImageFormatted> imagep)
	:	mImagep(imagep)
	{
	}

	/*virtual*/ void completedRaw(LLChannelDescriptors const& channels, buffer_ptr_t const& buffer)
	{
		LLBufferStream istr(channels, buffer.get());
		std::stringstream strstrm;
		strstrm << istr.rdbuf();
		const std::string body = strstrm.str();

		if (mStatus != HTTP_OK)
		{
			LL_WARNS() << "Failed to get upload config (" << mStatus << ')' << LL_ENDL;
			LLWebProfile::reportImageUploadStatus(false);
			return;
		}

		auto root = LlsdFromJsonString(body);
		if (root.isUndefined())
		{
			LL_WARNS() << "Failed to get valid json body" << LL_ENDL;
			LLWebProfile::reportImageUploadStatus(false);
			return;
		}

		// *TODO: 404 = not supported by the grid
		// *TODO: increase timeout or handle HTTP_INTERNAL_ERROR_* time errors.

		// Convert config to LLSD.
		const auto data = root["data"];
		const std::string upload_url = root["url"].asString();
		LLSD config = data;
		if (!data.has("add_loc")) config["add_loc"] = "0";
		if (!data.has("caption")) config["caption"] = LLStringUtil::null;

		// Do the actual image upload using the configuration.
		LL_DEBUGS("Snapshots") << "Got upload config, POSTing image to " << upload_url << ", config=[" << config << ']' << LL_ENDL;
		LLWebProfile::post(mImagep, config, upload_url);
	}

protected:
	/*virtual*/ AIHTTPTimeoutPolicy const& getHTTPTimeoutPolicy(void) const { return webProfileResponders_timeout; }
	/*virtual*/ char const* getName(void) const { return "LLWebProfileResponders::ConfigResponder"; }

private:
	LLPointer<LLImageFormatted> mImagep;
};

///////////////////////////////////////////////////////////////////////////////
// LLWebProfilePostImageRedirectResponder
class LLWebProfileResponders::PostImageRedirectResponder : public LLHTTPClient::ResponderWithCompleted
{
	LOG_CLASS(LLWebProfileResponders::PostImageRedirectResponder);

public:
	/*virtual*/ void completedRaw(LLChannelDescriptors const& channels, buffer_ptr_t const& buffer)
	{
		if (mStatus != HTTP_OK)
		{
			LL_WARNS() << "Failed to upload image: " << mStatus << ' ' << mReason << LL_ENDL;
			LLWebProfile::reportImageUploadStatus(false);
			return;
		}

		LLBufferStream istr(channels, buffer.get());
		std::stringstream strstrm;
		strstrm << istr.rdbuf();
		const std::string body = strstrm.str();
		LL_INFOS() << "Image uploaded." << LL_ENDL;
		LL_DEBUGS("Snapshots") << "Uploading image succeeded. Response: [" << body << ']' << LL_ENDL;
		LLWebProfile::reportImageUploadStatus(true);
	}

protected:
	/*virtual*/ AIHTTPTimeoutPolicy const& getHTTPTimeoutPolicy(void) const { return webProfileResponders_timeout; }
	/*virtual*/ char const* getName(void) const { return "LLWebProfileResponders::PostImageRedirectResponder"; }

private:
	LLPointer<LLImageFormatted> mImagep;
};


///////////////////////////////////////////////////////////////////////////////
// LLWebProfileResponders::PostImageResponder
class LLWebProfileResponders::PostImageResponder : public LLHTTPClient::ResponderWithCompleted
{
	LOG_CLASS(LLWebProfileResponders::PostImageResponder);

public:
	/*virtual*/ bool needsHeaders(void) const { return true; }

	/*virtual*/ void completedHeaders(void)
	{
		// Server abuses 303 status; Curl can't handle it because it tries to resent
		// the just uploaded data, which fails
		// (CURLE_SEND_FAIL_REWIND: Send failed since rewinding of the data stream failed).
		// Handle it manually.
		if (mStatus == HTTP_SEE_OTHER)
		{
			AIHTTPHeaders headers;
			headers.addHeader("Accept", "*/*");
			headers.addHeader("Cookie", LLWebProfile::getAuthCookie());
			headers.addHeader("User-Agent", LLViewerMedia::getCurrentUserAgent());
			std::string redir_url;
			mReceivedHeaders.getFirstValue("location", redir_url);
			LL_DEBUGS("Snapshots") << "Got redirection URL: " << redir_url << LL_ENDL;
			LLHTTPClient::get(redir_url, new LLWebProfileResponders::PostImageRedirectResponder, headers);
		}
		else
		{
			LL_WARNS() << "Unexpected POST status: " << mStatus << ' ' << mReason << LL_ENDL;
			LL_DEBUGS("Snapshots") << "received_headers: [" << mReceivedHeaders << ']' << LL_ENDL;
			LLWebProfile::reportImageUploadStatus(false);
		}
	}

	// Override just to suppress warnings.
	/*virtual*/ void completedRaw(LLChannelDescriptors const& channels, buffer_ptr_t const& buffer)
	{
	}

protected:
	/*virtual*/ bool pass_redirect_status(void) const { return true; }
	/*virtual*/ AIHTTPTimeoutPolicy const& getHTTPTimeoutPolicy(void) const { return webProfileResponders_timeout; }
	/*virtual*/ char const* getName(void) const { return "LLWebProfileResponders::PostImageResponder"; }
};

///////////////////////////////////////////////////////////////////////////////
// LLWebProfile

std::string LLWebProfile::sAuthCookie;
LLWebProfile::status_callback_t LLWebProfile::mStatusCallback;

// static
void LLWebProfile::uploadImage(LLPointer<LLImageFormatted> image, const std::string& caption, bool add_location)
{
	// Get upload configuration data.
	std::string config_url(getProfileURL(LLStringUtil::null) + "snapshots/s3_upload_config");
	config_url += "?caption=" + LLURI::escape(caption);
	config_url += "&add_loc=" + std::string(add_location ? "1" : "0");

	LL_DEBUGS("Snapshots") << "Requesting " << config_url << LL_ENDL;
	AIHTTPHeaders headers;
	headers.addHeader("Accept", "*/*");
	headers.addHeader("Cookie", LLWebProfile::getAuthCookie());
	headers.addHeader("User-Agent", LLViewerMedia::getCurrentUserAgent());
	LLHTTPClient::get(config_url, new LLWebProfileResponders::ConfigResponder(image), headers);
}

// static
void LLWebProfile::setAuthCookie(const std::string& cookie)
{
	LL_DEBUGS("Snapshots") << "Setting auth cookie: " << cookie << LL_ENDL;
	sAuthCookie = cookie;
}

// static
void LLWebProfile::post(LLPointer<LLImageFormatted> image, const LLSD& config, const std::string& url)
{
	if (dynamic_cast<LLImagePNG*>(image.get()) == 0)
	{
		LL_WARNS() << "Image to upload is not a PNG" << LL_ENDL;
		llassert(dynamic_cast<LLImagePNG*>(image.get()) != 0);
		return;
	}

	const std::string boundary = "----------------------------0123abcdefab";

	AIHTTPHeaders headers;
	headers.addHeader("Accept", "*/*");
	headers.addHeader("Cookie", LLWebProfile::getAuthCookie());
	headers.addHeader("User-Agent", LLViewerMedia::getCurrentUserAgent());
	headers.addHeader("Content-Type", "multipart/form-data; boundary=" + boundary);

	std::ostringstream body;

	// *NOTE: The order seems to matter.
	body	<< "--" << boundary << "\r\n"
			<< "Content-Disposition: form-data; name=\"key\"\r\n\r\n"
			<< config["key"].asString() << "\r\n";

	body	<< "--" << boundary << "\r\n"
			<< "Content-Disposition: form-data; name=\"AWSAccessKeyId\"\r\n\r\n"
			<< config["AWSAccessKeyId"].asString() << "\r\n";

	body	<< "--" << boundary << "\r\n"
			<< "Content-Disposition: form-data; name=\"acl\"\r\n\r\n"
			<< config["acl"].asString() << "\r\n";

	body	<< "--" << boundary << "\r\n"
			<< "Content-Disposition: form-data; name=\"Content-Type\"\r\n\r\n"
			<< config["Content-Type"].asString() << "\r\n";

	body	<< "--" << boundary << "\r\n"
			<< "Content-Disposition: form-data; name=\"policy\"\r\n\r\n"
			<< config["policy"].asString() << "\r\n";

	body	<< "--" << boundary << "\r\n"
			<< "Content-Disposition: form-data; name=\"signature\"\r\n\r\n"
			<< config["signature"].asString() << "\r\n";

	body	<< "--" << boundary << "\r\n"
			<< "Content-Disposition: form-data; name=\"success_action_redirect\"\r\n\r\n"
			<< config["success_action_redirect"].asString() << "\r\n";

	body	<< "--" << boundary << "\r\n"
			<< "Content-Disposition: form-data; name=\"file\"; filename=\"snapshot.png\"\r\n"
			<< "Content-Type: image/png\r\n\r\n";
	size_t const body_size = body.str().size();

	std::ostringstream footer;
	footer << "\r\n--" << boundary << "--\r\n";
	size_t const footer_size = footer.str().size();

	size_t size = body_size + image->getDataSize() + footer_size;
	// postRaw() takes ownership of the buffer and releases it later.
	U8* data = new U8 [size];
	memcpy(data, body.str().data(), body_size);
	// Insert the image data.
	memcpy(data + body_size, image->getData(), image->getDataSize());
	memcpy(data + body_size + image->getDataSize(), footer.str().data(), footer_size);

	// Send request, successful upload will trigger posting metadata.
	LLHTTPClient::postRaw(url, data, size, new LLWebProfileResponders::PostImageResponder(), headers/*,*/ DEBUG_CURLIO_PARAM(debug_off), no_keep_alive);
}

// static
void LLWebProfile::reportImageUploadStatus(bool ok)
{
	if (mStatusCallback)
	{
		mStatusCallback(ok);
	}
}

// static
std::string LLWebProfile::getAuthCookie()
{
	// This is needed to test image uploads on Linux viewer built with OpenSSL 1.0.0 (0.9.8 works fine).
	const char* debug_cookie = getenv("LL_SNAPSHOT_COOKIE");
	return debug_cookie ? debug_cookie : sAuthCookie;
}
