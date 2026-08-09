//============================================================================
// tb_front.cpp -- Verilator testbench for the 5.8 GHz radar front-end RTL.
//
// Six independently verilated models are driven from one program:
//   radar_nco       exact match against a C++ copy of the accumulator and ROM
//   radar_dechirp   exact match against radar::fx::cmul_conj_q15
//   radar_decim4    measured frequency response, passband and alias rejection
//   radar_seq       cycle-exact timing assertions on the chirp sequencer
//   radar_window    exact match against radar::fx::round_sat
//   radar_regs      reset defaults, every field, and the one-cycle pulses
//
// The reference for every fixed-point operation is soft/include/radar/core.hpp
// itself -- not a re-implementation of it -- so the two halves of the contract
// are compared directly.
//
// Exit status is non-zero if any check fails.
//============================================================================
#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <cmath>
#include <complex>
#include <random>
#include <string>
#include <vector>

#include <verilated.h>

#include "Vradar_nco.h"
#include "Vradar_dechirp.h"
#include "Vradar_decim4.h"
#include "Vradar_seq.h"
#include "Vradar_window.h"
#include "Vradar_regs.h"

#include "radar/core.hpp"

using radar::ci16;
using radar::i16;
using radar::i64;

//----------------------------------------------------------------------------
// Result bookkeeping
//----------------------------------------------------------------------------
namespace {

struct TestResult {
    std::string name;
    bool        pass;
    std::string note;
};

std::vector<TestResult> g_results;
int                     g_fail = 0;

void record(const std::string& name, bool pass, const std::string& note = "")
{
    g_results.push_back({name, pass, note});
    if (!pass) ++g_fail;
    std::printf("    [%s] %-46s %s\n", pass ? "PASS" : "FAIL", name.c_str(), note.c_str());
}

std::string fmt(const char* f, ...)
{
    char    buf[512];
    va_list ap;
    va_start(ap, f);
    std::vsnprintf(buf, sizeof(buf), f, ap);
    va_end(ap);
    return std::string(buf);
}

void banner(const char* s)
{
    std::printf("\n  %s\n  %s\n", s, std::string(std::strlen(s), '-').c_str());
}

// One clock edge on any model that has a `clk` member.
template <class T>
void tick(T* d)
{
    d->clk = 0;
    d->eval();
    d->clk = 1;
    d->eval();
}

constexpr double kPi = 3.14159265358979323846;

} // namespace

//============================================================================
// (b) radar_nco
//============================================================================
namespace {

// Statement-for-statement copy of radar_nco.v, including the quarter-wave ROM
// and the three pipeline stages, so the comparison can demand bit equality on
// every cycle rather than only on valid cycles.
struct NcoRef {
    static const int LUT_N = 1024;

    int16_t  rom[LUT_N];
    uint32_t phase, freq;
    uint16_t a_i1, a_q1;
    bool     n_i1, n_q1, v1;
    int16_t  r_i2, r_q2;
    bool     n_i2, n_q2, v2;
    int16_t  o_i, o_q;
    bool     ov;

    NcoRef()
    {
        for (int k = 0; k < LUT_N; ++k) rom[k] = (int16_t)rom_entry(k);
        clear();
    }

    // The contract: round(32767 * sin(2*pi*(k + 0.5)/4096)).  Every entry is
    // positive, so truncating after adding a half is round-to-nearest, which
    // is exactly what the Verilog $rtoi(... + 0.5) does.
    static int rom_entry(int k)
    {
        const double ang = (2.0 * kPi) * ((k + 0.5) / 4096.0);
        int          rv  = (int)(32767.0 * std::sin(ang) + 0.5);
        if (rv > 32767) rv = 32767;
        return rv;
    }

    void clear()
    {
        phase = 0; freq = 0;
        a_i1 = 0; a_q1 = 0; n_i1 = false; n_q1 = false; v1 = false;
        r_i2 = 0; r_q2 = 0; n_i2 = false; n_q2 = false; v2 = false;
        o_i = 0; o_q = 0; ov = false;
    }

    void step_rst()
    {
        // stage 2 has no reset in the RTL: the ROM read happens every clock
        const int16_t nr_i = rom[a_i1], nr_q = rom[a_q1];
        const bool    nn_i = n_i1,      nn_q = n_q1;
        o_i = 0; o_q = 0; ov = false;
        r_i2 = nr_i; r_q2 = nr_q; n_i2 = nn_i; n_q2 = nn_q; v2 = false;
        a_i1 = 0; a_q1 = 0; n_i1 = false; n_q1 = false; v1 = false;
        phase = 0; freq = 0;
    }

    void step(bool ena, bool restart, int32_t fstart, int32_t fslope)
    {
        const uint32_t idxq  = (phase >> 20) & 0xFFFu;      // top 12 bits
        const uint32_t idxi  = (idxq + 1024u) & 0xFFFu;     // cos = sin + 90 deg
        const uint16_t foldq = (idxq & 0x400u) ? (uint16_t)((~idxq) & 0x3FFu)
                                               : (uint16_t)(idxq & 0x3FFu);
        const uint16_t foldi = (idxi & 0x400u) ? (uint16_t)((~idxi) & 0x3FFu)
                                               : (uint16_t)(idxi & 0x3FFu);
        const bool negq = (idxq & 0x800u) != 0;
        const bool negi = (idxi & 0x800u) != 0;

        o_i  = n_i2 ? (int16_t)(-r_i2) : r_i2;
        o_q  = n_q2 ? (int16_t)(-r_q2) : r_q2;
        ov   = v2;
        r_i2 = rom[a_i1]; r_q2 = rom[a_q1];
        n_i2 = n_i1;      n_q2 = n_q1;      v2 = v1;
        a_i1 = foldi;     a_q1 = foldq;
        n_i1 = negi;      n_q1 = negq;      v1 = ena;

        if (restart) {
            phase = 0;
            freq  = (uint32_t)fstart;
        } else if (ena) {
            const uint32_t p = phase + freq;     // pre-update freq
            freq  = freq + (uint32_t)fslope;
            phase = p;
        }
    }
};

void test_nco()
{
    banner("(b) radar_nco -- chirp accumulator and quarter-wave ROM");

    Vradar_nco* dut = new Vradar_nco;
    NcoRef      ref;

    dut->rst = 1; dut->ena = 0; dut->restart = 0;
    dut->freq_start = 0; dut->freq_slope = 0;
    for (int i = 0; i < 4; ++i) { tick(dut); ref.step_rst(); }
    dut->rst = 0;

    struct Scenario {
        const char* name;
        int32_t     fstart, fslope;
        int         mode;     // 0 continuous, 1 gapped ena, 2 mid-run restart
    };
    const Scenario scen[] = {
        {"radar chirp, -25 MHz start, 1e12 Hz/s", -1747626667,  1137778, 0},
        {"quarter-rate carrier, no slope",        0x40000000,         0, 0},
        {"large negative slope, wraps hard",       1747626667, -3456789, 0},
        {"gapped enable",                         -1747626667,  1137778, 1},
        {"restart in mid sweep",                  -1747626667,  1137778, 2},
    };

    std::mt19937 rng(20260810u);
    bool         all_ok    = true;
    long         mismatch  = 0;
    long         cycles    = 0;
    long         valid_out = 0;

    for (const Scenario& s : scen) {
        dut->freq_start = (uint32_t)s.fstart;
        dut->freq_slope = (uint32_t)s.fslope;

        dut->restart = 1; dut->ena = 0;
        tick(dut);
        ref.step(false, true, s.fstart, s.fslope);
        dut->restart = 0;

        for (int n = 0; n < 4096; ++n) {
            bool ena = true, restart = false;
            if (s.mode == 1) ena     = (rng() & 3u) != 0;      // 75% duty
            if (s.mode == 2) restart = (n == 1500) || (n == 2900);

            dut->ena = ena ? 1 : 0;
            dut->restart = restart ? 1 : 0;
            tick(dut);
            ref.step(ena, restart, s.fstart, s.fslope);
            ++cycles;
            if (ref.ov) ++valid_out;

            if ((int16_t)dut->out_i != ref.o_i ||
                (int16_t)dut->out_q != ref.o_q ||
                (dut->out_valid != 0) != ref.ov) {
                if (mismatch < 5)
                    std::printf("      mismatch %s n=%d  rtl=(%d,%d,v%d) ref=(%d,%d,v%d)\n",
                                s.name, n, (int)(int16_t)dut->out_i,
                                (int)(int16_t)dut->out_q, (int)dut->out_valid,
                                (int)ref.o_i, (int)ref.o_q, (int)ref.ov);
                ++mismatch;
                all_ok = false;
            }
        }
        dut->restart = 0;
    }

    record("nco bit-exact vs C++ model",
           all_ok,
           fmt("%ld cycles, %ld valid samples, %ld mismatches", cycles, valid_out, mismatch));

    // --- the quadrant fold against the mathematical definition -------------
    // Step the phase by exactly one table entry per sample and compare all
    // 4096 points of the full circle with round(32767*sin(2*pi*(m+0.5)/4096))
    // evaluated directly.  This checks the fold and the half-sample offset
    // against the formula rather than against another copy of the fold.
    dut->rst = 1; dut->ena = 0; dut->restart = 0;
    for (int i = 0; i < 4; ++i) tick(dut);
    dut->rst = 0;
    dut->freq_start = (uint32_t)(1u << 20);      // one ROM step per sample
    dut->freq_slope = 0;
    dut->restart = 1; dut->ena = 0;
    tick(dut);
    dut->restart = 0; dut->ena = 1;

    auto direct = [](int m) -> int {
        const double ang = (2.0 * kPi) * (((m & 4095) + 0.5) / 4096.0);
        return (int)std::llround(32767.0 * std::sin(ang));
    };

    std::vector<std::pair<int, int>> circle;
    for (int t = 0; t < 4096 + 8 && (int)circle.size() < 4096; ++t) {
        tick(dut);
        if (dut->out_valid)
            circle.push_back({(int)(int16_t)dut->out_i, (int)(int16_t)dut->out_q});
    }
    long fold_bad = 0;
    for (int s = 0; s < (int)circle.size(); ++s) {
        if (circle[s].second != direct(s) || circle[s].first != direct(s + 1024)) {
            if (fold_bad < 4)
                std::printf("      fold mismatch m=%d rtl=(%d,%d) direct=(%d,%d)\n",
                            s, circle[s].first, circle[s].second,
                            direct(s + 1024), direct(s));
            ++fold_bad;
        }
    }
    record("quadrant fold matches the sine formula over all 4096 points",
           fold_bad == 0 && circle.size() == 4096,
           fmt("rom[0]=%d rom[1023]=%d -- the +0.5 offset keeps every entry "
               "off zero and inside +/-32767, %ld mismatches",
               NcoRef::rom_entry(0), NcoRef::rom_entry(1023), fold_bad));

    // --- latency: restart to the first sample of the sweep -----------------
    dut->rst = 1; dut->ena = 0; dut->restart = 0;
    for (int i = 0; i < 4; ++i) tick(dut);
    dut->rst = 0;
    dut->freq_start = (uint32_t)1137778;
    dut->freq_slope = (uint32_t)1137778;
    dut->restart = 1; dut->ena = 0;
    tick(dut);                        // this edge ends "the restart cycle"
    dut->restart = 0; dut->ena = 1;
    int  lat   = 0;
    bool found = false;
    for (int n = 0; n < 16 && !found; ++n) {
        tick(dut);
        if (dut->out_valid) { lat = n + 1; found = true; }
    }
    const bool lat_ok = found && lat == 4 &&
                        (int16_t)dut->out_i == 32767 && (int16_t)dut->out_q == 25;
    record("nco latency and sweep origin",
           lat_ok,
           fmt("first sample %d clocks after restart, value (%d,%d) = phase 0",
               lat, (int)(int16_t)dut->out_i, (int)(int16_t)dut->out_q));

    dut->final();
    delete dut;
}

} // namespace

//============================================================================
// (c) radar_dechirp
//============================================================================
namespace {

void test_dechirp()
{
    banner("(c) radar_dechirp -- conjugate multiply vs radar::fx::cmul_conj_q15");

    struct Vec { int16_t ai, aq, bi, bq; int sh; };
    std::vector<Vec> vecs;

    // every corner of the s16 range against every other, at the two extreme
    // shifts -- this is where rounding and saturation interact
    const int16_t corners[] = {-32768, -32767, -16384, -1, 0, 1, 16384, 32766, 32767};
    for (int sh : {0, 1, 15}) {
        for (int16_t ai : corners)
            for (int16_t aq : corners)
                for (int16_t bi : {(int16_t)-32768, (int16_t)-1, (int16_t)0, (int16_t)32767})
                    for (int16_t bq : {(int16_t)-32768, (int16_t)-1, (int16_t)0, (int16_t)32767})
                        vecs.push_back({ai, aq, bi, bq, sh});
    }

    // 10000 random pairs with a random shift each
    std::mt19937                            rng(0xC0FFEEu);
    std::uniform_int_distribution<int>      d16(-32768, 32767);
    std::uniform_int_distribution<int>      dsh(0, 15);
    for (int n = 0; n < 10000; ++n)
        vecs.push_back({(int16_t)d16(rng), (int16_t)d16(rng),
                        (int16_t)d16(rng), (int16_t)d16(rng), dsh(rng)});

    Vradar_dechirp* dut = new Vradar_dechirp;
    dut->rst = 1; dut->in_valid = 0; dut->shift = 0;
    dut->in_i = 0; dut->in_q = 0; dut->ref_i = 0; dut->ref_q = 0;
    for (int i = 0; i < 4; ++i) tick(dut);
    dut->rst = 0;

    const int    LAT = 4;
    const size_t N   = vecs.size();
    long         bad = 0, checked = 0;
    long         sat_hits = 0;
    int          measured_lat = -1;

    for (size_t n = 0; n < N + LAT + 2; ++n) {
        if (n < N) {
            dut->in_valid = 1;
            dut->in_i  = (uint16_t)vecs[n].ai;
            dut->in_q  = (uint16_t)vecs[n].aq;
            dut->ref_i = (uint16_t)vecs[n].bi;
            dut->ref_q = (uint16_t)vecs[n].bq;
            dut->shift = (uint8_t)vecs[n].sh;
        } else {
            dut->in_valid = 0;
        }
        tick(dut);

        // after tick n the model shows clock cycle n+1, so an output visible
        // now belongs to the input presented on cycle n+1-LAT
        if (measured_lat < 0 && dut->out_valid) measured_lat = (int)n + 1;

        if (n + 1 >= (size_t)LAT) {
            const size_t k = n + 1 - LAT;
            if (k < N) {
                const Vec& v = vecs[k];
                const ci16 exp = radar::fx::cmul_conj_q15(ci16(v.ai, v.aq),
                                                          ci16(v.bi, v.bq),
                                                          15 + v.sh);
                const int16_t gi = (int16_t)dut->out_i;
                const int16_t gq = (int16_t)dut->out_q;
                ++checked;
                if (exp.re == 32767 || exp.re == -32768 ||
                    exp.im == 32767 || exp.im == -32768) ++sat_hits;
                if (gi != exp.re || gq != exp.im || !dut->out_valid) {
                    if (bad < 5)
                        std::printf("      mismatch k=%zu in=(%d,%d) ref=(%d,%d) sh=%d"
                                    "  rtl=(%d,%d) exp=(%d,%d)\n",
                                    k, v.ai, v.aq, v.bi, v.bq, v.sh,
                                    gi, gq, (int)exp.re, (int)exp.im);
                    ++bad;
                }
            }
        }
    }

    record("dechirp bit-exact vs core.hpp",
           bad == 0,
           fmt("%ld vectors (%zu corner + 10000 random), %ld saturating, %ld mismatches",
               checked, N - 10000, sat_hits, bad));
    record("dechirp latency", measured_lat == LAT,
           fmt("%d clocks from in_valid to out_valid", measured_lat));

    dut->final();
    delete dut;
}

} // namespace

//============================================================================
// (d) radar_decim4
//============================================================================
namespace {

constexpr double kFs    = 61.44e6;
constexpr double kFsOut = 15.36e6;

struct DecimMeas {
    double f_in_mhz;
    double f_out_mhz;
    double gain_db;
};

// Drive a complex tone through the decimator and measure the coherent gain at
// the frequency it lands on after decimation.  A Hann window keeps leakage
// from the rounding-noise floor and from DC far below the numbers of interest;
// the measurement floor with these lengths is about -134 dB.
DecimMeas measure_tone(Vradar_decim4* dut, double f_hz, double amp,
                       int n_discard, int n_meas)
{
    dut->rst = 1; dut->flush = 0; dut->in_valid = 0; dut->in_i = 0; dut->in_q = 0;
    for (int i = 0; i < 4; ++i) tick(dut);
    dut->rst = 0;
    dut->flush = 1;
    tick(dut);
    dut->flush = 0;

    const double f_out = f_hz - std::round(f_hz / kFsOut) * kFsOut;
    const double wstep = 2.0 * kPi * f_out / kFsOut;

    std::complex<double> acc(0.0, 0.0);
    double               wsum = 0.0;

    long n_in = 0, n_out = 0;
    while (n_out < n_discard + n_meas) {
        const double th = 2.0 * kPi * f_hz * (double)n_in / kFs;
        dut->in_valid = 1;
        dut->in_i = (uint16_t)(int16_t)std::lround(amp * std::cos(th));
        dut->in_q = (uint16_t)(int16_t)std::lround(amp * std::sin(th));
        tick(dut);
        ++n_in;
        if (dut->out_valid) {
            const long m = n_out - n_discard;
            if (m >= 0) {
                const double w  = 0.5 - 0.5 * std::cos(2.0 * kPi * (double)m / (double)n_meas);
                const double ph = -wstep * (double)m;
                const std::complex<double> y((double)(int16_t)dut->out_i,
                                             (double)(int16_t)dut->out_q);
                acc  += w * y * std::complex<double>(std::cos(ph), std::sin(ph));
                wsum += w;
            }
            ++n_out;
        }
    }
    dut->in_valid = 0;

    const double gain = std::abs(acc) / (amp * wsum);
    return {f_hz / 1e6, f_out / 1e6, 20.0 * std::log10(gain + 1e-300)};
}

void test_decim4()
{
    banner("(d) radar_decim4 -- 61.44 -> 15.36 MSps, passband and alias rejection");

    Vradar_decim4* dut = new Vradar_decim4;

    // --- latency and decimation ratio --------------------------------------
    dut->rst = 1; dut->flush = 0; dut->in_valid = 0; dut->in_i = 0; dut->in_q = 0;
    for (int i = 0; i < 4; ++i) tick(dut);
    dut->rst = 0; dut->flush = 1; tick(dut); dut->flush = 0;

    int  first_valid = -1;
    int  n_valid     = 0;
    const int NCLK   = 400;
    for (int n = 0; n < NCLK; ++n) {
        dut->in_valid = 1;
        dut->in_i = (uint16_t)(int16_t)((n == 0) ? 20000 : 0);
        dut->in_q = 0;
        tick(dut);
        if (dut->out_valid) {
            if (first_valid < 0) first_valid = n;
            ++n_valid;
        }
    }
    record("decim4 latency and 4:1 ratio",
           first_valid == 11 && n_valid == (NCLK - first_valid + 3) / 4,
           fmt("first out_valid at clock %d (8 clocks after the completing "
               "input), %d outputs in %d clocks", first_valid, n_valid, NCLK));

    // --- DC gain must be exactly 1.0 ---------------------------------------
    dut->rst = 1; tick(dut); dut->rst = 0;
    dut->flush = 1; tick(dut); dut->flush = 0;
    const int16_t dc_i = 12345, dc_q = -23456;
    int16_t got_i = 0, got_q = 0;
    int     got_n = 0;
    for (int n = 0; n < 1200; ++n) {
        dut->in_valid = 1;
        dut->in_i = (uint16_t)dc_i;
        dut->in_q = (uint16_t)dc_q;
        tick(dut);
        if (dut->out_valid && n > 800) {
            got_i = (int16_t)dut->out_i;
            got_q = (int16_t)dut->out_q;
            ++got_n;
        }
    }
    record("decim4 DC gain is exactly 1.0",
           got_n > 0 && got_i == dc_i && got_q == dc_q,
           fmt("in (%d,%d) -> out (%d,%d), coefficients sum to exactly 2^17",
               dc_i, dc_q, got_i, got_q));

    // --- frequency response -------------------------------------------------
    const double amp       = 30000.0;
    const int    n_discard = 1000;
    const int    n_meas    = 4096;

    const double tones[] = {1.0,  3.0,  5.0,  7.0,  7.68, 8.5,  10.0, 12.0,
                            13.6, 14.0, 15.0, 16.0, 16.36, 17.0, 20.0, 22.36,
                            24.0, 28.0, 29.0, 30.0};

    std::vector<DecimMeas> ms;
    for (double f : tones) ms.push_back(measure_tone(dut, f * 1e6, amp, n_discard, n_meas));

    std::printf("\n      input MHz   lands at MHz   composite gain dB\n");
    for (const DecimMeas& m : ms)
        std::printf("      %9.3f   %12.3f   %+17.2f\n", m.f_in_mhz, m.f_out_mhz, m.gain_db);
    std::printf("\n");

    auto at = [&](double f) -> const DecimMeas& {
        for (const DecimMeas& m : ms) if (std::fabs(m.f_in_mhz - f) < 1e-6) return m;
        return ms[0];
    };

    // 1 MHz must pass essentially untouched
    const DecimMeas& p1 = at(1.0);
    record("1 MHz passband loss < 0.5 dB", p1.gain_db > -0.5,
           fmt("measured %+.4f dB", p1.gain_db));

    // The tone that folds exactly on top of the 1 MHz signal after decimation
    // by 4 is 1 + 15.36 = 16.36 MHz.  That is what "aliases after decimation"
    // means for a 15.36 MSps complex output, and it is the number the anti-
    // alias filter exists to make small.
    const DecimMeas& a1 = at(16.36);
    record("16.36 MHz alias (folds onto 1 MHz) > 50 dB down", a1.gain_db < -50.0,
           fmt("measured %.1f dB", a1.gain_db));

    // Everything that folds into the wanted band -- 1.9 MHz of beat frequency
    // is 285 m of range, past anything the link budget reaches.
    double worst    = -1e9;
    double worst_f  = 0.0;
    int    n_inband = 0;
    for (const DecimMeas& m : ms) {
        if (std::fabs(m.f_in_mhz) < 7.68) continue;           // not an alias
        if (std::fabs(m.f_out_mhz) > 1.9) continue;           // misses the band
        ++n_inband;
        if (m.gain_db > worst) { worst = m.gain_db; worst_f = m.f_in_mhz; }
    }
    record("every fold into the +/-1.9 MHz wanted band > 50 dB down",
           n_inband >= 6 && worst < -50.0,
           fmt("worst of %d is %.1f dB at %.2f MHz", n_inband, worst, worst_f));

    // 7 MHz is BELOW the 7.68 MHz output Nyquist, so it does not alias at all
    // and a halfband pair neither can nor should remove it.  Reported, not
    // asserted against a rejection threshold.
    const DecimMeas& p7 = at(7.0);
    record("7 MHz reported (below output Nyquist, does not fold)",
           p7.gain_db > -8.0 && p7.gain_db < 0.5,
           fmt("measured %+.2f dB, lands at %+.3f MHz -- inside the output band",
               p7.gain_db, p7.f_out_mhz));

    dut->final();
    delete dut;
}

} // namespace

//============================================================================
// (e) radar_seq
//============================================================================
namespace {

struct SeqSample {
    uint8_t  gate, restart, tx0, tx1, inv, fstart, fend, run, sel, nco;
    uint16_t sidx, cidx;
};

// Run one configuration and check every timing rule.
bool check_seq(const char* label, int mode, int n_chirp, int t_sweep, int t_pri,
               bool tx_enable, std::string& note)
{
    Vradar_seq* dut = new Vradar_seq;
    dut->rst = 1; dut->enable = 0; dut->mimo_mode = (uint8_t)mode;
    dut->tx_enable = tx_enable ? 1 : 0;
    dut->t_sweep = (uint16_t)t_sweep;
    dut->t_pri   = (uint16_t)t_pri;
    dut->n_chirp = (uint16_t)n_chirp;
    for (int i = 0; i < 4; ++i) tick(dut);
    dut->rst = 0;

    const int n_total = (mode == 0) ? n_chirp * 2 : n_chirp;
    const int n_cycle = t_pri * n_total * 2 + 32;      // two whole intervals

    std::vector<SeqSample> tr;
    tr.reserve(n_cycle);

    dut->enable = 1;
    for (int n = 0; n < n_cycle; ++n) {
        // drop enable inside the second interval: the sequencer must still
        // finish it, then stop
        if (n == t_pri * n_total + t_pri / 2) dut->enable = 0;
        tick(dut);
        SeqSample s;
        s.gate    = dut->adc_gate;
        s.restart = dut->nco_restart;
        s.nco     = dut->nco_ena;
        s.tx0     = dut->tx0_ena;
        s.tx1     = dut->tx1_ena;
        s.inv     = dut->tx_invert;
        s.fstart  = dut->frame_start;
        s.fend    = dut->frame_end;
        s.run     = dut->running;
        s.sel     = dut->tx_sel;
        s.sidx    = dut->sample_idx;
        s.cidx    = dut->chirp_idx;
        tr.push_back(s);
    }
    dut->final();
    delete dut;

    std::vector<int> starts, fstarts, fends;
    for (int n = 0; n < n_cycle; ++n) {
        if (tr[n].restart) starts.push_back(n);
        if (tr[n].fstart)  fstarts.push_back(n);
        if (tr[n].fend)    fends.push_back(n);
    }

    std::vector<std::string> errs;
    auto err = [&](const std::string& e) { if (errs.size() < 6) errs.push_back(e); };

    if ((int)starts.size() != 2 * n_total)
        err(fmt("%zu chirp starts, expected %d", starts.size(), 2 * n_total));

    // jitter: every start exactly t_pri after the previous one
    int max_jit = 0;
    for (size_t k = 1; k < starts.size(); ++k) {
        const int d = starts[k] - starts[k - 1];
        if (d - t_pri != 0) max_jit = std::max(max_jit, std::abs(d - t_pri));
    }
    if (max_jit != 0) err(fmt("chirp-start jitter %d clocks", max_jit));

    // adc_gate exactly t_sweep cycles per chirp, starting at the chirp start
    for (size_t k = 0; k < starts.size(); ++k) {
        const int s0 = starts[k];
        const int s1 = (k + 1 < starts.size()) ? starts[k + 1] : std::min(s0 + t_pri, n_cycle);
        int hi = 0;
        for (int n = s0; n < s1; ++n) {
            if (tr[n].gate) ++hi;
            const bool want = (n - s0) < t_sweep;
            if ((tr[n].gate != 0) != want)
                err(fmt("chirp %zu clock %d: adc_gate %d, expected %d",
                        k, n - s0, (int)tr[n].gate, (int)want));
            if (tr[n].nco != tr[n].gate)
                err(fmt("chirp %zu clock %d: nco_ena does not track adc_gate", k, n - s0));
            const uint16_t want_idx = want ? (uint16_t)(n - s0) : 0;
            if (tr[n].sidx != want_idx)
                err(fmt("chirp %zu clock %d: sample_idx %u, expected %u",
                        k, n - s0, tr[n].sidx, want_idx));
            const uint16_t want_c = (uint16_t)(k % n_total);
            if (tr[n].cidx != want_c)
                err(fmt("chirp %zu clock %d: chirp_idx %u, expected %u",
                        k, n - s0, tr[n].cidx, want_c));
        }
        if (hi != t_sweep && s1 - s0 >= t_sweep)
            err(fmt("chirp %zu: adc_gate high %d clocks, expected %d", k, hi, t_sweep));

        // transmitter pattern for this chirp
        const int  ci  = (int)(k % n_total);
        const bool odd = (ci & 1) != 0;
        for (int n = s0; n < s1; ++n) {
            const bool act  = tr[n].gate && tx_enable;
            bool want0 = false, want1 = false, wantv = false, wants = false;
            switch (mode) {
                case 0: want0 = act && !odd; want1 = act && odd;
                        wantv = false;       wants = odd;  break;
                case 1: want0 = act;         want1 = act;
                        wantv = tr[n].run && odd; wants = false; break;
                case 2: want0 = act;         want1 = false;
                        wantv = false;       wants = false; break;
                default: want0 = false;      want1 = act;
                        wantv = false;       wants = true;  break;
            }
            if ((tr[n].tx0 != 0) != want0 || (tr[n].tx1 != 0) != want1) {
                err(fmt("chirp %zu clock %d: tx0/tx1 = %d/%d, expected %d/%d",
                        k, n - s0, (int)tr[n].tx0, (int)tr[n].tx1,
                        (int)want0, (int)want1));
                n = s1;    // one report per chirp is enough
            } else if ((tr[n].inv != 0) != wantv) {
                err(fmt("chirp %zu clock %d: tx_invert %d, expected %d",
                        k, n - s0, (int)tr[n].inv, (int)wantv));
                n = s1;
            } else if ((tr[n].sel != 0) != wants) {
                err(fmt("chirp %zu clock %d: tx_sel %d, expected %d",
                        k, n - s0, (int)tr[n].sel, (int)wants));
                n = s1;
            }
        }
    }

    // interval boundaries
    if (fstarts.size() != 2)
        err(fmt("%zu frame_start pulses, expected 2", fstarts.size()));
    else if (fstarts[1] - fstarts[0] != t_pri * n_total)
        err(fmt("interval spacing %d, expected %d", fstarts[1] - fstarts[0], t_pri * n_total));
    if (fends.size() != 2)
        err(fmt("%zu frame_end pulses, expected 2", fends.size()));
    else if (fends[0] - fstarts[0] != t_pri * n_total - 1)
        err(fmt("frame_end at %d after frame_start, expected %d",
                fends[0] - fstarts[0], t_pri * n_total - 1));

    // enable dropped mid-interval: the interval still finishes, then it stops
    if (!fends.empty()) {
        const int last_end = fends.back();
        for (int n = last_end + 1; n < n_cycle; ++n)
            if (tr[n].run) { err(fmt("still running %d clocks after the last "
                                     "frame_end", n - last_end)); break; }
    }

    if (errs.empty()) {
        note = fmt("%s: %d chirps x %d clocks, gate %d, zero jitter over %zu starts",
                   label, n_total, t_pri, t_sweep, starts.size());
        return true;
    }
    note = label;
    for (const std::string& e : errs) note += "  | " + e;
    return false;
}

void test_seq()
{
    banner("(e) radar_seq -- TDM/DDM chirp timing");

    std::string note;
    bool ok;

    ok = check_seq("TDM, 4 per TX = 8 chirps", 0, 4, 40, 64, true, note);
    record("TDM 8 chirps, timing and TX alternation", ok, note);

    ok = check_seq("DDM, 8 chirps", 1, 8, 40, 64, true, note);
    record("DDM 8 chirps, both TX on, invert on odd", ok, note);

    ok = check_seq("TX0 only, 8 chirps", 2, 8, 40, 64, true, note);
    record("TX0-only mode", ok, note);

    ok = check_seq("TX1 only, 8 chirps", 3, 8, 40, 64, true, note);
    record("TX1-only mode", ok, note);

    ok = check_seq("TDM, tx_enable low", 0, 4, 40, 64, false, note);
    record("tx_enable low keeps both transmitters off", ok, note);

    // genuinely parametric: the contract default of 128 per transmitter, the
    // old default of 256, and the real 3072/3840 chirp geometry
    ok = check_seq("TDM, 128 per TX = 256 chirps", 0, 128, 40, 64, true, note);
    record("n_chirp = 128 (contract default)", ok, note);

    ok = check_seq("TDM, 256 per TX = 512 chirps", 0, 256, 40, 64, true, note);
    record("n_chirp = 256", ok, note);

    ok = check_seq("DDM, 128 chirps", 1, 128, 40, 64, true, note);
    record("n_chirp = 128 in DDM", ok, note);

    ok = check_seq("real geometry, 3072/3840, 6 per TX", 0, 6, 3072, 3840, true, note);
    record("50 us sweep on a 62.5 us PRI", ok, note);
}

} // namespace

//============================================================================
// (f) radar_window
//============================================================================
namespace {

void test_window()
{
    banner("(f) radar_window -- table lookup, rounding and zero padding");

    const int DEPTH = 1024;
    const int NSTREAM = 1200;              // deliberately past the table depth

    std::mt19937                       rng(0x5EEDu);
    std::uniform_int_distribution<int> d16(-32768, 32767);

    std::vector<int16_t> tbl(DEPTH);
    for (int i = 0; i < DEPTH; ++i) {
        double w = 0.5 - 0.5 * std::cos(2.0 * kPi * i / (DEPTH - 1));   // Hann
        tbl[i] = (int16_t)radar::fx::sat((i64)std::llround(w * 32767.0), 16);
    }
    // and some deliberate extremes so the saturation path is exercised
    tbl[0] = 0; tbl[1] = 1; tbl[2] = -1; tbl[3] = 32767; tbl[4] = -32768;
    tbl[5] = -32768; tbl[767] = 32767; tbl[768] = 0; tbl[1023] = -32768;

    Vradar_window* dut = new Vradar_window;
    dut->rst = 1; dut->in_valid = 0; dut->coef_we = 0;
    dut->in_i = 0; dut->in_q = 0; dut->sample_idx = 0;
    dut->coef_addr = 0; dut->coef_data = 0;
    for (int i = 0; i < 4; ++i) tick(dut);
    dut->rst = 0;

    for (int i = 0; i < DEPTH; ++i) {
        dut->coef_we   = 1;
        dut->coef_addr = (uint16_t)i;
        dut->coef_data = (uint16_t)tbl[i];
        tick(dut);
    }
    // a write beyond the table must be dropped, not wrap onto entry 0
    dut->coef_we = 1; dut->coef_addr = 2000; dut->coef_data = (uint16_t)0x1234;
    tick(dut);
    dut->coef_we = 0;

    std::vector<int16_t> si(NSTREAM), sq(NSTREAM);
    for (int n = 0; n < NSTREAM; ++n) { si[n] = (int16_t)d16(rng); sq[n] = (int16_t)d16(rng); }
    // force the worst saturating case: -32768 * -32768 rounds to +32768
    si[4] = -32768; sq[4] = -32768;
    si[5] = -32768; sq[5] =  32767;
    si[1023] = -32768; sq[1023] = -32768;

    const int LAT = 3;
    long bad = 0, checked = 0, zeroed = 0, sat_hits = 0;
    int  measured_lat = -1;

    for (int n = 0; n < NSTREAM + LAT + 2; ++n) {
        if (n < NSTREAM) {
            dut->in_valid   = 1;
            dut->in_i       = (uint16_t)si[n];
            dut->in_q       = (uint16_t)sq[n];
            dut->sample_idx = (uint16_t)n;
        } else {
            dut->in_valid = 0;
        }
        tick(dut);
        if (measured_lat < 0 && dut->out_valid) measured_lat = n + 1;

        const int k = n + 1 - LAT;
        if (k >= 0 && k < NSTREAM) {
            int16_t ei, eq;
            if (k < DEPTH) {
                ei = (int16_t)radar::fx::round_sat((i64)si[k] * (i64)tbl[k], 15, 16);
                eq = (int16_t)radar::fx::round_sat((i64)sq[k] * (i64)tbl[k], 15, 16);
                if (ei == 32767 || ei == -32768 || eq == 32767 || eq == -32768) ++sat_hits;
            } else {
                ei = 0; eq = 0;
                ++zeroed;
            }
            ++checked;
            if ((int16_t)dut->out_i != ei || (int16_t)dut->out_q != eq || !dut->out_valid) {
                if (bad < 5)
                    std::printf("      mismatch idx=%d in=(%d,%d) coef=%d "
                                "rtl=(%d,%d) exp=(%d,%d)\n",
                                k, si[k], sq[k], k < DEPTH ? tbl[k] : 0,
                                (int)(int16_t)dut->out_i, (int)(int16_t)dut->out_q, ei, eq);
                ++bad;
            }
        }
    }

    record("window product bit-exact vs core.hpp", bad == 0,
           fmt("%ld samples checked, %ld saturating, %ld mismatches",
               checked, sat_hits, bad));
    record("samples past the table depth come out zero", zeroed == NSTREAM - DEPTH && bad == 0,
           fmt("indices %d..%d all zero (%ld samples)", DEPTH, NSTREAM - 1, zeroed));
    record("window latency", measured_lat == LAT,
           fmt("%d clocks from in_valid to out_valid", measured_lat));

    dut->final();
    delete dut;
}

} // namespace

//============================================================================
// (g) radar_regs
//============================================================================
namespace {

void wr(Vradar_regs* d, uint8_t addr, uint32_t data)
{
    d->set_stb = 1; d->set_addr = addr; d->set_data = data;
    tick(d);
    d->set_stb = 0; d->set_addr = 0; d->set_data = 0;
}

void test_regs()
{
    banner("(g) radar_regs -- settings bus, defaults and pulses");

    Vradar_regs* dut = new Vradar_regs;
    dut->rst = 1; dut->set_stb = 0; dut->set_addr = 0; dut->set_data = 0;
    for (int i = 0; i < 4; ++i) tick(dut);
    dut->rst = 0;
    tick(dut);

    std::vector<std::string> bad;
    auto chk = [&](const char* n, long got, long want) {
        if (got != want) bad.push_back(fmt("%s = %ld, expected %ld", n, got, want));
    };

    chk("ctrl_enable",       dut->ctrl_enable, 0);
    chk("ctrl_tx_enable",    dut->ctrl_tx_enable, 0);
    chk("ctrl_map_enable",   dut->ctrl_map_enable, 1);
    chk("ctrl_hits_enable",  dut->ctrl_hits_enable, 1);
    chk("ctrl_mimo_mode",    dut->ctrl_mimo_mode, 0);
    chk("ctrl_loopback",     dut->ctrl_loopback, 0);
    chk("ctrl_frame_limit",  dut->ctrl_frame_limit, 0);
    chk("freq_start",        (int32_t)dut->freq_start, -1747626667L);
    chk("freq_slope",        (int32_t)dut->freq_slope, 1137778);
    chk("t_sweep",           dut->t_sweep, 3072);
    chk("t_pri",             dut->t_pri, 3840);
    chk("n_chirp",           dut->n_chirp, 128);
    chk("tx_gain",           dut->tx_gain, 32767);
    chk("dechirp_sh",        dut->dechirp_sh, 0);
    chk("fft_scale_r",       dut->fft_scale_r, 0x55555);
    chk("fft_scale_d",       dut->fft_scale_d, 0x5555);
    chk("cfar_guard_range",  dut->cfar_guard_range, 2);
    chk("cfar_guard_dopp",   dut->cfar_guard_dopp, 2);
    chk("cfar_train_range",  dut->cfar_train_range, 8);
    chk("cfar_train_dopp",   dut->cfar_train_dopp, 8);
    chk("cfar_kind",         dut->cfar_kind, 0);
    chk("cfar_alpha",        dut->cfar_alpha, 0x000A0000);
    chk("range_zero",        dut->range_zero, 0);
    chk("map_decim_r",       dut->map_decim_r, 1);
    chk("map_decim_d",       dut->map_decim_d, 1);
    chk("max_hits",          dut->max_hits, 64);
    chk("zero_dopp",         dut->zero_dopp, 2);
    chk("geom_n_range_log2", dut->geom_n_range_log2, 8);
    chk("geom_n_chirp_log2", dut->geom_n_chirp_log2, 8);
    chk("test_tone",         (int32_t)dut->test_tone, 69905067);
    chk("soft_reset",        dut->ctrl_soft_reset, 0);
    chk("win_we",            dut->win_we, 0);
    chk("version_stb",       dut->version_stb, 0);

    record("reset defaults match radar_pkg.svh", bad.empty(),
           bad.empty() ? "34 fields, surveillance point, transmitter off"
                       : bad[0]);
    bad.clear();

    // --- write every register ----------------------------------------------
    wr(dut, 0, 0xBEEF0000u | 0x00FDu);   // ctrl: enable, mimo=3, tx, map, hits, loopback
    chk("ctrl_enable",      dut->ctrl_enable, 1);
    chk("ctrl_mimo_mode",   dut->ctrl_mimo_mode, 3);
    chk("ctrl_tx_enable",   dut->ctrl_tx_enable, 1);
    chk("ctrl_map_enable",  dut->ctrl_map_enable, 1);
    chk("ctrl_hits_enable", dut->ctrl_hits_enable, 1);
    chk("ctrl_loopback",    dut->ctrl_loopback, 1);
    chk("ctrl_frame_limit", dut->ctrl_frame_limit, 0xBEEF);
    chk("soft_reset pulse", dut->ctrl_soft_reset, 1);
    tick(dut);
    chk("soft_reset cleared next clock", dut->ctrl_soft_reset, 0);
    chk("ctrl_enable held",              dut->ctrl_enable, 1);

    wr(dut, 1, 0x12345678u); chk("freq_start", (int32_t)dut->freq_start, 0x12345678);
    wr(dut, 2, 0xFFFF0001u); chk("freq_slope", (int32_t)dut->freq_slope, -65535);
    wr(dut, 3, 0x0000ABCDu); chk("t_sweep",    dut->t_sweep, 0xABCD);
    wr(dut, 4, 0x00001234u); chk("t_pri",      dut->t_pri,   0x1234);
    wr(dut, 5, 0x00000100u); chk("n_chirp",    dut->n_chirp, 256);
    wr(dut, 6, 0x00004000u); chk("tx_gain",    dut->tx_gain, 0x4000);
    wr(dut, 7, 0x0000000Du); chk("dechirp_sh", dut->dechirp_sh, 13);
    wr(dut, 8, 0x000FFFFFu); chk("fft_scale_r",dut->fft_scale_r, 0xFFFFF);
    wr(dut, 9, 0x0000AAAAu); chk("fft_scale_d",dut->fft_scale_d, 0xAAAA);

    wr(dut, 10, 0x000002FFu); chk("win_addr", dut->win_addr, 0x2FF);
    chk("win_we quiet on address write", dut->win_we, 0);
    wr(dut, 11, 0xDEADBEEFu);
    chk("win_we pulse", dut->win_we, 1);
    chk("win_data",     dut->win_data, 0xDEADBEEFL);
    chk("win_addr held",dut->win_addr, 0x2FF);
    tick(dut);
    chk("win_we cleared next clock", dut->win_we, 0);

    wr(dut, 12, 0x0003C9A5u);
    chk("cfar_guard_range", dut->cfar_guard_range, 5);
    chk("cfar_guard_dopp",  dut->cfar_guard_dopp, 10);
    chk("cfar_train_range", dut->cfar_train_range, 9);
    chk("cfar_train_dopp",  dut->cfar_train_dopp, 12);
    chk("cfar_kind",        dut->cfar_kind, 3);

    wr(dut, 13, 0x00058000u); chk("cfar_alpha", dut->cfar_alpha, 0x58000);
    wr(dut, 14, 0x0000002Au); chk("range_zero", dut->range_zero, 42);
    wr(dut, 15, 0x00000403u); chk("map_decim_r", dut->map_decim_r, 3);
    chk("map_decim_d", dut->map_decim_d, 4);
    wr(dut, 16, 0x00000080u); chk("max_hits",  dut->max_hits, 128);
    wr(dut, 17, 0x00000007u); chk("zero_dopp", dut->zero_dopp, 7);
    wr(dut, 18, 0x00000097u);
    chk("geom_n_range_log2", dut->geom_n_range_log2, 7);
    chk("geom_n_chirp_log2", dut->geom_n_chirp_log2, 9);
    wr(dut, 19, 0x00000000u); chk("version_stb pulse", dut->version_stb, 1);
    tick(dut);
    chk("version_stb cleared next clock", dut->version_stb, 0);
    wr(dut, 20, 0xF0000001u); chk("test_tone", (int32_t)dut->test_tone, (int32_t)0xF0000001);

    // an unmapped address must change nothing
    const uint16_t before = dut->t_sweep;
    wr(dut, 200, 0xFFFFFFFFu);
    chk("unmapped write ignored", dut->t_sweep, before);
    chk("unmapped write raises no pulse", dut->ctrl_soft_reset + dut->win_we + dut->version_stb, 0);

    record("every field writable at its contract address", bad.empty(),
           bad.empty() ? "addresses 0..20 including GEOM at 18 and TEST_TONE at 20"
                       : bad[0]);
    if (!bad.empty())
        for (size_t i = 0; i < bad.size() && i < 6; ++i)
            std::printf("      %s\n", bad[i].c_str());

    dut->final();
    delete dut;
}

} // namespace

// Verilator's runtime asks the testbench what time it is.  Nothing here uses
// $time, so a constant is honest and sufficient.
double sc_time_stamp() { return 0.0; }

//============================================================================
int main(int argc, char** argv)
{
    Verilated::commandArgs(argc, argv);

    std::printf("\n================================================================\n");
    std::printf("  radar front-end RTL verification\n");
    std::printf("================================================================\n");

    test_nco();
    test_dechirp();
    test_decim4();
    test_seq();
    test_window();
    test_regs();

    std::printf("\n================================================================\n");
    std::printf("  SUMMARY\n");
    std::printf("================================================================\n");
    for (const TestResult& r : g_results)
        std::printf("  %-6s %s\n", r.pass ? "PASS" : "FAIL", r.name.c_str());
    std::printf("----------------------------------------------------------------\n");
    std::printf("  %zu checks, %d failed\n\n", g_results.size(), g_fail);

    return g_fail == 0 ? 0 : 1;
}
