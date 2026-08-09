//============================================================================
// pipeline.cpp -- the radar, running
//
// One thread owns a frame from end to end.  That is deliberate: a stage-per-
// thread arrangement would look more parallel and be slower, because the
// frame is 8 MB of intermediate state and handing it between cores costs more
// than the work saved.  Inside the frame, the range transforms -- the only
// part that is genuinely embarrassingly parallel -- are spread over a small
// worker pool.
//
// On hardware most of this thread does nothing: the fabric has already
// produced the map and the detections, and what is left is angle, clustering
// and tracking on a few hundred points.
//============================================================================
#include "radar/pipeline.hpp"

#include "radar/aoa.hpp"
#include "radar/cfar.hpp"
#include "radar/cluster.hpp"
#include "radar/fft.hpp"
#include "radar/log.hpp"
#include "radar/microdoppler.hpp"
#include "radar/refmodel.hpp"
#include "radar/source.hpp"
#include "radar/thread.hpp"
#include "radar/track.hpp"
#include "radar/waveform.hpp"

#include <atomic>
#include <condition_variable>
#include <cstring>
#include <mutex>
#include <thread>
#include <vector>

namespace radar {

//============================================================================
// A minimal fixed thread pool.  std::async would allocate per task, and the
// range transforms run 256 times a frame at 62.5 frames a second.
//============================================================================
namespace {

class Pool {
public:
    explicit Pool(int n) {
        if (n < 1) n = 1;
        stop_ = false;
        for (int i = 0; i < n; ++i) {
            workers_.emplace_back([this, i] {
                rt::set_name("radar-worker");
                (void)i;
                for (;;) {
                    std::function<void()> job;
                    {
                        std::unique_lock<std::mutex> lk(m_);
                        cv_.wait(lk, [this] { return stop_ || !q_.empty(); });
                        if (stop_ && q_.empty()) return;
                        job = std::move(q_.front());
                        q_.pop_back();
                    }
                    job();
                    if (--outstanding_ == 0) {
                        std::lock_guard<std::mutex> lk(dm_);
                        dcv_.notify_all();
                    }
                }
            });
        }
    }
    ~Pool() {
        { std::lock_guard<std::mutex> lk(m_); stop_ = true; }
        cv_.notify_all();
        for (auto& t : workers_) if (t.joinable()) t.join();
    }

    /// Run fn(i) for i in [0, n), then return. The calling thread takes a
    /// share of the work rather than idling.
    void parallel_for(int n, const std::function<void(int)>& fn) {
        if (n <= 0) return;
        if (workers_.empty() || n == 1) { for (int i = 0; i < n; ++i) fn(i); return; }

        const int chunks = int(workers_.size()) + 1;
        const int per    = (n + chunks - 1) / chunks;
        outstanding_ = chunks - 1;
        {
            std::lock_guard<std::mutex> lk(m_);
            for (int c = 1; c < chunks; ++c) {
                const int a = c * per, b = std::min(n, a + per);
                q_.push_back([fn, a, b] { for (int i = a; i < b; ++i) fn(i); });
            }
        }
        cv_.notify_all();
        for (int i = 0; i < std::min(n, per); ++i) fn(i);   // our own share
        std::unique_lock<std::mutex> lk(dm_);
        dcv_.wait(lk, [this] { return outstanding_.load() == 0; });
    }

private:
    std::vector<std::thread>          workers_;
    std::vector<std::function<void()>> q_;
    std::mutex                        m_, dm_;
    std::condition_variable           cv_, dcv_;
    std::atomic<int>                  outstanding_{0};
    bool                              stop_ = true;
};

double mono() {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

} // namespace

//============================================================================
struct Pipeline::Impl {
    explicit Impl(const Config& c)
        : cfg(c), wf(c), rm(c), cfar(c), aoa(c), clus(c), trk(c), md(c),
          pool(c.worker_threads) {}

    Config      cfg;
    Waveform    wf;
    RefModel    rm;
    Cfar2D      cfar;
    AoaEngine   aoa;
    Clusterer   clus;
    Tracker     trk;
    MicroDoppler md;
    Calibration cal;
    Pool        pool;

    std::unique_ptr<IqSource>    src;
    std::unique_ptr<FileRecorder> rec;
    u64                          rec_cap = 0;

    std::thread        worker;
    std::atomic<bool>  run{false};
    std::atomic<bool>  frozen{false};
    std::atomic<bool>  dirty{false};
    std::atomic<double> dyn_range{70.0};

    // Pending control changes, applied between frames so a frame is never
    // processed half under one setting and half under another.
    std::mutex   ctl_m;
    Config       pending;

    mutable std::mutex fm;
    RdFrame            last_frame;
    std::vector<Track> last_tracks;
    bool               have_frame = false;

    FrameFn      cb;
    mutable std::mutex sm;
    Stats        st;

    // Scratch, allocated once.
    IqCpi              cpi;
    RdFrame            frame;
    std::vector<Hit>   hits;
    std::vector<Target> targets;
    std::vector<Track>  tracks;
    std::vector<cf32>   slow;

    void loop();
    void back_end(RdFrame& f, double t_frame_start);
    void apply_pending();
};

//----------------------------------------------------------------------------
void Pipeline::Impl::apply_pending() {
    if (!dirty.exchange(false)) return;
    Config c;
    { std::lock_guard<std::mutex> lk(ctl_m); c = pending; }
    c.derive();
    const std::string bad = c.validate();
    if (!bad.empty()) {
        LOG_W("refusing that setting: %s", bad.c_str());
        return;
    }
    // Only the stages whose parameters changed are rebuilt. Rebuilding the
    // tracker would throw away every track, which is not what someone nudging
    // a CFAR slider expects.
    const bool cfar_changed = c.pfa != cfg.pfa || c.cfar_kind != cfg.cfar_kind
                            || c.zero_dopp_blank != cfg.zero_dopp_blank
                            || c.range_zero_bin != cfg.range_zero_bin;
    const bool aoa_changed  = c.aoa != cfg.aoa;
    cfg = c;
    if (cfar_changed) cfar = Cfar2D(cfg);
    if (aoa_changed)  aoa  = AoaEngine(cfg);
}

//----------------------------------------------------------------------------
// Everything after the range-Doppler map exists.
//----------------------------------------------------------------------------
void Pipeline::Impl::back_end(RdFrame& f, double t0) {
    double t = mono();
    double stage[8] = {};

    //-- Detection ---------------------------------------------------------
    // A hardware source has already run CFAR in the fabric and sent the
    // detections, so re-running it here would be wasted work and would
    // disagree with what the FPGA decided.
    if (f.hits.empty()) {
        cfar.detect(f, hits);
        f.hits = hits;
    }
    f.noise_floor = cfar.noise_floor();
    stage[4] = mono() - t; t = mono();

    //-- Range, velocity, angle -------------------------------------------
    const int P = cfg.n_doppler;
    for (auto& h : f.hits) {
        h.range_m     = wf.range_of_bin(h.range_bin) - wf.range_of_bin(cal.range_zero_bin());
        h.velocity_ms = wf.velocity_of_bin(h.dopp_bin);

        // The transmitters take turns, so the second one's chirps happen half
        // a time-division interval after the first one's. That puts a phase of
        // exp(-j*pi*m/P) on its two virtual channels at Doppler bin m, and it
        // has to come off before the four channels can be compared. The fabric
        // deliberately leaves this to us: it does not change the magnitude, so
        // it cannot affect the map or the detection, and doing it here costs a
        // multiply per detection instead of a multiplier in silicon.
        if (cfg.mimo == MimoMode::Tdm) {
            const double ang = -kPi * double(h.dopp_bin) / double(P);
            const cf32   w(float(std::cos(ang)), float(std::sin(ang)));
            h.virt[2] *= w;   // transmitter 1, receiver 0
            h.virt[3] *= w;   // transmitter 1, receiver 1
        }

        cal.apply(h.virt);
        auto r = aoa.estimate(h.virt);
        if (r.valid) {
            // One refinement pass through the angle-dependent part of the
            // calibration. The residual is small against the beamwidth, so a
            // second pass would not move the answer.
            std::array<cf32, 4> v = h.virt;
            cal.apply_angular(v, r.az_deg, r.el_deg);
            r = aoa.estimate(v);
        }
        h.azimuth_deg   = r.az_deg;
        h.elevation_deg = r.el_deg;
        h.angle_valid   = r.valid;
        h.angle_quality = r.quality_db;
    }
    stage[5] = mono() - t; t = mono();

    //-- Clustering --------------------------------------------------------
    clus.cluster(f.hits, targets);
    f.targets = targets;
    stage[6] = mono() - t; t = mono();

    //-- Tracking ----------------------------------------------------------
    trk.update(targets, cfg.d.t_cpi_s, tracks);
    stage[7] = mono() - t;

    //-- Micro-Doppler, for confirmed tracks, when we have the complex cube --
    if (f.cube_valid) {
        for (auto& tk : tracks) {
            if (!tk.confirmed) continue;
            const double r = std::sqrt(tk.x * tk.x + tk.y * tk.y + tk.z * tk.z);
            const int rb = clampv(wf.bin_of_range(r) + cal.range_zero_bin(), 0, f.n_range - 1);
            slow.resize(std::size_t(f.n_doppler) * cfg.d.n_chirp_total / f.n_doppler);
            // Take the whole Doppler row for that range bin as the target's
            // slow-time signature. It is already the transform of the slow
            // time series, so the micro-Doppler stage inverts it back.
            slow.assign(f.n_doppler, cf32(0, 0));
            for (int d = 0; d < f.n_doppler; ++d) {
                cf32 acc(0, 0);
                for (int v = 0; v < f.n_virt; ++v) acc += f.cube_at(v, rb, d);
                slow[std::size_t(d)] = acc;
            }
            md.analyse(slow.data(), int(slow.size()), tk);
        }
    }

    //-- Publish -----------------------------------------------------------
    {
        std::lock_guard<std::mutex> lk(sm);
        ++st.frames;
        if (f.overflow) ++st.overflows;
        st.n_tracks = int(tracks.size());
        for (int i = 0; i < 8; ++i) st.stage_ms[i] = stage[i] * 1e3;
        const double wall = mono() - t0;
        st.cpu_frac = wall / std::max(cfg.d.t_cpi_s, 1e-9);
        // Exponentially smoothed, because a single frame's rate is noise.
        st.frame_rate_hz = st.frame_rate_hz == 0.0
                         ? 1.0 / std::max(wall, 1e-9)
                         : 0.9 * st.frame_rate_hz + 0.1 / std::max(wall, 1e-9);
    }
    {
        std::lock_guard<std::mutex> lk(fm);
        last_tracks = tracks;
        have_frame  = true;
        // Copy only what a consumer can use without the pipeline's scratch.
        last_frame.allocate(f.n_range, f.n_doppler, f.n_virt, false);
        std::memcpy(last_frame.power.data(), f.power.data(), f.power.size() * sizeof(float));
        last_frame.index = f.index;
        last_frame.hits = f.hits;
        last_frame.targets = f.targets;
        last_frame.noise_floor = f.noise_floor;
    }
    if (rec) rec->write(f);
    if (cb) {
        Stats snap;
        { std::lock_guard<std::mutex> lk(sm); snap = st; }
        cb(f, tracks, snap);
    }
}

//----------------------------------------------------------------------------
void Pipeline::Impl::loop() {
    rt::set_name("radar-pipeline");
    if (cfg.realtime) {
        const double p = cfg.d.t_cpi_s;
        if (rt::set_realtime(p, p * 0.6, p * 0.9))
            LOG_I("pipeline thread is running at real-time priority");
        else
            LOG_W("could not get real-time priority; frames may be late under load");
    }

    const bool from_fabric = src->gives_frames();
    LOG_I("pipeline: %s", from_fabric
          ? "the fabric sends maps and detections; the host does angle and tracking"
          : "the host is running the fabric's datapath in the bit-exact model");

    while (run.load(std::memory_order_relaxed)) {
        apply_pending();
        const double t0 = mono();

        if (from_fabric) {
            if (!src->next_frame(frame, 1.0)) {
                if (src->ended()) break;
                continue;
            }
        } else {
            if (!src->next_raw(cpi, 1.0)) {
                if (src->ended()) break;
                continue;
            }
            if (frozen.load()) continue;
            std::vector<const ci16*> ptrs(std::size_t(cpi.n_rx));
            for (int r = 0; r < cpi.n_rx; ++r) ptrs[std::size_t(r)] = cpi.chirp(r, 0);
            rm.process_cpi(ptrs.data(), cpi.n_rx, cpi.n_chirp_total, frame);
            frame.index      = cpi.index;
            frame.timestamp_s = cpi.timestamp_s;
            frame.overflow   = cpi.overflow;
        }
        if (frozen.load()) continue;
        back_end(frame, t0);
    }
    run.store(false);
}

//============================================================================
Pipeline::Pipeline(const Config& cfg) : p_(new Impl(cfg)) {
    p_->pending = cfg;
    if (!cfg.calib_path.empty()) {
        std::string err;
        if (!p_->cal.load(cfg.calib_path, &err)) LOG_W("calibration: %s", err.c_str());
    }
}

Pipeline::~Pipeline() { stop(); }

bool Pipeline::start(std::string& err) {
    if (p_->run.load()) return true;
    p_->src = make_source(p_->cfg);
    if (!p_->src) { err = "no source for that configuration"; return false; }
    if (!p_->src->open(p_->cfg)) {
        err = p_->src->last_error().empty() ? "the source would not open" : p_->src->last_error();
        return false;
    }
    p_->run.store(true);
    p_->worker = std::thread([this] { p_->loop(); });
    return true;
}

void Pipeline::stop() {
    if (!p_) return;
    p_->run.store(false);
    if (p_->worker.joinable()) p_->worker.join();
    if (p_->src) { p_->src->close(); p_->src.reset(); }
    record_stop();
}

bool Pipeline::running() const { return p_->run.load(); }

void Pipeline::on_frame(FrameFn fn) { p_->cb = std::move(fn); }

const Config& Pipeline::config() const { return p_->cfg; }
const char*   Pipeline::source_name() const { return p_->src ? p_->src->name() : "not started"; }

Stats Pipeline::stats() const {
    std::lock_guard<std::mutex> lk(p_->sm);
    Stats s = p_->st;
    if (p_->src) {
        const Stats ss = p_->src->stats();
        s.dropped  += ss.dropped;
        s.bytes_in  = ss.bytes_in;
    }
    return s;
}

//-- Runtime control ---------------------------------------------------------
#define SET_PENDING(field, value)                              \
    do {                                                       \
        std::lock_guard<std::mutex> lk(p_->ctl_m);             \
        p_->pending = p_->cfg;                                 \
        p_->pending.field = (value);                           \
        p_->dirty.store(true);                                 \
    } while (0)

void Pipeline::set_pfa(double v)             { SET_PENDING(pfa, clampv(v, 1e-12, 1e-1)); }
void Pipeline::set_cfar_kind(CfarKind k)     { SET_PENDING(cfar_kind, k); }
void Pipeline::set_aoa_method(AoaMethod m)   { SET_PENDING(aoa, m); }
void Pipeline::set_zero_dopp_blank(int b)    { SET_PENDING(zero_dopp_blank, clampv(b, 0, 32)); }
#undef SET_PENDING

void Pipeline::set_dynamic_range_db(double d) { p_->dyn_range.store(clampv(d, 10.0, 120.0)); }
void Pipeline::freeze(bool on)                { p_->frozen.store(on); }
bool Pipeline::frozen() const                 { return p_->frozen.load(); }

//-- Calibration -------------------------------------------------------------
bool Pipeline::calibrate_range_zero(std::string& msg) {
    RdFrame f;
    if (!latest(f)) { msg = "no frame yet -- start the radar first"; return false; }

    // Collapse the map onto range at zero Doppler, where the leakage lives.
    std::vector<float> profile(std::size_t(f.n_range), 0.0f);
    const int zc = f.n_doppler / 2;
    for (int r = 0; r < f.n_range; ++r) {
        float best = 0;
        for (int d = std::max(0, zc - 2); d <= std::min(f.n_doppler - 1, zc + 2); ++d)
            best = std::max(best, f.at(r, d));
        profile[std::size_t(r)] = best;
    }
    const int bin = p_->cal.solve_range_zero(profile.data(), f.n_range);
    if (bin < 0) {
        msg = "no transmit-leakage peak stands far enough above the profile to be "
              "trusted as the range origin -- is the transmitter on?";
        return false;
    }
    char buf[160];
    std::snprintf(buf, sizeof(buf), "range origin set to bin %d%+.2f (%.2f m of cable and "
                  "converter delay removed)", bin, p_->cal.range_zero_frac(),
                  p_->wf.range_of_bin(bin));
    msg = buf;
    return true;
}

bool Pipeline::calibrate_boresight(std::string& msg) {
    RdFrame f;
    if (!latest(f) || f.hits.empty()) {
        msg = "no detections in the last frame -- put a reference reflector on boresight";
        return false;
    }
    const Hit* best = &f.hits[0];
    for (const auto& h : f.hits) if (h.power > best->power) best = &h;
    if (best->snr_db < 20.0) {
        char buf[160];
        std::snprintf(buf, sizeof(buf), "the strongest return is only %.1f dB above the noise; "
                      "calibrating on it would bake that noise into every angle. Want 20 dB.",
                      best->snr_db);
        msg = buf;
        return false;
    }
    p_->cal.solve_boresight(best->virt);
    char buf[160];
    std::snprintf(buf, sizeof(buf), "boresight calibrated on a %.1f dB return at %.1f m",
                  best->snr_db, best->range_m);
    msg = buf;
    return true;
}

bool Pipeline::calibrate_field_point(double az, double el, std::string& msg) {
    RdFrame f;
    if (!latest(f) || f.hits.empty()) { msg = "no detections to calibrate on"; return false; }
    const Hit* best = &f.hits[0];
    for (const auto& h : f.hits) if (h.power > best->power) best = &h;
    p_->cal.add_field_point(az, el, best->virt);
    const bool solved = p_->cal.solve_field();
    char buf[192];
    std::snprintf(buf, sizeof(buf), "field point %zu recorded at (%.0f, %.0f) degrees%s",
                  p_->cal.field_point_count(), az, el,
                  solved ? "; the angle-dependent table is now in use"
                         : "; at least three points are needed before it is used");
    msg = buf;
    return true;
}

bool Pipeline::calibration_save(const std::string& path, std::string& msg) {
    std::string err;
    const bool ok = p_->cal.save(path, &err);
    msg = ok ? ("calibration written to " + path) : err;
    return ok;
}

bool Pipeline::calibration_load(const std::string& path, std::string& msg) {
    std::string err;
    const bool ok = p_->cal.load(path, &err);
    msg = ok ? ("calibration loaded from " + path) : err;
    return ok;
}

const Calibration& Pipeline::calibration() const { return p_->cal; }

//-- Recording ---------------------------------------------------------------
bool Pipeline::record_start(const std::string& path, u64 max_bytes, std::string& err) {
    p_->rec = std::make_unique<FileRecorder>();
    p_->rec->set_max_bytes(max_bytes);
    if (!p_->rec->open(p_->cfg, path)) {
        err = "could not open " + path + " for recording";
        p_->rec.reset();
        return false;
    }
    p_->rec_cap = max_bytes;
    return true;
}

void Pipeline::record_stop() {
    if (p_->rec) { p_->rec->close(); p_->rec.reset(); }
}

bool Pipeline::recording() const     { return bool(p_->rec); }
u64  Pipeline::recorded_bytes() const { return p_->rec ? p_->rec->bytes() : 0; }

bool Pipeline::latest(RdFrame& out) const {
    std::lock_guard<std::mutex> lk(p_->fm);
    if (!p_->have_frame) return false;
    out.allocate(p_->last_frame.n_range, p_->last_frame.n_doppler, p_->last_frame.n_virt, false);
    std::memcpy(out.power.data(), p_->last_frame.power.data(),
                p_->last_frame.power.size() * sizeof(float));
    out.index       = p_->last_frame.index;
    out.hits        = p_->last_frame.hits;
    out.targets     = p_->last_frame.targets;
    out.noise_floor = p_->last_frame.noise_floor;
    return true;
}

} // namespace radar
