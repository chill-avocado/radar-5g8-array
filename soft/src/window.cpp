//============================================================================
// window.cpp -- the transform windows and their measured figures of merit
//
// Two rules run through this file.
//
// First, every window is the real thing.  Taylor is the genuine n-bar
// construction from its F_m coefficients, not a raised cosine that happens to
// look similar; Chebyshev is the genuine Dolph construction, the inverse
// transform of a ratio of Chebyshev polynomials.  A window that is 3 dB out
// where it matters would hide a target under the skirt of a larger one.
//
// Second, nothing here is a remembered constant.  Equivalent noise bandwidth,
// coherent gain, scallop loss and peak sidelobe are all measured from the
// window that was actually built, by transforming it on a grid at least
// thirty-two times finer than its own bin spacing.  That way the numbers stay
// true when somebody changes a length, a sidelobe target or a coefficient, and
// a mistake shows up as a number that moved rather than as a silent lie.
//============================================================================
#include "radar/window.hpp"

#include <algorithm>
#include <complex>
#include <vector>

namespace radar {

namespace {

//----------------------------------------------------------------------------
// A small double-precision transform, private to this file.
//
// The measurement grid runs to 32 times the window length and the levels being
// measured go down to -100 dB, so this works in double: single precision would
// put its own rounding floor uncomfortably close to a -92 dB sidelobe.  It is
// deliberately independent of radar::Fft, which is single precision and capped
// at 65536 points.
//----------------------------------------------------------------------------
using cd = std::complex<double>;

void fft_pow2(std::vector<cd>& a) {
    const std::size_t n = a.size();
    if (n < 2) return;
    for (std::size_t i = 1, j = 0; i < n; ++i) {          // bit reversal
        std::size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(a[i], a[j]);
    }
    for (std::size_t len = 2; len <= n; len <<= 1) {
        const double ang = -2.0 * kPi / double(len);
        const cd wl(std::cos(ang), std::sin(ang));
        for (std::size_t i = 0; i < n; i += len) {
            cd w(1.0, 0.0);
            for (std::size_t k = 0; k < len / 2; ++k) {
                const cd u = a[i + k];
                const cd v = a[i + k + len / 2] * w;
                a[i + k]             = u + v;
                a[i + k + len / 2]   = u - v;
                w *= wl;
            }
        }
    }
}

/// Direct transform of any length. Only the window construction uses it, and
/// only once per window, so O(n^2) in double is the right trade for exactness.
std::vector<cd> dft_any(const std::vector<cd>& x) {
    const std::size_t n = x.size();
    std::vector<cd> out(n);
    for (std::size_t k = 0; k < n; ++k) {
        double sr = 0.0, si = 0.0;
        for (std::size_t t = 0; t < n; ++t) {
            const double ang = -2.0 * kPi * double(k) * double(t) / double(n);
            const double c = std::cos(ang), s = std::sin(ang);
            sr += x[t].real() * c - x[t].imag() * s;
            si += x[t].real() * s + x[t].imag() * c;
        }
        out[k] = cd(sr, si);
    }
    return out;
}

/// Magnitude of the window's own transform on a grid at least 32x finer than
/// its bin spacing.  Index k corresponds to k * n / npad bins.
std::vector<double> padded_magnitude(const std::vector<float>& w, std::size_t& npad) {
    const std::size_t n = w.size();
    npad = 1024;
    while (npad < 32 * n) npad <<= 1;
    std::vector<cd> a(npad, cd(0.0, 0.0));
    for (std::size_t i = 0; i < n; ++i) a[i] = cd(double(w[i]), 0.0);
    fft_pow2(a);
    std::vector<double> mag(npad);
    for (std::size_t i = 0; i < npad; ++i) mag[i] = std::abs(a[i]);
    return mag;
}

//----------------------------------------------------------------------------
// The cosine-sum family, in the periodic (DFT-even) definition: the argument
// steps 2*pi*n/N rather than 2*pi*n/(N-1).  That is the definition the
// published figures of merit belong to -- Blackman-Harris reaches its quoted
// 2.0044 bins of noise bandwidth and -92 dB sidelobe only in this form -- and
// it is the right one for a window that multiplies a block before a DFT.
//----------------------------------------------------------------------------
std::vector<float> cosine_sum(int n, const double* a, int terms) {
    std::vector<float> w(static_cast<std::size_t>(n < 0 ? 0 : n));
    if (n < 2) { if (n == 1) w[0] = 1.0f; return w; }
    for (int i = 0; i < n; ++i) {
        double v = 0.0;
        for (int k = 0; k < terms; ++k) {
            const double s = (k % 2) ? -1.0 : 1.0;
            v += s * a[k] * std::cos(2.0 * kPi * double(k) * double(i) / double(n));
        }
        w[std::size_t(i)] = float(v);
    }
    return w;
}

//----------------------------------------------------------------------------
// Taylor, n-bar.  The window is 1 plus a short cosine series whose
// coefficients F_m are chosen so that the first nbar-1 sidelobes sit at the
// requested level and the rest decay like a rectangular window's.
//----------------------------------------------------------------------------
std::vector<float> taylor_window(int n, int nbar, double sll_db) {
    std::vector<float> w(static_cast<std::size_t>(n < 0 ? 0 : n));
    if (n < 2) { if (n == 1) w[0] = 1.0f; return w; }

    const double A   = std::acosh(std::pow(10.0, sll_db / 20.0)) / kPi;
    const double sp2 = double(nbar) * double(nbar) /
                       (A * A + (double(nbar) - 0.5) * (double(nbar) - 0.5));

    std::vector<double> F(static_cast<std::size_t>(nbar), 0.0);
    for (int m = 1; m <= nbar - 1; ++m) {
        double num = 1.0;
        for (int i = 1; i <= nbar - 1; ++i) {
            num *= 1.0 - double(m * m) / (sp2 * (A * A + (double(i) - 0.5) * (double(i) - 0.5)));
        }
        double den = 1.0;
        for (int i = 1; i <= nbar - 1; ++i) {
            if (i == m) continue;
            den *= 1.0 - double(m * m) / double(i * i);
        }
        F[std::size_t(m)] = ((m % 2) ? 1.0 : -1.0) * num / (2.0 * den);
    }

    double peak = 0.0;
    for (int i = 0; i < n; ++i) {
        double v = 1.0;
        for (int m = 1; m <= nbar - 1; ++m) {
            v += 2.0 * F[std::size_t(m)] *
                 std::cos(2.0 * kPi * double(m) * (double(i) - double(n - 1) * 0.5) / double(n));
        }
        w[std::size_t(i)] = float(v);
        peak = std::max(peak, v);
    }
    if (peak > 0.0) for (float& v : w) v = float(double(v) / peak);
    return w;
}

//----------------------------------------------------------------------------
// Dolph-Chebyshev.  The frequency response is written down first -- the ratio
// of Chebyshev polynomials that gives equal sidelobes everywhere -- and the
// window is its inverse transform.  Every sidelobe comes out at exactly the
// requested level, which is what makes it the narrowest window for a given
// sidelobe budget.
//----------------------------------------------------------------------------
std::vector<float> chebyshev_window(int n, double sll_db) {
    std::vector<float> w(static_cast<std::size_t>(n < 0 ? 0 : n));
    if (n < 2) { if (n == 1) w[0] = 1.0f; return w; }

    const int    order = n - 1;
    const double beta  = std::cosh(std::acosh(std::pow(10.0, sll_db / 20.0)) / double(order));

    std::vector<cd> p(static_cast<std::size_t>(n));
    for (int k = 0; k < n; ++k) {
        const double x = beta * std::cos(kPi * double(k) / double(n));
        double v;
        if (x > 1.0)       v = std::cosh(double(order) * std::acosh(x));
        else if (x < -1.0) v = (n % 2 ? 1.0 : -1.0) * std::cosh(double(order) * std::acosh(-x));
        else               v = std::cos(double(order) * std::acos(x));
        p[std::size_t(k)] = cd(v, 0.0);
    }

    std::vector<double> half;
    if (n % 2) {
        const std::vector<cd> t = dft_any(p);
        const int m = (n + 1) / 2;
        half.assign(m, 0.0);
        for (int k = 0; k < m; ++k) half[std::size_t(k)] = t[std::size_t(k)].real();
        // mirror: w = [half[m-1..1], half[0..m-1]]
        for (int k = m - 1; k >= 1; --k) w[std::size_t(m - 1 - k)] = float(half[std::size_t(k)]);
        for (int k = 0; k < m; ++k)      w[std::size_t(m - 1 + k)] = float(half[std::size_t(k)]);
    } else {
        for (int k = 0; k < n; ++k) {
            const double a = kPi * double(k) / double(n);
            p[std::size_t(k)] *= cd(std::cos(a), std::sin(a));
        }
        const std::vector<cd> t = dft_any(p);
        const int m = n / 2 + 1;
        half.assign(std::size_t(m), 0.0);
        for (int k = 0; k < m; ++k) half[std::size_t(k)] = t[std::size_t(k)].real();
        // mirror: w = [half[m-1..2], half[0..m-1]]
        int idx = 0;
        for (int k = m - 1; k >= 2; --k) w[std::size_t(idx++)] = float(half[std::size_t(k)]);
        for (int k = 0; k < m; ++k)      w[std::size_t(idx++)] = float(half[std::size_t(k)]);
    }

    double peak = 0.0;
    for (float v : w) peak = std::max(peak, double(v));
    if (peak > 0.0) for (float& v : w) v = float(double(v) / peak);
    return w;
}

/// The sidelobe target when the caller did not name one.  45 dB is the classic
/// Taylor default and is used for Chebyshev too, so there is one rule to know.
constexpr double kDefaultSidelobeDb = 45.0;

} // namespace

//============================================================================
// Public interface
//============================================================================
std::vector<float> make_window(WindowKind k, int n, double param) {
    if (n <= 0) return {};
    switch (k) {
        case WindowKind::Rect:
            return std::vector<float>(std::size_t(n), 1.0f);
        case WindowKind::Hann: {
            static const double a[2] = {0.5, 0.5};
            return cosine_sum(n, a, 2);
        }
        case WindowKind::Hamming: {
            static const double a[2] = {0.54, 0.46};
            return cosine_sum(n, a, 2);
        }
        case WindowKind::BlackmanHarris: {
            // The 4-term minimum-sidelobe set, -92 dB.  These four numbers sum
            // to exactly 1, so the window peaks at 1 without normalisation.
            static const double a[4] = {0.35875, 0.48829, 0.14128, 0.01168};
            return cosine_sum(n, a, 4);
        }
        case WindowKind::Taylor:
            return taylor_window(n, 5, param > 0.0 ? param : kDefaultSidelobeDb);
        case WindowKind::Chebyshev:
            return chebyshev_window(n, param > 0.0 ? param : kDefaultSidelobeDb);
    }
    return std::vector<float>(std::size_t(n), 1.0f);
}

double window_coherent_gain(const std::vector<float>& w) {
    if (w.empty()) return 0.0;
    double s = 0.0;
    for (float v : w) s += double(v);
    return s / double(w.size());
}

double window_enbw(const std::vector<float>& w) {
    if (w.empty()) return 0.0;
    double s1 = 0.0, s2 = 0.0;
    for (float v : w) { s1 += double(v); s2 += double(v) * double(v); }
    if (s1 == 0.0) return 0.0;
    return double(w.size()) * s2 / (s1 * s1);
}

double window_scallop_loss_db(const std::vector<float>& w) {
    // Worst case is a tone exactly between two bins.  That point is evaluated
    // directly rather than read off the padded grid: half a bin does not
    // generally land on a power-of-two grid point, and a value that matters to
    // a hundredth of a dB should not be interpolated.
    const std::size_t n = w.size();
    if (n == 0) return 0.0;
    double dc = 0.0, hr = 0.0, hi = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        const double ang = kPi * double(i) / double(n);   // half a bin
        dc += double(w[i]);
        hr += double(w[i]) * std::cos(ang);
        hi -= double(w[i]) * std::sin(ang);
    }
    const double half = std::sqrt(hr * hr + hi * hi);
    if (dc <= 0.0 || half <= 0.0) return 0.0;
    return 20.0 * std::log10(dc / half);
}

double window_peak_sidelobe_db(const std::vector<float>& w) {
    if (w.size() < 4) return 0.0;
    std::size_t npad = 0;
    const std::vector<double> mag = padded_magnitude(w, npad);
    const double peak = mag[0];
    if (peak <= 0.0) return 0.0;

    // Walk out of the main lobe: the first place the response stops falling is
    // its edge.  Everything past that, up to the halfway point (the response
    // of a real window is symmetric about zero), is sidelobe.
    std::size_t i = 1;
    while (i + 1 < npad / 2 && mag[i] < mag[i - 1]) ++i;

    double worst = 0.0;
    for (std::size_t k = i; k <= npad / 2; ++k) worst = std::max(worst, mag[k]);
    if (worst <= 0.0) return -300.0;
    return 20.0 * std::log10(worst / peak);
}

std::vector<i16> quantise_window(const std::vector<float>& w) {
    std::vector<i16> q(w.size());
    for (std::size_t i = 0; i < w.size(); ++i) q[i] = fx::to_q15(double(w[i]));
    return q;
}

const char* window_name(WindowKind k) {
    switch (k) {
        case WindowKind::Rect:           return "rect";
        case WindowKind::Hann:           return "hann";
        case WindowKind::Hamming:        return "hamming";
        case WindowKind::BlackmanHarris: return "blackman-harris";
        case WindowKind::Taylor:         return "taylor";
        case WindowKind::Chebyshev:      return "chebyshev";
    }
    return "unknown";
}

} // namespace radar
