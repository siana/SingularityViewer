/**
 * @file aicurl.h
 * @brief Thread safe wrapper for libcurl.
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
 *   17/03/2012
 *   Initial version, written by Aleric Inglewood @ SL
 */

#ifndef AICURL_H
#define AICURL_H

#include <string>
#include <vector>
#include <set>
#include <stdexcept>
#include <boost/intrusive_ptr.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/utility.hpp>

#include "llpreprocessor.h"
#include <curl/curl.h>		// Needed for files that include this header (also for aicurlprivate.h).
#ifdef DEBUG_CURLIO
#include "debug_libcurl.h"
#endif

// Make sure we don't use this option: it is not thread-safe.
#undef CURLOPT_DNS_USE_GLOBAL_CACHE
#define CURLOPT_DNS_USE_GLOBAL_CACHE do_not_use_CURLOPT_DNS_USE_GLOBAL_CACHE

#include "stdtypes.h"		// U16, S32, U32, F64
#include "llatomic.h"		// LLAtomicU32
#include "aithreadsafe.h"
#include "aicurlperservice.h"	// AIPerServicePtr

// Debug Settings.
extern bool gNoVerifySSLCert;

class LLSD;
class LLBufferArray;
class LLChannelDescriptors;
class AIHTTPTimeoutPolicy;
class LLControlGroup;

// Some pretty printing for curl easy handle related things:
// Print the lock object related to the current easy handle in every debug output.
#ifdef CWDEBUG
#include <libcwd/buf2str.h>
#include <sstream>
#define DoutCurl(x) do { \
	using namespace libcwd; \
	std::ostringstream marker; \
	marker << (void*)this->get_lockobj(); \
	libcw_do.push_marker(); \
	libcw_do.marker().assign(marker.str().data(), marker.str().size()); \
	libcw_do.inc_indent(2); \
	Dout(dc::curl, x); \
	libcw_do.dec_indent(2); \
	libcw_do.pop_marker(); \
  } while(0)
#define DoutCurlEntering(x) do { \
	using namespace libcwd; \
	std::ostringstream marker; \
	marker << (void*)this->get_lockobj(); \
	libcw_do.push_marker(); \
	libcw_do.marker().assign(marker.str().data(), marker.str().size()); \
	libcw_do.inc_indent(2); \
	DoutEntering(dc::curl, x); \
	libcw_do.dec_indent(2); \
	libcw_do.pop_marker(); \
  } while(0)
#else // !CWDEBUG
#define DoutCurl(x) Dout(dc::curl, x << " [" << (void*)this->get_lockobj() << ']')
#define DoutCurlEntering(x) DoutEntering(dc::curl, x << " [" << (void*)this->get_lockobj() << ']')
#endif // CWDEBUG

//-----------------------------------------------------------------------------
// Exceptions.
//

// A general curl exception.
//
class AICurlError : public std::runtime_error {
  public:
	AICurlError(std::string const& message) : std::runtime_error(message) { }
};

class AICurlNoEasyHandle : public AICurlError {
  public:
	AICurlNoEasyHandle(std::string const& message) : AICurlError(message) { }
};

class AICurlNoMultiHandle : public AICurlError {
  public:
	AICurlNoMultiHandle(std::string const& message) : AICurlError(message) { }
};

class AICurlNoBody : public AICurlError {
  public:
	AICurlNoBody(std::string const& message) : AICurlError(message) { }
};

// End Exceptions.
//-----------------------------------------------------------------------------

// Things defined in this namespace are called from elsewhere in the viewer code.
namespace AICurlInterface {

struct Stats {
  static LLAtomicU32 easy_calls;
  static LLAtomicU32 easy_errors;
  static LLAtomicU32 easy_init_calls;
  static LLAtomicU32 easy_init_errors;
  static LLAtomicU32 easy_cleanup_calls;
  static LLAtomicU32 multi_calls;
  static LLAtomicU32 multi_errors;
  static LLAtomicU32 running_handles;
  static LLAtomicU32 AICurlEasyRequest_count;
  static LLAtomicU32 AICurlEasyRequestStateMachine_count;
  static LLAtomicU32 BufferedCurlEasyRequest_count;
  static LLAtomicU32 ResponderBase_count;
  static LLAtomicU32 ThreadSafeBufferedCurlEasyRequest_count;
  static LLAtomicU32 status_count[100];
  static LLAtomicU32 llsd_body_count;
  static LLAtomicU32 llsd_body_parse_error;
  static LLAtomicU32 raw_body_count;

 static void print(void);
 static U32 status2index(U32 status);
 static U32 index2status(U32 index);
};

//-----------------------------------------------------------------------------
// Global functions.

// Called to handle changes in Debug Settings.
bool handleCurlMaxTotalConcurrentConnections(LLSD const& newvalue);
bool handleCurlConcurrentConnectionsPerService(LLSD const& newvalue);
bool handleNoVerifySSLCert(LLSD const& newvalue);

// Called once at start of application (from newview/llappviewer.cpp by main thread (before threads are created)),
// with main purpose to initialize curl.
void initCurl(void);

// Called once at start of application (from LLAppViewer::initThreads), starts AICurlThread.
void startCurlThread(LLControlGroup* control_group);

// Called once at the end of application before terminating other threads (most notably the texture thread workers)
// with the purpose to stop the curl thread from doing any call backs to running responders: the responders sometimes
// access objects that will be shot down when bringing down other threads.
void shutdownCurl(void);

// Called once at end of application (from newview/llappviewer.cpp by main thread) after all other threads have been terminated
// with the purpose to stop the curl thread, free curl resources and deinitialize curl.
void cleanupCurl(void);

// Called from indra/newview/llfloaterabout.cpp for the About floater, and
// from newview/llappviewer.cpp in behalf of debug output.
// Just returns curl_version().
std::string getVersionString(void);

// Called from newview/llappviewer.cpp (and llcrashlogger/llcrashlogger.cpp) to set
// the Certificate Authority file used to verify HTTPS certs.
void setCAFile(std::string const& file);

// Not called from anywhere.
// Can be used to set the path to the Certificate Authority file.
void setCAPath(std::string const& file);

// Returns number of queued 'add' commands minus the number of queued 'remove' commands.
U32 getNumHTTPCommands(void);

// Returns the number of queued requests.
U32 getNumHTTPQueued(void);

// Returns the number of curl requests currently added to the multi handle.
U32 getNumHTTPAdded(void);

// Return the maximum number of total allowed added curl requests.
U32 getMaxHTTPAdded(void);

// This used to be LLAppViewer::getTextureFetch()->getNumHTTPRequests().
// Returns the number of active curl easy handles (that are actually attempting to download something).
U32 getNumHTTPRunning(void);

// Cache for gSavedSettings so we have access from llmessage.
extern LLControlGroup* sConfigGroup;

} // namespace AICurlInterface

// Forward declaration (see aicurlprivate.h).
namespace AICurlPrivate {
  class BufferedCurlEasyRequest;
} // namespace AICurlPrivate

// Define access types (_crat = Const Read Access Type, _rat = Read Access Type, _wat = Write Access Type).
// Typical usage is:
// AICurlEasyRequest h1;				// Create easy handle.
// AICurlEasyRequest h2(h1);			// Make lightweight copies.
// AICurlEasyRequest_wat h2_w(*h2);	// Lock and obtain write access to the easy handle.
// Use *h2_w, which is a reference to the locked BufferedCurlEasyRequest instance.
// Note: As it is not allowed to use curl easy handles in any way concurrently,
// read access would at most give access to a CURL const*, which will turn out
// to be completely useless; therefore it is sufficient and efficient to use
// an AIThreadSafeSimple and it's unlikely that AICurlEasyRequest_rat will be used.
typedef AIAccessConst<AICurlPrivate::BufferedCurlEasyRequest> AICurlEasyRequest_rat;
typedef AIAccess<AICurlPrivate::BufferedCurlEasyRequest> AICurlEasyRequest_wat;

// Events generated by AICurlPrivate::CurlEasyHandle.
struct AICurlEasyHandleEvents {
	// Events.
	virtual void added_to_multi_handle(AICurlEasyRequest_wat& curl_easy_request_w) = 0;
	virtual void finished(AICurlEasyRequest_wat& curl_easy_request_w) = 0;
	virtual void removed_from_multi_handle(AICurlEasyRequest_wat& curl_easy_request_w) = 0;
	virtual void bad_file_descriptor(AICurlEasyRequest_wat& curl_easy_request_w) = 0;
#ifdef SHOW_ASSERT
	virtual void queued_for_removal(AICurlEasyRequest_wat& curl_easy_request_w) = 0;
#endif
	// Avoid compiler warning.
	virtual ~AICurlEasyHandleEvents() { }
};

// Pointer to data we're going to POST.
class AIPostField : public LLThreadSafeRefCount {
  protected:
	char const* mData;

  public:
	AIPostField(char const* data) : mData(data) { }
	char const* data(void) const { return mData; }
};

// The pointer to the data that we have to POST is passed around as AIPostFieldPtr,
// which causes it to automatically clean up when there are no pointers left
// pointing to it.
typedef LLPointer<AIPostField> AIPostFieldPtr;

#include "aicurlprivate.h"

// AICurlPrivate::BufferedCurlEasyRequestPtr, a boost::intrusive_ptr, is no more threadsafe than a
// builtin type, but wrapping it in AIThreadSafe is obviously not going to help here.
// Therefore we use the following trick: we wrap BufferedCurlEasyRequestPtr too, and only allow
// read accesses on it.

// AICurlEasyRequest: a thread safe, reference counting, buffered, auto-cleaning curl easy handle.
class AICurlEasyRequest {
  private:
	// Use AICurlEasyRequestStateMachine, not AICurlEasyRequest.
	friend class AICurlEasyRequestStateMachine;

	// Initial construction is allowed (thread-safe).
	// Note: If ThreadSafeBufferedCurlEasyRequest() throws then the memory allocated is still freed.
	// 'new' never returned however and neither the constructor nor destructor of mBufferedCurlEasyRequest is called in this case.
	// This might throw AICurlNoEasyHandle.
	AICurlEasyRequest(void) :
	    mBufferedCurlEasyRequest(new AICurlPrivate::ThreadSafeBufferedCurlEasyRequest) { AICurlInterface::Stats::AICurlEasyRequest_count++; }

  public:
	// Update stats.
	~AICurlEasyRequest() { --AICurlInterface::Stats::AICurlEasyRequest_count; }

	// Used for storing this object in a standard container (see MultiHandle::add_easy_request).
	AICurlEasyRequest(AICurlEasyRequest const& orig) : mBufferedCurlEasyRequest(orig.mBufferedCurlEasyRequest) { AICurlInterface::Stats::AICurlEasyRequest_count++; }

	// For the rest, only allow read operations.
	AIThreadSafeSimple<AICurlPrivate::BufferedCurlEasyRequest>& operator*(void) const { llassert(mBufferedCurlEasyRequest.get()); return *mBufferedCurlEasyRequest; }
	AIThreadSafeSimple<AICurlPrivate::BufferedCurlEasyRequest>* operator->(void) const { llassert(mBufferedCurlEasyRequest.get()); return mBufferedCurlEasyRequest.get(); }
	AIThreadSafeSimple<AICurlPrivate::BufferedCurlEasyRequest>* get(void) const { return mBufferedCurlEasyRequest.get(); }

	// Returns true if this object points to the same BufferedCurlEasyRequest object.
	bool operator==(AICurlEasyRequest const& cer) const { return mBufferedCurlEasyRequest == cer.mBufferedCurlEasyRequest; }
	
	// Returns true if this object points to a different BufferedCurlEasyRequest object.
	bool operator!=(AICurlEasyRequest const& cer) const { return mBufferedCurlEasyRequest != cer.mBufferedCurlEasyRequest; }
	
	// Queue this request for insertion in the multi session.
	void addRequest(void);

	// Queue a command to remove this request from the multi session (or cancel a queued command to add it).
	void removeRequest(void);

#ifdef DEBUG_CURLIO
	// Turn on/off debug output.
	void debug(bool debug) { AICurlEasyRequest_wat(*mBufferedCurlEasyRequest)->debug(debug); }
#endif

  private:
	// The actual pointer to the ThreadSafeBufferedCurlEasyRequest instance.
	AICurlPrivate::BufferedCurlEasyRequestPtr mBufferedCurlEasyRequest;

  private:
	// Assignment would not be thread-safe; we may create this object and read from it.
	// Note: Destruction is implicitly assumed thread-safe, as it would be a logic error to
	// destruct it while another thread still needs it, concurrent or not.
	AICurlEasyRequest& operator=(AICurlEasyRequest const&) { return *this; }

  public:
	// Instead of assignment, it might be helpful to use swap.
	void swap(AICurlEasyRequest& cer) { mBufferedCurlEasyRequest.swap(cer.mBufferedCurlEasyRequest); }

  public:
	// The more exotic member functions of this class, to deal with passing this class
	// as CURLOPT_PRIVATE pointer to a curl handle and afterwards restore it.
	// For "internal use" only; don't use things from AICurlPrivate yourself.

	// It's thread-safe to give read access the underlaying boost::intrusive_ptr.
	// It's not OK to then call get() on that and store the AICurlPrivate::ThreadSafeBufferedCurlEasyRequest* separately.
	AICurlPrivate::BufferedCurlEasyRequestPtr const& get_ptr(void) const { return mBufferedCurlEasyRequest; }

	// If we have a correct (with regard to reference counting) AICurlPrivate::BufferedCurlEasyRequestPtr,
	// then it's OK to construct a AICurlEasyRequest from it.
	// Note that the external AICurlPrivate::BufferedCurlEasyRequestPtr needs its own locking, because
	// it's not thread-safe in itself.
	AICurlEasyRequest(AICurlPrivate::BufferedCurlEasyRequestPtr const& ptr) : mBufferedCurlEasyRequest(ptr) { AICurlInterface::Stats::AICurlEasyRequest_count++; }

	// This one is obviously dangerous. It's for use only in MultiHandle::check_msg_queue.
	// See also the long comment in BufferedCurlEasyRequest::finalizeRequest with regard to CURLOPT_PRIVATE.
	explicit AICurlEasyRequest(AICurlPrivate::ThreadSafeBufferedCurlEasyRequest* ptr) : mBufferedCurlEasyRequest(ptr) { AICurlInterface::Stats::AICurlEasyRequest_count++; }
};

#define AICurlPrivate DONTUSE_AICurlPrivate

#endif
