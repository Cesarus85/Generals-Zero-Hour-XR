// GeneralsX @test 19/09/2026 POSIX ThreadClass revival: Execute runs the
// thread body on a second thread, Stop joins it, restart works.
#include "thread.h"
#include <atomic>
#include <cstdio>
#include <cstdlib>
static int checks = 0;
static void check(bool v) { ++checks; if (!v) { fprintf(stderr, "thread check %d failed\n", checks); exit(1); } }
static std::atomic<unsigned> ticks{0};
static std::atomic<unsigned> workerSeenId{0};
static unsigned mainId = 0;
class CounterThread : public ThreadClass {
public:
	CounterThread() : ThreadClass("wlib-test-tick") {}
	void Thread_Function() override {
		workerSeenId = _Get_Current_Thread_ID();
		while (running) { ++ticks; Switch_Thread(); }
	}
};
// Spins until ticks advances past baseline or ~2s elapse; returns the ticks seen.
static unsigned waitTicks(unsigned baseline) {
	for (int i = 0; i < 200 && ticks <= baseline; ++i) ThreadClass::Sleep_Ms(10);
	return ticks;
}
int main() {
	mainId = ThreadClass::_Get_Current_Thread_ID();
	check(mainId != 0);
	CounterThread t;
	check(!t.Is_Running());
	t.Execute();
	check(waitTicks(0) > 0);
	check(t.Is_Running());
	check(workerSeenId != 0 && workerSeenId != mainId);
	t.Stop(3000);
	check(!t.Is_Running());
	const unsigned frozen = ticks;
	ThreadClass::Sleep_Ms(50);
	check(ticks == frozen);
	// Restart after Stop works and joins again.
	t.Execute();
	check(waitTicks(frozen) > frozen);
	t.Stop(3000);
	check(!t.Is_Running());
	printf("PASS %d wlib thread checks\n", checks);
}
