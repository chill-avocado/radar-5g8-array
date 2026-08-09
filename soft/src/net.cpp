//============================================================================
// net.cpp -- HTTP + WebSocket, hand rolled on BSD sockets and poll()
//
// Layout of this file:
//   1. SHA-1 and base64        -- the two pieces of maths RFC 6455 needs
//   2. string and path helpers -- finding index.html without a build step
//   3. the built-in fallback page
//   4. WebSocket frame codec   -- build, parse, unmask
//   5. Client                  -- one connection's state
//   6. WebServer::Impl         -- the poll loop
//   7. the public surface
//
// The threading model is one sentence long: everything to do with a socket
// happens on the server thread, and nothing else does.  broadcast() hands a
// finished, already-framed buffer to a small staging vector under a lock held
// for a few hundred nanoseconds, then pokes a pipe.  It never touches a client
// and never makes a system call that can block, so the DSP thread cannot be
// slowed down by a browser on the end of a bad Wi-Fi link.
//============================================================================
#include "radar/net.hpp"
#include "radar/proto.hpp"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace radar {
namespace {

//============================================================================
// 1. SHA-1 and base64
//============================================================================

/// SHA-1, exactly as much of it as the WebSocket handshake needs.  The
/// handshake does not use it as a security primitive -- it is a fixed,
/// publicly known transformation whose only job is to prove that both ends
/// speak the protocol -- so there is no reason to drag in a crypto library.
struct Sha1 {
    u32 h[5]  = {0x67452301u, 0xEFCDAB89u, 0x98BADCFEu, 0x10325476u, 0xC3D2E1F0u};
    u64 total = 0;
    u8  buf[64]{};
    std::size_t n = 0;

    static u32 rol(u32 v, int s) { return (v << s) | (v >> (32 - s)); }

    void block(const u8* p) {
        u32 w[80];
        for (int i = 0; i < 16; ++i) {
            w[i] = (u32(p[i * 4]) << 24) | (u32(p[i * 4 + 1]) << 16) |
                   (u32(p[i * 4 + 2]) << 8) | u32(p[i * 4 + 3]);
        }
        for (int i = 16; i < 80; ++i) w[i] = rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);

        u32 a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
        for (int i = 0; i < 80; ++i) {
            u32 f, k;
            if (i < 20)      { f = (b & c) | (~b & d);          k = 0x5A827999u; }
            else if (i < 40) { f = b ^ c ^ d;                   k = 0x6ED9EBA1u; }
            else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDCu; }
            else             { f = b ^ c ^ d;                   k = 0xCA62C1D6u; }
            const u32 t = rol(a, 5) + f + e + k + w[i];
            e = d; d = c; c = rol(b, 30); b = a; a = t;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e;
    }

    void update(const void* data, std::size_t len) {
        const u8* p = static_cast<const u8*>(data);
        total += len;
        while (len) {
            const std::size_t take = std::min(len, std::size_t(64) - n);
            std::memcpy(buf + n, p, take);
            n += take; p += take; len -= take;
            if (n == 64) { block(buf); n = 0; }
        }
    }

    void finish(u8 out[20]) {
        const u64 bits = total * 8;
        u8 pad = 0x80;
        update(&pad, 1);
        pad = 0x00;
        while (n != 56) update(&pad, 1);
        u8 lenbe[8];
        for (int i = 0; i < 8; ++i) lenbe[i] = u8(bits >> ((7 - i) * 8));
        update(lenbe, 8);
        for (int i = 0; i < 5; ++i) {
            out[i * 4 + 0] = u8(h[i] >> 24);
            out[i * 4 + 1] = u8(h[i] >> 16);
            out[i * 4 + 2] = u8(h[i] >> 8);
            out[i * 4 + 3] = u8(h[i]);
        }
    }
};

std::string base64(const u8* data, std::size_t n) {
    static const char* kAlpha =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string o;
    o.reserve(((n + 2) / 3) * 4);
    std::size_t i = 0;
    for (; i + 3 <= n; i += 3) {
        const u32 v = (u32(data[i]) << 16) | (u32(data[i + 1]) << 8) | u32(data[i + 2]);
        o += kAlpha[(v >> 18) & 63];
        o += kAlpha[(v >> 12) & 63];
        o += kAlpha[(v >> 6) & 63];
        o += kAlpha[v & 63];
    }
    if (i + 1 == n) {
        const u32 v = u32(data[i]) << 16;
        o += kAlpha[(v >> 18) & 63];
        o += kAlpha[(v >> 12) & 63];
        o += "==";
    } else if (i + 2 == n) {
        const u32 v = (u32(data[i]) << 16) | (u32(data[i + 1]) << 8);
        o += kAlpha[(v >> 18) & 63];
        o += kAlpha[(v >> 12) & 63];
        o += kAlpha[(v >> 6) & 63];
        o += '=';
    }
    return o;
}

/// The fixed string RFC 6455 concatenates onto the client's key.
constexpr const char* kWsGuid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

std::string ws_accept_for(const std::string& key) {
    Sha1 s;
    s.update(key.data(), key.size());
    s.update(kWsGuid, std::strlen(kWsGuid));
    u8 digest[20];
    s.finish(digest);
    return base64(digest, 20);
}

//============================================================================
// 2. string and path helpers
//============================================================================

char lower_c(char c) { return (c >= 'A' && c <= 'Z') ? char(c - 'A' + 'a') : c; }

std::string lower(std::string s) {
    for (char& c : s) c = lower_c(c);
    return s;
}

bool iequal(const std::string& a, const char* b) {
    std::size_t i = 0;
    for (; i < a.size() && b[i]; ++i) if (lower_c(a[i]) != lower_c(b[i])) return false;
    return i == a.size() && b[i] == '\0';
}

bool icontains(const std::string& hay, const char* needle) {
    const std::string h = lower(hay);
    const std::string n = lower(needle);
    return h.find(n) != std::string::npos;
}

std::string trim(const std::string& s) {
    std::size_t a = 0, b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\t')) ++a;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r')) --b;
    return s.substr(a, b - a);
}

std::string url_decode(const std::string& s) {
    std::string o;
    o.reserve(s.size());
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '%' && i + 2 < s.size()) {
            auto hex = [](char c) -> int {
                if (c >= '0' && c <= '9') return c - '0';
                if (c >= 'a' && c <= 'f') return c - 'a' + 10;
                if (c >= 'A' && c <= 'F') return c - 'A' + 10;
                return -1;
            };
            const int hi = hex(s[i + 1]), lo = hex(s[i + 2]);
            if (hi >= 0 && lo >= 0) { o += char(hi * 16 + lo); i += 2; continue; }
        }
        o += s[i];
    }
    return o;
}

bool is_file(const std::string& p) {
    if (p.empty()) return false;
    struct stat st{};
    return ::stat(p.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}

std::string path_join(const std::string& a, const std::string& b) {
    if (a.empty()) return b;
    if (b.empty()) return a;
    if (a.back() == '/') return a + b;
    return a + "/" + b;
}

std::string parent_of(const std::string& p) {
    const std::size_t slash = p.find_last_of('/');
    if (slash == std::string::npos) return ".";
    if (slash == 0) return "/";
    return p.substr(0, slash);
}

bool ends_with(const std::string& s, const char* suffix) {
    const std::size_t n = std::strlen(suffix);
    return s.size() >= n && s.compare(s.size() - n, n, suffix) == 0;
}

/// Read a whole file.  Capped, because the page is served straight into a
/// socket buffer and a mistyped web_root should not be able to eat the heap.
bool read_file(const std::string& path, std::string& out, std::size_t cap = 16u << 20) {
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    out.clear();
    char buf[64 * 1024];
    while (true) {
        const std::size_t n = std::fread(buf, 1, sizeof(buf), f);
        if (n) out.append(buf, n);
        if (n < sizeof(buf)) break;
        if (out.size() > cap) { std::fclose(f); return false; }
    }
    std::fclose(f);
    return true;
}

/// Every place the display file might plausibly live, in the order to try.
std::vector<std::string> index_candidates(const std::string& web_root) {
    std::vector<std::string> c;
    if (!web_root.empty()) {
        if (ends_with(lower(web_root), ".html")) c.push_back(web_root);
        c.push_back(path_join(web_root, "index.html"));
        c.push_back(path_join(web_root, "web/index.html"));
        c.push_back(path_join(web_root, "soft/web/index.html"));
    }
    const std::string exe = executable_dir();
    if (!exe.empty()) {
        std::string d = exe;
        for (int up = 0; up < 4; ++up) {
            c.push_back(path_join(d, "web/index.html"));
            c.push_back(path_join(d, "soft/web/index.html"));
            c.push_back(path_join(d, "share/radar/web/index.html"));
            d = parent_of(d);
            if (d == "/" || d == ".") break;
        }
    }
    char cwdbuf[4096];
    if (::getcwd(cwdbuf, sizeof(cwdbuf))) {
        std::string d = cwdbuf;
        for (int up = 0; up < 6; ++up) {
            c.push_back(path_join(d, "soft/web/index.html"));
            c.push_back(path_join(d, "web/index.html"));
            d = parent_of(d);
            if (d == "/" || d == ".") break;
        }
    }
    return c;
}

//============================================================================
// 3. the built-in fallback page
//
// Deliberately tiny.  The real display is soft/web/index.html and is served
// from disk; this exists so that a daemon started from an unexpected working
// directory says what is wrong instead of showing a white rectangle.
//============================================================================
constexpr const char* kFallbackPage = R"HTML(<!doctype html>
<html><head><meta charset="utf-8"><title>radar - display file not found</title>
<style>
  html,body{margin:0;height:100%;background:#0b0e13;color:#c8d2e0;
    font:14px/1.6 ui-monospace,SFMono-Regular,Menlo,Consolas,monospace}
  .wrap{max-width:760px;margin:0 auto;padding:56px 24px}
  h1{font-size:18px;letter-spacing:.14em;text-transform:uppercase;color:#7fd4ff;margin:0 0 4px}
  .sub{color:#66748a;margin:0 0 28px}
  .card{border:1px solid #1d2634;background:#111621;border-radius:6px;padding:18px 20px;margin:0 0 16px}
  code{color:#ffd479}
  ul{margin:8px 0 0;padding-left:20px}
  li{color:#8695a8}
  .ok{color:#57d9a3}
</style></head><body><div class="wrap">
<h1>Radar display file not found</h1>
<p class="sub">The server is running. The operator display page is not where it expected it.</p>
<div class="card">
  <p>The full user interface lives in <code>soft/web/index.html</code> in the radar
  repository. The server looks for it next to the executable and then upwards
  towards the repository root.</p>
  <p>Point it straight at the file by setting <code>Config::web_root</code> to the
  directory that holds it, or start the daemon from the repository root.</p>
</div>
<div class="card">
  <p class="ok">The data link itself is fine.</p>
  <p>The WebSocket endpoint and the JSON status endpoint are both live on this
  port: <code>GET /status</code> returns the current configuration, health
  counters and track list.</p>
</div>
<div class="card" id="searched"><p>Paths searched:</p></div>
</div></body></html>
)HTML";

//============================================================================
// 4. WebSocket frame codec
//============================================================================

constexpr u8 kOpCont   = 0x0;
constexpr u8 kOpText   = 0x1;
constexpr u8 kOpBinary = 0x2;
constexpr u8 kOpClose  = 0x8;
constexpr u8 kOpPing   = 0x9;
constexpr u8 kOpPong   = 0xA;

/// Largest message accepted from a client.  Control traffic from a browser is
/// a few hundred bytes; anything near this is either a bug or an attack.
constexpr std::size_t kMaxInboundMessage = 1u << 20;

using Buf    = std::vector<u8>;
using BufPtr = std::shared_ptr<const Buf>;

/// Server-to-client frames are never masked (RFC 6455 5.1).  Built once and
/// shared by every client, which is why broadcast() costs one copy no matter
/// how many browsers are watching.
BufPtr make_frame(u8 opcode, const u8* data, std::size_t n) {
    auto v = std::make_shared<Buf>();
    v->reserve(n + 10);
    v->push_back(u8(0x80 | opcode));
    if (n < 126) {
        v->push_back(u8(n));
    } else if (n <= 0xFFFF) {
        v->push_back(126);
        v->push_back(u8(n >> 8));
        v->push_back(u8(n));
    } else {
        v->push_back(127);
        const u64 w = n;
        for (int i = 7; i >= 0; --i) v->push_back(u8(w >> (i * 8)));
    }
    if (n) v->insert(v->end(), data, data + n);
    return v;
}

BufPtr make_close_frame(u16 code, const char* reason) {
    u8 p[125];
    p[0] = u8(code >> 8);
    p[1] = u8(code);
    std::size_t n = 2;
    if (reason) {
        const std::size_t r = std::min(std::strlen(reason), std::size_t(120));
        std::memcpy(p + 2, reason, r);
        n += r;
    }
    return make_frame(kOpClose, p, n);
}

u64 now_ms() {
    using namespace std::chrono;
    return u64(duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}

u64 now_ns() {
    using namespace std::chrono;
    return u64(duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count());
}

#ifdef MSG_NOSIGNAL
constexpr int kSendFlags = MSG_NOSIGNAL;
#else
constexpr int kSendFlags = 0;
#endif

void set_nonblocking(int fd) {
    const int fl = ::fcntl(fd, F_GETFL, 0);
    if (fl >= 0) ::fcntl(fd, F_SETFL, fl | O_NONBLOCK);
}

void set_no_sigpipe(int fd) {
#ifdef SO_NOSIGPIPE
    int on = 1;
    ::setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &on, sizeof(on));
#else
    (void)fd;
#endif
}

//============================================================================
// 5. Client
//============================================================================

struct OutBuf {
    BufPtr buf;
    bool   droppable = true;   ///< false for handshakes, pongs and closes
};

struct Client {
    int  fd   = -1;
    u64  id   = 0;
    bool ws   = false;         ///< handshake completed
    bool dead = false;         ///< reap after this pass
    bool want_close = false;   ///< close the socket once the queue drains
    bool close_sent = false;

    std::vector<u8>    in;
    std::size_t        in_pos = 0;
    std::deque<OutBuf> out;
    std::size_t        out_off   = 0;   ///< bytes of out.front() already written
    std::size_t        out_bytes = 0;   ///< bytes still queued for this client

    u8              frag_op = 0;        ///< 0 = not in a fragmented message
    std::vector<u8> frag;

    u64 last_rx_ms   = 0;
    u64 last_ping_ms = 0;
};

} // namespace

//============================================================================
// 6. WebServer::Impl
//============================================================================

struct WebServer::Impl {
    // ---- configuration -------------------------------------------------
    int         port         = 0;
    std::string web_root;
    bool        allow_remote = false;
    std::string build_id     = "radar";
    std::size_t max_frames   = 64;
    std::size_t max_bytes    = 16u << 20;

    // ---- sockets and thread ---------------------------------------------
    int  listen_fd = -1;
    int  wake_r    = -1;
    int  wake_w    = -1;
    std::thread     thread;
    std::atomic<bool> quit{false};
    std::atomic<bool> up{false};

    // ---- owned by the server thread only ---------------------------------
    std::vector<std::unique_ptr<Client>> clients_list;
    u64 next_client_id = 1;
    std::string index_path;          ///< resolved once at start()
    std::vector<std::string> searched;

    // ---- shared with callers ----------------------------------------------
    std::mutex          tx_mu;
    std::vector<BufPtr> tx_pending;
    std::size_t         tx_pending_bytes = 0;

    std::mutex handlers_mu;
    std::map<std::string, std::function<std::string(const std::string&)>> handlers;
    std::function<void(const std::string&)> on_text;

    std::atomic<int> n_clients{0};
    std::atomic<u64> bytes_out{0};
    std::atomic<u64> dropped{0};
    std::atomic<u64> msgs_in{0};
    std::atomic<u64> queued{0};

    u64 t0_ns  = 0;
    u64 t0_ms  = 0;

    //------------------------------------------------------------------
    // Setup
    //------------------------------------------------------------------
    bool open_listener(std::string& err) {
        listen_fd = ::socket(AF_INET, SOCK_STREAM, 0);
        if (listen_fd < 0) { err = "could not create a socket: " + std::string(std::strerror(errno)); return false; }

        int on = 1;
        ::setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
        set_no_sigpipe(listen_fd);
        set_nonblocking(listen_fd);

        sockaddr_in a{};
        a.sin_family = AF_INET;
        a.sin_port   = htons(u16(port));
        // Loopback unless somebody has deliberately asked otherwise.  This is
        // a defence R&D platform: the display shows the live picture and can
        // change the operating point, so reaching it from another machine is
        // a decision that has to be typed out.
        a.sin_addr.s_addr = allow_remote ? htonl(INADDR_ANY) : htonl(INADDR_LOOPBACK);

        if (::bind(listen_fd, reinterpret_cast<sockaddr*>(&a), sizeof(a)) != 0) {
            const int e = errno;
            err = "could not bind port " + std::to_string(port) + ": ";
            err += (e == EADDRINUSE) ? "something else is already using it"
                                     : std::strerror(e);
            ::close(listen_fd); listen_fd = -1;
            return false;
        }
        if (::listen(listen_fd, 16) != 0) {
            err = "listen failed: " + std::string(std::strerror(errno));
            ::close(listen_fd); listen_fd = -1;
            return false;
        }
        if (port == 0) {
            sockaddr_in got{};
            socklen_t   len = sizeof(got);
            if (::getsockname(listen_fd, reinterpret_cast<sockaddr*>(&got), &len) == 0) {
                port = ntohs(got.sin_port);
            }
        }
        return true;
    }

    bool open_wake(std::string& err) {
        int fds[2];
        if (::pipe(fds) != 0) {
            err = "could not create the wake pipe: " + std::string(std::strerror(errno));
            return false;
        }
        wake_r = fds[0];
        wake_w = fds[1];
        set_nonblocking(wake_r);
        set_nonblocking(wake_w);   // so a full pipe can never stall the DSP thread
        return true;
    }

    void poke() {
        const u8 b = 1;
        ssize_t rc;
        do { rc = ::write(wake_w, &b, 1); } while (rc < 0 && errno == EINTR);
        (void)rc;   // EAGAIN means the pipe is already full, which is already a wake
    }

    void drain_wake() {
        u8 buf[256];
        while (::read(wake_r, buf, sizeof(buf)) > 0) {}
    }

    //------------------------------------------------------------------
    // Queueing
    //------------------------------------------------------------------
    void queue(Client& c, BufPtr b, bool droppable) {
        if (!b || c.dead) return;
        c.out.push_back(OutBuf{b, droppable});
        c.out_bytes += b->size();
    }

    void queue_frame(Client& c, u8 opcode, const u8* p, std::size_t n, bool droppable) {
        queue(c, make_frame(opcode, p, n), droppable);
    }

    void queue_raw(Client& c, const std::string& s) {
        auto v = std::make_shared<Buf>(s.begin(), s.end());
        queue(c, v, false);
    }

    /// A client that has fallen behind keeps the frame it is halfway through
    /// sending (dropping that one would corrupt the stream) and the newest
    /// frame, and loses everything in between.  Control traffic is never
    /// dropped: a pong or a close is a handful of bytes and losing one breaks
    /// the connection rather than the picture.
    void trim(Client& c) {
        if (c.out.size() <= max_frames && c.out_bytes <= max_bytes) return;

        std::deque<OutBuf> keep;
        std::size_t        bytes = 0;
        std::size_t        binned = 0;

        // The head, if it is partly written, has to stay exactly where it is.
        std::size_t start = 0;
        if (c.out_off > 0 && !c.out.empty()) {
            keep.push_back(c.out.front());
            bytes += c.out.front().buf->size() - c.out_off;
            start = 1;
        }
        // Find the newest droppable frame; keep it and every control frame.
        std::size_t newest = c.out.size();
        for (std::size_t i = c.out.size(); i-- > start;) {
            if (c.out[i].droppable) { newest = i; break; }
        }
        for (std::size_t i = start; i < c.out.size(); ++i) {
            const OutBuf& ob = c.out[i];
            if (!ob.droppable || i == newest) {
                keep.push_back(ob);
                bytes += ob.buf->size();
            } else {
                ++binned;
            }
        }
        if (!binned) return;
        c.out       = std::move(keep);
        c.out_bytes = bytes;
        dropped.fetch_add(binned, std::memory_order_relaxed);
    }

    //------------------------------------------------------------------
    // HTTP
    //------------------------------------------------------------------
    std::string http_response(int code, const char* reason, const std::string& ctype,
                              const std::string& body) {
        std::string h = "HTTP/1.1 " + std::to_string(code) + " " + reason + "\r\n";
        h += "Content-Type: " + ctype + "\r\n";
        h += "Content-Length: " + std::to_string(body.size()) + "\r\n";
        h += "Cache-Control: no-store\r\n";
        h += "X-Content-Type-Options: nosniff\r\n";
        h += "Connection: close\r\n\r\n";
        h += body;
        return h;
    }

    void respond(Client& c, int code, const char* reason, const std::string& ctype,
                 const std::string& body) {
        queue_raw(c, http_response(code, reason, ctype, body));
        c.want_close = true;
    }

    std::string default_status_json() const {
        const double up_s = double(now_ms() - t0_ms) / 1000.0;
        std::string o = "{";
        o += "\"server\":\"radar-net\",";
        o += "\"build\":\"" + proto::json_escape(build_id) + "\",";
        o += "\"protocol\":" + std::to_string(proto::kVersion) + ",";
        o += "\"port\":" + std::to_string(port) + ",";
        o += "\"bind\":\"" + std::string(allow_remote ? "0.0.0.0" : "127.0.0.1") + "\",";
        o += "\"uptime_s\":" + proto::json_num(up_s, 6) + ",";
        o += "\"clients\":" + std::to_string(n_clients.load()) + ",";
        o += "\"bytes_sent\":" + std::to_string(bytes_out.load()) + ",";
        o += "\"frames_queued\":" + std::to_string(queued.load()) + ",";
        o += "\"dropped_frames\":" + std::to_string(dropped.load()) + ",";
        o += "\"messages_in\":" + std::to_string(msgs_in.load()) + ",";
        o += "\"display\":" + (index_path.empty()
                                 ? std::string("null")
                                 : ("\"" + proto::json_escape(index_path) + "\""));
        o += "}";
        return o;
    }

    void serve_index(Client& c) {
        std::string body;
        if (!index_path.empty() && read_file(index_path, body)) {
            // Re-read every request on purpose: refreshing the browser picks
            // up an edited display without restarting the radar.
            respond(c, 200, "OK", "text/html; charset=utf-8", body);
            return;
        }
        std::string page = kFallbackPage;
        std::string list = "<ul>";
        for (const std::string& p : searched) list += "<li>" + proto::json_escape(p) + "</li>";
        list += "</ul>";
        const std::string marker = "<p>Paths searched:</p>";
        const std::size_t at = page.find(marker);
        if (at != std::string::npos) page.insert(at + marker.size(), list);
        respond(c, 200, "OK", "text/html; charset=utf-8", page);
    }

    /// Parse one complete request out of c.in.  Returns false if the client
    /// should be dropped.
    bool handle_http(Client& c) {
        const u8*   base = c.in.data() + c.in_pos;
        const std::size_t avail = c.in.size() - c.in_pos;

        // Find the end of the header block.
        std::size_t end = std::string::npos;
        for (std::size_t i = 0; i + 3 < avail; ++i) {
            if (base[i] == '\r' && base[i + 1] == '\n' && base[i + 2] == '\r' && base[i + 3] == '\n') {
                end = i + 4;
                break;
            }
        }
        if (end == std::string::npos) {
            if (avail > 16384) {
                respond(c, 431, "Request Header Fields Too Large", "text/plain",
                        "header block too large\n");
                return true;
            }
            return true;   // wait for more
        }

        const std::string block(reinterpret_cast<const char*>(base), end);
        c.in_pos += end;

        // ---- request line ----
        const std::size_t eol = block.find("\r\n");
        const std::string line = block.substr(0, eol);
        const std::size_t sp1 = line.find(' ');
        const std::size_t sp2 = (sp1 == std::string::npos) ? std::string::npos : line.find(' ', sp1 + 1);
        if (sp1 == std::string::npos || sp2 == std::string::npos || sp1 == 0) {
            respond(c, 400, "Bad Request", "text/plain", "malformed request line\n");
            return true;
        }
        const std::string method = line.substr(0, sp1);
        const std::string target = line.substr(sp1 + 1, sp2 - sp1 - 1);
        const std::string ver    = line.substr(sp2 + 1);
        if (target.empty() || target[0] != '/' || ver.compare(0, 5, "HTTP/") != 0) {
            respond(c, 400, "Bad Request", "text/plain", "malformed request line\n");
            return true;
        }

        // ---- headers ----
        std::map<std::string, std::string> hdr;
        std::size_t pos = eol + 2;
        while (pos < block.size()) {
            const std::size_t e = block.find("\r\n", pos);
            if (e == std::string::npos || e == pos) break;
            const std::string h = block.substr(pos, e - pos);
            const std::size_t colon = h.find(':');
            if (colon == std::string::npos) {
                respond(c, 400, "Bad Request", "text/plain", "malformed header\n");
                return true;
            }
            hdr[lower(trim(h.substr(0, colon)))] = trim(h.substr(colon + 1));
            pos = e + 2;
        }

        // ---- WebSocket upgrade ----
        const bool wants_ws = icontains(hdr["upgrade"], "websocket") &&
                              icontains(hdr["connection"], "upgrade");
        if (wants_ws) {
            const std::string key = hdr["sec-websocket-key"];
            if (!iequal(method, "GET") || key.empty() || hdr["sec-websocket-version"] != "13") {
                std::string body = "websocket upgrade requires GET, version 13 and a key\n";
                queue_raw(c, "HTTP/1.1 400 Bad Request\r\nSec-WebSocket-Version: 13\r\n"
                             "Content-Type: text/plain\r\nContent-Length: " +
                             std::to_string(body.size()) + "\r\nConnection: close\r\n\r\n" + body);
                c.want_close = true;
                return true;
            }
            std::string resp = "HTTP/1.1 101 Switching Protocols\r\n";
            resp += "Upgrade: websocket\r\n";
            resp += "Connection: Upgrade\r\n";
            resp += "Sec-WebSocket-Accept: " + ws_accept_for(key) + "\r\n\r\n";
            queue_raw(c, resp);
            c.ws = true;
            c.last_rx_ms = now_ms();

            // Say hello straight away so the page can show a live connection
            // before the first frame of radar data arrives.
            std::vector<u8> hello;
            proto::encode_hello(hello, t0_ns, now_ns(), build_id);
            queue_frame(c, kOpBinary, hello.data(), hello.size(), false);
            return true;
        }

        if (!iequal(method, "GET")) {
            respond(c, 405, "Method Not Allowed", "text/plain", "only GET is served here\n");
            return true;
        }

        // ---- route ----
        std::string path = target, query;
        const std::size_t q = target.find('?');
        if (q != std::string::npos) {
            path  = target.substr(0, q);
            query = target.substr(q + 1);
        }
        path = url_decode(path);

        std::function<std::string(const std::string&)> h;
        {
            std::lock_guard<std::mutex> lk(handlers_mu);
            auto it = handlers.find(path);
            if (it != handlers.end()) h = it->second;
        }
        if (h) {
            std::string body = h(query);
            if (body.empty()) {
                respond(c, 404, "Not Found", "text/plain", "no data\n");
            } else {
                const char f = body[0];
                const std::string ct = (f == '{' || f == '[') ? "application/json"
                                                              : "text/plain; charset=utf-8";
                respond(c, 200, "OK", ct, body);
            }
            return true;
        }

        // The only file this server will ever read off disk is the display
        // page itself, resolved once at start().  There is no path joining
        // against anything a client sends, so there is nothing to traverse.
        if (path == "/" || path == "/index.html") { serve_index(c); return true; }
        if (path == "/favicon.ico") {
            queue_raw(c, "HTTP/1.1 204 No Content\r\nConnection: close\r\n\r\n");
            c.want_close = true;
            return true;
        }
        respond(c, 404, "Not Found", "text/plain", "not found\n");
        return true;
    }

    //------------------------------------------------------------------
    // WebSocket
    //------------------------------------------------------------------
    void fail_ws(Client& c, u16 code, const char* why) {
        if (!c.close_sent) {
            queue(c, make_close_frame(code, why), false);
            c.close_sent = true;
        }
        c.want_close = true;
    }

    void deliver(Client& c, u8 op, const u8* p, std::size_t n) {
        if (op == kOpText) {
            msgs_in.fetch_add(1, std::memory_order_relaxed);
            std::function<void(const std::string&)> h;
            {
                std::lock_guard<std::mutex> lk(handlers_mu);
                h = on_text;
            }
            if (h) h(std::string(reinterpret_cast<const char*>(p), n));
        } else {
            // The display has no reason to send binary; count it and move on.
            msgs_in.fetch_add(1, std::memory_order_relaxed);
        }
        (void)c;
    }

    void handle_ws(Client& c) {
        while (!c.dead) {
            const std::size_t avail = c.in.size() - c.in_pos;
            if (avail < 2) return;
            const u8* p = c.in.data() + c.in_pos;

            const bool fin    = (p[0] & 0x80) != 0;
            const u8   rsv    = u8(p[0] & 0x70);
            const u8   op     = u8(p[0] & 0x0F);
            const bool masked = (p[1] & 0x80) != 0;
            u64        len    = u64(p[1] & 0x7F);
            std::size_t off   = 2;

            if (rsv) { fail_ws(c, 1002, "reserved bits set"); return; }

            if (len == 126) {
                if (avail < 4) return;
                len = (u64(p[2]) << 8) | u64(p[3]);
                off = 4;
            } else if (len == 127) {
                if (avail < 10) return;
                len = 0;
                for (int i = 0; i < 8; ++i) len = (len << 8) | u64(p[2 + i]);
                off = 10;
                if (len >> 63) { fail_ws(c, 1002, "length has the top bit set"); return; }
            }

            // Refuse an oversized message before buffering a byte of it.
            if (len > kMaxInboundMessage || c.frag.size() + len > kMaxInboundMessage) {
                fail_ws(c, 1009, "message too large");
                return;
            }
            // Every frame from a client must be masked.
            if (!masked) { fail_ws(c, 1002, "client frame was not masked"); return; }
            if (avail < off + 4) return;
            const u8 mask[4] = {p[off], p[off + 1], p[off + 2], p[off + 3]};
            off += 4;

            if (avail < off + len) return;   // payload still arriving

            const bool control = (op & 0x8) != 0;
            if (control && (!fin || len > 125)) {
                fail_ws(c, 1002, "fragmented or oversized control frame");
                return;
            }

            std::vector<u8> payload(std::size_t(len));
            for (std::size_t i = 0; i < payload.size(); ++i) payload[i] = u8(p[off + i] ^ mask[i & 3]);
            c.in_pos += off + std::size_t(len);

            switch (op) {
                case kOpCont: {
                    if (!c.frag_op) { fail_ws(c, 1002, "continuation with nothing to continue"); return; }
                    c.frag.insert(c.frag.end(), payload.begin(), payload.end());
                    if (fin) {
                        deliver(c, c.frag_op, c.frag.data(), c.frag.size());
                        c.frag.clear();
                        c.frag_op = 0;
                    }
                    break;
                }
                case kOpText:
                case kOpBinary: {
                    if (c.frag_op) { fail_ws(c, 1002, "new message inside a fragmented one"); return; }
                    if (fin) {
                        deliver(c, op, payload.data(), payload.size());
                    } else {
                        c.frag_op = op;
                        c.frag    = std::move(payload);
                    }
                    break;
                }
                case kOpClose: {
                    u16 code = 1000;
                    if (payload.size() >= 2) code = u16((u16(payload[0]) << 8) | payload[1]);
                    if (payload.size() == 1) { fail_ws(c, 1002, "malformed close payload"); return; }
                    if (!c.close_sent) {
                        // Echo the peer's status back, which is what the
                        // closing handshake asks for.
                        const u16 echo = (code >= 1000 && code <= 4999 && code != 1005 &&
                                          code != 1006 && code != 1015) ? code : u16(1000);
                        queue(c, make_close_frame(echo, nullptr), false);
                        c.close_sent = true;
                    }
                    c.want_close = true;
                    return;
                }
                case kOpPing: {
                    queue_frame(c, kOpPong, payload.data(), payload.size(), false);
                    break;
                }
                case kOpPong:
                    break;
                default:
                    fail_ws(c, 1002, "unknown opcode");
                    return;
            }
        }
    }

    //------------------------------------------------------------------
    // Socket pumping
    //------------------------------------------------------------------
    void compact(Client& c) {
        if (c.in_pos == 0) return;
        if (c.in_pos == c.in.size()) { c.in.clear(); c.in_pos = 0; return; }
        if (c.in_pos > 65536) {
            c.in.erase(c.in.begin(), c.in.begin() + std::ptrdiff_t(c.in_pos));
            c.in_pos = 0;
        }
    }

    void do_read(Client& c) {
        u8 buf[64 * 1024];
        std::size_t total = 0;
        while (!c.dead) {
            const ssize_t n = ::recv(c.fd, buf, sizeof(buf), 0);
            if (n > 0) {
                c.in.insert(c.in.end(), buf, buf + n);
                c.last_rx_ms = now_ms();
                total += std::size_t(n);
                if (c.ws) handle_ws(c); else { handle_http(c); if (c.ws) handle_ws(c); }
                compact(c);
                // Don't let one busy client monopolise the loop.
                if (total > (4u << 20)) break;
                if (std::size_t(n) < sizeof(buf)) break;
            } else if (n == 0) {
                c.dead = true;                 // peer closed
                return;
            } else if (errno == EINTR) {
                continue;
            } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            } else {
                c.dead = true;
                return;
            }
        }
    }

    void do_write(Client& c) {
        while (!c.out.empty() && !c.dead) {
            const Buf& b = *c.out.front().buf;
            const ssize_t n = ::send(c.fd, b.data() + c.out_off, b.size() - c.out_off, kSendFlags);
            if (n > 0) {
                c.out_off   += std::size_t(n);
                c.out_bytes -= std::size_t(n);
                bytes_out.fetch_add(u64(n), std::memory_order_relaxed);
                if (c.out_off >= b.size()) { c.out.pop_front(); c.out_off = 0; }
            } else if (n < 0 && errno == EINTR) {
                continue;
            } else if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                break;
            } else {
                c.dead = true;
                return;
            }
        }
        if (c.out.empty() && c.want_close) c.dead = true;
    }

    void do_accept() {
        while (true) {
            sockaddr_in peer{};
            socklen_t   len = sizeof(peer);
            const int fd = ::accept(listen_fd, reinterpret_cast<sockaddr*>(&peer), &len);
            if (fd < 0) {
                if (errno == EINTR) continue;
                return;             // EAGAIN: no more waiting connections
            }
            set_nonblocking(fd);
            set_no_sigpipe(fd);
            int on = 1;
            ::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on));

            if (clients_list.size() >= 32) {
                const char* busy = "HTTP/1.1 503 Service Unavailable\r\nConnection: close\r\n"
                                   "Content-Length: 18\r\n\r\ntoo many clients\r\n";
                (void)::send(fd, busy, std::strlen(busy), kSendFlags);
                ::close(fd);
                continue;
            }
            auto c = std::unique_ptr<Client>(new Client());
            c->fd = fd;
            c->id = next_client_id++;
            c->last_rx_ms = now_ms();
            clients_list.push_back(std::move(c));
            n_clients.store(int(clients_list.size()), std::memory_order_relaxed);
        }
    }

    void distribute() {
        std::vector<BufPtr> batch;
        {
            std::lock_guard<std::mutex> lk(tx_mu);
            if (tx_pending.empty()) return;
            batch.swap(tx_pending);
            tx_pending_bytes = 0;
        }
        for (auto& up_c : clients_list) {
            Client& c = *up_c;
            if (!c.ws || c.dead || c.want_close) continue;
            for (const BufPtr& b : batch) queue(c, b, true);
            trim(c);
        }
    }

    void keepalive() {
        const u64 t = now_ms();
        for (auto& up_c : clients_list) {
            Client& c = *up_c;
            if (!c.ws || c.dead || c.want_close) continue;
            if (t - c.last_rx_ms > 60000) { c.dead = true; continue; }
            if (t - c.last_ping_ms > 20000) {
                c.last_ping_ms = t;
                queue_frame(c, kOpPing, nullptr, 0, false);
            }
        }
    }

    void reap() {
        bool any = false;
        for (auto it = clients_list.begin(); it != clients_list.end();) {
            if ((*it)->dead) {
                ::close((*it)->fd);
                it  = clients_list.erase(it);
                any = true;
            } else {
                ++it;
            }
        }
        if (any) n_clients.store(int(clients_list.size()), std::memory_order_relaxed);
    }

    void run() {
        std::vector<pollfd> pfds;
        while (!quit.load(std::memory_order_relaxed)) {
            pfds.clear();
            pfds.push_back(pollfd{listen_fd, POLLIN, 0});
            pfds.push_back(pollfd{wake_r, POLLIN, 0});
            for (auto& c : clients_list) {
                short ev = POLLIN;
                if (!c->out.empty()) ev = short(ev | POLLOUT);
                pfds.push_back(pollfd{c->fd, ev, 0});
            }
            const std::size_t n_watched = clients_list.size();

            const int rc = ::poll(pfds.data(), nfds_t(pfds.size()), 50);
            if (rc < 0) {
                if (errno == EINTR) continue;
                break;
            }
            if (quit.load(std::memory_order_relaxed)) break;

            if (pfds[1].revents & POLLIN) drain_wake();

            for (std::size_t i = 0; i < n_watched && i < clients_list.size(); ++i) {
                Client& c = *clients_list[i];
                const short re = pfds[i + 2].revents;
                if (re & (POLLERR | POLLNVAL)) { c.dead = true; continue; }
                if (re & POLLIN)  do_read(c);
                if (!c.dead && (re & POLLOUT)) do_write(c);
                if (!c.dead && (re & POLLHUP) && c.out.empty()) c.dead = true;
            }

            if (pfds[0].revents & POLLIN) do_accept();

            distribute();

            // Anything queued by distribute(), by a handshake or by a pong
            // goes out now rather than waiting for the next poll to say so.
            for (auto& c : clients_list) {
                if (!c->dead && !c->out.empty()) do_write(*c);
            }

            keepalive();
            reap();
        }

        for (auto& c : clients_list) ::close(c->fd);
        clients_list.clear();
        n_clients.store(0, std::memory_order_relaxed);
    }
};

//============================================================================
// 7. the public surface
//============================================================================

WebServer::WebServer(int port) : p_(new Impl()) {
    p_->port = port;
}

WebServer::WebServer(int port, std::string web_root, bool allow_remote) : p_(new Impl()) {
    p_->port         = port;
    p_->web_root     = std::move(web_root);
    p_->allow_remote = allow_remote;
}

WebServer::~WebServer() { stop(); }

void WebServer::set_web_root(std::string root)  { p_->web_root = std::move(root); }
void WebServer::set_allow_remote(bool yes)      { p_->allow_remote = yes; }
void WebServer::set_build_id(std::string id)    { p_->build_id = std::move(id); }

void WebServer::set_queue_limit(std::size_t frames, std::size_t bytes) {
    p_->max_frames = frames ? frames : 1;
    p_->max_bytes  = bytes  ? bytes  : (1u << 20);
}

bool WebServer::start(std::string& err) {
    if (p_->up.load()) { err = "already running"; return false; }
    err.clear();

    p_->t0_ns = now_ns();
    p_->t0_ms = now_ms();

    p_->searched   = index_candidates(p_->web_root);
    p_->index_path = resolve_index_path(p_->web_root);

    if (!p_->open_listener(err)) return false;
    if (!p_->open_wake(err)) {
        ::close(p_->listen_fd);
        p_->listen_fd = -1;
        return false;
    }

    // A default status endpoint, so a fresh server answers curl before the
    // daemon has registered anything of its own.  on_get("/status", ...)
    // replaces it.
    {
        std::lock_guard<std::mutex> lk(p_->handlers_mu);
        if (p_->handlers.find("/status") == p_->handlers.end()) {
            Impl* impl = p_.get();
            p_->handlers["/status"] = [impl](const std::string&) {
                return impl->default_status_json();
            };
        }
    }

    p_->quit.store(false);
    p_->up.store(true);
    p_->thread = std::thread([this] { p_->run(); });
    return true;
}

void WebServer::stop() {
    if (!p_ || !p_->up.exchange(false)) return;
    p_->quit.store(true);
    p_->poke();
    if (p_->thread.joinable()) p_->thread.join();
    if (p_->listen_fd >= 0) { ::close(p_->listen_fd); p_->listen_fd = -1; }
    if (p_->wake_r >= 0)    { ::close(p_->wake_r); p_->wake_r = -1; }
    if (p_->wake_w >= 0)    { ::close(p_->wake_w); p_->wake_w = -1; }
    {
        std::lock_guard<std::mutex> lk(p_->tx_mu);
        p_->tx_pending.clear();
        p_->tx_pending_bytes = 0;
    }
}

bool WebServer::running() const { return p_->up.load(); }
int  WebServer::port() const    { return p_->port; }

std::string WebServer::url() const {
    return std::string("http://") + (p_->allow_remote ? "0.0.0.0" : "127.0.0.1") + ":" +
           std::to_string(p_->port) + "/";
}

void WebServer::broadcast(const u8* data, std::size_t n) {
    if (!data || !n || !p_->up.load(std::memory_order_relaxed)) return;

    // Frame it once, here, on the caller's thread: this is a memcpy and an
    // allocation, both bounded, neither able to block.  Every client then
    // shares the same buffer.
    BufPtr frame = make_frame(kOpBinary, data, n);
    p_->queued.fetch_add(1, std::memory_order_relaxed);

    bool woke = false;
    {
        std::lock_guard<std::mutex> lk(p_->tx_mu);
        p_->tx_pending.push_back(frame);
        p_->tx_pending_bytes += frame->size();
        // If the server thread has not run for a while -- a machine under
        // load, a debugger stopped on a breakpoint -- the staging area is
        // trimmed to the newest frame by exactly the same rule the per-client
        // queues use.  The pipeline keeps its timing either way.
        const std::size_t cap_frames = p_->max_frames * 4;
        const std::size_t cap_bytes  = p_->max_bytes * 2;
        if (p_->tx_pending.size() > cap_frames || p_->tx_pending_bytes > cap_bytes) {
            const std::size_t binned = p_->tx_pending.size() - 1;
            BufPtr newest = p_->tx_pending.back();
            p_->tx_pending.clear();
            p_->tx_pending.push_back(newest);
            p_->tx_pending_bytes = newest->size();
            p_->dropped.fetch_add(binned, std::memory_order_relaxed);
        }
        woke = true;
    }
    if (woke) p_->poke();
}

void WebServer::on_get(const std::string& path,
                       std::function<std::string(const std::string&)> h) {
    std::lock_guard<std::mutex> lk(p_->handlers_mu);
    if (h) p_->handlers[path] = std::move(h);
    else   p_->handlers.erase(path);
}

void WebServer::on_message(std::function<void(const std::string&)> h) {
    std::lock_guard<std::mutex> lk(p_->handlers_mu);
    p_->on_text = std::move(h);
}

int WebServer::clients() const        { return p_->n_clients.load(std::memory_order_relaxed); }
u64 WebServer::bytes_sent() const     { return p_->bytes_out.load(std::memory_order_relaxed); }
u64 WebServer::dropped_frames() const { return p_->dropped.load(std::memory_order_relaxed); }
u64 WebServer::messages_in() const    { return p_->msgs_in.load(std::memory_order_relaxed); }
u64 WebServer::frames_queued() const  { return p_->queued.load(std::memory_order_relaxed); }

//----------------------------------------------------------------------------
// Path resolution and the fallback page
//----------------------------------------------------------------------------

const char* builtin_index_html() { return kFallbackPage; }

std::string executable_dir() {
#ifdef __APPLE__
    u32 size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::vector<char> buf(size + 1, 0);
    if (_NSGetExecutablePath(buf.data(), &size) != 0) return "";
    char real[4096];
    const char* r = ::realpath(buf.data(), real);
    return parent_of(r ? std::string(r) : std::string(buf.data()));
#else
    char buf[4096];
    const ssize_t n = ::readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n <= 0) return "";
    buf[n] = '\0';
    return parent_of(std::string(buf));
#endif
}

std::string resolve_index_path(const std::string& web_root) {
    for (const std::string& p : index_candidates(web_root)) {
        if (is_file(p)) {
            char real[4096];
            const char* r = ::realpath(p.c_str(), real);
            return r ? std::string(r) : p;
        }
    }
    return "";
}

} // namespace radar
