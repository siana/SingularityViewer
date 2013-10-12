/**
 * @file aistatemachine.h
 * @brief State machine base class
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

#ifndef AISTATEMACHINE_H
#define AISTATEMACHINE_H

#include "aithreadsafe.h"
#include <llpointer.h>
#include <list>
#include <boost/signals2.hpp>

class AIStateMachine;

class AIEngine
{
  private:
	struct QueueElementComp;
	class QueueElement {
	  private:
		LLPointer<AIStateMachine> mStateMachine;

	  public:
		QueueElement(AIStateMachine* statemachine) : mStateMachine(statemachine) { }
		friend bool operator==(QueueElement const& e1, QueueElement const& e2) { return e1.mStateMachine == e2.mStateMachine; }
		friend bool operator!=(QueueElement const& e1, QueueElement const& e2) { return e1.mStateMachine != e2.mStateMachine; }
		friend struct QueueElementComp;

		AIStateMachine const& statemachine(void) const { return *mStateMachine; }
		AIStateMachine& statemachine(void) { return *mStateMachine; }
	};
	struct QueueElementComp {
	  inline bool operator()(QueueElement const& e1, QueueElement const& e2) const;
	};

  public:
	typedef std::list<QueueElement> queued_type;
	struct engine_state_type {
	  queued_type list;
	  bool waiting;
	  engine_state_type(void) : waiting(false) { }
	};

  private:
	AIThreadSafeSimpleDC<engine_state_type, LLCondition>	mEngineState;
	typedef AIAccessConst<engine_state_type, LLCondition>	engine_state_type_crat;
	typedef AIAccess<engine_state_type, LLCondition>		engine_state_type_rat;
	typedef AIAccess<engine_state_type, LLCondition>		engine_state_type_wat;
	char const* mName;

	static U64 sMaxCount;

  public:
	AIEngine(char const* name) : mName(name) { }

	void add(AIStateMachine* state_machine);

	void mainloop(void);
	void threadloop(void);
	void wake_up(void);
	void flush(void);

	char const* name(void) const { return mName; }

	static void setMaxCount(F32 StateMachineMaxTime);
};

extern AIEngine gMainThreadEngine;
extern AIEngine gStateMachineThreadEngine;

class AIStateMachine : public LLThreadSafeRefCount
{
  public:
	typedef U32 state_type;		//!< The type of run_state

  protected:
	// The type of event that causes multiplex() to be called.
	enum event_type {
	  initial_run,
	  schedule_run,
	  normal_run,
	  insert_abort
	};
	// The type of mState
	enum base_state_type {
	  bs_reset,				// Idle state before run() is called. Reference count is zero (except for a possible external LLPointer).
	  bs_initialize,		// State after run() and before/during initialize_impl().
	  bs_multiplex,			// State after initialize_impl() before finish() or abort().
	  bs_abort,
	  bs_finish,
	  bs_callback,
	  bs_killed
	};
  public:
	static state_type const max_state = bs_killed + 1;

  protected:
	struct multiplex_state_type {
	  base_state_type	base_state;
	  AIEngine*			current_engine;			// Current engine.
	  multiplex_state_type(void) : base_state(bs_reset), current_engine(NULL) { }
	};
	struct sub_state_type {
	  state_type		run_state;
	  state_type		advance_state;
	  bool				reset;
	  bool				need_run;
	  bool				idle;
	  bool				skip_idle;
	  bool				aborted;
	  bool				finished;
	};

  private:
	// Base state.
	AIThreadSafeSimpleDC<multiplex_state_type>	mState;
	typedef AIAccessConst<multiplex_state_type>	multiplex_state_type_crat;
	typedef AIAccess<multiplex_state_type>		multiplex_state_type_rat;
	typedef AIAccess<multiplex_state_type>		multiplex_state_type_wat;

  protected:
	// Sub state.
	AIThreadSafeSimpleDC<sub_state_type>	mSubState;
	typedef AIAccessConst<sub_state_type>	sub_state_type_crat;
	typedef AIAccess<sub_state_type>		sub_state_type_rat;
	typedef AIAccess<sub_state_type>		sub_state_type_wat;

  private:
	// Mutex protecting everything below and making sure only one thread runs the state machine at a time.
	LLMutex mMultiplexMutex;
	// Mutex that is locked while calling *_impl() functions and the call back.
	LLMutex mRunMutex;

	S64 mSleep;                                 //!< Non-zero while the state machine is sleeping.

	// Callback facilities.
	// From within an other state machine:
	LLPointer<AIStateMachine> mParent;			// The parent object that started this state machine, or NULL if there isn't any.
	state_type mNewParentState;					// The state at which the parent should continue upon a successful finish.
	bool mAbortParent;							// If true, abort parent on abort(). Otherwise continue as normal.
	bool mOnAbortSignalParent;					// If true and mAbortParent is false, change state of parent even on abort.
	// From outside a state machine:
	struct callback_type {
		typedef boost::signals2::signal<void (bool)> signal_type;
		callback_type(signal_type::slot_type const& slot) { connection = signal.connect(slot); }
		~callback_type() { connection.disconnect(); }
		void callback(bool success) const { signal(success); }
	  private:
		boost::signals2::connection connection;
		signal_type signal;
	};
	callback_type* mCallback;					// Pointer to signal/connection, or NULL when not connected.

	// Engine stuff.
	AIEngine* mDefaultEngine;					// Default engine.
	AIEngine* mYieldEngine;						// Requested engine.

#ifdef SHOW_ASSERT
	// Debug stuff.
	AIThreadID mThreadId;						// The thread currently running multiplex().
	base_state_type mDebugLastState;			// The previous state that multiplex() had a normal run with.
	bool mDebugShouldRun;						// Set if we found evidence that we should indeed call multiplex_impl().
	bool mDebugAborted;							// True when abort() was called.
	bool mDebugContPending;						// True while cont() was called by not handled yet.
	bool mDebugSetStatePending;					// True while set_state() was called by not handled yet.
	bool mDebugAdvanceStatePending;				// True while advance_state() was called by not handled yet.
	bool mDebugRefCalled;						// True when ref() is called (or will be called within the critial area of mMultiplexMutex).
#endif
	U64 mRuntime;								// Total time spent running in the main thread (in clocks).

  public:
	AIStateMachine(void) : mCallback(NULL), mDefaultEngine(NULL), mYieldEngine(NULL),
#ifdef SHOW_ASSERT
		mThreadId(AIThreadID::none), mDebugLastState(bs_killed), mDebugShouldRun(false), mDebugAborted(false), mDebugContPending(false),
		mDebugSetStatePending(false), mDebugAdvanceStatePending(false), mDebugRefCalled(false),
#endif
		mRuntime(0)
	{ }

  protected:
	// The user should call finish() (or abort(), or kill() from the call back when finish_impl() calls run()), not delete a class derived from AIStateMachine directly.
	virtual ~AIStateMachine() { llassert(multiplex_state_type_rat(mState)->base_state == bs_killed); }
 
  public:
	// These functions may be called directly after creation, or from within finish_impl(), or from the call back function.
	void run(AIStateMachine* parent, state_type new_parent_state, bool abort_parent = true, bool on_abort_signal_parent = true, AIEngine* default_engine = &gMainThreadEngine);
	void run(callback_type::signal_type::slot_type const& slot, AIEngine* default_engine = &gMainThreadEngine);
	void run(void) { run(NULL, 0, false, true, mDefaultEngine); }

	// This function may only be called from the call back function (and cancels a call to run() from finish_impl()).
	void kill(void);

  protected:
	// This function can be called from initialize_impl() and multiplex_impl() (both called from within multiplex()).
	void set_state(state_type new_state);									// Run this state the NEXT loop.
	// These functions can only be called from within multiplex_impl().
	void idle(void);														// Go idle unless cont() or advance_state() were called since the start of the current loop, or until they are called.
	void finish(void);														// Mark that the state machine finished and schedule the call back.
	void yield(void);														// Yield to give CPU to other state machines, but do not go idle.
	void yield(AIEngine* engine);											// Yield to give CPU to other state machines, but do not go idle. Continue running from engine 'engine'.
	void yield_frame(unsigned int frames);									// Run from the main-thread engine after at least 'frames' frames have passed.
	void yield_ms(unsigned int ms);											// Run from the main-thread engine after roughly 'ms' miliseconds have passed.

  public:
	// This function can be called from multiplex_imp(), but also by a child state machine and
	// therefore by any thread. The child state machine should use an LLPointer<AIStateMachine>
	// to access this state machine.
	void abort(void);														// Abort the state machine (unsuccessful finish).

	// These are the only two functions that can be called by any thread at any moment.
	// Those threads should use an LLPointer<AIStateMachine> to access this state machine.
	void cont(void);														// Guarantee at least one full run of multiplex() after this function is called. Cancels the last call to idle().
	void advance_state(state_type new_state);								// Guarantee at least one full run of multiplex() after this function is called
																			// iff new_state is larger than the last state that was processed.

  public:
	// Accessors.

	// Return true if the derived class is running (also when we are idle).
	bool running(void) const { return multiplex_state_type_crat(mState)->base_state == bs_multiplex; }
	// Return true if the derived class is running and idle.
	bool waiting(void) const
	{
	  multiplex_state_type_crat state_r(mState);
	  return state_r->base_state == bs_multiplex && sub_state_type_crat(mSubState)->idle;
	}
	// Return true if the derived class is running and idle or already being aborted.
	bool waiting_or_aborting(void) const
	{
	  multiplex_state_type_crat state_r(mState);
	  return state_r->base_state == bs_abort || ( state_r->base_state == bs_multiplex && sub_state_type_crat(mSubState)->idle);
	}
	// Return true if are added to the engine.
	bool active(AIEngine const* engine) const { return multiplex_state_type_crat(mState)->current_engine == engine; }
	bool aborted(void) const { return sub_state_type_crat(mSubState)->aborted; }

	// Use some safebool idiom (http://www.artima.com/cppsource/safebool.html) rather than operator bool.
	typedef state_type AIStateMachine::* const bool_type;
	// Return true if state machine successfully finished.
	operator bool_type() const
	{
	  sub_state_type_crat sub_state_r(mSubState);
	  return (sub_state_r->finished && !sub_state_r->aborted) ? &AIStateMachine::mNewParentState : 0;
	}

	// Return stringified state, for debugging purposes.
	char const* state_str(base_state_type state);
#if defined(CWDEBUG) || defined(DEBUG_CURLIO)
	char const* event_str(event_type event);
#endif

	void add(U64 count) { mRuntime += count; }
	U64 getRuntime(void) const { return mRuntime; }

  protected:
	virtual void initialize_impl(void) = 0;
	virtual void multiplex_impl(state_type run_state) = 0;
	virtual void abort_impl(void) { }
	virtual void finish_impl(void) { }
	virtual char const* state_str_impl(state_type run_state) const = 0;

  private:
	void reset(void);														// Called from run() to (re)initialize a (re)start.
	void multiplex(event_type event);										// Called from AIEngine to step through the states (and from reset() to kick start the state machine).
	state_type begin_loop(base_state_type base_state);						// Called from multiplex() at the start of a loop.
	void callback(void);													// Called when the state machine finished.
	bool sleep(U64 current_time)											// Count frames if necessary and return true when the state machine is still sleeping.
	{
	  if (mSleep == 0)
		return false;
	  else if (mSleep < 0)
		++mSleep;
	  else if ((U64)mSleep <= current_time)
		mSleep = 0;
	  return mSleep != 0;
	}
	void force_killed(void);												// Called from AIEngine::flush().

	friend class AIEngine;						// Calls multiplex() and force_killed().
};

bool AIEngine::QueueElementComp::operator()(QueueElement const& e1, QueueElement const& e2) const
{
  return e1.mStateMachine->getRuntime() < e2.mStateMachine->getRuntime();
}

// This can be used in state_str_impl.
#define AI_CASE_RETURN(x) do { case x: return #x; } while(0)

#endif
