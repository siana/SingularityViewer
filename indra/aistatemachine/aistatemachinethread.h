/**
 * @file aistatemachinethread.h
 * @brief Run code in a thread.
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
 *   23/01/2013
 *   Initial version, written by Aleric Inglewood @ SL
 */

#ifndef AISTATEMACHINETHREAD_H
#define AISTATEMACHINETHREAD_H

#include "aistatemachine.h"
#include "llthread.h"
#include "aithreadsafe.h"
#include <boost/format.hpp>

#ifdef EXAMPLE_CODE	// undefined

class HelloWorldThread : public AIThreadImpl {
  private:
	bool mStdErr;		// input
	bool mSuccess;		// output

  public:
	// Constructor.
	HelloWorldThread(void) : AIThreadImpl("HelloWorldThread"),						// MAIN THREAD
		mStdErr(false), mSuccess(false) { }

	// Some initialization function (if needed).
	void init(bool err)	{ mStdErr = err; }											// MAIN THREAD
	// Read back output.
	bool successful(void) const { return mSuccess; }

	// Mandatory signature.
	/*virtual*/ bool run(void)														// NEW THREAD
	{
	  if (mStdErr)
		std::cerr << "Hello world" << std::endl;
	  else
		std::cout << "Hello world" << std::endl;
	  mSuccess = true;
	  return true;			// true = finish, false = abort.
	}
};

// The states of this state machine.
enum hello_world_state_type {
  HelloWorld_start = AIStateMachine::max_state,
  HelloWorld_done
};

// The statemachine class (this is almost a template).
class HelloWorld : public AIStateMachine {
  private:
	LLPointer<AIStateMachineThread<HelloWorldThread> > mHelloWorld;
	bool mErr;

  public:
	HelloWorld() : mHelloWorld(new AIStateMachineThread<HelloWorldThread>), mErr(false) { }

	// Print to stderr or stdout?
	void init(bool err) { mErr = err; }

  protected:
	// Call finish() (or abort()), not delete.
	/*virtual*/ ~HelloWorld() { }

	// Handle initializing the object.
	/*virtual*/ void initialize_impl(void);

	// Handle run_state.
	/*virtual*/ void multiplex_impl(state_type run_state);

	// Implemenation of state_str for run states.
	/*virtual*/ char const* state_str_impl(state_type run_state) const
	{
	  switch(run_state)
	  {
		AI_CASE_RETURN(HelloWorld_start);
		AI_CASE_RETURN(HelloWorld_done);
	  }
	  return "UNKNOWN STATE";
	}
};

// The actual implementation of this statemachine starts here!

void HelloWorld::initialize_impl(void)
{
  mHelloWorld->thread_impl().init(mErr);			// Initialize the thread object.
  set_state(HelloWorld_start);
}

void HelloWorld::multiplex_impl(state_type run_state)
{
  switch (run_state)
  {
	case HelloWorld_start:
	{
	  mHelloWorld->run(this, HelloWorld_done);		// Run HelloWorldThread and set the state of 'this' to HelloWorld_done when finished.
	  idle(HelloWorld_start);						// Always go idle after starting a thread!
	  break;
	}
	case HelloWorld_done:
	{
	  // We're done. Lets also abort when the thread reported no success.
	  if (mHelloWorld->thread_impl().successful())	// Read output/result of thread object.
		finish();
	  else
		abort();
	  break;
	}
  }
}

#endif	// EXAMPLE CODE

class AIStateMachineThreadBase;

// Derive from this to implement the code that must run in another thread.
class AIThreadImpl {
  private:
    template<typename THREAD_IMPL> friend class AIStateMachineThread;
	typedef AIAccess<AIStateMachineThreadBase*> StateMachineThread_wat;
	AIThreadSafeSimpleDC<AIStateMachineThreadBase*> mStateMachineThread;

  public:
	virtual bool run(void) = 0;
	bool thread_done(bool result);
	bool state_machine_done(LLThread* threadp);

#ifdef LL_DEBUG
  private:
	char const* mName;
  protected:
	AIThreadImpl(char const* name = "AIStateMachineThreadBase::Thread") : mName(name) { }
  public:
	char const* getName(void) const { return mName; }
#endif

  protected:
	virtual ~AIThreadImpl() { }
};

// The base class for statemachine threads.
class AIStateMachineThreadBase : public AIStateMachine {
  private:
	// The actual thread (derived from LLThread).
	class Thread;

  protected:
	typedef AIStateMachine direct_base_type;

	// The states of this state machine.
	enum thread_state_type {
	  start_thread = direct_base_type::max_state,	// Start the thread (if necessary create it first).
	  wait_stopped									// Wait till the thread is stopped.
	};
  public:
	static state_type const max_state = wait_stopped + 1;

  protected:
	AIStateMachineThreadBase(CWD_ONLY(bool debug))
#ifdef CWDEBUG
	  : AIStateMachine(debug)
#endif
	{ }

  private:
	// Handle initializing the object.
	/*virtual*/ void initialize_impl(void);

	// Handle mRunState.
	/*virtual*/ void multiplex_impl(state_type run_state);

	// Handle aborting from current bs_run state.
	/*virtual*/ void abort_impl(void);

	// Implemenation of state_str for run states.
	/*virtual*/ char const* state_str_impl(state_type run_state) const;

	// Returns a reference to the implementation code that needs to be run in the thread.
	virtual AIThreadImpl& impl(void) = 0;

  private:
	Thread* mThread;		// The thread that the code is run in.
	bool mAbort;			// (Inverse of) return value of AIThreadImpl::run(). Only valid in state wait_stopped.

  public:
	void schedule_abort(bool do_abort) { mAbort = do_abort; }

};

// The state machine that runs T::run() in a thread.
// THREAD_IMPL Must be derived from AIThreadImpl.
template<typename THREAD_IMPL>
class AIStateMachineThread : public AIStateMachineThreadBase {
  private:
	THREAD_IMPL mThreadImpl;

  public:
	// Constructor.
	AIStateMachineThread(CWD_ONLY(bool debug))
#ifdef CWDEBUG
		: AIStateMachineThreadBase(debug)
#endif
	{
	  *AIThreadImpl::StateMachineThread_wat(mThreadImpl.mStateMachineThread) = this;
	}

	// Accessor.
	THREAD_IMPL& thread_impl(void) { return mThreadImpl; }

	/*virtual*/ const char* getName() const
	{
#define STRIZE(arg) #arg
		return (boost::format("%1%%2%%3%") % "AIStateMachineThread<" % STRIZE(THREAD_IMPL) % ">").str().c_str();
#undef STRIZE
	}

  protected:
	/*virtual*/ AIThreadImpl& impl(void) { return mThreadImpl; }
};

#endif

