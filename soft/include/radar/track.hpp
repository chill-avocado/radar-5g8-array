//============================================================================
// track.hpp -- following objects from frame to frame
//
// A detection says where something is now.  A track says where something is,
// where it is going, and how sure the radar is about both -- and, crucially,
// that the thing seen this frame is the same thing seen last frame.  Without
// that continuity there is no velocity beyond the instantaneous radial one, no
// way to coast through a frame where the target fades, and nothing to hang a
// classification on, since telling a drone from a bird takes more than one
// look.
//
// The filter is Cartesian with a constant-velocity model, and the measurement
// is polar.  That pairing is deliberate.  Motion is simple in Cartesian
// coordinates: an object flying straight is a straight line, and the process
// noise is honestly isotropic.  Measurement is simple in polar coordinates:
// range, two angles and range rate are what the radar actually observes, each
// with its own independent error.  Neither is simple in the other's frame, so
// the filter keeps both and links them with a Jacobian.
//
// Why an extended filter and not something fancier: the non-linearity here is
// the polar-to-Cartesian map, which is smooth and gently curved over the size
// of one uncertainty ellipsoid.  The linearisation error at 100 m range with a
// one-degree angle error is centimetres.  An unscented filter would cost four
// times as much to buy that back.
//============================================================================
#pragma once

#include "radar/config.hpp"
#include "radar/core.hpp"
#include "radar/types.hpp"

#include <cstring>
#include <vector>

namespace radar {

/// Quantities every stage needs that follow from the waveform.  Config::derive
/// fills these into Config::d; this recomputes any that are still zero, so a
/// stage handed a raw Config still behaves, and a stage handed a derived one
/// uses exactly the pipeline's numbers.
struct Scales {
    double lambda_m     = 0;
    double range_res_m  = 0;   ///< c / (2 B)
    double vel_res_ms   = 0;   ///< lambda / (2 T_cpi)
    double vel_max_ms   = 0;   ///< unambiguous, plus and minus
    double t_cpi_s      = 0;
    double prf_slow_hz  = 0;   ///< slow-time sample rate seen by one channel
};
Scales resolution_scales(const Config& cfg);

class Tracker {
public:
    explicit Tracker(const Config& cfg);

    /// Advance every track by `dt` seconds, associate `z`, and write the live
    /// tracks -- tentative as well as confirmed -- into `out`.
    ///
    /// A Tracker is a sequential object: one instance follows one scene and
    /// must be driven from one thread.  Separate instances are independent.
    void update(const std::vector<Target>& z, double dt, std::vector<Track>& out);

    /// Forget everything, including the identifier counter.  A new session
    /// starts from track one.
    void reset();

    std::size_t n_tracks()    const { return tracks_.size(); }
    std::size_t n_confirmed() const;

    /// Half-power beamwidth of the virtual array, degrees, from the real
    /// element positions.  The angle measurement variance is scaled by it.
    double beamwidth_az_deg() const { return bw_az_deg_; }
    double beamwidth_el_deg() const { return bw_el_deg_; }

private:
    void   predict(Track& t, double dt) const;
    void   measurement_noise(double snr_db, double el_rad, double R[4][4]) const;
    Track  spawn(const Target& m) const;

    Config cfg_;
    Scales sc_;
    double q_accel_;
    double gate_;
    int    confirm_n_, drop_n_;
    double sigma_r_, sigma_rr_;
    double k_phase_x_, k_phase_y_;   ///< radians of phase per unit direction cosine
    double bw_az_deg_, bw_el_deg_;
    double sigma_ang_floor_;         ///< calibration residual, radians
    u32    next_id_ = 1;

    std::vector<Track> tracks_;

    // Association scratch, kept so the hot path never allocates.
    struct Pair { double d2; int trk; int meas; };
    struct Cache {
        double H[4][6]     = {};
        double S_inv[4][4] = {};
        double R[4][4]     = {};
        double nu[4]       = {};
        bool   ok          = false;
    };
    std::vector<Pair>  pairs_;
    std::vector<Cache> cache_;
    std::vector<int>   meas_of_track_, track_of_meas_;
};

} // namespace radar
