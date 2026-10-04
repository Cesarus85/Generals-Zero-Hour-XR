/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#include "thread.h"
#include "Except.h"
#include "wwdebug.h"
#pragma warning ( push )
#pragma warning ( disable : 4201 )
#include "systimer.h"
#pragma warning ( pop )

#ifdef _WIN32
#include <process.h>
#include <windows.h>
#else
// GeneralsX @bugfix BenderAI 24/02/2026 Phase 5 - GetCurrentThreadIdAsInt for non-Windows
#include "thread_compat.h"
#endif
#ifdef _UNIX
#include <cstdio>
#include <time.h>
#endif

ThreadClass::ThreadClass(const char *thread_name, ExceptionHandlerType exception_handler) : handle(0), running(false), thread_priority(0)
{
	if (thread_name) {
		size_t nameLen = strlcpy(ThreadName, thread_name, ARRAY_SIZE(ThreadName));
		(void)nameLen; assert(nameLen < ARRAY_SIZE(ThreadName));
	} else {
		strcpy(ThreadName, "No name");
	}

	ExceptionHandler = exception_handler;
}

ThreadClass::~ThreadClass()
{
	Stop();
}

void __cdecl ThreadClass::Internal_Thread_Function(void* params)
{
	ThreadClass* tc=reinterpret_cast<ThreadClass*>(params);
	tc->running=true;
	// GeneralsX @bugfix BenderAI 24/02/2026 Phase 5 - pthread_t is a pointer on macOS; use int wrapper
#ifdef _WIN32
	tc->ThreadID = GetCurrentThreadId();
#else
	tc->ThreadID = GetCurrentThreadIdAsInt();
#endif

#ifdef _WIN32
	Register_Thread_ID(tc->ThreadID, tc->ThreadName);

#if defined(_MSC_VER)
	// MSVC supports structured exception handling (__try/__except)
	if (tc->ExceptionHandler != nullptr) {
		__try {
			tc->Thread_Function();
		} __except(tc->ExceptionHandler(GetExceptionCode(), GetExceptionInformation())) {};
	} else {
		tc->Thread_Function();
	}
#elif defined(__GNUC__) && defined(_WIN32)
	// GCC/MinGW-w64 doesn't support MSVC's __try/__except syntax
	// Call Thread_Function directly without SEH support
	tc->Thread_Function();
#else
	#error "ThreadClass::Internal_Thread_Function: Unsupported compiler. This code requires MSVC or GCC/MinGW-w64 targeting Windows."
#endif

#else //_WIN32
#ifdef _UNIX
	// Name the thread for ps/Perfetto/simpleperf (the kernel truncates to 15
	// chars). glibc hides pthread_setname_np without _GNU_SOURCE, so only use
	// it where it is unconditionally declared (Apple 1-arg, bionic 2-arg).
#if defined(__APPLE__)
	pthread_setname_np(tc->ThreadName);
#elif defined(__ANDROID__)
	pthread_setname_np(pthread_self(), tc->ThreadName);
#endif
#endif
	tc->Thread_Function();
#endif //_WIN32

#ifdef _WIN32
	Unregister_Thread_ID(tc->ThreadID, tc->ThreadName);
#endif // _WIN32
	tc->handle=0;
	tc->ThreadID = 0;
#ifdef _UNIX
	tc->posixFinished = true;
#endif
}

#ifdef _UNIX
void *ThreadClass::Posix_Trampoline(void *params)
{
	Internal_Thread_Function(params);
	return nullptr;
}
#endif

void ThreadClass::Execute()
{
	#ifdef _UNIX
		// GeneralsX @feature 19/09/2026 POSIX revival: actually spawn the
		// thread. Only one at a time; failures are logged, never fatal.
		WWASSERT(!posixHasThread);
		if (posixHasThread) {
			return;
		}
		running = false;
		posixFinished = false;
		const int rc = pthread_create(&posixThread, nullptr, &Posix_Trampoline, this);
		if (rc != 0) {
			fprintf(stderr, "[ThreadClass] pthread_create '%s' failed: %d\n", ThreadName, rc);
			return;
		}
		posixHasThread = true;
		fprintf(stderr, "[ThreadClass] started '%s'\n", ThreadName);
	#else
		WWASSERT(!handle);	// Only one thread at a time!
		handle=_beginthread(&Internal_Thread_Function,0,this);
		SetThreadPriority((HANDLE)handle,THREAD_PRIORITY_NORMAL+thread_priority);
		WWDEBUG_SAY(("ThreadClass::Execute: Started thread %s, thread ID is %X", ThreadName, handle));
	#endif
}

void ThreadClass::Set_Priority(int priority)
{
	#ifdef _UNIX
		// POSIX has no unprivileged per-thread priority (sched params need
		// privileges, nice is process-wide): record the hint only.
		thread_priority=priority;
		return;
	#else
		thread_priority=priority;
		if (handle) SetThreadPriority((HANDLE)handle,THREAD_PRIORITY_NORMAL+thread_priority);
	#endif
}

void ThreadClass::Stop(unsigned ms)
{
	#ifdef _UNIX
		// GeneralsX @feature 19/09/2026 POSIX revival: bounded join. Never
		// cancel: bionic has no pthread_cancel, and a thread stuck holding a
		// spinlock must not be killed. On timeout the thread is detached and
		// runs out on its own, mirroring thread_compat's TerminateThread.
		running=false;
		if (!posixHasThread) {
			return;
		}
		const unsigned sliceMs = 10;
		unsigned waited = 0;
		while (!posixFinished && waited < ms) {
			const struct timespec slice{0, (long)sliceMs * 1000000L};
			nanosleep(&slice, nullptr);
			waited += sliceMs;
		}
		if (posixFinished) {
			pthread_join(posixThread, nullptr);
			fprintf(stderr, "[ThreadClass] joined '%s'\n", ThreadName);
		} else {
			pthread_detach(posixThread);
			fprintf(stderr, "[ThreadClass] '%s' did not exit in %ums, detached\n", ThreadName, ms);
		}
		posixHasThread = false;
	#else
		running=false;
		unsigned time=TIMEGETTIME();
		while (handle) {
			if ((TIMEGETTIME()-time)>ms) {
				int res=TerminateThread((HANDLE)handle,0);
				res;	// just to silence compiler warnings
				WWASSERT(res);	// Thread still not killed!
				handle=0;
			}
			Sleep(0);
		}
	#endif
}

void ThreadClass::Sleep_Ms(unsigned ms)
{
	Sleep(ms);
}

#ifndef _UNIX
HANDLE test_event = ::CreateEvent (nullptr, FALSE, FALSE, "");
#endif

void ThreadClass::Switch_Thread()
{
	#ifdef _UNIX
		// Windows waits 1ms on an unsignalled event here; mirror that so idle
		// thread spins yield the core instead of busy-waiting. All callers
		// are idle loops (loader, flush pump, legacy network threads).
		const struct timespec slice{0, 1000000L};
		nanosleep(&slice, nullptr);
	#else
		//	::SwitchToThread ();
		::WaitForSingleObject (test_event, 1);
		//	Sleep(1);	// Note! Parameter can not be 0 (or the thread switch doesn't occur)
	#endif
}

// Return calling thread's unique thread id
unsigned ThreadClass::_Get_Current_Thread_ID()
{
	#ifdef _UNIX
		return (unsigned)GetCurrentThreadIdAsInt();
	#else
		return GetCurrentThreadId();
	#endif
}

bool ThreadClass::Is_Running()
{
#ifdef _UNIX
	return posixHasThread;
#else
	return !!handle;
#endif
}
