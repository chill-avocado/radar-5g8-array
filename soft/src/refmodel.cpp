//============================================================================
// refmodel.cpp -- the FPGA datapath, reproduced in integers
//
// WHY THE TRANSFORM IS DONE THE HARD WAY
// --------------------------------------
// It would be much less code to run the range and Doppler transforms in
// floating point and round the answer to 16 bits at the end.  It would also be
// wrong, and wrong in the direction that hides problems: a float transform
// keeps every small signal that a 16-bit one loses, so the model would be
// quieter than the hardware and every weak-target question would get the
// wrong answer.  The transform below is therefore a real 16-bit
// decimation-in-frequency radix-2, scaled stage by stage exactly as the fabric
// scales it, saturating exactly where the fabric saturates.
//
// SIGN CONVENTIONS, WHICH ARE LOAD BEARING
// ----------------------------------------
// De-chirp is conjugate(received) times reference, as core.hpp specifies, so a
// target's beat frequency is positive and range grows with bin index.  That
// same conjugation makes an approaching target's phase run backwards across
// chirps, which a forward transform would place in a negative Doppler bin.  So
// the Doppler transform uses the conjugate kernel -- in the fabric, the same
// butterflies with the twiddle table's sign flipped, no extra hardware -- and
// approaching targets come out at positive Doppler, matching Hit::velocity_ms
// and Waveform::velocity_of_bin().
//
// The Doppler axis is then rotated so zero velocity sits at index
// n_doppler / 2, because that is the map an operator looks at.
//
// HOW IT IS MADE FAST WITHOUT MOVING A SINGLE BIT
// -----------------------------------------------
// A coherent interval is 16 ms of radio time and the naive version of this
// file took 600 ms to process it, so the laptop could not keep up with its own
// simulator, let alone a live radio.  Two changes fix that, and neither of
// them changes an answer:
//
//   Eight transforms at a time.  The chirps are independent, so eight of them
//   are interleaved eight ways in memory -- sample k of transform u at
//   x[k*8 + u].  Every butterfly then reads sixteen consecutive 16-bit values,
//   one twiddle serves all eight transforms as a broadcast, and the whole
//   thing is one 256-bit instruction per operation with no strided access at
//   any stage.  The awkward part is that the arithmetic has to stay exactly
//   what the scalar version does, including a 33-bit intermediate that does
//   not fit a 32-bit lane; sub_round_shift() below is how that is done.
//
//   Threads.  Chirps do not talk to each other until the corner turn, so the
//   range transforms spread across cores with no locking at all.
//
// fixed_fft_x8() is checked against eight calls to fixed_fft() on random data,
// sample for sample, by radar-selftest.  Anything else would be a tolerance,
// and a tolerance is exactly what a reference model must not have.
//============================================================================
#include "radar/refmodel.hpp"

#include "radar/window.hpp"

#include <algorithm>
#include <atomic>
#include <cstring>
#include <thread>

#if defined(__AVX2__)
#  include <immintrin.h>
#  define RADAR_REF_AVX2 1
#endif

namespace radar {

namespace {

//----------------------------------------------------------------------------
// Modified Bessel function of the first kind, order zero.  Series summed until
// a term stops mattering; the RTL's generator does the same and the two must
// agree to the last integer.
//----------------------------------------------------------------------------
double bessel_i0(double x) {
    double sum = 1.0, term = 1.0;
    const double hx = x * 0.5;
    for (int k = 1; k < 200; ++k) {
        term *= (hx / double(k)) * (hx / double(k));
        sum  += term;
        if (term < 1e-18 * sum) break;
    }
    return sum;
}

std::vector<int> make_bitrev(int n) {
    const int bits = log2i(std::size_t(n));
    std::vector<int> p(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
        int r = 0;
        for (int b = 0; b < bits; ++b) if (i & (1 << b)) r |= 1 << (bits - 1 - b);
        p[std::size_t(i)] = r;
    }
    return p;
}

/// Rotate a cyclic buffer left by k places -- the fabric's address bit flip.
template <typename T>
void rotate_left(T* p, int n, int k) {
    if (n <= 1 || k <= 0 || k >= n) return;
    std::rotate(p, p + k, p + n);
}

//----------------------------------------------------------------------------
// One scalar butterfly, the definition everything else is checked against.
//----------------------------------------------------------------------------
inline void butterfly(ci16& xa, ci16& xb, ci16 w, int sc, bool unit_twiddle) {
    const ci16 a = xa, b = xb;
    const i32 sr = i32(a.re) + i32(b.re);
    const i32 si = i32(a.im) + i32(b.im);
    const i32 dr = i32(a.re) - i32(b.re);
    const i32 di = i32(a.im) - i32(b.im);
    xa = ci16(i16(fx::round_sat(sr, sc, 16)), i16(fx::round_sat(si, sc, 16)));
    if (unit_twiddle) {
        // The fabric has no multiplier where the twiddle is exactly one.
        xb = ci16(i16(fx::round_sat(dr, sc, 16)), i16(fx::round_sat(di, sc, 16)));
    } else {
        const i64 rr = i64(dr) * w.re - i64(di) * w.im;
        const i64 ii = i64(dr) * w.im + i64(di) * w.re;
        xb = ci16(i16(fx::round_sat(rr, 15 + sc, 16)),
                  i16(fx::round_sat(ii, 15 + sc, 16)));
    }
}

#if defined(RADAR_REF_AVX2)

//----------------------------------------------------------------------------
// round_sat(P - Q, s, 16) with everything kept inside 32-bit lanes.
//
// P and Q are the two halves of a twiddle product, each already a sum of two
// 16x16 products.  Their difference needs 33 bits, which a 32-bit lane does
// not have, so the subtraction is done on the halves instead:
//
//   P - Q + 2^(s-1) = 2 * [ (P>>1) - (Q>>1) + 2^(s-2) ] + [ (P&1) - (Q&1) ]
//
// Call the bracket D and the low-bit difference e, which is -1, 0 or +1.  Then
// the rounded, shifted result is D >> (s-1) exactly, except in the one case
// where e is negative and D sits precisely on a multiple of 2^(s-1), where it
// is one less.  Both P and Q are bounded by 32768 * sqrt(wr^2 + wi^2), which
// is 1.52e9 for a unit-modulus twiddle, so their halves subtract without
// coming near the edge of a signed 32-bit lane.
//
// The correction fires about fifteen times in a million butterflies.  It is
// still computed, because a reference model that is right most of the time is
// not a reference model.
//----------------------------------------------------------------------------
inline __m256i sub_round_shift(__m256i P, __m256i Q, int s) {
    const __m256i one = _mm256_set1_epi32(1);
    const __m256i D   = _mm256_add_epi32(
        _mm256_sub_epi32(_mm256_srai_epi32(P, 1), _mm256_srai_epi32(Q, 1)),
        _mm256_set1_epi32(1 << (s - 2)));
    const __m256i R = _mm256_sra_epi32(D, _mm_cvtsi32_si128(s - 1));
    // e < 0 exactly when P is even and Q is odd.
    const __m256i eneg = _mm256_cmpeq_epi32(
        _mm256_and_si256(_mm256_andnot_si256(P, Q), one), one);
    const __m256i onmul = _mm256_cmpeq_epi32(
        _mm256_and_si256(D, _mm256_set1_epi32((1 << (s - 1)) - 1)),
        _mm256_setzero_si256());
    // A true comparison is all ones, which is -1: adding it subtracts one.
    return _mm256_add_epi32(R, _mm256_and_si256(eneg, onmul));
}

/// a+b or a-b for eight complex, rounded, shifted and saturated back to 16
/// bits.  `coef` is (1, 1) for the sum and (1, -1) for the difference.
inline __m256i add_round_pack(__m256i a, __m256i b, int sc, __m256i coef) {
    const __m256i lo = _mm256_unpacklo_epi16(a, b);
    const __m256i hi = _mm256_unpackhi_epi16(a, b);
    __m256i vlo = _mm256_madd_epi16(lo, coef);
    __m256i vhi = _mm256_madd_epi16(hi, coef);
    if (sc > 0) {
        const __m256i h = _mm256_set1_epi32(1 << (sc - 1));
        const __m128i c = _mm_cvtsi32_si128(sc);
        vlo = _mm256_sra_epi32(_mm256_add_epi32(vlo, h), c);
        vhi = _mm256_sra_epi32(_mm256_add_epi32(vhi, h), c);
    }
    return _mm256_packs_epi32(vlo, vhi);
}

/// (a - b) * w for eight complex, rounded by 15 + sc and saturated.
inline __m256i twiddle_pack(__m256i a, __m256i b, i32 wa, i32 wb, int sc) {
    const __m256i W1 = _mm256_set1_epi32(wa);   // (wr, -wi)
    const __m256i W2 = _mm256_set1_epi32(wb);   // (wi,  wr)
    const __m256i Pre = _mm256_madd_epi16(a, W1);
    const __m256i Qre = _mm256_madd_epi16(b, W1);
    const __m256i Pim = _mm256_madd_epi16(a, W2);
    const __m256i Qim = _mm256_madd_epi16(b, W2);
    const int s = 15 + sc;
    const __m256i Rre = sub_round_shift(Pre, Qre, s);
    const __m256i Rim = sub_round_shift(Pim, Qim, s);
    return _mm256_packs_epi32(_mm256_unpacklo_epi32(Rre, Rim),
                              _mm256_unpackhi_epi32(Rre, Rim));
}

#endif // RADAR_REF_AVX2

/// Scratch for one chirp through the range chain.  Thread-local, because the
/// pipeline runs several chirps at once and they must not share it.
struct ChirpScratch {
    std::vector<ci16> dech, hb1, hb2, work;
};
ChirpScratch& chirp_scratch() {
    static thread_local ChirpScratch s;
    return s;
}

/// Run `f(i)` for i in [0, n), across `nthreads` threads, work-stealing so an
/// uneven chirp does not leave a core idle.
template <typename F>
void parallel_for(int n, int nthreads, F&& f) {
    if (nthreads <= 1 || n <= 1) {
        for (int i = 0; i < n; ++i) f(i);
        return;
    }
    const int t_max = std::min(nthreads, n);
    std::atomic<int> next{0};
    auto worker = [&] { for (int i = next++; i < n; i = next++) f(i); };
    std::vector<std::thread> pool;
    pool.reserve(std::size_t(t_max - 1));
    for (int t = 1; t < t_max; ++t) pool.emplace_back(worker);
    worker();
    for (std::thread& t : pool) t.join();
}

} // namespace

//============================================================================
// Generated tables
//============================================================================
const std::vector<i32>& halfband_taps_q17() {
    static const std::vector<i32> taps = [] {
        constexpr int    N    = 23;
        constexpr int    C    = 11;      // centre tap index
        constexpr double beta = 7.857;   // Kaiser, the classic 80 dB value

        std::vector<double> h(N);
        const double i0b = bessel_i0(beta);
        for (int k = 0; k < N; ++k) {
            const int m = k - C;
            double ideal;
            if (m == 0)            ideal = 0.5;
            else if ((m & 1) == 0) ideal = 0.0;   // the halfband zeros, exactly
            else {
                const double xx = 0.5 * double(m);
                ideal = 0.5 * std::sin(kPi * xx) / (kPi * xx);
            }
            const double r   = double(m) / double(C);
            const double arg = beta * std::sqrt(std::max(0.0, 1.0 - r * r));
            h[std::size_t(k)] = ideal * (bessel_i0(arg) / i0b);
        }

        double sum = 0.0;
        for (double v : h) sum += v;
        for (double& v : h) v /= sum;

        std::vector<i32> q(N);
        i64 acc = 0;
        for (int k = 0; k < N; ++k) {
            q[std::size_t(k)] = i32(std::llround(h[std::size_t(k)] * 131072.0));
            acc += q[std::size_t(k)];
        }
        q[C] += i32(131072 - acc);        // direct-current gain exactly 1.0
        return q;
    }();
    return taps;
}

std::vector<int> fft_scale_stages(const std::vector<i16>& win_q15, int n_fft) {
    const int stages = log2i(std::size_t(n_fft));
    std::vector<double> a(static_cast<std::size_t>(n_fft), 0.0);
    for (int i = 0; i < n_fft && i < int(win_q15.size()); ++i) {
        a[std::size_t(i)] = std::fabs(double(win_q15[std::size_t(i)])) / 32768.0;
    }

    std::vector<int> sh(static_cast<std::size_t>(stages), 0);
    int cum = 0;
    for (int s = 0; s < stages; ++s) {
        const int group  = 1 << (s + 1);      // inputs merged so far
        const int stride = n_fft >> (s + 1);  // and how far apart they sit
        double bound = 0.0;
        for (int n0 = 0; n0 < stride; ++n0) {
            double t = 0.0;
            for (int i = 0; i < group; ++i) t += a[std::size_t(n0 + i * stride)];
            bound = std::max(bound, t);
        }
        int need = 0;
        while (need < 30 && double(1 << need) < bound - 1e-12) ++need;
        int add = need - cum;
        if (add < 0) add = 0;
        if (add > 3) add = 3;               // two bits per stage in the register
        sh[std::size_t(s)] = add;
        cum += add;
    }
    return sh;
}

u32 fft_scale_word(const std::vector<int>& stages) {
    u32 w = 0;
    for (std::size_t s = 0; s < stages.size(); ++s) {
        w |= u32(stages[s] & 0x3) << (2 * s);
    }
    return w;
}

int chirps_per_channel(const Config& c) {
    return c.n_chirp;   // TDM halves the chirps per transmitter and doubles the
                        // total, so this is n_chirp in every mode
}

std::vector<i16> range_window_table(const Config& c) {
    const int len = c.decim > 0 ? c.n_sweep / c.decim : c.n_sweep;
    return quantise_window(make_window(c.range_window, len));
}

std::vector<i16> dopp_window_table(const Config& c) {
    const int len = std::min(chirps_per_channel(c), c.n_doppler);
    return quantise_window(make_window(c.dopp_window, len));
}

//============================================================================
// The fixed-point transform
//============================================================================
void FixedFftPlan::build(int n_, bool inv, const std::vector<int>& sh) {
    n       = n_;
    inverse = inv;
    shift   = sh;

    const std::size_t half = std::size_t(n / 2);
    tw.resize(half);
    twa.resize(half);
    twb.resize(half);

    // 32767 rather than 32768 because +1.0 has no signed 16-bit form; the gain
    // error is three parts in a hundred thousand per multiply and the fabric's
    // ROM holds these same numbers.
    const double sgn = inv ? 1.0 : -1.0;
    for (int k = 0; k < n / 2; ++k) {
        const double a  = sgn * 2.0 * kPi * double(k) / double(n);
        const i16    wr = i16(std::llround(32767.0 * std::cos(a)));
        const i16    wi = i16(std::llround(32767.0 * std::sin(a)));
        tw[std::size_t(k)]  = ci16(wr, wi);
        // Packed so that one 16x16 multiply-add instruction against a sample
        // laid out as (re, im) produces the whole real or imaginary product.
        twa[std::size_t(k)] = i32((u32(u16(i16(-wi))) << 16) | u32(u16(wr)));
        twb[std::size_t(k)] = i32((u32(u16(wr)) << 16) | u32(u16(wi)));
    }
    brev = make_bitrev(n);
}

int FixedFftPlan::total_shift() const {
    int t = 0;
    for (int s : shift) t += s;
    return t;
}

void fixed_fft(ci16* x, const FixedFftPlan& p) {
    const int n = p.n;
    int stage = 0, step = 1;
    for (int len = n; len >= 2; len >>= 1, ++stage, step <<= 1) {
        const int half = len >> 1;
        const int sc   = p.shift[std::size_t(stage)];
        for (int i = 0; i < n; i += len) {
            butterfly(x[i], x[i + half], ci16(), sc, true);
            for (int j = 1; j < half; ++j) {
                butterfly(x[i + j], x[i + j + half], p.tw[std::size_t(j * step)], sc, false);
            }
        }
    }
    for (int i = 0; i < n; ++i) {
        const int r = p.brev[std::size_t(i)];
        if (i < r) std::swap(x[i], x[r]);
    }
}

void fixed_fft_x8(ci16* x, const FixedFftPlan& p) {
    const int n = p.n;
#if defined(RADAR_REF_AVX2)
    const __m256i ones = _mm256_set1_epi16(1);
    const __m256i pm1  = _mm256_set1_epi32(i32(0xFFFF0001u));   // (1, -1)
#endif
    int stage = 0, step = 1;
    for (int len = n; len >= 2; len >>= 1, ++stage, step <<= 1) {
        const int half = len >> 1;
        const int sc   = p.shift[std::size_t(stage)];
        for (int i = 0; i < n; i += len) {
            for (int j = 0; j < half; ++j) {
                ci16* A = x + std::size_t(i + j) * 8;
                ci16* B = x + std::size_t(i + j + half) * 8;
#if defined(RADAR_REF_AVX2)
                const __m256i a = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(A));
                const __m256i b = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(B));
                _mm256_storeu_si256(reinterpret_cast<__m256i*>(A),
                                    add_round_pack(a, b, sc, ones));
                if (j == 0) {
                    _mm256_storeu_si256(reinterpret_cast<__m256i*>(B),
                                        add_round_pack(a, b, sc, pm1));
                } else {
                    const std::size_t t = std::size_t(j * step);
                    _mm256_storeu_si256(reinterpret_cast<__m256i*>(B),
                                        twiddle_pack(a, b, p.twa[t], p.twb[t], sc));
                }
#else
                const ci16 w = (j == 0) ? ci16() : p.tw[std::size_t(j * step)];
                for (int u = 0; u < 8; ++u) butterfly(A[u], B[u], w, sc, j == 0);
#endif
            }
        }
    }
    for (int i = 0; i < n; ++i) {
        const int r = p.brev[std::size_t(i)];
        if (i < r) {
            ci16 tmp[8];
            std::memcpy(tmp, x + std::size_t(i) * 8, sizeof tmp);
            std::memcpy(x + std::size_t(i) * 8, x + std::size_t(r) * 8, sizeof tmp);
            std::memcpy(x + std::size_t(r) * 8, tmp, sizeof tmp);
        }
    }
}

//============================================================================
// RefModel
//============================================================================
RefModel::RefModel(const Config& c, int threads) : cfg_(c), wf_(c) {
    cfg_.derive();
    set_threads(threads);

    hb_ = halfband_taps_q17();
    // Folded form: the filter is symmetric and every even offset from the
    // centre is zero, so twelve of the taps collapse into six sums of pairs
    // and the thirteenth is the centre.  Seven multiplies instead of thirteen,
    // and the accumulator stays exact because the pair is summed first.
    hb_fold_.clear();
    for (int k = 0; k <= 10; k += 2) hb_fold_.push_back(hb_[std::size_t(k)]);
    hb_fold_.push_back(hb_[11]);

    rwin_ = range_window_table(cfg_);
    dwin_ = dopp_window_table(cfg_);
    n_dopp_in_ = int(dwin_.size());

    rplan_.build(cfg_.n_range_fft, false, fft_scale_stages(rwin_, cfg_.n_range_fft));
    dplan_.build(cfg_.n_doppler,   true,  fft_scale_stages(dwin_, cfg_.n_doppler));
}

void RefModel::set_threads(int n) {
    threads_ = n < 1 ? 1 : (n > 64 ? 64 : n);
}

void RefModel::ensure_float_plans() const {
    std::call_once(fft_once_, [this] {
        fft_r_.reset(new Fft(cfg_.n_range_fft, false));
        fft_d_.reset(new Fft(cfg_.n_doppler,   true));
    });
}

namespace {

//----------------------------------------------------------------------------
// De-chirp: conjugate(received) times reference, rounded and saturated back to
// Q0.15.  The vector form computes the two halves of the product with one
// multiply-add instruction each and needs no widening, because a Q0.15 product
// pair fits a 32-bit lane with room to spare.
//----------------------------------------------------------------------------
void dechirp(const ci16* adc, const ci16* ref, int n, ci16* out) {
    int i = 0;
#if defined(RADAR_REF_AVX2)
    const __m256i half = _mm256_set1_epi32(1 << 14);
    const __m256i pm   = _mm256_set1_epi32(i32(0xFFFF0001u));
    for (; i + 8 <= n; i += 8) {
        const __m256i A = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(adc + i));
        const __m256i B = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(ref + i));
        // real: ar*br + ai*bi, straight from the interleaved layout
        const __m256i RR = _mm256_madd_epi16(A, B);
        // imaginary: ar*bi - ai*br, from the reference with its halves swapped
        // and the new upper half negated
        const __m256i Bsw = _mm256_or_si256(_mm256_slli_epi32(B, 16),
                                            _mm256_srli_epi32(B, 16));
        const __m256i II  = _mm256_madd_epi16(A, _mm256_sign_epi16(Bsw, pm));
        const __m256i r   = _mm256_srai_epi32(_mm256_add_epi32(RR, half), 15);
        const __m256i m   = _mm256_srai_epi32(_mm256_add_epi32(II, half), 15);
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(out + i),
                            _mm256_packs_epi32(_mm256_unpacklo_epi32(r, m),
                                               _mm256_unpackhi_epi32(r, m)));
    }
#endif
    for (; i < n; ++i) out[i] = fx::cmul_conj_q15(adc[i], ref[i], 15);
}

//----------------------------------------------------------------------------
// Halfband decimate by two, folded.  Output m is
//
//   sum over the six pairs of  h[2k] * (x[2m-2k] + x[2m-22+2k])
//   plus                       h[11] * x[2m-11]
//
// The delay line starts empty at every sweep, as it does in the fabric where
// the sequencer flushes it during the retrace, so the first eleven outputs see
// a partial filter.  The window's taper covers them.
//----------------------------------------------------------------------------
void halfband(const ci16* in, int nin, ci16* out, const i32* f) {
    const int guard = std::min(nin, 22);
    int t = 0;
    for (; t < guard; t += 2) {
        i64 ar = 0, ai = 0;
        for (int k = 0; k < 6; ++k) {
            const int i1 = t - 2 * k;
            const int i2 = t - 22 + 2 * k;
            i32 sr = 0, si = 0;
            if (i1 >= 0) { sr += in[i1].re; si += in[i1].im; }
            if (i2 >= 0) { sr += in[i2].re; si += in[i2].im; }
            ar += i64(f[k]) * sr;
            ai += i64(f[k]) * si;
        }
        const int ic = t - 11;
        if (ic >= 0) { ar += i64(f[6]) * in[ic].re; ai += i64(f[6]) * in[ic].im; }
        out[t / 2] = ci16(i16(fx::round_sat(ar, 17, 16)), i16(fx::round_sat(ai, 17, 16)));
    }
    const i64 f0 = f[0], f1 = f[1], f2 = f[2], f3 = f[3], f4 = f[4], f5 = f[5], fc = f[6];
    for (; t < nin; t += 2) {
        const ci16* p = in + t;
        const i32 s0r = p[  0].re + p[-22].re, s0i = p[  0].im + p[-22].im;
        const i32 s1r = p[ -2].re + p[-20].re, s1i = p[ -2].im + p[-20].im;
        const i32 s2r = p[ -4].re + p[-18].re, s2i = p[ -4].im + p[-18].im;
        const i32 s3r = p[ -6].re + p[-16].re, s3i = p[ -6].im + p[-16].im;
        const i32 s4r = p[ -8].re + p[-14].re, s4i = p[ -8].im + p[-14].im;
        const i32 s5r = p[-10].re + p[-12].re, s5i = p[-10].im + p[-12].im;
        const i64 ar = f0 * s0r + f1 * s1r + f2 * s2r + f3 * s3r + f4 * s4r + f5 * s5r
                     + fc * p[-11].re;
        const i64 ai = f0 * s0i + f1 * s1i + f2 * s2i + f3 * s3i + f4 * s4i + f5 * s5i
                     + fc * p[-11].im;
        out[t / 2] = ci16(i16(fx::round_sat(ar, 17, 16)), i16(fx::round_sat(ai, 17, 16)));
    }
}

} // namespace

//----------------------------------------------------------------------------
// One chirp, one receive channel
//----------------------------------------------------------------------------
void RefModel::range_chirp(const ci16* adc, int n, ci16* out_range_bins) const {
    const int ns   = cfg_.n_sweep;
    const int nd1  = ns / 2;
    const int nd2  = nd1 / 2;
    const int nfft = cfg_.n_range_fft;

    ChirpScratch& sc = chirp_scratch();
    if (int(sc.dech.size()) < ns)  sc.dech.assign(std::size_t(ns), ci16());
    if (int(sc.hb1.size())  < nd1) sc.hb1.assign(std::size_t(nd1), ci16());
    if (int(sc.hb2.size())  < nd2) sc.hb2.assign(std::size_t(nd2), ci16());

    const std::vector<ci16>& ref = wf_.chirp_q15();
    const int navail = std::min(n, ns);
    dechirp(adc, ref.data(), navail, sc.dech.data());
    for (int i = navail; i < ns; ++i) sc.dech[std::size_t(i)] = ci16();

    halfband(sc.dech.data(), ns,  sc.hb1.data(), hb_fold_.data());
    halfband(sc.hb1.data(),  nd1, sc.hb2.data(), hb_fold_.data());

    const int wlen = std::min(nd2, int(rwin_.size()));
    for (int i = 0; i < wlen; ++i) {
        const i32 w = rwin_[std::size_t(i)];
        out_range_bins[i] = ci16(i16(fx::round_sat(i64(sc.hb2[std::size_t(i)].re) * w, 15, 16)),
                                 i16(fx::round_sat(i64(sc.hb2[std::size_t(i)].im) * w, 15, 16)));
    }
    for (int i = wlen; i < nfft; ++i) out_range_bins[i] = ci16();

    fixed_fft(out_range_bins, rplan_);
}

//----------------------------------------------------------------------------
// Eight chirps at once.  Identical arithmetic; the only difference is that the
// windowed samples are written eight-way interleaved so the transform can walk
// them with full-width instructions.
//----------------------------------------------------------------------------
void RefModel::range_chirp_x8(const ci16* const* adc, int count, ci16* work,
                              ci16* out, int out_stride) const {
    const int ns   = cfg_.n_sweep;
    const int nd1  = ns / 2;
    const int nd2  = nd1 / 2;
    const int nfft = cfg_.n_range_fft;
    const int wlen = std::min(nd2, int(rwin_.size()));

    ChirpScratch& sc = chirp_scratch();
    if (int(sc.dech.size()) < ns)  sc.dech.assign(std::size_t(ns), ci16());
    if (int(sc.hb1.size())  < nd1) sc.hb1.assign(std::size_t(nd1), ci16());
    if (int(sc.hb2.size())  < nd2) sc.hb2.assign(std::size_t(nd2), ci16());

    const std::vector<ci16>& ref = wf_.chirp_q15();
    std::memset(static_cast<void*>(work), 0, sizeof(ci16) * std::size_t(nfft) * 8);

    for (int u = 0; u < count; ++u) {
        dechirp(adc[u], ref.data(), ns, sc.dech.data());
        halfband(sc.dech.data(), ns,  sc.hb1.data(), hb_fold_.data());
        halfband(sc.hb1.data(),  nd1, sc.hb2.data(), hb_fold_.data());
        for (int i = 0; i < wlen; ++i) {
            const i32 w = rwin_[std::size_t(i)];
            work[std::size_t(i) * 8 + u] =
                ci16(i16(fx::round_sat(i64(sc.hb2[std::size_t(i)].re) * w, 15, 16)),
                     i16(fx::round_sat(i64(sc.hb2[std::size_t(i)].im) * w, 15, 16)));
        }
    }

    fixed_fft_x8(work, rplan_);

    for (int u = 0; u < count; ++u) {
        ci16* dst = out + std::size_t(u) * out_stride;
        for (int r = 0; r < out_stride; ++r) dst[r] = work[std::size_t(r) * 8 + u];
    }
}

//----------------------------------------------------------------------------
// A whole coherent interval, fixed point
//----------------------------------------------------------------------------
void RefModel::process_cpi(const ci16* const* rx_chirps, int n_rx, int n_chirp_total,
                           RdFrame& out) const {
    const int ns   = cfg_.n_sweep;
    const int nr   = cfg_.n_range;
    const int nd   = cfg_.n_doppler;
    const int nfft = cfg_.n_range_fft;
    const int nrx  = std::min(n_rx, 2);

    out.allocate(nr, nd, 4, true);

    // Range transforms for every chirp of every receive channel.  Only the
    // bins the fabric keeps are held, which is what the corner-turn buffer
    // stores: n_range * n_chirp_total words per channel.
    std::vector<ci16> rb(static_cast<std::size_t>(nrx) * n_chirp_total * nr);
    const int ngroup = (n_chirp_total + 7) / 8;
    const int njob   = nrx * ngroup;

    parallel_for(njob, threads_, [&](int job) {
        const int rx = job / ngroup;
        const int g  = job % ngroup;
        const int k0 = g * 8;
        const int cnt = std::min(8, n_chirp_total - k0);

        ChirpScratch& sc = chirp_scratch();
        if (int(sc.work.size()) < nfft * 8) sc.work.assign(std::size_t(nfft) * 8, ci16());

        const ci16* ptr[8] = {};
        for (int u = 0; u < cnt; ++u) ptr[u] = rx_chirps[rx] + std::size_t(k0 + u) * ns;
        range_chirp_x8(ptr, cnt, sc.work.data(),
                       &rb[(std::size_t(rx) * n_chirp_total + k0) * nr], nr);
    });

    const int cpc = (cfg_.mimo == MimoMode::Tdm) ? n_chirp_total / 2 : n_chirp_total;
    const int nin = std::min(cpc, n_dopp_in_);

    // Corner turn and Doppler.  One range bin of one receive channel is a job;
    // there are hundreds of them and they share nothing.
    parallel_for(nrx * nr, threads_, [&](int job) {
        const int rx = job / nr;
        const int r  = job % nr;
        const ci16* base = &rb[std::size_t(rx) * n_chirp_total * nr];

        std::vector<ci16> series(static_cast<std::size_t>(nd));
        std::vector<ci16> alt(static_cast<std::size_t>(nd));

        auto doppler = [&](std::vector<ci16>& buf) {
            for (int i = 0; i < nin; ++i) {
                const i32 w = dwin_[std::size_t(i)];
                buf[std::size_t(i)] =
                    ci16(i16(fx::round_sat(i64(buf[std::size_t(i)].re) * w, 15, 16)),
                         i16(fx::round_sat(i64(buf[std::size_t(i)].im) * w, 15, 16)));
            }
            for (int i = nin; i < nd; ++i) buf[std::size_t(i)] = ci16();
            fixed_fft(buf.data(), dplan_);
            rotate_left(buf.data(), nd, nd / 2);      // zero velocity to the middle
        };
        auto store = [&](int v, const std::vector<ci16>& spec) {
            for (int d = 0; d < nd; ++d) {
                out.cube_at(v, r, d) = cf32(float(spec[std::size_t(d)].re),
                                            float(spec[std::size_t(d)].im));
            }
        };

        switch (cfg_.mimo) {
            case MimoMode::Tdm: {
                for (int tx = 0; tx < 2; ++tx) {
                    for (int i = 0; i < nin; ++i) {
                        const int k = 2 * i + tx;
                        series[std::size_t(i)] = (k < n_chirp_total)
                            ? base[std::size_t(k) * nr + r] : ci16();
                    }
                    doppler(series);
                    store(tx * 2 + rx, series);
                }
                break;
            }
            case MimoMode::Ddm: {
                for (int i = 0; i < nin; ++i) series[std::size_t(i)] = base[std::size_t(i) * nr + r];
                doppler(series);
                store(0 * 2 + rx, series);
                // Transmitter 1 was inverted on odd chirps, so its echo sits
                // half a Doppler band away; rotating the spectrum back by half
                // its length separates the two.
                alt = series;
                rotate_left(alt.data(), nd, nd / 2);
                store(1 * 2 + rx, alt);
                break;
            }
            case MimoMode::Tx0Only:
            case MimoMode::Tx1Only: {
                const int tx = (cfg_.mimo == MimoMode::Tx0Only) ? 0 : 1;
                for (int i = 0; i < nin; ++i) series[std::size_t(i)] = base[std::size_t(i) * nr + r];
                doppler(series);
                store(tx * 2 + rx, series);
                break;
            }
        }
    });

    // Non-coherent integration across the virtual channels.  Exact unsigned
    // squares summed in 64 bits, clamped to the 32-bit word the frame carries.
    for (int r = 0; r < nr; ++r) {
        for (int d = 0; d < nd; ++d) {
            u64 acc = 0;
            for (int v = 0; v < 4; ++v) {
                const cf32 s = out.cube_at(v, r, d);
                const i64 re = i64(s.real()), im = i64(s.imag());
                acc += u64(re * re + im * im);
            }
            if (acc > 0xFFFFFFFFull) acc = 0xFFFFFFFFull;
            out.at(r, d) = float(acc);
        }
    }

    // A robust stand-in for the detector's own estimate, so a frame produced
    // by the model is usable before CFAR has run over it.
    {
        std::vector<float> tmp(out.power.begin(), out.power.end());
        if (!tmp.empty()) {
            const std::size_t mid = tmp.size() / 2;
            std::nth_element(tmp.begin(), tmp.begin() + std::ptrdiff_t(mid), tmp.end());
            out.noise_floor = double(tmp[mid]);
        }
    }
}

//----------------------------------------------------------------------------
// The same chain in floating point
//
// Same chirp, same halfband coefficients, same window table, same overall
// scaling -- everything identical except that nothing is rounded to 16 bits on
// the way.  Subtracting one cube from the other therefore leaves exactly the
// error the fixed point introduced, and nothing else.
//----------------------------------------------------------------------------
void RefModel::process_cpi_float(const cf32* const* rx_chirps, int n_rx, int n_chirp_total,
                                 RdFrame& out) const {
    ensure_float_plans();

    const int ns   = cfg_.n_sweep;
    const int nd1  = ns / 2;
    const int nd2  = nd1 / 2;
    const int nr   = cfg_.n_range;
    const int nd   = cfg_.n_doppler;
    const int nfft = cfg_.n_range_fft;
    const int nrx  = std::min(n_rx, 2);

    out.allocate(nr, nd, 4, true);

    const double rscale = std::pow(0.5, double(range_fft_total_shift()));
    const double dscale = std::pow(0.5, double(dopp_fft_total_shift()));

    // Coefficients as the fabric holds them, read back into doubles.
    std::vector<double> hbf(hb_.size());
    for (std::size_t i = 0; i < hb_.size(); ++i) hbf[i] = double(hb_[i]) / 131072.0;
    std::vector<double> rwf(rwin_.size());
    for (std::size_t i = 0; i < rwin_.size(); ++i) rwf[i] = double(rwin_[i]) / 32768.0;
    std::vector<double> dwf(dwin_.size());
    for (std::size_t i = 0; i < dwin_.size(); ++i) dwf[i] = double(dwin_[i]) / 32768.0;

    const std::vector<cf32>& ref = wf_.chirp_float();

    std::vector<cf32> rb(static_cast<std::size_t>(nrx) * n_chirp_total * nr);
    {
        std::vector<cf32> dech(static_cast<std::size_t>(ns)), h1(static_cast<std::size_t>(nd1)),
                          h2(static_cast<std::size_t>(nd2));
        AlignedBuffer<cf32> buf(static_cast<std::size_t>(nfft));

        auto halfband_f = [&](const cf32* in, int nin_, cf32* outp) {
            for (int t = 0; t < nin_; t += 2) {
                double ar = 0.0, ai = 0.0;
                for (int k = 0; k < int(hbf.size()); ++k) {
                    const int s = t - k;
                    if (s < 0) break;
                    ar += hbf[std::size_t(k)] * double(in[s].real());
                    ai += hbf[std::size_t(k)] * double(in[s].imag());
                }
                outp[t / 2] = cf32(float(ar), float(ai));
            }
        };

        for (int rx = 0; rx < nrx; ++rx) {
            for (int k = 0; k < n_chirp_total; ++k) {
                const cf32* adc = rx_chirps[rx] + std::size_t(k) * ns;
                // Work in the fabric's integer scale so the two cubes line up.
                for (int i = 0; i < ns; ++i) {
                    const cf32 a = adc[i] * 32768.0f;
                    const cf32 b = ref[std::size_t(i)] * 32768.0f;
                    const double rr = double(a.real()) * b.real() + double(a.imag()) * b.imag();
                    const double ii = double(a.real()) * b.imag() - double(a.imag()) * b.real();
                    dech[std::size_t(i)] = cf32(float(rr / 32768.0), float(ii / 32768.0));
                }
                halfband_f(dech.data(), ns,  h1.data());
                halfband_f(h1.data(),   nd1, h2.data());

                const int wlen = std::min(nd2, int(rwf.size()));
                for (int i = 0; i < wlen; ++i)
                    buf[std::size_t(i)] = h2[std::size_t(i)] * float(rwf[std::size_t(i)]);
                for (int i = wlen; i < nfft; ++i) buf[std::size_t(i)] = cf32(0.0f, 0.0f);
                fft_r_->run(buf.data(), 1);
                for (int r = 0; r < nr; ++r) {
                    rb[(std::size_t(rx) * n_chirp_total + k) * nr + r] =
                        buf[std::size_t(r)] * float(rscale);
                }
            }
        }
    }

    const int cpc = (cfg_.mimo == MimoMode::Tdm) ? n_chirp_total / 2 : n_chirp_total;
    const int nin = std::min(cpc, n_dopp_in_);

    AlignedBuffer<cf32> series(static_cast<std::size_t>(nd));
    std::vector<cf32>   alt(static_cast<std::size_t>(nd));

    auto doppler = [&]() {
        for (int i = 0; i < nin; ++i)
            series[std::size_t(i)] = series[std::size_t(i)] * float(dwf[std::size_t(i)]);
        for (int i = nin; i < nd; ++i) series[std::size_t(i)] = cf32(0.0f, 0.0f);
        fft_d_->run(series.data(), 1);
        for (int i = 0; i < nd; ++i) series[std::size_t(i)] = series[std::size_t(i)] * float(dscale);
        fftshift(series.data(), nd);
    };

    auto store = [&](int v, int r, const cf32* spec) {
        for (int d = 0; d < nd; ++d) out.cube_at(v, r, d) = spec[d];
    };

    for (int rx = 0; rx < nrx; ++rx) {
        const cf32* base = &rb[std::size_t(rx) * n_chirp_total * nr];
        for (int r = 0; r < nr; ++r) {
            switch (cfg_.mimo) {
                case MimoMode::Tdm: {
                    for (int tx = 0; tx < 2; ++tx) {
                        for (int i = 0; i < nin; ++i) {
                            const int k = 2 * i + tx;
                            series[std::size_t(i)] = (k < n_chirp_total)
                                ? base[std::size_t(k) * nr + r] : cf32(0.0f, 0.0f);
                        }
                        doppler();
                        store(tx * 2 + rx, r, series.data());
                    }
                    break;
                }
                case MimoMode::Ddm: {
                    for (int i = 0; i < nin; ++i) series[std::size_t(i)] = base[std::size_t(i) * nr + r];
                    doppler();
                    store(0 * 2 + rx, r, series.data());
                    std::copy(series.begin(), series.end(), alt.begin());
                    rotate_left(alt.data(), nd, nd / 2);
                    store(1 * 2 + rx, r, alt.data());
                    break;
                }
                case MimoMode::Tx0Only:
                case MimoMode::Tx1Only: {
                    const int tx = (cfg_.mimo == MimoMode::Tx0Only) ? 0 : 1;
                    for (int i = 0; i < nin; ++i) series[std::size_t(i)] = base[std::size_t(i) * nr + r];
                    doppler();
                    store(tx * 2 + rx, r, series.data());
                    break;
                }
            }
        }
    }

    for (int r = 0; r < nr; ++r) {
        for (int d = 0; d < nd; ++d) {
            double acc = 0.0;
            for (int v = 0; v < 4; ++v) {
                const cf32 s = out.cube_at(v, r, d);
                acc += double(s.real()) * s.real() + double(s.imag()) * s.imag();
            }
            out.at(r, d) = float(acc);
        }
    }

    std::vector<float> tmp(out.power.begin(), out.power.end());
    if (!tmp.empty()) {
        const std::size_t mid = tmp.size() / 2;
        std::nth_element(tmp.begin(), tmp.begin() + std::ptrdiff_t(mid), tmp.end());
        out.noise_floor = double(tmp[mid]);
    }
}

} // namespace radar
