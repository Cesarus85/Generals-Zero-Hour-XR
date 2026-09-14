// Standalone (no DXVK/Android build dependency) litmus test for the
// "relaxed increment / release decrement + acquire fence on last release"
// refcounting idiom used across references/fbraz3-dxvk after
// Patches/dxvk-resource-refcount-memory-order.patch and
// Patches/dxvk-refcount-memory-order-audit.patch.
//
// Runs the exact synchronization pattern (not the real DXVK classes -- this
// has no DXVK/Vulkan/Android dependency, just the atomic refcount discipline
// itself) under two configurations:
//   BUGGY:  incRef=fetch_add(acquire), decRef=fetch_sub(acquire), no fence
//           (the pattern every one of the 6 fixed classes originally had)
//   FIXED:  incRef=fetch_add(relaxed), decRef=fetch_sub(release),
//           atomic_thread_fence(acquire) taken only by the thread that
//           observes the count reach zero
//
// Each iteration: two threads share one object (refcount starts at 2). The
// "writer" thread writes a payload value then drops its reference. The
// "releaser" thread (biased via a short sleep to run its decRef second,
// without that sleep itself providing any memory synchronization) drops the
// other reference and, since it is guaranteed to be the one bringing the
// count to zero, reads the payload the writer thread wrote and is
// responsible for destroying the object. Under the BUGGY discipline this is
// a genuine, ThreadSanitizer-detectable data race on `payload` (no proven
// happens-before edge between the write and the read); under FIXED it is
// not.
//
// Build (either configuration): g++ -std=c++17 -O1 -g -fsanitize=thread
//   -DLITMUS_MODE=0 -o litmus_buggy  litmus.cpp   (expect: TSan reports a race)
//   -DLITMUS_MODE=1 -o litmus_fixed  litmus.cpp   (expect: TSan STILL reports a
//     race here -- known TSan limitation, it does not model standalone
//     atomic_thread_fence; the compiler even warns about this. The code is
//     correct per the C++ standard (the exact Boost/libstdc++ shared_ptr
//     idiom) but TSan can't see it. Kept only to document/demonstrate the
//     false positive, not as a trustworthy verdict.)
//   -DLITMUS_MODE=2 -o litmus_tsan_ok litmus.cpp  (expect: clean. Same
//     release-decrement idea, but the synchronization is expressed as a
//     second acquire-ordered operation directly on the atomic itself --
//     a dummy load -- instead of a decoupled fence. TSan tracks
//     synchronization through operations on the atomic object directly, so
//     this variant is both standard-correct AND TSan-visible; use it as the
//     trustworthy "definitely fixed" reference when auditing real code.)

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <chrono>

#ifndef LITMUS_MODE
#define LITMUS_MODE 0
#endif

#ifndef LITMUS_ITERATIONS
#define LITMUS_ITERATIONS 200
#endif

struct Resource {
  std::atomic<uint32_t> refs{2};
  volatile int payload = 0;

  void incRef() {
#if LITMUS_MODE == 0
    refs.fetch_add(1u, std::memory_order_acquire);
#else
    refs.fetch_add(1u, std::memory_order_relaxed);
#endif
  }

  // Returns true if this call observed the count reach zero (caller owns
  // destruction/read-back in that case).
  bool decRef() {
#if LITMUS_MODE == 0
    return refs.fetch_sub(1u, std::memory_order_acquire) == 1u;
#elif LITMUS_MODE == 1
    if (refs.fetch_sub(1u, std::memory_order_release) == 1u) {
      std::atomic_thread_fence(std::memory_order_acquire);
      return true;
    }
    return false;
#else // LITMUS_MODE == 2: TSan-visible equivalent of the fence idiom
    if (refs.fetch_sub(1u, std::memory_order_release) == 1u) {
      (void)refs.load(std::memory_order_acquire);
      return true;
    }
    return false;
#endif
  }
};

int main() {
  int corruptionCount = 0;

  for (int i = 0; i < LITMUS_ITERATIONS; i++) {
    Resource *obj = new Resource();

    std::thread writer([obj] {
      obj->payload = 42; // "final write" that must be visible before destroy
      obj->decRef();     // always the non-last release (writer goes first)
    });

    std::thread releaser([obj, &corruptionCount, i] {
      // Bias (not synchronize) execution order: give the writer thread a
      // real chance to finish its write + decRef first. This does not
      // establish any happens-before edge on `payload` by itself -- it
      // only makes the "releaser is the one reaching zero" outcome
      // reliable enough to exercise every iteration instead of depending
      // on scheduler luck.
      std::this_thread::sleep_for(std::chrono::microseconds(50));
      bool last = obj->decRef();
      if (last) {
        int v = obj->payload;
        if (v != 42) {
          fprintf(stderr, "[iter %d] CORRUPTION DETECTED: payload=%d (expected 42)\n", i, v);
          corruptionCount++;
        }
        delete obj;
      } else {
        fprintf(stderr, "[iter %d] unexpected: releaser was not the last ref\n", i);
      }
    });

    writer.join();
    releaser.join();
  }

  if (corruptionCount > 0) {
    fprintf(stderr, "\n%d/%d iterations observed a stale payload value.\n", corruptionCount, LITMUS_ITERATIONS);
  } else {
    fprintf(stderr, "\nAll %d iterations observed the correct payload value (functional check only -- "
      "see ThreadSanitizer's own report above/below for the real verdict on the BUGGY build,\n"
      "since a functionally-correct-looking run can still contain a data race that just didn't "
      "manifest an observable wrong value this time).\n", LITMUS_ITERATIONS);
  }

  return 0;
}
