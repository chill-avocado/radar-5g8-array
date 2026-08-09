//============================================================================
// waveform.hpp -- what is transmitted, and what a bin means
//
// Two jobs.
//
// The first is to reproduce the transmitted chirp exactly as the fabric makes
// it: the same 32-bit phase accumulator, the same sine table, the same
// integers.  The reference model de-chirps against this, and the simulator
// transmits it, so a discrepancy between this and radar_nco.v would move the
// range origin without anything looking wrong.
//
// The second is to own the arithmetic that turns a bin index into a range or a
// velocity, and back.  Every stage downstream asks this class rather than
// working it out again, which is the only way four different places stay in
// agreement about where zero range is.
//============================================================================
#pragma once

#include "radar/config.hpp"
#include "radar/core.hpp"
#include "radar/types.hpp"

#include <vector>

namespace radar {

//----------------------------------------------------------------------------
// The two words the NCO is programmed with.  Free functions because both the
// host's register writes and the reference model need them and neither should
// own the definition.
//
//   freq_start_inc = round(2^32 * (-B/2) / fs)   the sweep starts low
//   freq_slope_inc = round(2^32 * (B/N) / fs)    one step per radio clock
//----------------------------------------------------------------------------
i32 nco_freq_start_inc(const Config& c);
i32 nco_freq_slope_inc(const Config& c);

/// The 4096-entry sine table the NCO addresses with the top 12 bits of phase.
///   table[k] = round(32767 * sin(2*pi*(k + 0.5) / 4096))
/// The half-step offset is what makes the quarter-wave folding exact: with it,
/// table[2047-k] == table[k] and table[k+2048] == -table[k], so the fabric
/// stores 1024 entries and derives the rest with an index flip and a negate.
const std::vector<i16>& nco_sine_table();

class Waveform {
public:
    explicit Waveform(const Config& c);

    /// One sweep, exactly the integers the fabric emits.
    const std::vector<ci16>& chirp_q15() const { return q15_; }
    /// The same samples as floats in [-1, 1), for the float reference path.
    const std::vector<cf32>& chirp_float() const { return f32_; }

    i32 freq_start_inc() const { return freq_start_; }
    i32 freq_slope_inc() const { return freq_slope_; }

    /// Which transmitter radiates on chirp k.  0 or 1 for the time-multiplexed
    /// and single-transmitter modes; -1 in Doppler-division, where both
    /// transmit on every chirp and are separated afterwards.
    int tx_for_chirp(int k) const;

    /// The Doppler-division sign applied to transmitter `tx` on chirp `k`:
    /// +1 everywhere except transmitter 1 on odd chirps, which is inverted so
    /// its echo lands half a Doppler band away from transmitter 0's.
    int ddm_sign(int k, int tx) const;

    /// Beat frequency a target produces per metre of range, 2 * slope / c.
    double beat_hz_per_metre() const;

    /// Range at the centre of a range bin, with the transmit-leakage bin
    /// treated as zero range.
    double range_of_bin(int bin) const;

    /// Velocity at a signed-centred Doppler bin; positive is approaching.
    double velocity_of_bin(int dopp_bin) const;

    /// Nearest range bin to a range, inverse of range_of_bin().
    int bin_of_range(double m) const;

    const Config& config() const { return cfg_; }

private:
    Config            cfg_;
    i32               freq_start_ = 0;
    i32               freq_slope_ = 0;
    std::vector<ci16> q15_;
    std::vector<cf32> f32_;
};

//----------------------------------------------------------------------------
// Range-Doppler coupling.
//
// A linear-FM sweep cannot tell a delay from a frequency offset: a moving
// target's Doppler shift adds to its beat frequency and the transform reads
// the sum as range.  With the de-chirp taken as conjugate(received) times
// reference, the beat is 2*R*mu/c - 2*v*f0/c, so an approaching target reports
// itself closer than it is by v * f0 / mu.
//
// At 5.8 GHz with a 1e12 Hz/s slope that is 5.8 mm per metre per second: a
// 100 m/s target reads 0.58 m short, a quarter of a range bin.  Small, but it
// is a bias rather than a noise, so it is removed rather than tolerated.
//----------------------------------------------------------------------------

/// Apparent range minus true range for a target closing at `v_ms`.
double doppler_range_coupling_m(const Config& c, double v_ms);

/// Correct one measurement in place.
void decouple_range_doppler(const Config& c, double& range_m, double v_ms);

/// Correct every detection in a list, using each one's own velocity.
void decouple_range_doppler(const Config& c, std::vector<Hit>& hits);

} // namespace radar
