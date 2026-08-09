//============================================================================
// tb_fft.cpp -- Verilator testbench for radar_fft
//
// Streams frames back to back into the hardware model and compares every
// output bin against a double-precision reference DFT of the same integer
// samples, with the same 1/N scaling the hardware applies.
//
// Four transform sizes are exercised: 1024 (range) and 512 / 256 / 128, the
// three Doppler sizes the runtime range-by-chirp split can select.
//
// Built by run_fft.sh, which verilates radar_fft four times with different
// parameters and links all four models into this one program.
//============================================================================
#include "Vfft1024.h"
#include "Vfft512.h"
#include "Vfft256.h"
#include "Vfft128.h"
#include "Vfftrev1024.h"

#include <verilated.h>

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdint>
#include <cstdio>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace {

const double PI = 3.14159265358979323846;

using cd  = std::complex<double>;
using ci  = std::pair<int16_t, int16_t>;   // one quantised complex sample

int g_fail = 0;

//----------------------------------------------------------------------------
// Quantise to the s16 Q0.15 the datapath carries.
//----------------------------------------------------------------------------
int16_t q15(double v) {
    long r = std::lround(v);
    if (r >  32767) r =  32767;
    if (r < -32768) r = -32768;
    return static_cast<int16_t>(r);
}

//----------------------------------------------------------------------------
// Reference transform: plain O(N^2) DFT of the integer samples, scaled by 1/N
// so it lands in the same units as the hardware output.  The index product is
// reduced modulo N before the angle is formed, which keeps every cosine
// argument inside one turn and the accumulated error near machine epsilon.
//----------------------------------------------------------------------------
std::vector<cd> dft_ref(const std::vector<ci>& x) {
    const int n = static_cast<int>(x.size());
    std::vector<cd> out(n);
    for (int k = 0; k < n; ++k) {
        cd acc(0.0, 0.0);
        for (int t = 0; t < n; ++t) {
            const long long m = (static_cast<long long>(k) * t) % n;
            const double    a = -2.0 * PI * static_cast<double>(m) / n;
            acc += cd(x[t].first, x[t].second) * cd(std::cos(a), std::sin(a));
        }
        out[k] = acc / static_cast<double>(n);
    }
    return out;
}

//----------------------------------------------------------------------------
// Signal generators.  Every frame of a case is different, so a frame boundary
// that leaked would show up immediately.
//----------------------------------------------------------------------------
std::vector<ci> make_tone(int n, double bin, double amp, double phase) {
    std::vector<ci> x(n);
    for (int t = 0; t < n; ++t) {
        const double a = 2.0 * PI * bin * t / n + phase;
        x[t] = ci(q15(amp * std::cos(a)), q15(amp * std::sin(a)));
    }
    return x;
}

std::vector<ci> make_random(int n, double amp, std::mt19937& rng) {
    // Uniform in both components, peak scaled so the complex magnitude never
    // exceeds full scale -- the datapath's limit is |x|, not |I| and |Q|.
    std::uniform_real_distribution<double> u(-1.0, 1.0);
    std::vector<ci> x(n);
    for (int t = 0; t < n; ++t) x[t] = ci(q15(amp * u(rng)), q15(amp * u(rng)));
    return x;
}

std::vector<ci> make_impulse(int n, double amp, double phase) {
    std::vector<ci> x(n, ci(0, 0));
    x[0] = ci(q15(amp * std::cos(phase)), q15(amp * std::sin(phase)));
    return x;
}

uint32_t bitrev(uint32_t v, int bits) {
    uint32_t r = 0;
    for (int b = 0; b < bits; ++b)
        if (v & (1u << b)) r |= 1u << (bits - 1 - b);
    return r;
}

//----------------------------------------------------------------------------
// Two largest bins by magnitude.
//----------------------------------------------------------------------------
void top_two(const std::vector<cd>& v, int& first, int& second) {
    first = second = -1;
    double b0 = -1.0, b1 = -1.0;
    for (int k = 0; k < static_cast<int>(v.size()); ++k) {
        const double m = std::abs(v[k]);
        if (m > b0)      { b1 = b0; second = first; b0 = m; first = k; }
        else if (m > b1) { b1 = m;  second = k; }
    }
}

//----------------------------------------------------------------------------
// One run of the model: reset, then stream frames with in_valid held high the
// whole time, capturing output frames until enough have arrived.
//----------------------------------------------------------------------------
struct RunOut {
    std::vector<std::vector<cd>> frames;
    int  latency  = -1;
    bool idx_ok   = true;
    bool last_ok  = true;
    bool overflow = false;
    bool complete = false;
};

// gap_mod > 0 stalls the input by dropping in_valid on one clock in every
// gap_mod, which must not change a single output bit.
template <class DUT>
RunOut stream(DUT& dut, int N, int NLOG2, uint32_t sch,
              const std::vector<std::vector<ci>>& in_frames, int n_check,
              int gap_mod = 0) {
    RunOut r;

    dut.rst       = 1;
    dut.in_valid  = 0;
    dut.in_i      = 0;
    dut.in_q      = 0;
    dut.in_last   = 0;
    dut.scale_sch = sch;
    for (int t = 0; t < 8; ++t) { dut.clk = 0; dut.eval(); dut.clk = 1; dut.eval(); }
    dut.rst = 0;

    std::vector<cd> cur;
    cur.reserve(N);
    uint32_t  exp_idx   = 0;
    long long tick      = 0;   // clocks
    long long si        = 0;   // input samples actually accepted
    long long first_in  = -1;
    long long max_ticks =
        static_cast<long long>(3 * NLOG2 + 2 * N) +
        static_cast<long long>(n_check + 3) * N + 256;
    if (gap_mod > 0) max_ticks = max_ticks * (gap_mod + 1) / gap_mod + 256;

    while (static_cast<int>(r.frames.size()) < n_check && tick < max_ticks) {
        const bool   v = (gap_mod <= 0) || ((tick % gap_mod) != 0);
        const size_t f = static_cast<size_t>(si / N);
        const int    p = static_cast<int>(si % N);
        int16_t xi = 0, xq = 0;
        if (f < in_frames.size()) { xi = in_frames[f][p].first; xq = in_frames[f][p].second; }

        dut.in_valid = v ? 1 : 0;
        dut.in_i     = static_cast<uint16_t>(xi);
        dut.in_q     = static_cast<uint16_t>(xq);
        dut.in_last  = (v && p == N - 1) ? 1 : 0;
        if (v && first_in < 0) first_in = tick;

        dut.clk = 0; dut.eval();
        dut.clk = 1; dut.eval();

        if (dut.out_valid) {
            if (r.latency < 0) r.latency = static_cast<int>(tick - first_in);
            const uint32_t idx      = dut.out_idx;
            const bool     want_end = (exp_idx == static_cast<uint32_t>(N - 1));
            if (idx != exp_idx)                              r.idx_ok  = false;
            if (static_cast<bool>(dut.out_last) != want_end) r.last_ok = false;
            cur.push_back(cd(static_cast<int16_t>(dut.out_i),
                             static_cast<int16_t>(dut.out_q)));
            exp_idx = want_end ? 0u : exp_idx + 1u;
            if (static_cast<int>(cur.size()) == N) { r.frames.push_back(cur); cur.clear(); }
        }
        if (dut.overflow) r.overflow = true;
        if (v) ++si;
        ++tick;
    }
    r.complete = (static_cast<int>(r.frames.size()) == n_check);
    return r;
}

//----------------------------------------------------------------------------
// Error metric: RMS of the complex difference over every checked bin of every
// checked frame, in dB relative to full scale (1.0 == 32768 LSB).
//----------------------------------------------------------------------------
double rms_dbfs(const std::vector<std::vector<cd>>& hw,
                const std::vector<std::vector<cd>>& ref) {
    double  acc = 0.0;
    long    cnt = 0;
    for (size_t f = 0; f < hw.size(); ++f) {
        for (size_t k = 0; k < hw[f].size(); ++k) {
            acc += std::norm(hw[f][k] - ref[f][k]);
            ++cnt;
        }
    }
    if (cnt == 0) return 0.0;
    const double rms = std::sqrt(acc / static_cast<double>(cnt));
    return 20.0 * std::log10((rms + 1e-300) / 32768.0);
}

void report(bool ok, const std::string& text) {
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", text.c_str());
    if (!ok) ++g_fail;
}

const double RMS_LIMIT_DB = -55.0;

//----------------------------------------------------------------------------
// Everything for one transform size.
//----------------------------------------------------------------------------
template <class DUT>
void run_size(int N, int NLOG2) {
    DUT dut;

    const int    n_check   = 4;
    const int    n_frames  = n_check + 8;
    const int    bin       = (N > 137) ? 137 : 37;
    const double full      = 32767.0;                  // |x| = 1.0
    const double rand_amp  = 32767.0 / std::sqrt(2.0); // |x| <= 1.0 as well

    uint32_t sch_half = 0;   // >>1 at every stage: overall gain 1/N
    for (int k = 0; k < NLOG2; ++k) sch_half |= (1u << (2 * k));

    std::printf("\n--- N = %d  (NLOG2 = %d, scale_sch = 0x%05x, gain 1/N) ---\n",
                N, NLOG2, sch_half);

    struct Case {
        std::string                  name;
        std::vector<std::vector<ci>> in;
        int                          want_peak;   // -1 = do not check
        bool                         half_bin;
        bool                         flat;
    };

    std::vector<Case> cases;
    std::mt19937 rng(0xC0FFEEu + static_cast<unsigned>(N));

    {   // exactly on a bin
        Case c; c.name = "tone on bin " + std::to_string(bin);
        c.want_peak = bin; c.half_bin = false; c.flat = false;
        for (int f = 0; f < n_frames; ++f)
            c.in.push_back(make_tone(N, bin, full, 0.7 * f));
        cases.push_back(c);
    }
    {   // half way between two bins
        Case c; c.name = "tone at bin " + std::to_string(bin) + ".5";
        c.want_peak = bin; c.half_bin = true; c.flat = false;
        for (int f = 0; f < n_frames; ++f)
            c.in.push_back(make_tone(N, bin + 0.5, full, 0.7 * f));
        cases.push_back(c);
    }
    {   // white uniform noise, fresh every frame
        Case c; c.name = "white uniform noise";
        c.want_peak = -1; c.half_bin = false; c.flat = false;
        for (int f = 0; f < n_frames; ++f) c.in.push_back(make_random(N, rand_amp, rng));
        cases.push_back(c);
    }
    {   // impulse at sample 0, output must be flat
        Case c; c.name = "impulse at sample 0";
        c.want_peak = -1; c.half_bin = false; c.flat = true;
        for (int f = 0; f < n_frames; ++f)
            c.in.push_back(make_impulse(N, full, 0.5 * f));
        cases.push_back(c);
    }

    int latency = -1;

    for (const Case& c : cases) {
        RunOut r = stream(dut, N, NLOG2, sch_half, c.in, n_check);

        char line[512];
        if (!r.complete) {
            std::snprintf(line, sizeof line,
                          "N=%-4d %-22s  no output: captured %d of %d frames",
                          N, c.name.c_str(), static_cast<int>(r.frames.size()), n_check);
            report(false, line);
            continue;
        }
        latency = r.latency;

        std::vector<std::vector<cd>> ref;
        for (int f = 0; f < n_check; ++f) ref.push_back(dft_ref(c.in[f]));

        const double rms = rms_dbfs(r.frames, ref);

        bool peak_ok = true;
        std::string peak_txt;
        if (c.want_peak >= 0 && !c.half_bin) {
            peak_ok = true;
            for (int f = 0; f < n_check && peak_ok; ++f) {
                int h0, h1, r0, r1;
                top_two(r.frames[f], h0, h1);
                top_two(ref[f],      r0, r1);
                if (h0 != c.want_peak || r0 != c.want_peak) peak_ok = false;
            }
            peak_txt = "  peak=" + std::to_string(c.want_peak) + (peak_ok ? " ok" : " WRONG");
        } else if (c.half_bin) {
            // A tone half way between bins splits its energy evenly, so the
            // right answer is that the two straddling bins are the top two.
            peak_ok = true;
            for (int f = 0; f < n_check && peak_ok; ++f) {
                int h0, h1, r0, r1;
                top_two(r.frames[f], h0, h1);
                top_two(ref[f],      r0, r1);
                const int lo = std::min(h0, h1), hi = std::max(h0, h1);
                const int rl = std::min(r0, r1), rh = std::max(r0, r1);
                if (lo != c.want_peak || hi != c.want_peak + 1) peak_ok = false;
                if (rl != c.want_peak || rh != c.want_peak + 1) peak_ok = false;
            }
            peak_txt = "  top2={" + std::to_string(c.want_peak) + "," +
                       std::to_string(c.want_peak + 1) + (peak_ok ? "} ok" : "} WRONG");
        }

        std::string flat_txt;
        bool flat_ok = true;
        if (c.flat) {
            double lo = 1e300, hi = -1e300;
            for (int f = 0; f < n_check; ++f)
                for (const cd& v : r.frames[f]) {
                    const double m = std::abs(v);
                    lo = std::min(lo, m); hi = std::max(hi, m);
                }
            const double ripple_db = 20.0 * std::log10((hi + 1e-300) / (lo + 1e-300));
            // An impulse scaled by 1/N comes out at 32767/N LSB, so the
            // flattest a 16-bit result can possibly be is one LSB of spread.
            const double ideal   = full / N;
            const double allowed = 20.0 * std::log10((ideal + 1.0) / (ideal - 1.0));
            char t[128];
            std::snprintf(t, sizeof t, "  flatness=%.3f dB pk-pk (1 LSB floor is %.3f)",
                          ripple_db, allowed);
            flat_txt = t;
            flat_ok  = (ripple_db <= allowed);
        }

        const bool ok = (rms < RMS_LIMIT_DB) && peak_ok && flat_ok &&
                        r.idx_ok && r.last_ok && !r.overflow;

        std::snprintf(line, sizeof line,
                      "N=%-4d %-22s  rms=%8.2f dBFS (limit %.0f)%s%s  idx=%s last=%s ovf=%d",
                      N, c.name.c_str(), rms, RMS_LIMIT_DB,
                      peak_txt.c_str(), flat_txt.c_str(),
                      r.idx_ok ? "ok" : "BAD", r.last_ok ? "ok" : "BAD",
                      r.overflow ? 1 : 0);
        report(ok, line);
    }

    //------------------------------------------------------------------------
    // (e) full-scale input with >>1 everywhere must never set overflow, and
    //     the same input with no shifting must set it -- otherwise the first
    //     result only proves the flag is dead.
    //------------------------------------------------------------------------
    {
        std::vector<std::vector<ci>> fs;
        for (int f = 0; f < n_frames; ++f) fs.push_back(make_tone(N, bin, full, 0.7 * f));

        RunOut a = stream(dut, N, NLOG2, sch_half, fs, n_check);
        char line[256];
        std::snprintf(line, sizeof line,
                      "N=%-4d %-22s  full-scale tone, all >>1, overflow=%d (want 0)",
                      N, "overflow clear", a.overflow ? 1 : 0);
        report(!a.overflow && a.complete, line);

        RunOut b = stream(dut, N, NLOG2, 0u, fs, n_check);
        std::snprintf(line, sizeof line,
                      "N=%-4d %-22s  full-scale tone, no shifts, overflow=%d (want 1)",
                      N, "overflow detects", b.overflow ? 1 : 0);
        report(b.overflow && b.complete, line);
    }

    //------------------------------------------------------------------------
    // A stalled input must not change the answer.  Everything in the datapath
    // is clock-enabled by in_valid, so dropping one clock in five has to give
    // bit-identical results -- if any register were left free-running this is
    // where it would show.
    //------------------------------------------------------------------------
    {
        std::vector<std::vector<ci>> in;
        for (int f = 0; f < n_frames; ++f) in.push_back(make_tone(N, bin, full, 0.7 * f));

        RunOut a = stream(dut, N, NLOG2, sch_half, in, n_check, 0);
        RunOut b = stream(dut, N, NLOG2, sch_half, in, n_check, 5);

        long diffs = 0;
        if (a.complete && b.complete) {
            for (int f = 0; f < n_check; ++f)
                for (int k = 0; k < N; ++k)
                    if (a.frames[f][k] != b.frames[f][k]) ++diffs;
        } else {
            diffs = -1;
        }

        char line[256];
        std::snprintf(line, sizeof line,
                      "N=%-4d %-22s  1 stall clock in 5, %ld of %d bins differ from the "
                      "continuous run  idx=%s last=%s",
                      N, "stalled input", diffs, n_check * N,
                      b.idx_ok ? "ok" : "BAD", b.last_ok ? "ok" : "BAD");
        report(diffs == 0 && b.idx_ok && b.last_ok, line);
    }

    //------------------------------------------------------------------------
    // (f) latency
    //------------------------------------------------------------------------
    {
        char line[256];
        const int expect = 3 * NLOG2 + 2 * N - 1;
        std::snprintf(line, sizeof line,
                      "N=%-4d %-22s  %d clocks first in_valid to first out_valid "
                      "(3*NLOG2 + 2N - 1 = %d)",
                      N, "latency", latency, expect);
        report(latency == expect, line);
    }

    dut.final();
}

} // namespace

// The RTL carries a `timescale, so the Verilated runtime wants a time source.
// Nothing here is time-driven -- the model is clocked by hand -- so it is zero.
double sc_time_stamp() { return 0.0; }

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);

    std::printf("radar_fft -- R2SDF streaming FFT, 4 frames back to back per case\n");
    std::printf("reference: double-precision DFT of the same integer samples, scaled 1/N\n");
    std::printf("full scale = 1.0 = 32768 LSB; inputs are full-scale in complex magnitude\n");

    run_size<Vfft1024>(1024, 10);
    run_size<Vfft512>(512, 9);
    run_size<Vfft256>(256, 8);
    run_size<Vfft128>(128, 7);

    std::printf("\n");
    if (g_fail == 0) {
        std::printf("ALL TESTS PASSED\n");
        return 0;
    }
    std::printf("%d TEST(S) FAILED\n", g_fail);
    return 1;
}
