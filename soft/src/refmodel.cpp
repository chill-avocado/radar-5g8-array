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
//============================================================================
#include "radar/refmodel.hpp"

#include "radar/window.hpp"

#include <algorithm>
#include <cstring>

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

/// Q0.15 twiddle table for a decimation-in-frequency transform of length n.
/// 32767 rather than 32768 because +1.0 has no s16 representation; the
/// resulting gain error is three parts in a hundred thousand per multiply and
/// the fabric's ROM holds the same numbers.
std::vector<ci16> make_twiddles_q15(int n, bool inverse) {
    std::vector<ci16> t(static_cast<std::size_t>(n / 2));
    const double sgn = inverse ? 1.0 : -1.0;
    for (int k = 0; k < n / 2; ++k) {
        const double a = sgn * 2.0 * kPi * double(k) / double(n);
        t[std::size_t(k)] = ci16(i16(std::llround(32767.0 * std::cos(a))),
                                 i16(std::llround(32767.0 * std::sin(a))));
    }
    return t;
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

//----------------------------------------------------------------------------
// The 16-bit transform.
//
// Decimation in frequency: natural order in, bit-reversed out, reordered at
// the end.  Each stage adds and subtracts, applies its scaling shift with
// round-half-up and saturation, and multiplies the difference by a twiddle --
// except where the twiddle is exactly one, which the fabric implements as no
// multiplier at all, so neither does this.
//----------------------------------------------------------------------------
void fixed_fft(ci16* x, int n, const ci16* tw, const int* shift, const int* brev) {
    int stage = 0;
    int step  = 1;
    for (int len = n; len >= 2; len >>= 1, ++stage, step <<= 1) {
        const int half = len >> 1;
        const int sc   = shift[stage];
        for (int i = 0; i < n; i += len) {
            for (int j = 0; j < half; ++j) {
                const ci16 a = x[i + j];
                const ci16 b = x[i + j + half];
                const i32 sr = i32(a.re) + i32(b.re);
                const i32 si = i32(a.im) + i32(b.im);
                const i32 dr = i32(a.re) - i32(b.re);
                const i32 di = i32(a.im) - i32(b.im);
                x[i + j] = ci16(i16(fx::round_sat(sr, sc, 16)),
                                i16(fx::round_sat(si, sc, 16)));
                if (j == 0) {
                    x[i + j + half] = ci16(i16(fx::round_sat(dr, sc, 16)),
                                           i16(fx::round_sat(di, sc, 16)));
                } else {
                    const ci16 w = tw[j * step];
                    const i64 rr = i64(dr) * w.re - i64(di) * w.im;
                    const i64 ii = i64(dr) * w.im + i64(di) * w.re;
                    x[i + j + half] = ci16(i16(fx::round_sat(rr, 15 + sc, 16)),
                                           i16(fx::round_sat(ii, 15 + sc, 16)));
                }
            }
        }
    }
    for (int i = 0; i < n; ++i) {
        const int r = brev[i];
        if (i < r) std::swap(x[i], x[r]);
    }
}

/// Rotate a cyclic buffer left by k places -- the fabric's address bit flip.
template <typename T>
void rotate_left(T* p, int n, int k) {
    if (n <= 1 || k <= 0 || k >= n) return;
    std::rotate(p, p + k, p + n);
}

/// Scratch for one chirp through the range chain.  Thread-local so several
/// worker threads can be inside range_chirp() at once, which the pipeline
/// relies on.
struct ChirpScratch {
    std::vector<ci16> dech, hb1, hb2;
};
ChirpScratch& chirp_scratch() {
    static thread_local ChirpScratch s;
    return s;
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
// RefModel
//============================================================================
RefModel::RefModel(const Config& c) : cfg_(c), wf_(c) {
    cfg_.derive();

    hb_ = halfband_taps_q17();
    for (int k = 0; k < int(hb_.size()); ++k) if (hb_[std::size_t(k)] != 0) hb_nz_.push_back(k);

    rwin_ = range_window_table(cfg_);
    dwin_ = dopp_window_table(cfg_);
    n_dopp_in_ = int(dwin_.size());

    rshift_ = fft_scale_stages(rwin_, cfg_.n_range_fft);
    dshift_ = fft_scale_stages(dwin_, cfg_.n_doppler);

    rtw_ = make_twiddles_q15(cfg_.n_range_fft, false);
    dtw_ = make_twiddles_q15(cfg_.n_doppler,   true);   // conjugate kernel
    rbrev_ = make_bitrev(cfg_.n_range_fft);
    dbrev_ = make_bitrev(cfg_.n_doppler);
}

int RefModel::range_fft_total_shift() const {
    int t = 0; for (int s : rshift_) t += s; return t;
}
int RefModel::dopp_fft_total_shift() const {
    int t = 0; for (int s : dshift_) t += s; return t;
}

void RefModel::ensure_float_plans() const {
    std::call_once(fft_once_, [this] {
        fft_r_.reset(new Fft(cfg_.n_range_fft, false));
        fft_d_.reset(new Fft(cfg_.n_doppler,   true));
    });
}

//----------------------------------------------------------------------------
// One chirp, one receive channel
//----------------------------------------------------------------------------
void RefModel::range_chirp(const ci16* adc, int n, ci16* out_range_bins) const {
    const int ns   = cfg_.n_sweep;
    const int nd1  = ns / 2;
    const int nd2  = nd1 / 2;
    const int nfft = cfg_.n_range_fft;

    ChirpScratch& sc = chirp_scratch();
    if (int(sc.dech.size()) < ns)   sc.dech.assign(std::size_t(ns), ci16());
    if (int(sc.hb1.size())  < nd1)  sc.hb1.assign(std::size_t(nd1), ci16());
    if (int(sc.hb2.size())  < nd2)  sc.hb2.assign(std::size_t(nd2), ci16());

    // 1. De-chirp.  conjugate(received) * reference, the product rounded and
    //    saturated back to Q0.15 with the shift the fabric is programmed with.
    const std::vector<ci16>& ref = wf_.chirp_q15();
    const int navail = std::min(n, ns);
    for (int i = 0; i < navail; ++i) {
        sc.dech[std::size_t(i)] = fx::cmul_conj_q15(adc[i], ref[std::size_t(i)], 15);
    }
    for (int i = navail; i < ns; ++i) sc.dech[std::size_t(i)] = ci16();

    // 2. Two halfband stages, 61.44 -> 30.72 -> 15.36 MSps.  The delay line
    //    starts empty at every sweep, as it does in the fabric where the
    //    sequencer flushes it during the retrace.
    const i32* h = hb_.data();
    auto halfband = [&](const ci16* in, int nin, ci16* out) {
        for (int t = 0; t < nin; t += 2) {
            i64 ar = 0, ai = 0;
            for (int k : hb_nz_) {
                const int s = t - k;
                if (s < 0) break;                 // taps run in increasing k
                ar += i64(h[k]) * in[s].re;
                ai += i64(h[k]) * in[s].im;
            }
            out[t / 2] = ci16(i16(fx::round_sat(ar, 17, 16)),
                              i16(fx::round_sat(ai, 17, 16)));
        }
    };
    halfband(sc.dech.data(), ns,  sc.hb1.data());
    halfband(sc.hb1.data(),  nd1, sc.hb2.data());

    // 3. Window, then zero pad out to the transform length.
    const int wlen = std::min(nd2, int(rwin_.size()));
    for (int i = 0; i < wlen; ++i) {
        const i32 w = rwin_[std::size_t(i)];
        out_range_bins[i] = ci16(i16(fx::round_sat(i64(sc.hb2[std::size_t(i)].re) * w, 15, 16)),
                                 i16(fx::round_sat(i64(sc.hb2[std::size_t(i)].im) * w, 15, 16)));
    }
    for (int i = wlen; i < nfft; ++i) out_range_bins[i] = ci16();

    // 4. Range transform.
    fixed_fft(out_range_bins, nfft, rtw_.data(), rshift_.data(), rbrev_.data());
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
    {
        std::vector<ci16> full(static_cast<std::size_t>(nfft));
        for (int rx = 0; rx < nrx; ++rx) {
            for (int k = 0; k < n_chirp_total; ++k) {
                range_chirp(rx_chirps[rx] + std::size_t(k) * ns, ns, full.data());
                std::memcpy(&rb[(std::size_t(rx) * n_chirp_total + k) * nr],
                            full.data(), sizeof(ci16) * std::size_t(nr));
            }
        }
    }

    const int cpc = (cfg_.mimo == MimoMode::Tdm) ? n_chirp_total / 2 : n_chirp_total;
    const int nin = std::min(cpc, n_dopp_in_);

    std::vector<ci16> series(static_cast<std::size_t>(nd));
    std::vector<ci16> alt(static_cast<std::size_t>(nd));

    auto doppler = [&](std::vector<ci16>& buf) {
        for (int i = 0; i < nin; ++i) {
            const i32 w = dwin_[std::size_t(i)];
            buf[std::size_t(i)] = ci16(i16(fx::round_sat(i64(buf[std::size_t(i)].re) * w, 15, 16)),
                                       i16(fx::round_sat(i64(buf[std::size_t(i)].im) * w, 15, 16)));
        }
        for (int i = nin; i < nd; ++i) buf[std::size_t(i)] = ci16();
        fixed_fft(buf.data(), nd, dtw_.data(), dshift_.data(), dbrev_.data());
        rotate_left(buf.data(), nd, nd / 2);      // zero velocity to the middle
    };

    auto store = [&](int v, int r, const std::vector<ci16>& spec) {
        for (int d = 0; d < nd; ++d) {
            out.cube_at(v, r, d) = cf32(float(spec[std::size_t(d)].re),
                                        float(spec[std::size_t(d)].im));
        }
    };

    for (int rx = 0; rx < nrx; ++rx) {
        const ci16* base = &rb[std::size_t(rx) * n_chirp_total * nr];
        for (int r = 0; r < nr; ++r) {
            switch (cfg_.mimo) {
                case MimoMode::Tdm: {
                    for (int tx = 0; tx < 2; ++tx) {
                        for (int i = 0; i < nin; ++i) {
                            const int k = 2 * i + tx;
                            series[std::size_t(i)] = (k < n_chirp_total)
                                ? base[std::size_t(k) * nr + r] : ci16();
                        }
                        doppler(series);
                        store(tx * 2 + rx, r, series);
                    }
                    break;
                }
                case MimoMode::Ddm: {
                    for (int i = 0; i < nin; ++i) series[std::size_t(i)] = base[std::size_t(i) * nr + r];
                    doppler(series);
                    store(0 * 2 + rx, r, series);
                    // Transmitter 1 was inverted on odd chirps, so its echo
                    // sits half a Doppler band away; rotating the spectrum
                    // back by half its length separates the two.
                    alt = series;
                    rotate_left(alt.data(), nd, nd / 2);
                    store(1 * 2 + rx, r, alt);
                    break;
                }
                case MimoMode::Tx0Only:
                case MimoMode::Tx1Only: {
                    const int tx = (cfg_.mimo == MimoMode::Tx0Only) ? 0 : 1;
                    for (int i = 0; i < nin; ++i) series[std::size_t(i)] = base[std::size_t(i) * nr + r];
                    doppler(series);
                    store(tx * 2 + rx, r, series);
                    break;
                }
            }
        }
    }

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
        std::vector<cf32> dech(static_cast<std::size_t>(ns)), h1(static_cast<std::size_t>(nd1)), h2(static_cast<std::size_t>(nd2));
        AlignedBuffer<cf32> buf(static_cast<std::size_t>(nfft));

        auto halfband = [&](const cf32* in, int nin_, cf32* outp) {
            for (int t = 0; t < nin_; t += 2) {
                double ar = 0.0, ai = 0.0;
                for (int k : hb_nz_) {
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
                halfband(dech.data(), ns,  h1.data());
                halfband(h1.data(),   nd1, h2.data());

                const int wlen = std::min(nd2, int(rwf.size()));
                for (int i = 0; i < wlen; ++i) buf[std::size_t(i)] = h2[std::size_t(i)] * float(rwf[std::size_t(i)]);
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
        for (int i = 0; i < nin; ++i) series[std::size_t(i)] = series[std::size_t(i)] * float(dwf[std::size_t(i)]);
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
