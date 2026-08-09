//============================================================================
// refmodel.hpp -- the bit-exact model of the FPGA datapath
//
// This is the file the rest of the project is measured against.  It does, in
// C++ integer arithmetic, exactly what the fabric does: the same de-chirp
// product, the same halfband coefficients, the same window table, the same
// 16-bit butterflies with the same per-stage scaling and the same
// round-half-up-then-saturate at every truncation.
//
// It earns its keep twice over.
//
//   For verification, an RTL testbench can drive the same stimulus through
//   both and compare integers, not tolerances.  A mismatch is a bug, full
//   stop -- there is no "close enough" to argue about.
//
//   For development, the whole radar runs on a laptop with no hardware
//   attached.  Feed process_cpi() synthetic echoes and everything downstream
//   -- detection, angle, tracking, the web display -- sees precisely the
//   numbers it will see on the day the gateware is loaded.
//
// A floating-point twin of the same chain sits alongside it, so the cost of
// the fixed point is a measured number rather than an assumption.
//============================================================================
#pragma once

#include "radar/config.hpp"
#include "radar/core.hpp"
#include "radar/fft.hpp"
#include "radar/types.hpp"
#include "radar/waveform.hpp"

#include <memory>
#include <mutex>
#include <vector>

namespace radar {

//----------------------------------------------------------------------------
// The halfband decimator, generated rather than tabulated.
//
// 23 taps, symmetric, every even offset from the centre zero -- the structure
// that lets the fabric decimate by two with seven multipliers instead of
// twenty-three.  The recipe, which radar_halfband.v's initial block follows to
// the letter:
//
//   m         = k - 11                       k = 0 .. 22
//   ideal[m]  = 0.5                          when m == 0
//             = 0                            when m is even and non-zero
//             = 0.5 * sin(pi*m/2)/(pi*m/2)   otherwise
//   kaiser[m] = I0(beta * sqrt(1 - (m/11)^2)) / I0(beta),   beta = 7.857
//   h[m]      = ideal[m] * kaiser[m]
//   h        /= sum(h)                       unit direct-current gain, doubles
//   q[k]      = llround(h[m] * 131072)       131072 = 2^17, the Q0.17 scale
//   q[11]    += 131072 - sum(q)              rounding residue into the centre
//
// I0 is the modified Bessel function of the first kind, order zero, summed as
// I0(x) = sum ((x/2)^i / i!)^2 until a term falls below 1e-18 of the running
// total.  beta = 7.857 is the classic Kaiser value for 80 dB of stopband.
//
// The final line is what makes the gain exactly one: quantising 23 doubles
// leaves a residue of a few parts in 131072, and putting all of it in the
// centre tap keeps the filter symmetric while forcing sum(q) == 2^17, so a
// constant passes through the cascade completely unchanged.
//----------------------------------------------------------------------------
const std::vector<i32>& halfband_taps_q17();

//----------------------------------------------------------------------------
// Fixed-point transform scaling.
//
// A 16-bit FFT has to shed the growth it creates or it saturates, and it must
// not shed more than that or it throws away small signals.  Each butterfly
// stage can at most double a value, so the schedule is worked out from the
// only thing that bounds the data: the window that feeds the transform.
//
// After stage s a value is the sum of 2^(s+1) windowed inputs spaced
// n/2^(s+1) apart, so its magnitude cannot exceed full scale times the largest
// such sum of window coefficients.  The shift for that stage is whatever is
// needed to bring that bound back under full scale, and no more.  With
// Blackman-Harris over 768 samples zero-padded to 1024 the coefficients sum to
// 275.5, so nine bits come out across the ten stages instead of the ten a
// blind one-bit-per-stage schedule would take -- 6 dB of small-signal
// sensitivity, free, and still provably impossible to overflow.
//
// The host writes the result to REG_FFT_SCALE_R / REG_FFT_SCALE_D, so the
// fabric and this model cannot disagree: there is one calculation and its
// answer is programmed in.
//----------------------------------------------------------------------------
std::vector<int> fft_scale_stages(const std::vector<i16>& win_q15, int n_fft);
u32              fft_scale_word(const std::vector<int>& stages);

/// The two window tables exactly as the FPGA holds them.  Free functions so
/// that the host's register writes and the model build the identical table.
std::vector<i16> range_window_table(const Config& c);
std::vector<i16> dopp_window_table(const Config& c);

/// Chirps that reach one virtual channel: n_chirp in every MIMO mode, since
/// time-multiplexing halves the chirps per transmitter while doubling the
/// total.  The Doppler transform is this long before zero padding.
int chirps_per_channel(const Config& c);

class RefModel {
public:
    explicit RefModel(const Config& c);

    //--------------------------------------------------------------------
    // One receive channel, one chirp.
    //
    // De-chirp, decimate by four, window, range transform.  `adc` holds `n`
    // samples of one sweep; fewer than a full sweep is zero-filled, more is
    // ignored.  `out_range_bins` receives the complete transform, all
    // n_range_fft bins, of which the fabric keeps and streams the first
    // n_range.  Every step is the integer arithmetic of the fabric.
    //--------------------------------------------------------------------
    void range_chirp(const ci16* adc, int n, ci16* out_range_bins) const;

    //--------------------------------------------------------------------
    // A whole coherent interval.
    //
    // rx_chirps[rx] points at n_chirp_total * n_sweep consecutive samples,
    // chirp-major.  Out comes the complex range-Doppler cube for the four
    // virtual channels and the integrated power map, both in the fabric's
    // fixed point: the cube holds the s16 real and imaginary parts as they
    // stand, and the map holds the unsigned 32-bit sum of their squares over
    // the four channels.  The Doppler axis is centred, so index n_doppler/2
    // is zero velocity and index n_doppler/2 + d is Doppler bin d.
    //--------------------------------------------------------------------
    void process_cpi(const ci16* const* rx_chirps, int n_rx, int n_chirp_total,
                     RdFrame& out) const;

    /// The same chain in floating point, using the same quantised tables, so
    /// the difference between the two is purely what the fixed point cost.
    /// Input samples are the usual normalised [-1, 1) complex floats.
    void process_cpi_float(const cf32* const* rx_chirps, int n_rx, int n_chirp_total,
                           RdFrame& out) const;

    const std::vector<i32>& halfband_coefs()   const { return hb_; }
    const std::vector<i16>& range_window_q15() const { return rwin_; }
    const std::vector<i16>& dopp_window_q15()  const { return dwin_; }

    /// The scaling schedules as written to the settings bus.
    u32 range_fft_scale_word() const { return fft_scale_word(rshift_); }
    u32 dopp_fft_scale_word()  const { return fft_scale_word(dshift_); }
    /// Total bits shed by each transform, useful for scaling comparisons.
    int range_fft_total_shift() const;
    int dopp_fft_total_shift()  const;

    const Config&   config()   const { return cfg_; }
    const Waveform& waveform() const { return wf_; }

private:
    Config   cfg_;
    Waveform wf_;

    std::vector<i32>  hb_;        ///< 23 halfband taps, Q0.17
    std::vector<int>  hb_nz_;     ///< indices of the taps that are not zero
    std::vector<i16>  rwin_, dwin_;
    std::vector<int>  rshift_, dshift_;
    std::vector<ci16> rtw_, dtw_; ///< Q0.15 twiddles, forward and inverse
    std::vector<int>  rbrev_, dbrev_;
    int               n_dopp_in_ = 0;

    // Float-path transforms, built the first time the float chain is asked
    // for.  Planning them costs real time with some backends and the fixed
    // path -- the one that runs on every frame -- must not pay for it.
    mutable std::unique_ptr<Fft> fft_r_, fft_d_;
    mutable std::once_flag       fft_once_;
    void ensure_float_plans() const;
};

} // namespace radar
