/**
 * @file aihttptimeout.cpp
 * @brief Implementation of HTTPTimeout
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

#ifndef HTTPTIMEOUT_TESTSUITE

#include "linden_common.h"
#include "aihttptimeoutpolicy.h"
#include "lltimer.h"		// calc_clock_frequency()
#include "aicurl.h"			// DoutCurl

#undef AICurlPrivate

#else

#include "../llcommon/stdtypes.h"
#include <iostream>
#include <cassert>

#define llassert assert
#define llassert_always assert
#define DoutCurl(...) do { std::cout << __VA_ARGS__ ; } while(0)
#define Debug(...) do { } while(0)
#define llmin std::min
#define llwarns std::cout
#define llendl std::endl
int const CURLE_OPERATION_TIMEDOUT  = 1;
int const CURLE_COULDNT_RESOLVE_HOST = 2;
int const CURLINFO_NAMELOOKUP_TIME = 3;

F64 calc_clock_frequency(void) { return 1000000.0; } 

template<typename T>
struct AIAccess {
  T* mP;
  AIAccess(T* p) : mP(p) { }
  T* operator->() const { return mP; }
};

struct AIHTTPTimeoutPolicy {
  U16 getReplyDelay(void) const { return 60; }
  U16 getLowSpeedTime(void) const { return 30; }
  U32 getLowSpeedLimit(void) const { return 7000; }
  static bool connect_timed_out(std::string const&) { return false; }
};

namespace AICurlPrivate {

class BufferedCurlEasyRequest {
public:
  char const* getLowercaseHostname(void) const { return "hostname.com"; }
  char const* getLowercaseServicename(void) const { return "hostname.com:12047"; }
  void getinfo(const int&, double* p) { *p = 0.1; }
};

}

#endif // HTTPTIMEOUT_TESTSUITE

#include "aihttptimeout.h"

// If this is set, treat dc::curlio as off in the assertion below.
#if defined(CWDEBUG) || defined(DEBUG_CURLIO)
bool gCurlIo;
#endif

namespace AICurlPrivate {
namespace curlthread {

//-----------------------------------------------------------------------------
// HTTPTimeout

//static
F64 const HTTPTimeout::sClockWidth_10ms = 100.0 / calc_clock_frequency();		// Time between two clock ticks, in 10ms units.
F64 const HTTPTimeout::sClockWidth_40ms = HTTPTimeout::sClockWidth_10ms * 0.25;	// Time between two clock ticks, in 40ms units.
U64 HTTPTimeout::sTime_10ms;													// Time in 10ms units, set once per select() exit.

// CURL-THREAD
// This is called when body data was sent to the server socket.
//                                                    <-----mLowSpeedOn------>
// queued--><--DNS lookup + connect + send headers-->[<--send body (if any)-->]<--replydelay--><--receive headers + body--><--done
//                                                    ^ ^ ^       ^   ^      ^
//                                                    | | |       |   |      |
bool HTTPTimeout::data_sent(size_t n, bool finished)
{
  // Generate events.
  if (!mLowSpeedOn)
  {
	// If we can send data (for the first time) then that's our only way to know we connected.
	reset_lowspeed();
  }
  // Detect low speed.
  return lowspeed(n, finished);
}

// CURL-THREAD
// This is called when the 'low speed' timer should be started.
//                                                    <-----mLowSpeedOn------>                 <-------mLowSpeedOn-------->
// queued--><--DNS lookup + connect + send headers-->[<--send body (if any)-->]<--replydelay--><--receive headers + body--><--done
//                                                    ^                                        ^
//                                                    |                                        |
void HTTPTimeout::reset_lowspeed(void)
{
  mLowSpeedClock = sTime_10ms;
  mLowSpeedOn = true;
  mLastBytesSent = false;	// We're just starting!
  mLastSecond = -1;			// This causes lowspeed to initialize the rest.
  mStalled = (U64)-1;		// Stop reply delay timer.
  DoutCurl("reset_lowspeed: mLowSpeedClock = " << mLowSpeedClock << "; mStalled = -1");
}

void HTTPTimeout::being_redirected(void)
{
  mBeingRedirected = true;
}

void HTTPTimeout::upload_starting(void)
{
  // We're not supposed start with an upload when it already finished, unless we're being redirected.
  llassert(!mUploadFinished || mBeingRedirected);
  mUploadFinished = false;
  // Apparently there is something to upload. Start detecting low speed timeouts.
  reset_lowspeed();
}

// CURL-THREAD
// This is called when everything we had to send to the server has been sent.
//                                                    <-----mLowSpeedOn------>
// queued--><--DNS lookup + connect + send headers-->[<--send body (if any)-->]<--replydelay--><--receive headers + body--><--done
//                                                                             ^
//                                                                             |
void HTTPTimeout::upload_finished(void)
{
  // Disable this assert when there isn't enough debug output to do anything with it.
#if defined(CWDEBUG) || defined(DEBUG_CURLIO)
  llassert(!mUploadFinished);	// If we get here twice, then the 'upload finished' detection failed.
#endif
  mUploadFinished = true;
  // Only accept a call to upload_starting() if being_redirected() is called after this point.
  mBeingRedirected = false;
  // We finished uploading (if there was a body to upload at all), so no more transfer rate timeouts.
  mLowSpeedOn = false;
  // Timeout if the server doesn't reply quick enough.
  mStalled = sTime_10ms + 100 * mPolicy->getReplyDelay();
  DoutCurl("upload_finished: mStalled set to Time_10ms (" << sTime_10ms << ") + " << (mStalled - sTime_10ms) << " (" << mPolicy->getReplyDelay() << " seconds)");
}

// CURL-THREAD
// This is called when data was received from the server.
//
//          <--------------------------------mNothingReceivedYet------------------------------><-------mLowSpeedOn-------->
// queued--><--DNS lookup + connect + send headers-->[<--send body (if any)-->]<--replydelay--><--receive headers + body--><--done
//                                                                                             ^  ^   ^     ^    ^  ^ ^   ^
//                                                                                             |  |   |     |    |  | |   |
bool HTTPTimeout::data_received(size_t n/*,*/
#if defined(CWDEBUG) || defined(DEBUG_CURLIO)
	ASSERT_ONLY_COMMA(bool upload_error_status)
#else
	ASSERT_ONLY_COMMA(bool)
#endif
	)
{
  // The HTTP header of the reply is the first thing we receive.
  if (mNothingReceivedYet && n > 0)
  {
	if (!mUploadFinished)
	{
	  // mUploadFinished not being set this point should only happen for GET requests (in fact, then it is normal),
	  // because in that case it is impossible to detect the difference between connecting and waiting for a reply without
	  // using CURLOPT_DEBUGFUNCTION. Note that mDebugIsHeadOrGetMethod is only valid when the debug channel 'curlio' is on,
	  // because it is set in the debug callback function.
	  // This is also normal if we received a HTTP header with an error status, since that can interrupt our upload.
	  Debug(llassert(upload_error_status || AICurlEasyRequest_wat(*mLockObj)->mDebugIsHeadOrGetMethod || !dc::curlio.is_on() || gCurlIo));
	  // 'Upload finished' detection failed, generate it now.
	  upload_finished();
	}
	// Mark that something was received.
	mNothingReceivedYet = false;
	// We received something; switch to getLowSpeedLimit()/getLowSpeedTime().
	reset_lowspeed();
  }
  return mLowSpeedOn ? lowspeed(n) : false;
}

// CURL_THREAD
// bytes is the number of bytes we just sent or received (including headers).
// Returns true if the transfer should be aborted.
//
// queued--><--DNS lookup + connect + send headers-->[<--send body (if any)-->]<--replydelay--><--receive headers + body--><--done
//                                                    ^ ^ ^       ^   ^      ^                 ^  ^   ^     ^    ^  ^ ^   ^
//                                                    | | |       |   |      |                 |  |   |     |    |  | |   |
bool HTTPTimeout::lowspeed(size_t bytes, bool finished)
{
  //DoutCurlEntering("HTTPTimeout::lowspeed(" << bytes << ", " << finished << ")");		commented out... too spammy for normal use.

  // The algorithm to determine if we timed out if different from how libcurls CURLOPT_LOW_SPEED_TIME works.
  //
  // libcurl determines the transfer rate since the last call to an equivalent 'lowspeed' function, and then
  // triggers a timeout if CURLOPT_LOW_SPEED_TIME long such a transfer value is less than CURLOPT_LOW_SPEED_LIMIT.
  // That doesn't work right because once there IS data it can happen that this function is called a few
  // times (with less than a milisecond in between) causing seemingly VERY high "transfer rate" spikes.
  // The only correct way to determine the transfer rate is to actually average over CURLOPT_LOW_SPEED_TIME
  // seconds.
  //
  // We do this as follows: we create low_speed_time (in seconds) buckets and fill them with the number
  // of bytes received during that second. We also keep track of the sum of all bytes received between 'now'
  // and 'llmax(starttime, now - low_speed_time)'. Then if that period reaches at least low_speed_time
  // seconds (when now >= starttime + low_speed_time) and the transfer rate (sum / low_speed_time) is
  // less than low_speed_limit, we abort.

  // When are we?
  S32 second = (sTime_10ms - mLowSpeedClock) / 100;
  // This REALLY should never happen, but due to another bug it did happened
  // and caused something so evil and hard to find that... NEVER AGAIN!
  llassert(second >= 0);

  // finished should be false until the very last call to this function.
  mLastBytesSent = finished;

  // If this is the same second as last time, just add the number of bytes to the current bucket.
  if (second == mLastSecond)
  {
	mTotalBytes += bytes;
	mBuckets[mBucket] += bytes;
	return false;
  }

  // We arrived at a new second.
  // The below is at most executed once per second, even though for
  // every currently connected transfer, CPU is not a big issue.

  // Determine the number of buckets needed and increase the number of buckets if needed.
  U16 const low_speed_time = mPolicy->getLowSpeedTime();
  if (low_speed_time > mBuckets.size())
  {
	mBuckets.resize(low_speed_time, 0);
  }

  S32 s = mLastSecond;
  mLastSecond = second;

  // If this is the first time this function is called, we need to do some initialization.
  if (s == -1)
  {
	mBucket = 0;				// It doesn't really matter where we start.
	mOverwriteSecond = second + low_speed_time;
	mTotalBytes = bytes;
	mBuckets[mBucket] = bytes;
	return false;
  }

  // Update all administration.
  U16 bucket = mBucket;
  while(1)		// Run over all the seconds that were skipped.
  {
	if (++bucket == low_speed_time)
	  bucket = 0;
	if (++s == second)
	  break;
	if (s >= mOverwriteSecond)
	{
	  mTotalBytes -= mBuckets[bucket];
	}
	mBuckets[bucket] = 0;
  }
  mBucket = bucket;
  if (s >= mOverwriteSecond)
  {
	mTotalBytes -= mBuckets[mBucket];
  }
  mTotalBytes += bytes;
  mBuckets[mBucket] = bytes;

  // Check if we timed out.
  U32 const low_speed_limit = mPolicy->getLowSpeedLimit();	// In bytes/s
  U32 mintotalbytes = low_speed_limit * low_speed_time;		// In bytes.
  DoutCurl("Transfered " << mTotalBytes << " bytes in " << llmin(second, (S32)low_speed_time) << " seconds after " << second << " second" << ((second == 1) ? "" : "s") << ".");
  if (second >= low_speed_time)
  {
	DoutCurl("Average transfer rate is " << (mTotalBytes / low_speed_time) << " bytes/s (low speed limit is " << low_speed_limit << " bytes/s)");
	if (mTotalBytes < mintotalbytes)
	{
	  if (finished)
	  {
		llwarns <<
#ifdef CWDEBUG
		(void*)get_lockobj() << ": "
#endif
		"Transfer rate timeout (average transfer rate below " << low_speed_limit <<
		" bytes/s for more than " << low_speed_time << " second" << ((low_speed_time == 1) ? "" : "s") <<
		") but we just sent the LAST bytes! Waiting an additional 4 seconds." << llendl;
		// Lets hope these last bytes will make it and do not time out on transfer speed anymore.
		// Just give these bytes 4 more seconds to be written to the socket (after which we'll
		// assume that the 'upload finished' detection failed and we'll wait another ReplyDelay
		// seconds before finally, actually timing out.
		mStalled = sTime_10ms + 400;		// 4 seconds into the future.
		DoutCurl("mStalled set to sTime_10ms (" << sTime_10ms << ") + 400 (4 seconds)");
		return false;
	  }
	  // The average transfer rate over the passed low_speed_time seconds is too low. Abort the transfer.
	  llwarns <<
#ifdef CWDEBUG
		(void*)get_lockobj() << ": "
#endif
		"aborting slow connection (average transfer rate below " << low_speed_limit <<
		" bytes/s for more than " << low_speed_time << " second" << ((low_speed_time == 1) ? "" : "s") << ")." << llendl;
	  // This causes curl to exit with CURLE_WRITE_ERROR.
	  return true;
	}
  }

  // Calculate how long the data transfer may stall until we should timeout.
  //
  //                Assume 6 buckets:  0 1 2 3 4 5  (low_speed_time == 6)
  // Seconds since start of transfer:  4 5 6 7 8 9  (mOverwriteSecond == 10)
  //                  Current second:    ^
  //                 Data in buckets:  A B w x y z  (mTotalBytes = A + B; w, x, y and z are undefined)
  //
  // Obviously, we need to stall at LEAST till second low_speed_time before we can "timeout".
  // And possibly more if mTotalBytes is already >= mintotalbytes.
  //
  // The code below finds 'max_stall_time', so that when from now on the buckets
  // are filled with 0, then at 'second + max_stall_time' we should time out,
  // meaning that the resulting mTotalBytes after writing 0 at that second
  // will be less than mintotalbytes and 'second + max_stall_time' >= low_speed_time.
  //
  llassert_always(mintotalbytes > 0);
  S32 max_stall_time = low_speed_time - second;	// Minimum value.
  // Note that if max_stall_time <= 0 here, then second >= low_speed_time and
  // thus mTotalBytes >= mintotalbytes because we didn't timeout already above.
  if (mTotalBytes >= mintotalbytes)
  {
	// In this case max_stall_time has a minimum value equal to when we will reach mOverwriteSecond,
	// because that is the first second at which mTotalBytes will decrease.
	max_stall_time = mOverwriteSecond - second - 1;
	U32 total_bytes = mTotalBytes;
	int bucket = -1;		// Must be one less as the start bucket, which corresponds with mOverwriteSecond (and thus with the current max_stall_time).
	do
	{
	  ++max_stall_time;					// Next second (mOverwriteSecond upon entry of the loop).
	  ++bucket;							// The next bucket (0 upon entry of the loop).
	  // Once we reach the end of the vector total_bytes MUST have reached 0 exactly and we should have left this loop.
	  llassert_always(bucket < low_speed_time);
	  total_bytes -= mBuckets[bucket];	// Empty this bucket.
	}
	while(total_bytes >= mintotalbytes);
  }
  // If this function isn't called again within max_stall_time seconds, we stalled.
  mStalled = sTime_10ms + 100 * max_stall_time;
  DoutCurl("mStalled set to sTime_10ms (" << sTime_10ms << ") + " << (mStalled - sTime_10ms) << " (" << max_stall_time << " seconds)");

  return false;
}

// CURL-THREAD
// This is called immediately before done() after curl finished, with code.
//                                                                                             <-------mLowSpeedOn-------->
// queued--><--DNS lookup + connect + send headers-->[<--send body (if any)-->]<--replydelay--><--receive headers + body--><--done
//                                                                                                                         ^
//                                                                                                                         |
void HTTPTimeout::done(AICurlEasyRequest_wat const& curlEasyRequest_w, CURLcode code)
{
  if (code == CURLE_OPERATION_TIMEDOUT || code == CURLE_COULDNT_RESOLVE_HOST)
  {
	bool dns_problem = false;
	if (code == CURLE_COULDNT_RESOLVE_HOST)
	{
	  // Note that CURLINFO_OS_ERRNO returns 0; we don't know any more than this.
	  llwarns << "Failed to resolve hostname " << curlEasyRequest_w->getLowercaseHostname() << llendl;
	  dns_problem = true;
	}
	else if (mNothingReceivedYet)
	{
	  // Only consider this to possibly be related to a DNS lookup if we didn't
	  // resolved the host yet, which can be detected by asking for
	  // CURLINFO_NAMELOOKUP_TIME which is set when libcurl initiates the
	  // actual connect and thus knows the IP# (possibly from it's DNS cache).
	  double namelookup_time;
	  curlEasyRequest_w->getinfo(CURLINFO_NAMELOOKUP_TIME, &namelookup_time);
	  dns_problem = (namelookup_time == 0);
	}
	if (dns_problem)
	{
	  // Inform policy object that there might be problems with resolving this host.
	  // This will increase the connect timeout the next time we try to connect to this host.
	  AIHTTPTimeoutPolicy::connect_timed_out(curlEasyRequest_w->getLowercaseHostname());
	  // AIFIXME: use return value to change priority
	}
  }
  // Make sure no timeout will happen anymore.
  mLowSpeedOn = false;
  mStalled = (U64)-1;
  DoutCurl("done: mStalled set to -1");
}

bool HTTPTimeout::maybe_upload_finished(void)
{
  if (!mUploadFinished && mLastBytesSent)
  {
	// Assume that 'upload finished' detection failed and the server is slow with a reply.
	// Switch to waiting for a reply.
	upload_finished();
	return true;
  }
  // The upload certainly finished or certainly did not finish.
  return false;
}

// Libcurl uses GetTickCount on windows, with a resolution of 10 to 16 ms.
// As a result, we can not assume that namelookup_time == 0 has a special meaning.
#define LOWRESTIMER LL_WINDOWS

void HTTPTimeout::print_diagnostics(CurlEasyRequest const* curl_easy_request, char const* eff_url)
{
#ifndef HTTPTIMEOUT_TESTSUITE
  llwarns << "Request to \"" << curl_easy_request->getLowercaseServicename() << "\" timed out for " << curl_easy_request->getTimeoutPolicy()->name() << llendl;
  llinfos << "Effective URL: \"" << eff_url << "\"." << llendl;
  double namelookup_time, connect_time, appconnect_time, pretransfer_time, starttransfer_time;
  curl_easy_request->getinfo(CURLINFO_NAMELOOKUP_TIME, &namelookup_time);
  curl_easy_request->getinfo(CURLINFO_CONNECT_TIME, &connect_time);
  curl_easy_request->getinfo(CURLINFO_APPCONNECT_TIME, &appconnect_time);
  curl_easy_request->getinfo(CURLINFO_PRETRANSFER_TIME, &pretransfer_time);
  curl_easy_request->getinfo(CURLINFO_STARTTRANSFER_TIME, &starttransfer_time);
  if (namelookup_time == 0
#if LOWRESTIMER
	  && connect_time == 0
#endif
	  )
  {
#if LOWRESTIMER
	llinfos << "Hostname seems to have been still in the DNS cache." << llendl;
#else
	llwarns << "Curl returned CURLE_OPERATION_TIMEDOUT and DNS lookup did not occur according to timings. Apparently the resolve attempt timed out (bad network?)" << llendl;
	llassert(connect_time == 0);
	llassert(appconnect_time == 0);
	llassert(pretransfer_time == 0);
	llassert(starttransfer_time == 0);
	return;
#endif
  }
  // If namelookup_time is less than 500 microseconds, then it's very likely just a DNS cache lookup.
  else if (namelookup_time < 500e-6)
  {
#if LOWRESTIMER
	llinfos << "Hostname was most likely still in DNS cache (or lookup occured in under ~10ms)." << llendl;
#else
	llinfos << "Hostname was still in DNS cache." << llendl;
#endif
  }
  else
  {
	llinfos << "DNS lookup of " << curl_easy_request->getLowercaseHostname() << " took " << namelookup_time << " seconds." << llendl;
  }
  if (connect_time == 0
#if LOWRESTIMER
	  && namelookup_time > 0		// connect_time, when set, is namelookup_time + something.
#endif
	  )
  {
	llwarns << "Curl returned CURLE_OPERATION_TIMEDOUT and connection did not occur according to timings: apparently the connect attempt timed out (bad network?)" << llendl;
	llassert(appconnect_time == 0);
	llassert(pretransfer_time == 0);
	llassert(starttransfer_time == 0);
	return;
  }
  // If connect_time is almost equal to namelookup_time, then it was just set because it was already connected.
  if (connect_time - namelookup_time <= 1e-5)
  {
#if LOWRESTIMER		// Assuming 10ms resolution.
	llinfos << "The socket was most likely already connected (or you connected to a proxy with a connect time of under ~10 ms)." << llendl;
#else
	llinfos << "The socket was already connected (to remote or proxy)." << llendl;
#endif
	// I'm assuming that the SSL/TLS handshake can be measured with a low res timer.
	if (appconnect_time == 0)
	{
	  llwarns << "The SSL/TLS handshake never occurred according to the timings!" << llendl;
	  return;
	}
	// If appconnect_time is almost equal to connect_time, then it was just set because this is a connection re-use.
	if (appconnect_time - connect_time <= 1e-5)
	{
	  llinfos << "Connection with HTTP server was already established; this was a re-used connection." << llendl;
	}
	else
	{
	  llinfos << "SSL/TLS handshake with HTTP server took " << (appconnect_time - connect_time) << " seconds." << llendl;
	}
  }
  else
  {
	llinfos << "Socket connected to remote host (or proxy) in " << (connect_time - namelookup_time) << " seconds." << llendl;
	if (appconnect_time == 0)
	{
	  llwarns << "The SSL/TLS handshake never occurred according to the timings!" << llendl;
	  return;
	}
	llinfos << "SSL/TLS handshake with HTTP server took " << (appconnect_time - connect_time) << " seconds." << llendl;
  }
  if (pretransfer_time == 0)
  {
	llwarns << "The transfer never happened because there was too much in the pipeline (apparently)." << llendl;
	return;
  }
  else if (pretransfer_time - appconnect_time >= 1e-5)
  {
	llinfos << "Apparently there was a delay, due to waits in line for the pipeline, of " << (pretransfer_time - appconnect_time) << " seconds before the transfer began." << llendl;
  }
  if (starttransfer_time == 0)
  {
	llwarns << "No data was ever received from the server according to the timings." << llendl;
  }
  else
  {
	llinfos << "The time it took to send the request to the server plus the time it took before the server started to reply was " << (starttransfer_time - pretransfer_time) << " seconds." << llendl;
  }
  if (mNothingReceivedYet)
  {
	llinfos << "No data at all was actually received from the server." << llendl;
  }
  if (mUploadFinished)
  {
	llinfos << "The request upload finished successfully." << llendl;
  }
  else if (mLastBytesSent)
  {
	llinfos << "All bytes where sent to libcurl for upload." << llendl;
  }
  if (mLastSecond > 0 && mLowSpeedOn)
  {
	llinfos << "The " << (mNothingReceivedYet ? "upload" : "download") << " did last " << mLastSecond << " second" << ((mLastSecond == 1) ? "" : "s") << ", before it timed out." << llendl;
  }
#endif // HTTPTIMEOUT_TESTSUITE
}

} // namespace curlthread
} // namespace AICurlPrivate

#ifdef HTTPTIMEOUT_TESTSUITE
int main()
{
  using namespace AICurlPrivate::curlthread;
  AIHTTPTimeoutPolicy policy;
  HTTPTimeout test(&policy, NULL);
}
#endif // HTTPTIMEOUT_TESTSUITE

