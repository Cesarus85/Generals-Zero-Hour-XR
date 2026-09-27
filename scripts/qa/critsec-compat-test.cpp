// GeneralsX @test 19/09/2026 threads_compat CRITICAL_SECTION: nested
// enter/exit must fully release, so a second thread can enter afterwards.
// Guards the archive-read lock used by background texture streaming.
#include "threads_compat.h"
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <pthread.h>
#include <time.h>
static int checks = 0;
static void check(bool v) { ++checks; if (!v) { fprintf(stderr, "critsec check %d failed\n", checks); exit(1); } }
static CRITICAL_SECTION cs;
static std::atomic<bool> workerEntered{false};
static void *worker(void *) {
	EnterCriticalSection(&cs);
	workerEntered = true;
	LeaveCriticalSection(&cs);
	return nullptr;
}
static void sleepMs(long ms) {
	const struct timespec slice{0, ms * 1000000L};
	nanosleep(&slice, nullptr);
}
int main() {
	InitializeCriticalSection(&cs);
	EnterCriticalSection(&cs);
	LeaveCriticalSection(&cs);
	// Nested enter/exit must balance: a cross-thread enter afterwards proves
	// the mutex is fully released instead of stuck at a residual count.
	EnterCriticalSection(&cs);
	EnterCriticalSection(&cs);
	LeaveCriticalSection(&cs);
	LeaveCriticalSection(&cs);
	pthread_t w;
	check(pthread_create(&w, nullptr, &worker, nullptr) == 0);
	bool entered = false;
	for (int i = 0; i < 200 && !entered; ++i) {
		entered = workerEntered;
		if (!entered) sleepMs(10);
	}
	if (!entered) pthread_detach(w); // never hang the harness on failure
	check(entered);
	if (entered) pthread_join(w, nullptr);
	DeleteCriticalSection(&cs);
	printf("PASS %d critsec compat checks\n", checks);
}
