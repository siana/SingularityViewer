/** 
 * @file llapp.cpp
 * @brief Implementation of the LLApp class.
 *
 * $LicenseInfo:firstyear=2003&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
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

#include "linden_common.h"

#include "llapp.h"

#ifdef LL_DARWIN
#include <sys/types.h>
#include <unistd.h>
#include <sys/sysctl.h>
#endif

#include "llcommon.h"
#include "llapr.h"
#include "llerrorcontrol.h"
#include "llerrorthread.h"
#include "llframetimer.h"
#include "lllivefile.h"
#include "llmemory.h"
#include "llstl.h" // for DeletePointer()
#include "llstring.h"
#include "lleventtimer.h"

//
// Signal handling
//
// Windows uses structured exceptions, so it's handled a bit differently.
//
#if LL_WINDOWS
#include <windows.h>

LONG WINAPI default_windows_exception_handler(struct _EXCEPTION_POINTERS *exception_infop);
BOOL ConsoleCtrlHandler(DWORD fdwCtrlType);
#else
# include <signal.h>
# include <unistd.h> // for fork()
# include "apr_signal.h" // for apr_signal_description_get(int signum)
void setup_signals();
void default_unix_signal_handler(int signum, siginfo_t *info, void *);

# if LL_DARWIN
/* OSX doesn't support SIGRT* */
S32 LL_SMACKDOWN_SIGNAL = SIGUSR1;
S32 LL_HEARTBEAT_SIGNAL = SIGUSR2;
# else // linux or (assumed) other similar unixoid
/* We want reliable delivery of our signals - SIGRT* is it. */
/* Old LinuxThreads versions eat SIGRTMIN+0 to SIGRTMIN+2, avoid those. */
/* Note that SIGRTMIN/SIGRTMAX may expand to a glibc function call with a
   nonconstant result so these are not consts and cannot be used in constant-
   expressions.  SIGRTMAX may return -1 on rare broken setups. */
S32 LL_SMACKDOWN_SIGNAL = (SIGRTMAX >= 0) ? (SIGRTMAX-1) : SIGUSR1;
S32 LL_HEARTBEAT_SIGNAL = (SIGRTMAX >= 0) ? (SIGRTMAX-0) : SIGUSR2;
# endif // LL_DARWIN
#endif // LL_WINDOWS

// the static application instance
LLApp* LLApp::sApplication = nullptr;

// Allows the generation of core files for post mortem under gdb
// and disables crashlogger
BOOL LLApp::sDisableCrashlogger = FALSE; 

// Local flag for whether or not to do logging in signal handlers.
//static
BOOL LLApp::sLogInSignal = FALSE;

// static
LLApp::EAppStatus LLApp::sStatus = LLApp::APP_STATUS_STOPPED; // Keeps track of application status
LLAppErrorHandler LLApp::sErrorHandler = nullptr;
BOOL LLApp::sErrorThreadRunning = FALSE;
#if !LL_WINDOWS
LLApp::child_map LLApp::sChildMap;
LLAtomicU32* LLApp::sSigChildCount = NULL;
LLAppChildCallback LLApp::sDefaultChildCallback = NULL;
#endif


LLApp::LLApp()
	: mThreadErrorp(nullptr)
{
	commonCtor();
}

void LLApp::commonCtor()
{
	// Set our status to running
	setStatus(APP_STATUS_RUNNING);

	LLCommon::initClass();

	// initialize the options structure. We need to make this an array
	// because the structured data will not auto-allocate if we
	// reference an invalid location with the [] operator.
	mOptions = LLSD::emptyArray();
	LLSD sd;
	for(int i = 0; i < PRIORITY_COUNT; ++i)
	{
		mOptions.append(sd);
	}

	// Make sure we clean up APR when we exit
	// Don't need to do this if we're cleaning up APR in the destructor
	//atexit(ll_cleanup_apr);

	// Set the application to this instance.
	sApplication = this;

	// initialize the buffer to write the minidump filename to
	// (this is used to avoid allocating memory in the crash handler)
	mCrashReportPipeStr = L"\\\\.\\pipe\\LLCrashReporterPipe";
}

LLApp::LLApp(LLErrorThread *error_thread) :
	mThreadErrorp(error_thread)
{
	commonCtor();
}


LLApp::~LLApp()
{

	// reclaim live file memory
	std::for_each(mLiveFiles.begin(), mLiveFiles.end(), DeletePointer());
	mLiveFiles.clear();

	setStopped();
	// HACK: wait for the error thread to clean itself
	ms_sleep(20);
	if (mThreadErrorp)
	{
		delete mThreadErrorp;
		mThreadErrorp = nullptr;
	}
	
	LLCommon::cleanupClass();
}

// static
LLApp* LLApp::instance()
{
	return sApplication;
}


LLSD LLApp::getOption(const std::string& name) const
{
	LLSD rv;
    auto iter = mOptions.beginArray();
    auto end = mOptions.endArray();
	for(; iter != end; ++iter)
	{
		rv = (*iter)[name];
		if(rv.isDefined()) break;
	}
	return rv;
}

bool LLApp::parseCommandOptions(int argc, char** argv)
{
	LLSD commands;
	std::string name;
	std::string value;
	for(int ii = 1; ii < argc; ++ii)
	{
		if(argv[ii][0] != '-')
		{
			LL_INFOS() << "Did not find option identifier while parsing token: "
				<< argv[ii] << LL_ENDL;
			return false;
		}
		int offset = 1;
		if(argv[ii][1] == '-') ++offset;
		name.assign(&argv[ii][offset]);
		if(((ii+1) >= argc) || (argv[ii+1][0] == '-'))
		{
			// we found another option after this one or we have
			// reached the end. simply record that this option was
			// found and continue.
			int flag = name.compare("logfile");
			if (0 == flag)
			{
				commands[name] = "log";
			}
			else
			{
				commands[name] = true;
			}
			
			continue;
		}
		++ii;
		value.assign(argv[ii]);

#if LL_WINDOWS
		//Windows changed command line parsing.  Deal with it.
		size_t slen = value.length() - 1;
		size_t start = 0;
		size_t end = slen;
		if (argv[ii][start]=='"')start++;
		if (argv[ii][end]=='"')end--;
		if (start!=0 || end!=slen) 
		{
			value = value.substr (start,end);
		}
#endif

		commands[name] = value;
	}
	setOptionData(PRIORITY_COMMAND_LINE, commands);
	return true;
}


void LLApp::manageLiveFile(LLLiveFile* livefile)
{
	if(!livefile) return;
	livefile->checkAndReload();
	livefile->addToEventTimer();
	mLiveFiles.push_back(livefile);
}

bool LLApp::setOptionData(OptionPriority level, LLSD data)
{
	if((level < 0)
	   || (level >= PRIORITY_COUNT)
	   || (data.type() != LLSD::TypeMap))
	{
		return false;
	}
	mOptions[level] = data;
	return true;
}

LLSD LLApp::getOptionData(OptionPriority level)
{
	if((level < 0) || (level >= PRIORITY_COUNT))
	{
		return LLSD();
	}
	return mOptions[level];
}

#if LL_WINDOWS
//The following code is needed for 32-bit apps on 64-bit windows to keep it from eating
//crashes.   It is a lovely undocumented 'feature' in SP1 of Windows 7. An excellent
//in-depth article on the issue may be found here:  http://randomascii.wordpress.com/2012/07/05/when-even-crashing-doesn-work/
void EnableCrashingOnCrashes()
{
	typedef BOOL (WINAPI *tGetPolicy)(LPDWORD lpFlags);
	typedef BOOL (WINAPI *tSetPolicy)(DWORD dwFlags);
	const DWORD EXCEPTION_SWALLOWING = 0x1;

	HMODULE kernel32 = LoadLibraryA("kernel32.dll");
	tGetPolicy pGetPolicy = (tGetPolicy)GetProcAddress(kernel32,
		"GetProcessUserModeExceptionPolicy");
	tSetPolicy pSetPolicy = (tSetPolicy)GetProcAddress(kernel32,
		"SetProcessUserModeExceptionPolicy");
	if (pGetPolicy && pSetPolicy)
	{
		DWORD dwFlags;
		if (pGetPolicy(&dwFlags))
		{
			// Turn off the filter
			pSetPolicy(dwFlags & ~EXCEPTION_SWALLOWING);
		}
	}
}
#endif

void LLApp::setupErrorHandling()
{
	// Error handling is done by starting up an error handling thread, which just sleeps and
	// occasionally checks to see if the app is in an error state, and sees if it needs to be run.

#if !LL_WINDOWS
	//
	// Start up signal handling.
	//
	// There are two different classes of signals.  Synchronous signals are delivered to a specific
	// thread, asynchronous signals can be delivered to any thread (in theory)
	//
	setup_signals();
#endif // ! LL_WINDOWS
#if !defined(USE_CRASHPAD)
	startErrorThread();
#endif
}

void LLApp::startErrorThread()
{
	//
	// Start the error handling thread, which is responsible for taking action
	// when the app goes into the APP_STATUS_ERROR state
	//
	if(!mThreadErrorp)
	{
		LL_INFOS() << "Starting error thread" << LL_ENDL;
		mThreadErrorp = new LLErrorThread();
		mThreadErrorp->setUserData((void *) this);
		mThreadErrorp->start();
	}
}

void LLApp::stopErrorThread()
{
	LLApp::setStopped();	// Signal error thread that we stopped.
	int count = 0;
	while (mThreadErrorp && !mThreadErrorp->isStopped() && ++count < 100)
	{
		ms_sleep(10);
	}
	if (mThreadErrorp && !mThreadErrorp->isStopped())
	{
		LL_WARNS() << "Failed to stop Error Thread." << LL_ENDL;
	}
}

void LLApp::setErrorHandler(LLAppErrorHandler handler)
{
	LLApp::sErrorHandler = handler;
}

// static
void LLApp::runErrorHandler()
{
	if (LLApp::sErrorHandler)
	{
		LLApp::sErrorHandler();
	}

	//LL_INFOS() << "App status now STOPPED" << LL_ENDL;
	LLApp::setStopped();
}

// static
void LLApp::setStatus(EAppStatus status)
{
	sStatus = status;
}


// static
void LLApp::setError()
{
	// set app status to ERROR so that the LLErrorThread notices
	setStatus(APP_STATUS_ERROR);
}

void LLApp::setDebugFileNames(const std::string &path)
{
  	mStaticDebugFileName = path + "static_debug_info.log";
  	mDynamicDebugFileName = path + "dynamic_debug_info.log";
}

// static
void LLApp::setQuitting()
{
	if (!isExiting())
	{
		// If we're already exiting, we don't want to reset our state back to quitting.
		LL_INFOS() << "Setting app state to QUITTING" << LL_ENDL;
		setStatus(APP_STATUS_QUITTING);
	}
}


// static
void LLApp::setStopped()
{
	setStatus(APP_STATUS_STOPPED);
}


// static
bool LLApp::isStopped()
{
	return (APP_STATUS_STOPPED == sStatus);
}


// static
bool LLApp::isRunning()
{
	return (APP_STATUS_RUNNING == sStatus);
}


// static
bool LLApp::isError()
{
	return (APP_STATUS_ERROR == sStatus);
}


// static
bool LLApp::isQuitting()
{
	return (APP_STATUS_QUITTING == sStatus);
}

// static
bool LLApp::isExiting()
{
	return isQuitting() || isError() || isStopped();
}

void LLApp::disableCrashlogger()
{
	sDisableCrashlogger = TRUE;
}

// static
bool LLApp::isCrashloggerDisabled()
{
	return (sDisableCrashlogger == TRUE); 
}

#if !LL_WINDOWS
// static
U32 LLApp::getSigChildCount()
{
	if (sSigChildCount)
	{
		return U32(*sSigChildCount);
	}
	return 0;
}

// static
void LLApp::incSigChildCount()
{
	if (sSigChildCount)
	{
		(*sSigChildCount)++;
	}
}

#endif


// static
int LLApp::getPid()
{
#if LL_WINDOWS
	return GetCurrentProcessId();
#else
	return getpid();
#endif
}

#if LL_WINDOWS
LONG WINAPI default_windows_exception_handler(struct _EXCEPTION_POINTERS *exception_infop)
{
	// Translate the signals/exceptions into cross-platform stuff
	// Windows implementation

	// Make sure the user sees something to indicate that the app crashed.
	LONG retval;

	if (LLApp::isError())
	{
		LL_WARNS() << "Got another fatal signal while in the error handler, die now!" << LL_ENDL;
		retval = EXCEPTION_EXECUTE_HANDLER;
		return retval;
	}

	// Flag status to error, so thread_error starts its work
	LLApp::setError();

	// Block in the exception handler until the app has stopped
	// This is pretty sketchy, but appears to work just fine
	while (!LLApp::isStopped())
	{
		ms_sleep(10);
	}

	//
	// At this point, we always want to exit the app.  There's no graceful
	// recovery for an unhandled exception.
	// 
	// Just kill the process.
	retval = EXCEPTION_EXECUTE_HANDLER;	
	return retval;
}

// Win32 doesn't support signals. This is used instead.
BOOL ConsoleCtrlHandler(DWORD fdwCtrlType) 
{ 
	switch (fdwCtrlType) 
	{ 
 		case CTRL_BREAK_EVENT: 
		case CTRL_LOGOFF_EVENT: 
		case CTRL_SHUTDOWN_EVENT: 
		case CTRL_CLOSE_EVENT: // From end task or the window close button.
		case CTRL_C_EVENT:  // from CTRL-C on the keyboard
			// Just set our state to quitting, not error
			if (LLApp::isQuitting() || LLApp::isError())
			{
				// We're already trying to die, just ignore this signal
				if (LLApp::sLogInSignal)
				{
					LL_INFOS() << "Signal handler - Already trying to quit, ignoring signal!" << LL_ENDL;
				}
				return TRUE;
			}
			LLApp::setQuitting();
			return TRUE; 
	
		default: 
			return FALSE; 
	} 
} 

#else //!LL_WINDOWS
void LLApp::setChildCallback(pid_t pid, LLAppChildCallback callback)
{
	LLChildInfo child_info;
	child_info.mCallback = callback;
	LLApp::sChildMap[pid] = child_info;
}

void LLApp::setDefaultChildCallback(LLAppChildCallback callback)
{
	LLApp::sDefaultChildCallback = callback;
}

pid_t LLApp::fork()
{
	fflush(NULL); // flush all buffers before the child inherits them
	pid_t pid = ::fork();
	if( pid < 0 )
	{
		int system_error = errno;
		LL_WARNS() << "Unable to fork! Operating system error code: "
				<< system_error << LL_ENDL;
	}
	else if (pid == 0)
	{
		// Sleep a bit to allow the parent to set up child callbacks.
		ms_sleep(10);

		// We need to disable signal handling, because we don't have a
		// signal handling thread anymore.
		setupErrorHandling();
	}
	else
	{
		LL_INFOS() << "Forked child process " << pid << LL_ENDL;
	}
	return pid;
}

void setup_signals()
{
	//
	// Set up signal handlers that may result in program termination
	//
	struct sigaction act;
	act.sa_sigaction = default_unix_signal_handler;
	sigemptyset( &act.sa_mask );
	act.sa_flags = SA_SIGINFO;

	// Synchronous signals
	sigaction(SIGABRT, &act, NULL);
	sigaction(SIGALRM, &act, NULL);
	sigaction(SIGBUS, &act, NULL);
	sigaction(SIGFPE, &act, NULL);
	sigaction(SIGHUP, &act, NULL); 
	sigaction(SIGILL, &act, NULL);
	sigaction(SIGPIPE, &act, NULL);
	sigaction(SIGSEGV, &act, NULL);
	sigaction(SIGSYS, &act, NULL);

	sigaction(LL_HEARTBEAT_SIGNAL, &act, NULL);
	sigaction(LL_SMACKDOWN_SIGNAL, &act, NULL);

	// Asynchronous signals that are normally ignored
#ifndef LL_IGNORE_SIGCHLD
	sigaction(SIGCHLD, &act, NULL);
#endif // LL_IGNORE_SIGCHLD
	sigaction(SIGUSR2, &act, NULL);

	// Asynchronous signals that result in attempted graceful exit
	sigaction(SIGHUP, &act, NULL);
	sigaction(SIGTERM, &act, NULL);
	sigaction(SIGINT, &act, NULL);

	// Asynchronous signals that result in core
	sigaction(SIGQUIT, &act, NULL);
}

void clear_signals()
{
	struct sigaction act;
	act.sa_handler = SIG_DFL;
	sigemptyset( &act.sa_mask );
	act.sa_flags = SA_SIGINFO;

	// Synchronous signals
	sigaction(SIGABRT, &act, NULL);
	sigaction(SIGALRM, &act, NULL);
	sigaction(SIGBUS, &act, NULL);
	sigaction(SIGFPE, &act, NULL);
	sigaction(SIGHUP, &act, NULL); 
	sigaction(SIGILL, &act, NULL);
	sigaction(SIGPIPE, &act, NULL);
	sigaction(SIGSEGV, &act, NULL);
	sigaction(SIGSYS, &act, NULL);

	sigaction(LL_HEARTBEAT_SIGNAL, &act, NULL);
	sigaction(LL_SMACKDOWN_SIGNAL, &act, NULL);

	// Asynchronous signals that are normally ignored
#ifndef LL_IGNORE_SIGCHLD
	sigaction(SIGCHLD, &act, NULL);
#endif // LL_IGNORE_SIGCHLD

	// Asynchronous signals that result in attempted graceful exit
	sigaction(SIGHUP, &act, NULL);
	sigaction(SIGTERM, &act, NULL);
	sigaction(SIGINT, &act, NULL);

	// Asynchronous signals that result in core
	sigaction(SIGUSR2, &act, NULL);
	sigaction(SIGQUIT, &act, NULL);
}



void default_unix_signal_handler(int signum, siginfo_t *info, void *)
{
	// Unix implementation of synchronous signal handler
	// This runs in the thread that threw the signal.
	// We do the somewhat sketchy operation of blocking in here until the error handler
	// has gracefully stopped the app.

	if (LLApp::sLogInSignal)
	{
		LL_INFOS() << "Signal handler - Got signal " << signum << " - " << apr_signal_description_get(signum) << LL_ENDL;
	}


	switch (signum)
	{
	case SIGCHLD:
		if (LLApp::sLogInSignal)
		{
			LL_INFOS() << "Signal handler - Got SIGCHLD from " << info->si_pid << LL_ENDL;
		}

		// Check result code for all child procs for which we've
		// registered callbacks THIS WILL NOT WORK IF SIGCHLD IS SENT
		// w/o killing the child (Go, launcher!)
		// TODO: Now that we're using SIGACTION, we can actually
		// implement the launcher behavior to determine who sent the
		// SIGCHLD even if it doesn't result in child termination
		if (LLApp::sChildMap.count(info->si_pid))
		{
			LLApp::sChildMap[info->si_pid].mGotSigChild = TRUE;
		}
		
		LLApp::incSigChildCount();

		return;
	case SIGABRT:
		// Abort just results in termination of the app, no funky error handling.
		if (LLApp::sLogInSignal)
		{
			LL_WARNS() << "Signal handler - Got SIGABRT, terminating" << LL_ENDL;
		}
		clear_signals();
		raise(signum);
		return;
	case SIGINT:
	case SIGHUP:
	case SIGTERM:
		if (LLApp::sLogInSignal)
		{
			LL_WARNS() << "Signal handler - Got SIGINT, HUP, or TERM, exiting gracefully" << LL_ENDL;
		}
		// Graceful exit
		// Just set our state to quitting, not error
		if (LLApp::isQuitting() || LLApp::isError())
		{
			// We're already trying to die, just ignore this signal
			if (LLApp::sLogInSignal)
			{
				LL_INFOS() << "Signal handler - Already trying to quit, ignoring signal!" << LL_ENDL;
			}
			return;
		}
		LLApp::setQuitting();
		return;
	case SIGALRM:
	case SIGPIPE:
	case SIGUSR2:
	default:
		if (signum == LL_SMACKDOWN_SIGNAL ||
		    signum == SIGBUS ||
		    signum == SIGILL ||
		    signum == SIGFPE ||
		    signum == SIGSEGV ||
		    signum == SIGQUIT)
		{ 
			if (signum == LL_SMACKDOWN_SIGNAL)
			{
				// Smackdown treated just like any other app termination, for now
				if (LLApp::sLogInSignal)
				{
					LL_WARNS() << "Signal handler - Handling smackdown signal!" << LL_ENDL;
				}
				else
				{
					// Don't log anything, even errors - this is because this signal could happen anywhere.
					LLError::setDefaultLevel(LLError::LEVEL_NONE);
				}
				
				// Change the signal that we reraise to SIGABRT, so we generate a core dump.
				signum = SIGABRT;
			}
			
			if (LLApp::sLogInSignal)
			{
				LL_WARNS() << "Signal handler - Handling fatal signal!" << LL_ENDL;
			}
			if (LLApp::isError())
			{
				// Received second fatal signal while handling first, just die right now
				// Set the signal handlers back to default before handling the signal - this makes the next signal wipe out the app.
				clear_signals();
				
				if (LLApp::sLogInSignal)
				{
					LL_WARNS() << "Signal handler - Got another fatal signal while in the error handler, die now!" << LL_ENDL;
				}
				raise(signum);
				return;
			}
			
			if (LLApp::sLogInSignal)
			{
				LL_WARNS() << "Signal handler - Flagging error status and waiting for shutdown" << LL_ENDL;
			}
									
			if (LLApp::isCrashloggerDisabled())	// Don't gracefully handle any signal, crash and core for a gdb post mortem
			{
				clear_signals();
				LL_WARNS() << "Fatal signal received, not handling the crash here, passing back to operating system" << LL_ENDL;
				raise(signum);
				return;
			}		
			
			// Flag status to ERROR, so thread_error does its work.
			LLApp::setError();
			// Block in the signal handler until somebody says that we're done.
			while (LLApp::sErrorThreadRunning && !LLApp::isStopped())
			{
				ms_sleep(10);
			}
			
			if (LLApp::sLogInSignal)
			{
				LL_WARNS() << "Signal handler - App is stopped, reraising signal" << LL_ENDL;
			}
			clear_signals();
			raise(signum);
			return;
		} else {
			if (LLApp::sLogInSignal)
			{
				LL_INFOS() << "Signal handler - Unhandled signal " << signum << ", ignoring!" << LL_ENDL;
			}
		}
	}
}
#endif // !WINDOWS
