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
// Two transforms are used, because the three questions being asked want
// opposite things from one.  How wide the skirt is, and whether it is
// lopsided, is a question about frequency and wants the longest window
// available -- a walking person's limbs are only a metre or two per second
// away from the torso, which is finer than a short window can resolve at this
// pulse rate.  When the blades flash is a question about time and wants a
// short sliding window, because a flash lasting a millisecond vanishes
// completely in a transform over the whole interval.  So the skirt is measured
// on one transform of the entire coherent interval, and the flashes on a
// sixty-four sample window slid across it.
//============================================================================
#pragma once

#include "radar/config.hpp"
#include "radar/core.hpp"
#include "radar/fft.hpp"
#include "radar/types.hpp"

#include <cstring>
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
    /// Safe to call from several threads at once, including on tracks that
    /// share this object: the evidence store is the only shared state and it
    /// is locked, and radar::Fft::run is const and re-entrant.
    void analyse(const cf32* slow_time, int n, Track& t) const;

    /// Drop the accumulated evidence for a track that has died, so a reused
    /// identifier does not inherit a stale opinion.
    void forget(u32 track_id) const;
    void reset() const;

    //-- what the numbers mean, for the display and the self-test ------------
    double slow_prf_hz()  const { return prf_hz_; }         ///< slow-time sample rate
    double stft_bin_hz()  const { return prf_hz_ / kWin; }  ///< spectrogram bin width
    double stft_rate_hz() const { return prf_hz_ / kHop; }  ///< spectrogram frames per second
    double cpi_bin_hz()   const { return prf_hz_ / n_cpi_; }///< width-measurement resolution
    double hz_per_ms()    const { return 2.0 / lambda_m_; } ///< Doppler per metre per second
    int    cpi_fft_size() const { return n_cpi_; }

    static constexpr int kWin   = 64;  ///< short-time window, samples
    static constexpr int kHop   = 16;  ///< 75 per cent overlap
    static constexpr int kNotch = 2;   ///< bins either side of the body line

    static const char* kRotorcraft;   ///< "rotorcraft"
    static const char* kPerson;       ///< "person"
    static const char* kVehicle;      ///< "vehicle"
    static const char* kUnknown;      ///< "unknown"

private:
    struct Evidence { int frames = 0; double score[4] = {0, 0, 0, 0}; };

    Fft    fft_stft_;   ///< 64 point, for the spectrogram and the blade flashes
    Fft    fft_cpi_;    ///< whole interval, for the width and the symmetry
    int    n_cpi_;
    double lambda_m_;
    double prf_hz_;

    mutable std::mutex                        mu_;
    mutable std::unordered_map<u32, Evidence> evidence_;
};

} // namespace radar
