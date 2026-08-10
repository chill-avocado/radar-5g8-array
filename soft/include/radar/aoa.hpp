//============================================================================
// aoa.hpp -- direction finding from the four virtual channels
//
// Two transmitters and two receivers give four virtual elements arranged on a
// square of side half a wavelength: one pair separated left-to-right, one pair
// separated up-and-down.  Half a wavelength is the largest spacing that leaves
// no ambiguity anywhere in the forward hemisphere, and it is also the smallest
// aperture that can measure an angle at all, so everything here is working at
// the very limit of what four elements can do.  Which is the point: the angle
// accuracy comes from signal-to-noise ratio and from the estimator, not from a
// big array, because there is no big array.
//
// Four methods are provided, in increasing order of cost and of what they can
// do with the data:
//
//   Monopulse  closed form, two phase differences, no search.  Nanoseconds.
//              One target only, and it must be well above the noise.
//   Bartlett   the conventional beam pattern, scanned.  Never fails, never
//              resolves two things inside one beam, and the beam here is
//              sixty degrees wide.
//   Capon      steers a null onto everything except the direction being
//              tested.  Much sharper than Bartlett, but needs a covariance
//              matrix of full rank, so it needs several snapshots.
//   MUSIC      splits the covariance into a signal part and a noise part and
//              looks for directions orthogonal to the noise.  The sharpest of
//              the four and the only one that separates two targets inside one
//              beamwidth, at the price of having to guess how many targets
//              there are.
//
// The array is symmetric about its own centre, and that is worth more here
// than anywhere else: for any arrival direction the mirrored, conjugated
// steering vector is the original one, so mirroring the covariance produces a
// second look at the same scene with different complex amplitudes.  It is
// applied always, and it doubles the effective number of looks at no cost in
// aperture.
//
// Even so, one range-Doppler cell is one snapshot, and one snapshot -- even
// doubled -- is not enough for MUSIC on four elements.  There is no way round
// that: a subspace method needs a noise subspace, a rank-deficient covariance
// does not have one, and the smallest eigenvalues of a rank-one matrix point
// nowhere in particular.  Ask for MUSIC with too few snapshots and this class
// runs Capon instead and records that it did.  See music_min_snapshots().
//============================================================================
#pragma once

// core.hpp uses std::memset in a member that this header instantiates, so the
// declaration has to be in scope before it.
#include <cstring>

#include "radar/config.hpp"
#include "radar/core.hpp"
#include "radar/types.hpp"

#include <array>
#include <atomic>
#include <vector>

namespace radar {

class AoaEngine {
public:
    explicit AoaEngine(const Config& cfg);

    struct Result {
        double az_deg     = 0;
        double el_deg     = 0;
        double quality_db = 0;    ///< peak minus the highest sidelobe outside it
        bool   valid      = false;
        int    n_sources  = 0;    ///< model order the estimator settled on

        /// What actually ran.  Differs from Config::aoa when the covariance
        /// was too rank-poor for the requested method and the estimate fell
        /// back to Bartlett, which always works.
        AoaMethod method   = AoaMethod::Bartlett;
        bool      degraded = false;   ///< true when method != the configured one
    };

    /// One complex sample per virtual channel, index (tx * 2 + rx).
    Result estimate(const std::array<cf32, 4>& v) const;

    /// `n_snap` snapshots laid out as n_snap blocks of four channels.
    /// The covariance is formed here.
    Result estimate(const cf32* snapshots, int n_snap) const;

    /// The angle spectrum from the most recent estimate() **on this thread**,
    /// in dB, azimuth-major: index = i_az * spectrum_el() + i_el.  Empty after
    /// a monopulse estimate, which computes no spectrum -- that is the whole
    /// reason monopulse is cheap.
    const std::vector<float>& last_spectrum() const;
    int spectrum_az() const { return n_az_; }
    int spectrum_el() const { return n_el_; }

    /// Grid axes in degrees, for labelling the display.
    const std::vector<float>& az_axis_deg() const { return az_deg_; }
    const std::vector<float>& el_axis_deg() const { return el_deg_; }

    /// Half-power width of this array's own beam, in degrees, measured from
    /// the real element positions rather than assumed.  The tracker turns this
    /// into an angle measurement variance.
    double beamwidth_az_deg() const { return bw_az_deg_; }
    double beamwidth_el_deg() const { return bw_el_deg_; }

    /// Fewest snapshots MUSIC will run on.
    ///
    /// A subspace method has to separate four eigenvalues into a signal group
    /// and a noise group, and with fewer looks than elements there is no noise
    /// group to find: the covariance is rank deficient, its smallest
    /// eigenvalues are numerically arbitrary, and the pseudospectrum peaks
    /// somewhere unrelated to the target.  Mirroring the array buys a factor of
    /// two, and the usual requirement of about twice as many looks as elements
    /// then lands here.  Ask for MUSIC with fewer and the engine quietly runs
    /// Capon instead and says so in Result::method.
    ///
    /// One range-Doppler cell is one snapshot.  To reach this the pipeline has
    /// to hand estimate() several cells -- the neighbours in range and Doppler
    /// around the detection are the natural choice, since a target that spans
    /// more than one cell gives independent looks at the same direction.
    static int music_min_snapshots() { return 4; }

    /// Multiply each channel by its correction.  Amplitude and phase mismatch
    /// between the four receive paths is indistinguishable from a real angle,
    /// so this has to happen before anything else looks at the data.
    static void apply_calibration(std::array<cf32, 4>& v, const std::array<cf32, 4>& cal);

    /// Given the four channels measured on a target known to be at boresight,
    /// return the corrections that make them identical.  The common gain and
    /// the common phase are left alone -- only the differences between
    /// channels carry angle information, and only the differences are removed.
    static std::array<cf32, 4> solve_boresight_calibration(const std::array<cf32, 4>& measured);

private:
    struct Grid;      ///< precomputed steering products
    struct Work;      ///< per-thread scratch

    Work&  work() const;
    Result run_monopulse(const std::array<cf32, 4>& v) const;
    Result run_spectrum(const cf64 R[4][4], int n_snap_eff, AoaMethod want) const;
    void   warn_once(const char* what) const;

    /// One line on the first degradation only.  A message per detection would
    /// be five hundred lines a frame.
    mutable std::atomic<bool> warned_{false};

    AoaMethod method_;
    int       n_az_, n_el_;
    double    az_span_deg_, el_span_deg_;
    double    az_step_deg_, el_step_deg_;
    double    lambda_m_;
    double    bw_az_deg_ = 0, bw_el_deg_ = 0;

    /// conj(a_i) * a_j for the six unordered pairs, per grid point.  Every one
    /// of the three scanned methods evaluates a Hermitian quadratic form
    /// a^H M a; since every steering entry has unit magnitude that form is
    /// trace(M) plus twice the real part of six products, so six numbers per
    /// grid point serve all three methods and cost six multiply-adds instead
    /// of sixteen.
    std::vector<cf32> gram_;
    std::vector<float> az_deg_, el_deg_;
    std::array<int, 6> pair_i_{{0, 0, 0, 1, 1, 2}};
    std::array<int, 6> pair_j_{{1, 2, 3, 2, 3, 3}};
};

} // namespace radar
