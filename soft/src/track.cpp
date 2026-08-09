//============================================================================
// track.cpp -- see track.hpp
//============================================================================
#include "radar/track.hpp"

#include <algorithm>
#include <cmath>

namespace radar {

namespace {

constexpr int NX = 6;   // state:       x y z vx vy vz
constexpr int NZ = 4;   // measurement: range, azimuth, elevation, range rate

inline double wrap_pi(double a) {
    while (a >  kPi) a -= 2.0 * kPi;
    while (a < -kPi) a += 2.0 * kPi;
    return a;
}

/// Cholesky inverse of a small real symmetric positive-definite matrix.
/// Returns false when the matrix is not positive definite, which for an
/// innovation covariance means the filter has diverged and the caller has to
/// refuse the update rather than propagate a nonsense gain.
bool sym_inverse4(const double A[NZ][NZ], double Ainv[NZ][NZ]) {
    double L[NZ][NZ] = {};
    for (int i = 0; i < NZ; ++i) {
        for (int j = 0; j <= i; ++j) {
            double sum = A[i][j];
            for (int k = 0; k < j; ++k) sum -= L[i][k] * L[j][k];
            if (i == j) {
                if (!(sum > 0.0) || !std::isfinite(sum)) return false;
                L[i][i] = std::sqrt(sum);
            } else {
                L[i][j] = sum / L[j][j];
            }
        }
    }
    double Li[NZ][NZ] = {};
    for (int c = 0; c < NZ; ++c) {
        Li[c][c] = 1.0 / L[c][c];
        for (int r = c + 1; r < NZ; ++r) {
            double sum = 0.0;
            for (int k = c; k < r; ++k) sum += L[r][k] * Li[k][c];
            Li[r][c] = -sum / L[r][r];
        }
    }
    for (int i = 0; i < NZ; ++i)
        for (int j = 0; j < NZ; ++j) {
            double sum = 0.0;
            for (int k = std::max(i, j); k < NZ; ++k) sum += Li[k][i] * Li[k][j];
            Ainv[i][j] = sum;
        }
    return true;
}

} // namespace

//============================================================================
Scales resolution_scales(const Config& cfg) {
    Scales s;
    s.lambda_m = (cfg.d.lambda_m > 0)
                     ? cfg.d.lambda_m
                     : phys::c0 / (cfg.centre_freq_hz > 0 ? cfg.centre_freq_hz
                                                          : array_geom::f0_hz);

    // Range resolution is set by the swept bandwidth alone: two echoes are
    // separable when their round-trip delay differs by more than one over the
    // bandwidth.  Zero padding the transform makes the bins finer than this
    // but does not make the radar see any more detail, which is exactly why
    // the measurement noise has to come from the resolution and not the bin.
    s.range_res_m = (cfg.d.range_res_m > 0) ? cfg.d.range_res_m
                                            : phys::c0 / (2.0 * std::max(1.0, cfg.sweep_bw_hz));

    const double t_pri = (cfg.d.t_pri_s > 0)
                             ? cfg.d.t_pri_s
                             : double(cfg.n_pri) / std::max(1.0, cfg.sample_rate_hz);
    // In time-division multiplexing the two transmitters take turns, so a
    // given virtual channel is only refreshed every other pulse repetition
    // interval.  Its slow-time sample rate, and therefore the unambiguous
    // velocity, is halved.
    const int    tdm_factor = (cfg.mimo == MimoMode::Tdm) ? array_geom::n_tx : 1;
    const double t_slow     = t_pri * tdm_factor;

    s.t_cpi_s     = (cfg.d.t_cpi_s > 0) ? cfg.d.t_cpi_s : t_slow * cfg.n_chirp;
    s.prf_slow_hz = (t_slow > 0) ? 1.0 / t_slow : 0.0;
    s.vel_res_ms  = (cfg.d.vel_res_ms > 0) ? cfg.d.vel_res_ms
                                           : s.lambda_m / (2.0 * std::max(1e-9, s.t_cpi_s));
    s.vel_max_ms  = (cfg.d.vel_max_ms > 0) ? cfg.d.vel_max_ms
                                           : s.lambda_m * s.prf_slow_hz / 4.0;
    return s;
}

//============================================================================
Tracker::Tracker(const Config& cfg)
    : cfg_(cfg),
      sc_(resolution_scales(cfg)),
      q_accel_(cfg.track_q_accel > 0 ? cfg.track_q_accel : 1.0),
      gate_(cfg.track_gate_chi2 > 0 ? cfg.track_gate_chi2 : 16.0),
      confirm_n_(std::max(1, cfg.track_confirm_n)),
      drop_n_(std::max(1, cfg.track_drop_n)) {

    // A bin index is a quantised measurement, and a quantised measurement is
    // uniformly distributed inside its bin.  The standard deviation of a
    // uniform distribution one bin wide is that width over the square root of
    // twelve; there is nothing to tune here.
    sigma_r_  = sc_.range_res_m / std::sqrt(12.0);
    sigma_rr_ = sc_.vel_res_ms  / std::sqrt(12.0);

    // Angle precision comes from phase, so the constant that matters is how
    // many radians of phase one unit of direction cosine buys.  That is the
    // wavenumber times the aperture, taken from the element positions rather
    // than assumed, so a retuned carrier or a different board changes it
    // automatically.
    double xmin = 1e30, xmax = -1e30, ymin = 1e30, ymax = -1e30;
    for (int i = 0; i < array_geom::n_virt; ++i) {
        xmin = std::min(xmin, array_geom::virt_xy[i][0]);
        xmax = std::max(xmax, array_geom::virt_xy[i][0]);
        ymin = std::min(ymin, array_geom::virt_xy[i][1]);
        ymax = std::max(ymax, array_geom::virt_xy[i][1]);
    }
    const double k0 = 2.0 * kPi / sc_.lambda_m;
    k_phase_x_ = k0 * std::max(1e-9, xmax - xmin);
    k_phase_y_ = k0 * std::max(1e-9, ymax - ymin);

    // Half-power width of the array pattern along each axis, measured from the
    // same element positions.  Two elements spaced half a wavelength give a
    // pattern that falls to half power at a direction cosine of one half, so
    // this comes out at sixty degrees -- a very wide beam, which is why the
    // angle accuracy has to be bought with signal-to-noise ratio.
    auto pattern = [&](double u, double v) {
        double re = 0, im = 0;
        for (int i = 0; i < array_geom::n_virt; ++i) {
            const double p = k0 * (array_geom::virt_xy[i][0] * u + array_geom::virt_xy[i][1] * v);
            re += std::cos(p); im += std::sin(p);
        }
        return re * re + im * im;
    };
    auto hpbw = [&](bool along_x) {
        const double p0 = pattern(0, 0);
        double prev = p0, prev_a = 0.0;
        for (int i = 1; i <= 9000; ++i) {
            const double a = rad(i * 0.01);
            const double p = along_x ? pattern(std::sin(a), 0.0) : pattern(0.0, std::sin(a));
            if (p <= 0.5 * p0) {
                const double f = (prev - 0.5 * p0) / (prev - p);
                return 2.0 * deg(prev_a + f * (rad(i * 0.01) - prev_a));
            }
            prev = p; prev_a = a;
        }
        return 180.0;
    };
    bw_az_deg_ = hpbw(true);
    bw_el_deg_ = hpbw(false);

    // No array is calibrated better than a fraction of a degree, so however
    // strong the echo the angle error never goes below this.  A filter that
    // believes an infinitely precise measurement stops listening to its own
    // model and starts chasing every fluctuation.
    sigma_ang_floor_ = rad(0.2);

    tracks_.reserve(64);
    pairs_.reserve(256);
}

void Tracker::reset() {
    tracks_.clear();
    next_id_ = 1;
}

std::size_t Tracker::n_confirmed() const {
    std::size_t k = 0;
    for (const Track& t : tracks_) if (t.confirmed) ++k;
    return k;
}

//============================================================================
void Tracker::measurement_noise(double snr_db, double el_rad, double R[NZ][NZ]) const {
    for (int i = 0; i < NZ; ++i)
        for (int j = 0; j < NZ; ++j) R[i][j] = 0.0;

    const double snr = std::max(undb(snr_db), 1.0);

    // Angle error against signal-to-noise ratio.
    //
    // The array measures an angle by comparing the phase of two halves of
    // itself.  The uncertainty of a phase measurement falls as the square root
    // of signal-to-noise ratio, and one radian of phase is worth one over
    // (wavenumber times aperture) of direction cosine, so the angle error is
    // that aperture factor divided by the root of twice the ratio.
    //
    // This is the Cramer-Rao bound, and it is written that way on purpose.
    // Scaling the beamwidth by the ratio itself rather than its square root
    // would claim a hundredth of a degree at 30 dB, which no radar achieves,
    // and would be pessimistic by a factor of three at 10 dB.  The floor and
    // the cap below are what keeps the honest formula sane at both ends.
    const double root = std::sqrt(2.0 * snr);
    const double cos_el = std::max(0.2, std::cos(el_rad));

    // A direction cosine along x is sin(azimuth) times cos(elevation), so the
    // same phase error is worth more azimuth when the target is high up.
    double s_az = (1.0 / (k_phase_x_ * root)) / cos_el;
    double s_el =  1.0 / (k_phase_y_ * root);

    s_az = clampv(s_az, sigma_ang_floor_, rad(0.5 * bw_az_deg_));
    s_el = clampv(s_el, sigma_ang_floor_, rad(0.5 * bw_el_deg_));

    R[0][0] = sigma_r_  * sigma_r_;
    R[1][1] = s_az * s_az;
    R[2][2] = s_el * s_el;
    R[3][3] = sigma_rr_ * sigma_rr_;
}

//============================================================================
void Tracker::predict(Track& t, double dt) const {
    if (!(dt > 0.0)) return;

    t.x += t.vx * dt;  t.y += t.vy * dt;  t.z += t.vz * dt;

    // P <- F P F^T with F = [[I, dt I],[0, I]], written out because it is
    // mostly identity and a general multiply would be four times the work.
    double P[NX][NX];
    for (int i = 0; i < NX; ++i)
        for (int j = 0; j < NX; ++j) P[i][j] = t.P[i][j];

    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < NX; ++j) P[i][j] += dt * P[i + 3][j];
    for (int j = 0; j < 3; ++j)
        for (int i = 0; i < NX; ++i) P[i][j] += dt * P[i][j + 3];

    // Process noise for a piecewise-constant white acceleration model: over
    // one frame the target is assumed to hold an unknown constant
    // acceleration drawn from a distribution of standard deviation q, which
    // moves it by half q dt squared and changes its velocity by q dt.  The
    // block structure below is the covariance of exactly that.
    const double q  = q_accel_ * q_accel_;
    const double d2 = dt * dt, d3 = d2 * dt, d4 = d3 * dt;
    const double qpp = q * d4 / 4.0, qpv = q * d3 / 2.0, qvv = q * d2;
    for (int i = 0; i < 3; ++i) {
        P[i][i]         += qpp;
        P[i][i + 3]     += qpv;
        P[i + 3][i]     += qpv;
        P[i + 3][i + 3] += qvv;
    }

    for (int i = 0; i < NX; ++i)
        for (int j = 0; j < NX; ++j) t.P[i][j] = 0.5 * (P[i][j] + P[j][i]);

    t.age_s += dt;
}

//============================================================================
Track Tracker::spawn(const Target& m) const {
    Track t;
    t.id = 0;   // filled by the caller, which owns the counter

    const double az = rad(m.azimuth_deg), el = rad(m.elevation_deg);
    const double r  = std::max(m.range_m, 1e-3);
    const double ce = std::cos(el), se = std::sin(el);
    const double ca = std::cos(az), sa = std::sin(az);

    t.x = r * ce * sa;   // right
    t.y = r * ce * ca;   // boresight
    t.z = r * se;        // up

    // Only the component of velocity along the line of sight is measured.  The
    // across-beam components are unknown, so the state starts with the radial
    // part and the covariance below says loudly that the rest is a guess.
    // Config's velocity_ms is positive towards the radar; range rate is the
    // opposite sign.
    const double rdot = -m.velocity_ms;
    t.vx = rdot * (t.x / r);
    t.vy = rdot * (t.y / r);
    t.vz = rdot * (t.z / r);

    double R[NZ][NZ];
    measurement_noise(m.snr_db, el, R);

    // Map the polar measurement covariance into Cartesian: J R J^T with J the
    // derivative of position with respect to range and the two angles.  Doing
    // this properly rather than assuming a sphere matters here, because the
    // beam is sixty degrees wide and the across-range uncertainty at 200 m is
    // tens of metres while the along-range uncertainty is under a metre.  A
    // spherical initial covariance would either throw away the good range or
    // believe the bad angle.
    const double J[3][3] = {
        { ce * sa,  r * ce * ca, -r * se * sa },
        { ce * ca, -r * ce * sa, -r * se * ca },
        { se,       0.0,          r * ce      },
    };
    const double rv[3] = { R[0][0], R[1][1], R[2][2] };
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) {
            double s = 0.0;
            for (int k = 0; k < 3; ++k) s += J[i][k] * rv[k] * J[j][k];
            t.P[i][j] = s;
        }

    const double sv = std::max(sc_.vel_max_ms, 20.0);
    for (int i = 3; i < NX; ++i) t.P[i][i] = sv * sv;

    t.hits        = 1;
    t.misses      = 0;
    t.confirmed   = (confirm_n_ <= 1);
    t.last_snr_db = m.snr_db;
    t.age_s       = 0.0;
    return t;
}

//============================================================================
void Tracker::update(const std::vector<Target>& z, double dt, std::vector<Track>& out) {
    for (Track& t : tracks_) predict(t, dt);

    const int nt = int(tracks_.size());
    const int nm = int(z.size());
    meas_of_track_.assign(std::size_t(nt), -1);
    track_of_meas_.assign(std::size_t(nm), -1);
    pairs_.clear();

    // Everything the update needs, computed once per (track, measurement) pair
    // and kept so neither the assignment nor the correction recomputes it.
    const std::size_t stride = std::size_t(std::max(nm, 1));
    cache_.assign(std::size_t(nt) * stride, Cache{});

    for (int i = 0; i < nt; ++i) {
        const Track& t = tracks_[std::size_t(i)];
        const double px = t.x, py = t.y, pz = t.z;
        const double rho2 = px * px + py * py;
        const double rho  = std::sqrt(rho2);
        const double r2   = rho2 + pz * pz;
        const double r    = std::sqrt(r2);
        if (!(r > 1e-6) || !(rho > 1e-9)) continue;

        const double rdot = (px * t.vx + py * t.vy + pz * t.vz) / r;

        // Measurement Jacobian.  Rows: range, azimuth, elevation, range rate.
        //
        // Azimuth is measured from boresight, which is +y, towards +x, so it
        // is atan2(x, y) and not the usual atan2(y, x); the derivatives swap
        // and change sign accordingly.  Elevation is the angle up out of the
        // x-y plane.  The range-rate row is the derivative of (p.v)/|p|, whose
        // position part is the velocity minus its own radial component,
        // divided by range -- the across-beam velocity is what moves the
        // measured range rate when the target shifts sideways, and dropping
        // that term is the classic way to make a tracker lag on a turn.
        double H[NZ][NX] = {};
        H[0][0] = px / r;  H[0][1] = py / r;  H[0][2] = pz / r;
        H[1][0] = py / rho2;  H[1][1] = -px / rho2;  H[1][2] = 0.0;
        H[2][0] = -pz * px / (r2 * rho);
        H[2][1] = -pz * py / (r2 * rho);
        H[2][2] = rho / r2;
        const double v[3] = { t.vx, t.vy, t.vz };
        const double p[3] = { px, py, pz };
        for (int k = 0; k < 3; ++k) {
            H[3][k]     = (v[k] - rdot * p[k] / r) / r;
            H[3][k + 3] = p[k] / r;
        }

        const double h_az = std::atan2(px, py);
        const double h_el = std::atan2(pz, rho);

        for (int j = 0; j < nm; ++j) {
            Cache& c = cache_[std::size_t(i) * stride + std::size_t(j)];
            c.ok = false;
            const Target& m = z[std::size_t(j)];

            double (&R)[NZ][NZ] = c.R;
            measurement_noise(m.snr_db, rad(m.elevation_deg), R);

            // S = H P H^T + R
            double PHt[NX][NZ] = {};
            for (int a = 0; a < NX; ++a)
                for (int b = 0; b < NZ; ++b) {
                    double s = 0.0;
                    for (int k = 0; k < NX; ++k) s += t.P[a][k] * H[b][k];
                    PHt[a][b] = s;
                }
            double S[NZ][NZ];
            for (int a = 0; a < NZ; ++a)
                for (int b = 0; b < NZ; ++b) {
                    double s = R[a][b];
                    for (int k = 0; k < NX; ++k) s += H[a][k] * PHt[k][b];
                    S[a][b] = s;
                }
            for (int a = 0; a < NZ; ++a)
                for (int b = a + 1; b < NZ; ++b) {
                    const double v2 = 0.5 * (S[a][b] + S[b][a]);
                    S[a][b] = S[b][a] = v2;
                }
            if (!sym_inverse4(S, c.S_inv)) continue;

            c.nu[0] = m.range_m - r;
            c.nu[1] = wrap_pi(rad(m.azimuth_deg)   - h_az);
            c.nu[2] = wrap_pi(rad(m.elevation_deg) - h_el);
            c.nu[3] = (-m.velocity_ms) - rdot;
            for (int a = 0; a < NZ; ++a)
                for (int b = 0; b < NX; ++b) c.H[a][b] = H[a][b];
            c.ok = true;

            double d2 = 0.0;
            for (int a = 0; a < NZ; ++a)
                for (int b = 0; b < NZ; ++b) d2 += c.nu[a] * c.S_inv[a][b] * c.nu[b];
            if (d2 <= gate_ && std::isfinite(d2)) pairs_.push_back({d2, i, j});
        }
    }

    //-- Assignment ----------------------------------------------------------
    //
    // Sort every in-gate pair by how well it fits and take them in that order,
    // skipping anything whose track or measurement is already spoken for.
    //
    // Greedy is enough here, and not as a compromise.  The gate is a
    // four-dimensional ellipsoid in range, two angles and range rate, and it
    // is small in every one of those: a measurement only falls inside two
    // tracks' gates when two objects agree in range, in both angles and in
    // radial speed all at once, to within the radar's resolution.  At that
    // point the two hypotheses are physically indistinguishable in this frame
    // and an optimal assignment has nothing extra to work with -- it would
    // pick the same pairs, or pick differently for reasons the data do not
    // support.  One frame later the range rates have separated them.  What
    // does matter is that ties break the same way every time, which sorting on
    // distance and then on index guarantees.
    std::sort(pairs_.begin(), pairs_.end(), [](const Pair& a, const Pair& b) {
        if (a.d2 != b.d2) return a.d2 < b.d2;
        if (a.trk != b.trk) return a.trk < b.trk;
        return a.meas < b.meas;
    });
    for (const Pair& pr : pairs_) {
        if (meas_of_track_[std::size_t(pr.trk)] >= 0) continue;
        if (track_of_meas_[std::size_t(pr.meas)] >= 0) continue;
        meas_of_track_[std::size_t(pr.trk)] = pr.meas;
        track_of_meas_[std::size_t(pr.meas)] = pr.trk;
    }

    //-- Correct -------------------------------------------------------------
    for (int i = 0; i < nt; ++i) {
        const int j = meas_of_track_[std::size_t(i)];
        Track& t = tracks_[std::size_t(i)];
        if (j < 0) { ++t.misses; continue; }

        const Cache& c = cache[std::size_t(i) * std::size_t(std::max(nm, 1)) + std::size_t(j)];
        const Target& m = z[std::size_t(j)];

        double R[NZ][NZ];
        measurement_noise(m.snr_db, rad(m.elevation_deg), R);

        double PHt[NX][NZ] = {};
        for (int a = 0; a < NX; ++a)
            for (int b = 0; b < NZ; ++b) {
                double s = 0.0;
                for (int k = 0; k < NX; ++k) s += t.P[a][k] * c.H[b][k];
                PHt[a][b] = s;
            }
        double K[NX][NZ] = {};
        for (int a = 0; a < NX; ++a)
            for (int b = 0; b < NZ; ++b) {
                double s = 0.0;
                for (int k = 0; k < NZ; ++k) s += PHt[a][k] * c.S_inv[k][b];
                K[a][b] = s;
            }

        double st[NX] = { t.x, t.y, t.z, t.vx, t.vy, t.vz };
        for (int a = 0; a < NX; ++a)
            for (int b = 0; b < NZ; ++b) st[a] += K[a][b] * c.nu[b];
        t.x = st[0]; t.y = st[1]; t.z = st[2];
        t.vx = st[3]; t.vy = st[4]; t.vz = st[5];

        // Joseph form.  The short version, P <- (I - K H) P, is only correct
        // for the exactly optimal gain; the moment the gain is off -- because
        // the Jacobian was linearised, or because the inverse lost a digit --
        // it can produce a covariance that is not symmetric positive definite,
        // and a filter with a negative variance never recovers.  Joseph stays
        // positive definite for any gain at all, which is worth the extra
        // matrix multiply.
        double A[NX][NX];
        for (int a = 0; a < NX; ++a)
            for (int b = 0; b < NX; ++b) {
                double s = (a == b) ? 1.0 : 0.0;
                for (int k = 0; k < NZ; ++k) s -= K[a][k] * c.H[k][b];
                A[a][b] = s;
            }
        double AP[NX][NX] = {};
        for (int a = 0; a < NX; ++a)
            for (int b = 0; b < NX; ++b) {
                double s = 0.0;
                for (int k = 0; k < NX; ++k) s += A[a][k] * t.P[k][b];
                AP[a][b] = s;
            }
        double KR[NX][NZ] = {};
        for (int a = 0; a < NX; ++a)
            for (int b = 0; b < NZ; ++b) {
                double s = 0.0;
                for (int k = 0; k < NZ; ++k) s += K[a][k] * R[k][b];
                KR[a][b] = s;
            }
        for (int a = 0; a < NX; ++a)
            for (int b = 0; b < NX; ++b) {
                double s = 0.0;
                for (int k = 0; k < NX; ++k) s += AP[a][k] * A[b][k];
                for (int k = 0; k < NZ; ++k) s += KR[a][k] * K[b][k];
                t.P[a][b] = s;
            }
        for (int a = 0; a < NX; ++a)
            for (int b = a + 1; b < NX; ++b) {
                const double s = 0.5 * (t.P[a][b] + t.P[b][a]);
                t.P[a][b] = t.P[b][a] = s;
            }

        ++t.hits;
        t.misses      = 0;
        t.last_snr_db = m.snr_db;
        if (t.hits >= confirm_n_) t.confirmed = true;
    }

    //-- Births and deaths ---------------------------------------------------
    for (int j = 0; j < nm; ++j) {
        if (track_of_meas_[std::size_t(j)] >= 0) continue;
        Track t = spawn(z[std::size_t(j)]);
        t.id = next_id_++;
        tracks_.push_back(t);
    }
    tracks_.erase(std::remove_if(tracks_.begin(), tracks_.end(),
                                 [this](const Track& t) { return t.misses >= drop_n_; }),
                  tracks_.end());

    // Tentative tracks go out as well as confirmed ones, flagged as such.  The
    // display wants to show something the moment an echo appears, and the
    // classifier wants as long a run of micro-Doppler as it can get, which
    // means starting before the track is trusted.
    out.assign(tracks_.begin(), tracks_.end());
}

} // namespace radar
