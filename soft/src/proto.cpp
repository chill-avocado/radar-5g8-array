//============================================================================
// proto.cpp -- encoders, decoders and the JSON status view
//
// Everything here is straight-line data shuffling.  The only place that is
// remotely hot is the quantising loop in encode_rdmap(), which touches 65536
// cells per frame; it is written so the compiler can vectorise the arithmetic
// and so the only per-cell library call is log10f.
//============================================================================
#include "radar/proto.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <type_traits>

namespace radar {
namespace proto {
namespace {

//----------------------------------------------------------------------------
// Little helpers for appending POD to a byte vector.
//----------------------------------------------------------------------------
template <typename T>
void put(std::vector<u8>& out, const T& v) {
    static_assert(std::is_trivially_copyable<T>::value, "wire types must be POD");
    const auto at = out.size();
    out.resize(at + sizeof(T));
    std::memcpy(out.data() + at, &v, sizeof(T));
}

/// Write the header now, come back and fill in `bytes` once the payload has
/// been appended.  Avoids building the payload twice or into a scratch buffer.
struct HeaderPatch {
    std::vector<u8>* out;
    std::size_t      at;
    std::size_t      payload_start;
};

HeaderPatch begin_msg(std::vector<u8>& out, MsgType type, u16 flags, u64 frame, u64 t_ns) {
    HeaderPatch hp{&out, out.size(), 0};
    Header h{};
    h.magic = kMagic;
    h.type  = static_cast<u16>(type);
    h.flags = flags;
    h.frame = frame;
    h.t_ns  = t_ns;
    h.bytes = 0;
    put(out, h);
    hp.payload_start = out.size();
    return hp;
}

void end_msg(const HeaderPatch& hp) {
    const u32 n = static_cast<u32>(hp.out->size() - hp.payload_start);
    std::memcpy(hp.out->data() + hp.at + offsetof(Header, bytes), &n, sizeof(u32));
}

template <typename T>
bool take(const u8*& p, std::size_t& left, T& v) {
    if (left < sizeof(T)) return false;
    std::memcpy(&v, p, sizeof(T));
    p += sizeof(T);
    left -= sizeof(T);
    return true;
}

/// Copy a std::string into a fixed NUL-padded field, truncating cleanly.
void set_label(char* dst, std::size_t cap, u8& len_out, const std::string& s) {
    std::memset(dst, 0, cap);
    const std::size_t n = std::min(s.size(), cap - 1);
    if (n) std::memcpy(dst, s.data(), n);
    len_out = static_cast<u8>(n);
}

float safe_db(double linear) {
    // db() in core.hpp adds 1e-300 so log10 never sees a zero; keep that, and
    // additionally clamp anything non-finite so a single bad cell cannot make
    // the whole quantisation window collapse.
    const double v = 10.0 * std::log10(linear + 1e-300);
    if (!std::isfinite(v)) return -300.0f;
    return static_cast<float>(v);
}

} // namespace

//----------------------------------------------------------------------------
// Quantisation window
//----------------------------------------------------------------------------
DbRange auto_range(const f32* power, std::size_t n, double span_db) {
    DbRange r;
    float peak = 0.0f;
    for (std::size_t i = 0; i < n; ++i) {
        const float v = power[i];
        if (std::isfinite(v) && v > peak) peak = v;
    }
    const float top = (peak > 0.0f) ? safe_db(peak) : 0.0f;
    r.max_db = top;
    r.min_db = static_cast<f32>(top - (span_db > 1.0 ? span_db : 1.0));
    return r;
}

DbRange auto_range(const RdFrame& f, double span_db) {
    return auto_range(f.power.data(), f.power.size(), span_db);
}

//----------------------------------------------------------------------------
// Encoders
//----------------------------------------------------------------------------
void encode_hello(std::vector<u8>& out, u64 t0_ns, u64 t_ns, const std::string& build) {
    const auto hp = begin_msg(out, MsgType::Hello, 0, 0, t_ns);
    WireHello h{};
    h.version  = kVersion;
    h.flags    = 0;
    h.reserved = 0;
    h.t0_ns    = t0_ns;
    const std::size_t n = std::min(build.size(), sizeof(h.build) - 1);
    if (n) std::memcpy(h.build, build.data(), n);
    put(out, h);
    end_msg(hp);
}

void encode_rdmap(std::vector<u8>& out, const RdFrame& f, u64 t_ns, DbRange r,
                  u16 extra_flags) {
    u16 flags = extra_flags;
    if (f.overflow)   flags |= flag::kOverflow;
    if (f.cube_valid) flags |= flag::kCubeValid;

    const auto hp = begin_msg(out, MsgType::RdMap, flags, f.index, t_ns);

    RdMapHead head{};
    head.n_range   = static_cast<u16>(f.n_range);
    head.n_doppler = static_cast<u16>(f.n_doppler);
    head.min_db    = r.min_db;
    head.max_db    = r.max_db;
    // A collapsed window would divide by zero below and would also be useless
    // to look at, so widen it to something an operator can read.
    if (!(head.max_db > head.min_db)) head.max_db = head.min_db + 1.0f;
    put(out, head);

    const std::size_t n     = std::size_t(f.n_range) * std::size_t(f.n_doppler);
    const std::size_t have  = std::min(n, f.power.size());
    const float       lo    = head.min_db;
    const float       inv   = 1.0f / (head.max_db - head.min_db);

    const std::size_t at = out.size();
    out.resize(at + n);
    u8* q = out.data() + at;

    const float* p = f.power.data();
    for (std::size_t i = 0; i < have; ++i) {
        // 10*log10 of the linear power, then straight into the 8-bit window.
        const float v  = p[i];
        const float dbv = 10.0f * std::log10((v > 0.0f ? v : 0.0f) + 1e-30f);
        q[i] = quantise(dbv, lo, inv);
    }
    // A frame whose power buffer is short (never happens in the pipeline, but
    // a replay file can be truncated) reads as floor rather than as garbage.
    if (have < n) std::memset(q + have, 0, n - have);

    end_msg(hp);
}

void encode_hits(std::vector<u8>& out, const RdFrame& f, u64 t_ns, u16 extra_flags) {
    u16 flags = extra_flags;
    if (f.overflow) flags |= flag::kOverflow;

    const auto hp = begin_msg(out, MsgType::Hits, flags, f.index, t_ns);

    HitsHead head{};
    head.n_hits    = static_cast<u32>(f.hits.size());
    head.n_targets = static_cast<u32>(f.targets.size());
    put(out, head);

    for (const Hit& h : f.hits) {
        WireHit w{};
        w.range_bin     = static_cast<u16>(h.range_bin < 0 ? 0 : h.range_bin);
        w.dopp_bin      = static_cast<i16>(h.dopp_bin);
        w.range_m       = static_cast<f32>(h.range_m);
        w.velocity_ms   = static_cast<f32>(h.velocity_ms);
        w.snr_db        = static_cast<f32>(h.snr_db);
        w.azimuth_deg   = static_cast<f32>(h.azimuth_deg);
        w.elevation_deg = static_cast<f32>(h.elevation_deg);
        w.angle_quality = static_cast<f32>(h.angle_quality);
        w.angle_valid   = h.angle_valid ? 1u : 0u;
        put(out, w);
    }
    for (const Target& t : f.targets) {
        WireTarget w{};
        w.range_m        = static_cast<f32>(t.range_m);
        w.velocity_ms    = static_cast<f32>(t.velocity_ms);
        w.azimuth_deg    = static_cast<f32>(t.azimuth_deg);
        w.elevation_deg  = static_cast<f32>(t.elevation_deg);
        w.snr_db         = static_cast<f32>(t.snr_db);
        w.x              = static_cast<f32>(t.x);
        w.y              = static_cast<f32>(t.y);
        w.z              = static_cast<f32>(t.z);
        w.extent_m       = static_cast<f32>(t.extent_m);
        w.dopp_spread_ms = static_cast<f32>(t.dopp_spread_ms);
        w.n_hits         = static_cast<u16>(t.n_hits < 0 ? 0 : t.n_hits);
        put(out, w);
    }
    end_msg(hp);
}

void encode_tracks(std::vector<u8>& out, const std::vector<Track>& tracks,
                   u64 frame, u64 t_ns, u16 extra_flags) {
    const auto hp = begin_msg(out, MsgType::Tracks, extra_flags, frame, t_ns);

    TracksHead head{};
    head.n_tracks = static_cast<u32>(tracks.size());
    put(out, head);

    for (const Track& t : tracks) {
        WireTrack w{};
        w.id  = t.id;
        w.x   = static_cast<f32>(t.x);   w.y  = static_cast<f32>(t.y);   w.z  = static_cast<f32>(t.z);
        w.vx  = static_cast<f32>(t.vx);  w.vy = static_cast<f32>(t.vy);  w.vz = static_cast<f32>(t.vz);
        // Upper triangle of the 3x3 position block of P.
        w.pxx = static_cast<f32>(t.P[0][0]);
        w.pxy = static_cast<f32>(t.P[0][1]);
        w.pxz = static_cast<f32>(t.P[0][2]);
        w.pyy = static_cast<f32>(t.P[1][1]);
        w.pyz = static_cast<f32>(t.P[1][2]);
        w.pzz = static_cast<f32>(t.P[2][2]);
        w.last_snr_db = static_cast<f32>(t.last_snr_db);
        w.age_s       = static_cast<f32>(t.age_s);
        w.micro_bw_hz = static_cast<f32>(t.micro_bw_hz);
        w.blade_hz    = static_cast<f32>(t.blade_hz);
        w.label_conf  = static_cast<f32>(t.label_conf);
        w.hits      = static_cast<u16>(t.hits   < 0 ? 0 : t.hits);
        w.misses    = static_cast<u16>(t.misses < 0 ? 0 : t.misses);
        w.spec_time = static_cast<u16>(t.spec_time < 0 ? 0 : t.spec_time);
        w.spec_freq = static_cast<u16>(t.spec_freq < 0 ? 0 : t.spec_freq);
        w.confirmed = t.confirmed ? 1u : 0u;
        set_label(w.label, sizeof(w.label), w.label_len, t.label);
        put(out, w);
    }
    end_msg(hp);
}

bool encode_spectrogram(std::vector<u8>& out, const Track& t, u64 frame, u64 t_ns,
                        f32 hz_per_bin, f32 s_per_row, DbRange r, u16 extra_flags) {
    const std::size_t n = std::size_t(t.spec_time <= 0 ? 0 : t.spec_time) *
                          std::size_t(t.spec_freq <= 0 ? 0 : t.spec_freq);
    if (!n || t.spectrogram.size() < n) return false;

    const auto hp = begin_msg(out, MsgType::Spectrogram, extra_flags, frame, t_ns);

    SpecHead head{};
    head.track_id   = t.id;
    head.n_time     = static_cast<u16>(t.spec_time);
    head.n_freq     = static_cast<u16>(t.spec_freq);
    head.min_db     = r.min_db;
    head.max_db     = r.max_db;
    head.hz_per_bin = hz_per_bin;
    head.s_per_row  = s_per_row;
    if (!(head.max_db > head.min_db)) head.max_db = head.min_db + 1.0f;
    put(out, head);

    const float lo  = head.min_db;
    const float inv = 1.0f / (head.max_db - head.min_db);

    const std::size_t at = out.size();
    out.resize(at + n);
    u8* q = out.data() + at;
    // The spectrogram in Track is already in dB, unlike the range-Doppler map.
    for (std::size_t i = 0; i < n; ++i) {
        const float v = t.spectrogram[i];
        q[i] = quantise(std::isfinite(v) ? v : lo, lo, inv);
    }

    end_msg(hp);
    return true;
}

bool encode_spectrogram(std::vector<u8>& out, const Track& t, u64 frame, u64 t_ns,
                        f32 hz_per_bin, f32 s_per_row, u16 extra_flags) {
    const std::size_t n = std::size_t(t.spec_time <= 0 ? 0 : t.spec_time) *
                          std::size_t(t.spec_freq <= 0 ? 0 : t.spec_freq);
    if (!n || t.spectrogram.size() < n) return false;

    float hi = -1e30f, lo = 1e30f;
    for (std::size_t i = 0; i < n; ++i) {
        const float v = t.spectrogram[i];
        if (!std::isfinite(v)) continue;
        if (v > hi) hi = v;
        if (v < lo) lo = v;
    }
    if (hi < lo) { hi = 0.0f; lo = -60.0f; }
    // Never let the floor sit more than 60 dB under the peak: below that the
    // display is colouring in noise and the interesting structure is squeezed
    // into a couple of quantiser steps.
    if (hi - lo > 60.0f) lo = hi - 60.0f;
    DbRange r{lo, hi};
    return encode_spectrogram(out, t, frame, t_ns, hz_per_bin, s_per_row, r, extra_flags);
}

void encode_stats(std::vector<u8>& out, const Stats& s, const NetTally& net,
                  SourceKind src, u64 frame, u64 t_ns, u16 extra_flags) {
    const auto hp = begin_msg(out, MsgType::Stats, extra_flags, frame, t_ns);

    WireStats w{};
    w.frames        = s.frames;
    w.overflows     = s.overflows;
    w.dropped       = s.dropped;
    w.bytes_in      = s.bytes_in;
    w.bytes_sent    = net.bytes_sent;
    w.net_dropped   = net.dropped_frames;
    w.cpu_frac      = static_cast<f32>(s.cpu_frac);
    w.frame_rate_hz = static_cast<f32>(s.frame_rate_hz);
    for (int i = 0; i < 8; ++i) w.stage_ms[i] = static_cast<f32>(s.stage_ms[i]);
    w.n_tracks = static_cast<u16>(s.n_tracks < 0 ? 0 : s.n_tracks);
    w.clients  = static_cast<u16>(net.clients < 0 ? 0 : net.clients);
    w.source   = static_cast<u16>(src);
    w.flags    = extra_flags;
    put(out, w);

    end_msg(hp);
}

void encode_config(std::vector<u8>& out, const Config& c, u64 frame, u64 t_ns,
                   u16 extra_flags) {
    const auto hp = begin_msg(out, MsgType::Config, extra_flags, frame, t_ns);

    WireConfig w{};
    w.centre_freq_hz  = c.centre_freq_hz;
    w.sample_rate_hz  = c.sample_rate_hz;
    w.sweep_bw_hz     = c.sweep_bw_hz;
    w.pfa             = c.pfa;
    w.range_bin_m     = static_cast<f32>(c.d.range_bin_m);
    w.range_max_m     = static_cast<f32>(c.d.range_max_m);
    w.range_res_m     = static_cast<f32>(c.d.range_res_m);
    w.vel_res_ms      = static_cast<f32>(c.d.vel_res_ms);
    w.vel_max_ms      = static_cast<f32>(c.d.vel_max_ms);
    w.frame_rate_hz   = static_cast<f32>(c.d.frame_rate_hz);
    w.t_cpi_s         = static_cast<f32>(c.d.t_cpi_s);
    w.aoa_az_span_deg = static_cast<f32>(c.aoa_az_span_deg);
    w.aoa_el_span_deg = static_cast<f32>(c.aoa_el_span_deg);
    w.tx_gain_db      = static_cast<f32>(c.tx_gain_db);
    w.rx_gain_db      = static_cast<f32>(c.rx_gain_db);
    w.n_range         = static_cast<u16>(c.n_range);
    w.n_doppler       = static_cast<u16>(c.n_doppler);
    w.n_chirp         = static_cast<u16>(c.n_chirp);
    w.max_hits        = static_cast<u16>(c.max_hits);
    w.zero_dopp_blank = static_cast<u16>(c.zero_dopp_blank);
    w.cfar_kind       = static_cast<u8>(c.cfar_kind);
    w.aoa_method      = static_cast<u8>(c.aoa);
    w.source          = static_cast<u8>(c.source);
    w.mimo            = static_cast<u8>(c.mimo);
    w.n_virt          = static_cast<u8>(c.d.n_virt);
    w.tx_enable       = c.tx_enable ? 1u : 0u;
    put(out, w);

    end_msg(hp);
}

//----------------------------------------------------------------------------
// Decoders
//----------------------------------------------------------------------------
bool decode_header(const u8* msg, std::size_t n, Header& h,
                   const u8** payload, std::size_t* payload_bytes) {
    if (!msg || n < kHeaderBytes) return false;
    std::memcpy(&h, msg, sizeof(Header));
    if (h.magic != kMagic) return false;
    if (std::size_t(h.bytes) > n - kHeaderBytes) return false;
    if (payload)       *payload = msg + kHeaderBytes;
    if (payload_bytes) *payload_bytes = h.bytes;
    return true;
}

bool decode_hello(const u8* payload, std::size_t n, WireHello& out) {
    std::size_t left = n;
    const u8* p = payload;
    return take(p, left, out);
}

bool decode_rdmap(const u8* payload, std::size_t n, RdMapMsg& out) {
    std::size_t left = n;
    const u8* p = payload;
    if (!take(p, left, out.head)) return false;
    const std::size_t cells = std::size_t(out.head.n_range) * std::size_t(out.head.n_doppler);
    if (left < cells) return false;
    out.q.assign(p, p + cells);
    return true;
}

bool decode_hits(const u8* payload, std::size_t n, HitsMsg& out) {
    std::size_t left = n;
    const u8* p = payload;
    HitsHead head{};
    if (!take(p, left, head)) return false;
    if (left < std::size_t(head.n_hits) * sizeof(WireHit) +
               std::size_t(head.n_targets) * sizeof(WireTarget)) return false;
    out.hits.resize(head.n_hits);
    if (head.n_hits) {
        std::memcpy(out.hits.data(), p, std::size_t(head.n_hits) * sizeof(WireHit));
        p    += std::size_t(head.n_hits) * sizeof(WireHit);
        left -= std::size_t(head.n_hits) * sizeof(WireHit);
    }
    out.targets.resize(head.n_targets);
    if (head.n_targets) {
        std::memcpy(out.targets.data(), p, std::size_t(head.n_targets) * sizeof(WireTarget));
    }
    return true;
}

bool decode_tracks(const u8* payload, std::size_t n, TracksMsg& out) {
    std::size_t left = n;
    const u8* p = payload;
    TracksHead head{};
    if (!take(p, left, head)) return false;
    if (left < std::size_t(head.n_tracks) * sizeof(WireTrack)) return false;
    out.tracks.resize(head.n_tracks);
    if (head.n_tracks) {
        std::memcpy(out.tracks.data(), p, std::size_t(head.n_tracks) * sizeof(WireTrack));
    }
    return true;
}

bool decode_spectrogram(const u8* payload, std::size_t n, SpecMsg& out) {
    std::size_t left = n;
    const u8* p = payload;
    if (!take(p, left, out.head)) return false;
    const std::size_t cells = std::size_t(out.head.n_time) * std::size_t(out.head.n_freq);
    if (left < cells) return false;
    out.q.assign(p, p + cells);
    return true;
}

bool decode_stats(const u8* payload, std::size_t n, WireStats& out) {
    std::size_t left = n;
    const u8* p = payload;
    return take(p, left, out);
}

bool decode_config(const u8* payload, std::size_t n, WireConfig& out) {
    std::size_t left = n;
    const u8* p = payload;
    return take(p, left, out);
}

//----------------------------------------------------------------------------
// Names
//----------------------------------------------------------------------------
const char* type_name(MsgType t) {
    switch (t) {
        case MsgType::Hello:       return "hello";
        case MsgType::RdMap:       return "rdmap";
        case MsgType::Hits:        return "hits";
        case MsgType::Tracks:      return "tracks";
        case MsgType::Stats:       return "stats";
        case MsgType::Spectrogram: return "spectrogram";
        case MsgType::Config:      return "config";
    }
    return "unknown";
}

const char* source_name(SourceKind s) {
    switch (s) {
        case SourceKind::Simulate: return "simulate";
        case SourceKind::Uhd:      return "hardware";
        case SourceKind::File:     return "replay";
    }
    return "unknown";
}

const char* cfar_name(CfarKind k) {
    switch (k) {
        case CfarKind::Ca:   return "CA";
        case CfarKind::Go:   return "GO";
        case CfarKind::So:   return "SO";
        case CfarKind::Os:   return "OS";
        case CfarKind::None: return "none";
    }
    return "unknown";
}

const char* aoa_name(AoaMethod a) {
    switch (a) {
        case AoaMethod::Monopulse: return "monopulse";
        case AoaMethod::Bartlett:  return "bartlett";
        case AoaMethod::Capon:     return "capon";
        case AoaMethod::Music:     return "music";
    }
    return "unknown";
}

const char* mimo_name(MimoMode m) {
    switch (m) {
        case MimoMode::Tdm:     return "tdm";
        case MimoMode::Ddm:     return "ddm";
        case MimoMode::Tx0Only: return "tx0";
        case MimoMode::Tx1Only: return "tx1";
    }
    return "unknown";
}

//----------------------------------------------------------------------------
// JSON
//----------------------------------------------------------------------------
std::string json_escape(const std::string& s) {
    std::string o;
    o.reserve(s.size() + 8);
    for (unsigned char c : s) {
        switch (c) {
            case '"':  o += "\\\""; break;
            case '\\': o += "\\\\"; break;
            case '\b': o += "\\b";  break;
            case '\f': o += "\\f";  break;
            case '\n': o += "\\n";  break;
            case '\r': o += "\\r";  break;
            case '\t': o += "\\t";  break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    o += buf;
                } else {
                    o += static_cast<char>(c);
                }
        }
    }
    return o;
}

std::string json_num(double v, int sig) {
    if (!std::isfinite(v)) return "null";
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.*g", sig, v);
    return buf;
}

namespace {
std::string kv(const char* k, const std::string& v) {
    return std::string("\"") + k + "\":" + v;
}
std::string kvs(const char* k, const std::string& v) {
    return std::string("\"") + k + "\":\"" + json_escape(v) + "\"";
}
std::string kvn(const char* k, double v, int sig = 6) {
    return kv(k, json_num(v, sig));
}
std::string kvu(const char* k, unsigned long long v) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%llu", v);
    return kv(k, buf);
}
std::string kvb(const char* k, bool v) {
    return kv(k, v ? "true" : "false");
}
} // namespace

std::string json_stats(const Stats& s, const NetTally& net) {
    static const char* kStage[8] = {"dechirp", "range", "corner", "doppler",
                                    "cfar", "aoa", "cluster", "track"};
    std::string o = "{";
    o += kvu("frames", s.frames)            + ",";
    o += kvu("overflows", s.overflows)      + ",";
    o += kvu("dropped", s.dropped)          + ",";
    o += kvn("cpu_frac", s.cpu_frac, 4)     + ",";
    o += kvn("frame_rate_hz", s.frame_rate_hz, 5) + ",";
    o += kvu("bytes_in", s.bytes_in)        + ",";
    o += kv("n_tracks", std::to_string(s.n_tracks)) + ",";
    o += "\"stage_ms\":{";
    for (int i = 0; i < 8; ++i) {
        if (i) o += ",";
        o += kvn(kStage[i], s.stage_ms[i], 4);
    }
    o += "},";
    o += "\"net\":{";
    o += kvu("bytes_sent", net.bytes_sent)      + ",";
    o += kvu("dropped_frames", net.dropped_frames) + ",";
    o += kv("clients", std::to_string(net.clients));
    o += "}";
    o += "}";
    return o;
}

std::string json_config(const Config& c) {
    std::string o = "{";
    o += kvs("source", source_name(c.source))       + ",";
    o += kvs("device_args", c.device_args)          + ",";
    o += kvn("centre_freq_hz", c.centre_freq_hz, 12) + ",";
    o += kvn("sample_rate_hz", c.sample_rate_hz, 12) + ",";
    o += kvn("tx_gain_db", c.tx_gain_db, 5)         + ",";
    o += kvn("rx_gain_db", c.rx_gain_db, 5)         + ",";
    o += kvn("rx_bandwidth_hz", c.rx_bandwidth_hz, 10) + ",";
    o += kvn("sweep_bw_hz", c.sweep_bw_hz, 10)      + ",";
    o += kv("n_sweep", std::to_string(c.n_sweep))   + ",";
    o += kv("n_pri", std::to_string(c.n_pri))       + ",";
    o += kv("n_chirp", std::to_string(c.n_chirp))   + ",";
    o += kvs("mimo", mimo_name(c.mimo))             + ",";
    o += kvb("tx_enable", c.tx_enable)              + ",";
    o += kv("decim", std::to_string(c.decim))       + ",";
    o += kv("n_range_fft", std::to_string(c.n_range_fft)) + ",";
    o += kv("n_range", std::to_string(c.n_range))   + ",";
    o += kv("n_doppler", std::to_string(c.n_doppler)) + ",";
    o += kvs("cfar", cfar_name(c.cfar_kind))        + ",";
    o += kvn("pfa", c.pfa, 4)                       + ",";
    o += kv("guard_range", std::to_string(c.guard_range)) + ",";
    o += kv("guard_dopp", std::to_string(c.guard_dopp))   + ",";
    o += kv("train_range", std::to_string(c.train_range)) + ",";
    o += kv("train_dopp", std::to_string(c.train_dopp))   + ",";
    o += kv("max_hits", std::to_string(c.max_hits)) + ",";
    o += kv("zero_dopp_blank", std::to_string(c.zero_dopp_blank)) + ",";
    o += kvs("aoa", aoa_name(c.aoa))                + ",";
    o += kvn("aoa_az_span_deg", c.aoa_az_span_deg, 5) + ",";
    o += kvn("aoa_el_span_deg", c.aoa_el_span_deg, 5) + ",";
    o += kv("track_confirm_n", std::to_string(c.track_confirm_n)) + ",";
    o += kv("track_drop_n", std::to_string(c.track_drop_n)) + ",";
    o += kv("range_zero_bin", std::to_string(c.range_zero_bin)) + ",";
    o += kv("worker_threads", std::to_string(c.worker_threads)) + ",";
    o += kvs("clock_source", c.clock_source) + ",";
    o += kvs("record_path", c.record_path)   + ",";
    o += kvs("scene_path", c.scene_path)     + ",";
    o += kv("http_port", std::to_string(c.http_port)) + ",";
    o += "\"derived\":{";
    o += kvn("lambda_m", c.d.lambda_m, 8)           + ",";
    o += kvn("range_res_m", c.d.range_res_m, 6)     + ",";
    o += kvn("range_bin_m", c.d.range_bin_m, 6)     + ",";
    o += kvn("range_max_m", c.d.range_max_m, 6)     + ",";
    o += kvn("vel_res_ms", c.d.vel_res_ms, 6)       + ",";
    o += kvn("vel_max_ms", c.d.vel_max_ms, 6)       + ",";
    o += kvn("frame_rate_hz", c.d.frame_rate_hz, 6) + ",";
    o += kvn("t_cpi_s", c.d.t_cpi_s, 6)             + ",";
    o += kvn("usb_bytes_s", c.d.usb_bytes_s, 8)     + ",";
    o += kv("n_virt", std::to_string(c.d.n_virt));
    o += "}";
    o += "}";
    return o;
}

std::string json_tracks(const std::vector<Track>& tracks) {
    std::string o = "[";
    bool first = true;
    for (const Track& t : tracks) {
        if (!first) o += ",";
        first = false;
        const double rng   = std::sqrt(t.x * t.x + t.y * t.y + t.z * t.z);
        const double speed = std::sqrt(t.vx * t.vx + t.vy * t.vy + t.vz * t.vz);
        const double az    = deg(std::atan2(t.x, t.y));
        const double el    = (rng > 1e-9) ? deg(std::asin(clampv(t.z / rng, -1.0, 1.0))) : 0.0;
        o += "{";
        o += kvu("id", t.id)                  + ",";
        o += kvn("x", t.x, 6)                 + ",";
        o += kvn("y", t.y, 6)                 + ",";
        o += kvn("z", t.z, 6)                 + ",";
        o += kvn("vx", t.vx, 6)               + ",";
        o += kvn("vy", t.vy, 6)               + ",";
        o += kvn("vz", t.vz, 6)               + ",";
        o += kvn("range_m", rng, 6)           + ",";
        o += kvn("speed_ms", speed, 6)        + ",";
        o += kvn("azimuth_deg", az, 5)        + ",";
        o += kvn("elevation_deg", el, 5)      + ",";
        o += kv("hits", std::to_string(t.hits))     + ",";
        o += kv("misses", std::to_string(t.misses)) + ",";
        o += kvb("confirmed", t.confirmed)    + ",";
        o += kvn("snr_db", t.last_snr_db, 5)  + ",";
        o += kvn("age_s", t.age_s, 5)         + ",";
        o += kvs("label", t.label)            + ",";
        o += kvn("label_conf", t.label_conf, 4) + ",";
        o += kvn("micro_bw_hz", t.micro_bw_hz, 5) + ",";
        o += kvn("blade_hz", t.blade_hz, 5)   + ",";
        o += kv("spec_time", std::to_string(t.spec_time)) + ",";
        o += kv("spec_freq", std::to_string(t.spec_freq)) + ",";
        // Position block of P, as the display sees it: [xx, xy, xz, yy, yz, zz].
        o += "\"cov_pos\":[";
        o += json_num(t.P[0][0], 6) + "," + json_num(t.P[0][1], 6) + "," +
             json_num(t.P[0][2], 6) + "," + json_num(t.P[1][1], 6) + "," +
             json_num(t.P[1][2], 6) + "," + json_num(t.P[2][2], 6);
        o += "]";
        o += "}";
    }
    o += "]";
    return o;
}

std::string json_status(const Config& c, const Stats& s, const NetTally& net,
                        const std::vector<Track>& tracks) {
    std::string o = "{";
    o += "\"protocol\":" + std::to_string(kVersion) + ",";
    o += "\"config\":" + json_config(c) + ",";
    o += "\"stats\":"  + json_stats(s, net) + ",";
    o += "\"tracks\":" + json_tracks(tracks);
    o += "}";
    return o;
}

} // namespace proto
} // namespace radar
