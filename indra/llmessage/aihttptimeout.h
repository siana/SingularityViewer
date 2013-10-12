/**
 * @file aihttptimeout.h
 * @brief HTTP timeout control class.
 *
 * Copyright (c) 2012, Aleric Inglewood.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * There are special exceptions to the terms and conditions of the GPL as
 * it is applied to this Source Code. View the full text of the exception
 * in the file doc/FLOSS-exception.txt in this software distribution.
 *
 * CHANGELOG
 *   and additional copyright holders.
 *
 *   28/04/2012
 *   Initial version, written by Aleric Inglewood @ SL
 */

#ifndef AIHTTPTIMEOUT_H
#define AIHTTPTIMEOUT_H

#include <vector>

#ifndef HTTPTIMEOUT_TESTSUITE

#include "llrefcount.h"
#include "aithreadsafe.h"		// AIAccess

#include <curl/curl.h>      // Needed for files that include this header (also for aicurlprivate.h).
#ifdef DEBUG_CURLIO
#include "debug_libcurl.h"
#endif

#else

#include <sys/types.h>

class LLRefCount { };
template<typename T> struct AIAccess;
typedef int CURLcode;

#define ASSERT_ONLY_COMMA(...) , __VA_ARGS__

#endif

class AIHTTPTimeoutPolicy;

// Forward declaration (see aicurlprivate.h).
namespace AICurlPrivate {
  class BufferedCurlEasyRequest;
} // namespace AICurlPrivate

typedef AIAccess<AICurlPrivate::BufferedCurlEasyRequest> AICurlEasyRequest_wat;

namespace AICurlPrivate {

class CurlEasyRequest;
class ThreadSafeBufferedCurlEasyRequest;

namespace curlthread {

// A class that keeps track of timeout administration per connection.
class HTTPTimeout : public LLRefCount {
  private:
	AIHTTPTimeoutPolicy const* mPolicy;			// A pointer to the used timeout policy.
	std::vector<U32> mBuckets;					// An array with the number of bytes transfered in each second.
	U16 mBucket;								// The bucket corresponding to mLastSecond.
	bool mNothingReceivedYet;					// Set when created, reset when the first HTML reply header from the server is received.
	bool mLowSpeedOn;							// Set while uploading or downloading data.
	bool mLastBytesSent;						// Set when the last bytes were sent to libcurl to be uploaded.
	bool mBeingRedirected;						// Set when a 302 header is received, reset when upload finished is detected.
	bool mUploadFinished;						// Used to keep track of whether upload_finished was called yet.
	S32 mLastSecond;							// The time at which lowspeed() was last called, in seconds since mLowSpeedClock.
	S32 mOverwriteSecond;						// The second at which the first bucket of this transfer will be overwritten.
	U32 mTotalBytes;							// The sum of all bytes in mBuckets.
	U64 mLowSpeedClock;							// The time (sTime_10ms) at which low speed detection (re)started.
	U64 mStalled;								// The time (sTime_10ms) at which this transaction is considered to be stalling if nothing is transfered anymore.
  public:
	static F64 const sClockWidth_10ms;			// Time between two clock ticks in 10 ms units.
	static F64 const sClockWidth_40ms;			// Time between two clock ticks in 40 ms units.
	static U64 sTime_10ms;						// Time since the epoch in 10 ms units.
#if defined(CWDEBUG) || defined(DEBUG_CURLIO)
	ThreadSafeBufferedCurlEasyRequest* mLockObj;
#endif

  public:
	HTTPTimeout(AIHTTPTimeoutPolicy const* policy, ThreadSafeBufferedCurlEasyRequest* lock_obj) :
		mPolicy(policy), mNothingReceivedYet(true), mLowSpeedOn(false), mLastBytesSent(false), mBeingRedirected(false), mUploadFinished(false), mStalled((U64)-1)
#if defined(CWDEBUG) || defined(DEBUG_CURLIO)
		, mLockObj(lock_obj)
#endif
		{ }

	// Called when a redirect header is received.
	void being_redirected(void);

	// Called when curl makes the socket writable (again).
	void upload_starting(void);

	// Called when everything we had to send to the server has been sent.
	void upload_finished(void);

	// Called when data is sent. Returns true if transfer timed out.
	bool data_sent(size_t n, bool finished);

	// Called when data is received. Returns true if transfer timed out.
	bool data_received(size_t n/*,*/ ASSERT_ONLY_COMMA(bool upload_error_status = false));

	// Called immediately before done() after curl finished, with code.
	void done(AICurlEasyRequest_wat const& curlEasyRequest_w, CURLcode code);

	// Returns true when we REALLY timed out. Might call upload_finished heuristically.
	bool has_stalled(void) { return mStalled < sTime_10ms && !maybe_upload_finished(); }

	// Called from BufferedCurlEasyRequest::processOutput if a timeout occurred.
	void print_diagnostics(CurlEasyRequest const* curl_easy_request, char const* eff_url);

#if defined(CWDEBUG) || defined(DEBUG_CURLIO)
	void* get_lockobj(void) const { return mLockObj; }
#endif

  private:
	// (Re)start low speed transer rate detection.
	void reset_lowspeed(void);

	// Common low speed detection, Called from data_sent or data_received.
	bool lowspeed(size_t bytes, bool finished = false);

	// Return false when we timed out on reply delay, or didn't sent all bytes yet.
	// Otherwise calls upload_finished() and return true;
	bool maybe_upload_finished(void);
};

} // namespace curlthread
} // namespace AICurlPrivate

#if defined(CWDEBUG) || defined(DEBUG_CURLIO)
extern bool gCurlIo;
#endif

#endif

