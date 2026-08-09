//============================================================================
// thread.hpp -- real-time scheduling, on macOS as well as Linux
//
// The receive thread has a hard deadline: one coherent processing interval is
// 32 ms and the USB buffer behind it holds about that much.  Miss the deadline
// and the radio reports an overflow, which costs a frame.  Asking the operating
// system for a real-time guarantee is therefore not decoration.
//
// The two operating systems express the same idea very differently.
//
//   macOS   thread_policy_set(THREAD_TIME_CONSTRAINT_POLICY).  You describe
//           the work: "every `period`, I need `computation` of processor time,
//           and it must all land inside `constraint` of the period starting".
//           The kernel then schedules the thread ahead of everything in the
//           normal timeshare band.  The three numbers are in *mach absolute
//           time units*, which are NOT nanoseconds on every machine -- see
//           set_realtime()'s implementation.
//
//   Linux   pthread_setschedparam with SCHED_FIFO and a fixed priority.  No
//           notion of a budget: the thread simply outranks everything below it.
//
// Everything here degrades gracefully.  A laptop that refuses the request, a
// container without CAP_SYS_NICE, a machine with no affinity support -- each
// returns false and the caller carries on at normal priority.  Nothing throws,
// nothing aborts, nothing writes to the console.
//============================================================================
#pragma once

namespace radar {
namespace rt {

/// Ask for a real-time guarantee on the calling thread.
///
/// @param period_s      how often the work repeats, seconds.  Pass 0 for work
///                      that is not periodic; the kernel then treats
///                      computation as a one-off budget.
/// @param compute_s     processor time needed each period, seconds.
/// @param constraint_s  deadline measured from the start of the period,
///                      seconds.  Must be >= compute_s; equal to it means
///                      "no slack at all", which the kernel is entitled to
///                      refuse under load.
/// @return true if the policy was accepted.  False means the thread is still
///         running, just at normal priority.
bool set_realtime(double period_s, double compute_s, double constraint_s);

/// Undo set_realtime() and return the calling thread to normal scheduling.
bool clear_realtime();

/// True when the calling thread currently holds a real-time policy.  Reads the
/// answer back from the kernel rather than remembering what was asked for, so
/// it is a genuine check.  The three out-parameters, when not null, receive the
/// policy actually in force, in seconds.
bool is_realtime(double* period_s = nullptr,
                 double* compute_s = nullptr,
                 double* constraint_s = nullptr);

/// Bind the calling thread to a processor core.
///
/// On Linux this is a hard binding through pthread_setaffinity_np.
///
/// On macOS there is no way to pin a thread to a numbered core: the kernel
/// deliberately does not expose one.  What it does expose is an affinity
/// *tag* -- threads sharing a tag are scheduled onto cores that share a cache,
/// and threads with different tags are pushed apart.  Passing a core number
/// here sets the tag to that number, which gets the cache behaviour that
/// matters without pretending to a control the platform does not offer.
/// Pass a negative core to clear the tag.
bool set_affinity(int core);

/// Name the calling thread, so it is identifiable in Instruments, Activity
/// Monitor, top and a debugger.  macOS allows 63 characters, Linux 15.
bool set_name(const char* name);

/// Physical cores, not hardware threads.  Hyperthreads share the execution
/// units this pipeline saturates, so the worker count should follow this and
/// not std::thread::hardware_concurrency().  Returns 1 if it cannot be found.
int physical_cores();

/// Seconds from an arbitrary fixed origin, monotonic, unaffected by the wall
/// clock being adjusted.  Used for every interval and deadline in the stack.
double now_s();

/// RAII wrapper: takes the real-time policy on construction and puts the old
/// one back on destruction, including on an exception unwinding through the
/// scope.  ok() reports whether the request was granted.
class ScopedRealtime {
public:
    ScopedRealtime(double period_s, double compute_s, double constraint_s);
    ~ScopedRealtime();

    ScopedRealtime(const ScopedRealtime&)            = delete;
    ScopedRealtime& operator=(const ScopedRealtime&) = delete;
    ScopedRealtime(ScopedRealtime&&)                 = delete;
    ScopedRealtime& operator=(ScopedRealtime&&)      = delete;

    bool ok() const { return ok_; }

private:
    bool   ok_          = false;
    bool   had_prior_   = false;   ///< the thread was already real-time
    double prior_[3]    = {0, 0, 0};
};

} // namespace rt
} // namespace radar
