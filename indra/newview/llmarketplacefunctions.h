/**
 * @file llmarketplacefunctions.h
 * @brief Miscellaneous marketplace-related functions and classes
 * class definition
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
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

#ifndef LL_LLMARKETPLACEFUNCTIONS_H
#define LL_LLMARKETPLACEFUNCTIONS_H


#include <llsd.h>
#include <boost/function.hpp>
#include <boost/signals2.hpp>

#include "llsingleton.h"
#include "llstring.h"
#include "llhttpstatuscodes.h"


LLSD getMarketplaceStringSubstitutions();


namespace MarketplaceErrorCodes
{
	enum eCode
	{
		IMPORT_DONE = HTTP_OK,
		IMPORT_PROCESSING = HTTP_ACCEPTED,
		IMPORT_REDIRECT = HTTP_FOUND,
		IMPORT_BAD_REQUEST = HTTP_BAD_REQUEST,
		IMPORT_AUTHENTICATION_ERROR = HTTP_UNAUTHORIZED,
		IMPORT_FORBIDDEN = HTTP_FORBIDDEN,
		IMPORT_NOT_FOUND = HTTP_NOT_FOUND,
		IMPORT_DONE_WITH_ERRORS = HTTP_CONFLICT,
		IMPORT_JOB_FAILED = HTTP_GONE,
		IMPORT_JOB_LOW_SPEED = HTTP_INTERNAL_ERROR_LOW_SPEED,
		IMPORT_JOB_TIMEOUT = HTTP_INTERNAL_ERROR_CURL_TIMEOUT,
		IMPORT_SERVER_SITE_DOWN = HTTP_INTERNAL_SERVER_ERROR,
		IMPORT_SERVER_API_DISABLED = HTTP_SERVICE_UNAVAILABLE,
	};
}

namespace MarketplaceStatusCodes
{
	enum sCode
	{
		MARKET_PLACE_NOT_INITIALIZED = 0,
		MARKET_PLACE_INITIALIZING = 1,
		MARKET_PLACE_CONNECTION_FAILURE = 2,
		MARKET_PLACE_MERCHANT = 3,
		MARKET_PLACE_NOT_MERCHANT = 4,
	};
}


class LLMarketplaceInventoryImporter
	: public LLSingleton<LLMarketplaceInventoryImporter>
{
public:
	static void update();

	LLMarketplaceInventoryImporter();

	typedef boost::signals2::signal<void (bool)> status_changed_signal_t;
	typedef boost::signals2::signal<void (U32, const LLSD&)> status_report_signal_t;

	boost::signals2::connection setInitializationErrorCallback(const status_report_signal_t::slot_type& cb);
	boost::signals2::connection setStatusChangedCallback(const status_changed_signal_t::slot_type& cb);
	boost::signals2::connection setStatusReportCallback(const status_report_signal_t::slot_type& cb);

	void initialize();
	bool triggerImport();
	bool isImportInProgress() const { return mImportInProgress; }
	U32 getMarketPlaceStatus() const { return mMarketPlaceStatus; }

protected:
	void reinitializeAndTriggerImport();
	void updateImport();

private:
	bool mAutoTriggerImport;
	bool mImportInProgress;
	bool mInitialized;
	U32 mMarketPlaceStatus;

	status_report_signal_t *	mErrorInitSignal;
	status_changed_signal_t *	mStatusChangedSignal;
	status_report_signal_t *	mStatusReportSignal;
};



#endif // LL_LLMARKETPLACEFUNCTIONS_H

