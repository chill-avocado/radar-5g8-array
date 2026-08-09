//============================================================================
// waveform.cpp -- the numerically-controlled oscillator, bit for bit
//
// THE CONTRACT WITH radar_nco.v
// -----------------------------
// Two registers, both 32 bits, both updated on every radio clock from their
// previous values -- which is what a pair of non-blocking assignments in one
// always_ff block does, and is the whole of the arithmetic:
//
//     phase <= phase + freq;      // uses the OLD freq
//     freq  <= freq  + slope;
//
// At reset phase is 0 and freq is freq_start_inc.  The sample emitted on clock
// n is read from the sine table at the CURRENT phase, before either register
// moves, so
//
//     phase(n) = freq_start * n + slope * n * (n - 1) / 2   (mod 2^32)
//     freq(n)  = freq_start + slope * n                     (mod 2^32)
//
// Both wrap as unsigned 32-bit values.  The instantaneous frequency therefore
// runs from freq_start to freq_start + (N-1) * slope, which for the standard
// programming is -B/2 up to very nearly +B/2: the sweep is symmetric about the
// carrier to within one slope step.
//
// The output is a complex sample, cosine on I and sine on Q, both taken from
// the same table one quarter turn apart:
//
//     idx = phase[31:20]                       // top 12 bits, 4096 entries
//     I   = table[(idx + 1024) & 4095]
//     Q   = table[idx]
//
// If the RTL and this file ever disagree, the de-chirp reference stops
// matching what was transmitted and the range origin drifts by an amount
// nothing else can see.  That is why the arithmetic is spelled out above
// rather than left to be inferred from the code.
//============================================================================
#include "radar/waveform.hpp"

#include <cmath>

namespace radar {

//----------------------------------------------------------------------------
// NCO programming words
//----------------------------------------------------------------------------
i32 nco_freq_start_inc(const Config& c) {
    const double two32 = 4294967296.0;
    return i32(std::llround(two32 * (-c.sweep_bw_hz * 0.5) / c.sample_rate_hz));
}

i32 nco_freq_slope_inc(const Config& c) {
    const double two32 = 4294967296.0;
    const double per_clock = c.sweep_bw_hz / double(c.n_sweep <= 0 ? 1 : c.n_sweep);
    return i32(std::llround(two32 * per_clock / c.sample_rate_hz));
}

const std::vector<i16>& nco_sine_table() {
    static const std::vector<i16> table = [] {
        std::vector<i16> t(4096);
        for (int k = 0; k < 4096; ++k) {
            const double a = 2.0 * kPi * (double(k) + 0.5) / 4096.0;
            t[std::size_t(k)] = i16(std::llround(32767.0 * std::sin(a)));
        }
        return t;
    }();
    return table;
}

//----------------------------------------------------------------------------
// Waveform
//----------------------------------------------------------------------------
Waveform::Waveform(const Config& c) : cfg_(c) {
    cfg_.derive();
    freq_start_ = nco_freq_start_inc(cfg_);
    freq_slope_ = nco_freq_slope_inc(cfg_);

    const std::vector<i16>& tab = nco_sine_table();
    const int n = cfg_.n_sweep > 0 ? cfg_.n_sweep : 0;

    q15_.resize(std::size_t(n));
    f32_.resize(std::size_t(n));

    u32 phase = 0;
    u32 freq  = u32(freq_start_);            // two's complement, wraps
    const u32 slope = u32(freq_slope_);

    for (int i = 0; i < n; ++i) {
        const u32 idx = (phase >> 20) & 0xFFFu;
        const ci16 s(tab[(idx + 1024u) & 0xFFFu], tab[idx]);
        q15_[std::size_t(i)] = s;
        f32_[std::size_t(i)] = s.to_float();
        phase += freq;                       // old freq, as the RTL does
        freq  += slope;
    }
}

int Waveform::tx_for_chirp(int k) const {
    switch (cfg_.mimo) {
        case MimoMode::Tdm:     return k & 1;
        case MimoMode::Ddm:     return -1;      // both, separated in Doppler
        case MimoMode::Tx0Only: return 0;
        case MimoMode::Tx1Only: return 1;
    }
    return 0;
}

int Waveform::ddm_sign(int k, int tx) const {
    if (cfg_.mimo != MimoMode::Ddm) return 1;
    return (tx == 1 && (k & 1)) ? -1 : 1;
}

double Waveform::beat_hz_per_metre() const {
    return 2.0 * cfg_.d.chirp_slope_hz_s / phys::c0;
}

double Waveform::range_of_bin(int bin) const {
    return double(bin - cfg_.range_zero_bin) * cfg_.d.range_bin_m;
}

int Waveform::bin_of_range(double m) const {
    if (cfg_.d.range_bin_m <= 0.0) return cfg_.range_zero_bin;
    return int(std::llround(m / cfg_.d.range_bin_m)) + cfg_.range_zero_bin;
}

double Waveform::velocity_of_bin(int dopp_bin) const {
    // One Doppler bin is one cycle of phase advance across the whole coherent
    // interval, so the velocity step is lambda / (2 * T_eff * N_doppler).
    const double eff_pri = cfg_.d.t_pri_s * (cfg_.mimo == MimoMode::Tdm ? 2.0 : 1.0);
    const int    nd      = cfg_.n_doppler > 0 ? cfg_.n_doppler : 1;
    if (eff_pri <= 0.0) return 0.0;
    return double(dopp_bin) * cfg_.d.lambda_m / (2.0 * eff_pri * double(nd));
}

//----------------------------------------------------------------------------
// Range-Doppler coupling
//----------------------------------------------------------------------------
double doppler_range_coupling_m(const Config& c, double v_ms) {
    const double mu = c.d.chirp_slope_hz_s;
    if (mu == 0.0) return 0.0;
    return -v_ms * c.centre_freq_hz / mu;
}

void decouple_range_doppler(const Config& c, double& range_m, double v_ms) {
    range_m -= doppler_range_coupling_m(c, v_ms);
}

void decouple_range_doppler(const Config& c, std::vector<Hit>& hits) {
    for (Hit& h : hits) h.range_m -= doppler_range_coupling_m(c, h.velocity_ms);
}

} // namespace radar
