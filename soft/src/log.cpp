#include "radar/log.hpp"

#include <atomic>
#include <chrono>
#include <cstring>
#include <mutex>
#include <unordered_map>

namespace radar {
namespace {

std::atomic<int>  g_level{int(LogLevel::Info)};
std::atomic<bool> g_colour{true};
std::mutex        g_mutex;
FILE*             g_file       = nullptr;
std::size_t       g_file_bytes = 0;
std::size_t       g_file_cap   = 0;

struct RateEntry {
    double   last   = -1e30;
    unsigned skipped = 0;
};
std::unordered_map<std::string, RateEntry> g_rate;

double mono() {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

const char* level_tag(LogLevel l) {
    switch (l) {
        case LogLevel::Trace: return "trace";
        case LogLevel::Debug: return "debug";
        case LogLevel::Info:  return "info ";
        case LogLevel::Warn:  return "WARN ";
        case LogLevel::Error: return "ERROR";
        default:              return "     ";
    }
}

const char* level_colour(LogLevel l) {
    switch (l) {
        case LogLevel::Trace: return "\033[38;5;244m";
        case LogLevel::Debug: return "\033[38;5;110m";
        case LogLevel::Info:  return "\033[0m";
        case LogLevel::Warn:  return "\033[38;5;214m";
        case LogLevel::Error: return "\033[38;5;203m";
        default:              return "\033[0m";
    }
}

void emit(LogLevel l, const char* text) {
    const double t = mono();
    static const double t0 = t;

    std::lock_guard<std::mutex> lock(g_mutex);

    FILE* out = (l >= LogLevel::Warn) ? stderr : stdout;
    if (g_colour.load(std::memory_order_relaxed)) {
        std::fprintf(out, "%s[%8.3f] %s  %s\033[0m\n", level_colour(l), t - t0, level_tag(l), text);
    } else {
        std::fprintf(out, "[%8.3f] %s  %s\n", t - t0, level_tag(l), text);
    }
    std::fflush(out);

    if (g_file && g_file_bytes < g_file_cap) {
        const int n = std::fprintf(g_file, "[%8.3f] %s  %s\n", t - t0, level_tag(l), text);
        if (n > 0) g_file_bytes += std::size_t(n);
        if (g_file_bytes >= g_file_cap) {
            std::fprintf(g_file, "[log capped at %zu bytes, further output goes to the console only]\n",
                         g_file_cap);
            std::fflush(g_file);
        }
    }
}

} // namespace

void log_set_level(LogLevel l) { g_level.store(int(l), std::memory_order_relaxed); }
LogLevel log_level() { return LogLevel(g_level.load(std::memory_order_relaxed)); }
void log_set_colour(bool on) { g_colour.store(on, std::memory_order_relaxed); }

void log_set_file(const std::string& path, std::size_t max_bytes) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_file) { std::fclose(g_file); g_file = nullptr; }
    g_file_bytes = 0;
    g_file_cap   = max_bytes;
    if (!path.empty()) g_file = std::fopen(path.c_str(), "w");
}

void log_write(LogLevel l, const char* fmt, ...) {
    if (int(l) < g_level.load(std::memory_order_relaxed)) return;
    char    buf[2048];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    emit(l, buf);
}

unsigned log_rate_limited(LogLevel l, const char* key, double period_s, const char* fmt, ...) {
    if (int(l) < g_level.load(std::memory_order_relaxed)) return 0;

    unsigned skipped = 0;
    bool     print   = false;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        RateEntry&   e = g_rate[key];
        const double t = mono();
        if (t - e.last >= period_s) {
            skipped = e.skipped;
            e.skipped = 0;
            e.last  = t;
            print   = true;
        } else {
            ++e.skipped;
        }
    }
    if (!print) return 0;

    char    buf[2048];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    if (skipped) {
        char buf2[2176];
        std::snprintf(buf2, sizeof(buf2), "%s  (+%u more suppressed)", buf, skipped);
        emit(l, buf2);
    } else {
        emit(l, buf);
    }
    return skipped;
}

} // namespace radar
