//============================================================================
// microdoppler.cpp -- see microdoppler.hpp
//============================================================================
#include "radar/microdoppler.hpp"
#include "radar/track.hpp"   // resolution_scales

#include <algorithm>
#include <cmath>

namespace radar {

const char* MicroDoppler::kRotorcraft = "rotorcraft";
const char* MicroDoppler::kPerson     = "person";
const char* MicroDoppler::kVehicle    = "vehicle";
const char* MicroDoppler::kUnknown    = "unknown";

namespace {

constexpr int W = MicroDoppler::kWin;
constexpr int H = MicroDoppler::kHop;
constexpr int N = MicroDoppler::kNotch;

/// Bins either side of the body line removed from the short-time frames before
/// their energy is measured.  A Hann window puts a tone that sits on a bin into
/// exactly three bins, so one either side removes the body and nothing else --
/// and nothing else is what is wanted, because on a short window the whole of a
/// modest rotor's skirt may be only a couple of bins wide.
constexpr int kStftNotch = 1;

enum Class { kClsRotor = 0, kClsPerson = 1, kClsVehicle = 2, kClsUnknown = 3 };

//----------------------------------------------------------------------------
// Decision thresholds, in physical units, with where each one comes from.
//
// At 5.8 GHz one metre per second of radial speed is 38.7 Hz of Doppler, so
// every velocity below can be read straight off a spectrum.
//----------------------------------------------------------------------------

/// A rotor has to spread the return by at least this much velocity before the
/// spread is put down to rotating parts.  A five-inch propeller at eight
/// thousand revolutions a minute has a tip speed of 53 m/s, and even a
/// distant, partly shadowed tip return spreads the echo by tens of metres per
/// second, so 3 m/s sits an order of magnitude below anything a real rotor
/// produces while staying clear of a person.  The rule also demands at least
/// four resolution cells of spread, whichever is larger, so a short record
/// cannot be used to claim a distinction it could not have measured.
constexpr double kRotorSpreadMs   = 3.0;
constexpr double kRotorSpreadBins = 4.0;

/// A rotor's blades advance and retreat in equal measure, so the skirt
/// straddles the body line.  Perfect symmetry is never measured -- the
/// advancing blade is usually the brighter one, and the airframe shadows one
/// side -- so the test asks only that the weaker side carries more than about
/// half the energy of the stronger.
constexpr double kRotorSymmetry = 0.55;

/// A walking person's torso moves at about 1.4 m/s and the swinging limbs
/// reach two to three times that, so the spread is a couple of metres per
/// second.  Below half a metre per second, or below one resolution cell,
/// nothing is moving independently of the body at all.
constexpr double kPersonSpreadLoMs = 0.5;

/// Fraction of the echo's energy that has to lie outside the body line before
/// the residual is treated as a measurement rather than as window leakage.  A
/// Hann window puts a tone that sits on a bin into exactly three bins, so a
/// perfectly rigid target leaves nothing outside the notch; two per cent is
/// generous cover for a body line that drifted off-bin during the record.
constexpr double kLeakFloor = 0.02;

/// How strongly the per-frame energy has to repeat before the repetition is
/// called a blade rate rather than a coincidence.  Mean-removed noise reaches
/// about 0.28 by chance one time in twenty over a hundred-frame series, so 0.3
/// is roughly a one-in-twenty false blade rate per frame -- which the evidence
/// accumulation then washes out, since chance does not repeat at the same rate
/// interval after interval.
constexpr double kBladeAcMin = 0.30;

/// A periodicity is only called one when at least this many cycles of it fit
/// inside the record.  Two would be the bare minimum for the autocorrelation
/// to have a second peak at all; three leaves enough overlap for that peak to
/// mean something.  This is what makes the answer honest on a short record:
/// with one coherent interval accumulated the lower limit is 187 Hz, so a
/// hundred hertz rotor correctly reports nothing until a second interval
/// arrives.
constexpr double kBladeMinCycles = 3.0;

/// Blade rates outside this band are never looked for, whatever the record
/// length.  Below 5 Hz nothing mechanical is periodic on any timescale a radar
/// frame can hold; above 500 Hz the flashes come faster than a window sliding
/// sixteen samples at a time can sample them.
constexpr double kBladeLoHz = 5.0;
constexpr double kBladeHiHz = 500.0;

/// Evidence forgetting factor.  0.35 gives a memory about three frames long;
/// at sixty frames a second that is fifty milliseconds -- long enough to ride
/// out one bad look, short enough to follow a target that changes what it is
/// doing.
constexpr double kEvidenceAlpha     = 0.35;
constexpr int    kMinFramesForLabel = 3;
constexpr double kMinScoreForLabel  = 0.50;

struct Scratch {
    std::vector<cf32>   rec;      ///< accumulated slow time, copied out of the store
    std::vector<cf32>   sig;      ///< de-trended slow time
    std::vector<cf32>   frames;   ///< windowed short-time frames
    std::vector<cf32>   longbuf;  ///< windowed, zero-padded whole record
    std::vector<double> pw;       ///< short-time power, zero Doppler centred
    std::vector<double> fine;     ///< long-transform power, zero Doppler centred
    std::vector<double> sorted;   ///< for the median noise estimate
    std::vector<double> energy;   ///< residual energy per short-time frame
};

Scratch& scratch() {
    static thread_local Scratch s;
    return s;
}

inline int next_pow2_at_least(int v, int lo, int hi) {
    int n = lo;
    while (n < v && n < hi) n <<= 1;
    return clampv(n, lo, hi);
}

inline double wrap_pi(double a) {
    while (a >  kPi) a -= 2.0 * kPi;
    while (a < -kPi) a += 2.0 * kPi;
    return a;
}

/// Hann coefficient, written once so the two transforms cannot drift apart.
inline float hann(int i, int n) {
    return float(0.5 * (1.0 - std::cos(2.0 * kPi * i / (n - 1))));
}

//----------------------------------------------------------------------------
// Take the bulk motion out of a slow-time record.
//
// The body's own Doppler shift says how fast the object is travelling and
// nothing about what it is.  Worse, it drags the whole pattern around the
// spectrum, so a width or a symmetry measured without removing it is really a
// measurement of the target's speed.
//
// Two terms are removed, not one.  The constant term is the mean Doppler, and
// the estimator for it is the phase of the one-lag autocorrelation -- not an
// approximation to the spectral centroid but exactly it, since the
// autocorrelation and the power spectrum are a transform pair, so the phase at
// lag one is the power-weighted circular mean frequency.  Circular matters,
// because a target near the edge of the unambiguous band has its skirt wrapped
// round to the other side and an ordinary average would put the centroid in
// the middle of nothing.
//
// The linear term is the target's acceleration, and it only matters because
// the record is long.  A drone pulling 10 m/s^2 changes speed by 1.3 m/s over
// 128 ms, which is 50 Hz of Doppler -- six resolution cells of smear on a
// measurement whose whole point is to resolve a couple of cells.  Estimating
// it as the difference between the mean Doppler of the two halves and taking
// it out costs ten lines and buys back the resolution the long record was
// accumulated for.
void detrend_bulk(const cf32* x, int n, std::vector<cf32>& out) {
    auto lag1 = [x](int lo, int hi) {
        cf64 r(0, 0);
        for (int k = lo; k + 1 < hi; ++k) r += cf64(x[k + 1]) * std::conj(cf64(x[k]));
        return (std::abs(r) > 0) ? std::arg(r) : 0.0;
    };
    const int half = n / 2;
    const double d_all = lag1(0, n);
    double rate = 0.0;
    if (half >= 8) {
        const double d1 = lag1(0, half), d2 = lag1(half, n);
        rate = wrap_pi(d2 - d1) / double(half);   // radians per sample, per sample
    }

    out.resize(std::size_t(n));
    const double mid = 0.5 * (n - 1);
    for (int k = 0; k < n; ++k) {
        const double dk = k - mid;
        const double phase = -(d_all * k + 0.5 * rate * dk * dk);
        out[std::size_t(k)] = x[k] * cf32(float(std::cos(phase)), float(std::sin(phase)));
    }
}

} // namespace

//============================================================================
MicroDoppler::MicroDoppler(const Config& cfg)
    : n_long_(next_pow2_at_least(kHistCpi * std::max(cfg.n_chirp * array_geom::n_tx, kWin),
                                 kWin, 65536)),
      fft_stft_(kWin, false),
      fft_long_(n_long_, false) {
    const Scales sc = resolution_scales(cfg);
    lambda_m_ = sc.lambda_m;
    prf_hz_   = sc.prf_slow_hz > 0 ? sc.prf_slow_hz : 1.0;
    t_cpi_s_  = sc.t_cpi_s > 0 ? sc.t_cpi_s : 1.0;
}

void MicroDoppler::forget(u32 id) const {
    std::lock_guard<std::mutex> lk(mu_);
    evidence_.erase(id);
}

void MicroDoppler::reset() const {
    std::lock_guard<std::mutex> lk(mu_);
    evidence_.clear();
}

double MicroDoppler::width_resolution_hz(int n_cpi) const {
    const double t = t_cpi_s_ * std::max(1, n_cpi);
    return (t > 0) ? 1.0 / t : 0.0;
}

void MicroDoppler::blade_band_hz(int n_cpi, double& lo, double& hi) const {
    const double t = t_cpi_s_ * std::max(1, n_cpi);
    lo = std::max(kBladeLoHz, (t > 0) ? kBladeMinCycles / t : kBladeHiHz);
    hi = std::min(kBladeHiHz, 0.5 * prf_hz_ / H);
}

//============================================================================
void MicroDoppler::analyse(const cf32* slow_time, int n, Track& t, double slow_prf_hz) const {
    t.micro_bw_hz = 0.0;
    t.blade_hz    = 0.0;
    if (!slow_time || n < W) {
        t.spectrogram.clear();
        t.spec_time = t.spec_freq = 0;
        return;
    }

    Scratch& s = scratch();
    // The caller may hand over one virtual channel's chirps or both
    // transmitters interleaved, and the two have different sample rates for
    // the same interval.  Deriving the rate from how many samples arrived and
    // how long an interval lasts is right either way and needs no agreement
    // about multiplexing between this stage and the one above it.
    const double prf = (slow_prf_hz > 0) ? slow_prf_hz
                     : (t_cpi_s_ > 0 ? double(n) / t_cpi_s_ : prf_hz_);

    //-- Take a copy of the accumulated record -------------------------------
    int n_cpi_held = 1;
    {
        std::lock_guard<std::mutex> lk(mu_);
        Evidence& ev = evidence_[t.id];
        if (ev.cpi_len != n) { ev.hist.clear(); ev.cpi_len = n; }
        ev.hist.insert(ev.hist.end(), slow_time, slow_time + n);
        const std::size_t cap = std::size_t(kHistCpi) * std::size_t(n);
        if (ev.hist.size() > cap)
            ev.hist.erase(ev.hist.begin(), ev.hist.begin() + (ev.hist.size() - cap));
        const std::size_t take = std::min(ev.hist.size(), std::size_t(n_long_));
        s.rec.assign(ev.hist.end() - std::ptrdiff_t(take), ev.hist.end());
        n_cpi_held = int(ev.hist.size() / std::size_t(n));
    }
    const int rec_n = int(s.rec.size());
    const double t_rec = double(rec_n) / prf;

    //-- Spectrogram of the current interval ---------------------------------
    // The display wants this frame, not the last eight, so the picture comes
    // from the interval that just arrived even though everything measured
    // below uses the whole record.
    {
        detrend_bulk(slow_time, n, s.sig);
        const int nf = (n - W) / H + 1;
        s.frames.resize(std::size_t(nf) * W);
        for (int m = 0; m < nf; ++m) {
            cf32*       dst = s.frames.data() + std::size_t(m) * W;
            const cf32* src = s.sig.data() + std::size_t(m) * H;
            for (int i = 0; i < W; ++i) dst[i] = src[i] * hann(i, W);
        }
        fft_stft_.run(s.frames.data(), nf);

        t.spec_time = nf;
        t.spec_freq = W;
        t.spectrogram.resize(std::size_t(nf) * W);
        double peak = 0.0;
        s.pw.resize(std::size_t(nf) * W);
        for (int m = 0; m < nf; ++m) {
            const cf32* src = s.frames.data() + std::size_t(m) * W;
            double*     dst = s.pw.data() + std::size_t(m) * W;
            for (int k = 0; k < W; ++k) {
                const double p = std::norm(cf64(src[k]));
                dst[(k + W / 2) & (W - 1)] = p;     // zero Doppler to the middle
                peak = std::max(peak, p);
            }
        }
        const double ref = (peak > 0) ? peak : 1.0;
        for (std::size_t i = 0; i < s.pw.size(); ++i)
            t.spectrogram[i] = float(std::max(-60.0, 10.0 * std::log10(s.pw[i] / ref + 1e-300)));
    }

    //-- Everything measured runs on the accumulated record ------------------
    detrend_bulk(s.rec.data(), rec_n, s.sig);

    const double fine_bin  = prf / n_long_;
    const double frame_hz  = prf / H;
    const double ms_per_hz = lambda_m_ / 2.0;

    s.longbuf.assign(std::size_t(n_long_), cf32(0, 0));
    for (int k = 0; k < rec_n && k < n_long_; ++k)
        s.longbuf[std::size_t(k)] = s.sig[std::size_t(k)] * hann(k, rec_n);
    fft_long_.run(s.longbuf.data(), 1);

    s.fine.assign(std::size_t(n_long_), 0.0);
    for (int k = 0; k < n_long_; ++k)
        s.fine[std::size_t((k + n_long_ / 2) & (n_long_ - 1))] =
            std::norm(cf64(s.longbuf[std::size_t(k)]));

    // Take the noise pedestal off before measuring anything.  Thermal noise
    // fills every bin equally, so at 10 dB signal-to-noise ratio a tenth of the
    // energy is spread flat across the band and a width measured on the raw
    // spectrum would report the width of the band.  The median is the right
    // estimator of that pedestal because the target, however wide its skirt,
    // occupies a minority of the bins.
    s.sorted.assign(s.fine.begin(), s.fine.end());
    std::nth_element(s.sorted.begin(), s.sorted.begin() + n_long_ / 2, s.sorted.end());
    const double pedestal = s.sorted[std::size_t(n_long_ / 2)];
    for (int k = 0; k < n_long_; ++k)
        s.fine[std::size_t(k)] = std::max(0.0, s.fine[std::size_t(k)] - pedestal);

    //-- Skirt width and symmetry -------------------------------------------
    const int cf = n_long_ / 2;
    double e_body = 0.0, e_res = 0.0, e_neg = 0.0, e_pos = 0.0;
    for (int k = 0; k < n_long_; ++k) {
        const int off = k - cf;
        const double p = s.fine[std::size_t(k)];
        if (std::abs(off) <= N) { e_body += p; continue; }
        e_res += p;
        if (off < 0) e_neg += p; else e_pos += p;
    }
    const double e_tot      = e_body + e_res;
    const double resid_frac = (e_tot > 0) ? e_res / e_tot : 0.0;

    double v_spread_ms = 0.0, symmetry = 0.0;
    if (resid_frac >= kLeakFloor && e_res > 0.0) {
        // Widen a band outwards from the edge of the body line until it holds
        // ninety-five per cent of what is left, interpolating inside the last
        // bin so the answer is not quantised.  Measured from the notch edge
        // and not from zero, because what is wanted is how far the skirt
        // reaches beyond the body, which is what types.hpp calls the spectral
        // width beyond the bulk motion.
        double acc = 0.0, off95 = double(N);
        const double target = 0.95 * e_res;
        for (int off = N + 1; off <= cf; ++off) {
            double add = 0.0;
            if (cf - off >= 0)     add += s.fine[std::size_t(cf - off)];
            if (cf + off < n_long_) add += s.fine[std::size_t(cf + off)];
            if (acc + add >= target) {
                const double frac = (add > 0) ? (target - acc) / add : 0.0;
                off95 = double(off - 1) + clampv(frac, 0.0, 1.0);
                break;
            }
            acc += add;
            off95 = double(off);
        }
        t.micro_bw_hz = 2.0 * std::max(0.0, off95 - double(N)) * fine_bin;
        v_spread_ms   = t.micro_bw_hz * ms_per_hz;
        symmetry      = std::min(e_neg, e_pos) / std::max(std::max(e_neg, e_pos), 1e-300);
    }

    //-- Blade rate ----------------------------------------------------------
    //
    // A blade sweeping through broadside throws a bright flash, so the energy
    // outside the body line pulses at the rate blades pass.  Autocorrelating
    // that pulse train finds the period without caring what shape the pulse
    // is, which matters because a flash is a spike and a spike puts harmonics
    // all over the spectrum.
    //
    // The alternative would be to read the flash rate off the spacing of the
    // sidebands in the spectrum, which is the same information seen the other
    // way round.  The pulse train is the better route here because the flash
    // is narrow: its energy is spread across dozens of sidebands, each of them
    // weak and each of them sitting on the skirt of a much stronger body line,
    // whereas in time it is a single bright event per blade pass.  It also
    // degrades gracefully -- a rotor whose sidebands are unresolved still has
    // a countable flash rate.
    const int nf_long = (rec_n - W) / H + 1;
    double blade_hz = 0.0;
    if (nf_long >= 8) {
        s.frames.resize(std::size_t(nf_long) * W);
        for (int m = 0; m < nf_long; ++m) {
            cf32*       dst = s.frames.data() + std::size_t(m) * W;
            const cf32* src = s.sig.data() + std::size_t(m) * H;
            for (int i = 0; i < W; ++i) dst[i] = src[i] * hann(i, W);
        }
        fft_stft_.run(s.frames.data(), nf_long);

        s.energy.assign(std::size_t(nf_long), 0.0);
        double mean_e = 0.0, e_out = 0.0, e_all = 0.0;
        for (int m = 0; m < nf_long; ++m) {
            const cf32* src = s.frames.data() + std::size_t(m) * W;
            double e = 0.0, tot = 0.0;
            for (int k = 0; k < W; ++k) {
                const double p = std::norm(cf64(src[k]));
                const int sh = ((k + W / 2) & (W - 1)) - W / 2;   // signed Doppler bin
                tot += p;
                if (std::abs(sh) > kStftNotch) e += p;
            }
            s.energy[std::size_t(m)] = e;
            mean_e += e; e_out += e; e_all += tot;
        }
        mean_e /= nf_long;
        for (int m = 0; m < nf_long; ++m) s.energy[std::size_t(m)] -= mean_e;

        double lo_hz, hi_hz;
        blade_band_hz(n_cpi_held, lo_hz, hi_hz);
        if (t_rec > 0) lo_hz = std::max(lo_hz, kBladeMinCycles / t_rec);

        double ac0 = 0.0;
        for (int m = 0; m < nf_long; ++m)
            ac0 += s.energy[std::size_t(m)] * s.energy[std::size_t(m)];

        if (e_all > 0 && e_out / e_all >= kLeakFloor && ac0 > 0 && lo_hz < hi_hz) {
            const std::vector<double>& e = s.energy;
            auto ac = [&](int lag) {
                double v = 0.0;
                for (int m = 0; m + lag < nf_long; ++m)
                    v += e[std::size_t(m)] * e[std::size_t(m + lag)];
                return v / ac0;
            };
            const int max_lag = nf_long / 2;
            int    best_lag = -1;
            double best_val = kBladeAcMin;
            for (int lag = 2; lag <= max_lag; ++lag) {
                const double f = frame_hz / lag;
                if (f < lo_hz || f > hi_hz) continue;
                const double v = ac(lag);
                // Only a genuine local maximum counts.  The autocorrelation of
                // any decaying series slopes downwards from lag zero, and
                // taking its largest value without this test would always
                // return the shortest lag on offer.
                if (v > best_val && v >= ac(lag - 1) && (lag == max_lag || v >= ac(lag + 1))) {
                    best_val = v; best_lag = lag;
                }
            }
            if (best_lag > 0) {
                double lag_ref = best_lag;
                if (best_lag > 2 && best_lag < max_lag) {
                    const double ym = ac(best_lag - 1), y0 = best_val, yp = ac(best_lag + 1);
                    const double den = ym - 2.0 * y0 + yp;
                    if (std::abs(den) > 1e-12) {
                        const double d = 0.5 * (ym - yp) / den;
                        if (std::abs(d) <= 0.5) lag_ref = best_lag + d;
                    }
                }
                blade_hz = frame_hz / lag_ref;
                if (blade_hz < lo_hz || blade_hz > hi_hz) blade_hz = 0.0;
            }
        }
    }
    t.blade_hz = blade_hz;

    //-- Vote ----------------------------------------------------------------
    // The two spread limits are the larger of a physical speed and a number of
    // resolution cells, so a short record never lets the classifier claim a
    // distinction it could not have measured.
    const double bin_ms     = fine_bin * ms_per_hz;
    const double rotor_min  = std::max(kRotorSpreadMs, kRotorSpreadBins * bin_ms);
    const double person_min = std::max(kPersonSpreadLoMs, bin_ms);

    int vote = kClsUnknown;
    if (v_spread_ms >= rotor_min && symmetry >= kRotorSymmetry && blade_hz > 0.0) {
        vote = kClsRotor;
    } else if (v_spread_ms >= person_min && v_spread_ms < rotor_min
               && symmetry < kRotorSymmetry && blade_hz <= 0.0) {
        vote = kClsPerson;
    } else if (v_spread_ms < person_min && blade_hz <= 0.0) {
        vote = kClsVehicle;
    }

    //-- Accumulate ----------------------------------------------------------
    // One interval is sixteen milliseconds of one aspect angle.  A rotor caught
    // with its blades edge-on for that instant looks rigid, and a person caught
    // mid-stride with both arms down looks like a post.  Averaging the vote
    // over a handful of intervals is what turns three brittle rules into a
    // label that does not flicker.
    double conf = 0.0;
    int    best = kClsUnknown;
    {
        std::lock_guard<std::mutex> lk(mu_);
        Evidence& ev = evidence_[t.id];
        for (int i = 0; i < 4; ++i)
            ev.score[i] = (1.0 - kEvidenceAlpha) * ev.score[i]
                        + kEvidenceAlpha * ((i == vote) ? 1.0 : 0.0);
        ++ev.frames;
        for (int i = 0; i < 4; ++i) if (ev.score[i] > ev.score[best]) best = i;
        conf = ev.score[best];
        if (ev.frames < kMinFramesForLabel) best = -1;
    }

    if (best < 0 || conf < kMinScoreForLabel) {
        t.label      = "";
        t.label_conf = conf;
    } else {
        switch (best) {
            case kClsRotor:   t.label = kRotorcraft; break;
            case kClsPerson:  t.label = kPerson;     break;
            case kClsVehicle: t.label = kVehicle;    break;
            default:          t.label = kUnknown;    break;
        }
        t.label_conf = conf;
    }
}

} // namespace radar
