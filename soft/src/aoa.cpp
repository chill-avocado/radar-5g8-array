//============================================================================
// aoa.cpp -- see aoa.hpp
//============================================================================
#include "radar/aoa.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace radar {

namespace {

constexpr int kM = 4;   // virtual channels

//----------------------------------------------------------------------------
// 4x4 Hermitian eigendecomposition by cyclic Jacobi rotations.
//
// Four by four is small enough that the classical sweep beats anything
// cleverer: six off-diagonal entries, each annihilated by one plane rotation,
// and about five sweeps to reach machine precision.  Written out rather than
// pulled from a library because the whole DSP path has to build on a bare
// toolchain, and because a fixed-size solver with no allocation and no
// branching on data can sit in the per-detection hot path.
//
// The complex case differs from the textbook real one in a single step: the
// off-diagonal entry is first spun onto the positive real axis by a diagonal
// phase, after which the ordinary real rotation applies.  The two combined
// give the unitary that zeroes it.
//
// On return w holds the eigenvalues in descending order and the columns of V
// the matching eigenvectors, so A == V diag(w) V^H.
//----------------------------------------------------------------------------
void jacobi_hermitian4(const cf64 in[kM][kM], double w[kM], cf64 V[kM][kM]) {
    cf64 A[kM][kM];
    for (int i = 0; i < kM; ++i)
        for (int j = 0; j < kM; ++j) A[i][j] = in[i][j];
    for (int i = 0; i < kM; ++i)
        for (int j = 0; j < kM; ++j) V[i][j] = (i == j) ? cf64(1, 0) : cf64(0, 0);

    double scale = 0.0;
    for (int i = 0; i < kM; ++i) scale += std::abs(A[i][i]);
    const double tol = (scale > 0 ? scale : 1.0) * 1e-30;

    for (int sweep = 0; sweep < 40; ++sweep) {
        double off = 0.0;
        for (int p = 0; p < kM; ++p)
            for (int q = p + 1; q < kM; ++q) off += std::norm(A[p][q]);
        if (off <= tol) break;

        for (int p = 0; p < kM; ++p) {
            for (int q = p + 1; q < kM; ++q) {
                const double g = std::abs(A[p][q]);
                if (g <= 1e-300) continue;

                const cf64   ph  = A[p][q] / g;                   // unit phase
                const double app = A[p][p].real(), aqq = A[q][q].real();
                const double tau = (aqq - app) / (2.0 * g);
                // Smaller-magnitude root of t^2 - 2*tau*t - 1 = 0.  Choosing
                // the small root keeps every rotation under 45 degrees, which
                // is what makes the sweep converge quadratically instead of
                // wandering.
                const double sgn = (tau >= 0.0) ? 1.0 : -1.0;
                const double t   = -sgn / (std::abs(tau) + std::sqrt(1.0 + tau * tau));
                const double c   = 1.0 / std::sqrt(1.0 + t * t);
                const double s   = t * c;

                const cf64 upp = ph * c, upq = -ph * s;
                const cf64 uqp = cf64(s, 0), uqq = cf64(c, 0);

                // Rows and columns other than p and q pass straight through,
                // so only the two affected vectors need touching.
                for (int k = 0; k < kM; ++k) {
                    if (k == p || k == q) continue;
                    const cf64 akp = A[k][p], akq = A[k][q];
                    A[k][p] = akp * upp + akq * uqp;
                    A[k][q] = akp * upq + akq * uqq;
                    A[p][k] = std::conj(A[k][p]);
                    A[q][k] = std::conj(A[k][q]);
                }
                // The 2x2 block, transformed in full rather than by the
                // closed-form diagonal update, so a Hermitian matrix stays
                // exactly Hermitian however the rounding falls.
                const cf64 apq = A[p][q];
                const cf64 t00 = app * upp + apq * uqp;
                const cf64 t01 = app * upq + apq * uqq;
                const cf64 t10 = std::conj(apq) * upp + aqq * uqp;
                const cf64 t11 = std::conj(apq) * upq + aqq * uqq;
                A[p][p] = cf64((std::conj(upp) * t00 + std::conj(uqp) * t10).real(), 0.0);
                A[q][q] = cf64((std::conj(upq) * t01 + std::conj(uqq) * t11).real(), 0.0);
                A[p][q] = cf64(0, 0);
                A[q][p] = cf64(0, 0);

                for (int k = 0; k < kM; ++k) {
                    const cf64 vkp = V[k][p], vkq = V[k][q];
                    V[k][p] = vkp * upp + vkq * uqp;
                    V[k][q] = vkp * upq + vkq * uqq;
                }
            }
        }
    }

    int idx[kM] = {0, 1, 2, 3};
    for (int i = 0; i < kM; ++i) w[i] = A[i][i].real();
    std::sort(idx, idx + kM, [&](int a, int b) { return w[a] > w[b]; });

    double  ws[kM];
    cf64    Vs[kM][kM];
    for (int c = 0; c < kM; ++c) {
        ws[c] = w[idx[c]];
        for (int r = 0; r < kM; ++r) Vs[r][c] = V[r][idx[c]];
    }
    for (int c = 0; c < kM; ++c) {
        w[c] = ws[c];
        for (int r = 0; r < kM; ++r) V[r][c] = Vs[r][c];
    }
}

//----------------------------------------------------------------------------
// Inverse of a 4x4 Hermitian positive-definite matrix by Cholesky.
//
// A = L L^H with L lower triangular and real positive on the diagonal, then
// A^-1 = L^-H L^-1.  Going through the factor rather than a cofactor
// expansion matters: the factorisation itself fails, cleanly and detectably,
// exactly when the matrix is not positive definite, which for a covariance
// means the estimate is rank-poor and the caller has to do something else.
//----------------------------------------------------------------------------
bool chol_inverse_hermitian4(const cf64 A[kM][kM], cf64 Ainv[kM][kM]) {
    cf64   L[kM][kM] = {};
    double dgn[kM]   = {};

    for (int i = 0; i < kM; ++i) {
        for (int j = 0; j <= i; ++j) {
            cf64 sum = A[i][j];
            for (int k = 0; k < j; ++k) sum -= L[i][k] * std::conj(L[j][k]);
            if (i == j) {
                const double d = sum.real();
                if (!(d > 0.0) || !std::isfinite(d)) return false;
                dgn[i]  = std::sqrt(d);
                L[i][i] = cf64(dgn[i], 0.0);
            } else {
                L[i][j] = sum / dgn[j];
            }
        }
    }

    // Forward-substitute the identity to get L^-1, lower triangular.
    cf64 Li[kM][kM] = {};
    for (int c = 0; c < kM; ++c) {
        Li[c][c] = cf64(1.0 / dgn[c], 0.0);
        for (int r = c + 1; r < kM; ++r) {
            cf64 sum(0, 0);
            for (int k = c; k < r; ++k) sum += L[r][k] * Li[k][c];
            Li[r][c] = -sum / dgn[r];
        }
    }

    for (int i = 0; i < kM; ++i)
        for (int j = 0; j < kM; ++j) {
            cf64 sum(0, 0);
            for (int k = std::max(i, j); k < kM; ++k) sum += std::conj(Li[k][i]) * Li[k][j];
            Ainv[i][j] = sum;
        }
    return true;
}

//----------------------------------------------------------------------------
// Forward-backward averaging.
//
// The four virtual elements sit on a square centred on the origin, so element
// 3 is the mirror of element 0 and element 2 the mirror of element 1.  For any
// arrival direction the mirrored, conjugated steering vector is the original
// steering vector exactly.  That gives a second look at the scene for free:
// mirroring and conjugating the covariance produces a matrix with the same
// directions in it but different, effectively independent, complex amplitudes.
//
// It is the reason a single snapshot -- all a single range-Doppler cell ever
// provides -- can support a subspace method at all.  Two targets landing in
// one cell are perfectly correlated in that snapshot, their covariance is rank
// one, and any subspace method sees one target.  After averaging with the
// mirrored copy the rank is two and both are visible.
//
// Ordinary spatial smoothing would do the same job by sliding a subarray
// across the aperture, but a two-by-two grid only admits subarrays of two
// elements, which throws away the second axis entirely and can then resolve
// nothing.  On a centro-symmetric array the mirror trick costs no aperture at
// all, so it is strictly the better choice here.
//----------------------------------------------------------------------------
void forward_backward(cf64 R[kM][kM]) {
    cf64 out[kM][kM];
    for (int i = 0; i < kM; ++i)
        for (int j = 0; j < kM; ++j)
            out[i][j] = 0.5 * (R[i][j] + std::conj(R[kM - 1 - i][kM - 1 - j]));
    for (int i = 0; i < kM; ++i)
        for (int j = 0; j < kM; ++j) R[i][j] = out[i][j];
}

//----------------------------------------------------------------------------
// Minimum-description-length model order.
//
// Asks how many of the four eigenvalues are genuinely larger than the rest.
// If the smallest M-k of them come from noise alone they should all be equal,
// so their geometric and arithmetic means coincide; the more they differ the
// more likely one of them is really a target.  The second term charges for
// each extra source claimed, which is what stops the criterion from simply
// choosing the largest possible order.
//----------------------------------------------------------------------------
int mdl_order(const double w[kM], int n_snap) {
    if (n_snap < 2) return 1;   // no penalty term to speak of, so no evidence
    double trace = 0.0;
    for (int i = 0; i < kM; ++i) trace += std::max(0.0, w[i]);
    if (!(trace > 0.0)) return 1;
    const double floor_ev = trace * 1e-12;

    int    best_k   = 1;
    double best_val = std::numeric_limits<double>::infinity();
    for (int k = 0; k < kM; ++k) {
        const int n = kM - k;
        double sum = 0.0, log_sum = 0.0;
        for (int i = k; i < kM; ++i) {
            const double ev = std::max(w[i], floor_ev);
            sum     += ev;
            log_sum += std::log(ev);
        }
        const double log_geo = log_sum / n;
        const double log_ari = std::log(sum / n);
        const double val = -double(n_snap) * n * (log_geo - log_ari)
                         + 0.5 * k * (2 * kM - k) * std::log(double(n_snap));
        if (val < best_val) { best_val = val; best_k = k; }
    }
    return clampv(best_k, 1, kM - 1);
}

//----------------------------------------------------------------------------
// Least-squares quadratic surface through a 3x3 patch, returning the offset of
// its stationary point from the centre cell in cells.
//
// Fitting the whole surface rather than two independent parabolas matters when
// the peak sits on a ridge running diagonally across the grid, which is what
// an angle spectrum looks like near the edge of the field of view, where
// azimuth and elevation stop being independent.
//
// Returns false when the patch has no interior stationary point of the right
// kind, in which case the caller keeps the grid cell.
//----------------------------------------------------------------------------
bool quad_peak_3x3(const double f[9], double& di, double& dj) {
    // Basis 1, i, j, i^2, j^2, ij over i,j in {-1,0,1}.  The normal equations
    // decouple into three independent pieces because the design is symmetric.
    double A0 = 0, A1 = 0, A2 = 0, Si = 0, Sj = 0, Sij = 0;
    for (int a = 0; a < 3; ++a) {
        const double i = a - 1.0;
        for (int b = 0; b < 3; ++b) {
            const double j = b - 1.0;
            const double v = f[a * 3 + b];
            A0  += v;
            A1  += i * i * v;
            A2  += j * j * v;
            Si  += i * v;
            Sj  += j * v;
            Sij += i * j * v;
        }
    }
    const double bb = Si / 6.0;
    const double cc = Sj / 6.0;
    const double gg = Sij / 4.0;
    const double aa = (5.0 * A0 - 3.0 * (A1 + A2)) / 9.0;
    const double de_sum  = (A0 - 9.0 * aa) / 6.0;      // d + e
    const double de_diff = (A1 - A2) / 2.0;            // d - e
    const double dd = 0.5 * (de_sum + de_diff);
    const double ee = 0.5 * (de_sum - de_diff);

    // Stationary point of a + b i + c j + d i^2 + e j^2 + g ij.
    const double det = 4.0 * dd * ee - gg * gg;
    // Callers always hand in a patch whose centre is the smallest value, so
    // the fitted surface has to curve upwards in both directions for the
    // stationary point to be the minimum being looked for.  A saddle means the
    // patch straddles a ridge and the grid cell is the better answer.
    if (!(det > 1e-18) || !(dd > 0.0)) return false;
    di = (-2.0 * ee * bb + gg * cc) / det;
    dj = (-2.0 * dd * cc + gg * bb) / det;
    if (!std::isfinite(di) || !std::isfinite(dj)) return false;
    if (std::abs(di) > 1.0 || std::abs(dj) > 1.0) return false;
    return true;
}

} // namespace

//============================================================================
// Per-thread scratch
//============================================================================
struct AoaEngine::Work {
    std::vector<float>  spec;    ///< dB, azimuth-major
    std::vector<double> quad;    ///< the raw quadratic form, before the log
};

AoaEngine::Work& AoaEngine::work() const {
    static thread_local Work w;
    const std::size_t n = std::size_t(n_az_) * n_el_;
    if (w.spec.size() != n) { w.spec.resize(n); w.quad.resize(n); }
    return w;
}

const std::vector<float>& AoaEngine::last_spectrum() const { return work().spec; }

//============================================================================
AoaEngine::AoaEngine(const Config& cfg)
    : method_(cfg.aoa),
      n_az_(std::max(1, cfg.aoa_az_bins)),
      n_el_(std::max(1, cfg.aoa_el_bins)),
      az_span_deg_(cfg.aoa_az_span_deg),
      el_span_deg_(cfg.aoa_el_span_deg),
      lambda_m_(phys::c0 / (cfg.centre_freq_hz > 0 ? cfg.centre_freq_hz : array_geom::f0_hz)) {

    az_step_deg_ = (n_az_ > 1) ? (2.0 * az_span_deg_) / (n_az_ - 1) : 0.0;
    el_step_deg_ = (n_el_ > 1) ? (2.0 * el_span_deg_) / (n_el_ - 1) : 0.0;

    az_deg_.resize(n_az_);
    el_deg_.resize(n_el_);
    for (int i = 0; i < n_az_; ++i) az_deg_[i] = float(-az_span_deg_ + i * az_step_deg_);
    for (int j = 0; j < n_el_; ++j) el_deg_[j] = float(-el_span_deg_ + j * el_step_deg_);

    // Steering products, once.  Everything downstream is a Hermitian quadratic
    // form and every steering entry has unit magnitude, so only the six
    // pairwise phase differences are ever needed, not the vectors themselves.
    const double k0 = 2.0 * kPi / lambda_m_;
    gram_.resize(std::size_t(n_az_) * n_el_ * 6);
    for (int ia = 0; ia < n_az_; ++ia) {
        const double az = rad(double(az_deg_[ia]));
        for (int ie = 0; ie < n_el_; ++ie) {
            const double el = rad(double(el_deg_[ie]));
            const double u  = std::sin(az) * std::cos(el);   // along the x axis
            const double v  = std::sin(el);                  // along the y axis
            double phase[kM];
            for (int m = 0; m < kM; ++m)
                phase[m] = k0 * (array_geom::virt_xy[m][0] * u + array_geom::virt_xy[m][1] * v);
            cf32* g = gram_.data() + (std::size_t(ia) * n_el_ + ie) * 6;
            for (int t = 0; t < 6; ++t) {
                const double dp = phase[pair_j_[t]] - phase[pair_i_[t]];
                g[t] = cf32(float(std::cos(dp)), float(std::sin(dp)));
            }
        }
    }

    // Half-power width of the array's own beam, scanned from the real element
    // positions.  The tracker needs a number here and the honest one is
    // whatever this array actually does, not a textbook aperture formula.
    auto beam_power = [&](double az, double el) {
        const double u = std::sin(az) * std::cos(el), v = std::sin(el);
        cf64 s(0, 0);
        for (int m = 0; m < kM; ++m) {
            const double p = k0 * (array_geom::virt_xy[m][0] * u + array_geom::virt_xy[m][1] * v);
            s += cf64(std::cos(p), std::sin(p));
        }
        return std::norm(s);
    };
    auto half_width = [&](bool along_az) {
        const double p0 = beam_power(0, 0);
        double prev = p0, prev_x = 0.0;
        for (int i = 1; i <= 9000; ++i) {
            const double x = rad(i * 0.01);
            const double p = along_az ? beam_power(x, 0.0) : beam_power(0.0, x);
            if (p <= 0.5 * p0) {
                const double f = (prev - 0.5 * p0) / (prev - p);
                return 2.0 * deg(prev_x + f * (x - prev_x));
            }
            prev = p; prev_x = x;
        }
        return 180.0;
    };
    bw_az_deg_ = half_width(true);
    bw_el_deg_ = half_width(false);
}

//============================================================================
void AoaEngine::apply_calibration(std::array<cf32, 4>& v, const std::array<cf32, 4>& cal) {
    for (int i = 0; i < 4; ++i) v[std::size_t(i)] *= cal[std::size_t(i)];
}

std::array<cf32, 4> AoaEngine::solve_boresight_calibration(const std::array<cf32, 4>& m) {
    // A target dead ahead reaches all four elements at the same instant, so
    // any difference between the four measurements is the receiver's, not the
    // target's.  The reference is chosen as the geometric mean amplitude and
    // the circular mean phase, so the correction removes only the differences
    // and leaves the overall gain and the overall delay untouched -- otherwise
    // calibration would quietly rescale every subsequent range and SNR figure.
    double log_amp = 0.0;
    cf64   phasor(0, 0);
    int    live = 0;
    for (int i = 0; i < 4; ++i) {
        const double a = std::abs(m[std::size_t(i)]);
        if (a > 0.0) {
            log_amp += std::log(a);
            phasor  += cf64(m[std::size_t(i)]) / a;
            ++live;
        }
    }
    std::array<cf32, 4> cal{};
    if (live == 0) { cal.fill(cf32(1.0f, 0.0f)); return cal; }

    const double amp_ref = std::exp(log_amp / live);
    const double ph_ref  = (std::abs(phasor) > 0.0) ? std::arg(phasor) : 0.0;
    const cf64   ref     = amp_ref * cf64(std::cos(ph_ref), std::sin(ph_ref));

    for (int i = 0; i < 4; ++i) {
        const cf64   mi = cf64(m[std::size_t(i)]);
        const double a2 = std::norm(mi);
        cal[std::size_t(i)] = (a2 > 0.0) ? cf32(ref * std::conj(mi) / a2) : cf32(1.0f, 0.0f);
    }
    return cal;
}

//============================================================================
AoaEngine::Result AoaEngine::run_monopulse(const std::array<cf32, 4>& v) const {
    Result r;
    r.method    = AoaMethod::Monopulse;
    r.n_sources = 1;
    r.degraded  = (method_ != AoaMethod::Monopulse);
    // Monopulse scans nothing, which is the entire reason it is cheap.  An
    // empty spectrum is how the display is told there is nothing to draw.
    work().spec.clear();

    double energy = 0.0;
    for (int i = 0; i < 4; ++i) energy += std::norm(v[std::size_t(i)]);
    if (!(energy > 0.0)) return r;

    // Channel order is (tx*2 + rx), which puts x = -lambda/4 at indices 0 and 1
    // and x = +lambda/4 at 2 and 3; y = -lambda/4 at 0 and 2, +lambda/4 at 1
    // and 3.  The two rows are averaged as complex products rather than as two
    // angles, because averaging angles goes wrong the moment one of them is
    // near half a turn and the other is just past it.
    const cf64 v0(v[0]), v1(v[1]), v2(v[2]), v3(v[3]);
    const cf64 dx = v2 * std::conj(v0) + v3 * std::conj(v1);   // +x relative to -x
    const cf64 dy = v1 * std::conj(v0) + v3 * std::conj(v2);   // +y relative to -y

    // Spacing is exactly half a wavelength, so the phase difference across a
    // pair is pi times the direction cosine.  One turn of phase therefore maps
    // onto the whole visible region and nothing folds back into it.
    const double u = std::arg(dx) / kPi;
    const double w = std::arg(dy) / kPi;
    if (std::abs(w) > 1.0) return r;

    const double el     = std::asin(clampv(w, -1.0, 1.0));
    const double cos_el = std::cos(el);
    if (cos_el < 1e-6) return r;
    const double su = u / cos_el;
    if (std::abs(su) > 1.0) {
        // The pair of direction cosines is off the unit sphere.  That is not a
        // steep angle, it is noise, and reporting an angle anyway would put a
        // phantom target on the edge of the display.
        return r;
    }

    r.az_deg = deg(std::asin(su));
    r.el_deg = deg(el);
    r.valid  = true;

    // With no spectrum there is no sidelobe to measure against, so quality is
    // how well the four measurements actually fit the single steering vector
    // the answer implies: signal energy over residual energy.
    const double k0 = 2.0 * kPi / lambda_m_;
    cf64 proj(0, 0);
    for (int m = 0; m < kM; ++m) {
        const double p = k0 * (array_geom::virt_xy[m][0] * u + array_geom::virt_xy[m][1] * w);
        proj += std::conj(cf64(std::cos(p), std::sin(p))) * cf64(v[std::size_t(m)]);
    }
    const double sig  = std::norm(proj) / 4.0;
    const double resid = std::max(energy - sig, energy * 1e-12);
    r.quality_db = 10.0 * std::log10(sig / resid + 1e-300);
    return r;
}

//============================================================================
AoaEngine::Result AoaEngine::run_spectrum(const cf64 R[kM][kM], int n_snap_eff,
                                          AoaMethod want) const {
    Result r;
    r.method = want;

    double trace = 0.0;
    for (int i = 0; i < kM; ++i) trace += R[i][i].real();
    if (!(trace > 0.0) || !std::isfinite(trace)) return r;

    // Build the Hermitian kernel M of the quadratic form a^H M a.  All three
    // scanned methods reduce to one, which is why they share a single loop.
    cf64 M[kM][kM];
    int  n_src = 1;
    bool invert_spectrum = false;   // true when the peak is a minimum of the form

    if (want == AoaMethod::Bartlett) {
        for (int i = 0; i < kM; ++i)
            for (int j = 0; j < kM; ++j) M[i][j] = R[i][j];

    } else if (want == AoaMethod::Capon) {
        // Diagonal loading, one percent of the mean diagonal.  Without it a
        // rank-poor covariance -- which is every single-snapshot covariance --
        // has no inverse at all, and with too much of it Capon degenerates
        // into Bartlett.  One percent puts the resolution most of the way to
        // the ideal while keeping the inverse comfortably conditioned.
        cf64 Rl[kM][kM];
        const double load = 0.01 * trace / kM;
        for (int i = 0; i < kM; ++i)
            for (int j = 0; j < kM; ++j) Rl[i][j] = R[i][j] + ((i == j) ? cf64(load, 0) : cf64(0, 0));
        if (!chol_inverse_hermitian4(Rl, M)) {
            // Not positive definite even after loading, which means the data
            // are not a covariance at all.  Bartlett cannot fail, so use it.
            r = run_spectrum(R, n_snap_eff, AoaMethod::Bartlett);
            r.degraded = true;
            return r;
        }
        invert_spectrum = true;

    } else {  // MUSIC
        double w[kM];
        cf64   V[kM][kM];
        jacobi_hermitian4(R, w, V);
        n_src = std::min(mdl_order(w, n_snap_eff), kM - 1);

        // Everything left after the strongest n_src directions is called noise.
        // A direction that is orthogonal to all of it must be a source, and
        // that orthogonality is far sharper than any beam pattern, which is
        // where MUSIC's resolution comes from.
        for (int i = 0; i < kM; ++i)
            for (int j = 0; j < kM; ++j) {
                cf64 sum(0, 0);
                for (int k = n_src; k < kM; ++k) sum += V[i][k] * std::conj(V[j][k]);
                M[i][j] = sum;
            }
        invert_spectrum = true;
    }

    r.n_sources = n_src;
    r.degraded  = (want != method_);

    //-- Scan ----------------------------------------------------------------
    Work& wk = work();
    double m_trace = 0.0;
    for (int i = 0; i < kM; ++i) m_trace += M[i][i].real();
    cf64 mp[6];
    for (int t = 0; t < 6; ++t) mp[t] = M[pair_i_[t]][pair_j_[t]];

    const cf32* g = gram_.data();
    double best   = invert_spectrum ?  std::numeric_limits<double>::infinity()
                                    : -std::numeric_limits<double>::infinity();
    int    best_p = 0;

    for (int ia = 0; ia < n_az_; ++ia) {
        for (int ie = 0; ie < n_el_; ++ie) {
            const std::size_t p = std::size_t(ia) * n_el_ + ie;
            const cf32* gp = g + p * 6;
            double acc = m_trace;
            for (int t = 0; t < 6; ++t) {
                const double gr = double(gp[t].real()), gi = double(gp[t].imag());
                acc += 2.0 * (mp[t].real() * gr - mp[t].imag() * gi);
            }
            if (acc < 0.0) acc = 0.0;      // Hermitian form, so only rounding
            wk.quad[p] = acc;
            const double dbv = invert_spectrum ? -10.0 * std::log10(acc + 1e-30)
                                               :  10.0 * std::log10(acc + 1e-30);
            wk.spec[p] = float(dbv);
            if (invert_spectrum ? (acc < best) : (acc > best)) { best = acc; best_p = int(p); }
        }
    }

    int pa = best_p / n_el_, pe = best_p % n_el_;

    //-- Sub-cell refinement -------------------------------------------------
    //
    // Fitting the raw quadratic form rather than its logarithm is deliberate.
    // Near a null the form vanishes quadratically -- that is what a null is --
    // so a quadratic surface is not an approximation there but the exact local
    // shape, and the fitted minimum lands on the true direction even when the
    // null is far narrower than the grid.  In decibels the same null is a
    // spike that no polynomial fits at all.
    double az_out = double(az_deg_[pa]), el_out = double(el_deg_[pe]);
    if (n_az_ >= 3 && n_el_ >= 3) {
        const int ca = clampv(pa, 1, n_az_ - 2);
        const int ce = clampv(pe, 1, n_el_ - 2);
        double patch[9];
        for (int a = 0; a < 3; ++a)
            for (int b = 0; b < 3; ++b) {
                const double q = wk.quad[std::size_t(ca - 1 + a) * n_el_ + (ce - 1 + b)];
                patch[a * 3 + b] = invert_spectrum ? q : -q;   // always minimise
            }
        double di = 0, dj = 0;
        if (quad_peak_3x3(patch, di, dj)) {
            az_out = double(az_deg_[ca]) + di * az_step_deg_;
            el_out = double(el_deg_[ce]) + dj * el_step_deg_;
        }
    }
    r.az_deg = clampv(az_out, -az_span_deg_, az_span_deg_);
    r.el_deg = clampv(el_out, -el_span_deg_, el_span_deg_);
    r.valid  = true;

    //-- Peak against the worst competing lobe -------------------------------
    const double peak_db = double(wk.spec[std::size_t(best_p)]);
    double side_db = -std::numeric_limits<double>::infinity();
    for (int ia = 0; ia < n_az_; ++ia) {
        if (std::abs(ia - pa) <= 2) continue;
        for (int ie = 0; ie < n_el_; ++ie)
            side_db = std::max(side_db, double(wk.spec[std::size_t(ia) * n_el_ + ie]));
    }
    for (int ia = std::max(0, pa - 2); ia <= std::min(n_az_ - 1, pa + 2); ++ia)
        for (int ie = 0; ie < n_el_; ++ie) {
            if (std::abs(ie - pe) <= 2) continue;
            side_db = std::max(side_db, double(wk.spec[std::size_t(ia) * n_el_ + ie]));
        }
    r.quality_db = std::isfinite(side_db) ? (peak_db - side_db) : 0.0;
    return r;
}

//============================================================================
AoaEngine::Result AoaEngine::estimate(const std::array<cf32, 4>& v) const {
    if (method_ == AoaMethod::Monopulse) return run_monopulse(v);
    return estimate(v.data(), 1);
}

AoaEngine::Result AoaEngine::estimate(const cf32* snapshots, int n_snap) const {
    if (!snapshots || n_snap <= 0) return Result{};

    if (method_ == AoaMethod::Monopulse) {
        // Coherently average the snapshots first.  A monopulse ratio is a
        // phase measurement, and phase averages coherently, so summing the
        // snapshots is exactly the right way to spend them.
        std::array<cf32, 4> v{};
        for (int s = 0; s < n_snap; ++s)
            for (int i = 0; i < 4; ++i) v[std::size_t(i)] += snapshots[s * 4 + i];
        return run_monopulse(v);
    }

    cf64 R[kM][kM] = {};
    for (int s = 0; s < n_snap; ++s) {
        const cf32* x = snapshots + s * 4;
        for (int i = 0; i < kM; ++i)
            for (int j = 0; j < kM; ++j)
                R[i][j] += cf64(x[i]) * std::conj(cf64(x[j]));
    }
    const double inv = 1.0 / double(n_snap);
    for (int i = 0; i < kM; ++i)
        for (int j = 0; j < kM; ++j) R[i][j] *= inv;

    // Fewer snapshots than elements means the covariance cannot possibly have
    // full rank on its own, so the mirrored copy is folded in.  With four or
    // more snapshots the raw estimate is left alone: the mirror assumes the
    // array is perfectly symmetric, and once there is enough data it is better
    // to believe the measurements than the assumption.
    int n_eff = n_snap;
    if (n_snap < kM) { forward_backward(R); n_eff = 2 * n_snap; }

    return run_spectrum(R, n_eff, method_);
}

} // namespace radar
