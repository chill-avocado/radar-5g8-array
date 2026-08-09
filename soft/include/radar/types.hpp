//============================================================================
// types.hpp -- the data that moves between pipeline stages
//
// One frame per coherent processing interval.  Everything the host produces
// hangs off RdFrame, so a stage can be tested by handing it a frame and
// looking at what comes back, with no pipeline around it.
//============================================================================
#pragma once

#include "radar/core.hpp"
#include <array>
#include <vector>

namespace radar {

/// One CFAR detection, before angle and clustering.
struct Hit {
    int    range_bin   = 0;
    int    dopp_bin    = 0;      ///< signed-centred: 0 is zero Doppler
    double range_m     = 0;
    double velocity_ms = 0;      ///< positive = approaching
    double power       = 0;      ///< integrated over the virtual channels
    double snr_db      = 0;
    std::array<cf32, 4> virt{};  ///< calibrated virtual-channel samples
    // Filled by the angle stage.
    double azimuth_deg   = 0;
    double elevation_deg = 0;
    bool   angle_valid   = false;
    double angle_quality = 0;    ///< peak-to-second-peak of the spectrum, dB
};

/// A cluster of hits treated as one physical object.
struct Target {
    double range_m       = 0;
    double velocity_ms   = 0;
    double azimuth_deg   = 0;
    double elevation_deg = 0;
    double snr_db        = 0;
    double x = 0, y = 0, z = 0;  ///< metres, radar boresight = +y, right = +x
    int    n_hits        = 0;
    double extent_m      = 0;    ///< range spread of the cluster
    double dopp_spread_ms = 0;   ///< Doppler spread -- large for rotors
};

/// Tracker state.  Cartesian constant-velocity with a polar measurement.
struct Track {
    u32    id       = 0;
    double x = 0, y = 0, z = 0;
    double vx = 0, vy = 0, vz = 0;
    double P[6][6]  = {};
    int    hits     = 0;
    int    misses   = 0;
    bool   confirmed = false;
    double last_snr_db = 0;
    double age_s    = 0;
    // Classification, from the micro-Doppler stage.
    std::string label;           ///< "" until enough evidence accumulates
    double      label_conf = 0;
    double      micro_bw_hz = 0; ///< spectral width beyond the bulk motion
    double      blade_hz    = 0; ///< dominant periodic modulation, if any
    std::vector<float> spectrogram; ///< n_time * n_freq, dB, row major
    int         spec_time = 0, spec_freq = 0;
};

/// Everything produced from one coherent processing interval.
struct RdFrame {
    u64    index      = 0;
    double timestamp_s = 0;
    int    n_range    = 0;
    int    n_doppler  = 0;
    int    n_virt     = 4;
    bool   overflow   = false;
    double noise_floor = 0;      ///< CFAR training mean, linear power

    /// Non-coherently integrated power map, range major, n_range * n_doppler.
    /// This is what the display shows and what CFAR ran on.
    AlignedBuffer<float> power;

    /// Per-virtual-channel complex range-Doppler cube, only populated when the
    /// host is running the full pipeline itself (simulation or replay).  On
    /// hardware the FPGA sends only the detections' channel samples, which
    /// land in Hit::virt.
    AlignedBuffer<cf32> cube;    ///< n_virt * n_range * n_doppler
    bool   cube_valid = false;

    std::vector<Hit>    hits;
    std::vector<Target> targets;

    float&       at(int r, int d)       { return power[std::size_t(r) * n_doppler + d]; }
    const float& at(int r, int d) const { return power[std::size_t(r) * n_doppler + d]; }
    cf32&        cube_at(int v, int r, int d) {
        return cube[(std::size_t(v) * n_range + r) * n_doppler + d];
    }
    const cf32&  cube_at(int v, int r, int d) const {
        return cube[(std::size_t(v) * n_range + r) * n_doppler + d];
    }

    void allocate(int nr, int nd, int nv, bool with_cube) {
        n_range = nr; n_doppler = nd; n_virt = nv;
        power.resize(std::size_t(nr) * nd);
        power.zero();
        cube_valid = with_cube;
        if (with_cube) { cube.resize(std::size_t(nv) * nr * nd); cube.zero(); }
    }
};

/// Running health counters, published alongside every frame.
struct Stats {
    u64    frames        = 0;
    u64    overflows     = 0;
    u64    dropped       = 0;
    double cpu_frac      = 0;    ///< pipeline time / wall time
    double frame_rate_hz = 0;
    double stage_ms[8]   = {};   ///< dechirp, range, corner, dopp, cfar, aoa,
                                 ///< cluster, track
    u64    bytes_in      = 0;
    int    n_tracks      = 0;
};

} // namespace radar
