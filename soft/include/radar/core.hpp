//============================================================================
// core.hpp -- fundamental types, fixed-point arithmetic and constants
//
// The fixed-point helpers here are the C++ half of a contract with the FPGA
// RTL.  fpga/rtl/radar_pkg.svh is the other half.  round_sat() below is the
// exact behaviour every saturating truncation in the RTL performs; if the two
// ever disagree the bit-exact reference model stops being a reference.
//============================================================================
#pragma once

#include <cstddef>
#include <cstdint>
#include <cmath>
#include <complex>
#include <memory>
#include <new>
#include <string>
#include <vector>

namespace radar {

//----------------------------------------------------------------------------
// Scalar aliases.  Explicit widths everywhere -- this code talks to hardware.
//----------------------------------------------------------------------------
using i8  = std::int8_t;   using u8  = std::uint8_t;
using i16 = std::int16_t;  using u16 = std::uint16_t;
using i32 = std::int32_t;  using u32 = std::uint32_t;
using i64 = std::int64_t;  using u64 = std::uint64_t;
using f32 = float;         using f64 = double;

using cf32 = std::complex<float>;
using cf64 = std::complex<double>;

/// A complex sample in the hardware's native s16 Q0.15 format.
struct ci16 {
    i16 re = 0;
    i16 im = 0;
    constexpr ci16() = default;
    constexpr ci16(i16 r, i16 i) : re(r), im(i) {}
    /// Convert to float in [-1, 1).  32768 is the divisor, not 32767, so that
    /// the mapping is exactly a power of two and round-trips without bias.
    cf32 to_float() const { return {re * (1.0f / 32768.0f), im * (1.0f / 32768.0f)}; }
};

//----------------------------------------------------------------------------
// Physical constants and array geometry.
//
// The geometry numbers come from kicad/radar_5g8_*/array_report.json, which is
// generated from the same polygon list that produced the Gerbers.  They are
// restated here rather than parsed at runtime so the DSP has no file
// dependency, and checked against the JSON by radar-selftest.
//----------------------------------------------------------------------------
namespace phys {
constexpr double c0          = 299792458.0;      ///< m/s
constexpr double k_boltz     = 1.380649e-23;     ///< J/K
constexpr double T0          = 290.0;            ///< K
}

namespace array_geom {
constexpr double f0_hz        = 5.80e9;
constexpr double lambda_m     = phys::c0 / f0_hz;         ///< 51.6884 mm
constexpr double pitch_m      = lambda_m / 2.0;           ///< 25.8442 mm
constexpr int    n_tx         = 2;
constexpr int    n_rx         = 2;
constexpr int    n_virt       = n_tx * n_rx;

/// Virtual element positions in metres, (x = azimuth, y = elevation).
/// Virtual position = transmit position + receive position; the constant
/// offset between the two boards is common to all four and cancels out of any
/// angle estimate, which is why the 250 mm board separation does not appear.
/// Index order is (tx * n_rx + rx), matching the hardware's channel packing.
constexpr double virt_xy[n_virt][2] = {
    {-lambda_m / 4.0, -lambda_m / 4.0},   // tx0 rx0
    {-lambda_m / 4.0,  lambda_m / 4.0},   // tx0 rx1
    { lambda_m / 4.0, -lambda_m / 4.0},   // tx1 rx0
    { lambda_m / 4.0,  lambda_m / 4.0},   // tx1 rx1
};

/// Measured on the built boards, openEMS, both elements present, -40 dB
/// convergence.  Used by the link budget and by the simulator's RF model.
constexpr double element_gain_dbi   = 6.1;
constexpr double tx_rx_isolation_db = -41.1;
constexpr double axial_ratio_db[2]  = {2.83, 3.16};
}

//----------------------------------------------------------------------------
// Fixed-point arithmetic, bit-exact with the RTL.
//----------------------------------------------------------------------------
namespace fx {

/// Round-half-up then saturate to `width` signed bits.
///
/// This is what the RTL does: add half a least-significant bit, arithmetic
/// shift right, then clamp.  Ties go towards positive infinity in both
/// implementations, which matters because a DC-heavy signal such as transmit
/// leakage would otherwise accumulate a bias.
inline i64 round_sat(i64 v, int shift, int width) {
    if (shift > 0) {
        v = (v + (i64(1) << (shift - 1))) >> shift;
    }
    const i64 hi = (i64(1) << (width - 1)) - 1;
    const i64 lo = -(i64(1) << (width - 1));
    return v > hi ? hi : (v < lo ? lo : v);
}

/// Saturate without rounding, for paths where the RTL simply clamps.
inline i64 sat(i64 v, int width) {
    const i64 hi = (i64(1) << (width - 1)) - 1;
    const i64 lo = -(i64(1) << (width - 1));
    return v > hi ? hi : (v < lo ? lo : v);
}

/// Complex multiply of two Q0.15 samples, result rounded and saturated back to
/// Q0.15 after an additional `shift` bits of headroom removal.  The product of
/// two Q0.15 numbers is Q1.30, so shift == 15 restores Q0.15 with unit gain.
inline ci16 cmul_q15(ci16 a, ci16 b, int shift = 15) {
    const i64 rr = i64(a.re) * b.re - i64(a.im) * b.im;
    const i64 ii = i64(a.re) * b.im + i64(a.im) * b.re;
    return ci16(i16(round_sat(rr, shift, 16)), i16(round_sat(ii, shift, 16)));
}

/// Conjugate multiply: a* . b.  This is the de-chirp operation -- the received
/// sample conjugated against the reference chirp, so a target's beat frequency
/// comes out positive and range increases with FFT bin index.
inline ci16 cmul_conj_q15(ci16 a, ci16 b, int shift = 15) {
    const i64 rr = i64(a.re) * b.re + i64(a.im) * b.im;
    const i64 ii = i64(a.re) * b.im - i64(a.im) * b.re;
    return ci16(i16(round_sat(rr, shift, 16)), i16(round_sat(ii, shift, 16)));
}

/// Exact power, I*I + Q*Q.  Unsigned, no rounding, never saturates for s16
/// inputs (max 2 * 32768^2 = 2^31, which fits u32).
inline u32 power(ci16 a) {
    return u32(i32(a.re) * i32(a.re) + i32(a.im) * i32(a.im));
}

/// Float to Q0.15 with the same rounding and clamping the DAC path uses.
inline i16 to_q15(double x) {
    const i64 v = i64(std::llround(x * 32768.0));
    return i16(sat(v, 16));
}

} // namespace fx

//----------------------------------------------------------------------------
// Cache-aligned buffer.  Every hot array in the pipeline uses this so that
// AVX2 loads are aligned and adjacent buffers never share a cache line.
//----------------------------------------------------------------------------
constexpr std::size_t kCacheLine = 64;

template <typename T>
class AlignedBuffer {
public:
    AlignedBuffer() = default;
    explicit AlignedBuffer(std::size_t n) { resize(n); }

    void resize(std::size_t n) {
        if (n == n_) return;
        T* p = nullptr;
        if (n) {
            const std::size_t bytes = ((n * sizeof(T) + kCacheLine - 1) / kCacheLine) * kCacheLine;
            p = static_cast<T*>(::operator new(bytes, std::align_val_t(kCacheLine)));
            for (std::size_t i = 0; i < n; ++i) new (p + i) T();
        }
        release();
        data_ = p;
        n_    = n;
    }

    void release() {
        if (data_) {
            for (std::size_t i = 0; i < n_; ++i) data_[i].~T();
            ::operator delete(data_, std::align_val_t(kCacheLine));
        }
        data_ = nullptr;
        n_    = 0;
    }

    ~AlignedBuffer() { release(); }
    AlignedBuffer(const AlignedBuffer&)            = delete;
    AlignedBuffer& operator=(const AlignedBuffer&) = delete;
    AlignedBuffer(AlignedBuffer&& o) noexcept : data_(o.data_), n_(o.n_) { o.data_ = nullptr; o.n_ = 0; }
    AlignedBuffer& operator=(AlignedBuffer&& o) noexcept {
        if (this != &o) { release(); data_ = o.data_; n_ = o.n_; o.data_ = nullptr; o.n_ = 0; }
        return *this;
    }

    T*          data()       { return data_; }
    const T*    data() const { return data_; }
    std::size_t size() const { return n_; }
    bool        empty() const { return n_ == 0; }
    T&          operator[](std::size_t i)       { return data_[i]; }
    const T&    operator[](std::size_t i) const { return data_[i]; }
    T*          begin()       { return data_; }
    T*          end()         { return data_ + n_; }
    const T*    begin() const { return data_; }
    const T*    end()   const { return data_ + n_; }
    void        zero() { if (data_) std::memset(static_cast<void*>(data_), 0, n_ * sizeof(T)); }

private:
    T*          data_ = nullptr;
    std::size_t n_    = 0;
};

//----------------------------------------------------------------------------
// Non-owning view.  std::span is C++20; this build targets C++17.
//----------------------------------------------------------------------------
template <typename T>
struct Span {
    T*          ptr = nullptr;
    std::size_t len = 0;
    Span() = default;
    Span(T* p, std::size_t n) : ptr(p), len(n) {}
    template <typename U> Span(AlignedBuffer<U>& b) : ptr(b.data()), len(b.size()) {}
    template <typename U> Span(std::vector<U>& b) : ptr(b.data()), len(b.size()) {}
    T&          operator[](std::size_t i) const { return ptr[i]; }
    std::size_t size() const { return len; }
    bool        empty() const { return len == 0; }
    T*          begin() const { return ptr; }
    T*          end()   const { return ptr + len; }
    Span        subspan(std::size_t off, std::size_t n) const { return Span(ptr + off, n); }
};

//----------------------------------------------------------------------------
// Small helpers used throughout.
//----------------------------------------------------------------------------
constexpr double kPi = 3.14159265358979323846;

inline double db(double linear)      { return 10.0 * std::log10(linear + 1e-300); }
inline double db_amp(double linear)  { return 20.0 * std::log10(std::abs(linear) + 1e-300); }
inline double undb(double d)         { return std::pow(10.0, d / 10.0); }
inline double deg(double rad)        { return rad * 180.0 / kPi; }
inline double rad(double deg_)       { return deg_ * kPi / 180.0; }

template <typename T> constexpr T clampv(T v, T lo, T hi) { return v < lo ? lo : (v > hi ? hi : v); }

inline bool is_pow2(std::size_t n) { return n && ((n & (n - 1)) == 0); }
inline int  log2i(std::size_t n)   { int k = 0; while ((std::size_t(1) << k) < n) ++k; return k; }

} // namespace radar
