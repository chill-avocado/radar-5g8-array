//============================================================================
// log.hpp -- logging that cannot itself break the radar
//
// Disk on the target machine is scarce and the user does not want files piling
// up, so nothing is written to disk unless a path is given explicitly, and
// when it is, the file is capped and truncated rather than rotated.  The hot
// path uses log_rate_limited() so a fault that repeats 31 times a second
// prints once a second and counts the rest.
//============================================================================
#pragma once

#include <cstdarg>
#include <cstdio>
#include <string>

namespace radar {

enum class LogLevel : int { Trace = 0, Debug = 1, Info = 2, Warn = 3, Error = 4, Off = 5 };

void        log_set_level(LogLevel l);
LogLevel    log_level();
void        log_set_file(const std::string& path, std::size_t max_bytes = 4u << 20);
void        log_set_colour(bool on);
void        log_write(LogLevel l, const char* fmt, ...) __attribute__((format(printf, 2, 3)));

/// Prints at most once per `period_s` for each distinct call site. Returns the
/// number of occurrences suppressed since the last print, so the message can
/// say how many were swallowed.
unsigned    log_rate_limited(LogLevel l, const char* key, double period_s,
                             const char* fmt, ...) __attribute__((format(printf, 4, 5)));

#define LOG_T(...) ::radar::log_write(::radar::LogLevel::Trace, __VA_ARGS__)
#define LOG_D(...) ::radar::log_write(::radar::LogLevel::Debug, __VA_ARGS__)
#define LOG_I(...) ::radar::log_write(::radar::LogLevel::Info,  __VA_ARGS__)
#define LOG_W(...) ::radar::log_write(::radar::LogLevel::Warn,  __VA_ARGS__)
#define LOG_E(...) ::radar::log_write(::radar::LogLevel::Error, __VA_ARGS__)

} // namespace radar
