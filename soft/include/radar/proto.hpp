//============================================================================
// proto.hpp -- the wire format between the radar daemon and the display
//
// One binary protocol, little-endian, packed.  Every message is a fixed 28-byte
// header followed by a payload whose layout depends on the type.  The payloads
// are laid out so that a browser can walk them with a DataView and a few typed
// array views: no varints, no strings of unknown length in the middle of a
// record, no per-element branching.  Nothing on the hot path costs more than a
// memcpy on the sending side and an offset calculation on the receiving side.
//
// Why little-endian and packed: every machine this runs on is little-endian
// (x86-64 host, ARM64 host, and JavaScript's DataView is told the byte order
// explicitly), so the encoder is a struct copy and the decoder is a cast.  The
// #error below refuses to build anywhere that assumption breaks rather than
// silently producing garbage.
//
// Nothing here includes a socket header.  proto.cpp is pure data shuffling and
// can be linked into a test with no server present.
//============================================================================
#pragma once

#include <cstring>              // core.hpp uses memset; include before it
#include "radar/core.hpp"
#include "radar/config.hpp"
#include "radar/types.hpp"

#include <string>
#include <vector>

#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__)
#error "radar::proto assumes a little-endian host"
#endif

namespace radar {
namespace proto {

//----------------------------------------------------------------------------
// Framing
//----------------------------------------------------------------------------

/// "RADA" read as a little-endian u32 spells the bytes A D A R on the wire.
/// Present so a capture tool, or a client that has lost sync, can resynchronise
/// on a byte boundary instead of guessing.
constexpr u32 kMagic = 0x52414441;

/// Bumped whenever a payload layout changes in a way an old client would
/// misread.  Sent in Hello; a client that sees a version it does not know
/// should say so rather than draw nonsense.
constexpr u16 kVersion = 1;

enum class MsgType : u16 {
    Hello       = 1,
    RdMap       = 2,
    Hits        = 3,
    Tracks      = 4,
    Stats       = 5,
    Spectrogram = 6,
    Config      = 7,
};

/// Header::flags bits.  Small facts about the frame that the display needs
/// before it has parsed the payload.
namespace flag {
constexpr u16 kOverflow   = 1u << 0;  ///< the radio overran during this frame
constexpr u16 kCubeValid  = 1u << 1;  ///< full complex cube existed host-side
constexpr u16 kFrozen     = 1u << 2;  ///< daemon is holding the display still
constexpr u16 kEndOfFrame = 1u << 3;  ///< last message of this frame's bundle
}

#pragma pack(push, 1)

struct Header {
    u32 magic;   ///< kMagic
    u16 type;    ///< MsgType
    u16 flags;   ///< flag::*
    u64 frame;   ///< RdFrame::index this message belongs to
    u64 t_ns;    ///< host monotonic time of publication, nanoseconds
    u32 bytes;   ///< payload bytes that follow this header
};
static_assert(sizeof(Header) == 28, "Header must be 28 bytes on the wire");

constexpr std::size_t kHeaderBytes = sizeof(Header);

//----------------------------------------------------------------------------
// RdMap -- the range-Doppler power map, the single biggest thing on the link
//
// The map is quantised to 8 bits per cell between min_db and max_db.  This is
// deliberate, not a shortcut.  A 256 x 256 map at the default 31 frames per
// second is 65536 cells x 31 = 2.03 million cells per second: 2.0 MB/s as
// bytes, 8.1 MB/s as f32.  The display maps those cells through a colour ramp
// with 256 levels in it, so the four extra bytes per cell would be thrown away
// by the screen anyway.  Over a WebSocket on loopback that difference is the
// difference between a display that never touches the DSP and one that does.
// min_db/max_db travel with every map so the quantisation is self-describing
// and the operator can still read a true dB value off the crosshair.
//----------------------------------------------------------------------------
struct RdMapHead {
    u16 n_range;
    u16 n_doppler;
    f32 min_db;    ///< quantised value 0
    f32 max_db;    ///< quantised value 255
};
static_assert(sizeof(RdMapHead) == 12, "");

//----------------------------------------------------------------------------
// Hits -- the detection layer.  One message carries both the raw CFAR
// detections and the clusters formed from them, because the display draws them
// together and they are always produced together.
//   payload: u32 n_hits, u32 n_targets, WireHit[n_hits], WireTarget[n_targets]
//----------------------------------------------------------------------------
struct WireHit {
    u16 range_bin;
    i16 dopp_bin;        ///< signed, 0 is zero Doppler
    f32 range_m;
    f32 velocity_ms;     ///< positive = approaching
    f32 snr_db;
    f32 azimuth_deg;
    f32 elevation_deg;
    f32 angle_quality;   ///< peak to second peak, dB
    u8  angle_valid;
    u8  pad[3];
};
static_assert(sizeof(WireHit) == 32, "");

struct WireTarget {
    f32 range_m;
    f32 velocity_ms;
    f32 azimuth_deg;
    f32 elevation_deg;
    f32 snr_db;
    f32 x, y, z;         ///< metres, boresight = +y, right = +x
    f32 extent_m;
    f32 dopp_spread_ms;  ///< wide for anything with a rotor on it
    u16 n_hits;
    u16 pad;
};
static_assert(sizeof(WireTarget) == 44, "");

struct HitsHead {
    u32 n_hits;
    u32 n_targets;
};
static_assert(sizeof(HitsHead) == 8, "");

//----------------------------------------------------------------------------
// Tracks
//
// The 3x3 position block of the 6x6 filter covariance travels with every
// track, as its six unique entries: the matrix is symmetric, so sending nine
// would be sending three lies.  The display eigen-decomposes the (x, y)
// corner of it to draw the uncertainty ellipse, which is the only honest way
// to show an operator how much to trust a dot on a plan view.
//----------------------------------------------------------------------------
struct WireTrack {
    u32 id;
    f32 x, y, z;
    f32 vx, vy, vz;
    f32 pxx, pxy, pxz, pyy, pyz, pzz;   ///< position block of P, upper triangle
    f32 last_snr_db;
    f32 age_s;
    f32 micro_bw_hz;     ///< spectral width beyond the bulk motion
    f32 blade_hz;        ///< dominant periodic modulation, 0 if none found
    f32 label_conf;
    u16 hits;
    u16 misses;
    u16 spec_time;       ///< spectrogram rows available for this track
    u16 spec_freq;       ///< spectrogram columns
    u8  confirmed;
    u8  label_len;       ///< used bytes of label, 0..21
    char label[22];      ///< NUL padded, truncated if the classifier is wordy
};
static_assert(sizeof(WireTrack) == 104, "");

struct TracksHead {
    u32 n_tracks;
};
static_assert(sizeof(TracksHead) == 4, "");

//----------------------------------------------------------------------------
// Spectrogram -- micro-Doppler history for one track.
// Quantised to 8 bits for the same reason the range-Doppler map is.
//   payload: SpecHead, then n_time * n_freq bytes, row major, oldest row first
//----------------------------------------------------------------------------
struct SpecHead {
    u32 track_id;
    u16 n_time;
    u16 n_freq;
    f32 min_db;
    f32 max_db;
    f32 hz_per_bin;      ///< frequency axis scale
    f32 s_per_row;       ///< time axis scale
};
static_assert(sizeof(SpecHead) == 24, "");

//----------------------------------------------------------------------------
// Stats -- health.  Mirrors radar::Stats and adds what the link itself knows,
// because "the display is behind" and "the radar is behind" look identical to
// an operator unless you tell them apart.
//----------------------------------------------------------------------------
struct WireStats {
    u64 frames;
    u64 overflows;
    u64 dropped;
    u64 bytes_in;
    u64 bytes_sent;      ///< network: bytes written to all clients
    u64 net_dropped;     ///< network: frames binned because a client lagged
    f32 cpu_frac;
    f32 frame_rate_hz;
    f32 stage_ms[8];     ///< dechirp, range, corner, dopp, cfar, aoa, cluster, track
    u16 n_tracks;
    u16 clients;
    u16 source;          ///< SourceKind
    u16 flags;           ///< flag::kFrozen, flag::kOverflow
};
static_assert(sizeof(WireStats) == 96, "");

/// The network-side counters, handed to the encoder by whoever owns the server.
struct NetTally {
    u64 bytes_sent    = 0;
    u64 dropped_frames = 0;
    int clients       = 0;
};

//----------------------------------------------------------------------------
// Config -- everything the display needs to put real numbers on its axes and
// to show the operating point it is looking at.  Sent on connect and again
// whenever anything changes.
//----------------------------------------------------------------------------
struct WireConfig {
    f64 centre_freq_hz;
    f64 sample_rate_hz;
    f64 sweep_bw_hz;
    f64 pfa;
    f32 range_bin_m;
    f32 range_max_m;
    f32 range_res_m;
    f32 vel_res_ms;
    f32 vel_max_ms;      ///< unambiguous, +/-
    f32 frame_rate_hz;
    f32 t_cpi_s;
    f32 aoa_az_span_deg;
    f32 aoa_el_span_deg;
    f32 tx_gain_db;
    f32 rx_gain_db;
    u16 n_range;
    u16 n_doppler;
    u16 n_chirp;
    u16 max_hits;
    u16 zero_dopp_blank;
    u8  cfar_kind;       ///< CfarKind
    u8  aoa_method;      ///< AoaMethod
    u8  source;          ///< SourceKind
    u8  mimo;            ///< MimoMode
    u8  n_virt;
    u8  tx_enable;
};
static_assert(sizeof(WireConfig) == 92, "");

//----------------------------------------------------------------------------
// Hello -- first message on every connection.
//----------------------------------------------------------------------------
struct WireHello {
    u16 version;         ///< kVersion
    u16 flags;
    u32 reserved;
    u64 t0_ns;           ///< daemon start time, so the display can show uptime
    char build[32];      ///< build identifier, NUL padded
};
static_assert(sizeof(WireHello) == 48, "");

#pragma pack(pop)

//----------------------------------------------------------------------------
// Quantisation
//----------------------------------------------------------------------------

/// The dB window a map or spectrogram was quantised into.
struct DbRange {
    f32 min_db = 0;
    f32 max_db = 0;
};

/// Pick a sensible window for a power map: the peak, and `span_db` below it.
/// Guards against an all-zero map, which happens on the very first frame.
DbRange auto_range(const f32* power, std::size_t n, double span_db = 80.0);
DbRange auto_range(const RdFrame& f, double span_db = 80.0);

/// dB value -> 0..255.  Inlined because it runs 65536 times per frame.
inline u8 quantise(float db_value, float min_db, float inv_span) {
    const float t = (db_value - min_db) * inv_span;
    const float s = t <= 0.0f ? 0.0f : (t >= 1.0f ? 255.0f : t * 255.0f + 0.5f);
    return static_cast<u8>(s);
}

/// 0..255 -> dB.  The display and the tests both need to undo the quantisation.
inline float dequantise(u8 q, float min_db, float max_db) {
    return min_db + (max_db - min_db) * (static_cast<float>(q) * (1.0f / 255.0f));
}

//----------------------------------------------------------------------------
// Encoding.  Every one of these appends a complete message -- header and
// payload -- to `out`, and leaves anything already in `out` alone, so a whole
// frame's worth of messages can be built into one buffer and sent once.
//----------------------------------------------------------------------------

void encode_hello(std::vector<u8>& out, u64 t0_ns, u64 t_ns, const std::string& build);

/// The map, quantised into an explicit dB window.  Pass auto_range() unless
/// the operator has pinned the window by hand.
void encode_rdmap(std::vector<u8>& out, const RdFrame& f, u64 t_ns, DbRange r,
                  u16 extra_flags = 0);

/// Detections and the clusters made from them.
void encode_hits(std::vector<u8>& out, const RdFrame& f, u64 t_ns, u16 extra_flags = 0);

void encode_tracks(std::vector<u8>& out, const std::vector<Track>& tracks,
                   u64 frame, u64 t_ns, u16 extra_flags = 0);

/// One track's micro-Doppler history.  Does nothing if the track has no
/// spectrogram yet.  Returns true if a message was appended.
bool encode_spectrogram(std::vector<u8>& out, const Track& t, u64 frame, u64 t_ns,
                        f32 hz_per_bin, f32 s_per_row, DbRange r, u16 extra_flags = 0);

/// Same, choosing the dB window from the data.
bool encode_spectrogram(std::vector<u8>& out, const Track& t, u64 frame, u64 t_ns,
                        f32 hz_per_bin, f32 s_per_row, u16 extra_flags = 0);

void encode_stats(std::vector<u8>& out, const Stats& s, const NetTally& net,
                  SourceKind src, u64 frame, u64 t_ns, u16 extra_flags = 0);

void encode_config(std::vector<u8>& out, const Config& c, u64 frame, u64 t_ns,
                   u16 extra_flags = 0);

//----------------------------------------------------------------------------
// Decoding.  Used by the tests, by any C++ client, and by the replay tool.
// Every one of these checks the length before it reads, and returns false
// rather than trusting the sender.
//----------------------------------------------------------------------------

struct RdMapMsg {
    RdMapHead head{};
    std::vector<u8> q;     ///< n_range * n_doppler, range major
    float db_at(int r, int d) const {
        return dequantise(q[std::size_t(r) * head.n_doppler + d], head.min_db, head.max_db);
    }
};

struct HitsMsg {
    std::vector<WireHit>    hits;
    std::vector<WireTarget> targets;
};

struct TracksMsg {
    std::vector<WireTrack> tracks;
};

struct SpecMsg {
    SpecHead head{};
    std::vector<u8> q;
};

/// Read a header and locate the payload.  `payload` points into `msg`.
/// Returns false on a short buffer, a bad magic, or a length that runs past
/// the end of the buffer.
bool decode_header(const u8* msg, std::size_t n, Header& h,
                   const u8** payload, std::size_t* payload_bytes);

bool decode_hello(const u8* payload, std::size_t n, WireHello& out);
bool decode_rdmap(const u8* payload, std::size_t n, RdMapMsg& out);
bool decode_hits(const u8* payload, std::size_t n, HitsMsg& out);
bool decode_tracks(const u8* payload, std::size_t n, TracksMsg& out);
bool decode_spectrogram(const u8* payload, std::size_t n, SpecMsg& out);
bool decode_stats(const u8* payload, std::size_t n, WireStats& out);
bool decode_config(const u8* payload, std::size_t n, WireConfig& out);

/// Human-readable name, for logs and for the status endpoint.
const char* type_name(MsgType t);
const char* source_name(SourceKind s);
const char* cfar_name(CfarKind k);
const char* aoa_name(AoaMethod a);
const char* mimo_name(MimoMode m);

//----------------------------------------------------------------------------
// JSON.  A second, slower, human-readable view of the same state, served over
// plain HTTP so the radar can be checked with curl from a machine with no
// browser on it -- which is most of the machines this will run next to.
//----------------------------------------------------------------------------

std::string json_escape(const std::string& s);

/// A finite number, or `null`.  JSON has no NaN, and a status endpoint that
/// emits one is a status endpoint that breaks every parser pointed at it.
std::string json_num(double v, int sig = 6);

std::string json_stats(const Stats& s, const NetTally& net);
std::string json_config(const Config& c);
std::string json_tracks(const std::vector<Track>& tracks);

/// Everything at once: { "config": ..., "stats": ..., "tracks": [...] }
std::string json_status(const Config& c, const Stats& s, const NetTally& net,
                        const std::vector<Track>& tracks);

} // namespace proto
} // namespace radar
