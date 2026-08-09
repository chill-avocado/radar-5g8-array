//============================================================================
// net.hpp -- the HTTP and WebSocket server that carries the display
//
// No dependencies beyond POSIX sockets and the standard library.  The
// handshake, the frame codec and the HTTP parsing are all in net.cpp, because
// pulling a whole web framework into a real-time radar to move a few megabytes
// a second over loopback would be the tail wagging the dog -- and because
// every line of a defence platform's network surface should be one you can
// read in an afternoon.
//
// The single rule that shapes the whole design: the display must never
// backpressure the DSP.  broadcast() is called from the pipeline thread and
// never blocks on a socket, never waits on a syscall, and never waits for a
// slow client.  A client that cannot keep up loses frames.  That is the
// correct trade: a stale picture is worse than a skipped one, and a stalled
// pipeline is worse than both.
//============================================================================
#pragma once

#include <cstring>              // core.hpp uses memset; include before it
#include "radar/core.hpp"

#include <functional>
#include <memory>
#include <string>

namespace radar {

class WebServer {
public:
    /// Binds 127.0.0.1 only, and serves the built-in fallback page.
    explicit WebServer(int port);
    WebServer(int port, std::string web_root, bool allow_remote);
    ~WebServer();

    WebServer(const WebServer&)            = delete;
    WebServer& operator=(const WebServer&) = delete;

    //------------------------------------------------------------------
    // Configuration.  All of it must be set before start().
    //------------------------------------------------------------------

    /// Directory holding index.html.  Empty means "work it out": next to the
    /// executable, then up towards the repository root.  See
    /// resolve_index_path().
    void set_web_root(std::string root);

    /// Listen on 0.0.0.0 instead of 127.0.0.1.  Off by default and it stays
    /// off unless somebody types this line on purpose.  The display exposes
    /// the radar's live picture and its controls; putting that on every
    /// interface of the machine is a decision, not a default.
    void set_allow_remote(bool yes);

    /// How much a client may fall behind before its queue is trimmed to the
    /// newest frame.  Either limit triggers the trim.
    void set_queue_limit(std::size_t frames, std::size_t bytes);

    /// Free-text build identifier, reported in the Hello message and /status.
    void set_build_id(std::string id);

    //------------------------------------------------------------------
    // Lifetime
    //------------------------------------------------------------------

    /// Binds, listens and starts the poll thread.  Returns false and fills
    /// `err` with a plain-English reason if it cannot.
    bool start(std::string& err);

    /// Closes every connection and joins the thread.  Safe to call twice, and
    /// called by the destructor.
    void stop();

    bool        running() const;
    int         port() const;
    /// The address to type into a browser, e.g. "http://127.0.0.1:8730/".
    std::string url() const;

    //------------------------------------------------------------------
    // Traffic
    //------------------------------------------------------------------

    /// Broadcast a binary WebSocket message to every connected client.
    /// Non-blocking: a client that cannot keep up gets its queue trimmed to
    /// the newest frame rather than stalling the radar.  This matters -- the
    /// display must never backpressure the DSP.
    ///
    /// Safe to call from the pipeline thread.  The only work done on the
    /// caller's thread is one allocation, one memcpy and a short lock; every
    /// socket call happens on the server thread.
    void broadcast(const u8* data, std::size_t n);

    /// Serve GET paths.  Register handlers for JSON endpoints.  The handler
    /// receives the raw query string (without the '?') and returns the
    /// response body; an empty return becomes a 404.  A body starting with
    /// '{' or '[' is served as application/json, anything else as text/plain.
    /// Handlers run on the server thread, so they must not block.
    void on_get(const std::string& path, std::function<std::string(const std::string& query)> h);

    /// Receive control messages from the browser (text frames, JSON).
    /// Runs on the server thread.
    void on_message(std::function<void(const std::string&)> h);

    //------------------------------------------------------------------
    // Counters
    //------------------------------------------------------------------
    int  clients() const;
    u64  bytes_sent() const;
    u64  dropped_frames() const;
    u64  messages_in() const;
    /// Broadcasts accepted, whether or not any client was listening.
    u64  frames_queued() const;

private:
    struct Impl;
    std::unique_ptr<Impl> p_;
};

/// The short page served when the real display file cannot be found.  It says
/// so, and says where to look, rather than showing a blank window.
const char* builtin_index_html();

/// Find soft/web/index.html.  Tries, in order: `web_root` if it is set (either
/// the file itself or a directory containing it), then locations relative to
/// the running executable, then locations relative to the working directory,
/// walking up towards a repository root.  Returns an empty string if nothing
/// is found, in which case the built-in page is served instead.
std::string resolve_index_path(const std::string& web_root);

/// Absolute path of the running executable's directory, or "" if it cannot be
/// determined.  Exposed because the daemon wants it for the same reason the
/// server does.
std::string executable_dir();

} // namespace radar
