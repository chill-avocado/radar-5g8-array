//============================================================================
// fft.cpp -- the three interchangeable transform backends
//
// Which one is compiled in is decided by the build:
//   RADAR_FFT_ACCELERATE   Apple's vDSP, a system framework
//   RADAR_FFT_FFTW         FFTW3 single precision
//   (neither)              the self-contained Stockham radix-4/2 below
//
// All three answer to the same class and are checked against dft_reference()
// by radar-selftest, so swapping backends can change the speed and never the
// numbers beyond single-precision rounding.
//
// Every backend must be safe to call from several worker threads at once on
// different buffers, because the pipeline runs one range transform per chirp
// across a thread pool.  Anything a backend needs as scratch is therefore
// either on the stack or thread-local; the planned state is read-only once
// built.
//============================================================================
#include "radar/fft.hpp"

#include <algorithm>
#include <cstring>
#include <vector>

#if defined(RADAR_FFT_ACCELERATE)
#  include <Accelerate/Accelerate.h>
#elif defined(RADAR_FFT_FFTW)
#  include <fftw3.h>
#  include <mutex>
#endif

#if defined(__AVX2__) && defined(__FMA__)
#  include <immintrin.h>
#  define RADAR_FFT_HAVE_AVX2 1
#endif

namespace radar {

//============================================================================
// Reference transform and spectrum rotation -- backend independent
//============================================================================
void dft_reference(const cf32* in, cf32* out, int n, bool inverse) {
    const double sign = inverse ? 1.0 : -1.0;
    for (int k = 0; k < n; ++k) {
        double sr = 0.0, si = 0.0;
        for (int t = 0; t < n; ++t) {
            const double ang = sign * 2.0 * kPi * double(k) * double(t) / double(n);
            const double c = std::cos(ang), s = std::sin(ang);
            const double xr = double(in[t].real()), xi = double(in[t].imag());
            sr += xr * c - xi * s;
            si += xr * s + xi * c;
        }
        out[k] = cf32(float(sr), float(si));
    }
}

void fftshift(cf32* data, int n) {
    if (n <= 1 || (n & 1)) return;
    std::rotate(data, data + n / 2, data + n);
}

void fftshift(float* data, int n) {
    if (n <= 1 || (n & 1)) return;
    std::rotate(data, data + n / 2, data + n);
}

//============================================================================
// Backend: Apple Accelerate (vDSP)
//============================================================================
#if defined(RADAR_FFT_ACCELERATE)

struct Fft::Impl {
    vDSP_DFT_Setup setup = nullptr;
    ~Impl() { if (setup) vDSP_DFT_DestroySetup(setup); }
};

const char* Fft::backend() { return "accelerate"; }

void Fft::plan(int n, bool inverse) {
    n_ = n;
    inverse_ = inverse;
    impl_.reset(new Impl);
    impl_->setup = vDSP_DFT_zop_CreateSetup(
        nullptr, vDSP_Length(n),
        inverse ? vDSP_DFT_INVERSE : vDSP_DFT_FORWARD);
}

namespace {
/// vDSP wants the real and imaginary halves in separate arrays.  One scratch
/// block per thread keeps the staging off the allocator and off other threads.
struct SplitScratch {
    std::vector<float> re, im, ore, oim;
    void ensure(int n) {
        if (int(re.size()) < n) { re.resize(n); im.resize(n); ore.resize(n); oim.resize(n); }
    }
};
SplitScratch& split_scratch() {
    static thread_local SplitScratch s;
    return s;
}
} // namespace

void Fft::run(const cf32* in, cf32* out, int batch) const {
    const int n = n_;
    SplitScratch& sc = split_scratch();
    sc.ensure(n);
    for (int b = 0; b < batch; ++b) {
        const cf32* src = in + std::size_t(b) * n;
        cf32*       dst = out + std::size_t(b) * n;
        for (int k = 0; k < n; ++k) { sc.re[k] = src[k].real(); sc.im[k] = src[k].imag(); }
        vDSP_DFT_Execute(impl_->setup, sc.re.data(), sc.im.data(), sc.ore.data(), sc.oim.data());
        for (int k = 0; k < n; ++k) dst[k] = cf32(sc.ore[k], sc.oim[k]);
    }
}

void Fft::run(cf32* data, int batch) const { run(data, data, batch); }

//============================================================================
// Backend: FFTW3
//============================================================================
#elif defined(RADAR_FFT_FFTW)

namespace {
/// FFTW's planner touches global state, so only one thread may be inside it.
/// Execution through fftwf_execute_dft() is thread safe and is not guarded.
std::mutex& planner_mutex() { static std::mutex m; return m; }
} // namespace

struct Fft::Impl {
    // Four plans, because FFTW insists that a plan be replayed with the same
    // in-place-ness and the same alignment it was built with.  The two MEASURE
    // plans are the ones that actually run; the UNALIGNED pair is the safety
    // net for a caller whose buffer did not come out of AlignedBuffer.
    fftwf_plan out_of_place  = nullptr;
    fftwf_plan in_place      = nullptr;
    fftwf_plan out_unaligned = nullptr;
    fftwf_plan in_unaligned  = nullptr;
    int        align         = 0;
    ~Impl() {
        std::lock_guard<std::mutex> lk(planner_mutex());
        if (out_of_place)  fftwf_destroy_plan(out_of_place);
        if (in_place)      fftwf_destroy_plan(in_place);
        if (out_unaligned) fftwf_destroy_plan(out_unaligned);
        if (in_unaligned)  fftwf_destroy_plan(in_unaligned);
    }
};

const char* Fft::backend() { return "fftw"; }

void Fft::plan(int n, bool inverse) {
    n_ = n;
    inverse_ = inverse;
    impl_.reset(new Impl);

    const int sign = inverse ? FFTW_BACKWARD : FFTW_FORWARD;
    int nn = n;

    std::lock_guard<std::mutex> lk(planner_mutex());
    fftwf_complex* a = static_cast<fftwf_complex*>(fftwf_malloc(sizeof(fftwf_complex) * std::size_t(n)));
    fftwf_complex* b = static_cast<fftwf_complex*>(fftwf_malloc(sizeof(fftwf_complex) * std::size_t(n)));
    std::memset(a, 0, sizeof(fftwf_complex) * std::size_t(n));
    std::memset(b, 0, sizeof(fftwf_complex) * std::size_t(n));

    impl_->align = fftwf_alignment_of(reinterpret_cast<float*>(a));

    // howmany == 1: the batch loop lives in run(), so one plan serves every
    // batch size the pipeline ever asks for.
    impl_->out_of_place = fftwf_plan_many_dft(
        1, &nn, 1, a, nullptr, 1, nn, b, nullptr, 1, nn, sign, FFTW_MEASURE);
    impl_->in_place = fftwf_plan_many_dft(
        1, &nn, 1, a, nullptr, 1, nn, a, nullptr, 1, nn, sign, FFTW_MEASURE);
    impl_->out_unaligned = fftwf_plan_many_dft(
        1, &nn, 1, a, nullptr, 1, nn, b, nullptr, 1, nn, sign, FFTW_ESTIMATE | FFTW_UNALIGNED);
    impl_->in_unaligned = fftwf_plan_many_dft(
        1, &nn, 1, a, nullptr, 1, nn, a, nullptr, 1, nn, sign, FFTW_ESTIMATE | FFTW_UNALIGNED);

    fftwf_free(a);
    fftwf_free(b);
}

void Fft::run(const cf32* in, cf32* out, int batch) const {
    const int n = n_;
    const bool inplace = (in == out);
    const bool ok_align =
        fftwf_alignment_of(reinterpret_cast<float*>(const_cast<cf32*>(in)))  == impl_->align &&
        fftwf_alignment_of(reinterpret_cast<float*>(out)) == impl_->align;

    fftwf_plan p = inplace ? (ok_align ? impl_->in_place  : impl_->in_unaligned)
                           : (ok_align ? impl_->out_of_place : impl_->out_unaligned);

    for (int b = 0; b < batch; ++b) {
        fftwf_complex* src = reinterpret_cast<fftwf_complex*>(const_cast<cf32*>(in) + std::size_t(b) * n);
        fftwf_complex* dst = reinterpret_cast<fftwf_complex*>(out + std::size_t(b) * n);
        fftwf_execute_dft(p, src, dst);
    }
}

void Fft::run(cf32* data, int batch) const { run(data, data, batch); }

//============================================================================
// Backend: built in -- Stockham autosort, radix 4 with a radix-2 tail
//============================================================================
#else

const char* Fft::backend() { return "builtin"; }

namespace {

//----------------------------------------------------------------------------
// Stockham is the right shape for this job.  It reads one buffer and writes
// another every pass, which means no bit-reversal permutation at the end and,
// more usefully, the innermost loop walks both buffers with unit stride.  That
// inner loop is what the AVX2 path below vectorises: four complex numbers per
// instruction, one twiddle broadcast across all of them.
//
// Radix 4 halves the number of passes compared with radix 2 and removes three
// quarters of the twiddle multiplies that are exactly 1.  When log2(n) is odd
// one radix-2 pass finishes the job, and it is always the last pass, by which
// point the block stride is large so it vectorises too.
//----------------------------------------------------------------------------

#if defined(RADAR_FFT_HAVE_AVX2)

inline __m256 cmul_bcast(__m256 x, __m256 wcd, __m256 wdc) {
    // x = [r0 i0 r1 i1 ...], wcd = [wr wi ...], wdc = [wi wr ...]
    // even lanes: r*wr - i*wi     odd lanes: r*wi + i*wr
    const __m256 xr = _mm256_moveldup_ps(x);
    const __m256 xi = _mm256_movehdup_ps(x);
    return _mm256_fmaddsub_ps(xr, wcd, _mm256_mul_ps(xi, wdc));
}

inline __m256 mul_pos_i(__m256 x) {                 // (r, i) -> (-i, r)
    const __m256 sw = _mm256_permute_ps(x, 0xB1);
    return _mm256_xor_ps(sw, _mm256_setr_ps(-0.0f, 0.0f, -0.0f, 0.0f, -0.0f, 0.0f, -0.0f, 0.0f));
}

inline __m256 mul_neg_i(__m256 x) {                 // (r, i) -> (i, -r)
    const __m256 sw = _mm256_permute_ps(x, 0xB1);
    return _mm256_xor_ps(sw, _mm256_setr_ps(0.0f, -0.0f, 0.0f, -0.0f, 0.0f, -0.0f, 0.0f, -0.0f));
}

#endif // RADAR_FFT_HAVE_AVX2

/// One radix-4 Stockham pass.  `nn` is the length of the sub-transform being
/// split, `s` the number of interleaved transforms already formed.
void pass4(const float* src, float* dst, int n, int nn, int s,
           const float* tw, bool inverse) {
    const int m    = nn / 4;
    const int step = n / nn;

    for (int p = 0; p < m; ++p) {
        const int t1 = p * step, t2 = 2 * p * step, t3 = 3 * p * step;
        const float w1r = tw[2 * t1], w1i = tw[2 * t1 + 1];
        const float w2r = tw[2 * t2], w2i = tw[2 * t2 + 1];
        const float w3r = tw[2 * t3], w3i = tw[2 * t3 + 1];

        const float* sa = src + 2 * std::size_t(s) * (p + 0 * m);
        const float* sb = src + 2 * std::size_t(s) * (p + 1 * m);
        const float* sc = src + 2 * std::size_t(s) * (p + 2 * m);
        const float* sd = src + 2 * std::size_t(s) * (p + 3 * m);
        float* d0 = dst + 2 * std::size_t(s) * (4 * p + 0);
        float* d1 = dst + 2 * std::size_t(s) * (4 * p + 1);
        float* d2 = dst + 2 * std::size_t(s) * (4 * p + 2);
        float* d3 = dst + 2 * std::size_t(s) * (4 * p + 3);

        int q = 0;
#if defined(RADAR_FFT_HAVE_AVX2)
        if (s >= 4) {
            const __m256 W1cd = _mm256_setr_ps(w1r, w1i, w1r, w1i, w1r, w1i, w1r, w1i);
            const __m256 W1dc = _mm256_setr_ps(w1i, w1r, w1i, w1r, w1i, w1r, w1i, w1r);
            const __m256 W2cd = _mm256_setr_ps(w2r, w2i, w2r, w2i, w2r, w2i, w2r, w2i);
            const __m256 W2dc = _mm256_setr_ps(w2i, w2r, w2i, w2r, w2i, w2r, w2i, w2r);
            const __m256 W3cd = _mm256_setr_ps(w3r, w3i, w3r, w3i, w3r, w3i, w3r, w3i);
            const __m256 W3dc = _mm256_setr_ps(w3i, w3r, w3i, w3r, w3i, w3r, w3i, w3r);
            for (; q + 4 <= s; q += 4) {
                const __m256 a = _mm256_loadu_ps(sa + 2 * q);
                const __m256 b = _mm256_loadu_ps(sb + 2 * q);
                const __m256 c = _mm256_loadu_ps(sc + 2 * q);
                const __m256 d = _mm256_loadu_ps(sd + 2 * q);
                const __m256 apc = _mm256_add_ps(a, c);
                const __m256 amc = _mm256_sub_ps(a, c);
                const __m256 bpd = _mm256_add_ps(b, d);
                const __m256 bmd = _mm256_sub_ps(b, d);
                const __m256 jb  = inverse ? mul_neg_i(bmd) : mul_pos_i(bmd);
                _mm256_storeu_ps(d0 + 2 * q, _mm256_add_ps(apc, bpd));
                _mm256_storeu_ps(d1 + 2 * q, cmul_bcast(_mm256_sub_ps(amc, jb), W1cd, W1dc));
                _mm256_storeu_ps(d2 + 2 * q, cmul_bcast(_mm256_sub_ps(apc, bpd), W2cd, W2dc));
                _mm256_storeu_ps(d3 + 2 * q, cmul_bcast(_mm256_add_ps(amc, jb), W3cd, W3dc));
            }
        }
#endif
        for (; q < s; ++q) {
            const float ar = sa[2 * q], ai = sa[2 * q + 1];
            const float br = sb[2 * q], bi = sb[2 * q + 1];
            const float cr = sc[2 * q], ci = sc[2 * q + 1];
            const float dr = sd[2 * q], di = sd[2 * q + 1];

            const float apcr = ar + cr, apci = ai + ci;
            const float amcr = ar - cr, amci = ai - ci;
            const float bpdr = br + dr, bpdi = bi + di;
            const float bmdr = br - dr, bmdi = bi - di;
            // +i * (r, i) = (-i, r);  -i * (r, i) = (i, -r)
            const float jr = inverse ?  bmdi : -bmdi;
            const float ji = inverse ? -bmdr :  bmdr;

            const float x1r = amcr - jr, x1i = amci - ji;
            const float x2r = apcr - bpdr, x2i = apci - bpdi;
            const float x3r = amcr + jr, x3i = amci + ji;

            d0[2 * q]     = apcr + bpdr;
            d0[2 * q + 1] = apci + bpdi;
            d1[2 * q]     = x1r * w1r - x1i * w1i;
            d1[2 * q + 1] = x1r * w1i + x1i * w1r;
            d2[2 * q]     = x2r * w2r - x2i * w2i;
            d2[2 * q + 1] = x2r * w2i + x2i * w2r;
            d3[2 * q]     = x3r * w3r - x3i * w3i;
            d3[2 * q + 1] = x3r * w3i + x3i * w3r;
        }
    }
}

/// One radix-2 Stockham pass, used only to finish an odd log2(n).
void pass2(const float* src, float* dst, int n, int nn, int s, const float* tw) {
    const int m    = nn / 2;
    const int step = n / nn;

    for (int p = 0; p < m; ++p) {
        const int t = p * step;
        const float wr = tw[2 * t], wi = tw[2 * t + 1];

        const float* sa = src + 2 * std::size_t(s) * (p + 0);
        const float* sb = src + 2 * std::size_t(s) * (p + m);
        float* d0 = dst + 2 * std::size_t(s) * (2 * p + 0);
        float* d1 = dst + 2 * std::size_t(s) * (2 * p + 1);

        int q = 0;
#if defined(RADAR_FFT_HAVE_AVX2)
        if (s >= 4) {
            const __m256 Wcd = _mm256_setr_ps(wr, wi, wr, wi, wr, wi, wr, wi);
            const __m256 Wdc = _mm256_setr_ps(wi, wr, wi, wr, wi, wr, wi, wr);
            for (; q + 4 <= s; q += 4) {
                const __m256 a = _mm256_loadu_ps(sa + 2 * q);
                const __m256 b = _mm256_loadu_ps(sb + 2 * q);
                _mm256_storeu_ps(d0 + 2 * q, _mm256_add_ps(a, b));
                _mm256_storeu_ps(d1 + 2 * q, cmul_bcast(_mm256_sub_ps(a, b), Wcd, Wdc));
            }
        }
#endif
        for (; q < s; ++q) {
            const float ar = sa[2 * q], ai = sa[2 * q + 1];
            const float br = sb[2 * q], bi = sb[2 * q + 1];
            const float xr = ar - br, xi = ai - bi;
            d0[2 * q]     = ar + br;
            d0[2 * q + 1] = ai + bi;
            d1[2 * q]     = xr * wr - xi * wi;
            d1[2 * q + 1] = xr * wi + xi * wr;
        }
    }
}

std::vector<float>& work_scratch(std::size_t n) {
    static thread_local std::vector<float> s;
    if (s.size() < 2 * n) s.resize(2 * n);
    return s;
}

} // namespace

struct Fft::Impl {
    int                n       = 0;
    int                npasses = 0;
    bool               inverse = false;
    std::vector<float> tw;      ///< interleaved exp(sign * 2*pi*i*t/n), t < n
};

void Fft::plan(int n, bool inverse) {
    n_ = n;
    inverse_ = inverse;
    impl_.reset(new Impl);
    impl_->n = n;
    impl_->inverse = inverse;

    impl_->tw.resize(2 * std::size_t(n));
    const double sign = inverse ? 1.0 : -1.0;
    for (int t = 0; t < n; ++t) {
        const double ang = sign * 2.0 * kPi * double(t) / double(n);
        impl_->tw[2 * std::size_t(t)]     = float(std::cos(ang));
        impl_->tw[2 * std::size_t(t) + 1] = float(std::sin(ang));
    }

    int passes = 0;
    for (int nn = n; nn > 1;) {
        if (nn % 4 == 0) nn /= 4; else nn /= 2;
        ++passes;
    }
    impl_->npasses = passes;
}

void Fft::run(const cf32* in, cf32* out, int batch) const {
    const int n       = n_;
    const int npasses = impl_->npasses;
    const float* tw   = impl_->tw.data();
    const bool inverse = inverse_;

    std::vector<float>& scratch = work_scratch(std::size_t(n));

    for (int b = 0; b < batch; ++b) {
        const float* src_in = reinterpret_cast<const float*>(in + std::size_t(b) * n);
        float*       dstp   = reinterpret_cast<float*>(out + std::size_t(b) * n);

        // Choose which buffer the first pass writes into so that the last pass
        // lands in `out` and no copy is needed at the end.
        float* A = (npasses % 2 == 0) ? dstp : scratch.data();
        float* B = (npasses % 2 == 0) ? scratch.data() : dstp;
        if (src_in != A) std::memcpy(A, src_in, sizeof(float) * 2 * std::size_t(n));

        float* s = A;
        float* d = B;
        int nn = n, st = 1;
        while (nn > 1) {
            if (nn % 4 == 0) { pass4(s, d, n, nn, st, tw, inverse); nn /= 4; st *= 4; }
            else             { pass2(s, d, n, nn, st, tw);          nn /= 2; st *= 2; }
            std::swap(s, d);
        }
        if (s != dstp) std::memcpy(dstp, s, sizeof(float) * 2 * std::size_t(n));
    }
}

void Fft::run(cf32* data, int batch) const { run(data, data, batch); }

#endif // backend selection

//============================================================================
// Shared boilerplate
//============================================================================
Fft::Fft(int n, bool inverse) { plan(n, inverse); }
Fft::~Fft() = default;
Fft::Fft(Fft&&) noexcept = default;
Fft& Fft::operator=(Fft&&) noexcept = default;

} // namespace radar
