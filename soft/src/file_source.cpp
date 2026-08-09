//============================================================================
// file_source.cpp -- recording, replay, and the gateware's wire format
//
// Three things live here because they are the same subject from two ends.
//
//   wire::FrameDecoder / wire::encode
//       The 32-bit word stream section 7 of fpga/rtl/radar_pkg.svh defines.
//       uhd_source runs the radio's samples through the decoder; the recorder
//       and the simulator can produce exactly the same words through encode(),
//       so a replay is byte-identical to what the hardware would have sent and
//       the decoder is exercised whether or not a radio is plugged in.
//
//   FileRecorder
//       Writes frames to disk.  Always capped, because the machine this runs on
//       does not have room for an afternoon of range-Doppler cubes, and stops
//       cleanly at the cap instead of filling the volume.
//
//   FileSource
//       Plays a recording back at its original rate, or as fast as the disk
//       will go, optionally on a loop.  It also accepts a raw dump of gateware
//       words, which is what you get by pointing a capture tool at the radio.
//
// TRUNCATION
//   A recording that was cut short -- the process was killed, the disk filled,
//   the cable came out -- must replay everything up to the cut.  Every record
//   therefore carries its own length and checksum in front of it, so the reader
//   can tell "the file ends here" from "this record is damaged", and neither is
//   allowed to lose the frames that came before.
//============================================================================

#include <cstring>

#include "radar/source.hpp"
#include "radar/json.hpp"
#include "radar/log.hpp"
#include "radar/thread.hpp"

#include <algorithm>
#include <cmath>
#include <thread>

namespace radar {

//============================================================================
// wire
//============================================================================
namespace wire {

namespace {

inline u16 hi16(u32 w) { return u16(w >> 16); }
inline u16 lo16(u32 w) { return u16(w & 0xFFFFu); }
inline u32 pack16(u16 h, u16 l) { return (u32(h) << 16) | u32(l); }

/// Doppler bins arrive as an unsigned transform index; RdFrame carries them
/// centred on zero velocity, which is what everything downstream expects.
inline int centre_dopp(int raw, int n) { return (raw >= n / 2) ? raw - n : raw; }
inline int uncentre_dopp(int centred, int n) {
    int v = centred % n;
    if (v < 0) v += n;
    return v;
}

} // namespace

std::size_t frame_words(const Config& c, int n_hits) {
    return std::size_t(kHdrWords) + std::size_t(c.n_range) * std::size_t(c.n_doppler) +
           std::size_t(n_hits) * kHitWords + 1;
}

FrameDecoder::FrameDecoder(const Config& c) {
    max_range_   = std::max(1, c.n_range);
    max_dopp_    = std::max(1, c.n_doppler);
    max_hits_    = std::max(1, c.max_hits);
    range_bin_m_ = c.d.range_bin_m > 0
                       ? c.d.range_bin_m
                       : phys::c0 * c.decim / (2.0 * c.sweep_bw_hz / (c.n_sweep / c.sample_rate_hz) *
                                               c.n_range_fft / c.sample_rate_hz * c.decim);
    // The expression above is the range bin spacing written out longhand; when
    // derive() has run it is simply taken from the config.
    if (!(range_bin_m_ > 0) || !std::isfinite(range_bin_m_)) {
        const double t_sweep = c.n_sweep / c.sample_rate_hz;
        const double mu      = c.sweep_bw_hz / t_sweep;
        const double bin_hz  = (c.sample_rate_hz / c.decim) / c.n_range_fft;
        range_bin_m_         = bin_hz * phys::c0 / (2.0 * mu);
    }
    vel_res_ms_ = c.d.vel_res_ms > 0
                      ? c.d.vel_res_ms
                      : (phys::c0 / c.centre_freq_hz) /
                            (2.0 * std::max(1, c.n_doppler) * (c.n_pri / c.sample_rate_hz) *
                             (c.mimo == MimoMode::Tdm ? 2.0 : 1.0));
    range_zero_ = c.range_zero_bin;

    buf_.assign(std::size_t(kHdrWords) + std::size_t(max_range_) * max_dopp_ +
                    std::size_t(max_hits_) * kHitWords + 1,
                0u);
    reset();
}

void FrameDecoder::reset() {
    have_ = 0;
    want_ = 0;
    consumed_ = 0;
}

bool FrameDecoder::header_plausible() const {
    if (have_ < std::size_t(kHdrWords)) return false;
    if (hi16(buf_[1]) != kFmtVersion) return false;
    const int nr = int(hi16(buf_[3]));
    const int nd = int(lo16(buf_[3]));
    const int nh = int(hi16(buf_[4]));
    const u16 fl = lo16(buf_[1]);
    if (nh > max_hits_) return false;
    if (fl & kFlagMap) {
        if (nr <= 0 || nd <= 0) return false;
        if (nr > max_range_ || nd > max_dopp_) return false;
    }
    return true;
}

void FrameDecoder::emit(RdFrame& out) const {
    const u16 flags = lo16(buf_[1]);
    const int nr    = int(hi16(buf_[3]));
    const int nd    = int(lo16(buf_[3]));
    const int nh    = int(hi16(buf_[4]));
    const bool has_map = (flags & kFlagMap) != 0;

    out.allocate(has_map ? nr : 0, has_map ? nd : 0, 4, false);
    out.index       = buf_[2];
    out.overflow    = (flags & kFlagOverflow) != 0;
    out.noise_floor = double(buf_[5]);
    // The radio timestamp is in sample ticks; the caller converts it to seconds
    // because only it knows the rate the radio is running at.  It is carried in
    // the frame's index-adjacent field rather than being lost.
    const u64 ticks = u64(buf_[6]) | (u64(buf_[7]) << 32);
    out.timestamp_s = double(ticks);

    std::size_t p = kHdrWords;
    if (has_map) {
        const std::size_t n = std::size_t(nr) * nd;
        for (std::size_t i = 0; i < n; ++i) out.power[i] = float(buf_[p + i]);
        p += n;
    }

    out.hits.clear();
    out.hits.reserve(std::size_t(nh));
    for (int h = 0; h < nh; ++h) {
        const u32* w = &buf_[p + std::size_t(h) * kHitWords];
        Hit hit;
        hit.range_bin = int(w[0] & 0xFFu);
        hit.dopp_bin  = centre_dopp(int((w[0] >> 8) & 0xFFu), nd > 0 ? nd : 256);
        hit.power     = double(w[1]);
        for (int v = 0; v < 4; ++v) {
            const i16 re = i16(w[2 + v] >> 16);
            const i16 im = i16(w[2 + v] & 0xFFFFu);
            hit.virt[std::size_t(v)] = ci16(re, im).to_float();
        }
        hit.range_m     = (hit.range_bin - range_zero_) * range_bin_m_;
        hit.velocity_ms = hit.dopp_bin * vel_res_ms_;
        hit.snr_db      = out.noise_floor > 0 ? db(hit.power / out.noise_floor) : 0.0;
        out.hits.push_back(hit);
    }
    out.targets.clear();
}

void FrameDecoder::resync() {
    ++resyncs_;
    // Something in this candidate did not add up.  There may be a real frame
    // starting inside what was collected -- that is exactly what happens when a
    // packet goes missing mid-frame -- so hunt forward through the buffer
    // rather than throwing all of it away.
    std::size_t j = 1;
    while (j < have_ && buf_[j] != kMagic) ++j;
    skipped_ += j;
    if (j < have_) {
        std::memmove(buf_.data(), buf_.data() + j, (have_ - j) * sizeof(u32));
        have_ -= j;
    } else {
        have_ = 0;
    }
    want_ = 0;
}

bool FrameDecoder::step(RdFrame& out) {
    for (;;) {
        if (have_ < std::size_t(kHdrWords)) return false;
        if (want_ == 0) {
            if (!header_plausible()) { ++bad_frames_; resync(); continue; }
            const u16 flags = lo16(buf_[1]);
            const std::size_t nmap =
                (flags & kFlagMap) ? std::size_t(hi16(buf_[3])) * std::size_t(lo16(buf_[3])) : 0;
            want_ = std::size_t(kHdrWords) + nmap + std::size_t(hi16(buf_[4])) * kHitWords + 1;
            if (want_ > buf_.size()) { ++bad_frames_; want_ = 0; resync(); continue; }
        }
        if (have_ < want_) return false;
        if (buf_[want_ - 1] != kEndMark) {
            // The end marker is not where the header said it would be, so the
            // header lied or the body lost words.  Either way this frame is not
            // trustworthy.
            ++bad_frames_;
            want_ = 0;
            resync();
            continue;
        }
        emit(out);
        ++frames_;
        // Anything after this frame stays in the buffer for the next call.
        const std::size_t left = have_ - want_;
        if (left) std::memmove(buf_.data(), buf_.data() + want_, left * sizeof(u32));
        have_ = left;
        want_ = 0;
        return true;
    }
}

bool FrameDecoder::feed(const u32* words, std::size_t n, RdFrame& out) {
    consumed_ = 0;
    // Anything already buffered from a previous call may complete a frame
    // before a single new word is looked at.
    if (have_ && step(out)) return true;

    std::size_t i = 0;
    while (i < n) {
        if (have_ == 0) {
            // Hunting for the start of a frame.  This is the cheap state and
            // the one a stream spends its time in after a loss.
            if (words[i] != kMagic) { ++i; ++skipped_; continue; }
        }
        buf_[have_++] = words[i++];
        if (have_ >= std::size_t(kHdrWords)) {
            if (step(out)) { consumed_ = i; return true; }
        }
        if (have_ >= buf_.size()) { ++bad_frames_; resync(); }
    }
    consumed_ = n;
    return false;
}

void encode(const RdFrame& f, std::vector<u32>& out, const Config& c) {
    const bool has_map = f.n_range > 0 && f.n_doppler > 0 && f.power.size() > 0;
    const int  nh      = int(std::min<std::size_t>(f.hits.size(), std::size_t(std::max(0, c.max_hits))));
    const std::size_t nmap = has_map ? std::size_t(f.n_range) * f.n_doppler : 0;

    out.clear();
    out.resize(std::size_t(kHdrWords) + nmap + std::size_t(nh) * kHitWords + 1);

    u16 flags = 0;
    if (has_map)    flags |= kFlagMap;
    if (nh > 0)     flags |= kFlagHits;
    if (f.overflow) flags |= kFlagOverflow;
    if (c.tx_enable) flags |= kFlagTxOn;
    flags |= u16(u16(int(c.mimo) & 0x3) << kFlagMimoShift);

    out[0] = kMagic;
    out[1] = pack16(kFmtVersion, flags);
    out[2] = u32(f.index);
    out[3] = pack16(u16(has_map ? f.n_range : 0), u16(has_map ? f.n_doppler : 0));
    out[4] = pack16(u16(nh), 0);
    out[5] = u32(clampv(std::llround(f.noise_floor), 0ll, 4294967295ll));
    const u64 ticks = u64(std::llround(std::max(0.0, f.timestamp_s)));
    out[6] = u32(ticks & 0xFFFFFFFFull);
    out[7] = u32(ticks >> 32);

    std::size_t p = kHdrWords;
    for (std::size_t i = 0; i < nmap; ++i) {
        out[p + i] = u32(clampv(std::llround(double(f.power[i])), 0ll, 4294967295ll));
    }
    p += nmap;

    const int nd = f.n_doppler > 0 ? f.n_doppler : 256;
    for (int h = 0; h < nh; ++h) {
        const Hit& hit = f.hits[std::size_t(h)];
        u32*       w   = &out[p + std::size_t(h) * kHitWords];
        const u32 rb = u32(clampv(hit.range_bin, 0, 255));
        const u32 db_ = u32(uncentre_dopp(hit.dopp_bin, nd) & 0xFF);
        w[0] = (u32(hit.angle_valid ? 1u : 0u) << 24) | (db_ << 8) | rb;
        w[1] = u32(clampv(std::llround(hit.power), 0ll, 4294967295ll));
        for (int v = 0; v < 4; ++v) {
            const i16 re = fx::to_q15(hit.virt[std::size_t(v)].real());
            const i16 im = fx::to_q15(hit.virt[std::size_t(v)].imag());
            w[2 + v] = (u32(u16(re)) << 16) | u32(u16(im));
        }
    }
    p += std::size_t(nh) * kHitWords;
    out[p] = kEndMark;
}

} // namespace wire

//============================================================================
// The recording file
//============================================================================
namespace {

constexpr u32 kFileMagic = 0x52444152u;   ///< bytes 52 41 44 52 -> "RADR"
constexpr u16 kFileVer   = 1;
constexpr u32 kRecMagic  = 0x464D5246u;   ///< bytes 46 52 4D 46 -> "FRMF"
constexpr int kFileHdrBytes = 32;
constexpr int kRecHdrBytes  = 16;

/// CRC-32, the ordinary reflected polynomial.  Its job here is to tell a
/// damaged record from a short one, not to be cryptography.
u32 crc32(const unsigned char* p, std::size_t n, u32 crc = 0xFFFFFFFFu) {
    static u32 table[256];
    static bool built = false;
    if (!built) {
        for (u32 i = 0; i < 256; ++i) {
            u32 c = i;
            for (int k = 0; k < 8; ++k) c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            table[i] = c;
        }
        built = true;
    }
    for (std::size_t i = 0; i < n; ++i) crc = table[(crc ^ p[i]) & 0xFF] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFu;
}

//-- little-endian primitives, so a recording moves between machines ---------
struct Writer {
    std::vector<unsigned char>* b;
    void u8v(u8 v)   { b->push_back(v); }
    void u16v(u16 v) { for (int i = 0; i < 2; ++i) b->push_back(u8((v >> (8 * i)) & 0xFF)); }
    void u32v(u32 v) { for (int i = 0; i < 4; ++i) b->push_back(u8((v >> (8 * i)) & 0xFF)); }
    void u64v(u64 v) { for (int i = 0; i < 8; ++i) b->push_back(u8((v >> (8 * i)) & 0xFF)); }
    void i32v(i32 v) { u32v(u32(v)); }
    void f32v(float v)  { u32 t; std::memcpy(&t, &v, 4); u32v(t); }
    void f64v(double v) { u64 t; std::memcpy(&t, &v, 8); u64v(t); }
    void raw(const void* p, std::size_t n) {
        const unsigned char* q = static_cast<const unsigned char*>(p);
        b->insert(b->end(), q, q + n);
    }
};

struct Reader {
    const unsigned char* p = nullptr;
    std::size_t n = 0, i = 0;
    bool ok = true;
    bool need(std::size_t k) { if (i + k > n) { ok = false; return false; } return true; }
    u8  u8v()  { if (!need(1)) return 0; return p[i++]; }
    u16 u16v() { if (!need(2)) return 0; u16 v = u16(p[i]) | u16(u16(p[i + 1]) << 8); i += 2; return v; }
    u32 u32v() { if (!need(4)) return 0; u32 v = 0; for (int k = 0; k < 4; ++k) v |= u32(p[i + k]) << (8 * k); i += 4; return v; }
    u64 u64v() { if (!need(8)) return 0; u64 v = 0; for (int k = 0; k < 8; ++k) v |= u64(p[i + k]) << (8 * k); i += 8; return v; }
    i32 i32v() { return i32(u32v()); }
    float  f32v() { u32 t = u32v(); float v; std::memcpy(&v, &t, 4); return v; }
    double f64v() { u64 t = u64v(); double v; std::memcpy(&v, &t, 8); return v; }
};

/// The Config, as JSON, so a recording carries the operating point that made it
/// and a replay does not have to be told what it is looking at.
std::string config_json(const Config& c) {
    Json j = Json::object();
    j.set("centre_freq_hz", Json(c.centre_freq_hz));
    j.set("sample_rate_hz", Json(c.sample_rate_hz));
    j.set("sweep_bw_hz", Json(c.sweep_bw_hz));
    j.set("n_sweep", Json(c.n_sweep));
    j.set("n_pri", Json(c.n_pri));
    j.set("n_chirp", Json(c.n_chirp));
    j.set("n_range", Json(c.n_range));
    j.set("n_range_fft", Json(c.n_range_fft));
    j.set("n_doppler", Json(c.n_doppler));
    j.set("decim", Json(c.decim));
    j.set("mimo", Json(int(c.mimo)));
    j.set("rx_gain_db", Json(c.rx_gain_db));
    j.set("tx_gain_db", Json(c.tx_gain_db));
    j.set("range_zero_bin", Json(c.range_zero_bin));
    j.set("max_hits", Json(c.max_hits));
    return j.dump(0);
}

} // namespace

//----------------------------------------------------------------------------
// FileRecorder
//----------------------------------------------------------------------------
FileRecorder::~FileRecorder() { close(); }

bool FileRecorder::open(const Config& c, const std::string& path) {
    return open(c, path, max_bytes_, write_cube_);
}

bool FileRecorder::open(const Config& c, const std::string& path, u64 max_bytes, bool with_cube) {
    close();
    err_.clear();
    max_bytes_  = max_bytes;
    write_cube_ = with_cube;
    capped_     = false;
    bytes_      = 0;
    frames_     = 0;

    f_ = std::fopen(path.c_str(), "wb");
    if (!f_) { err_ = "recorder: cannot create " + path; return false; }

    const std::string js = config_json(c);
    std::vector<unsigned char> hdr;
    hdr.reserve(std::size_t(kFileHdrBytes) + js.size() + 8);
    Writer w{&hdr};
    w.u32v(kFileMagic);
    w.u16v(kFileVer);
    w.u16v(u16(with_cube ? 1 : 0));
    w.u32v(u32(js.size()));
    w.u32v(0);
    w.u64v(0);
    w.u64v(0);
    w.raw(js.data(), js.size());
    while (hdr.size() % 8) hdr.push_back(0);

    if (std::fwrite(hdr.data(), 1, hdr.size(), f_) != hdr.size()) {
        err_ = "recorder: cannot write the header to " + path;
        std::fclose(f_); f_ = nullptr; return false;
    }
    bytes_ = hdr.size();

    // One frame's worth of serialisation space, taken once so write() never
    // touches the allocator.
    const std::size_t worst =
        std::size_t(kRecHdrBytes) + 64 +
        std::size_t(c.n_range) * c.n_doppler * 4 +
        (with_cube ? std::size_t(4) * c.n_range * c.n_doppler * 8 : 0) +
        std::size_t(std::max(0, c.max_hits)) * 128 + 4096;
    scratch_.reserve(worst);
    return true;
}

void FileRecorder::write(const RdFrame& f) {
    if (!f_ || capped_) return;

    scratch_.clear();
    Writer w{&scratch_};
    const bool cube = write_cube_ && f.cube_valid && f.cube.size() > 0;

    w.u64v(f.index);
    w.f64v(f.timestamp_s);
    w.i32v(f.n_range);
    w.i32v(f.n_doppler);
    w.i32v(f.n_virt);
    w.u32v((f.overflow ? 1u : 0u) | (cube ? 2u : 0u));
    w.f64v(f.noise_floor);
    w.u32v(u32(f.hits.size()));
    w.u32v(u32(f.targets.size()));

    for (std::size_t i = 0; i < f.power.size(); ++i) w.f32v(f.power[i]);
    if (cube) {
        for (std::size_t i = 0; i < f.cube.size(); ++i) {
            w.f32v(f.cube[i].real());
            w.f32v(f.cube[i].imag());
        }
    }
    for (const Hit& h : f.hits) {
        w.i32v(h.range_bin); w.i32v(h.dopp_bin);
        w.f64v(h.range_m);   w.f64v(h.velocity_ms);
        w.f64v(h.power);     w.f64v(h.snr_db);
        for (int v = 0; v < 4; ++v) { w.f32v(h.virt[std::size_t(v)].real()); w.f32v(h.virt[std::size_t(v)].imag()); }
        w.f64v(h.azimuth_deg); w.f64v(h.elevation_deg);
        w.u32v(h.angle_valid ? 1u : 0u);
        w.f64v(h.angle_quality);
    }
    for (const Target& t : f.targets) {
        w.f64v(t.range_m); w.f64v(t.velocity_ms);
        w.f64v(t.azimuth_deg); w.f64v(t.elevation_deg);
        w.f64v(t.snr_db);
        w.f64v(t.x); w.f64v(t.y); w.f64v(t.z);
        w.i32v(t.n_hits);
        w.f64v(t.extent_m); w.f64v(t.dopp_spread_ms);
    }

    const u64 need = u64(kRecHdrBytes) + scratch_.size();
    if (bytes_ + need > max_bytes_) {
        capped_ = true;
        LOG_W("recorder: reached the %.0f MB cap after %llu frames; the file is closed and valid",
              double(max_bytes_) / (1024 * 1024), (unsigned long long)frames_);
        close();
        return;
    }

    unsigned char hdr[kRecHdrBytes];
    const u32 crc = crc32(scratch_.data(), scratch_.size());
    const u32 words[4] = {kRecMagic, u32(scratch_.size()), crc, 0};
    for (int i = 0; i < 4; ++i)
        for (int k = 0; k < 4; ++k) hdr[i * 4 + k] = u8((words[i] >> (8 * k)) & 0xFF);

    if (std::fwrite(hdr, 1, sizeof(hdr), f_) != sizeof(hdr) ||
        std::fwrite(scratch_.data(), 1, scratch_.size(), f_) != scratch_.size()) {
        err_ = "recorder: the write failed, probably out of disk space";
        close();
        return;
    }
    bytes_ += need;
    ++frames_;
}

void FileRecorder::close() {
    if (f_) { std::fclose(f_); f_ = nullptr; }
}

//============================================================================
// FileSource
//============================================================================
namespace {

class FileSource : public IqSource {
public:
    FileSource(std::string path, FileSourceOptions opt)
        : path_(std::move(path)), opt_(opt) {}
    ~FileSource() override { close(); }

    bool open(const Config& c) override;
    void close() override;
    bool running() const override { return f_ != nullptr; }

    bool next_raw(IqCpi& out, double timeout_s) override {
        (void)out; (void)timeout_s;
        set_error("replay: this recording holds processed frames, not raw samples; "
                  "the pipeline should be calling next_frame");
        return false;
    }
    bool gives_frames() const override { return true; }
    bool next_frame(RdFrame& out, double timeout_s) override;

    const char* name() const override { return "file"; }
    bool  ended() const override { return ended_; }
    Stats stats() const override;

private:
    bool read_header();
    bool next_structured(RdFrame& out);
    bool next_wire(RdFrame& out);
    bool rewind_to_first();

    std::string       path_;
    FileSourceOptions opt_;
    Config            cfg_;
    std::FILE*        f_ = nullptr;
    bool              raw_wire_ = false;
    long              first_record_ = 0;
    bool              ended_ = false;

    u64    frames_ = 0;
    u64    bad_    = 0;
    u64    bytes_  = 0;
    double t_first_rec_ = 0;      ///< timestamp of the first frame played
    double t_first_wall_ = 0;
    double tick_s_ = 0;           ///< seconds per radio tick, for wire timestamps

    std::vector<unsigned char>    rec_;
    std::vector<u32>              words_;
    std::size_t                   wpos_ = 0, wlen_ = 0;
    std::unique_ptr<wire::FrameDecoder> dec_;
};

bool FileSource::open(const Config& c) {
    clear_error();
    close();
    cfg_    = c;
    ended_  = false;
    frames_ = 0; bad_ = 0; bytes_ = 0;
    tick_s_ = c.sample_rate_hz > 0 ? 1.0 / c.sample_rate_hz : 0.0;

    if (path_.empty()) {
        set_error("replay: no recording path was given (set record_path, or pass one to "
                  "make_file_source)");
        return false;
    }
    f_ = std::fopen(path_.c_str(), "rb");
    if (!f_) { set_error("replay: cannot open " + path_); return false; }
    if (!read_header()) { std::fclose(f_); f_ = nullptr; return false; }

    dec_.reset(new wire::FrameDecoder(c));
    rec_.reserve(1 << 20);
    words_.resize(1 << 16);
    wpos_ = wlen_ = 0;
    t_first_rec_ = t_first_wall_ = 0;

    LOG_I("replay: %s, %s format%s%s", path_.c_str(),
          raw_wire_ ? "raw gateware words" : "recorded frames",
          opt_.as_fast ? ", as fast as possible" : "",
          opt_.loop ? ", looping" : "");
    return true;
}

void FileSource::close() {
    if (f_) { std::fclose(f_); f_ = nullptr; }
    dec_.reset();
}

bool FileSource::read_header() {
    unsigned char head[kFileHdrBytes];
    const std::size_t got = std::fread(head, 1, sizeof(head), f_);
    if (got < 4) { set_error("replay: " + path_ + " is too short to be a recording"); return false; }

    u32 magic = 0;
    for (int k = 0; k < 4; ++k) magic |= u32(head[k]) << (8 * k);

    if (magic == wire::kMagic) {
        // A raw dump of the words the gateware emits.  Start again from zero
        // and hand the whole file to the decoder.
        raw_wire_     = true;
        first_record_ = 0;
        std::fseek(f_, 0, SEEK_SET);
        return true;
    }
    if (magic != kFileMagic) {
        set_error("replay: " + path_ + " does not start with a recording header or a "
                  "gateware magic word");
        return false;
    }
    if (got < sizeof(head)) { set_error("replay: " + path_ + " has a truncated header"); return false; }

    Reader r{head, sizeof(head), 4, true};
    const u16 ver = r.u16v();
    r.u16v();                                   // flags
    const u32 jlen = r.u32v();
    if (ver != kFileVer) {
        set_error("replay: " + path_ + " is format version " + std::to_string(ver) +
                  ", this build reads version " + std::to_string(kFileVer));
        return false;
    }
    // Skip the JSON and its padding.
    std::size_t skip = jlen;
    while ((sizeof(head) + skip) % 8) ++skip;
    if (std::fseek(f_, long(sizeof(head) + skip), SEEK_SET) != 0) {
        set_error("replay: " + path_ + " ends inside its header");
        return false;
    }
    raw_wire_     = false;
    first_record_ = long(sizeof(head) + skip);
    return true;
}

bool FileSource::rewind_to_first() {
    if (!f_) return false;
    std::clearerr(f_);
    if (std::fseek(f_, first_record_, SEEK_SET) != 0) return false;
    wpos_ = wlen_ = 0;
    if (dec_) dec_->reset();
    t_first_rec_ = t_first_wall_ = 0;
    return true;
}

bool FileSource::next_structured(RdFrame& out) {
    for (;;) {
        unsigned char hdr[kRecHdrBytes];
        const std::size_t got = std::fread(hdr, 1, sizeof(hdr), f_);
        if (got < sizeof(hdr)) return false;                 // clean end of file
        u32 w[4] = {};
        for (int i = 0; i < 4; ++i)
            for (int k = 0; k < 4; ++k) w[i] |= u32(hdr[i * 4 + k]) << (8 * k);
        if (w[0] != kRecMagic) {
            // Damaged. Walk forward a byte at a time looking for the next
            // record rather than giving up on the rest of the file.
            ++bad_;
            long back = long(sizeof(hdr)) - 1;
            if (std::fseek(f_, -back, SEEK_CUR) != 0) return false;
            continue;
        }
        const u32 len = w[1];
        if (len > (256u << 20)) { ++bad_; continue; }
        rec_.resize(len);
        if (std::fread(rec_.data(), 1, len, f_) != len) return false;   // truncated tail
        if (crc32(rec_.data(), len) != w[2]) { ++bad_; continue; }
        bytes_ += sizeof(hdr) + len;

        Reader r{rec_.data(), rec_.size(), 0, true};
        const u64 idx = r.u64v();
        const double ts = r.f64v();
        const int nr = r.i32v(), nd = r.i32v(), nv = r.i32v();
        const u32 flags = r.u32v();
        const double nf = r.f64v();
        const u32 nh = r.u32v(), nt = r.u32v();
        const bool cube = (flags & 2u) != 0;
        if (!r.ok || nr < 0 || nd < 0 || nv < 0) { ++bad_; continue; }

        out.allocate(nr, nd, nv > 0 ? nv : 4, cube);
        out.index       = idx;
        out.timestamp_s = ts;
        out.overflow    = (flags & 1u) != 0;
        out.noise_floor = nf;
        for (std::size_t i = 0; i < out.power.size(); ++i) out.power[i] = r.f32v();
        if (cube) {
            for (std::size_t i = 0; i < out.cube.size(); ++i) {
                const float re = r.f32v(), im = r.f32v();
                out.cube[i] = cf32(re, im);
            }
        }
        out.hits.clear();
        out.hits.reserve(nh);
        for (u32 i = 0; i < nh && r.ok; ++i) {
            Hit h;
            h.range_bin = r.i32v(); h.dopp_bin = r.i32v();
            h.range_m = r.f64v();   h.velocity_ms = r.f64v();
            h.power = r.f64v();     h.snr_db = r.f64v();
            for (int v = 0; v < 4; ++v) { const float re = r.f32v(), im = r.f32v(); h.virt[std::size_t(v)] = cf32(re, im); }
            h.azimuth_deg = r.f64v(); h.elevation_deg = r.f64v();
            h.angle_valid = r.u32v() != 0;
            h.angle_quality = r.f64v();
            out.hits.push_back(h);
        }
        out.targets.clear();
        out.targets.reserve(nt);
        for (u32 i = 0; i < nt && r.ok; ++i) {
            Target t;
            t.range_m = r.f64v(); t.velocity_ms = r.f64v();
            t.azimuth_deg = r.f64v(); t.elevation_deg = r.f64v();
            t.snr_db = r.f64v();
            t.x = r.f64v(); t.y = r.f64v(); t.z = r.f64v();
            t.n_hits = r.i32v();
            t.extent_m = r.f64v(); t.dopp_spread_ms = r.f64v();
            out.targets.push_back(t);
        }
        if (!r.ok) { ++bad_; continue; }
        return true;
    }
}

bool FileSource::next_wire(RdFrame& out) {
    for (;;) {
        if (wpos_ < wlen_) {
            if (dec_->feed(words_.data() + wpos_, wlen_ - wpos_, out)) {
                wpos_ += dec_->consumed();
                out.timestamp_s *= tick_s_;      // radio ticks to seconds
                return true;
            }
            wpos_ = wlen_;
        }
        const std::size_t got = std::fread(words_.data(), sizeof(u32), words_.size(), f_);
        bytes_ += got * sizeof(u32);
        if (got == 0) return false;
        wpos_ = 0;
        wlen_ = got;
    }
}

bool FileSource::next_frame(RdFrame& out, double timeout_s) {
    clear_error();
    if (!f_) { set_error("replay: next_frame called before open"); return false; }

    bool got = raw_wire_ ? next_wire(out) : next_structured(out);
    if (!got) {
        if (opt_.loop && frames_ > 0 && rewind_to_first()) {
            got = raw_wire_ ? next_wire(out) : next_structured(out);
        }
        if (!got) {
            ended_ = true;
            set_error("replay: reached the end of " + path_ + " after " +
                      std::to_string(frames_) + " frames" +
                      (bad_ ? " (" + std::to_string(bad_) + " damaged records skipped)" : ""));
            return false;
        }
    }

    // Timing.  The first frame sets the origin; every later one waits until its
    // own recorded offset has elapsed.
    if (!opt_.as_fast) {
        const double mult = opt_.rate_mult > 0 ? opt_.rate_mult : 1.0;
        if (frames_ == 0) {
            t_first_rec_  = out.timestamp_s;
            t_first_wall_ = rt::now_s();
        } else {
            const double due  = t_first_wall_ + (out.timestamp_s - t_first_rec_) / mult;
            const double wait = due - rt::now_s();
            if (wait > timeout_s) {
                // Not due yet.  Put the frame back by remembering nothing: the
                // caller will ask again and the file position has already moved,
                // so instead of rewinding we simply sleep the timeout and hand
                // it over.  A replay that is behind its own clock is not an
                // error worth losing a frame over.
                std::this_thread::sleep_for(std::chrono::duration<double>(timeout_s));
            } else if (wait > 0) {
                std::this_thread::sleep_for(std::chrono::duration<double>(wait));
            }
        }
    }
    ++frames_;
    return true;
}

Stats FileSource::stats() const {
    Stats s;
    s.frames   = frames_;
    s.dropped  = bad_;
    s.bytes_in = bytes_;
    return s;
}

/// Pull "?fast&loop&rate=2.0" off the end of a path.
std::string split_options(const std::string& path, FileSourceOptions& opt) {
    const std::size_t q = path.find('?');
    if (q == std::string::npos) return path;
    const std::string base = path.substr(0, q);
    std::string       rest = path.substr(q + 1);
    std::size_t       i    = 0;
    while (i <= rest.size()) {
        const std::size_t j = rest.find('&', i);
        const std::string tok = rest.substr(i, j == std::string::npos ? std::string::npos : j - i);
        if (tok == "fast")      opt.as_fast = true;
        else if (tok == "loop") opt.loop = true;
        else if (tok.rfind("rate=", 0) == 0) opt.rate_mult = std::atof(tok.c_str() + 5);
        if (j == std::string::npos) break;
        i = j + 1;
    }
    return base;
}

} // namespace

std::unique_ptr<IqSource> make_file_source(const Config& c, const std::string& path,
                                           const FileSourceOptions& opt) {
    (void)c;
    FileSourceOptions o = opt;
    const std::string p = split_options(path, o);
    return std::unique_ptr<IqSource>(new FileSource(p, o));
}

} // namespace radar
