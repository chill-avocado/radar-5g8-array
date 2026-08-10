//============================================================================
// microdoppler.hpp -- telling a drone from a person from a car
//
// Everything that moves produces a Doppler shift.  Everything that moves while
// also having parts that move produces a smear around that shift, and the
// shape of the smear is a fingerprint of the mechanism.  A quadcopter's rotors
// turn at eight thousand revolutions a minute with blade tips going fifty
// metres a second in both directions at once, so the body's single Doppler
// line sits in the middle of a wide, two-sided skirt, with a bright flash every
// time a blade sweeps through broadside.  A walking person's limbs swing
// forwards faster than they swing back, giving a narrower, lopsided smear.  A
// car is a rigid box and gives one clean line and almost nothing else.
//
// That is the whole discriminator, and it is written as those three sentences
// rather than as a trained model on purpose.  A rule with a number attached to
// a physical quantity can be argued with, checked against a measurement and
// defended in a report.  It also degrades honestly: when the evidence matches
// none of the three the answer is "unknown", not the nearest neighbour in some
// feature space nobody can inspect.
//
// Two transforms are used, because the questions being asked want opposite
// things from one.  How wide the skirt is, and whether it is lopsided, is a
// question about frequency and wants the longest window available.  When the
// blades flash is a question about time and wants a short sliding window,
// because a flash lasting a millisecond vanishes completely in a transform
// over a whole interval.  So the skirt is measured with one long transform,
// and the flashes with a sixty-four sample window slid across the record.
//
// Neither question fits inside one coherent interval.  Sixteen milliseconds
// buys 62.5 Hz of frequency resolution, which is 1.6 metres per second at this
// carrier -- coarser than the whole of a walking person's limb motion.  And a
// blade flashing a hundred times a second completes 1.6 cycles in that time,
// which no periodicity estimator can honestly call periodic.  So the slow-time
// samples are kept per track across consecutive intervals and the analysis
// runs on the accumulated record.  Eight intervals is 128 ms: 7.8 Hz of
// resolution, 0.2 metres per second, and a dozen cycles of a hundred hertz
// blade line.
//
// Stitching intervals together assumes the samples are one continuous stream,
// which they are while the target stays in the same range bin -- true of the
// slow targets that need the resolution.  A fast target crossing a bin puts a
// phase step in the record, which can only widen the measured skirt, and a
// widened skirt without a blade line reads as "unknown" rather than as a
// confident wrong answer.
//============================================================================
#pragma once

// core.hpp uses std::memset in a member that this header instantiates, so the
// declaration has to be in scope before it.
#include <cstring>

#include "radar/config.hpp"
#include "radar/core.hpp"
#include "radar/fft.hpp"
#include "radar/types.hpp"

#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace radar {

class MicroDoppler {
public:
    explicit MicroDoppler(const Config& cfg);

    /// Analyse the slow-time samples of one target's range bin across a whole
    /// coherent interval and fill in the classification fields of `t`.
    ///
    /// Writes Track::spectrogram, spec_time, spec_freq, micro_bw_hz, blade_hz,
    /// label and label_conf.  Evidence accumulates per track identifier, so
    /// calling this every frame makes the label steadier rather than noisier.
    ///
    /// `slow_prf_hz` is the rate the samples arrive at.  Left at zero it is
    /// taken to be `n` samples spread across one coherent interval, which is
    /// what the pipeline hands over and which is right whether the caller
    /// passes one virtual channel's chirps or both transmitters interleaved.
    ///
    /// Safe to call from several threads at once, including on tracks that
    /// share this object: the evidence store is the only shared state and it
    /// is locked, and radar::Fft::run is const and re-entrant.
    void analyse(const cf32* slow_time, int n, Track& t, double slow_prf_hz = 0.0) const;

    /// Drop the accumulated evidence and slow-time history for a track that
    /// has died, so a reused identifier does not inherit a stale opinion.
    void forget(u32 track_id) const;
    void reset() const;

    //-- what the numbers mean, for the display and the self-test ------------
    double slow_prf_hz()  const { return prf_hz_; }         ///< slow-time sample rate
    double stft_bin_hz()  const { return prf_hz_ / kWin; }  ///< spectrogram bin width
    double stft_rate_hz() const { return prf_hz_ / kHop; }  ///< spectrogram frames per second
    double hz_per_ms()    const { return 2.0 / lambda_m_; } ///< Doppler per metre per second
    int    history_cpi()  const { return kHistCpi; }
    int    long_fft_size() const { return n_long_; }

    /// Frequency resolution of the width measurement once `n_cpi` intervals
    /// have accumulated, and the blade rates detectable from that much record.
    /// blade_lo is what limits the answer early on: three cycles have to fit
    /// inside the record before a periodicity is called one.
    double width_resolution_hz(int n_cpi) const;
    void   blade_band_hz(int n_cpi, double& lo, double& hi) const;

    static constexpr int kWin     = 64;  ///< short-time window, samples
    static constexpr int kHop     = 16;  ///< 75 per cent overlap
    static constexpr int kNotch   = 2;   ///< long-transform bins either side of the body
    static constexpr int kHistCpi = 8;   ///< coherent intervals kept per track

    static const char* kRotorcraft;   ///< "rotorcraft"
    static const char* kPerson;       ///< "person"
    static const char* kVehicle;      ///< "vehicle"
    static const char* kUnknown;      ///< "unknown"

private:
    struct Evidence { int frames = 0; double score[4] = {0, 0, 0, 0}; };

    /// Declared before the transforms because it sizes one of them, and
    /// members are built in declaration order.
    int    n_cpi_;
    Fft    fft_stft_;   ///< 64 point, for the spectrogram and the blade flashes
    Fft    fft_cpi_;    ///< whole interval, for the width and the symmetry
    double lambda_m_;
    double prf_hz_;

    mutable std::mutex                        mu_;
    mutable std::unordered_map<u32, Evidence> evidence_;
};

} // namespace radar
