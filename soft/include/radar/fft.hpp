//============================================================================
// fft.hpp -- complex FFT with a pluggable backend
//
// Three backends, chosen at build time and reported at runtime:
//   accelerate  Apple vDSP.  A system framework, so no install, and it uses
//               AVX2 on this Intel machine.  Default on macOS.
//   fftw        FFTW3 single precision, if it was found.
//   builtin     Self-contained Stockham radix-4/2 with an AVX2 inner loop.
//               Always available, so the stack builds anywhere.
//
// All three are verified against a direct DFT by radar-selftest, and
// radar-bench times them so the fastest can be picked for this machine
// rather than assumed.
//============================================================================
#pragma once

#include "radar/core.hpp"
#include <memory>

namespace radar {

class Fft {
public:
    Fft() = default;
    Fft(int n, bool inverse);
    ~Fft();
    Fft(Fft&&) noexcept;
    Fft& operator=(Fft&&) noexcept;
    Fft(const Fft&)            = delete;
    Fft& operator=(const Fft&) = delete;

    /// n must be a power of two, 8 <= n <= 65536.
    void plan(int n, bool inverse);
    int  size() const { return n_; }
    bool inverse() const { return inverse_; }

    /// In-place transform of `batch` contiguous blocks of `size()` samples.
    /// Unnormalised in both directions: a forward then inverse pair scales by
    /// n, matching the FPGA's unscaled convention.
    void run(cf32* data, int batch = 1) const;

    /// Out-of-place. `in` and `out` must not overlap.
    void run(const cf32* in, cf32* out, int batch = 1) const;

    /// Which implementation this build uses.
    static const char* backend();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    int  n_       = 0;
    bool inverse_ = false;
};

/// Reference transform, O(n^2), any n. Used only to check the fast paths.
void dft_reference(const cf32* in, cf32* out, int n, bool inverse);

/// Rotate a spectrum so that bin 0 lands in the middle, in place.
/// n must be even. Used to centre Doppler on zero velocity.
void fftshift(cf32* data, int n);
void fftshift(float* data, int n);

} // namespace radar
