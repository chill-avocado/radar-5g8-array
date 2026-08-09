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

enum Class { kClsRotor = 0, kClsPerson = 1, kClsVehicle = 2, kClsUnknown = 3 };

//----------------------------------------------------------------------------
// Decision thresholds, in physical units, with where each one comes from.
//
// At 5.8 GHz one metre per second of radial speed is 38.7 Hz of Doppler, so
// every velocity below can be read straight off a spectrum.
//----------------------------------------------------------------------------

/// A rotor has to spread the return by at least this much velocity before the
/// spread is attributed to rotating parts.  A five-inch propeller at eight
/// thousand revolutions a minute has a tip speed of 53 m/s, and even a
/// distant, partly shadowed tip return spreads the echo by tens of metres per
/// second, so 3 m/s sits an order of magnitude below anything a real rotor
/// produces while staying clear of a person.  The rule below also demands at
/// least four resolution cells of spread, whichever is larger, so a
/// configuration with a coarse transform cannot claim a rotor it could not
/// have seen.
constexpr double kRotorSpreadMs = 3.0;
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
/// generous cover for a body line that has drifted off-bin during the
/// interval.
constexpr double kLeakFloor = 0.02;

/// How strongly the per-frame energy has to repeat before the repetition is
/// called a blade rate rather than a coincidence.  Mean-removed noise in a
/// thirteen-frame series reaches about 0.28 by chance one time in twenty, so
/// 0.3 is roughly a one-in-twenty false blade rate per frame -- which the
/// evidence accumulation then washes out, since chance does not repeat at the
/// same rate frame after frame.
constexpr double kBladeAcMin = 0.30;

/// Blade rates outside this band are not looked for.  Below 5 Hz nothing
/// mechanical is periodic on the timescale of a coherent interval; above
/// 500 Hz the flashes come faster than the sliding window can sample them.
constexpr double kBladeLoHz = 5.0;
constexpr double kBladeHiHz = 500.0;

/// Evidence forgetting factor.  0.35 gives a memory about three frames long;
/// at thirty frames a second that is a tenth of a second -- long enough to
/// ride out one bad look, short enough to follow a target that changes what it
/// is doing.
constexpr double kEvidenceAlpha     = 0.35;
constexpr int    kMinFramesForLabel = 3;
constexpr double kMinScoreForLabel  = 0.50;

struct Scratch {
    std::vector<cf32>   sig;      ///< de-rotated slow time
    std::vector<cf32>   frames;   ///< windowed short-time frames
    std::vector<cf32>   cpi;      ///< windowed, zero-padded whole interval
    std::vector<double> pw;       ///< short-time power, zero Doppler centred
    std::vector<double> fine;     ///< whole-interval power, zero Doppler centred
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

} // namespace

//============================================================================
MicroDoppler::MicroDoppler(const Config& cfg)
    : fft_stft_(kWin, false),
      n_cpi_(next_pow2_at_least(std::max(cfg.n_chirp, kWin), kWin, 65536)) {
    fft_cpi_.plan(n_cpi_, false);
    const Scales sc = resolution_scales(cfg);
    lambda_m_ = sc.lambda_m;
    prf_hz_   = sc.prf_slow_hz > 0 ? sc.prf_slow_hz : 1.0;
}

void MicroDoppler::forget(u32 id) const {
    std::lock_guard<std::mutex> lk(mu_);
    evidence_.erase(id);
}

void MicroDoppler::reset() const {
    std::lock_guard<std::mutex> lk(mu_);
    evidence_.clear();
}

//============================================================================
void MicroDoppler::analyse(const cf32* slow_time, int n, Track& t) const {
    t.micro_bw_hz = 0.0;
    t.blade_hz    = 0.0;
    if (!slow_time || n < W) {
        t.spectrogram.clear();
        t.spec_time = t.spec_freq = 0;
        return;
    }

    Scratch& s = scratch();
    const int    n_frames = (n - W) / H + 1;
    const double stft_bin = prf_hz_ / W;
    const double fine_bin = prf_hz_ / n_cpi_;
    const double frame_hz = prf_hz_ / H;
    const double ms_per_hz = lambda_m_ / 2.0;   // Doppler hertz to metres per second

    //-- Remove the bulk motion ---------------------------------------------
    //
    // The body's own Doppler shift says how fast the object is travelling and
    // nothing about what it is.  Worse, it drags the whole pattern around the
    // spectrum, so a width or a symmetry measured without removing it is
    // really a measurement of the target's speed.
    //
    // The estimator is the phase of the one-lag autocorrelation.  That is not
    // an approximation to the spectral centroid, it is the centroid: the
    // autocorrelation and the power spectrum are a transform pair, so the
    // phase at lag one is exactly the power-weighted circular mean frequency.
    // Circular matters, because a target near the edge of the unambiguous band
    // has its skirt wrapped round to the other side, and an ordinary average
    // would put the centroid in the middle of nothing.
    cf64 r1(0, 0);
    for (int k = 0; k + 1 < n; ++k) r1 += cf64(slow_time[k + 1]) * std::conj(cf64(slow_time[k]));
    const double dphi = (std::abs(r1) > 0) ? std::arg(r1) : 0.0;

    s.sig.resize(std::size_t(n));
    for (int k = 0; k < n; ++k) {
        const double a = -dphi * k;
        s.sig[std::size_t(k)] = slow_time[k] * cf32(float(std::cos(a)), float(std::sin(a)));
    }

    //-- Short-time transform, for the display and for the flashes -----------
    //
    // Hann rather than anything narrower: the body line is far stronger than
    // the skirt being measured, and a rectangular window's own sidelobes,
    // thirteen decibels down, would manufacture a skirt where there is none.
    s.frames.resize(std::size_t(n_frames) * W);
    for (int m = 0; m < n_frames; ++m) {
        cf32*       dst = s.frames.data() + std::size_t(m) * W;
        const cf32* src = s.sig.data() + std::size_t(m) * H;
        for (int i = 0; i < W; ++i)
            dst[i] = src[i] * float(0.5 * (1.0 - std::cos(2.0 * kPi * i / (W - 1))));
    }
    fft_stft_.run(s.frames.data(), n_frames);

    s.pw.resize(std::size_t(n_frames) * W);
    double peak = 0.0;
    for (int m = 0; m < n_frames; ++m) {
        const cf32* src = s.frames.data() + std::size_t(m) * W;
        double*     dst = s.pw.data() + std::size_t(m) * W;
        for (int k = 0; k < W; ++k) {
            const double p = std::norm(cf64(src[k]));
            dst[(k + W / 2) & (W - 1)] = p;    // zero Doppler to the middle
            peak = std::max(peak, p);
        }
    }

    t.spec_time = n_frames;
    t.spec_freq = W;
    t.spectrogram.resize(std::size_t(n_frames) * W);
    const double ref = (peak > 0) ? peak : 1.0;
    for (std::size_t i = 0; i < s.pw.size(); ++i)
        t.spectrogram[i] = float(std::max(-60.0, 10.0 * std::log10(s.pw[i] / ref + 1e-300)));

    //-- Whole-interval transform, for the width and the symmetry ------------
    const int nc = std::min(n, n_cpi_);
    s.cpi.assign(std::size_t(n_cpi_), cf32(0, 0));
    for (int k = 0; k < nc; ++k)
        s.cpi[std::size_t(k)] = s.sig[std::size_t(k)]
                              * float(0.5 * (1.0 - std::cos(2.0 * kPi * k / (nc - 1))));
    fft_cpi_.run(s.cpi.data(), 1);

    s.fine.resize(std::size_t(n_cpi_));
    for (int k = 0; k < n_cpi_; ++k)
        s.fine[std::size_t((k + n_cpi_ / 2) & (n_cpi_ - 1))] = std::norm(cf64(s.cpi[std::size_t(k)]));

    // Take the noise pedestal off before measuring anything.  Thermal noise
    // fills every bin equally, so at 10 dB signal-to-noise ratio a tenth of the
    // energy is spread flat across the band and a width measured on the raw
    // spectrum would report the width of the band.  The median is the right
    // estimator of that pedestal because the target, however wide its skirt,
    // occupies a minority of the bins.
    s.sorted.assign(s.fine.begin(), s.fine.end());
    std::nth_element(s.sorted.begin(), s.sorted.begin() + n_cpi_ / 2, s.sorted.end());
    const double pedestal = s.sorted[std::size_t(n_cpi_ / 2)];
    for (int k = 0; k < n_cpi_; ++k) s.fine[std::size_t(k)] = std::max(0.0, s.fine[std::size_t(k)] - pedestal);

    //-- Skirt width and symmetry -------------------------------------------
    const int cf = n_cpi_ / 2;
    double e_body = 0.0, e_res = 0.0, e_neg = 0.0, e_pos = 0.0;
    for (int k = 0; k < n_cpi_; ++k) {
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
        // ninety-five per cent of what is left, interpolating within the last
        // bin so the answer is not quantised.  Measured from the notch edge
        // rather than from zero, because what is wanted is how far the skirt
        // reaches beyond the body, not how far it reaches from stationary.
        double acc = 0.0, off95 = double(N);
        for (int off = N + 1; off <= cf; ++off) {
            double add = 0.0;
            if (cf - off >= 0)  add += s.fine[std::size_t(cf - off)];
            if (cf + off < n_cpi_) add += s.fine[std::size_t(cf + off)];
            const double target = 0.95 * e_res;
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
    s.energy.resize(std::size_t(n_frames));
    double mean_e = 0.0, e_out = 0.0, e_all = 0.0;
    for (int m = 0; m < n_frames; ++m) {
        double e = 0.0, tot = 0.0;
        for (int k = 0; k < W; ++k) {
            const double p = s.pw[std::size_t(m) * W + k];
            tot += p;
            if (std::abs(k - W / 2) > N) e += p;
        }
        s.energy[std::size_t(m)] = e;
        mean_e += e; e_out += e; e_all += tot;
    }
    mean_e /= n_frames;
    for (int m = 0; m < n_frames; ++m) s.energy[std::size_t(m)] -= mean_e;

    double blade_hz = 0.0;
    const bool worth_looking = (e_all > 0) && (e_out / e_all >= kLeakFloor) && (n_frames >= 6);
    if (worth_looking) {
        double ac0 = 0.0;
        for (int m = 0; m < n_frames; ++m)
            ac0 += s.energy[std::size_t(m)] * s.energy[std::size_t(m)];
        if (ac0 > 0) {
            const std::vector<double>& e = s.energy;
            auto ac = [&](int lag) {
                double v = 0.0;
                for (int m = 0; m + lag < n_frames; ++m)
                    v += e[std::size_t(m)] * e[std::size_t(m + lag)];
                return v / ac0;
            };
            const int max_lag = n_frames / 2;
            int    best_lag = -1;
            double best_val = kBladeAcMin;
            for (int lag = 2; lag <= max_lag; ++lag) {
                const double f = frame_hz / lag;
                if (f < kBladeLoHz || f > kBladeHiHz) continue;
                const double v = ac(lag);
                // Only a genuine local maximum counts.  The autocorrelation of
                // any decaying series slopes downwards from lag zero, and
                // taking its largest value without this test would always
                // return lag two.
                if (v > best_val && v >= ac(lag - 1) && (lag == max_lag || v >= ac(lag + 1))) {
                    best_val = v; best_lag = lag;
                }
            }
            if (best_lag > 0) {
                // Sub-lag interpolation.  Thirteen frames five hundred to the
                // second puts neighbouring lags 25 Hz apart at a hundred hertz,
                // and a parabola through the three points recovers most of it.
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
                if (blade_hz < kBladeLoHz || blade_hz > kBladeHiHz) blade_hz = 0.0;
            }
        }
    }
    t.blade_hz = blade_hz;

    //-- Vote ----------------------------------------------------------------
    // The two spread limits are the larger of a physical speed and a number of
    // resolution cells, so a coarser transform never lets the classifier claim
    // a distinction it could not have measured.
    const double bin_ms    = fine_bin * ms_per_hz;
    const double rotor_min = std::max(kRotorSpreadMs, kRotorSpreadBins * bin_ms);
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
    // One frame is thirty-two milliseconds of one aspect angle.  A rotor
    // caught with its blades edge-on for that instant looks rigid, and a
    // person caught mid-stride with both arms down looks like a post.
    // Averaging the vote over a handful of frames is what turns three brittle
    // rules into a label that does not flicker.
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
