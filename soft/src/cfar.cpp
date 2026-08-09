//============================================================================
// cfar.cpp -- see cfar.hpp
//============================================================================
#include "radar/cfar.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>

namespace radar {

//============================================================================
// Threshold multipliers
//
// Throughout, the noise power in one cell is exponentially distributed with
// some unknown mean mu.  Every expression below is independent of mu -- that
// independence is what "constant false alarm rate" means.
//============================================================================
namespace {

/// log of C(n, k) via lgamma.  The binomial coefficients in the greatest-of
/// and smallest-of expressions reach 10^77 for a hundred-cell half-window and
/// get multiplied by numbers near 10^-79, so they can only be formed in logs.
inline double log_binom(double n, double k) {
    return std::lgamma(n + 1.0) - std::lgamma(k + 1.0) - std::lgamma(n - k + 1.0);
}

/// Bisect a monotonically decreasing pfa(alpha) for the alpha that hits the
/// target.  Works in log10(alpha) because the useful range spans nine decades
/// -- alpha is of order 10 for a large window and of order 100000 for a
/// single-cell one -- and 200 halvings of that interval land well inside
/// double precision.
template <typename F>
double solve_alpha(F pfa_of_alpha, double target) {
    double lo = -8.0, hi = 14.0;
    for (int i = 0; i < 200; ++i) {
        const double mid = 0.5 * (lo + hi);
        if (pfa_of_alpha(std::pow(10.0, mid)) > target) lo = mid; else hi = mid;
    }
    return std::pow(10.0, 0.5 * (lo + hi));
}

} // namespace

double cfar_pfa_ca(double alpha, int n) {
    if (n <= 0) return 1.0;
    // The mean of n exponentials is Gamma(n, mu/n); its Laplace transform at
    // alpha gives P(cell > alpha * mean) = (1 + alpha/n)^-n directly.
    return std::exp(-double(n) * std::log1p(alpha / double(n)));
}

double cfar_alpha_ca(double pfa, int n) {
    if (n <= 0) return std::numeric_limits<double>::infinity();
    return double(n) * (std::pow(pfa, -1.0 / double(n)) - 1.0);
}

namespace {
/// The sum the two split-window variants share:
///   S(alpha) = 2 * sum_{k=0}^{m-1} C(m-1+k, k) (2 + alpha/m)^-(m+k).
/// This is exactly the smallest-of false-alarm probability, and the
/// greatest-of one is 2*(1 + alpha/m)^-m minus it.  That the two must add up
/// to twice the single-half cell-averaging answer is not a coincidence: the
/// maximum and the minimum of two independent variates have complementary
/// distribution functions, so their two integrals recombine into two copies
/// of the one-sided result.  It is also a free check on the arithmetic.
double split_tail_sum(double alpha, int m) {
    const double beta = alpha / double(m);
    const double l2b  = std::log(2.0 + beta);
    double sum = 0.0;
    for (int k = 0; k < m; ++k)
        sum += std::exp(log_binom(double(m - 1 + k), double(k)) - double(m + k) * l2b);
    return 2.0 * sum;
}
} // namespace

double cfar_pfa_so(double alpha, int m) {
    if (m <= 0) return 1.0;
    return std::min(1.0, split_tail_sum(alpha, m));
}

double cfar_pfa_go(double alpha, int m) {
    if (m <= 0) return 1.0;
    const double both = 2.0 * cfar_pfa_ca(alpha, m);
    const double p    = both - split_tail_sum(alpha, m);
    // At any threshold worth using the subtracted term is more than thirty
    // orders of magnitude below the first, so this is not a cancellation in
    // practice.  The clamp only guards the far tail where both underflow.
    return p > 0.0 ? std::min(1.0, p) : std::numeric_limits<double>::denorm_min();
}

double cfar_alpha_go(double pfa, int m) {
    if (m <= 0) return std::numeric_limits<double>::infinity();
    return solve_alpha([m](double a) { return cfar_pfa_go(a, m); }, pfa);
}

double cfar_alpha_so(double pfa, int m) {
    if (m <= 0) return std::numeric_limits<double>::infinity();
    return solve_alpha([m](double a) { return cfar_pfa_so(a, m); }, pfa);
}

double cfar_pfa_os(double alpha, int n, int k) {
    if (n <= 0 || k <= 0 || k > n) return 1.0;
    // Rohling's ordered-statistic result, written as a ratio of Gamma
    // functions:  Pfa = (n!/(n-k)!) * Gamma(n-k+1+alpha) / Gamma(n+1+alpha),
    // identical to the product form prod_{i<k} (n-i)/(n-i+alpha).  This is
    // exact: no Gaussian limit, no assumption that the rank sits near the
    // median.  The multiplier that comes back is therefore the right one and
    // not an approximation with an error to quote.
    const double lg = std::lgamma(double(n) + 1.0) - std::lgamma(double(n - k) + 1.0)
                    + std::lgamma(double(n - k) + 1.0 + alpha)
                    - std::lgamma(double(n) + 1.0 + alpha);
    return std::min(1.0, std::exp(lg));
}

double cfar_alpha_os(double pfa, int n, int k) {
    if (n <= 0 || k <= 0 || k > n) return std::numeric_limits<double>::infinity();
    return solve_alpha([n, k](double a) { return cfar_pfa_os(a, n, k); }, pfa);
}

//============================================================================
// Per-thread working storage
//
// The hot path must not allocate, and one detector must be usable from several
// worker threads on different frames at once.  Those two requirements together
// rule out a mutable member, so the buffers live in thread-local storage,
// sized on the first frame a thread processes and reused afterwards.  A thread
// that only ever sees one map size allocates exactly once.
//============================================================================
struct Cfar2D::Scratch {
    const void* owner = nullptr;   ///< which Cfar2D built the row tables
    int nr = 0, nd = 0, ext_w = 0;

    std::vector<double> ii;        ///< integral image, (nr+1) * (ext_w+1)
    std::vector<int>    wrap;      ///< extended column -> real Doppler column

    // Per range row: the clipped window and the multipliers that go with it.
    std::vector<int>    r_lo, r_hi, g_lo, g_hi;
    std::vector<int>    n_ref, n_lead, n_lag, os_k;
    std::vector<double> a_ca, a_split, a_os;
    std::vector<char>   split_ok;  ///< both halves present and the same size

    std::vector<float>  gather;    ///< reference values, ordered statistic only
    std::vector<Hit>    cand;

    inline double rect(int r0, int r1, int c0, int c1) const {
        // Inclusive rows [r0,r1] and extended columns [c0,c1].
        //
        // Written as a difference of two differences rather than the usual
        // a - b - c + d.  All four corners of a large integral image sit close
        // to the running total, so subtracting the two that share a column
        // first cancels the common part while it is still exactly
        // representable.  On a 512-by-512 map of unit-mean noise the corners
        // reach 2.6e5 while a 17-by-17 window sums to about 289; the naive
        // order throws away four significant digits of that window, this one
        // throws away none.
        const std::size_t stride = std::size_t(ext_w) + 1;
        const double* top = ii.data() + std::size_t(r0)     * stride;
        const double* bot = ii.data() + std::size_t(r1 + 1) * stride;
        return (bot[c1 + 1] - top[c1 + 1]) - (bot[c0] - top[c0]);
    }
};

Cfar2D::Scratch& Cfar2D::scratch(int nr, int nd) const {
    static thread_local Scratch s;
    if (s.owner == this && s.nr == nr && s.nd == nd) return s;

    const int ext_w = nd + 2 * halo_d_;
    s.owner = this;
    s.nr = nr; s.nd = nd; s.ext_w = ext_w;
    s.ii.assign(std::size_t(nr + 1) * (ext_w + 1), 0.0);

    s.wrap.resize(ext_w);
    for (int cx = 0; cx < ext_w; ++cx) {
        int d = (cx - halo_d_) % nd;
        if (d < 0) d += nd;
        s.wrap[cx] = d;
    }

    const int wd = 2 * halo_d_ + 1;      // Doppler window width, always full
    const int wg = 2 * guard_d_ + 1;     // Doppler guard width

    s.r_lo.resize(nr); s.r_hi.resize(nr); s.g_lo.resize(nr); s.g_hi.resize(nr);
    s.n_ref.resize(nr); s.n_lead.resize(nr); s.n_lag.resize(nr); s.os_k.resize(nr);
    s.a_ca.resize(nr); s.a_split.resize(nr); s.a_os.resize(nr);
    s.split_ok.assign(nr, 0);

    // Only the handful of rows near the two ends of the map have a clipped
    // window, so at most a couple of dozen distinct cell counts occur.  The
    // greatest-of and smallest-of inverses each cost a two-hundred-step
    // bisection over a hundred-term sum, which is worth doing once per count
    // and not once per row.
    std::map<int, double> memo_ca, memo_split, memo_os;
    auto get = [](std::map<int, double>& m, int key, auto&& f) {
        auto it = m.find(key);
        if (it != m.end()) return it->second;
        const double v = f(key);
        m.emplace(key, v);
        return v;
    };

    for (int r = 0; r < nr; ++r) {
        const int rl = std::max(0, r - halo_r_);
        const int rh = std::min(nr - 1, r + halo_r_);
        const int gl = std::max(0, r - guard_r_);
        const int gh = std::min(nr - 1, r + guard_r_);
        s.r_lo[r] = rl; s.r_hi[r] = rh; s.g_lo[r] = gl; s.g_hi[r] = gh;

        const int nref = std::max(0, (rh - rl + 1) * wd - (gh - gl + 1) * wg);
        s.n_ref[r]  = nref;
        s.n_lead[r] = std::max(0, rh - gh) * wd;
        s.n_lag[r]  = std::max(0, gl - rl) * wd;
        if (nref <= 0) { s.a_ca[r] = s.a_split[r] = s.a_os[r] = 0.0; s.os_k[r] = 0; continue; }

        const double pfa = pfa_;
        s.a_ca[r] = get(memo_ca, nref, [pfa](int n) { return cfar_alpha_ca(pfa, n); });

        // The greatest-of / smallest-of theory assumes two half-windows of the
        // same size.  Inside the map they are; at the very first and very last
        // range bins one half is clipped or missing and the split test has
        // nothing to compare, so those rows fall back to plain cell averaging
        // over whatever reference cells remain.  The low end is blanked for
        // transmit leakage anyway, so in practice this touches only the last
        // few range bins, at the limit of the instrumented range.
        if (s.n_lead[r] > 0 && s.n_lead[r] == s.n_lag[r]) {
            s.split_ok[r] = 1;
            const CfarKind k = kind_;
            s.a_split[r] = get(memo_split, s.n_lead[r], [pfa, k](int m) {
                return (k == CfarKind::Go) ? cfar_alpha_go(pfa, m) : cfar_alpha_so(pfa, m);
            });
        } else {
            s.a_split[r] = s.a_ca[r];
        }

        const int k = std::max(1, std::min(nref, int(std::floor(0.75 * double(nref)))));
        s.os_k[r] = k;
        s.a_os[r] = get(memo_os, nref, [pfa, k](int n) { return cfar_alpha_os(pfa, n, k); });
    }

    if (kind_ == CfarKind::Os) s.gather.reserve(std::size_t((2 * halo_r_ + 1) * wd));
    s.cand.reserve(std::size_t(max_hits_) * 4u);
    return s;
}

//============================================================================
Cfar2D::Cfar2D(const Config& cfg)
    : kind_(cfg.cfar_kind),
      guard_r_(std::max(0, cfg.guard_range)),
      guard_d_(std::max(0, cfg.guard_dopp)),
      train_r_(std::max(0, cfg.train_range)),
      train_d_(std::max(0, cfg.train_dopp)),
      halo_r_(guard_r_ + train_r_),
      halo_d_(guard_d_ + train_d_),
      max_hits_(std::max(1, cfg.max_hits)),
      zero_dopp_blank_(std::max(0, cfg.zero_dopp_blank)),
      range_zero_bin_(std::max(0, cfg.range_zero_bin)),
      pfa_(cfg.pfa > 0.0 && cfg.pfa < 1.0 ? cfg.pfa : 1e-5) {

    const int wd = 2 * halo_d_ + 1;
    const int wg = 2 * guard_d_ + 1;
    n_ref_nominal_   = std::max(1, (2 * halo_r_ + 1) * wd - (2 * guard_r_ + 1) * wg);
    os_rank_nominal_ = std::max(1, std::min(n_ref_nominal_,
                                            int(std::floor(0.75 * double(n_ref_nominal_)))));
    const int m_half = std::max(1, train_r_ * wd);

    switch (kind_) {
        case CfarKind::Ca:
            alpha_nominal_ = cfar_alpha_ca(pfa_, n_ref_nominal_);
            realised_pfa_  = cfar_pfa_ca(alpha_nominal_, n_ref_nominal_);
            break;
        case CfarKind::Go:
            alpha_nominal_ = cfar_alpha_go(pfa_, m_half);
            realised_pfa_  = cfar_pfa_go(alpha_nominal_, m_half);
            break;
        case CfarKind::So:
            alpha_nominal_ = cfar_alpha_so(pfa_, m_half);
            realised_pfa_  = cfar_pfa_so(alpha_nominal_, m_half);
            break;
        case CfarKind::Os:
            alpha_nominal_ = cfar_alpha_os(pfa_, n_ref_nominal_, os_rank_nominal_);
            realised_pfa_  = cfar_pfa_os(alpha_nominal_, n_ref_nominal_, os_rank_nominal_);
            break;
        case CfarKind::None:
        default:
            alpha_nominal_ = 0.0;
            realised_pfa_  = 0.0;
            break;
    }
}

//============================================================================
void Cfar2D::detect(const RdFrame& in, std::vector<Hit>& out) const {
    out.clear();
    const int nr = in.n_range, nd = in.n_doppler;
    if (nr <= 0 || nd <= 0 || in.power.size() < std::size_t(nr) * nd) return;

    Scratch& s = scratch(nr, nd);
    const int ext_w  = s.ext_w;
    const int stride = ext_w + 1;

    //-- Integral image over a Doppler-extended copy of the map --------------
    //
    // Doppler is periodic: a target at the top of the unambiguous velocity
    // band folds round to the bottom, and its training cells have to follow it
    // round.  Rather than branch on the wrap inside the inner loop, the map is
    // widened by one halo at each side with the wrapped columns copied in.
    // The prefix sum then answers every window, wrapped or not, from four
    // memory reads regardless of how large the window is.
    {
        double* ii = s.ii.data();
        std::fill(ii, ii + std::size_t(stride), 0.0);
        const int* wrap = s.wrap.data();
        for (int r = 0; r < nr; ++r) {
            const float*  row = in.power.data() + std::size_t(r) * nd;
            const double* up  = ii + std::size_t(r) * stride;
            double*       cur = ii + std::size_t(r + 1) * stride;
            cur[0] = 0.0;
            double acc = 0.0;
            for (int cx = 0; cx < ext_w; ++cx) {
                acc += double(row[wrap[cx]]);
                cur[cx + 1] = up[cx + 1] + acc;
            }
        }
    }

    if (kind_ == CfarKind::None) {
        // No detections, but the display still wants the noise level.
        noise_floor_.store(s.rect(0, nr - 1, halo_d_, halo_d_ + nd - 1)
                               / double(std::size_t(nr) * nd),
                           std::memory_order_relaxed);
        return;
    }

    //-- Sweep ---------------------------------------------------------------
    // Doppler bin nd/2 is zero velocity: the map arrives fftshifted so the
    // display has approach and recession either side of centre, and Hit's
    // dopp_bin is defined signed about that centre.
    const int  dc        = nd / 2;
    const int  r_start   = std::min(nr, range_zero_bin_ + 1);
    const bool have_cube = in.cube_valid
                        && in.cube.size() >= std::size_t(in.n_virt) * nr * nd;
    const std::size_t cand_cap = std::size_t(max_hits_) * 16u;

    s.cand.clear();
    double noise_acc = 0.0;
    long   noise_n   = 0;

    auto by_power = [](const Hit& a, const Hit& b) { return a.power > b.power; };

    for (int r = r_start; r < nr; ++r) {
        const int nref = s.n_ref[r];
        if (nref <= 0) continue;
        const int    rl = s.r_lo[r], rh = s.r_hi[r], gl = s.g_lo[r], gh = s.g_hi[r];
        const double inv_nref = 1.0 / double(nref);
        const float* prow = in.power.data() + std::size_t(r) * nd;

        for (int d = 0; d < nd; ++d) {
            if (std::abs(d - dc) <= zero_dopp_blank_) continue;  // clutter / leakage line

            const int cx  = d + halo_d_;                 // this cell, extended coords
            const int c0  = cx - halo_d_,  c1  = cx + halo_d_;
            const int gc0 = cx - guard_d_, gc1 = cx + guard_d_;

            const double ref_sum = s.rect(rl, rh, c0, c1) - s.rect(gl, gh, gc0, gc1);
            const double ca_mean = ref_sum * inv_nref;
            noise_acc += ca_mean;
            ++noise_n;

            const double cell = double(prow[d]);
            double thresh, noise_est;

            switch (kind_) {
                case CfarKind::Go:
                case CfarKind::So: {
                    if (s.split_ok[r]) {
                        const double lead = s.rect(gh + 1, rh, c0, c1) / double(s.n_lead[r]);
                        const double lag  = s.rect(rl, gl - 1, c0, c1) / double(s.n_lag[r]);
                        // Greatest-of lifts the threshold at a clutter edge so
                        // the step itself does not light up.  Smallest-of drops
                        // it so a second target sitting in one half cannot mask
                        // the first.  Opposite trades, which is why both exist.
                        noise_est = (kind_ == CfarKind::Go) ? std::max(lead, lag)
                                                            : std::min(lead, lag);
                        thresh    = s.a_split[r] * noise_est;
                    } else {
                        noise_est = ca_mean;
                        thresh    = s.a_ca[r] * ca_mean;
                    }
                    break;
                }
                case CfarKind::Os: {
                    // No shortcut is available here.  A rank statistic cannot
                    // be bounded from below by the window mean, so every cell
                    // has to have its reference set gathered.  nth_element
                    // partitions in linear time and never sorts the rest,
                    // which is the only saving on offer.
                    s.gather.clear();
                    for (int rr = rl; rr <= rh; ++rr) {
                        const bool guard_row = (rr >= gl && rr <= gh);
                        const float* q = in.power.data() + std::size_t(rr) * nd;
                        for (int c = c0; c <= c1; ++c) {
                            if (guard_row && c >= gc0 && c <= gc1) continue;
                            s.gather.push_back(q[s.wrap[c]]);
                        }
                    }
                    const int ng = int(s.gather.size());
                    const int k  = std::min(s.os_k[r], ng);
                    if (k <= 0) continue;
                    std::nth_element(s.gather.begin(), s.gather.begin() + (k - 1), s.gather.end());
                    const double zk = double(s.gather[k - 1]);
                    thresh = s.a_os[r] * zk;
                    // Convert the order statistic back into a mean-equivalent
                    // so an ordered-statistic hit's SNR is comparable with a
                    // cell-averaging one.  The k-th of n exponentials has mean
                    // mu * sum_{i=n-k+1}^{n} 1/i, so dividing by that harmonic
                    // tail removes the bias the ranking introduces.
                    double h = 0.0;
                    for (int i = ng - k + 1; i <= ng; ++i) h += 1.0 / double(i);
                    noise_est = h > 0.0 ? zk / h : zk;
                    break;
                }
                case CfarKind::Ca:
                default:
                    noise_est = ca_mean;
                    thresh    = s.a_ca[r] * ca_mean;
                    break;
            }

            if (!(cell > thresh)) continue;

            Hit h;
            h.range_bin = r;
            h.dopp_bin  = d - dc;
            h.power     = cell;
            h.snr_db    = 10.0 * std::log10(cell / (noise_est + 1e-300) + 1e-300);
            // range_m and velocity_ms stay zero.  Turning a bin index into
            // metres and metres per second needs the waveform, which this
            // stage deliberately does not know about; the pipeline fills them
            // from Waveform straight after this call and before clustering.
            if (have_cube) {
                const int nv = std::min(4, in.n_virt);
                for (int v = 0; v < nv; ++v) h.virt[std::size_t(v)] = in.cube_at(v, r, d);
            }
            s.cand.push_back(h);

            // A map with a strong ridge in it can put a large fraction of its
            // cells over threshold.  Cutting back to the strongest whenever the
            // list gets far larger than the output cap bounds both the memory
            // and the final sort without changing the answer, since everything
            // discarded is weaker than everything kept.
            if (s.cand.size() >= cand_cap) {
                std::partial_sort(s.cand.begin(), s.cand.begin() + max_hits_,
                                  s.cand.end(), by_power);
                s.cand.resize(std::size_t(max_hits_));
            }
        }
    }

    noise_floor_.store(noise_n ? noise_acc / double(noise_n) : 0.0, std::memory_order_relaxed);

    //-- Strongest first, then cut -------------------------------------------
    const std::size_t keep = std::min<std::size_t>(s.cand.size(), std::size_t(max_hits_));
    if (keep < s.cand.size())
        std::partial_sort(s.cand.begin(), s.cand.begin() + keep, s.cand.end(), by_power);
    else
        std::sort(s.cand.begin(), s.cand.end(), by_power);
    out.assign(s.cand.begin(), s.cand.begin() + keep);
}

} // namespace radar
