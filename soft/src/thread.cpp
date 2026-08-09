//============================================================================
// thread.cpp -- see thread.hpp for what this is for and why
//============================================================================
#include "radar/thread.hpp"

#include <chrono>
#include <cstring>

#if defined(__APPLE__)
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <mach/thread_act.h>
#include <mach/thread_policy.h>
#include <pthread.h>
#include <sys/sysctl.h>
#elif defined(__linux__)
#include <pthread.h>
#include <sched.h>
#include <unistd.h>
#include <cstdio>
#endif

namespace radar {
namespace rt {

//----------------------------------------------------------------------------
// macOS
//----------------------------------------------------------------------------
#if defined(__APPLE__)

namespace {

/// Mach absolute time ticks per second.
///
/// mach_absolute_time() counts in a unit the hardware chose, and
/// mach_timebase_info() gives the conversion: nanoseconds = ticks * numer /
/// denom.  On Intel Macs numer and denom are both 1, so a tick is a nanosecond
/// and every example on the internet that hard-codes nanoseconds happens to
/// work.  On Apple Silicon numer is 125 and denom is 3, so a tick is 41.67 ns
/// and the same code asks for a period 41.67 times too long -- the request is
/// accepted and the guarantee is worthless.  Hence this.
double ticks_per_second() {
    static double cached = [] {
        mach_timebase_info_data_t tb{};
        if (mach_timebase_info(&tb) != KERN_SUCCESS || tb.numer == 0) return 1e9;
        return 1e9 * double(tb.denom) / double(tb.numer);
    }();
    return cached;
}

/// Clamp a seconds value into the uint32_t the kernel takes, never zero for a
/// quantity the kernel requires to be positive.
uint32_t to_ticks(double seconds, bool allow_zero) {
    if (!(seconds > 0.0)) return allow_zero ? 0u : 1u;
    const double t = seconds * ticks_per_second();
    if (t >= 4294967295.0) return 4294967295u;
    const uint32_t v = uint32_t(t);
    return (v == 0 && !allow_zero) ? 1u : v;
}

mach_port_t this_thread() { return pthread_mach_thread_np(pthread_self()); }

/// Read the policy actually in force.  `is_set` comes back false when the
/// kernel handed us the system default rather than something this thread asked
/// for, which is how "am I really real-time?" is answered.
bool read_time_constraint(thread_time_constraint_policy_data_t& out, bool& is_set) {
    mach_msg_type_number_t count = THREAD_TIME_CONSTRAINT_POLICY_COUNT;
    boolean_t get_default        = FALSE;
    const kern_return_t kr =
        thread_policy_get(this_thread(), THREAD_TIME_CONSTRAINT_POLICY,
                          reinterpret_cast<thread_policy_t>(&out), &count, &get_default);
    if (kr != KERN_SUCCESS) return false;
    is_set = (get_default == FALSE);
    return true;
}

} // namespace

bool set_realtime(double period_s, double compute_s, double constraint_s) {
    if (!(compute_s > 0.0)) return false;
    if (constraint_s < compute_s) constraint_s = compute_s;

    thread_time_constraint_policy_data_t pol{};
    pol.period      = to_ticks(period_s, /*allow_zero=*/true);
    pol.computation = to_ticks(compute_s, false);
    pol.constraint  = to_ticks(constraint_s, false);
    // preemptible = 0 says the thread must not be interrupted inside its
    // computation window.  The kernel honours this only up to the budget above,
    // which is why asking for a realistic computation time matters: overstate
    // it and the request is refused, understate it and the thread is preempted
    // mid-frame anyway.
    pol.preemptible = 0;

    const kern_return_t kr =
        thread_policy_set(this_thread(), THREAD_TIME_CONSTRAINT_POLICY,
                          reinterpret_cast<thread_policy_t>(&pol),
                          THREAD_TIME_CONSTRAINT_POLICY_COUNT);
    return kr == KERN_SUCCESS;
}

bool clear_realtime() {
    thread_standard_policy_data_t pol{};
    const kern_return_t kr =
        thread_policy_set(this_thread(), THREAD_STANDARD_POLICY,
                          reinterpret_cast<thread_policy_t>(&pol),
                          THREAD_STANDARD_POLICY_COUNT);
    return kr == KERN_SUCCESS;
}

bool is_realtime(double* period_s, double* compute_s, double* constraint_s) {
    thread_time_constraint_policy_data_t pol{};
    bool is_set = false;
    if (!read_time_constraint(pol, is_set)) return false;
    if (!is_set) return false;
    const double tps = ticks_per_second();
    if (period_s)     *period_s     = pol.period / tps;
    if (compute_s)    *compute_s    = pol.computation / tps;
    if (constraint_s) *constraint_s = pol.constraint / tps;
    return true;
}

bool set_affinity(int core) {
    thread_affinity_policy_data_t pol{};
    // THREAD_AFFINITY_TAG_NULL (0) means "no preference".  Tags are advisory
    // hints about which threads want to share a cache, so the core number is
    // used as the tag: different cores get different tags and the kernel keeps
    // them apart, which is the part that actually matters here.
    pol.affinity_tag = (core < 0) ? THREAD_AFFINITY_TAG_NULL : integer_t(core + 1);
    const kern_return_t kr =
        thread_policy_set(this_thread(), THREAD_AFFINITY_POLICY,
                          reinterpret_cast<thread_policy_t>(&pol),
                          THREAD_AFFINITY_POLICY_COUNT);
    return kr == KERN_SUCCESS;
}

bool set_name(const char* name) {
    if (!name) return false;
    char buf[64];
    std::strncpy(buf, name, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    return pthread_setname_np(buf) == 0;
}

int physical_cores() {
    int    n   = 0;
    size_t len = sizeof(n);
    if (sysctlbyname("hw.physicalcpu", &n, &len, nullptr, 0) == 0 && n > 0) return n;
    len = sizeof(n);
    if (sysctlbyname("hw.ncpu", &n, &len, nullptr, 0) == 0 && n > 0) return n;
    return 1;
}

//----------------------------------------------------------------------------
// Linux
//----------------------------------------------------------------------------
#elif defined(__linux__)

namespace {
/// Priority to ask for.  Deliberately not the maximum: leaving headroom above
/// means a watchdog or the kernel's own threads can still run.
int fifo_priority() {
    const int lo = sched_get_priority_min(SCHED_FIFO);
    const int hi = sched_get_priority_max(SCHED_FIFO);
    if (hi <= lo) return lo;
    return lo + (hi - lo) * 3 / 4;
}
} // namespace

bool set_realtime(double period_s, double compute_s, double constraint_s) {
    (void)period_s; (void)compute_s; (void)constraint_s;
    sched_param sp{};
    sp.sched_priority = fifo_priority();
    return pthread_setschedparam(pthread_self(), SCHED_FIFO, &sp) == 0;
}

bool clear_realtime() {
    sched_param sp{};
    sp.sched_priority = 0;
    return pthread_setschedparam(pthread_self(), SCHED_OTHER, &sp) == 0;
}

bool is_realtime(double* period_s, double* compute_s, double* constraint_s) {
    int         policy = 0;
    sched_param sp{};
    if (pthread_getschedparam(pthread_self(), &policy, &sp) != 0) return false;
    if (policy != SCHED_FIFO && policy != SCHED_RR) return false;
    // SCHED_FIFO carries no budget, so the numbers the caller asked for cannot
    // be read back.  Report zeros and let the boolean carry the answer.
    if (period_s)     *period_s     = 0.0;
    if (compute_s)    *compute_s    = 0.0;
    if (constraint_s) *constraint_s = 0.0;
    return true;
}

bool set_affinity(int core) {
    cpu_set_t set;
    CPU_ZERO(&set);
    if (core < 0) {
        const long n = sysconf(_SC_NPROCESSORS_ONLN);
        for (long i = 0; i < (n > 0 ? n : 1); ++i) CPU_SET(size_t(i), &set);
    } else {
        CPU_SET(size_t(core), &set);
    }
    return pthread_setaffinity_np(pthread_self(), sizeof(set), &set) == 0;
}

bool set_name(const char* name) {
    if (!name) return false;
    char buf[16];                       // Linux truncates at 16 including the nul
    std::strncpy(buf, name, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    return pthread_setname_np(pthread_self(), buf) == 0;
}

int physical_cores() {
    // Count distinct (physical id, core id) pairs in /proc/cpuinfo, so
    // hyperthreads are not double counted.
    std::FILE* f = std::fopen("/proc/cpuinfo", "r");
    if (f) {
        char line[256];
        int  pkg = -1, core = -1, n = 0;
        int  seen[1024][2];
        while (std::fgets(line, sizeof(line), f)) {
            if (std::strncmp(line, "physical id", 11) == 0) {
                const char* c = std::strchr(line, ':');
                if (c) pkg = std::atoi(c + 1);
            } else if (std::strncmp(line, "core id", 7) == 0) {
                const char* c = std::strchr(line, ':');
                if (c) core = std::atoi(c + 1);
            } else if (line[0] == '\n' && pkg >= 0 && core >= 0) {
                bool dup = false;
                for (int i = 0; i < n; ++i)
                    if (seen[i][0] == pkg && seen[i][1] == core) { dup = true; break; }
                if (!dup && n < 1024) { seen[n][0] = pkg; seen[n][1] = core; ++n; }
                pkg = core = -1;
            }
        }
        std::fclose(f);
        if (n > 0) return n;
    }
    const long n = sysconf(_SC_NPROCESSORS_ONLN);
    return n > 0 ? int(n) : 1;
}

//----------------------------------------------------------------------------
// Anywhere else: succeed at nothing, quietly.
//----------------------------------------------------------------------------
#else

bool set_realtime(double, double, double) { return false; }
bool clear_realtime()                     { return false; }
bool is_realtime(double*, double*, double*) { return false; }
bool set_affinity(int)                    { return false; }
bool set_name(const char*)                { return false; }
int  physical_cores()                     { return 1; }

#endif

//----------------------------------------------------------------------------
// Portable pieces
//----------------------------------------------------------------------------
double now_s() {
    using clock = std::chrono::steady_clock;
    static const clock::time_point origin = clock::now();
    return std::chrono::duration<double>(clock::now() - origin).count();
}

ScopedRealtime::ScopedRealtime(double period_s, double compute_s, double constraint_s) {
    had_prior_ = is_realtime(&prior_[0], &prior_[1], &prior_[2]);
    ok_        = set_realtime(period_s, compute_s, constraint_s);
}

ScopedRealtime::~ScopedRealtime() {
    if (!ok_) return;                       // nothing was changed, change nothing back
    if (had_prior_ && prior_[1] > 0.0) {
        set_realtime(prior_[0], prior_[1], prior_[2]);
    } else {
        clear_realtime();
    }
}

} // namespace rt
} // namespace radar
