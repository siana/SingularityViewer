/**
 * @file aisyncclient.h
 * @brief Declaration of AISyncClient.
 *
 * Copyright (c) 2013, Aleric Inglewood.
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
 *   12/12/2013
 *   Initial version, written by Aleric Inglewood @ SL
 */

#ifndef AI_SYNC_CLIENT_H
#define AI_SYNC_CLIENT_H

#ifdef SYNC_TESTSUITE
/*
 * To compile the testsuite, run:
 *
 * cd indra/llcommon
 * g++ -O3 -DCWDEBUG -DSYNC_TESTSUITE -I. -I../cwdebug aisyncclient.cpp -lcwd
 */

#include <stdint.h>
#include <cassert>
typedef uint32_t U32;
typedef int32_t S32;
typedef uint64_t U64;
typedef float F32;
typedef double F64;
#define LL_COMMON_API
#define SHOW_ASSERT
#define ASSERT_ONLY_COMMA(...) , __VA_ARGS__
#define llassert assert

struct LLFrameTimer
{
  double mStartTime;
  double mExpiry;
  static double getCurrentTime(void);
  static U64 sFrameCount;
  static U64 getFrameCount() { return sFrameCount; }
  F64 getStartTime() const { return mStartTime; }
  void reset(double expiration) { mStartTime = getCurrentTime(); mExpiry = mStartTime + expiration; }
  bool hasExpired(void) const { return getCurrentTime() > mExpiry; }
};

template<typename T>
struct LLSingleton
{
  static T sInstance;
  static T& instance(void) { return sInstance; }
};

template<typename T>
T LLSingleton<T>::sInstance;

#else // !SYNC_TESTSUITE
#include "llsingleton.h"
#include "llframetimer.h"
#endif
#include <list>
#include <boost/intrusive_ptr.hpp>

//---------------------------------------------------------------------------------------------------------------------
// Keys with a different syncgroup are never equal (so they are never synchronized).
enum syncgroups
{
#ifdef SYNC_TESTSUITE
  syncgroup_test1,
  syncgroup_test2,
#else
  syncgroup_motions,			// Syncgroup used for animations.
#endif
  syncgroup_size
};

// Each key type must return a unique identifier that exists of its syncgroup (the least significant 8 bit) plus a few bit to make it unique (bit 9 and higher).
enum synckeytype_t
{
#ifdef SYNC_TESTSUITE
  synckeytype_test1a = 0x000 + syncgroup_test1,
  synckeytype_test1b = 0x100 + syncgroup_test1,
  synckeytype_test2a = 0x000 + syncgroup_test2,
  synckeytype_test2b = 0x100 + syncgroup_test2,
#else
  synckeytype_motion = syncgroup_motions	// There is currently only one key type in the syncgroup_motions group: AISyncKeyMotion.
#endif
};

typedef U32 synceventset_t;					// A mask where each bit represents a ready state.

static F32 const sSyncKeyExpirationTime = 0.25;  // In seconds.

class LL_COMMON_API AISyncKey
{
  private:
	LLFrameTimer mFrameTimer;				// This timer is started at the moment the sync key is created.
	U64 mStartFrameCount;					// The frame count at which the timer was started.

  public:
	// Constructor.
	AISyncKey(AISyncKey const* from_key) : mStartFrameCount(from_key ? from_key->mStartFrameCount : LLFrameTimer::getFrameCount())
	{
	  if (from_key)
	  {
		mFrameTimer.copy(from_key->mFrameTimer);
	  }
	  else
	  {
		mFrameTimer.reset(sSyncKeyExpirationTime);
	  }
	}

	// Destructor.
	virtual ~AISyncKey() { }

	// Return true if this key expired.
	bool expired(void) const
	{
	  // The key has expired when sSyncKeyExpirationTime seconds have elapsed AND at least two frames have passed.
	  return mFrameTimer.getFrameCount() > mStartFrameCount + 1 && mFrameTimer.hasExpired();
	}

	// Returns true if this object and key would not compare equal based on time because this object is too old.
	bool is_older_than(AISyncKey const& key) const
	{
	  return key.mStartFrameCount > mStartFrameCount + 1 && key.mFrameTimer.getStartTime() > mFrameTimer.getStartTime() + sSyncKeyExpirationTime;
	}

	// Return the creation time of this key (in number of seconds since application start).
	F64 getCreationTime(void) const { return mFrameTimer.getStartTime(); }

	// Returns true if the two keys match, meaning that they should be synchronized.
	friend bool operator==(AISyncKey const& key1, AISyncKey const& key2);

	// Returns an ID that uniquely identifies the derived type.
	// Currently the only derived type is AISyncKeyMotion with ID synckeytype_motion.
	virtual synckeytype_t getkeytype(void) const = 0;

	// Returns true if the data in the derived objects match, meaning that they should be synchronized.
    virtual bool equals(AISyncKey const& key) const = 0;
};

// Forward declaration.
class AISyncClient;
class AISyncServer;
LL_COMMON_API extern void intrusive_ptr_add_ref(AISyncServer* server);
LL_COMMON_API extern void intrusive_ptr_release(AISyncServer* server);

struct LL_COMMON_API AISyncClientData
{
  AISyncClient* mClientPtr;
  synceventset_t mReadyEvents;

  AISyncClientData(AISyncClient* client) : mClientPtr(client), mReadyEvents(0) { }
};

class LL_COMMON_API AISyncServer
{
  public:
	typedef std::list<AISyncClientData> client_list_t;

  private:
	int mRefCount;					// Number of boost::intrusive_ptr objects pointing to this object.
	AISyncKey* mKey;				// The key of the first client that was added.
	client_list_t mClients;			// A list with pointers to all registered clients.
	bool mSynchronized;				// Set when a server gets more than one client, and not reset anymore after that.
	synceventset_t mReadyEvents;	// 0xFFFFFFFF bitwise-AND-ed with all clients.
	synceventset_t mPendingEvents;	// The bitwise-OR of all clients.

  public:
	AISyncServer(AISyncKey* key) : mRefCount(0), mKey(key), mSynchronized(false), mReadyEvents((synceventset_t)-1), mPendingEvents(0) { }
	~AISyncServer() { delete mKey; }

	// Add a new client to this server.
	void add(AISyncClient* client);

	// Remove a client from this server.
	void remove(AISyncClient* client);

	// Return the key associated to this server (which is the key produced by the first client of the largest synckeytype_t that was added).
	AISyncKey const& key(void) const { return *mKey; }
	// Replace they key with another key (of larger synckeytype_t).
	void swapkey(AISyncKey*& key_ptr) { AISyncKey* tmp = key_ptr; key_ptr = mKey; mKey = tmp; }

	// Returns true if this server never had more than one client.
	bool never_synced(void) const { return !mSynchronized; }

	// Set readiness of all events at once.
	void ready(synceventset_t events, synceventset_t yesno, AISyncClient* client);

	// Unregister the (only) client because it's own its own and will never need synchronization.
	void unregister_last_client(void);

	// Return the events that all clients for.
	synceventset_t events_with_all_clients_ready(void) const { return mReadyEvents; }

	// Return events that at least one client is ready for.
	synceventset_t events_with_at_least_one_client_ready(void) const { return mPendingEvents; }

	// Return a list of all registered clients.
	client_list_t const& getClients(void) const { return mClients; }

  private:
	// Call event1_ready() or event1_not_ready() on all clients if the least significant bit of mReadyEvents changed.
	void trigger(synceventset_t old_ready_events);

#ifdef SYNC_TESTSUITE
	void sanity_check(void) const;
#endif

  private:
    friend LL_COMMON_API void intrusive_ptr_add_ref(AISyncServer* server);
    friend LL_COMMON_API void intrusive_ptr_release(AISyncServer* server);
};

class LL_COMMON_API AISyncServerMap : public LLSingleton<AISyncServerMap>
{
  public:
	typedef boost::intrusive_ptr<AISyncServer> server_ptr_t;	// The type of a (stored) pointer to the server objects.
	typedef std::list<server_ptr_t> server_list_t;				// The type of the list with pointers to the server objects.

  private:
	server_list_t mServers;									// A list with pointers to all server objects.

  public:
	// Find or create a server object that the client belongs to and store the client in it.
	// If a new server is created, it is stored in mServers.
	void register_client(AISyncClient* client, AISyncKey* new_key);

  private:
    friend LL_COMMON_API void intrusive_ptr_release(AISyncServer* server);
	// Remove a server from the map, only called by intrusive_ptr_release when there is one pointer left;
	// therefore, the server should not have any clients.
	void remove_server(AISyncServer* server);
};

class LL_COMMON_API AISyncClient
{
  private:
	friend class AISyncServer;
	boost::intrusive_ptr<AISyncServer> mServer;		// The server this client was registered with, or NULL when unregistered.

  public:
#ifdef SHOW_ASSERT
	synceventset_t mReadyEvents;
	AISyncClient(void) : mReadyEvents(0) { }
#endif
	virtual ~AISyncClient() { llassert(!mServer); /* If this fails then you need to add unregister_client() to the top of the destructor of the derived class that implements deregistered(). */ }
	virtual AISyncKey* createSyncKey(AISyncKey const* from_key = NULL) const = 0;

	virtual void event1_ready(void) = 0;
	virtual void event1_not_ready(void) = 0;

	// Only client. Client was forcefully deregistered from expired server because it was the only client.
	virtual void deregistered(void)
	{
#ifdef SHOW_ASSERT
      mReadyEvents = 0;
#endif
	}

	AISyncServer* server(void) const { return mServer.get(); }

	// Add this client to a server with matching sync key. Optionally the server is first created.
	void register_client(void) { AISyncServerMap::instance().register_client(this, createSyncKey()); }

	// Remove this client from its server, if any.
	void unregister_client(void) { if (mServer) mServer->remove(this); }

	// Call 'ready' when you are ready (or not) to get a call to start().
	// Returns true if that call was made (immediately), otherwise it may happen later.

	// Set readiness of all events at once.
	void ready(synceventset_t events, synceventset_t yesno)
    {
      if (!mServer)
      {
        register_client();
      }
      mServer->ready(events, yesno, this);
    }
};

#endif
