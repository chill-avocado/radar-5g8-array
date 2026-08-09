//============================================================================
// sim_source.cpp -- a scene in front of the radar, sampled as the AD9361 would
//
// This is the file that lets the whole radar be run, demonstrated and regression
// tested on a laptop with no hardware attached.  It is worth stating plainly
// what that means: it does not fake a range-Doppler map.  It builds the
// electromagnetic field arriving at each receive antenna, sample by sample, and
// hands over the twelve-bit numbers the converter would produce.  Everything
// downstream -- de-chirp, decimation, both transforms, detection, angle
// finding, tracking -- then does exactly the work it will do on hardware, and
// if it is wrong here it is wrong there.
//
// WHAT IS MODELLED, AND WHY EACH PIECE EARNS ITS PLACE
//
//   Transmitted waveform      taken from radar::Waveform, the same object the
//                             gateware's register values are computed from, so
//                             the simulator cannot drift away from the hardware.
//
//   Bistatic geometry         the transmit and receive antennas are on two
//                             boards 250 mm apart, so the path is genuinely
//                             two-legged: tau = (R_tx + R_rx)/c.  The virtual
//                             array positions in radar::array_geom are the sums
//                             of the real ones, and this file splits them back
//                             apart to recover where the real elements are.
//
//   Delay, not beat           the received chirp is the transmitted one delayed
//                             and phase shifted.  The beat tone appears only
//                             after the de-chirp downstream, which is the whole
//                             point: if the simulator produced beat tones it
//                             would be testing an assumption instead of the
//                             pipeline.  Sub-sample delay is exact -- see
//                             "TWO WAYS TO DELAY A CHIRP" below.
//
//   Motion inside the interval  each target's geometry is recomputed for every
//                             chirp, so Doppler emerges from the target moving
//                             rather than being added afterwards, and the phase
//                             is also ramped across each individual chirp,
//                             which is what produces the range-Doppler coupling
//                             an up-chirp really has.
//
//   Transmit leakage          a direct path at the measured -41.1 dB isolation,
//                             at the radio's fixed loop delay, so the leakage
//                             peak sits at a constant low range bin exactly as
//                             it will on the bench.
//
//   Ground clutter            Rayleigh scatterers with area-proportional
//                             density and a little wind, so it is strong, at
//                             zero Doppler, and not perfectly cancellable.
//
//   Thermal noise             at the true absolute level, kTB with the B210's
//                             8 dB noise figure, referred to the converter's
//                             full scale for the configured gain.  This is the
//                             number that makes the simulator's signal-to-noise
//                             ratios agree with design/system_budget.py instead
//                             of being decorative.
//
//   Oscillator phase noise    one filtered-noise process shared by transmit and
//                             receive, so that short paths -- the leakage --
//                             partly cancel it and long paths do not, plus an
//                             independent part that never cancels.
//
//   Quantisation              twelve bits, then left-justified into s16, which
//                             is what the AD9361 and the B200 gateware do.
//
// TWO WAYS TO DELAY A CHIRP
//
//   Delaying a sampled waveform by a fraction of a sample is normally done with
//   a windowed-sinc interpolator, and doing it by rounding to the nearest sample
//   would put a 2.4 metre staircase into every range measurement.  Both methods
//   are implemented here:
//
//     Sinc      a bank of 64 fractionally delayed copies of the chirp, each
//               built with a 16-tap Kaiser-windowed sinc.  General: it works on
//               any waveform.
//
//     Exact     for a linear-FM chirp the delay theorem gives the answer in
//               closed form -- a delayed chirp is the same chirp multiplied by a
//               tone at the beat frequency and a constant phase.  No
//               interpolation, so no interpolation error at all, and the shared
//               waveform stays in first-level cache instead of sixty-four
//               separate tables fighting over the last-level cache.  That is
//               what makes real time possible.
//
//   The simulator fits a linear-FM model to whatever Waveform hands it.  If the
//   fit is good it uses Exact; if not it falls back to Sinc automatically.  The
//   two agree to better than -70 dB on the default waveform, which is checked
//   by the self test and is the evidence that the fast path is not a cheat.
//============================================================================

#include <cstring>          // core.hpp uses memset; make sure it is declared

#include "radar/source.hpp"
#include "radar/json.hpp"
#include "radar/log.hpp"
#include "radar/thread.hpp"
#include "radar/waveform.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <condition_variable>
#include <fstream>
#include <mutex>
#include <sstream>
#include <thread>
#include <vector>

namespace radar {
namespace {

//============================================================================
// Constants of the model.  Every one of these is a statement about the
// hardware, not a tuning knob, so they live here and are documented.
//============================================================================

/// Radiated power per transmitter at the reference gain setting.  The B210
/// delivers about +10 dBm at 5.8 GHz near the top of its transmit gain range,
/// and that is the figure design/system_budget.py uses for the passive board.
/// Below the reference the output follows the setting decibel for decibel;
/// above it the amplifier is already in compression and nothing more comes out.
constexpr double kTxPowerDbmAtRefGain = 10.0;
constexpr double kTxRefGainDb         = 70.0;

/// Input power that fills the converter at 0 dB receive gain, referred to the
/// antenna connector.  Each decibel of receive gain moves this down by one.
constexpr double kRxFullScaleDbmAt0Gain = 0.0;

/// Receiver noise figure and the lumped cable, connector, mismatch and
/// processing loss.  Both are the values in design/system_budget.py; changing
/// either here without changing it there breaks the agreement the whole
/// simulator is calibrated against.
constexpr double kNoiseFigureDb = 8.0;
constexpr double kSystemLossDb  = 3.0;

/// Element pattern.  A cos^n power pattern over the forward hemisphere has
/// directivity 2(n+1); the measured 6.1 dBi peak therefore corresponds to
/// n = 1.0369, which is close to the classic patch shape and gives a 96 degree
/// half-power beamwidth in both planes.
const double kPatternExp = std::pow(10.0, array_geom::element_gain_dbi / 10.0) / 2.0 - 1.0;

/// Distance between the transmit board and the receive board.  The mechanical
/// design puts them on a bracket with a settable 220-280 mm lap joint and calls
/// 250 mm the design point.  Taken here as a pure azimuth offset: transmit
/// board to the left of boresight, receive board to the right.  It cancels out
/// of the virtual array by construction, which is exactly why array_geom does
/// not carry it, but it does not cancel out of the propagation delay, so the
/// simulator has to know it.
constexpr double kBoardSeparationM = 0.250;

/// Radio loop delay: transmit filter chain, converter, analogue path, receive
/// filter chain, plus the 250 mm hop across the bracket.  Fixed by the radio,
/// not by the configuration -- the sweep can be reprogrammed all day and this
/// does not move -- which is why the transmit-leakage peak lands in the same
/// range bin every time and can be calibrated out once with REG_RANGE_ZERO.
constexpr double kRadioLoopDelayS = 320e-9;

/// Converter resolution.  The AD9361's ADC is twelve bits; the B200 gateware
/// left-justifies those into the sixteen-bit samples UHD delivers, so the
/// values that come out are multiples of sixteen.
constexpr int kAdcBits = 12;

/// Peak of the deterministic signal, as a fraction of full scale, that the
/// receive gain is backed off to reach when the configured gain would clip.
constexpr double kHeadroom = 0.70;

/// Oscillator model.  A one-pole loop: flat inside the phase-locked loop's
/// bandwidth, falling as 1/f^2 outside it, which is a first-order description
/// of any integrated synthesiser including the AD9361's.  The level is set to
/// hit the AD9361's typical -120 dBc/Hz at 1 MHz offset at 5.8 GHz.
constexpr double kPllLoopBwHz       = 100e3;
constexpr double kPhaseNoiseAt1MDbc = -120.0;
/// The part that is not shared between the transmit and receive conversions --
/// separate dividers, separate buffers, the synthesiser's own broadband floor.
/// This one never cancels, however short the path, so it is what limits how
/// far the transmit leakage can be suppressed.
constexpr double kPhaseNoiseFloorDbc = -145.0;

/// Delay classes for the phase noise, in seconds.  A path's phase noise is
/// suppressed by |2 sin(pi f tau)|, so the suppression changes quickly for
/// short delays and hardly at all for long ones; logarithmic spacing therefore
/// puts the resolution where the physics is.
const double kPhaseClassTau[] = {0.0, 60e-9, 150e-9, 350e-9, 800e-9, 1.8e-6, 4.0e-6, 9.0e-6};
constexpr int kPhaseClasses   = int(sizeof(kPhaseClassTau) / sizeof(kPhaseClassTau[0]));
/// History kept in front of every chirp, comfortably past the longest class
/// (9 us is 553 samples at 61.44 MSps).
constexpr int kPhiPad = 1024;

/// Fractional-delay interpolator, used by the Sinc path.
constexpr int kFracSteps = 64;   ///< sub-sample positions, 1/64 sample = 3.8 cm
constexpr int kSincTaps  = 16;   ///< Kaiser-windowed, beta below

/// Radar height above the ground, for placing clutter.
constexpr double kRadarHeightM = 3.0;

//============================================================================
// Small utilities
//============================================================================

/// xorshift128+, because every scatterer needs its own reproducible stream and
/// std::mt19937 is nineteen times the state and several times the cost.
struct Rng {
    u64 s0 = 0x9E3779B97F4A7C15ull, s1 = 0xBF58476D1CE4E5B9ull;

    explicit Rng(u64 seed = 1) { reseed(seed); }
    void reseed(u64 seed) {
        // splitmix64 to spread a small seed over both words
        u64 z = seed + 0x9E3779B97F4A7C15ull;
        auto mix = [](u64 x) {
            x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
            x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
            return x ^ (x >> 31);
        };
        s0 = mix(z);
        s1 = mix(z + 0x9E3779B97F4A7C15ull);
        if (!s0 && !s1) s0 = 1;
    }
    u64 next() {
        u64 x = s0, y = s1;
        s0 = y;
        x ^= x << 23;
        s1 = x ^ y ^ (x >> 17) ^ (y >> 26);
        return s1 + y;
    }
    /// Uniform in [0, 1).
    double uniform() { return double(next() >> 11) * (1.0 / 9007199254740992.0); }
    /// Uniform in [-1, 1).
    double sym() { return uniform() * 2.0 - 1.0; }
    /// Gaussian, Box-Muller.  Only used off the hot path.
    double gauss() {
        const double u1 = std::max(1e-16, uniform());
        const double u2 = uniform();
        return std::sqrt(-2.0 * std::log(u1)) * std::cos(2.0 * kPi * u2);
    }
};

/// Modified Bessel function of the first kind, order zero, by its series.
/// Converges in a couple of dozen terms for the arguments a Kaiser window uses.
double bessel_i0(double x) {
    double sum = 1.0, term = 1.0;
    const double hx2 = x * x * 0.25;
    for (int k = 1; k < 60; ++k) {
        term *= hx2 / (double(k) * double(k));
        sum += term;
        if (term < sum * 1e-16) break;
    }
    return sum;
}

//============================================================================
// Derived geometry.  Config::derive() fills most of this, but the simulator
// does not depend on it having been called -- it recomputes anything missing
// from the primitive fields, so a hand-built Config still works.
//============================================================================
struct Geom {
    double fs = 0;              ///< radio sample rate, Hz
    double fc = 0;              ///< carrier, Hz
    double t_sweep = 0;         ///< active sweep, s
    double t_pri = 0;           ///< pulse repetition interval, s
    double t_cpi = 0;
    double mu = 0;              ///< chirp slope, Hz/s
    double f_start = 0;         ///< baseband frequency at the start of the sweep
    double lambda = 0;
    int    n_sweep = 0;
    int    n_pri = 0;
    int    n_chirp_total = 0;
    int    n_rx = 0;
    int    n_tx = 0;
    bool   ddm = false;
    bool   tx_on = true;

    void from(const Config& c) {
        fs      = c.sample_rate_hz > 0 ? c.sample_rate_hz : 61.44e6;
        fc      = c.centre_freq_hz;
        n_sweep = c.n_sweep;
        n_pri   = c.n_pri;
        t_sweep = n_sweep / fs;
        t_pri   = n_pri / fs;
        mu      = (c.d.chirp_slope_hz_s > 0) ? c.d.chirp_slope_hz_s : c.sweep_bw_hz / t_sweep;
        lambda  = (c.d.lambda_m > 0) ? c.d.lambda_m : phys::c0 / fc;
        n_rx    = array_geom::n_rx;
        n_tx    = array_geom::n_tx;
        ddm     = (c.mimo == MimoMode::Ddm);
        tx_on   = c.tx_enable;
        n_chirp_total = c.d.n_chirp_total > 0
                            ? c.d.n_chirp_total
                            : (c.mimo == MimoMode::Tdm ? c.n_chirp * n_tx : c.n_chirp);
        t_cpi = n_chirp_total * t_pri;
        // f_start is fitted from the waveform in open(); this is the fallback.
        f_start = -0.5 * c.sweep_bw_hz;
    }
};

//============================================================================
// One scatterer.  Targets, rotor blades and clutter are all the same thing to
// the radio-frequency model, which is the point: there is one propagation path
// implementation and everything goes through it.
//============================================================================
struct Scat {
    double x = 0, y = 0, z = 0;        ///< position at the start of the interval
    double vx = 0, vy = 0, vz = 0;     ///< constant over an interval
    double rcs = 0.01;                 ///< square metres

    // Rotor blade, if this is one.  The blade orbits the parent body.
    bool   blade = false;
    double orb_r = 0;                  ///< tip radius, metres
    double orb_w = 0;                  ///< angular rate, rad/s
    double orb_ph = 0;                 ///< phase at the start of the interval
    double orb_ax = 0, orb_az = 0;     ///< unit vector in the disc plane, x and z parts
    double orb_bx = 0, orb_bz = 0;     ///< the other in-plane unit vector
    double orb_by = 0, orb_ay = 0;
};

/// Per (scatterer, transmitter, receiver) parameters for one chirp.  These are
/// what the sample loop consumes; everything geometric has already happened.
struct ChanParam {
    float  ar = 0, ai = 0;      ///< complex amplitude at the first valid sample
    double rot_r = 1, rot_i = 0;///< per-sample phasor rotation, kept in double
    int    n0 = 0;              ///< first sample the echo has arrived by
    int    cls = 0;             ///< phase-noise delay class
    int    frac = 0;            ///< fractional-delay table index, Sinc path
    int    idel = 0;            ///< integer sample delay, Sinc path
};

enum class DelayMode { Exact, Sinc };

//============================================================================
// A very small fixed worker pool.  Threads are made once, at open(), and then
// woken with a plain function pointer -- no std::function, so no allocation on
// the steady-state path.
//============================================================================
class Pool {
public:
    using TaskFn = void (*)(void* ctx, int begin, int end);

    ~Pool() { stop(); }

    void start(int n) {
        stop();
        n = std::max(1, n);
        stop_ = false;
        for (int i = 0; i < n; ++i) {
            threads_.emplace_back([this, i] {
                rt::set_name("radar-sim");
                rt::set_affinity(i);
                worker(i);
            });
        }
    }

    void stop() {
        if (threads_.empty()) return;
        {
            std::unique_lock<std::mutex> lk(m_);
            stop_ = true;
            ++gen_;
        }
        cv_.notify_all();
        for (auto& t : threads_) if (t.joinable()) t.join();
        threads_.clear();
    }

    int size() const { return int(threads_.size()) + 1; }   // workers plus the caller

    /// Split [0, n) across the workers and this thread, and return when all of
    /// it is done.
    void run(TaskFn fn, void* ctx, int n) {
        const int parts = size();
        if (threads_.empty() || n <= 1) { fn(ctx, 0, n); return; }
        {
            std::unique_lock<std::mutex> lk(m_);
            fn_ = fn; ctx_ = ctx; n_ = n; parts_ = parts;
            done_ = 0;
            ++gen_;
        }
        cv_.notify_all();
        // The calling thread takes the last slice rather than idling.
        run_slice(parts - 1);
        std::unique_lock<std::mutex> lk(m_);
        cv_done_.wait(lk, [this] { return done_ == parts_ - 1; });
        fn_ = nullptr;
    }

private:
    void run_slice(int p) {
        const int b = int((long long)n_ * p / parts_);
        const int e = int((long long)n_ * (p + 1) / parts_);
        if (e > b) fn_(ctx_, b, e);
    }

    void worker(int idx) {
        u64 seen = 0;
        for (;;) {
            {
                std::unique_lock<std::mutex> lk(m_);
                cv_.wait(lk, [&] { return stop_ || gen_ != seen; });
                seen = gen_;
                if (stop_) return;
            }
            run_slice(idx);
            {
                std::unique_lock<std::mutex> lk(m_);
                ++done_;
            }
            cv_done_.notify_one();
        }
    }

    std::vector<std::thread> threads_;
    std::mutex               m_;
    std::condition_variable  cv_, cv_done_;
    bool                     stop_ = false;
    u64                      gen_  = 0;
    TaskFn                   fn_   = nullptr;
    void*                    ctx_  = nullptr;
    int                      n_ = 0, parts_ = 1, done_ = 0;
};

//============================================================================
// The kernel.
//
// acc[n] += p(n) * s[n - shift], where p advances by a fixed rotation each
// sample.  Eight phasors are carried side by side so the inner statement has no
// loop-carried dependency and the compiler can put it in one AVX2 register;
// that is worth roughly a factor of six over the obvious version.
//============================================================================
void accumulate(float* __restrict accr, float* __restrict acci,
                const float* __restrict sr0, const float* __restrict si0,
                int shift, int n0, int n1, float p0r, float p0i,
                double rot_re, double rot_im) {
    if (n1 <= n0) return;

    // Phasors for the first eight samples, and the rotation that advances the
    // whole group by eight.
    float pr[8], pi[8];
    pr[0] = p0r; pi[0] = p0i;
    for (int j = 1; j < 8; ++j) {
        pr[j] = float(pr[j - 1] * rot_re - pi[j - 1] * rot_im);
        pi[j] = float(pr[j - 1] * rot_im + pi[j - 1] * rot_re);
    }
    double r8 = 1.0, i8 = 0.0;
    for (int j = 0; j < 8; ++j) {
        const double t = r8 * rot_re - i8 * rot_im;
        i8 = r8 * rot_im + i8 * rot_re;
        r8 = t;
    }
    const float cr = float(r8), ci = float(i8);

    int n = n0;
    int guard = 0;
    for (; n + 8 <= n1; n += 8) {
        const int m = n - shift;
        for (int j = 0; j < 8; ++j) {
            const float a = sr0[m + j], b = si0[m + j];
            accr[n + j] += pr[j] * a - pi[j] * b;
            acci[n + j] += pr[j] * b + pi[j] * a;
            const float t = pr[j] * cr - pi[j] * ci;
            pi[j]         = pr[j] * ci + pi[j] * cr;
            pr[j]         = t;
        }
        // Single-precision multiplication leaks a little magnitude over a few
        // hundred steps.  One Newton step every sixty-four groups costs nothing
        // and holds it to the last bit.
        if (++guard == 64) {
            guard = 0;
            for (int j = 0; j < 8; ++j) {
                const float m = 1.5f - 0.5f * (pr[j] * pr[j] + pi[j] * pi[j]);
                pr[j] *= m; pi[j] *= m;
            }
        }
    }
    for (int j = 0; n < n1; ++n, ++j) {
        const float a = sr0[n - shift], b = si0[n - shift];
        accr[n] += pr[j] * a - pi[j] * b;
        acci[n] += pr[j] * b + pi[j] * a;
    }
}

//============================================================================
// The simulator
//============================================================================
class SimSource : public IqSource {
public:
    explicit SimSource(SimScene scene) : scene_(std::move(scene)) {}
    ~SimSource() override { close(); }

    bool open(const Config& c) override;
    void close() override;
    bool running() const override { return running_; }
    bool next_raw(IqCpi& out, double timeout_s) override;
    const char* name() const override { return "simulate"; }
    Stats stats() const override;

    // Exposed for the self test and the bench tool.
    double  gain_backoff_db() const { return backoff_db_; }
    double  noise_sigma() const { return noise_sigma_; }
    double  lfm_fit_residual() const { return fit_resid_; }
    int     leakage_bin() const { return leak_bin_; }
    int     scatterer_count() const { return int(scat_.size()); }
    void    set_delay_mode(DelayMode m) { mode_ = m; }
    DelayMode delay_mode() const { return mode_; }
    const Geom& geom() const { return g_; }
    double  last_generate_s() const { return last_gen_s_; }

private:
    struct Slice;                        // per-thread scratch
    static void chirp_task(void* ctx, int b, int e);
    void        do_chirps(int begin, int end);

    void build_scene_scatterers();
    void build_fractional_tables();
    void fit_waveform();
    void set_levels();
    void generate_phase_noise();

    /// Amplitude and delay of one path at one instant.
    void path(const Scat& s, double t, int tx, int rx,
              double& amp, double& tau, double& fd) const;

    SimScene scene_;
    Config   cfg_;
    Geom     g_;
    std::unique_ptr<Waveform> wf_;

    bool   running_ = false;
    u64    index_   = 0;
    double t0_wall_ = 0;
    double t_sim_   = 0;                 ///< simulated time at the start of the next interval
    double last_gen_s_ = 0;
    u64    frames_  = 0;
    double gen_total_s_ = 0;

    DelayMode mode_ = DelayMode::Exact;
    double    fit_resid_ = 0;

    // Waveform, split into real and imaginary planes so the kernel vectorises.
    std::vector<float> wr_, wi_;
    // Fractional-delay bank, Sinc path: [frac][sample], zero padded at the front
    // by kSincTaps so an integer shift can never read before the buffer.
    std::vector<float> tr_, ti_;
    int                tstride_ = 0;

    // Scatterers, and the element positions they see.
    std::vector<Scat> scat_;
    double p_tx_[array_geom::n_tx][3] = {};
    double p_rx_[array_geom::n_rx][3] = {};

    // Levels.
    double amp_scale_    = 1.0;   ///< sqrt(1 / P_fullscale), volts to full-scale units
    double noise_sigma_  = 0.0;   ///< per component, full-scale units
    double leak_amp_     = 0.0;
    double backoff_db_   = 0.0;
    int    leak_bin_     = 0;
    double p_tx_w_       = 0.01;

    // Phase noise for the current interval: one continuous sequence over the
    // whole interval, generated before the parallel section because a filtered
    // noise process is inherently sequential.
    std::vector<float> phi_;
    double             phi_a_ = 0, phi_sigma_ = 0, phi_state_ = 0;
    double             phi_white_ = 0;
    Rng                rng_noise_;

    Pool                pool_;
    std::vector<Slice>  slices_;
    IqCpi*              target_ = nullptr;   ///< interval being filled
};

//----------------------------------------------------------------------------
// Per-thread scratch.  All of it is sized once in open().
//----------------------------------------------------------------------------
struct SimSource::Slice {
    std::vector<float>     accr, acci;      // one class's accumulation
    std::vector<float>     outr, outi;      // the whole receive channel
    std::vector<ChanParam> par;             // scatterers x transmitters
    Rng                    rng;
};

//----------------------------------------------------------------------------
// Waveform fit.  Recover the start frequency and the slope from whatever
// Waveform produced, and measure how far it is from a straight ramp.  A good
// fit unlocks the exact closed-form delay; a poor one falls back to the
// interpolator, and either way the number is reported rather than assumed.
//----------------------------------------------------------------------------
void SimSource::fit_waveform() {
    const auto& c = wf_->chirp_float();
    const int   n = int(std::min<std::size_t>(c.size(), std::size_t(g_.n_sweep)));
    if (n < 8) { mode_ = DelayMode::Sinc; fit_resid_ = 1.0; return; }

    // Unwrap the phase, then least-squares a quadratic in the sample index.
    std::vector<double> ph(std::size_t(n), 0.0);
    double prev = std::atan2(double(c[0].imag()), double(c[0].real()));
    double acc  = prev;
    ph[0]       = acc;
    for (int i = 1; i < n; ++i) {
        const double a = std::atan2(double(c[std::size_t(i)].imag()), double(c[std::size_t(i)].real()));
        double       d = a - prev;
        while (d > kPi)  d -= 2 * kPi;
        while (d < -kPi) d += 2 * kPi;
        acc += d;
        ph[std::size_t(i)] = acc;
        prev = a;
    }
    // Normal equations for ph = a0 + a1*i + a2*i^2.
    double S[3][4] = {};
    for (int i = 0; i < n; ++i) {
        const double x[3] = {1.0, double(i), double(i) * double(i)};
        for (int r = 0; r < 3; ++r) {
            for (int cc = 0; cc < 3; ++cc) S[r][cc] += x[r] * x[cc];
            S[r][3] += x[r] * ph[std::size_t(i)];
        }
    }
    for (int col = 0; col < 3; ++col) {                 // Gaussian elimination
        int piv = col;
        for (int r = col + 1; r < 3; ++r) if (std::fabs(S[r][col]) > std::fabs(S[piv][col])) piv = r;
        for (int k = 0; k < 4; ++k) std::swap(S[col][k], S[piv][k]);
        const double d = S[col][col];
        if (std::fabs(d) < 1e-300) { mode_ = DelayMode::Sinc; fit_resid_ = 1.0; return; }
        for (int k = 0; k < 4; ++k) S[col][k] /= d;
        for (int r = 0; r < 3; ++r) {
            if (r == col) continue;
            const double f = S[r][col];
            for (int k = 0; k < 4; ++k) S[r][k] -= f * S[col][k];
        }
    }
    const double a1 = S[1][3], a2 = S[2][3];
    // phase = 2*pi*(f_start*t + 0.5*mu*t^2) with t = i/fs
    const double mu_fit  = a2 * g_.fs * g_.fs / kPi;
    const double fs_fit  = a1 * g_.fs / (2 * kPi) - 0.5 * mu_fit / g_.fs;

    // Residual: worst deviation from the fitted ramp, in radians, and the
    // amplitude ripple.  Both have to be small for the closed form to hold.
    double worst = 0, amin = 1e30, amax = 0;
    for (int i = 0; i < n; ++i) {
        const double model = S[0][3] + a1 * i + a2 * double(i) * double(i);
        worst              = std::max(worst, std::fabs(ph[std::size_t(i)] - model));
        const double m     = std::hypot(double(c[std::size_t(i)].real()), double(c[std::size_t(i)].imag()));
        amin = std::min(amin, m);
        amax = std::max(amax, m);
    }
    const double ripple = (amax > 0) ? (amax - amin) / amax : 1.0;
    fit_resid_ = std::max(worst, ripple);

    if (fit_resid_ < 0.02) {                 // 20 mrad and 2 % of amplitude
        g_.mu      = mu_fit;
        g_.f_start = fs_fit;
        mode_      = scene_.sinc_delay ? DelayMode::Sinc : DelayMode::Exact;
    } else {
        // Not a straight ramp, so the closed form does not apply.  The
        // interpolator does, and it is used without being asked.
        mode_ = DelayMode::Sinc;
    }
}

//----------------------------------------------------------------------------
// Fractional-delay bank.  kFracSteps copies of the chirp, copy q delayed by
// q/kFracSteps of a sample, each built with a Kaiser-windowed sinc.  beta 8.6
// puts the stopband at -90 dB, well under the twelve-bit converter's floor, so
// the interpolator is not what limits anything.
//----------------------------------------------------------------------------
void SimSource::build_fractional_tables() {
    const auto& c = wf_->chirp_float();
    const int   n = g_.n_sweep;
    tstride_      = n;
    tr_.assign(std::size_t(kFracSteps) * tstride_, 0.0f);
    ti_.assign(std::size_t(kFracSteps) * tstride_, 0.0f);

    const double beta = 8.6;
    const double i0b  = bessel_i0(beta);
    const int    half = kSincTaps / 2 - 1;    ///< where the kernel's peak sits
    std::vector<double> h(std::size_t(kSincTaps), 0.0);

    for (int q = 0; q < kFracSteps; ++q) {
        const double frac = double(q) / kFracSteps;
        double       sum  = 0;
        for (int t = 0; t < kSincTaps; ++t) {
            // A filter with group delay (half + frac) samples: taps are the
            // sinc sampled about that point, tapered by a Kaiser window.
            const double x = double(t) - (double(half) + frac);
            const double s = (std::fabs(x) < 1e-12) ? 1.0 : std::sin(kPi * x) / (kPi * x);
            const double u = 2.0 * double(t) / double(kSincTaps - 1) - 1.0;
            const double w = bessel_i0(beta * std::sqrt(std::max(0.0, 1.0 - u * u))) / i0b;
            h[std::size_t(t)] = s * w;
            sum += h[std::size_t(t)];
        }
        for (int t = 0; t < kSincTaps; ++t) h[std::size_t(t)] /= sum;   // unity at DC

        float* dr = tr_.data() + std::size_t(q) * tstride_;
        float* di = ti_.data() + std::size_t(q) * tstride_;
        // table[m] = sum_t h[t] * s[m + half - t], which is s(m - frac).  The
        // waveform is zero outside the sweep, which is physically right: before
        // the ramp starts the transmitter is in its retrace and radiating
        // nothing at all.
        for (int m = 0; m < n; ++m) {
            double ar = 0, ai = 0;
            for (int t = 0; t < kSincTaps; ++t) {
                const int src = m + half - t;
                if (src < 0 || src >= n) continue;
                ar += h[std::size_t(t)] * double(c[std::size_t(src)].real());
                ai += h[std::size_t(t)] * double(c[std::size_t(src)].imag());
            }
            dr[m] = float(ar);
            di[m] = float(ai);
        }
    }
}

//----------------------------------------------------------------------------
// Levels.  This is the part that makes the simulator's numbers mean something.
//----------------------------------------------------------------------------
void SimSource::set_levels() {
    // Radiated power per transmitter.
    const double pt_dbm = kTxPowerDbmAtRefGain + std::min(0.0, cfg_.tx_gain_db - kTxRefGainDb);
    p_tx_w_ = std::pow(10.0, pt_dbm / 10.0) * 1e-3;
    if (!g_.tx_on) p_tx_w_ = 0.0;

    // Full scale referred to the antenna connector, then volts to full-scale
    // units.  amp_scale_ turns a received power in watts into an amplitude
    // where 1.0 means the converter is at its rail.
    const double fs_dbm = kRxFullScaleDbmAt0Gain - cfg_.rx_gain_db;
    const double p_fs   = std::pow(10.0, fs_dbm / 10.0) * 1e-3;
    amp_scale_          = 1.0 / std::sqrt(p_fs);

    // Thermal noise.  kTB with the receiver's noise figure, over the complex
    // sample rate, split evenly between the two components.
    const double n_tot = phys::k_boltz * phys::T0 * std::pow(10.0, kNoiseFigureDb / 10.0) * g_.fs;
    noise_sigma_       = scene_.noise_on ? std::sqrt(n_tot / (2.0 * p_fs)) : 0.0;

    // Transmit leakage: the measured board isolation, both transmitters, at the
    // radio's loop delay.
    const double p_leak = p_tx_w_ * std::pow(10.0, array_geom::tx_rx_isolation_db / 10.0);
    leak_amp_           = scene_.leakage_on ? std::sqrt(p_leak) * amp_scale_ : 0.0;
    const double bin_hz = g_.fs / cfg_.decim / std::max(1, cfg_.n_range_fft);
    leak_bin_           = int(std::lround(g_.mu * kRadioLoopDelayS / bin_hz));

    // Worst realistic peak: the leakage adds coherently across transmitters,
    // everything else adds in power with a generous crest factor, and the noise
    // needs four standard deviations of room.
    double p_sum = 0;
    for (const auto& s : scat_) {
        double a, tau, fd;
        path(s, 0.0, 0, 0, a, tau, fd);
        p_sum += a * a;
    }
    const double peak = leak_amp_ * g_.n_tx + 3.0 * std::sqrt(p_sum) + 4.0 * noise_sigma_;

    backoff_db_ = 0.0;
    if (peak > kHeadroom && peak > 0) {
        // The converter would clip.  Back the receive gain off until it does
        // not -- which is what the radio's own gain control does on the bench,
        // and it costs nothing in sensitivity because the thermal noise comes
        // down with the signal and still sits well above the twelve-bit floor.
        backoff_db_ = 20.0 * std::log10(peak / kHeadroom);
        const double k = kHeadroom / peak;
        amp_scale_ *= k;
        noise_sigma_ *= k;
        leak_amp_ *= k;
    }

    // Oscillator model.  A one-pole process phi[n] = a phi[n-1] + w[n] has, for
    // offsets well above the loop bandwidth, S_phi(f) = 2 sigma^2 fs / (2 pi f)^2;
    // solve that for sigma at the quoted 1 MHz level.
    phi_a_ = std::exp(-2.0 * kPi * kPllLoopBwHz / g_.fs);
    const double s1m = 2.0 * std::pow(10.0, kPhaseNoiseAt1MDbc / 10.0);        // rad^2/Hz
    phi_sigma_ = std::sqrt(s1m * std::pow(2.0 * kPi * 1e6, 2) / (2.0 * g_.fs));
    phi_white_ = std::sqrt(2.0 * std::pow(10.0, kPhaseNoiseFloorDbc / 10.0) * g_.fs / 2.0);
    if (!scene_.phase_noise_on) { phi_sigma_ = 0; phi_white_ = 0; }

    const double noise_dbfs = noise_sigma_ > 0 ? 20.0 * std::log10(noise_sigma_ * std::sqrt(2.0)) : -999.0;
    const double lsb        = double(1 << (kAdcBits - 1));
    LOG_I("sim: transmit %.1f dBm/port, full scale %.1f dBm (%.1f dB of gain backed off), "
          "noise %.1f dBFS = %.1f converter counts rms, leakage at range bin %d",
          pt_dbm, fs_dbm - backoff_db_, backoff_db_, noise_dbfs,
          noise_sigma_ * std::sqrt(2.0) * lsb, leak_bin_);
}

//----------------------------------------------------------------------------
// One propagation path, at one instant, for one transmit/receive pair.
//
// Returns the amplitude in full-scale units, the two-way delay, and the
// Doppler frequency.  This is the whole radio-frequency model in twenty lines,
// and every scatterer in the scene goes through it.
//----------------------------------------------------------------------------
void SimSource::path(const Scat& s, double t, int tx, int rx,
                     double& amp, double& tau, double& fd) const {
    // Where it is, and how fast.
    double px = s.x + s.vx * t, py = s.y + s.vy * t, pz = s.z + s.vz * t;
    double vx = s.vx, vy = s.vy, vz = s.vz;
    if (s.blade) {
        // A blade orbits its parent in the rotor disc.  Position and velocity
        // both come from the orbit, which is what puts the rotor's sidebands
        // where they belong instead of pasting a modulation on afterwards.
        const double a = s.orb_ph + s.orb_w * t;
        const double ca = std::cos(a), sa = std::sin(a);
        px += s.orb_r * (s.orb_ax * ca + s.orb_bx * sa);
        py += s.orb_r * (s.orb_ay * ca + s.orb_by * sa);
        pz += s.orb_r * (s.orb_az * ca + s.orb_bz * sa);
        const double w = s.orb_r * s.orb_w;
        vx += w * (-s.orb_ax * sa + s.orb_bx * ca);
        vy += w * (-s.orb_ay * sa + s.orb_by * ca);
        vz += w * (-s.orb_az * sa + s.orb_bz * ca);
    }

    const double dtx[3] = {px - p_tx_[tx][0], py - p_tx_[tx][1], pz - p_tx_[tx][2]};
    const double drx[3] = {px - p_rx_[rx][0], py - p_rx_[rx][1], pz - p_rx_[rx][2]};
    const double rt = std::sqrt(dtx[0] * dtx[0] + dtx[1] * dtx[1] + dtx[2] * dtx[2]);
    const double rr = std::sqrt(drx[0] * drx[0] + drx[1] * drx[1] + drx[2] * drx[2]);
    if (rt < 1e-3 || rr < 1e-3) { amp = 0; tau = 0; fd = 0; return; }

    tau = (rt + rr) / phys::c0;

    // Element pattern, applied on both legs.  cos^n of the angle off boresight,
    // zero behind the ground plane.
    const double ct = dtx[1] / rt, cr = drx[1] / rr;      // boresight is +y
    const double gt = ct > 0 ? std::pow(ct, kPatternExp) : 0.0;
    const double gr = cr > 0 ? std::pow(cr, kPatternExp) : 0.0;
    const double g0 = std::pow(10.0, array_geom::element_gain_dbi / 10.0);

    // Bistatic radar equation.
    const double loss = std::pow(10.0, kSystemLossDb / 10.0);
    const double pr   = p_tx_w_ * g0 * gt * g0 * gr * g_.lambda * g_.lambda * s.rcs /
                      (std::pow(4.0 * kPi, 3) * rt * rt * rr * rr * loss);
    amp = std::sqrt(std::max(0.0, pr)) * amp_scale_;

    // Range rate along the bistatic bisector, positive when closing.
    const double rate = (vx * dtx[0] + vy * dtx[1] + vz * dtx[2]) / rt +
                        (vx * drx[0] + vy * drx[1] + vz * drx[2]) / rr;
    fd = -rate / g_.lambda;                                // +ve when approaching
}

//----------------------------------------------------------------------------
// Turn the scene description into scatterers.
//----------------------------------------------------------------------------
void SimSource::build_scene_scatterers() {
    scat_.clear();
    Rng rng(scene_.seed);

    auto place = [&](double range, double az_deg, double el_deg,
                     double speed, double head_deg, double climb, double rcs) {
        Scat s;
        const double az = rad(az_deg), el = rad(el_deg);
        s.x   = range * std::cos(el) * std::sin(az);
        s.y   = range * std::cos(el) * std::cos(az);
        s.z   = range * std::sin(el);
        const double hd = rad(head_deg);
        s.vx  = speed * std::sin(hd);
        s.vy  = speed * std::cos(hd);
        s.vz  = climb;
        s.rcs = rcs;
        return s;
    };

    for (const auto& o : scene_.objects) {
        // The body itself, minus whatever the blades carry.
        const double body_rcs = std::max(1e-9, o.rcs_m2 - o.blades * o.blade_rcs_m2);
        Scat body = place(o.range_m, o.azimuth_deg, o.elevation_deg,
                          o.speed_ms, o.heading_deg, o.climb_ms, body_rcs);

        if (o.micro_spread_ms > 0) {
            // A walking person or a wheeled vehicle is not a point: limbs and
            // wheels move at their own speeds.  Four sub-scatterers with
            // radial-velocity offsets give the body a Doppler width, which is
            // what the classifier downstream keys on.
            const int    n   = 4;
            const double frc = 1.0 / n;
            for (int i = 0; i < n; ++i) {
                Scat sc = body;
                sc.rcs  = body_rcs * frc;
                const double dv = o.micro_spread_ms * rng.gauss();
                // Push the extra velocity along the line of sight.
                const double r = std::sqrt(sc.x * sc.x + sc.y * sc.y + sc.z * sc.z);
                sc.vx -= dv * sc.x / r; sc.vy -= dv * sc.y / r; sc.vz -= dv * sc.z / r;
                scat_.push_back(sc);
            }
        } else {
            scat_.push_back(body);
        }

        // Rotor blades.  The disc is taken as horizontal, which is what a
        // multirotor in level flight has, so the blade velocity is mostly
        // across the line of sight at the sides of the disc and along it at the
        // front and back -- that is what makes the sidebands.
        for (int b = 0; b < o.blades; ++b) {
            Scat sb  = body;
            sb.rcs   = o.blade_rcs_m2;
            sb.blade = true;
            sb.orb_r = o.blade_len_m;
            sb.orb_w = 2.0 * kPi * o.blade_hz;
            sb.orb_ph = 2.0 * kPi * double(b) / std::max(1, o.blades);
            sb.orb_ax = 1; sb.orb_ay = 0; sb.orb_az = 0;      // disc in the x-y plane
            sb.orb_bx = 0; sb.orb_by = 1; sb.orb_bz = 0;
            scat_.push_back(sb);
        }
    }

    // Ground clutter.
    if (scene_.clutter_on && scene_.clutter_count > 0) {
        const double sigma0 = std::pow(10.0, scene_.clutter_sigma0_db / 10.0);
        // Half-power beamwidth of the element pattern, in azimuth.
        const double half   = std::acos(std::pow(0.5, 1.0 / kPatternExp));
        const double theta  = 2.0 * half;
        const double rmax   = std::max(10.0, scene_.clutter_max_m);
        // Draw ranges with density proportional to R, which is what an annulus
        // of constant width contains.  With that density every scatterer stands
        // for the same mean cross section, sigma0 * theta * rmax^2 / (2 N), and
        // the total per range cell comes out right at every range.
        const double mean_rcs = sigma0 * theta * rmax * rmax / (2.0 * scene_.clutter_count);
        for (int i = 0; i < scene_.clutter_count; ++i) {
            const double r  = std::max(5.0, rmax * std::sqrt(rng.uniform()));
            const double az = deg(half) * rng.sym();
            const double el = -deg(std::atan2(kRadarHeightM, r));
            // Rayleigh amplitude is an exponentially distributed power.
            const double rcs = mean_rcs * -std::log(std::max(1e-12, rng.uniform()));
            Scat s = place(r, az, el, 0, 0, 0, rcs);
            // Wind.  Small, random, and different for every scatterer, so the
            // clutter has a Doppler width instead of being one perfectly
            // cancellable line at zero.
            const double w = scene_.clutter_wind_ms;
            s.vx = w * rng.gauss(); s.vy = w * rng.gauss(); s.vz = 0.3 * w * rng.gauss();
            scat_.push_back(s);
        }
    }
}

//----------------------------------------------------------------------------
// Phase noise for one interval.
//----------------------------------------------------------------------------
void SimSource::generate_phase_noise() {
    const std::size_t n = phi_.size();
    if (!n) return;
    if (phi_sigma_ <= 0) { std::fill(phi_.begin(), phi_.end(), 0.0f); return; }
    double st = phi_state_;
    for (std::size_t i = 0; i < n; ++i) {
        // Sum of three uniforms: variance exact, shape close enough to normal
        // that after the range transform's 768-sample coherent sum nothing can
        // tell the difference, and no logarithm on a path that runs a million
        // times per interval.
        const u64 r = rng_noise_.next();
        const double u = (double(int((r >> 0) & 0xFFFF) - 32768) +
                          double(int((r >> 16) & 0xFFFF) - 32768) +
                          double(int((r >> 32) & 0xFFFF) - 32768)) * (1.0 / 32768.0);
        st = phi_a_ * st + phi_sigma_ * u;
        phi_[i] = float(st);
    }
    phi_state_ = st;
}

//----------------------------------------------------------------------------
// open / close
//----------------------------------------------------------------------------
bool SimSource::open(const Config& c) {
    clear_error();
    close();
    cfg_ = c;
    g_.from(c);

    if (g_.n_sweep <= 0 || g_.n_chirp_total <= 0 || g_.n_pri < g_.n_sweep) {
        set_error("simulator: the configuration's chirp geometry is not realisable "
                  "(sweep " + std::to_string(g_.n_sweep) + " samples, interval " +
                  std::to_string(g_.n_pri) + " samples, " +
                  std::to_string(g_.n_chirp_total) + " chirps)");
        return false;
    }

    wf_.reset(new Waveform(c));
    if (int(wf_->chirp_float().size()) < g_.n_sweep) {
        set_error("simulator: the waveform is " + std::to_string(wf_->chirp_float().size()) +
                  " samples but the configuration asks for " + std::to_string(g_.n_sweep));
        wf_.reset();
        return false;
    }

    fit_waveform();

    // Waveform planes for the kernel.
    wr_.resize(std::size_t(g_.n_sweep));
    wi_.resize(std::size_t(g_.n_sweep));
    for (int i = 0; i < g_.n_sweep; ++i) {
        wr_[std::size_t(i)] = wf_->chirp_float()[std::size_t(i)].real();
        wi_[std::size_t(i)] = wf_->chirp_float()[std::size_t(i)].imag();
    }
    build_fractional_tables();

    // Split the virtual array back into real transmit and receive positions.
    // virtual = transmit + receive, so averaging over the other index recovers
    // each one up to the common offset, which is then shared out evenly.
    double cx = 0, cy = 0;
    for (int i = 0; i < array_geom::n_virt; ++i) { cx += array_geom::virt_xy[i][0]; cy += array_geom::virt_xy[i][1]; }
    cx /= array_geom::n_virt; cy /= array_geom::n_virt;
    for (int t = 0; t < g_.n_tx; ++t) {
        double ax = 0, ay = 0;
        for (int r = 0; r < g_.n_rx; ++r) {
            ax += array_geom::virt_xy[t * g_.n_rx + r][0];
            ay += array_geom::virt_xy[t * g_.n_rx + r][1];
        }
        p_tx_[t][0] = ax / g_.n_rx - cx * 0.5 - kBoardSeparationM * 0.5;
        p_tx_[t][1] = 0.0;
        p_tx_[t][2] = ay / g_.n_rx - cy * 0.5;
    }
    for (int r = 0; r < g_.n_rx; ++r) {
        double ax = 0, ay = 0;
        for (int t = 0; t < g_.n_tx; ++t) {
            ax += array_geom::virt_xy[t * g_.n_rx + r][0];
            ay += array_geom::virt_xy[t * g_.n_rx + r][1];
        }
        p_rx_[r][0] = ax / g_.n_tx - cx * 0.5 + kBoardSeparationM * 0.5;
        p_rx_[r][1] = 0.0;
        p_rx_[r][2] = ay / g_.n_tx - cy * 0.5;
    }

    build_scene_scatterers();
    set_levels();

    // Phase noise runs over the whole interval, plus enough history in front of
    // the first chirp for the longest delay class to look back into.
    phi_.assign(std::size_t(g_.n_chirp_total) * g_.n_pri + 2 * kPhiPad, 0.0f);
    rng_noise_.reseed(scene_.seed ^ 0xA5A5A5A5u);
    phi_state_ = 0;

    // Workers.  One per physical core, minus the one the caller is on.
    const int cores = std::max(1, rt::physical_cores());
    pool_.start(std::max(0, cores - 1));
    slices_.clear();
    slices_.resize(std::size_t(pool_.size()));
    const std::size_t npar = scat_.size() * std::size_t(g_.n_tx);
    for (std::size_t i = 0; i < slices_.size(); ++i) {
        slices_[i].accr.assign(std::size_t(g_.n_sweep), 0.0f);
        slices_[i].acci.assign(std::size_t(g_.n_sweep), 0.0f);
        slices_[i].outr.assign(std::size_t(g_.n_sweep), 0.0f);
        slices_[i].outi.assign(std::size_t(g_.n_sweep), 0.0f);
        slices_[i].par.assign(npar + 4, ChanParam{});
        slices_[i].rng.reseed(scene_.seed * 6364136223846793005ull + 1442695040888963407ull + i);
    }

    index_   = 0;
    frames_  = 0;
    t_sim_   = 0;
    gen_total_s_ = 0;
    t0_wall_ = rt::now_s();
    running_ = true;

    LOG_I("sim: %d scatterers, %d chirps of %d samples on %d receivers, "
          "%s delay (waveform fit residual %.4f), %d worker threads",
          int(scat_.size()), g_.n_chirp_total, g_.n_sweep, g_.n_rx,
          mode_ == DelayMode::Exact ? "closed-form" : "windowed-sinc",
          fit_resid_, pool_.size());
    return true;
}

void SimSource::close() {
    if (!running_ && !wf_) return;
    pool_.stop();
    slices_.clear();
    wf_.reset();
    running_ = false;
}

Stats SimSource::stats() const {
    Stats s;
    s.frames    = frames_;
    s.overflows = 0;
    s.dropped   = 0;
    s.bytes_in  = frames_ * u64(g_.n_rx) * u64(g_.n_chirp_total) * u64(g_.n_sweep) * 4;
    const double wall = rt::now_s() - t0_wall_;
    s.frame_rate_hz = wall > 0 ? double(frames_) / wall : 0;
    s.cpu_frac      = wall > 0 ? gen_total_s_ / wall : 0;
    return s;
}

//----------------------------------------------------------------------------
// The interval.
//----------------------------------------------------------------------------
void SimSource::chirp_task(void* ctx, int b, int e) {
    static_cast<SimSource*>(ctx)->do_chirps(b, e);
}

void SimSource::do_chirps(int begin, int end) {
    // Which slice of scratch this thread owns.  The pool hands out contiguous
    // chirp ranges in slice order, so the range start identifies the slice.
    const int parts = pool_.size();
    int       slice = 0;
    for (int p = 0; p < parts; ++p) {
        if (int((long long)g_.n_chirp_total * p / parts) == begin) { slice = p; break; }
    }
    Slice& sc = slices_[std::size_t(slice)];

    const int   ns   = g_.n_sweep;
    const double fs  = g_.fs;
    const double lsb = double(1 << (kAdcBits - 1));
    const int    nsc = int(scat_.size());

    for (int k = begin; k < end; ++k) {
        const double tk = t_sim_ + double(k) * g_.t_pri;
        const std::size_t phi_base = std::size_t(k) * std::size_t(g_.n_pri) + kPhiPad;

        for (int rx = 0; rx < g_.n_rx; ++rx) {
            std::fill(sc.outr.begin(), sc.outr.end(), 0.0f);
            std::fill(sc.outi.begin(), sc.outi.end(), 0.0f);

            //--------------------------------------------------------------
            // Geometry for every path in the scene, once per chirp.  This is
            // where the targets move: tk advances by one interval each chirp,
            // so the two-way delay is recomputed and Doppler falls out of the
            // phase changing from chirp to chirp rather than being imposed.
            //--------------------------------------------------------------
            int np = 0;
            for (int tx = 0; tx < g_.n_tx; ++tx) {
                double sign = 1.0;
                if (g_.ddm) {
                    sign = double(wf_->ddm_sign(k, tx));
                } else {
                    if (wf_->tx_for_chirp(k) != tx) continue;
                }
                if (!g_.tx_on) continue;

                for (int i = 0; i < nsc; ++i) {
                    double amp, tau, fd;
                    path(scat_[std::size_t(i)], tk, tx, rx, amp, tau, fd);
                    if (amp <= 0) continue;
                    const double dsamp = tau * fs;
                    const int    n0    = int(std::ceil(dsamp));
                    if (n0 >= ns) continue;                   // beyond the sweep

                    ChanParam& p = sc.par[std::size_t(np)];
                    p.n0  = std::max(0, n0);
                    p.cls = 0;
                    for (int cc = kPhaseClasses - 1; cc >= 0; --cc)
                        if (tau >= kPhaseClassTau[cc]) { p.cls = cc; break; }

                    // Within-chirp phase ramp.  The delay itself barely changes
                    // over one 50 us sweep -- a 100 m/s target moves the echo
                    // by 0.002 of a sample, which is nothing -- but the carrier
                    // phase turns by 2*pi*fd*T_sweep, and that is the
                    // range-Doppler coupling.  It stops being negligible around
                    // 5 m/s, where the apparent range shift reaches a
                    // hundredth of a range cell; by 100 m/s it is 0.6 m.
                    double fast;      // cycles per second in fast time
                    double ph0;
                    if (mode_ == DelayMode::Exact) {
                        // Delay theorem for a linear ramp: delaying the chirp is
                        // the same as multiplying it by a tone at -mu*tau and a
                        // constant phase.  Exact, not an approximation.
                        fast = fd - g_.mu * tau;
                        ph0  = -2.0 * kPi * (g_.fc + g_.f_start) * tau + kPi * g_.mu * tau * tau;
                        p.frac = 0;
                        p.idel = 0;
                    } else {
                        const int idel = int(std::floor(dsamp));
                        int frac = int(std::lround((dsamp - idel) * kFracSteps));
                        int id   = idel;
                        if (frac >= kFracSteps) { frac -= kFracSteps; ++id; }
                        p.frac = frac;
                        p.idel = id;
                        fast   = fd;
                        ph0    = -2.0 * kPi * g_.fc * tau;
                    }
                    // The phasor is referenced to the first sample it is used
                    // at, so the eight-wide kernel starts in the right place.
                    const double th = ph0 + 2.0 * kPi * fast * double(p.n0) / fs;
                    const double aa = amp * sign;
                    p.ar    = float(aa * std::cos(th));
                    p.ai    = float(aa * std::sin(th));
                    const double w = 2.0 * kPi * fast / fs;
                    p.rot_r = std::cos(w);
                    p.rot_i = std::sin(w);
                    ++np;
                }
            }

            //--------------------------------------------------------------
            // Accumulate, one phase-noise delay class at a time.  Grouping by
            // delay is what lets the oscillator's phase noise be applied with
            // the right amount of range correlation -- almost total
            // cancellation for the leakage, almost none for a distant target --
            // without a separate multiply for every scatterer.
            //--------------------------------------------------------------
            for (int cls = 0; cls < kPhaseClasses; ++cls) {
                bool any = false;
                for (int i = 0; i < np; ++i) if (sc.par[std::size_t(i)].cls == cls) { any = true; break; }
                if (!any) continue;

                std::fill(sc.accr.begin(), sc.accr.end(), 0.0f);
                std::fill(sc.acci.begin(), sc.acci.end(), 0.0f);

                for (int i = 0; i < np; ++i) {
                    const ChanParam& p = sc.par[std::size_t(i)];
                    if (p.cls != cls) continue;
                    const float* sr;
                    const float* si;
                    int shift = 0;
                    if (mode_ == DelayMode::Exact) {
                        sr = wr_.data(); si = wi_.data();
                    } else {
                        const std::size_t off = std::size_t(p.frac) * tstride_;
                        sr    = tr_.data() + off;
                        si    = ti_.data() + off;
                        shift = p.idel;
                    }
                    accumulate(sc.accr.data(), sc.acci.data(), sr, si, shift,
                               p.n0, ns, p.ar, p.ai, p.rot_r, p.rot_i);
                }

                // Range correlation: what survives is phi(t - tau) - phi(t).
                // Both are milliradians, so the first two terms of the
                // exponential are exact to a part in ten million.
                const int dcls = int(std::lround(kPhaseClassTau[cls] * fs));
                for (int n = 0; n < ns; ++n) {
                    const float d  = phi_[phi_base + std::size_t(n) - std::size_t(dcls)] -
                                     phi_[phi_base + std::size_t(n)];
                    const float wr = 1.0f - 0.5f * d * d, wi = d;
                    sc.outr[std::size_t(n)] += sc.accr[std::size_t(n)] * wr - sc.acci[std::size_t(n)] * wi;
                    sc.outi[std::size_t(n)] += sc.accr[std::size_t(n)] * wi + sc.acci[std::size_t(n)] * wr;
                }
            }

            //--------------------------------------------------------------
            // Transmit leakage.  A direct path at the measured board isolation
            // and the radio's own loop delay, in the shortest phase-noise class
            // so that it cancels the oscillator almost completely -- which is
            // exactly the behaviour that decides how big the residual leakage
            // spike is on the bench.
            //--------------------------------------------------------------
            if (leak_amp_ > 0 && g_.tx_on) {
                const double tau = kRadioLoopDelayS;
                const int    n0  = int(std::ceil(tau * fs));
                for (int tx = 0; tx < g_.n_tx; ++tx) {
                    double sign = 1.0;
                    if (g_.ddm) sign = double(wf_->ddm_sign(k, tx));
                    else if (wf_->tx_for_chirp(k) != tx) continue;

                    double fast, ph0;
                    const float* sr;
                    const float* si;
                    int shift = 0;
                    if (mode_ == DelayMode::Exact) {
                        fast = -g_.mu * tau;
                        ph0  = -2.0 * kPi * (g_.fc + g_.f_start) * tau + kPi * g_.mu * tau * tau;
                        sr = wr_.data(); si = wi_.data();
                    } else {
                        int idel = int(std::floor(tau * fs));
                        int frac = int(std::lround((tau * fs - idel) * kFracSteps));
                        if (frac >= kFracSteps) { frac -= kFracSteps; ++idel; }
                        const std::size_t off = std::size_t(frac) * tstride_;
                        sr    = tr_.data() + off;
                        si    = ti_.data() + off;
                        shift = idel;
                        fast  = 0;
                        ph0   = -2.0 * kPi * g_.fc * tau;
                    }
                    // A small fixed phase difference between the two leakage
                    // paths, so they do not add perfectly and the residual is
                    // not artificially small.
                    ph0 += (tx == 1) ? 0.7 : 0.0;
                    const double th = ph0 + 2.0 * kPi * fast * double(n0) / fs;
                    const double w  = 2.0 * kPi * fast / fs;
                    accumulate(sc.outr.data(), sc.outi.data(), sr, si, shift, n0, ns,
                               float(leak_amp_ * sign * std::cos(th)),
                               float(leak_amp_ * sign * std::sin(th)),
                               std::cos(w), std::sin(w));
                }
            }

            //--------------------------------------------------------------
            // Receive-side oscillator noise that never cancels, thermal noise,
            // then twelve-bit conversion.
            //--------------------------------------------------------------
            ci16* dst = target_->chirp(rx, k);
            const float qn = float(noise_sigma_);
            const bool  white = phi_white_ > 0;
            for (int n = 0; n < ns; ++n) {
                float xr = sc.outr[std::size_t(n)], xi = sc.outi[std::size_t(n)];
                if (white) {
                    const u64    r  = sc.rng.next();
                    const double u  = (double(int((r >> 40) & 0xFFFF) - 32768)) * (1.0 / 32768.0);
                    const float  ps = float(phi_white_ * u * 1.7320508f);   // unit-variance uniform
                    const float  t  = xr - xi * ps;
                    xi              = xi + xr * ps;
                    xr              = t;
                }
                if (qn > 0) {
                    const u64 r1 = sc.rng.next(), r2 = sc.rng.next();
                    const float nr = float(int((r1 >> 0) & 0xFFFF) - 32768) +
                                     float(int((r1 >> 16) & 0xFFFF) - 32768) +
                                     float(int((r1 >> 32) & 0xFFFF) - 32768);
                    const float ni = float(int((r2 >> 0) & 0xFFFF) - 32768) +
                                     float(int((r2 >> 16) & 0xFFFF) - 32768) +
                                     float(int((r2 >> 32) & 0xFFFF) - 32768);
                    xr += qn * nr * (1.0f / 32768.0f);
                    xi += qn * ni * (1.0f / 32768.0f);
                }
                // Twelve bits, then left-justified into s16, which is what the
                // converter and the B200 gateware between them produce.
                int qr = int(std::lround(double(xr) * lsb));
                int qi = int(std::lround(double(xi) * lsb));
                qr = qr > 2047 ? 2047 : (qr < -2048 ? -2048 : qr);
                qi = qi > 2047 ? 2047 : (qi < -2048 ? -2048 : qi);
                dst[n] = ci16(i16(qr * 16), i16(qi * 16));
            }
        }
    }
}

bool SimSource::next_raw(IqCpi& out, double timeout_s) {
    clear_error();
    if (!running_) { set_error("simulator: next_raw called before open"); return false; }

    // Pace, if the scene asked for a wall-clock rate.
    if (scene_.play_rate_hz > 0) {
        const double due  = t0_wall_ + double(index_) / scene_.play_rate_hz;
        const double wait = due - rt::now_s();
        if (wait > timeout_s) return false;
        if (wait > 0) std::this_thread::sleep_for(std::chrono::duration<double>(wait));
    }

    out.allocate(g_.n_rx, g_.n_chirp_total, g_.n_sweep);
    out.index       = index_;
    out.timestamp_s = t_sim_;
    out.overflow    = false;

    const double t_begin = rt::now_s();
    generate_phase_noise();
    target_ = &out;
    pool_.run(&SimSource::chirp_task, this, g_.n_chirp_total);
    target_ = nullptr;
    last_gen_s_ = rt::now_s() - t_begin;
    gen_total_s_ += last_gen_s_;

    t_sim_ += g_.t_cpi;
    ++index_;
    ++frames_;
    return true;
}

} // namespace

//============================================================================
// Scene description: defaults and JSON
//============================================================================

SimScene default_scene() {
    SimScene s;
    s.objects.clear();

    SimObject d1;                       // small quadcopter, closing, rotors turning
    d1.label = "quad-173m";
    d1.range_m = 173.0; d1.azimuth_deg = 12.0; d1.elevation_deg = 5.0;
    d1.speed_ms = 18.0; d1.heading_deg = 190.0; d1.climb_ms = -0.5;
    d1.rcs_m2 = 0.01;
    d1.blades = 8; d1.blade_hz = 95.0; d1.blade_len_m = 0.06; d1.blade_rcs_m2 = 4e-4;
    s.objects.push_back(d1);

    SimObject d2;                       // nearer, crossing, no rotor detail
    d2.label = "quad-90m";
    d2.range_m = 90.0; d2.azimuth_deg = -25.0; d2.elevation_deg = 12.0;
    d2.speed_ms = 9.0; d2.heading_deg = 95.0; d2.climb_ms = 1.0;
    d2.rcs_m2 = 0.02;
    s.objects.push_back(d2);

    SimObject d3;                       // fast, far, near boresight
    d3.label = "fixedwing-260m";
    d3.range_m = 260.0; d3.azimuth_deg = 3.0; d3.elevation_deg = -2.0;
    d3.speed_ms = 26.0; d3.heading_deg = 178.0;
    d3.rcs_m2 = 0.03;
    s.objects.push_back(d3);

    SimObject p;                        // a person walking, limbs and all
    p.label = "walker-45m";
    p.range_m = 45.0; p.azimuth_deg = -8.0; p.elevation_deg = -2.0;
    p.speed_ms = 1.4; p.heading_deg = 170.0;
    p.rcs_m2 = 1.0; p.micro_spread_ms = 1.1;
    s.objects.push_back(p);

    SimObject v;                        // a van on a road, crossing
    v.label = "vehicle-120m";
    v.range_m = 120.0; v.azimuth_deg = 30.0; v.elevation_deg = -1.0;
    v.speed_ms = 13.0; v.heading_deg = 70.0;
    v.rcs_m2 = 10.0; v.micro_spread_ms = 0.35;
    s.objects.push_back(v);

    return s;
}

bool parse_scene(const std::string& text, SimScene& out, std::string& err) {
    err.clear();
    std::string jerr;
    const Json j = Json::parse(text, &jerr);
    if (!jerr.empty() || j.is_null()) {
        err = "scene: " + (jerr.empty() ? std::string("the document is empty or not JSON") : jerr);
        return false;
    }
    if (!j.is_object()) { err = "scene: the top level must be a JSON object"; return false; }

    SimScene s;
    s.objects.clear();

    const Json& objs = j["objects"];
    if (objs.is_array()) {
        for (std::size_t i = 0; i < objs.size(); ++i) {
            const Json& o = objs[i];
            SimObject t;
            t.label           = o["label"].str("");
            t.range_m         = o["range_m"].num(t.range_m);
            t.azimuth_deg     = o["azimuth_deg"].num(0.0);
            t.elevation_deg   = o["elevation_deg"].num(0.0);
            t.speed_ms        = o["speed_ms"].num(0.0);
            t.heading_deg     = o["heading_deg"].num(180.0);
            t.climb_ms        = o["climb_ms"].num(0.0);
            t.rcs_m2          = o["rcs_m2"].num(0.01);
            t.blades          = o["blades"].integer(0);
            t.blade_hz        = o["blade_hz"].num(0.0);
            t.blade_len_m     = o["blade_len_m"].num(0.06);
            t.blade_rcs_m2    = o["blade_rcs_m2"].num(4e-4);
            t.micro_spread_ms = o["micro_spread_ms"].num(0.0);
            if (!(t.range_m > 0)) {
                err = "scene: object " + std::to_string(i) + " has a range of " +
                      std::to_string(t.range_m) + " m, which cannot be";
                return false;
            }
            s.objects.push_back(t);
        }
    }

    if (j.has("clutter")) {
        const Json& c        = j["clutter"];
        s.clutter_on         = c["on"].boolean(true);
        s.clutter_count      = c["count"].integer(s.clutter_count);
        s.clutter_max_m      = c["max_range_m"].num(s.clutter_max_m);
        s.clutter_sigma0_db  = c["sigma0_db"].num(s.clutter_sigma0_db);
        s.clutter_wind_ms    = c["wind_ms"].num(s.clutter_wind_ms);
    }
    if (j.has("receiver")) {
        const Json& r     = j["receiver"];
        s.noise_on        = r["noise"].boolean(true);
        s.leakage_on      = r["leakage"].boolean(true);
        s.phase_noise_on  = r["phase_noise"].boolean(true);
        s.sinc_delay      = r["sinc_delay"].boolean(false);
        s.seed            = u32(r["seed"].num(double(s.seed)));
    }
    if (j.has("play_rate_hz")) s.play_rate_hz = j["play_rate_hz"].num(0.0);

    out = std::move(s);
    return true;
}

bool load_scene(const std::string& path, SimScene& out, std::string& err) {
    std::ifstream in(path, std::ios::binary);
    if (!in) { err = "scene: cannot open " + path; return false; }
    std::ostringstream ss;
    ss << in.rdbuf();
    return parse_scene(ss.str(), out, err);
}

std::unique_ptr<IqSource> make_sim_source(const Config& c, const SimScene& scene) {
    (void)c;
    return std::unique_ptr<IqSource>(new SimSource(scene));
}

//============================================================================
// The factory
//============================================================================
std::unique_ptr<IqSource> make_source(const Config& c) {
    switch (c.source) {
        case SourceKind::Uhd:
            return make_uhd_source(c);
        case SourceKind::File:
            return make_file_source(c, c.record_path, FileSourceOptions{});
        case SourceKind::Simulate:
        default: {
            SimScene s = default_scene();
            if (!c.scene_path.empty()) {
                std::string err;
                SimScene    loaded;
                if (load_scene(c.scene_path, loaded, err)) {
                    s = std::move(loaded);
                    LOG_I("sim: scene from %s, %d objects", c.scene_path.c_str(),
                          int(s.objects.size()));
                } else {
                    // A bad scene file is worth complaining about loudly, but it
                    // is not worth refusing to run over: the built-in scene is
                    // always there and the operator can see what went wrong.
                    LOG_E("%s -- falling back to the built-in scene", err.c_str());
                }
            }
            return std::unique_ptr<IqSource>(new SimSource(s));
        }
    }
}

} // namespace radar
