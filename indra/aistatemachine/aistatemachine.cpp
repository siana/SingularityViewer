/**
 * @file aistatemachine.cpp
 * @brief Implementation of AIStateMachine
 *
 * Copyright (c) 2010 - 2013, Aleric Inglewood.
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
 *   01/03/2010
 *   Initial version, written by Aleric Inglewood @ SL
 *
 *   28/02/2013
 *   Rewritten from scratch to fully support threading.
 */

#include "linden_common.h"
#include "aistatemachine.h"
#include "lltimer.h"

//==================================================================
// Overview

// A AIStateMachine is a base class that allows derived classes to
// go through asynchronous states, while the code still appears to
// be more or less sequential.
//
// These state machine objects can be reused to build more complex
// objects.
//
// It is important to note that each state has a duality: the object
// can have a state that will cause a corresponding function to be
// called; and often that function will end with changing the state
// again, to signal that it was handled. It is easy to confuse the
// function of a state with the state at the end of the function.
// For example, the state "initialize" could cause the member
// function 'init()' to be called, and at the end one would be
// inclined to set the state to "initialized". However, this is the
// wrong approach: the correct use of state names does reflect the
// functions that will be called, never the function that just was
// called.
//
// Each (derived) class goes through a series of states as follows:
//
//   Creation
//       |
//       v
//     (idle) <----.   		Idle until run() is called.
//       |         |
//   Initialize    |		Calls initialize_impl().
//       |         |
//       | (idle)  |		Idle until cont() or advance_state() is called.
//       |  |  ^   |
//       v  v  |   |
//   .-----------. |
//   | Multiplex | |		Call multiplex_impl() until idle(), abort() or finish() is called.
//   '-----------' |
//    |    |       |
//    v    |       |
//  Abort  |       |		Calls abort_impl().
//    |    |       |
//    v    v       |
//    Finish       |		Calls finish_impl(), which may call run().
//    |    |       |
//    |    v       |
//    | Callback   |        which may call kill() and/or run().
//    |  |   |     |
//    |  |   `-----'
//    v  v
//  Killed					Delete the statemachine (all statemachines must be allocated with new).
//
// Each state causes corresponding code to be called.
// Finish cleans up whatever is done by Initialize.
// Abort should clean up additional things done while running.
//
// The running state is entered by calling run().
//
// While the base class is in the bs_multiplex state, it is the derived class
// that goes through different states. The state variable of the derived
// class is only valid while the base class is in the bs_multiplex state.
//
// A derived class can exit the bs_multiplex state by calling one of two methods:
// abort() in case of failure, or finish() in case of success.
// Respectively these set the state to bs_abort and bs_finish.
//
// The methods of the derived class call set_state() to change their
// own state within the bs_multiplex state, or by calling either abort()
// or finish().
//
// Restarting a finished state machine can be done by calling run(),
// which will cause a re-initialize. The default is to destruct the
// state machine once the last LLPointer to it is deleted.
//


//==================================================================
// Declaration

// Every state machine is (indirectly) derived from AIStateMachine.
// For example:

#ifdef EXAMPLE_CODE	// undefined

class HelloWorld : public AIStateMachine {
  protected:
	// The base class of this state machine.
	typedef AIStateMachine direct_base_type;

	// The different states of the state machine.
	enum hello_world_state_type {
	  HelloWorld_start = direct_base_type::max_state,
	  HelloWorld_done,
	};
  public:
	static state_type const max_state = HelloWorld_done + 1;	// One beyond the largest state.

  public:
	// The derived class must have a default constructor.
	HelloWorld();

  protected:
	// The destructor must be protected.
	/*virtual*/ ~HelloWorld();

  protected:
	// The following virtual functions must be implemented:

	// Handle initializing the object.
	/*virtual*/ void initialize_impl(void);

	// Handle mRunState.
	/*virtual*/ void multiplex_impl(state_type run_state);

	// Handle aborting from current bs_multiplex state (the default AIStateMachine::abort_impl() does nothing).
	/*virtual*/ void abort_impl(void);

	// Handle cleaning up from initialization (or post abort) state (the default AIStateMachine::finish_impl() does nothing).
	/*virtual*/ void finish_impl(void);

	// Return human readable string for run_state.
	/*virtual*/ char const* state_str_impl(state_type run_state) const;
};

// In the .cpp file:

char const* HelloWorld::state_str_impl(state_type run_state) const
{
  switch(run_state)
  {
	// A complete listing of hello_world_state_type.
	AI_CASE_RETURN(HelloWorld_start);
	AI_CASE_RETURN(HelloWorld_done);
  }
#if directly_derived_from_AIStateMachine
  llassert(false);
  return "UNKNOWN STATE";
#else
  llassert(run_state < direct_base_type::max_state);
  return direct_base_type::state_str_impl(run_state);
#endif
}

#endif // EXAMPLE_CODE


//==================================================================
// Life cycle: creation, initialization, running and destruction

// Any thread may create a state machine object, initialize it by calling
// it's initializing member function and call one of the 'run' methods,
// which might or might not immediately start to execute the state machine.

#ifdef EXAMPLE_CODE
  HelloWorld* hello_world = new HelloWorld;
  hello_world->init(...);					// A custom initialization function.
  hello_world->run(...);					// One of the run() functions.
  // hello_world might be destructed here.
  // You can avoid possible destruction by using an LLPointer<HelloWorld>
  // instead of HelloWorld*.
#endif // EXAMPLE_CODE

// The call to run() causes a call to initialize_impl(), which MUST call
//   set_state() at least once (only the last call is used).
// Upon return from initialize_impl(), multiplex_impl() will be called
//   with that state.
// multiplex_impl() may never reentrant (cause itself to be called).
// multiplex_impl() should end by callling either one of:
//   idle(), yield*(), finish() [or abort()].
// Leaving multiplex_impl() without calling any of those might result in an
//   immediate reentry, which could lead to 100% CPU usage unless the state
//   is changed with set_state().
// If multiplex_impl() calls finish() then finish_impl() will be called [if it
//   calls abort() then abort_impl() will called, followed by finish_impl()].
// Upon return from multiplex_impl(), and if finish() [or abort()] was called,
//   the call back passed to run() will be called.
// Upon return from the call back, the state machine object might be destructed
//   (see below).
// If idle() was called, and the state was (still) current_state,
//   then multiplex_impl() will not be called again until the state is
//   advanced, or cont() is called.
//
// If the call back function does not call run(), then the state machine is
//   deleted when the last LLPointer<> reference is deleted.
// If kill() is called after run() was called, then the call to run() is ignored.


//==================================================================
// Aborting

// If abort() is called before initialize_impl() is entered, then the state
//   machine is destructed after the last LLPointer<> reference to it is
//   deleted (if any). Note that this is only possible when a child state
//   machine is aborted before the parent even runs.
//
// If abort() is called inside its initialize_impl() that initialize_impl()
//   should return immediately after.
// if idle(), abort() or finish() are called inside its multiplex_impl() then
//   that multiplex_impl() should return immediately after.
//


//==================================================================
// Thread safety

// Only one thread can "run" a state machine at a time; can call 'multiplex_impl'.
//
// Only from inside multiplex_impl (set_state also from initialize_impl), any of the
// following functions can be called:
//
// - set_state(new_state)		--> Force the state to new_state. This voids any previous call to set_state() or idle().
// - idle()						--> If there was no call to advance_state() since the last call to set_state(current_state))
//                                  then go idle (do nothing until cont() or advance_state() is called). If the current
//                                  state is not current_state,	then multiplex_impl shall be reentered immediately upon return.
// - finish()					--> Disables any scheduled runs.
// 								--> finish_impl		--> [optional] kill()
// 								--> call back
// 								--> [optional] delete
// 								--> [optional] reset, upon return from multiplex_impl, call initialize_impl and start again at the top of multiplex.
// - yield([engine])			--> give CPU to other state machines before running again, run next from a state machine engine.
// 									If no engine is passed, the state machine will run in it's default engine (as set during construction).
// - yield_frame()/yield_ms()	--> yield(&gMainThreadEngine)
//
// the following function may be called from multiplex_impl() of any state machine (and thus by any thread):
//
// - abort()					--> abort_impl
// 								--> finish()
//
// while the following functions may be called from anywhere (and any thread):
//
// - cont()						--> schedules a run if there was no call to set_state() or advance_state() since the last call to idle().
// - advance_state(new_state)	--> sets the state to new_state, if the new_state > current_state, and schedules a run (and voids the last call to idle()).
//
// In the above "scheduling a run" means calling multiplex_impl(), but the same holds for any *_impl()
// and the call back: Whenever one of those have to be called, thread_safe_impl() is called to
// determine if the current state machine allows that function to be called by the current thread,
// and if not - by which thread it should be called then (either main thread, or a special state machine
// thread). If thread switching is necessary, the call is literally scheduled in a queue of one
// of those two, otherwise it is run immediately.
//
// However, since only one thread at a time may be calling any *_impl function (except thread_safe_impl())
// or the call back function, it is possible that at the moment scheduling is necessary another thread
// is already running one of those functions. In that case thread_safe_impl() does not consider the
// current thread, but rather the running thread and does not do any scheduling if the running thread
// is ok, rather marks the need to continue running which should be picked up upon return from
// whatever the running thread is calling.

void AIEngine::add(AIStateMachine* state_machine)
{
  Dout(dc::statemachine, "Adding state machine [" << (void*)state_machine << "] to " << mName);
  engine_state_type_wat engine_state_w(mEngineState);
  engine_state_w->list.push_back(QueueElement(state_machine));
  if (engine_state_w->waiting)
  {
	engine_state_w.signal();
  }
}

extern void print_statemachine_diagnostics(U64 total_clocks, U64 max_delta, AIEngine::queued_type::const_reference slowest_state_machine);

// MAIN-THREAD
void AIEngine::mainloop(void)
{
  queued_type::iterator queued_element, end;
  {
	engine_state_type_wat engine_state_w(mEngineState);
	end = engine_state_w->list.end();
	queued_element = engine_state_w->list.begin();
  }
  U64 total_clocks = 0;
#ifndef LL_RELEASE_FOR_DOWNLOAD
  U64 max_delta = 0;
  queued_type::value_type slowest_element(NULL);
#endif
  while (queued_element != end)
  {
	AIStateMachine& state_machine(queued_element->statemachine());
	U64 start = get_clock_count();
	if (!state_machine.sleep(start))
	{
	  state_machine.multiplex(AIStateMachine::normal_run);
	}
	U64 delta = get_clock_count() - start;
	state_machine.add(delta);
	total_clocks += delta;
#ifndef LL_RELEASE_FOR_DOWNLOAD
	if (delta > max_delta)
	{
	  max_delta = delta;
	  slowest_element = *queued_element;
	}
#endif
	bool active = state_machine.active(this);		// This locks mState shortly, so it must be called before locking mEngineState because add() locks mEngineState while holding mState.
	engine_state_type_wat engine_state_w(mEngineState);
	if (!active)
	{
	  Dout(dc::statemachine, "Erasing state machine [" << (void*)&state_machine << "] from " << mName);
	  engine_state_w->list.erase(queued_element++);
	}
	else
	{
	  ++queued_element;
	}
	if (total_clocks >= sMaxCount)
	{
#ifndef LL_RELEASE_FOR_DOWNLOAD
	  print_statemachine_diagnostics(total_clocks, max_delta, slowest_element);
#endif
	  Dout(dc::statemachine, "Sorting " << engine_state_w->list.size() << " state machines.");
	  engine_state_w->list.sort(QueueElementComp());
	  break;
	}
  }
}

void AIEngine::flush(void)
{
  engine_state_type_wat engine_state_w(mEngineState);
  DoutEntering(dc::statemachine, "AIEngine::flush [" << mName << "]: calling force_killed() on " << engine_state_w->list.size() << " state machines.");
  for (queued_type::iterator iter = engine_state_w->list.begin(); iter != engine_state_w->list.end(); ++iter)
  {
	// To avoid an assertion in ~AIStateMachine.
	iter->statemachine().force_killed();
  }
  engine_state_w->list.clear();
}

// static
U64 AIEngine::sMaxCount;

// static
void AIEngine::setMaxCount(F32 StateMachineMaxTime)
{
  llassert(AIThreadID::in_main_thread());
  Dout(dc::statemachine, "(Re)calculating AIStateMachine::sMaxCount");
  sMaxCount = calc_clock_frequency() * StateMachineMaxTime / 1000;
}

#if defined(CWDEBUG) || defined(DEBUG_CURLIO)
char const* AIStateMachine::event_str(event_type event)
{
  switch(event)
  {
	AI_CASE_RETURN(initial_run);
	AI_CASE_RETURN(schedule_run);
	AI_CASE_RETURN(normal_run);
	AI_CASE_RETURN(insert_abort);
  }
  llassert(false);
  return "UNKNOWN EVENT";
}
#endif

void AIStateMachine::multiplex(event_type event)
{
  // If this fails then you are using a pointer to a state machine instead of an LLPointer.
  llassert(event == initial_run || getNumRefs() > 0);

  DoutEntering(dc::statemachine, "AIStateMachine::multiplex(" << event_str(event) << ") [" << (void*)this << "]");

  base_state_type state;
  state_type run_state;

  // Critical area of mState.
  {
	multiplex_state_type_rat state_r(mState);

	// If another thread is already running multiplex() then it will pick up
	// our need to run (by us having set need_run), so there is no need to run
	// ourselves.
	llassert(!mMultiplexMutex.isSelfLocked());		// We may never enter recursively!
	if (!mMultiplexMutex.tryLock())
	{
	  Dout(dc::statemachine, "Leaving because it is already being run [" << (void*)this << "]");
	  return;
	}

	//===========================================
	// Start of critical area of mMultiplexMutex.

	// If another thread already called begin_loop() since we needed a run,
	// then we must not schedule a run because that could lead to running
	// the same state twice. Note that if need_run was reset in the mean
	// time and then set again, then it can't hurt to schedule a run since
	// we should indeed run, again.
	if (event == schedule_run && !sub_state_type_rat(mSubState)->need_run)
	{
	  Dout(dc::statemachine, "Leaving because it was already being run [" << (void*)this << "]");
	  return;
	}

	// We're at the beginning of multiplex, about to actually run it.
	// Make a copy of the states.
	run_state = begin_loop((state = state_r->base_state));
  }
  // End of critical area of mState.

  bool keep_looping;
  bool destruct = false;
  do
  {

	if (event == normal_run)
	{
#ifdef CWDEBUG
	  if (state == bs_multiplex)
		Dout(dc::statemachine, "Running state bs_multiplex / " << state_str_impl(run_state) << " [" << (void*)this << "]");
	  else
		Dout(dc::statemachine, "Running state " << state_str(state) << " [" << (void*)this << "]");
#endif

#ifdef SHOW_ASSERT
	  // This debug code checks that each state machine steps precisely through each of it's states correctly.
	  if (state != bs_reset)
	  {
		switch(mDebugLastState)
		{
		  case bs_reset:
			llassert(state == bs_initialize || state == bs_killed);
			break;
		  case bs_initialize:
			llassert(state == bs_multiplex || state == bs_abort);
			break;
		  case bs_multiplex:
			llassert(state == bs_multiplex || state == bs_finish || state == bs_abort);
			break;
		  case bs_abort:
			llassert(state == bs_finish);
			break;
		  case bs_finish:
			llassert(state == bs_callback);
			break;
		  case bs_callback:
			llassert(state == bs_killed || state == bs_reset);
			break;
		  case bs_killed:
			llassert(state == bs_killed);
			break;
		}
	  }
	  // More sanity checks.
	  if (state == bs_multiplex)
	  {
		// set_state is only called from multiplex_impl and therefore synced with mMultiplexMutex.
		mDebugShouldRun |= mDebugSetStatePending;
		// Should we run at all?
		llassert(mDebugShouldRun);
	  }
	  // Any previous reason to run is voided by actually running.
	  mDebugShouldRun = false;
#endif

	  mRunMutex.lock();
	  // Now we are actually running a single state.
	  // If abort() was called at any moment before, we execute that state instead.
	  bool const late_abort = (state == bs_multiplex || state == bs_initialize) && sub_state_type_rat(mSubState)->aborted;
	  if (LL_UNLIKELY(late_abort))
	  {
		// abort() was called from a child state machine, from another thread, while we were already scheduled to run normally from an engine.
		// What we want to do here is pretend we detected the abort at the end of the *previous* run.
		// If the state is bs_multiplex then the previous state was either bs_initialize or bs_multiplex,
		// both of which would have switched to bs_abort: we set the state to bs_abort instead and just
		// continue this run.
		// However, if the state is bs_initialize we can't switch to bs_killed because that state isn't
		// handled in the switch below; it's only handled when exiting multiplex() directly after it is set.
		// Therefore, in that case we have to set the state BACK to bs_reset and run it again. This duplicated
		// run of bs_reset is not a problem because it happens to be a NoOp.
		state = (state == bs_initialize) ? bs_reset : bs_abort;
#ifdef CWDEBUG
		Dout(dc::statemachine, "Late abort detected! Running state " << state_str(state) << " instead [" << (void*)this << "]");
#endif
	  }
#ifdef SHOW_ASSERT
	  mDebugLastState = state;
	  // Make sure we only call ref() once and in balance with unref().
	  if (state == bs_initialize)
	  {
		// This -- and call to ref() (and the test when we're about to call unref()) -- is all done in the critical area of mMultiplexMutex.
		llassert(!mDebugRefCalled);
		mDebugRefCalled = true;
	  }
#endif
	  switch(state)
	  {
		case bs_reset:
		  // We're just being kick started to get into the right thread
		  // (possibly for the second time when a late abort was detected, but that's ok: we do nothing here).
		  break;
		case bs_initialize:
		  ref();
		  initialize_impl();
		  break;
		case bs_multiplex:
		  llassert(!mDebugAborted);
		  multiplex_impl(run_state);
		  break;
		case bs_abort:
		  abort_impl();
		  break;
		case bs_finish:
		  sub_state_type_wat(mSubState)->reset = false;		// By default, halt state machines when finished.
		  finish_impl();									// Call run() from finish_impl() or the call back to restart from the beginning.
		  break;
		case bs_callback:
		  callback();
		  break;
		case bs_killed:
		  mRunMutex.unlock();
		  // bs_killed is handled when it is set. So, this must be a re-entry.
		  // We can only get here when being called by an engine that we were added to before we were killed.
		  // This should already be have been set to NULL to indicate that we want to be removed from that engine.
		  llassert(!multiplex_state_type_rat(mState)->current_engine);
		  // Do not call unref() twice.
		  return;
	  }
	  mRunMutex.unlock();
	}

	{
	  multiplex_state_type_wat state_w(mState);

	  //=================================
	  // Start of critical area of mState

	  // Unless the state is bs_multiplex or bs_killed, the state machine needs to keep calling multiplex().
	  bool need_new_run = true;
	  if (event == normal_run || event == insert_abort)
	  {
		sub_state_type_rat sub_state_r(mSubState);

		if (event == normal_run)
		{
		  // Switch base state as function of sub state.
		  switch(state)
		  {
			case bs_reset:
			  if (sub_state_r->aborted)
			  {
				// We have been aborted before we could even initialize, no de-initialization is possible.
				state_w->base_state = bs_killed;
				// Stop running.
				need_new_run = false;
			  }
			  else
			  {
				// run() was called: call initialize_impl() next.
				state_w->base_state = bs_initialize;
			  }
			  break;
			case bs_initialize:
			  if (sub_state_r->aborted)
			  {
				// initialize_impl() called abort.
				state_w->base_state = bs_abort;
			  }
			  else
			  {
				// Start actually running.
				state_w->base_state = bs_multiplex;
				// If the state is bs_multiplex we only need to run again when need_run was set again in the meantime or when this state machine isn't idle.
				need_new_run = sub_state_r->need_run || !sub_state_r->idle;
			  }
			  break;
			case bs_multiplex:
			  if (sub_state_r->aborted)
			  {
				// abort() was called.
				state_w->base_state = bs_abort;
			  }
			  else if (sub_state_r->finished)
			  {
				// finish() was called.
				state_w->base_state = bs_finish;
			  }
			  else
			  {
				// Continue in bs_multiplex.
				// If the state is bs_multiplex we only need to run again when need_run was set again in the meantime or when this state machine isn't idle.
				need_new_run = sub_state_r->need_run || !sub_state_r->idle;
				// If this fails then the run state didn't change and neither idle() nor yield() was called.
				llassert_always(!(need_new_run && !sub_state_r->skip_idle && !mYieldEngine && sub_state_r->run_state == run_state));
			  }
			  break;
			case bs_abort:
			  // After calling abort_impl(), call finish_impl().
			  state_w->base_state = bs_finish;
			  break;
			case bs_finish:
			  // After finish_impl(), call the call back function.
			  state_w->base_state = bs_callback;
			  break;
			case bs_callback:
			  if (sub_state_r->reset)
			  {
				// run() was called (not followed by kill()).
				state_w->base_state = bs_reset;
			  }
			  else
			  {
				// After the call back, we're done.
				state_w->base_state = bs_killed;
				// Call unref().
				destruct = true;
				// Stop running.
				need_new_run = false;
			  }
			  break;
			default: // bs_killed
			  // We never get here.
			  break;
		  }
		}
		else // event == insert_abort
		{
		  // We have been aborted, but we're idle. If we'd just schedule a new run below, it would re-run
		  // the last state before the abort is handled. What we really need is to pick up as if the abort
		  // was handled directly after returning from the last run. If we're not running anymore, then
		  // do nothing as the state machine already ran and things should be processed normally
		  // (in that case this is just a normal schedule which can't harm because we're can't accidently
		  // re-run an old run_state).
		  if (state_w->base_state == bs_multiplex)		// Still running?
		  {
			// See the switch above for case bs_multiplex.
			llassert(sub_state_r->aborted);
			// abort() was called.
			state_w->base_state = bs_abort;
		  }
		}

#ifdef CWDEBUG
		if (state != state_w->base_state)
		  Dout(dc::statemachine, "Base state changed from " << state_str(state) << " to " << state_str(state_w->base_state) <<
			  "; need_new_run = " << (need_new_run ? "true" : "false") << " [" << (void*)this << "]");
#endif
	  }

	  // Figure out in which engine we should run.
	  AIEngine* engine = mYieldEngine ? mYieldEngine : (state_w->current_engine ? state_w->current_engine : mDefaultEngine);
	  // And the current engine we're running in.
	  AIEngine* current_engine = (event == normal_run) ? state_w->current_engine : NULL;

	  // Immediately run again if yield() wasn't called and it's OK to run in this thread.
	  // Note that when it's OK to run in any engine (mDefaultEngine is NULL) then the last
	  // compare is also true when current_engine == NULL.
	  keep_looping = need_new_run && !mYieldEngine && engine == current_engine;
	  mYieldEngine = NULL;

	  if (keep_looping)
	  {
		// Start a new loop.
		run_state = begin_loop((state = state_w->base_state));
		event = normal_run;
	  }
	  else
	  {
		if (need_new_run)
		{
		  // Add us to an engine if necessary.
		  if (engine != state_w->current_engine)
		  {
			// engine can't be NULL here: it can only be NULL if mDefaultEngine is NULL.
			engine->add(this);
			// Mark that we're added to this engine, and at the same time, that we're not added to the previous one.
			state_w->current_engine = engine;
		  }
		}
		else
		{
		  // Remove this state machine from any engine.
		  // Cause the engine to remove us.
		  state_w->current_engine = NULL;
		}

#ifdef SHOW_ASSERT
		// Mark that we stop running the loop.
		mThreadId.clear();

		if (destruct)
		{
		  // We're about to call unref(). Make sure we call that in balance with ref()!
		  llassert(mDebugRefCalled);
		  mDebugRefCalled  = false;
		}
#endif

		// End of critical area of mMultiplexMutex.
		//=========================================

		// Release the lock on mMultiplexMutex *first*, before releasing the lock on mState,
		// to avoid to ever call the tryLock() and fail, while this thread isn't still
		// BEFORE the critical area of mState!

		mMultiplexMutex.unlock();
	  }

	  // Now it is safe to leave the critical area of mState as the tryLock won't fail anymore.
	  // (Or, if we didn't release mMultiplexMutex because keep_looping is true, then this
	  // end of the critical area of mState is equivalent to the first critical area in this
	  // function.

	  // End of critical area of mState.
	  //================================
	}

  }
  while (keep_looping);

  if (destruct)
  {
	unref();
  }
}

AIStateMachine::state_type AIStateMachine::begin_loop(base_state_type base_state)
{
  DoutEntering(dc::statemachine, "AIStateMachine::begin_loop(" << state_str(base_state) << ") [" << (void*)this << "]");

  sub_state_type_wat sub_state_w(mSubState);
  // Honor a subsequent call to idle() (only necessary in bs_multiplex, but it doesn't hurt to reset this flag in other states too).
  sub_state_w->skip_idle = false;
  // Mark that we're about to honor all previous run requests.
  sub_state_w->need_run = false;
  // Honor previous calls to advance_state() (once run_state is initialized).
  if (base_state == bs_multiplex && sub_state_w->advance_state > sub_state_w->run_state)
  {
	Dout(dc::statemachine, "Copying advance_state to run_state, because it is larger [" << state_str_impl(sub_state_w->advance_state) << " > " << state_str_impl(sub_state_w->run_state) << "]");
	sub_state_w->run_state = sub_state_w->advance_state;
  }
#ifdef SHOW_ASSERT
  else
  {
	// If advance_state wasn't honored then it isn't a reason to run.
	// We're running anyway, but that should be because set_state() was called.
	mDebugAdvanceStatePending = false;
  }
#endif
  sub_state_w->advance_state = 0;

#ifdef SHOW_ASSERT
  // Mark that we're running the loop.
  mThreadId.reset();
  // This point marks handling cont().
  mDebugShouldRun |= mDebugContPending;
  mDebugContPending = false;
  // This point also marks handling advance_state().
  mDebugShouldRun |= mDebugAdvanceStatePending;
  mDebugAdvanceStatePending = false;
#endif

  // Make a copy of the state that we're about to run.
  return sub_state_w->run_state;
}

void AIStateMachine::run(AIStateMachine* parent, state_type new_parent_state, bool abort_parent, bool on_abort_signal_parent, AIEngine* default_engine)
{
  DoutEntering(dc::statemachine, "AIStateMachine::run(" <<
	  (void*)parent << ", " <<
	  (parent ? parent->state_str_impl(new_parent_state) : "NA") <<
	  ", abort_parent = " << (abort_parent ? "true" : "false") <<
	  ", on_abort_signal_parent = " << (on_abort_signal_parent ? "true" : "false") <<
	  ", default_engine = " << (default_engine ? default_engine->name() : "NULL") << ") [" << (void*)this << "]");

#ifdef SHOW_ASSERT
  {
	multiplex_state_type_rat state_r(mState);
	// Can only be run when in one of these states.
	llassert(state_r->base_state == bs_reset || state_r->base_state == bs_finish || state_r->base_state == bs_callback);
	// Must be the first time we're being run, or we must be called from finish_impl or a callback function.
	llassert(!(state_r->base_state == bs_reset && (mParent || mCallback)));
  }
#endif

  // Store the requested default engine.
  mDefaultEngine = default_engine;

  // Initialize sleep timer.
  mSleep = 0;

  // Allow NULL to be passed as parent to signal that we want to reuse the old one.
  if (parent)
  {
	mParent = parent;
	// In that case remove any old callback!
	if (mCallback)
	{
	  delete mCallback;
	  mCallback = NULL;
	}

	mNewParentState = new_parent_state;
	mAbortParent = abort_parent;
	mOnAbortSignalParent = on_abort_signal_parent;
  }

  // If abort_parent is requested then a parent must be provided.
  llassert(!abort_parent || mParent);
  // If a parent is provided, it must be running.
  llassert(!mParent || mParent->running());

  // Start from the beginning.
  reset();
}

void AIStateMachine::run(callback_type::signal_type::slot_type const& slot, AIEngine* default_engine)
{
  DoutEntering(dc::statemachine, "AIStateMachine::run(<slot>, default_engine = " << default_engine->name() << ") [" << (void*)this << "]");

#ifdef SHOW_ASSERT
  {
	multiplex_state_type_rat state_r(mState);
	// Can only be run when in one of these states.
	llassert(state_r->base_state == bs_reset || state_r->base_state == bs_finish || state_r->base_state == bs_callback);
	// Must be the first time we're being run, or we must be called from finish_impl or a callback function.
	llassert(!(state_r->base_state == bs_reset && (mParent || mCallback)));
  }
#endif

  // Store the requested default engine.
  mDefaultEngine = default_engine;

  // Initialize sleep timer.
  mSleep = 0;

  // Clean up any old callbacks.
  mParent = NULL;
  if (mCallback)
  {
	delete mCallback;
	mCallback = NULL;
  }

  // Create new call back.
  mCallback = new callback_type(slot);

  // Start from the beginning.
  reset();
}

void AIStateMachine::callback(void)
{
  DoutEntering(dc::statemachine, "AIStateMachine::callback() [" << (void*)this << "]");

  bool aborted = sub_state_type_rat(mSubState)->aborted;
  if (mParent)
  {
	// It is possible that the parent is not running when the parent is in fact aborting and called
	// abort on this object from it's abort_impl function. It that case we don't want to recursively
	// call abort again (or change it's state).
	if (mParent->running())
	{
	  if (aborted && mAbortParent)
	  {
		mParent->abort();
		mParent = NULL;
	  }
	  else if (!aborted || mOnAbortSignalParent)
	  {
		mParent->advance_state(mNewParentState);
	  }
	}
  }
  if (mCallback)
  {
	mCallback->callback(!aborted);
	if (multiplex_state_type_rat(mState)->base_state != bs_reset)
	{
	  delete mCallback;
	  mCallback = NULL;
	  mParent = NULL;
	}
  }
  else
  {
	// Not restarted by callback. Allow run() to be called later on.
	mParent = NULL;
  }
}

void AIStateMachine::force_killed(void)
{
  multiplex_state_type_wat state_w(mState);
  state_w->base_state = bs_killed;
}

void AIStateMachine::kill(void)
{
  DoutEntering(dc::statemachine, "AIStateMachine::kill() [" << (void*)this << "]");
#ifdef SHOW_ASSERT
  {
	multiplex_state_type_rat state_r(mState);
	// kill() may only be called from the call back function.
	llassert(state_r->base_state == bs_callback);
	// May only be called by the thread that is holding mMultiplexMutex.
	llassert(mThreadId.equals_current_thread());
  }
#endif
  sub_state_type_wat sub_state_w(mSubState);
  // Void last call to run() (ie from finish_impl()), if any.
  sub_state_w->reset = false;
}

void AIStateMachine::reset()
{
  DoutEntering(dc::statemachine, "AIStateMachine::reset() [" << (void*)this << "]");
#ifdef SHOW_ASSERT
  mDebugAborted = false;
  mDebugContPending = false;
  mDebugSetStatePending = false;
  mDebugRefCalled = false;
#endif
  mRuntime = 0;
  bool inside_multiplex;
  {
	multiplex_state_type_rat state_r(mState);
	// reset() is only called from run(), which may only be called when just created, from finish_impl() or from the call back function.
	llassert(state_r->base_state == bs_reset || state_r->base_state == bs_finish || state_r->base_state == bs_callback);
	inside_multiplex = state_r->base_state != bs_reset;
  }
  {
	sub_state_type_wat sub_state_w(mSubState);
	// Reset.
	sub_state_w->aborted = sub_state_w->finished = false;
	// Signal that we want to start running from the beginning.
	sub_state_w->reset = true;
	// Start running.
	sub_state_w->idle = false;
	// Keep running till we reach at least bs_multiplex.
	sub_state_w->need_run = true;
  }
  if (!inside_multiplex)
  {
	// Kick start the state machine.
	multiplex(initial_run);
  }
}

void AIStateMachine::set_state(state_type new_state)
{
  DoutEntering(dc::statemachine, "AIStateMachine::set_state(" << state_str_impl(new_state) << ") [" << (void*)this << "]");
#ifdef SHOW_ASSERT
  {
	multiplex_state_type_rat state_r(mState);
	// set_state() may only be called from initialize_impl() or multiplex_impl().
	llassert(state_r->base_state == bs_initialize || state_r->base_state == bs_multiplex);
	// May only be called by the thread that is holding mMultiplexMutex. If this fails, you probably called set_state() by accident instead of advance_state().
	llassert(mThreadId.equals_current_thread());
  }
#endif
  sub_state_type_wat sub_state_w(mSubState);
  // Force current state to the requested state.
  sub_state_w->run_state = new_state;
  // Void last call to advance_state.
  sub_state_w->advance_state = 0;
  // Void last call to idle(), if any.
  sub_state_w->idle = false;
  // Honor a subsequent call to idle().
  sub_state_w->skip_idle = false;
#ifdef SHOW_ASSERT
  // We should run. This can only be cancelled by a call to idle().
  mDebugSetStatePending = true;
#endif
}

void AIStateMachine::advance_state(state_type new_state)
{
  DoutEntering(dc::statemachine, "AIStateMachine::advance_state(" << state_str_impl(new_state) << ") [" << (void*)this << "]");
  {
	sub_state_type_wat sub_state_w(mSubState);
	// Ignore call to advance_state when the currently queued state is already greater or equal to the requested state.
	if (sub_state_w->advance_state >= new_state)
	{
	  Dout(dc::statemachine, "Ignored, because " << state_str_impl(sub_state_w->advance_state) << " >= " << state_str_impl(new_state) << ".");
	  return;
	}
	// Ignore call to advance_state when the current state is greater than the requested state: the new state would be
	// ignored in begin_loop(), as is already remarked there: an advanced state that is not honored is not a reason to run.
	// This call might as well not have happened. Not returning here is a bug because that is effectively a cont(), while
	// the state change is and should be being ignored: the statemachine would start running it's current state (again).
	if (sub_state_w->run_state > new_state)
	{
	  Dout(dc::statemachine, "Ignored, because " << state_str_impl(sub_state_w->run_state) << " > " << state_str_impl(new_state) << " (current state).");
	  return;
	}
	// Increment state.
	sub_state_w->advance_state = new_state;
	// Void last call to idle(), if any.
	sub_state_w->idle = false;
	// Ignore a call to idle if it occurs before we leave multiplex_impl().
	sub_state_w->skip_idle = true;
	// Mark that a re-entry of multiplex() is necessary.
	sub_state_w->need_run = true;
#ifdef SHOW_ASSERT
	// From this moment on.
	mDebugAdvanceStatePending = true;
	// If the new state is equal to the current state, then this should be considered to be a cont()
	// because also equal states are ignored in begin_loop(). However, unlike a cont() we ignore a call
	// to idle() when the statemachine is already running in this state (because that is a race condition
	// and ignoring the idle() is the most logical thing to do then). Hence we treated this as a full
	// fletched advance_state but need to tell the debug code that it's really also a cont().
	if (sub_state_w->run_state == new_state)
	{
	  // From this moment.
	  mDebugContPending = true;
	}
#endif
  }
  if (!mMultiplexMutex.isSelfLocked())
  {
	multiplex(schedule_run);
  }
}

void AIStateMachine::idle(void)
{
  DoutEntering(dc::statemachine, "AIStateMachine::idle() [" << (void*)this << "]");
#ifdef SHOW_ASSERT
  {
	multiplex_state_type_rat state_r(mState);
	// idle() may only be called from initialize_impl() or multiplex_impl().
	llassert(state_r->base_state == bs_multiplex || state_r->base_state == bs_initialize);
	// May only be called by the thread that is holding mMultiplexMutex.
	llassert(mThreadId.equals_current_thread());
  }
  // idle() following set_state() cancels the reason to run because of the call to set_state.
  mDebugSetStatePending = false;
#endif
  sub_state_type_wat sub_state_w(mSubState);
  // As idle may only be called from within the state machine, it should never happen that the state machine is already idle.
  llassert(!sub_state_w->idle);
  // Ignore call to idle() when advance_state() was called since last call to set_state().
  if (sub_state_w->skip_idle)
  {
	Dout(dc::statemachine, "Ignored, because skip_idle is true (advance_state() was called last).");
	return;
  }
  // Mark that we are idle.
  sub_state_w->idle = true;
  // Not sleeping (anymore).
  mSleep = 0;
}

void AIStateMachine::cont(void)
{
  DoutEntering(dc::statemachine, "AIStateMachine::cont() [" << (void*)this << "]");
  {
	sub_state_type_wat sub_state_w(mSubState);
	// Void last call to idle(), if any.
	sub_state_w->idle = false;
	// Mark that a re-entry of multiplex() is necessary.
	sub_state_w->need_run = true;
#ifdef SHOW_ASSERT
	// From this moment.
	mDebugContPending = true;
#endif
  }
  if (!mMultiplexMutex.isSelfLocked())
  {
	multiplex(schedule_run);
  }
}

void AIStateMachine::abort(void)
{
  DoutEntering(dc::statemachine, "AIStateMachine::abort() [" << (void*)this << "]");
  bool is_waiting = false;
  {
	multiplex_state_type_rat state_r(mState);
	sub_state_type_wat sub_state_w(mSubState);
	// Mark that we are aborted, iff we didn't already finish.
	sub_state_w->aborted = !sub_state_w->finished;
	// Mark that a re-entry of multiplex() is necessary.
	sub_state_w->need_run = true;
	// Schedule a new run when this state machine is waiting.
	is_waiting = state_r->base_state == bs_multiplex && sub_state_w->idle;
  }
  if (is_waiting && !mMultiplexMutex.isSelfLocked())
  {
	multiplex(insert_abort);
  }
  // Block until the current run finished.
  if (!mRunMutex.tryLock())
  {
	llwarns << "AIStateMachine::abort() blocks because the statemachine is still executing code in another thread." << llendl;
	mRunMutex.lock();
  }
  mRunMutex.unlock();
#ifdef SHOW_ASSERT
  // When abort() returns, it may never run again.
  mDebugAborted = true;
#endif
}

void AIStateMachine::finish(void)
{
  DoutEntering(dc::statemachine, "AIStateMachine::finish() [" << (void*)this << "]");
#ifdef SHOW_ASSERT
  {
	multiplex_state_type_rat state_r(mState);
	// finish() may only be called from multiplex_impl().
	llassert(state_r->base_state == bs_multiplex);
	// May only be called by the thread that is holding mMultiplexMutex.
	llassert(mThreadId.equals_current_thread());
  }
#endif
  sub_state_type_wat sub_state_w(mSubState);
  // finish() should not be called when idle.
  llassert(!sub_state_w->idle);
  // Mark that we are finished.
  sub_state_w->finished = true;
}

void AIStateMachine::yield(void)
{
  DoutEntering(dc::statemachine, "AIStateMachine::yield() [" << (void*)this << "]");
  multiplex_state_type_rat state_r(mState);
  // yield() may only be called from multiplex_impl().
  llassert(state_r->base_state == bs_multiplex);
  // May only be called by the thread that is holding mMultiplexMutex.
  llassert(mThreadId.equals_current_thread());
  // Set mYieldEngine to the best non-NUL value.
  mYieldEngine = state_r->current_engine ? state_r->current_engine : (mDefaultEngine ? mDefaultEngine : &gStateMachineThreadEngine);
}

void AIStateMachine::yield(AIEngine* engine)
{
  llassert(engine);
  DoutEntering(dc::statemachine, "AIStateMachine::yield(" << engine->name() << ") [" << (void*)this << "]");
#ifdef SHOW_ASSERT
  {
	multiplex_state_type_rat state_r(mState);
	// yield() may only be called from multiplex_impl().
	llassert(state_r->base_state == bs_multiplex);
	// May only be called by the thread that is holding mMultiplexMutex.
	llassert(mThreadId.equals_current_thread());
  }
#endif
  mYieldEngine = engine;
}

void AIStateMachine::yield_frame(unsigned int frames)
{
  DoutEntering(dc::statemachine, "AIStateMachine::yield_frame(" << frames << ") [" << (void*)this << "]");
  mSleep = -(S64)frames;
  // Sleeping is always done from the main thread.
  yield(&gMainThreadEngine);
}

void AIStateMachine::yield_ms(unsigned int ms)
{
  DoutEntering(dc::statemachine, "AIStateMachine::yield_ms(" << ms << ") [" << (void*)this << "]");
  mSleep = get_clock_count() + calc_clock_frequency() * ms / 1000;
  // Sleeping is always done from the main thread.
  yield(&gMainThreadEngine);
}

char const* AIStateMachine::state_str(base_state_type state)
{
  switch(state)
  {
	AI_CASE_RETURN(bs_reset);
	AI_CASE_RETURN(bs_initialize);
	AI_CASE_RETURN(bs_multiplex);
	AI_CASE_RETURN(bs_abort);
	AI_CASE_RETURN(bs_finish);
	AI_CASE_RETURN(bs_callback);
	AI_CASE_RETURN(bs_killed);
  }
  llassert(false);
  return "UNKNOWN BASE STATE";
}

AIEngine gMainThreadEngine("gMainThreadEngine");
AIEngine gStateMachineThreadEngine("gStateMachineThreadEngine");

// State Machine Thread main loop.
void AIEngine::threadloop(void)
{
  queued_type::iterator queued_element, end;
  {
	engine_state_type_wat engine_state_w(mEngineState);
	end = engine_state_w->list.end();
	queued_element = engine_state_w->list.begin();
	if (queued_element == end)
	{
	  // Nothing to do. Wait till something is added to the queue again.
	  engine_state_w->waiting = true;
	  engine_state_w.wait();
	  engine_state_w->waiting = false;
	  return;
	}
  }
  do
  {
	AIStateMachine& state_machine(queued_element->statemachine());
	state_machine.multiplex(AIStateMachine::normal_run);
	bool active = state_machine.active(this);		// This locks mState shortly, so it must be called before locking mEngineState because add() locks mEngineState while holding mState.
	engine_state_type_wat engine_state_w(mEngineState);
	if (!active)
	{
	  Dout(dc::statemachine, "Erasing state machine [" << (void*)&state_machine << "] from " << mName);
	  engine_state_w->list.erase(queued_element++);
	}
	else
	{
	  ++queued_element;
	}
  }
  while (queued_element != end);
}

void AIEngine::wake_up(void)
{
  engine_state_type_wat engine_state_w(mEngineState);
  if (engine_state_w->waiting)
  {
	engine_state_w.signal();
  }
}

//-----------------------------------------------------------------------------
// AIEngineThread

class AIEngineThread : public LLThread
{
  public:
	static AIEngineThread* sInstance;
	bool volatile mRunning;

  public:
    // MAIN-THREAD
    AIEngineThread(void);
    virtual ~AIEngineThread();

  protected:
	virtual void run(void);
};

//static
AIEngineThread* AIEngineThread::sInstance;

AIEngineThread::AIEngineThread(void) : LLThread("AIEngineThread"), mRunning(true)
{
}

AIEngineThread::~AIEngineThread(void)
{
}

void AIEngineThread::run(void)
{
  while(mRunning)
  {
	gStateMachineThreadEngine.threadloop();
  }
}

void startEngineThread(void)
{
  AIEngineThread::sInstance = new AIEngineThread;
  AIEngineThread::sInstance->start();
}

void stopEngineThread(void)
{
  AIEngineThread::sInstance->mRunning = false;
  gStateMachineThreadEngine.wake_up();
  int count = 401;
  while(--count && !AIEngineThread::sInstance->isStopped())
  {
	ms_sleep(10);
  }
  llinfos << "State machine thread" << (!AIEngineThread::sInstance->isStopped() ? " not" : "") << " stopped after " << ((400 - count) * 10) << "ms." << llendl;
}

