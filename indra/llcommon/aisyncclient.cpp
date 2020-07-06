/**
 * @file aisyncclient.cpp
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
 *   13/12/2013
 *   - Initial version, written by Aleric Inglewood @ SL
 */

/*
 * AISyncClient : the base class of a client object (LLMotion) that needs to be informed
 * of the state of other such objects and/or to poll a server object about the state of
 * other such objects, in order to be and stay synchronized with those other objects.
 * In the case of an LLMotion (animation), all clients would be started and stopped at
 * the same time, as long as they are part of the same synchronization group (AISyncServer).
 *
 * AISyncKey: object that determines what synchronization group a client belongs to.
 * When a new client is created, a new AISyncKey is created too, using information from
 * the client, the current frame number and the current frame time. If two AISyncKey
 * compare equal, using operator==(AISyncKey const& key1, AISyncKey const& key2),
 * then the clients that created them need to be synchronized.
 *
 * AISyncServer: object that represents a group of clients that need to be synchronized:
 * it's a wrapper around a std::list<AISyncClientData> with pointers to all the clients
 * that need to be synchronized. It also stores the AISyncKey of the first (oldest) client
 * that was added. Clients with keys that compare equal to that will be added.
 * If a client is added with a synckeytype_t that is larger then it always replaces
 * the existing key however.
 *
 * AISyncServerMap: A wrapper around std::list<boost::intrusive_ptr<AISyncServer> >
 * that stores pointers to all currently existing AISyncServer objects. New entries
 * are added at the end, so the oldest key is at the front.
 *
 * AISyncServerMap  list + - - - - - - + - - - - - - + ...		The AISyncServerMap is
 *                       |             |             |			a list with refcounted
 *                       V             V             V			pointers to AISyncServers.
 * AISyncClient	+--> AISyncServer
 *     |        <--- list     key ---> AISyncKey				Each AISyncServer is a
 * DerivedClient      .											list of normal pointers
 *                    .											AISyncClients and one
 *              <---  .											pointer to a AISyncKey.
 *              												Each AISyncClient is the
 *              												base class of a DerivedClient
 *              												and pointers back to the
 *              												server with a refcounted
 *              												pointer.
 *
 * A new client is passed to the AISyncServerMap to be stored in a new or existing AISyncServer
 * object, using the key that the client produces. A boost::intrusive_ptr<AISyncServer> member
 * of the client is set to point to this server.
 *
 * The lifetime of the server objects is determined by the intrusive_ptr objects that
 * point to it: all the clients (which have an externally determined lifetime) and one
 * pointer in the AISyncServerMap. However, regularly a check is done on all servers in
 * the list to find expired servers: objects with keys older than two frames and older
 * than 0.1 seconds; if such a server is found and it has zero or one client, then the
 * client is unregistered and the pointer (and thus the server) removed from the
 * AISyncServerMap. If it has two or more clients then the entry is kept until both
 * clients are removed, which therefore can only be detected in intrusive_ptr_release
 * which only has access to the server object. The server then is removed from the list
 * by searching through it for the pointer to the server.
 */

#include "sys.h"
#include "aisyncclient.h"
#include <cmath>
#include <algorithm>
#include "debug.h"

bool operator==(AISyncKey const& key1, AISyncKey const& key2)
{
  // Test if these keys match based on time.
  if (std::abs((S64)(key1.mStartFrameCount - key2.mStartFrameCount)) > 1 &&
	  std::abs(key1.mFrameTimer.getStartTime() - key2.mFrameTimer.getStartTime()) >= sSyncKeyExpirationTime)
  {
	return false;
  }
  // Time matches, let the derived classes determine if they also match otherwise.
  return key1.equals(key2);
}

#ifdef CWDEBUG
struct SyncEventSet {
  synceventset_t mBits;
  SyncEventSet(synceventset_t bits) : mBits(bits) { }
};

std::ostream& operator<<(std::ostream& os, SyncEventSet const& ses)
{
  for (int b = sizeof(ses.mBits) * 8 - 1; b >= 0; --b)
  {
	int m = 1 << b;
	os << ((ses.mBits & m) ? '1' : '0');
  }
  return os;
}

void print_clients(AISyncServer const* server, AISyncServer::client_list_t const& client_list)
{
  Dout(dc::notice, "Clients of server " << server << ": ");
  for (AISyncServer::client_list_t::const_iterator iter = client_list.begin(); iter != client_list.end(); ++ iter)
  {
	llassert(iter->mClientPtr->mReadyEvents == iter->mReadyEvents);
	Dout(dc::notice, "-> " << iter->mClientPtr << " : " << SyncEventSet(iter->mReadyEvents));
  }
}
#endif

void AISyncServerMap::register_client(AISyncClient* client, AISyncKey* new_key)
{
  // client must always be a new client that has to be stored somewhere.
  llassert(client->server() == NULL);
  // Obviously the client can't be ready for anything when it isn't registered yet.
  llassert(!client->mReadyEvents);

  // Find if a server with this key already exists.
  AISyncServer* server = NULL;
  for (server_list_t::iterator iter = mServers.begin(); iter != mServers.end();)
  {
    boost::intrusive_ptr<AISyncServer>& server_ptr = *iter++;	// Immediately increment iter because the call to unregister_last_client will erase it.
	AISyncKey const& server_key(server_ptr->key());
	if (server_key.is_older_than(*new_key))						// This means that the server key is expired: a new key will never match.
	{
	  if (server_ptr->never_synced())							// This means that it contains one or zero clients and will never contain more.
	  {
		// Get rid of this server.
		server_ptr->unregister_last_client();					// This will cause the server to be deleted, and erased from mServers.
	  }
	  continue;
	}
	if (*new_key == server_key)
	{
	  server = server_ptr.get();
	  // mServers stores new servers in strict order of the creation time of the keys,
	  // so once we find a server with a key that is equal, none of the remaining servers
	  // will have expired if they were never synced and we're done with the loop.
	  // Servers that synced might have been added later, but we don't unregister
	  // clients from those anyway because their sync partner might still show up.
	  break;
	}
  }

  if (server)
  {
	// A server already exists.
	// Keep the oldest key, unless this new key has a synckeytype_t that is larger!
	if (new_key->getkeytype() > server->key().getkeytype())
	{
	  server->swapkey(new_key);
	}
	delete new_key;
  }
  else
  {
	// Create a new server for this client. Transfers the ownership of the key allocation to the server.
	server = new AISyncServer(new_key);
	// Add it to mServers, before the last server that is younger then the new key.
	server_list_t::iterator where = mServers.end();								// Insert the new server before 'where',
	server_list_t::iterator new_where = where;
	while (where != mServers.begin())											// unless there exists a server before that
	{
	  --new_where;
	  if (new_key->getCreationTime() > (*new_where)->key().getCreationTime())	// and the new key is not younger then that,
	  {
		break;
	  }
	  where = new_where;														// then insert it before that element (etc).
	}
	// This method causes a single call to intrusive_ptr_add_ref and none to intrusive_ptr_release.
	server_ptr_t server_ptr = server;
	mServers.insert(where, server_ptr_t())->swap(server_ptr);
  }

  // Add the client to the server.
  server->add(client);
}

#ifdef SYNC_TESTSUITE
void AISyncServer::sanity_check(void) const
{
  synceventset_t ready_events = (synceventset_t)-1;		// All clients are ready.
  client_list_t::const_iterator client_iter = mClients.begin();
  while (client_iter != mClients.end())
  {
	ready_events &= client_iter->mReadyEvents;
	++client_iter;
  }
  synceventset_t pending_events = 0;					// At least one client is ready.
  client_iter = mClients.begin();
  while (client_iter != mClients.end())
  {
	pending_events |= client_iter->mReadyEvents;
	++client_iter;
  }
  llassert(ready_events == mReadyEvents);
  llassert(pending_events == mPendingEvents);
}
#endif

void AISyncServer::add(AISyncClient* client)
{
#ifdef SYNC_TESTSUITE
  sanity_check();
#endif

  // The client can't already be ready when it isn't even added to a server yet.
  llassert(!client->mReadyEvents);
  synceventset_t old_ready_events = mReadyEvents;
  // A new client is not ready for anything.
  mReadyEvents = 0;
  // Set mSynchronized if after adding client we'll have more than 1 client (that prevents the
  // server from being deleted unless all clients are actually destructed or explicitly unregistered).
  if (!mSynchronized && mClients.size() > 0)
  {
	mSynchronized = true;
  }
  // Trigger the existing clients to be not-ready anymore (if we were before).
  trigger(old_ready_events);
  // Only then add the new client (so that it didn't get not-ready trigger).
  mClients.push_back(client);
  client->mServer = this;

#ifdef SYNC_TESTSUITE
  sanity_check();
#endif
}

void AISyncServer::remove(AISyncClient* client)
{
#ifdef SYNC_TESTSUITE
  sanity_check();
#endif

  client_list_t::iterator client_iter = mClients.begin();
  synceventset_t remaining_ready_events = (synceventset_t)-1;		// All clients are ready.
  synceventset_t remaining_pending_events = 0;						// At least one client is ready (waiting for the other clients thus).
  client_list_t::iterator found_client = mClients.end();
  while (client_iter != mClients.end())
  {
	if (client_iter->mClientPtr == client)
	{
	  found_client = client_iter;
	}
	else
	{
	  remaining_ready_events &= client_iter->mReadyEvents;
	  remaining_pending_events |= client_iter->mReadyEvents;
	}
	++client_iter;
  }
  llassert(found_client != mClients.end());
  // This must be the same as client->mReadyEvents.
  llassert(found_client->mReadyEvents == client->mReadyEvents);
  mClients.erase(found_client);
  synceventset_t old_ready_events = mReadyEvents;
  mReadyEvents = remaining_ready_events;
  mPendingEvents = remaining_pending_events;
  trigger(old_ready_events);
  client->mServer.reset();
  client->deregistered();

#ifdef SYNC_TESTSUITE
  sanity_check();
#endif
}

void AISyncServer::unregister_last_client(void)
{
#ifdef SYNC_TESTSUITE
  sanity_check();
#endif

  // This function may only be called for servers with exactly one client that was never (potentially) synchronized.
  llassert(!mSynchronized && mClients.size() == 1);
  AISyncClient* client = mClients.begin()->mClientPtr;
  mClients.clear();
  client->mServer.reset();
  llassert(mReadyEvents == client->mReadyEvents);
  llassert(mPendingEvents == mReadyEvents);
  // No need to update mReadyEvents/mPendingEvents because the server is going to be deleted,
  // but inform the client that is no longer has a server.
  client->deregistered();

#ifdef SYNC_TESTSUITE
  sanity_check();
#endif
}

void AISyncServer::trigger(synceventset_t old_ready_events)
{
  // If event 1 changed, informat all clients about it.
  if (((old_ready_events ^ mReadyEvents) & 1))
  {
	for (client_list_t::iterator client_iter = mClients.begin(); client_iter != mClients.end(); ++client_iter)
	{
	  if ((mReadyEvents & 1))
	  {
		client_iter->mClientPtr->event1_ready();
	  }
	  else
	  {
		client_iter->mClientPtr->event1_not_ready();
	  }
	}
  }
}

void AISyncServer::ready(synceventset_t events, synceventset_t yesno, AISyncClient* client)
{
#ifdef SYNC_TESTSUITE
  sanity_check();
#endif

  synceventset_t added_events = events & yesno;
  synceventset_t removed_events = events & ~yesno;

  // Run over all clients to find the client and calculate the current state.
  synceventset_t remaining_ready_events = (synceventset_t)-1;		// All clients are ready.
  synceventset_t remaining_pending_events = 0;						// At least one client is ready (waiting for the other clients thus).
  client_list_t::iterator found_client = mClients.end();
  for (client_list_t::iterator client_iter = mClients.begin(); client_iter != mClients.end(); ++client_iter)
  {
	if (client_iter->mClientPtr == client)
	{
	  found_client = client_iter;
	}
	else
	{
	  remaining_ready_events &= client_iter->mReadyEvents;
	  remaining_pending_events |= client_iter->mReadyEvents;
	}
  }

  llassert(mReadyEvents == (remaining_ready_events & found_client->mReadyEvents));
  llassert(mPendingEvents == (remaining_pending_events | found_client->mReadyEvents));

  found_client->mReadyEvents &= ~removed_events;
  found_client->mReadyEvents |= added_events;
#ifdef SHOW_ASSERT
  client->mReadyEvents = found_client->mReadyEvents;
#endif

  synceventset_t old_ready_events = mReadyEvents;

  mReadyEvents = remaining_ready_events & found_client->mReadyEvents;
  mPendingEvents = remaining_pending_events | found_client->mReadyEvents;

  trigger(old_ready_events);

#ifdef SYNC_TESTSUITE
  sanity_check();
#endif
}

void intrusive_ptr_add_ref(AISyncServer* server)
{
  server->mRefCount++;
}

void intrusive_ptr_release(AISyncServer* server)
{
  llassert(server->mRefCount > 0);
  server->mRefCount--;
  if (server->mRefCount == 0)
  {
	delete server;
  }
  else if (server->mRefCount == 1)
  {
	// If the the last pointer to this server is in the the AISyncServerMap, then delete that too.
	AISyncServerMap::instance().remove_server(server);
  }
}

void AISyncServerMap::remove_server(AISyncServer* server)
{
  for (server_list_t::iterator iter = mServers.begin(); iter != mServers.end(); ++iter)
  {
	if (server == iter->get())
	{
	  mServers.erase(iter);		// This causes server to be deleted too.
	  return;
	}
  }
  // The server must be found: this function is only called from intrusive_ptr_release for servers
  // with just a single server_ptr_t left, which must be the one in mServers (otherwise it wasn't
  // even registered properly!)
  llassert(false);
}


//=============================================================================
//                             SYNC_TESTSUITE
//=============================================================================

#ifdef SYNC_TESTSUITE
#include <iostream>
#include <iomanip>
#include <cstdlib>
#include <boost/io/ios_state.hpp>

//static
U64 LLFrameTimer::sFrameCount;

double innerloop_count = 0;

double LLFrameTimer::getCurrentTime()
{
  return innerloop_count * 0.001;
}

template<synckeytype_t synckeytype>
class TestsuiteKey : public AISyncKey
{
  private:
	int mIndex;

  public:
	TestsuiteKey(int index) : mIndex(index) { }

	int getIndex(void) const { return mIndex; }

  // Virtual functions of AISyncKey.
  public:
	/*virtual*/ synckeytype_t getkeytype(void) const
	{
	  // Return a unique identifier for this class, where the low 8 bits represent the syncgroup.
	  return synckeytype;
	}

	/*virtual*/ bool equals(AISyncKey const& key) const
	{
	  synckeytype_t const theotherkey = (synckeytype_t)(synckeytype ^ 0x100);
	  switch (key.getkeytype())
	  {
		case synckeytype:
		{
		  // The other key is of the same type.
		  TestsuiteKey<synckeytype> const& test_key = static_cast<TestsuiteKey<synckeytype> const&>(key);
		  return (mIndex & 1) == (test_key.mIndex & 1);
		}
		case theotherkey:
		{
		  TestsuiteKey<theotherkey> const& test_key = static_cast<TestsuiteKey<theotherkey> const&>(key);
		  return (mIndex & 2) == (test_key.getIndex() & 2);
		}
		default:
		  // The keys must be in the same syncgroup.
		  break;
	  }
	  return false;
	}
};

template<synckeytype_t synckeytype>
class TestsuiteClient : public AISyncClient
{
  // AISyncClient events.
  protected:
	/*virtual*/ AISyncKey* createSyncKey(void) const
	{
	  return new TestsuiteKey<synckeytype>(mIndex);
	}

  private:
	int mIndex;
	bool mRequestedRegistered;
	synceventset_t mRequestedReady;
	bool mActualReady1;

  public:
	TestsuiteClient() : mIndex(-1), mRequestedRegistered(false), mRequestedReady(0), mActualReady1(false) { }
	~TestsuiteClient() { if (is_registered()) this->ready(mRequestedReady, (synceventset_t)0); }

	void setIndex(int index) { mIndex = index; }

  protected:
	/*virtual*/ void event1_ready(void)
	{
#ifdef DEBUG_SYNCOUTPUT
	  Dout(dc::notice, "Calling TestsuiteClient<" << synckeytype << ">::event1_ready() (mIndex = " << mIndex << ") of client " << this);
#endif
	  llassert(!mActualReady1);
	  mActualReady1 = true;
	}

	/*virtual*/ void event1_not_ready(void)
	{
#ifdef DEBUG_SYNCOUTPUT
	  Dout(dc::notice, "Calling TestsuiteClient<" << synckeytype << ">::event1_not_ready() (mIndex = " << mIndex << ") of client " << this);
#endif
	  llassert(mActualReady1);
	  mActualReady1 = false;
	}

	// This is called when the server expired and we're the only client on it.
	/*virtual*/ void deregistered(void)
	{
#ifdef DEBUG_SYNCOUTPUT
	  DoutEntering(dc::notice, "TestsuiteClient<" << synckeytype << ">::deregistered(), with this = " << this);
#endif
	  mRequestedRegistered = false;
	  mRequestedReady = 0;
	  mActualReady1 = false;
	  this->mReadyEvents = 0;
	}

  private:
	bool is_registered(void) const { return this->server(); }

  public:
	void change_state(unsigned long r);
	bool getRequestedRegistered(void) const { return mRequestedRegistered; }
	synceventset_t getRequestedReady(void) const { return mRequestedReady; }
};

TestsuiteClient<synckeytype_test1a>* client1ap;
TestsuiteClient<synckeytype_test1b>* client1bp;
TestsuiteClient<synckeytype_test2a>* client2ap;
TestsuiteClient<synckeytype_test2b>* client2bp;

int const number_of_clients_per_syncgroup = 8;

template<synckeytype_t synckeytype>
void TestsuiteClient<synckeytype>::change_state(unsigned long r)
{
  bool change_registered = r & 1;
  r >>= 1;
  synceventset_t toggle_events = r & 15;
  r >>= 4;
  if (change_registered)
  {
	if (mRequestedRegistered && !mRequestedReady)
	{
	  mRequestedRegistered = false;
	  this->unregister_client();
	}
  }
  else if (toggle_events)
  {
	mRequestedReady ^= toggle_events;
	mRequestedRegistered = true;
	this->ready(toggle_events, mRequestedReady & toggle_events);
  }
  llassert(mRequestedRegistered == is_registered());
  AISyncServer* server = this->server();
  if (mRequestedRegistered)
  {
	synceventset_t all_ready = synceventset_t(-1);
	synceventset_t any_ready = 0;
	int nr = 0;
	for (int cl = 0; cl < number_of_clients_per_syncgroup; ++cl)
	{
	  switch ((synckeytype & 0xff))
	  {
		case syncgroup_test1:
		{
		  if (client1ap[cl].server() == server)
		  {
			if (client1ap[cl].getRequestedRegistered())
			{
			  ++nr;
			  all_ready &= client1ap[cl].getRequestedReady();
			  any_ready |= client1ap[cl].getRequestedReady();
			}
		  }
		  if (client1bp[cl].server() == server)
		  {
			if (client1bp[cl].getRequestedRegistered())
			{
			  ++nr;
			  all_ready &= client1bp[cl].getRequestedReady();
			  any_ready |= client1bp[cl].getRequestedReady();
			}
		  }
		  break;
		}
		case syncgroup_test2:
		{
		  if (client2ap[cl].server() == server)
		  {
			if (client2ap[cl].getRequestedRegistered())
			{
			  ++nr;
			  all_ready &= client2ap[cl].getRequestedReady();
			  any_ready |= client2ap[cl].getRequestedReady();
			}
		  }
		  if (client2bp[cl].server() == server)
		  {
			if (client2bp[cl].getRequestedRegistered())
			{
			  ++nr;
			  all_ready &= client2bp[cl].getRequestedReady();
			  any_ready |= client2bp[cl].getRequestedReady();
			}
		  }
		  break;
		}
	  }
	}
	llassert(nr == server->getClients().size());
	llassert(!!(all_ready & 1) == mActualReady1);
	llassert(this->server()->events_with_all_clients_ready() == all_ready);
	llassert(this->server()->events_with_at_least_one_client_ready() == any_ready);
	llassert(nr == 0 || (any_ready & all_ready) == all_ready);
  }
  llassert(mRequestedReady == this->mReadyEvents);
}

int main()
{
  Debug(libcw_do.on());
  Debug(dc::notice.on());
  Debug(libcw_do.set_ostream(&std::cout));
  Debug(list_channels_on(libcw_do));

  unsigned short seed16v[3] = { 0x1234, 0xfedc, 0x7091 };

  for (int k = 0;; ++k)
  {
	std::cout << "Loop: " << k << "; SEED: " << std::hex << seed16v[0] << ", " << seed16v[1] << ", " << seed16v[2] << std::dec << std::endl;
	++LLFrameTimer::sFrameCount;

	seed48(seed16v);
	seed16v[0] = lrand48() & 0xffff;
	seed16v[1] = lrand48() & 0xffff;
	seed16v[2] = lrand48() & 0xffff;

	TestsuiteClient<synckeytype_test1a> client1a[number_of_clients_per_syncgroup];
	TestsuiteClient<synckeytype_test1b> client1b[number_of_clients_per_syncgroup];
	TestsuiteClient<synckeytype_test2a> client2a[number_of_clients_per_syncgroup];
	TestsuiteClient<synckeytype_test2b> client2b[number_of_clients_per_syncgroup];
	client1ap = client1a;
	client1bp = client1b;
	client2ap = client2a;
	client2bp = client2b;

	for (int i = 0; i < number_of_clients_per_syncgroup; ++i)
	{
	  client1a[i].setIndex(i);
	  client1b[i].setIndex(i);
	  client2a[i].setIndex(i);
	  client2b[i].setIndex(i);
	}

	for (int j = 0; j < 1000000; ++j)
	{
	  innerloop_count += 1;

#ifdef DEBUG_SYNCOUTPUT
	  Dout(dc::notice, "Innerloop: " << j);
#endif
	  unsigned long r = lrand48();
	  synckeytype_t keytype = (r & 1) ? ((r & 2) ? synckeytype_test1a : synckeytype_test1b) : ((r & 2) ? synckeytype_test2a : synckeytype_test2b);
	  r >>= 2;
	  int cl = (r & 255) % number_of_clients_per_syncgroup;
	  r >>= 8;
	  switch (keytype)
	  {
		case synckeytype_test1a:
		  client1a[cl].change_state(r);
		  break;
		case synckeytype_test1b:
		  client1b[cl].change_state(r);
		  break;
		case synckeytype_test2a:
		  client2a[cl].change_state(r);
		  break;
		case synckeytype_test2b:
		  client2b[cl].change_state(r);
		  break;
	  }
	}
  }
}

#endif
