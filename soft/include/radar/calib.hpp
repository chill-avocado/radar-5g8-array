//============================================================================
// calib.hpp -- turning four measured channels into four comparable ones
//
// Three separate errors have to come out before an angle means anything, and
// they are different in kind:
//
//  1. A fixed complex offset per virtual channel.  Cable lengths differ, the
//     two receive paths through the AD9361 differ, and the transmit and
//     receive boards are separate assemblies.  One boresight measurement with
//     a corner reflector removes all of it.
//
//  2. An angle-dependent residual.  The second element of each pair on the
//     built boards is mirrored -- that is what holds the circular polarisation
//     inside 3 dB at half-wave spacing -- but it means the two elements of a
//     pair are not identical radiators.  The simulation campaign measured the
//     leftover: 8.5 degrees of far-field phase difference within +/-20 degrees
//     and 10.5 degrees at +/-45, with 1.2 to 2.1 dB of amplitude difference,
//     once the known 25.844 mm spacing is removed.  That is worth up to 4.5
//     degrees of angle bias at the edge of the field.  It is a fixed,
//     repeatable function of angle, so a single boresight constant does NOT
//     remove it -- a table across the field does.
//
//  3. The range origin.  The transmit leakage arrives at a fixed group delay
//     through the AD9361 and shows up as a strong peak at a low range bin.
//     Because the FPGA generates the chirp and the de-chirp reference from one
//     phase accumulator, that delay is the ONLY unknown in the range scale,
//     and finding the leakage peak once fixes it for good.
//============================================================================
#pragma once

#include "radar/core.hpp"
#include "radar/types.hpp"

#include <array>
#include <string>
#include <vector>

namespace radar {

class Calibration {
public:
    Calibration();

    //-- Application -------------------------------------------------------
    /// Remove the fixed per-channel error. Always safe to call.
    void apply(std::array<cf32, 4>& v) const;

    /// Remove the fixed error and then the angle-dependent residual, using a
    /// first-pass angle estimate to look the residual up. Call apply() first,
    /// estimate the angle, then call this and estimate again; one iteration is
    /// enough because the residual is small compared to the beamwidth.
    void apply_angular(std::array<cf32, 4>& v, double az_deg, double el_deg) const;

    /// The range bin the transmit leakage sits in. Range zero.
    int    range_zero_bin() const { return range_zero_bin_; }
    void   set_range_zero_bin(int b) { range_zero_bin_ = b; }

    /// Sub-bin refinement of the range origin, in bins.
    double range_zero_frac() const { return range_zero_frac_; }

    //-- Measurement -------------------------------------------------------
    /// Fixed offset from one boresight measurement. The correction makes all
    /// four channels equal in amplitude and phase for a boresight target.
    void solve_boresight(const std::array<cf32, 4>& measured);

    /// Add a measurement taken off boresight. Once several are in, solve_field()
    /// builds the interpolation table.
    void add_field_point(double az_deg, double el_deg, const std::array<cf32, 4>& measured);
    void clear_field_points();
    std::size_t field_point_count() const { return points_.size(); }

    /// Build the angle-dependent table from the collected points. Uses natural
    /// neighbour weighting over the scattered points rather than fitting a
    /// polynomial, because the residual is not smooth across the mirrored
    /// element boundary. Returns false if there are too few points (< 3).
    bool solve_field();

    /// Locate the transmit-leakage peak in a range profile and set the range
    /// origin from it, with a quadratic sub-bin fit. `profile` is integrated
    /// power against range bin, zero Doppler. Returns the bin found, or -1 if
    /// no peak stands far enough above the surrounding profile to be trusted.
    int  solve_range_zero(const float* profile, int n_bins, double min_prominence_db = 12.0);

    /// A quick health check on a live frame: are the four channels within a
    /// sane amplitude spread of each other, and is the leakage where it was?
    /// Returns an empty string when healthy, otherwise what looks wrong.
    std::string check(const RdFrame& f) const;

    //-- Persistence -------------------------------------------------------
    bool load(const std::string& path, std::string* err = nullptr);
    bool save(const std::string& path, std::string* err = nullptr) const;
    bool valid() const { return solved_; }

    /// A calibration that changes nothing, for running uncalibrated.
    static Calibration identity();

    const std::array<cf32, 4>& fixed() const { return fixed_; }

private:
    struct FieldPoint {
        double              az_deg = 0, el_deg = 0;
        std::array<cf32, 4> correction{};
    };

    std::array<cf32, 4>     fixed_{};          ///< multiply the raw channel by this
    std::vector<FieldPoint> points_;
    bool                    solved_          = false;
    int                     range_zero_bin_  = 0;
    double                  range_zero_frac_ = 0.0;
    double                  leak_power_      = 0.0;  ///< for the health check
};

} // namespace radar
