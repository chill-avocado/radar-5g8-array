//============================================================================
// cfar.cpp -- see cfar.hpp
//============================================================================
#include "radar/cfar.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace radar {

//============================================================================
// Threshold multipliers
//
// Throughout, the noise power in one cell is exponentially distributed with
// some unknown mean mu.  Every expression below is independent of mu -- that
// independence is what "constant false alarm rate" means.
//============================================================================
namespace {

/// log of C(a+b, b) via lgamma.  The binomial coefficients in the greatest-of
/// and smallest-of expressions reach 10^77 for a 100-cell half-window and are
/// multiplied by numbers around 10^-79, so they can only be formed in logs.
inline double log_binom(double n, double k) {
    return std::lgamma(n + 1.0) - std::lgamma(k + 1.0) - std::lgamma(n - k + 1.0);
}

/// Bisect a monotonically decreasing pfa(alpha) for the alpha that hits the
/// target.  Works in log(alpha) because the useful range spans nine decades
/// (alpha is order 10 for a large window and order 10^5 for a single-cell
/// one), and 200 halvings of that interval land well inside double precision.
template <typename F>
double solve_alpha(F pfa_of_alpha, double target) {
    double lo = -8.0, hi = 14.0;    // log10(alpha)
    for (int i = 0; i < 200; ++i) {
        const double mid = 0.5 * (lo + hi);
        if (pfa_of_alpha(std::pow(10.0, mid)) > target) lo = mid; else hi = mid;
    }
    return std::pow(10.0, 0.5 * (lo + hi));
}

} // namespace

double cfar_pfa_ca(double alpha, int n) {
    if (n <= 0) return 1.0;
    // The mean of n exponentials is Gamma(n, mu/n); its Laplace transform
    // evaluated at alpha gives P(cell > alpha * mean) = (1 + alpha/n)^-n.
    return std::exp(-double(n) * std::log1p(alpha / double(n)));
}

double cfar_alpha_ca(double pfa, int n) {
    if (n <= 0) return std::numeric_limits<double>::infinity();
    return double(n) * (std::pow(pfa, -1.0 / double(n)) - 1.0);
}

namespace {
/// The sum that both split-window variants share:
///   S = 2 * sum_{k=0}^{m-1} C(m-1+k, k) (2 + beta)^-(m+k),  beta = alpha/m.
/// This is exactly the smallest-of false-alarm probability, and the
/// greatest-of one is 2*(1+beta)^-m minus it.  Deriving them together is not
/// a trick: the two halves are independent Gamma variates, so the maximum and
/// the minimum have complementary cumulative distributions and their two
/// integrals must add up to twice the single-half cell-averaging answer.
double split_tail_sum(double alpha, int m) {
    const double beta   = alpha / double(m);
    const double l2b    = std::log(2.0 + beta);
    double sum = 0.0;
    for (int k = 0; k < m; ++k) {
        sum += std::exp(log_binom(double(m - 1 + k), double(k)) - (double(m + k)) * l2b);
    }
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
    // orders of magnitude smaller than the first, so this difference is not a
    // cancellation in practice.  The clamp only guards the far tail where both
    // terms underflow together.
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
    // Rohling's result, written as a ratio of Gamma functions:
    //   Pfa = (n! / (n-k)!) * Gamma(n-k+1+alpha) / Gamma(n+1+alpha)
    // which is the same as the product form prod_{i<k} (n-i)/(n-i+alpha).
    // This is exact, not an approximation: no Gaussian limit, no assumption
    // that the rank is near the median.
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
// The hot path must not allocate, and the same detector must be usable from
// several worker threads on different frames at once.  Those two requirements
// together rule out a mutable member: the buffers live in thread-local storage
// instead, sized on the first frame a thread processes and reused after that.
// A thread that only ever sees one map size allocates exactly once.
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
    std::vector<bool>   split_ok;  ///< both halves present and equal in size

    std::vector<float>  gather;    ///< reference cell values, ordered-statistic only
    std::vector<Hit>    cand;

    inline double rect(int r0, int r1, int c0, int c1) const {
        // Inclusive rows [r0,r1] and extended columns [c0,c1].
        //
        // Written as a difference of two differences rather than the usual
        // a - b - c + d.  The four corners of a large integral image are all
        // close to the running total, so subtracting the two big numbers that
        // share the same column first cancels that common part while it is
        // still exactly representable.  On a 512-by-512 map of unit-mean
        // noise the corner values reach 2.6e5 and a 17-by-17 window sums to
        // about 289; the naive order loses four significant digits of that
        // window, this one loses none.
        const std::size_t stride = std::size_t(ext_w) + 1;
        const double* top = ii.data() + std::size_t(r0)     * stride;
        const double* bot = ii.data() + std::size_t(r1 + 1) * stride;
        return (bot[c1 + 1] - top[c1 + 1]) - (bot[c0] - top[c0]);
    }
};

Cfar2D::Scratch& Cfar2D::scratch(int nr, int nd) const {
    static thread_local Scratch s;
    const int ext_w = nd + 2 * halo_d_;

    if (s.owner == this && s.nr == nr && s.nd == nd) return s;

    s.owner = this;
    s.nr = nr; s.nd = nd; s.ext_w = ext_w;
    s.ii.assign(std::size_t(nr + 1) * (ext_w + 1), 0.0);

    s.wrap.resize(ext_w);
    for (int cx = 0; cx < ext_w; ++cx) {
        int d = cx - halo_d_;
        d %= nd; if (d < 0) d += nd;
        s.wrap[cx] = d;
    }

    const int wd = 2 * halo_d_ + 1;      // Doppler window width, always full
    const int wg = 2 * guard_d_ + 1;     // Doppler guard width

    s.r_lo.resize(nr); s.r_hi.resize(nr); s.g_lo.resize(nr); s.g_hi.resize(nr);
    s.n_ref.resize(nr); s.n_lead.resize(nr); s.n_lag.resize(nr); s.os_k.resize(nr);
    s.a_ca.resize(nr); s.a_split.resize(nr); s.a_os.resize(nr);
    s.split_ok.assign(nr, false);

    for (int r = 0; r < nr; ++r) {
        const int rl = std::max(0, r - halo_r_);
        const int rh = std::min(nr - 1, r + halo_r_);
        const int gl = std::max(0, r - guard_r_);
        const int gh = std::min(nr - 1, r + guard_r_);
        s.r_lo[r] = rl; s.r_hi[r] = rh; s.g_lo[r] = gl; s.g_hi[r] = gh;

        const int n_win   = (rh - rl + 1) * wd;
        const int n_guard = (gh - gl + 1) * wg;
        const int nref    = std::max(0, n_win - n_guard);
        s.n_ref[r]  = nref;
        s.n_lead[r] = std::max(0, rh - gh) * wd;
        s.n_lag[r]  = std::max(0, gl - rl) * wd;

        s.a_ca[r] = nref > 0 ? cfar_alpha_ca(pfa_, nref) : 0.0;

        // The greatest-of / smallest-of theory assumes two half-windows of the
        // same size.  Inside the map they are; at the first and last few range
        // bins one half is clipped or missing entirely and the split test has
        // nothing to compare, so those rows fall back to plain cell averaging
        // over whatever reference cells remain.  Bin zero and its neighbours
        // are blanked for transmit leakage anyway, so this affects only the
        // last few range bins, where there is no target energy to lose.
        if (s.n_lead[r] > 0 && s.n_lead[r] == s.n_lag[r]) {
            s.split_ok[r] = true;
            s.a_split[r]  = (kind_ == CfarKind::Go) ? cfar_alpha_go(pfa_, s.n_lead[r])
                                                    : cfar_alpha_so(pfa_, s.n_lead[r]);
        } else {
            s.a_split[r] = s.a_ca[r];
        }

        const int k = std::max(1, std::min(nref, int(std::floor(0.75 * double(nref)))));
        s.os_k[r] = k;
        s.a_os[r] = nref > 0 ? cfar_alpha_os(pfa_, nref, k) : 0.0;
    }

    if (kind_ == CfarKind::Os) {
        const int worst = (2 * halo_r_ + 1) * wd;
        s.gather.reserve(std::size_t(worst));
    }
    s.cand.reserve(std::size_t(std::max(max_hits_, 1)) * 4);
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
    n_ref_nominal_ = (2 * halo_r_ + 1) * wd - (2 * guard_r_ + 1) * wg;
    if (n_ref_nominal_ < 1) n_ref_nominal_ = 1;
    os_rank_nominal_ = std::max(1, std::min(n_ref_nominal_,
                                            int(std::floor(0.75 * double(n_ref_nominal_)))));

    const int m_half = train_r_ * wd;
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
    // band folds round to the bottom, and its training cells must follow it.
    // Rather than special-case the wrap inside the inner loop, the map is
    // widened by one halo on each side with the wrapped columns copied in.
    // The prefix sum then answers every window, wrapped or not, with four
    // memory reads.
    {
        double* ii = s.ii.data();
        std::fill(ii, ii + std::size_t(stride), 0.0);
        for (int r = 0; r < nr; ++r) {
            const float* row = in.power.data() + std::size_t(r) * nd;
            const double* up = ii + std::size_t(r) * stride;
            double* cur      = ii + std::size_t(r + 1) * stride;
            cur[0] = 0.0;
            double acc = 0.0;
            for (int cx = 0; cx < ext_w; ++cx) {
                acc += double(row[s.wrap[cx]]);
                cur[cx + 1] = up[cx + 1] + acc;
            }
        }
    }

    if (kind_ == CfarKind::None) {
        // No detections, but the noise estimate is still wanted by the display.
        const double total = s.rect(0, nr - 1, 0, nd - 1);
        noise_floor_.store(total / double(std::size_t(nr) * nd), std::memory_order_relaxed);
        return;
    }

    //-- Sweep -----------------------------------------------------------------
    const int dc      = nd / 2;                     // fftshifted map: zero Doppler is centre
    const int r_start = std::min(nr, range_zero_bin_ + 1);
    const bool have_cube = in.cube_valid && in.cube.size() >= std::size_t(in.n_virt) * nr * nd;

    s.cand.clear();
    double noise_acc = 0.0;
    long   noise_n   = 0;

    for (int r = r_start; r < nr; ++r) {
        const int    rl = s.r_lo[r], rh = s.r_hi[r], gl = s.g_lo[r], gh = s.g_hi[r];
        const int    nref = s.n_ref[r];
        if (nref <= 0) continue;
        const double inv_nref = 1.0 / double(nref);
        const float* prow = in.power.data() + std::size_t(r) * nd;

        for (int d = 0; d < nd; ++d) {
            if (std::abs(d - dc) <= zero_dopp_blank_) continue;   // clutter / leakage line

            const int cx  = d + halo_d_;                 // this cell in extended coords
            const int c0  = cx - halo_d_, c1 = cx + halo_d_;
            const int gc0 = cx - guard_d_, gc1 = cx + guard_d_;

            const double win_sum   = s.rect(rl, rh, c0, c1);
            const double guard_sum = s.rect(gl, gh, gc0, gc1);
            const double ref_sum   = win_sum - guard_sum;
            const double ca_mean   = ref_sum * inv_nref;
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
                        // Greatest-of raises the threshold at a clutter edge so
                        // the step itself does not light up; smallest-of lowers
                        // it so a second target sitting in one half cannot mask
                        // the first.  They are opposite trades, which is why
                        // both are offered.
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
                    if (cell < s.a_os[r] * ca_mean * 0.25) {
                        // Cheap reject.  The k-th smallest of the reference set
                        // is at most n/(n-k) times its mean, so a cell far
                        // below the mean cannot possibly clear the ordered
                        // threshold and does not need the cells gathered.
                        continue;
                    }
                    s.gather.clear();
                    for (int rr = rl; rr <= rh; ++rr) {
                        const bool in_guard_row = (rr >= gl && rr <= gh);
                        const float* q = in.power.data() + std::size_t(rr) * nd;
                        for (int c = c0; c <= c1; ++c) {
                            if (in_guard_row && c >= gc0 && c <= gc1) continue;
                            s.gather.push_back(q[s.wrap[c]]);
                        }
                    }
                    const int k = std::min<int>(s.os_k[r], int(s.gather.size()));
                    if (k <= 0) continue;
                    std::nth_element(s.gather.begin(), s.gather.begin() + (k - 1), s.gather.end());
                    const double zk = double(s.gather[k - 1]);
                    thresh = s.a_os[r] * zk;
                    // Report the noise as a mean-equivalent so that the SNR of
                    // an ordered-statistic hit is comparable with a cell
                    // averaging one.  The k-th of n exponentials has mean
                    // mu * sum_{i=n-k+1}^{n} 1/i, so dividing by that sum
                    // converts the order statistic back to an estimate of mu.
                    double h = 0.0;
                    for (int i = int(s.gather.size()) - k + 1; i <= int(s.gather.size()); ++i)
                        h += 1.0 / double(i);
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
            h.dopp_bin  = d - dc;         // signed, zero Doppler at zero
            h.power     = cell;
            h.snr_db    = 10.0 * std::log10(cell / (noise_est + 1e-300) + 1e-300);
            // range_m and velocity_ms stay zero.  Turning a bin index into
            // metres and metres per second needs the waveform, which this
            // stage deliberately does not know about; the pipeline fills them
            // from Waveform immediately after this call and before clustering.
            if (have_cube) {
                const int nv = std::min(4, in.n_virt);
                for (int v = 0; v < nv; ++v) h.virt[std::size_t(v)] = in.cube_at(v, r, d);
            }
            s.cand.push_back(h);
        }
    }

    noise_floor_.store(noise_n ? noise_acc / double(noise_n) : 0.0, std::memory_order_relaxed);

    //-- Strongest first, then cut ------------------------------------------
    const std::size_t keep = std::min<std::size_t>(s.cand.size(), std::size_t(max_hits_));
    auto by_power = [](const Hit& a, const Hit& b) { return a.power > b.power; };
    if (keep < s.cand.size()) {
        std::partial_sort(s.cand.begin(), s.cand.begin() + keep, s.cand.end(), by_power);
    } else {
        std::sort(s.cand.begin(), s.cand.end(), by_power);
    }
    out.assign(s.cand.begin(), s.cand.begin() + keep);
}

} // namespace radar
