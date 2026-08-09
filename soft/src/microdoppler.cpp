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

enum Class { kClsRotor = 0, kClsPerson = 1, kClsVehicle = 2, kClsUnknown = 3 };

//----------------------------------------------------------------------------
// Decision thresholds, in physical units, with where each comes from.
//
// At 5.8 GHz one metre per second of radial speed is 38.7 Hz of Doppler, so
// every velocity below can be read straight off a spectrogram.
//----------------------------------------------------------------------------

/// Above this much velocity spread, only something with rotating parts moving
/// far faster than the body itself can be responsible.  A five-inch propeller
/// at eight thousand revolutions per minute has a tip speed of 53 m/s, and
/// even a heavily attenuated tip return spreads the return by tens of metres
/// per second; 3 m/s is set an order of magnitude below that on purpose, so
/// that a distant or partly shadowed rotor still clears it, while sitting
/// comfortably above anything a person produces.
constexpr double kRotorSpreadMs = 3.0;

/// A rotor's blades advance and retreat in equal measure, so its skirt
/// straddles the body line.  Perfect symmetry is never measured -- the
/// advancing blade is usually the brighter one -- so the test asks only that
/// the weaker side carries more than about half the energy of the stronger.
constexpr double kRotorSymmetry = 0.55;

/// A walking person's torso moves at about 1.4 m/s and the swinging limbs
/// reach two to three times that, so the spread is a few metres per second and
/// no more.  Below 0.5 m/s there is nothing moving independently at all.
constexpr double kPersonSpreadLoMs = 0.5;
constexpr double kPersonSpreadHiMs = 4.0;

/// A rigid body gives one Doppler line.  Wheels do rotate, but they are low,
/// small and usually shadowed by the body, so what comes back is essentially
/// a single line and its window sidelobes.
constexpr double kVehicleSpreadMs = 0.5;

/// How strongly the per-frame energy has to repeat before the repetition is
/// called a blade rate rather than noise.  Correlated noise in a thirteen
/// frame series reaches about 0.28 by chance one time in twenty, so 0.3 is
/// roughly a one-in-twenty false blade rate per frame, which the evidence
/// accumulation below then washes out.
constexpr double kBladeAcMin = 0.30;

/// Blade rates outside this band are not looked for.  Below 5 Hz nothing
/// mechanical is periodic on the timescale of a coherent interval; above
/// 500 Hz the flash rate exceeds what the frame series can carry.
constexpr double kBladeLoHz = 5.0;
constexpr double kBladeHiHz = 500.0;

/// Evidence forgetting factor.  0.35 gives a memory about three frames long;
/// at thirty frames a second that is a tenth of a second, long enough to ride
/// out one bad look and short enough to follow a target that changes what it
/// is doing.
constexpr double kEvidenceAlpha = 0.35;
constexpr int    kMinFramesForLabel = 3;
constexpr double kMinScoreForLabel  = 0.50;

struct Scratch {
    std::vector<cf32>   sig;     ///< de-rotated slow time
    std::vector<cf32>   frames;  ///< windowed frames, ready for the transform
    std::vector<double> pw;      ///< per frame per bin power, shifted
    std::vector<double> avg;     ///< average spectrum
    std::vector<double> energy;  ///< residual energy per frame
};

Scratch& scratch() {
    static thread_local Scratch s;
    return s;
}

} // namespace

//============================================================================
MicroDoppler::MicroDoppler(const Config& cfg) : fft_(kWin, false) {
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
    const int n_frames = (n - W) / H + 1;
    const double bin_hz   = prf_hz_ / W;
    const double frame_hz = prf_hz_ / H;

    //-- Remove the bulk motion ---------------------------------------------
    //
    // The body's own Doppler shift says how fast the object is travelling and
    // nothing about what it is.  Worse, it moves the whole pattern around the
    // spectrum from frame to frame, so any measurement of width or symmetry
    // taken without removing it is really a measurement of the target's speed.
    //
    // The estimator is the phase of the one-lag autocorrelation.  That is not
    // an approximation to the spectral centroid, it is the centroid: the
    // autocorrelation and the power spectrum are a transform pair, so the
    // phase at lag one is exactly the power-weighted circular mean frequency.
    // Doing it circularly matters, because a target near the edge of the
    // unambiguous band has its skirt wrapped around to the other side, and an
    // ordinary average would put the centroid in the middle of nothing.
    cf64 r1(0, 0);
    for (int k = 0; k + 1 < n; ++k) r1 += cf64(slow_time[k + 1]) * std::conj(cf64(slow_time[k]));
    const double dphi = (std::abs(r1) > 0) ? std::arg(r1) : 0.0;

    s.sig.resize(std::size_t(n));
    for (int k = 0; k < n; ++k) {
        const double a = -dphi * k;
        s.sig[std::size_t(k)] = slow_time[k] * cf32(float(std::cos(a)), float(std::sin(a)));
    }

    //-- Short-time transform ------------------------------------------------
    // Hann rather than anything narrower: the bulk line is far stronger than
    // the micro-Doppler skirt being measured, and a rectangular window's
    // sidelobes alone would manufacture a skirt thirteen decibels down where
    // there is none.
    s.frames.resize(std::size_t(n_frames) * W);
    for (int m = 0; m < n_frames; ++m) {
        cf32* dst = s.frames.data() + std::size_t(m) * W;
        const cf32* src = s.sig.data() + std::size_t(m) * H;
        for (int i = 0; i < W; ++i) {
            const float wgt = float(0.5 * (1.0 - std::cos(2.0 * kPi * i / (W - 1))));
            dst[i] = src[i] * wgt;
        }
    }
    fft_.run(s.frames.data(), n_frames);

    // Shift so zero Doppler sits in the middle: bin 32 of each row.
    s.pw.resize(std::size_t(n_frames) * W);
    double peak = 0.0;
    for (int m = 0; m < n_frames; ++m) {
        const cf32* src = s.frames.data() + std::size_t(m) * W;
        double* dst = s.pw.data() + std::size_t(m) * W;
        for (int k = 0; k < W; ++k) {
            const double p = std::norm(cf64(src[k]));
            dst[(k + W / 2) & (W - 1)] = p;
            peak = std::max(peak, p);
        }
    }

    //-- Spectrogram for the display ----------------------------------------
    t.spec_time = n_frames;
    t.spec_freq = W;
    t.spectrogram.resize(std::size_t(n_frames) * W);
    const double ref = (peak > 0) ? peak : 1.0;
    for (std::size_t i = 0; i < s.pw.size(); ++i)
        t.spectrogram[i] = float(std::max(-60.0, 10.0 * std::log10(s.pw[i] / ref + 1e-300)));

    //-- Residual width ------------------------------------------------------
    //
    // The bulk line is now at the centre bin, and a Hann window smears a pure
    // tone over two bins either side, so those five bins are the body and
    // everything else is micro-motion.  Notching exactly the main lobe and no
    // more is what keeps a slow-moving rotor from being thrown away with the
    // body it is bolted to.
    s.avg.assign(std::size_t(W), 0.0);
    for (int m = 0; m < n_frames; ++m)
        for (int k = 0; k < W; ++k) s.avg[std::size_t(k)] += s.pw[std::size_t(m) * W + k];

    const int c = W / 2;
    double e_res = 0.0, e_neg = 0.0, e_pos = 0.0;
    for (int k = 0; k < W; ++k) {
        const int off = k - c;
        if (std::abs(off) <= kNotch) continue;
        const double p = s.avg[std::size_t(k)];
        e_res += p;
        if (off < 0) e_neg += p; else e_pos += p;
    }

    double v_spread_ms = 0.0, symmetry = 0.0;
    if (e_res > 0) {
        // Widen a symmetric band out from the body line until it holds 95 per
        // cent of what is left.  Ninety-five and not a half-power width,
        // because a rotor's energy is in the skirt and the tails, not in a
        // single peak whose height could be measured.
        double acc = 0.0;
        int    k95 = kNotch + 1;
        for (int off = kNotch + 1; off <= c; ++off) {
            if (c - off >= 0)     acc += s.avg[std::size_t(c - off)];
            if (c + off < W)      acc += s.avg[std::size_t(c + off)];
            k95 = off;
            if (acc >= 0.95 * e_res) break;
        }
        t.micro_bw_hz = 2.0 * k95 * bin_hz;
        v_spread_ms   = t.micro_bw_hz * lambda_m_ / 2.0;
        symmetry      = std::min(e_neg, e_pos) / std::max(std::max(e_neg, e_pos), 1e-300);
    }

    //-- Blade rate ----------------------------------------------------------
    //
    // A blade sweeping through broadside throws a bright flash, so the energy
    // outside the body line pulses at the rate blades pass.  Autocorrelating
    // that pulse train finds the period without caring what shape the pulse
    // is, which matters because a flash is a spike and a spike has harmonics
    // all over the spectrum.
    s.energy.resize(std::size_t(n_frames));
    double mean_e = 0.0;
    for (int m = 0; m < n_frames; ++m) {
        double e = 0.0;
        for (int k = 0; k < W; ++k)
            if (std::abs(k - c) > kNotch) e += s.pw[std::size_t(m) * W + k];
        s.energy[std::size_t(m)] = e;
        mean_e += e;
    }
    mean_e /= n_frames;
    for (int m = 0; m < n_frames; ++m) s.energy[std::size_t(m)] -= mean_e;

    double ac0 = 0.0;
    for (int m = 0; m < n_frames; ++m) ac0 += s.energy[std::size_t(m)] * s.energy[std::size_t(m)];

    double blade_hz = 0.0, blade_strength = 0.0;
    if (ac0 > 0 && n_frames >= 6) {
        const int max_lag = n_frames / 2;
        std::vector<double>& e = s.energy;
        auto ac = [&](int lag) {
            double v = 0.0;
            for (int m = 0; m + lag < n_frames; ++m) v += e[std::size_t(m)] * e[std::size_t(m + lag)];
            return v / ac0;
        };
        int    best_lag = -1;
        double best_val = kBladeAcMin;
        for (int lag = 2; lag <= max_lag; ++lag) {
            const double f = frame_hz / lag;
            if (f < kBladeLoHz || f > kBladeHiHz) continue;
            const double v = ac(lag);
            // Only a genuine local maximum counts.  The autocorrelation of any
            // decaying series slopes downwards from lag zero, and taking its
            // largest value without that test would always return lag two.
            if (v > best_val && v >= ac(lag - 1) && (lag == max_lag || v >= ac(lag + 1))) {
                best_val = v; best_lag = lag;
            }
        }
        if (best_lag > 0) {
            // Sub-lag interpolation.  The frame series is short and coarse --
            // thirteen frames five hundred to a second -- so neighbouring lags
            // are 25 Hz apart at a hundred hertz.  Fitting a parabola through
            // the three points recovers most of that.
            double lag_ref = best_lag;
            if (best_lag > 2 && best_lag < max_lag) {
                const double ym = ac(best_lag - 1), y0 = best_val, yp = ac(best_lag + 1);
                const double den = ym - 2.0 * y0 + yp;
                if (std::abs(den) > 1e-12) {
                    const double d = 0.5 * (ym - yp) / den;
                    if (std::abs(d) <= 0.5) lag_ref = best_lag + d;
                }
            }
            blade_hz       = frame_hz / lag_ref;
            blade_strength = best_val;
            if (blade_hz < kBladeLoHz || blade_hz > kBladeHiHz) { blade_hz = 0.0; blade_strength = 0.0; }
        }
    }
    t.blade_hz = blade_hz;

    //-- Vote ----------------------------------------------------------------
    int vote = kClsUnknown;
    if (v_spread_ms >= kRotorSpreadMs && symmetry >= kRotorSymmetry && blade_hz > 0.0) {
        vote = kClsRotor;
    } else if (v_spread_ms >= kPersonSpreadLoMs && v_spread_ms < kPersonSpreadHiMs
               && symmetry < kRotorSymmetry && blade_hz <= 0.0) {
        vote = kClsPerson;
    } else if (v_spread_ms < kVehicleSpreadMs && blade_hz <= 0.0) {
        vote = kClsVehicle;
    }
    (void)blade_strength;

    //-- Accumulate ----------------------------------------------------------
    // One frame is thirty-two milliseconds of one aspect angle.  A rotor
    // pointing its blades edge-on for that instant looks rigid, and a person
    // caught mid-stride with both arms down looks like a post.  Averaging the
    // vote over a handful of frames is what turns three brittle rules into a
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
