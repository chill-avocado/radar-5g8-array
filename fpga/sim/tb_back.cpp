//============================================================================
// tb_back.cpp -- Verilator testbench for the radar back end
//
//   radar_corner_turn   transpose, both runtime splits, two frames, no gap
//   radar_power         5000 random groups, bit-exact against radar::fx
//   radar_cfar2d        synthetic map, detection / false-alarm / noise checks
//   radar_pack          full packet parse against radar_pkg.svh section 7
//
// Every test prints PASS or FAIL on its own line and main() returns non-zero
// if any of them failed.
//============================================================================
#include <verilated.h>

#include "Vct.h"
#include "Vpw.h"
#include "Vcf.h"
#include "Vpk.h"

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <random>
#include <string>
#include <vector>
#include <algorithm>
#include <array>

//----------------------------------------------------------------------------
// Small harness
//----------------------------------------------------------------------------
// Verilator's legacy time hook; this testbench drives the clock by hand.
double sc_time_stamp() { return 0.0; }

static int g_fail = 0;

static void result(const std::string& name, bool ok, const std::string& note = "")
{
    printf("%-46s %s%s%s\n", name.c_str(), ok ? "PASS" : "FAIL",
           note.empty() ? "" : "  ", note.c_str());
    fflush(stdout);
    if (!ok) g_fail++;
}

template <class T> static inline void edge(T* d)
{
    d->clk = 1; d->eval();
    d->clk = 0; d->eval();
}

template <class T> static void hold_reset(T* d, int n)
{
    d->rst = 1;
    for (int i = 0; i < n; i++) { d->eval(); edge(d); }
    d->rst = 0;
    d->eval();
}

//============================================================================
// 1. radar_corner_turn
//============================================================================
struct CtStats {
    bool  ok            = true;
    long  first_out     = -1;
    long  bubbles       = 0;      // clocks with no output between first and last
    long  inter_frame   = 0;      // clocks with no output at the frame join
    int   frames_done   = 0;
    bool  overflow      = false;
    long  words         = 0;
    std::string why;
};

static CtStats corner_turn_run(int nrl, int ncl)
{
    CtStats s;
    Vct* d = new Vct;

    const long NR = 1L << nrl;
    const long NC = 1L << ncl;
    const long FR = NR * NC;
    const long TOTAL = 2 * FR;

    d->n_range_log2 = nrl;
    d->n_chirp_log2 = ncl;
    d->s_valid = 0; d->s_data = 0; d->s_last = 0; d->m_ready = 1;
    hold_reset(d, 8);

    if (d->cfg_error) { s.ok = false; s.why = "cfg_error on a legal split"; }

    long in_idx = 0, out_idx = 0, cycle = 0;
    long last_out = -1;
    long frame1_last_cycle = -1;
    const long LIMIT = TOTAL * 3 + 100000;

    long tail = 0;
    while ((out_idx < TOTAL || tail < 16) && cycle < LIMIT) {
        if (out_idx >= TOTAL) tail++;
        // ---- drive the input side
        if (in_idx < TOTAL) {
            const long k = in_idx % FR;
            const long c = k / NR;
            const long r = k % NR;
            d->s_valid = 1;
            d->s_data  = (uint32_t)((r << 16) | c);
            d->s_last  = (r == NR - 1);
        } else {
            d->s_valid = 0; d->s_last = 0;
        }
        d->m_ready = 1;
        d->eval();

        const bool in_fire  = d->s_valid && d->s_ready;
        const bool out_fire = d->m_valid && d->m_ready;

        if (d->overflow) { s.overflow = true; s.ok = false; s.why = "overflow asserted"; }

        if (out_fire) {
            const long k = out_idx % FR;
            const long r = k / NC;
            const long c = k % NC;
            const uint32_t expect = (uint32_t)((r << 16) | c);
            if (d->m_data != expect && s.ok) {
                s.ok = false;
                char b[160];
                snprintf(b, sizeof b, "word %ld: got %08x want %08x",
                         out_idx, (unsigned)d->m_data, (unsigned)expect);
                s.why = b;
            }
            const bool expect_last = (c == NC - 1);
            if (((bool)d->m_last) != expect_last && s.ok) {
                s.ok = false;
                char b[160];
                snprintf(b, sizeof b, "m_last wrong at word %ld", out_idx);
                s.why = b;
            }
            if (s.first_out < 0) s.first_out = cycle;
            last_out = cycle;
            if (out_idx == FR - 1) frame1_last_cycle = cycle;
            out_idx++;
        } else if (s.first_out >= 0 && out_idx < TOTAL) {
            s.bubbles++;
            if (frame1_last_cycle >= 0 && out_idx == FR) s.inter_frame++;
        }

        if (d->frame_done) s.frames_done++;
        if (in_fire) in_idx++;

        edge(d);
        cycle++;
    }

    s.words = out_idx;
    if (out_idx != TOTAL && s.ok) { s.ok = false; s.why = "stream did not complete"; }
    (void)last_out;

    // A bad split must raise cfg_error and stop accepting data.
    d->n_range_log2 = nrl;
    d->n_chirp_log2 = (ncl == 15) ? 14 : (ncl + 1);
    d->eval();
    if (!d->cfg_error || d->s_ready) {
        s.ok = false;
        if (s.why.empty()) s.why = "cfg_error did not fire on a bad split";
    }

    delete d;
    return s;
}

static void test_corner_turn()
{
    struct { int nrl, ncl; const char* name; } cfgs[] = {
        {8, 8, "surveillance 256 range x 256 chirps"},
        {7, 9, "fine Doppler 128 range x 512 chirps"},
    };

    for (auto& c : cfgs) {
        CtStats s = corner_turn_run(c.nrl, c.ncl);
        bool ok = s.ok && !s.overflow && s.frames_done == 2 &&
                  s.inter_frame == 0 && s.bubbles == 0;
        char note[256];
        snprintf(note, sizeof note,
                 "%s: %ld words, first out at clk %ld, %ld bubbles, "
                 "%ld at the frame join, %d frame_done, overflow=%d %s",
                 c.name, s.words, s.first_out, s.bubbles, s.inter_frame,
                 s.frames_done, (int)s.overflow, s.why.c_str());
        result("corner_turn transpose + ping-pong", ok, note);
    }
}

//============================================================================
// 2. radar_power
//============================================================================
// radar::fx::power -- exact, unsigned, never rounds.
static inline uint32_t fx_power(int16_t re, int16_t im)
{
    const int64_t v = (int64_t)re * re + (int64_t)im * im;
    return (uint32_t)(uint64_t)v;
}

static uint32_t ref_group(const int16_t iq[8], int shift)
{
    uint64_t sum = 0;
    for (int c = 0; c < 4; c++) sum += fx_power(iq[2 * c], iq[2 * c + 1]);
    uint64_t v = sum;
    if (shift > 0) v = (v + (1ull << (shift - 1))) >> shift;
    return (v > 0xFFFFFFFFull) ? 0xFFFFFFFFu : (uint32_t)v;
}

static void test_power()
{
    Vpw* d = new Vpw;
    const int NGROUP = 5000;
    const int SHIFT  = 2;

    std::mt19937 rng(20260810u);
    std::uniform_int_distribution<int> dist(-32768, 32767);

    std::vector<std::array<int16_t, 8>> groups(NGROUP);
    std::vector<uint32_t> expect(NGROUP);
    for (int g = 0; g < NGROUP; g++) {
        for (int k = 0; k < 8; k++) groups[g][k] = (int16_t)dist(rng);
        // sprinkle in the extremes, where a wrong width shows up first
        if (g < 8) for (int k = 0; k < 8; k++) groups[g][k] = (g & 1) ? -32768 : 32767;
        expect[g] = ref_group(groups[g].data(), SHIFT);
    }

    d->in_valid = 0; d->in_i = 0; d->in_q = 0; d->in_ch = 0; d->in_last = 0;
    d->out_shift = SHIFT;
    hold_reset(d, 4);

    long cycle = 0, feed = 0, got = 0;
    long mark_in = -1, mark_out = -1;
    bool ok = true;
    std::string why;

    const long FEED_N = (long)NGROUP * 4;
    while (got < NGROUP && cycle < FEED_N + 1000) {
        if (feed < FEED_N) {
            const int g  = (int)(feed / 4);
            const int ch = (int)(feed % 4);
            d->in_valid = 1;
            d->in_i     = groups[g][2 * ch];
            d->in_q     = groups[g][2 * ch + 1];
            d->in_ch    = ch;
            d->in_last  = (feed == FEED_N - 1);
            if (g == 0 && ch == 3) mark_in = cycle;
        } else {
            d->in_valid = 0; d->in_last = 0;
        }
        d->eval();

        if (d->out_valid) {
            if (got == 0) mark_out = cycle;
            if (d->out_pwr != expect[got] && ok) {
                ok = false;
                char b[192];
                snprintf(b, sizeof b, "group %ld: got %08x want %08x",
                         got, (unsigned)d->out_pwr, (unsigned)expect[got]);
                why = b;
            }
            const bool want_last = (got == NGROUP - 1);
            if (((bool)d->out_last) != want_last && ok) {
                ok = false; why = "out_last misplaced";
            }
            got++;
        }
        if (feed < FEED_N) feed++;
        edge(d);
        cycle++;
    }

    if (got != NGROUP && ok) { ok = false; why = "wrong number of outputs"; }

    char note[224];
    snprintf(note, sizeof note,
             "%ld/%d groups exact, out_shift=%d, latency %ld clk after in_ch=3 %s",
             got, NGROUP, SHIFT, mark_out - mark_in, why.c_str());
    result("power 4-channel integration, bit exact", ok, note);
    delete d;
}

//============================================================================
// 3. radar_cfar2d
//============================================================================
struct CfarRun {
    int      n_hits    = 0;
    uint32_t noise     = 0;
    long     latency   = -1;
    std::vector<uint32_t> hit_key;   // (range << 16) | doppler
    std::vector<uint32_t> hit_pwr;
};

struct CfarCfg {
    int NR = 256, ND = 256;
    int gr = 2, gd = 2, tr = 4, td = 4;
    int kind = 0;
    uint32_t alpha = 0;
    int zd = 0;
    int rz = 0;
    int maxhits = 4000;
};

static CfarRun cfar_run(const CfarCfg& c, const std::vector<uint32_t>& map,
                        long probe_cell)
{
    CfarRun out;
    Vcf* d = new Vcf;

    d->n_range       = c.NR;
    d->n_doppler     = c.ND;
    d->cfg_guard_r   = c.gr;
    d->cfg_guard_d   = c.gd;
    d->cfg_train_r   = c.tr;
    d->cfg_train_d   = c.td;
    d->cfg_kind      = c.kind;
    d->cfg_alpha     = c.alpha;
    d->cfg_zero_dopp  = c.zd;
    d->cfg_range_zero = c.rz;
    d->cfg_max_hits   = c.maxhits;
    d->in_valid = 0; d->in_pwr = 0; d->in_last = 0;

    hold_reset(d, 4);
    // Let the shared divider produce both reciprocals before streaming.
    for (int i = 0; i < 400; i++) { d->eval(); edge(d); }

    const long N = (long)c.NR * c.ND;
    long cycle = 0, feed = 0, probe_in = -1;
    bool done = false;

    while (!done && cycle < N + 5000) {
        if (feed < N) {
            d->in_valid = 1;
            d->in_pwr   = map[feed];
            d->in_last  = (feed == N - 1);
            if (feed == probe_cell) probe_in = cycle;
        } else {
            d->in_valid = 0; d->in_last = 0;
        }
        d->eval();

        if (d->hit_valid) {
            const uint32_t key = ((uint32_t)d->hit_range << 16) | (uint32_t)d->hit_dopp;
            out.hit_key.push_back(key);
            out.hit_pwr.push_back(d->hit_pwr);
            if (probe_in >= 0 && out.latency < 0) {
                const int Wr = c.gr + c.tr, Wd = c.gd + c.td;
                const long pr = probe_cell / c.ND, pd = probe_cell % c.ND;
                if ((long)d->hit_range == pr && (long)d->hit_dopp == pd) {
                    // The verdict lands Wr rows and Wd columns later, plus the
                    // pipeline; report just the pipeline part.
                    out.latency = cycle - (probe_in + (long)Wr * c.ND + Wd);
                }
            }
        }
        if (d->frame_done) {
            out.n_hits = d->n_hits;
            out.noise  = d->noise_out;
            done = true;
        }
        if (feed < N) feed++;
        edge(d);
        cycle++;
    }

    delete d;
    return out;
}

// Theoretical false-alarm probability of a cell-averaging CFAR with n
// reference cells and threshold multiplier alpha, for exponential (square-law
// detected Gaussian) noise:  Pfa = (1 + alpha/n)^-n
static double pfa_ca(double alpha, double n) { return std::pow(1.0 + alpha / n, -n); }

struct CfarOutcome { bool ok; std::string note; };

static CfarOutcome cfar_case(int NR, int ND, int kind, const char* label,
                             double target_pfa, bool strict)
{
    const int gr = 2, gd = 2, tr = 4, td = 4;
    const int Wr = gr + tr, Wd = gd + td;
    const int n_ref  = (2 * Wr + 1) * (2 * Wd + 1) - (2 * gr + 1) * (2 * gd + 1);
    const int n_half = tr * (2 * Wd + 1);

    // alpha that gives the wanted Pfa for cell averaging, in Q16.16
    const double alpha_f = n_ref * (std::pow(target_pfa, -1.0 / n_ref) - 1.0);
    const uint32_t alpha_q = (uint32_t)std::llround(alpha_f * 65536.0);

    // --- synthetic map: exponential noise plus three targets at 20 dB SNR
    const double mu = 1.0e6;
    std::mt19937 rng(0xC7A5u + kind * 977u + NR * 31u);
    std::uniform_real_distribution<double> uni(1e-12, 1.0);

    std::vector<uint32_t> map((size_t)NR * ND);
    double noise_sum = 0.0;
    for (size_t k = 0; k < map.size(); k++) {
        const double v = -mu * std::log(uni(rng));
        const double c = v > 4.0e9 ? 4.0e9 : v;
        map[k] = (uint32_t)c;
        noise_sum += (double)map[k];
    }
    const double true_mean = noise_sum / (double)map.size();

    struct Tgt { int r, d; };
    const Tgt tgt[3] = {
        { NR / 4,      ND / 4      },
        { NR / 2 + 7,  ND / 2 + 11 },
        { NR - Wr - 5, ND - Wd - 9 },
    };
    const uint32_t tgt_pwr = (uint32_t)(mu * 100.0);   // 20 dB above the mean
    for (auto& t : tgt) map[(size_t)t.r * ND + t.d] = tgt_pwr;

    CfarCfg cfg;
    cfg.NR = NR; cfg.ND = ND; cfg.gr = gr; cfg.gd = gd; cfg.tr = tr; cfg.td = td;
    cfg.kind = kind; cfg.alpha = alpha_q; cfg.zd = 0; cfg.maxhits = 60000;

    const long probe = (long)tgt[0].r * ND + tgt[0].d;
    CfarRun run = cfar_run(cfg, map, probe);

    // --- did we find all three?
    int found = 0;
    for (auto& t : tgt) {
        const uint32_t key = ((uint32_t)t.r << 16) | (uint32_t)t.d;
        if (std::find(run.hit_key.begin(), run.hit_key.end(), key) != run.hit_key.end())
            found++;
    }

    const long n_tested = (long)(NR - 2 * Wr) * (long)(ND - 2 * Wd);
    const int  false_alarms = run.n_hits - found;
    const double meas_pfa = (double)false_alarms / (double)n_tested;
    const double th_pfa   = (kind == 0) ? pfa_ca(alpha_f, n_ref)
                                        : pfa_ca(alpha_f, n_half);
    const double noise_err = (double)run.noise / true_mean - 1.0;

    bool ok = (found == 3);
    std::string why;
    if (found != 3) why = "targets missed";

    if (strict) {
        const bool pfa_ok = (meas_pfa <= th_pfa * 4.0) && (meas_pfa >= th_pfa / 4.0);
        if (!pfa_ok) { ok = false; why += " false-alarm rate outside 4x"; }
        if (std::fabs(noise_err) > 0.05) { ok = false; why += " noise estimate off by >5%"; }
    }

    char note[400];
    snprintf(note, sizeof note,
             "%s %dx%d n_ref=%d n_half=%d alpha=%.3f | targets %d/3 | hits %d "
             "(false %d of %ld tested) Pfa meas %.3e vs theory %.3e (%.2fx) | "
             "noise %u vs true mean %.0f (%+.2f%%) | pipeline %ld clk %s",
             label, NR, ND, n_ref, n_half, alpha_f, found, run.n_hits,
             false_alarms, n_tested, meas_pfa, th_pfa,
             th_pfa > 0 ? meas_pfa / th_pfa : 0.0,
             (unsigned)run.noise, true_mean, 100.0 * noise_err,
             run.latency, why.c_str());

    return CfarOutcome{ ok, note };
}

// Pass-through mode reports every tested, unblanked cell: an exact check of
// the edge rule and the zero-Doppler blanking.
static void cfar_geometry_case()
{
    const int NR = 256, ND = 256;
    const int gr = 2, gd = 2, tr = 4, td = 4;
    const int Wr = gr + tr, Wd = gd + td;
    const int zd = 3;
    const int rz = 40;

    std::vector<uint32_t> map((size_t)NR * ND, 1000u);

    CfarCfg cfg;
    cfg.NR = NR; cfg.ND = ND; cfg.gr = gr; cfg.gd = gd; cfg.tr = tr; cfg.td = td;
    cfg.kind = 3; cfg.alpha = 65536; cfg.zd = zd; cfg.rz = rz;
    cfg.maxhits = 65535;

    CfarRun run = cfar_run(cfg, map, -1);

    long expect = 0;
    long min_d = 1 << 30, max_d = -1, min_r = 1 << 30;
    for (int r = Wr; r <= NR - 1 - Wr; r++)
        for (int dd = Wd; dd <= ND - 1 - Wd; dd++) {
            const bool blanked = (dd <= zd) || (dd + zd >= ND) || (r <= rz);
            if (!blanked) expect++;
        }
    for (uint32_t k : run.hit_key) {
        const long dd = k & 0xFFFF;
        const long rr = k >> 16;
        min_d = std::min(min_d, dd);
        max_d = std::max(max_d, dd);
        min_r = std::min(min_r, rr);
    }

    const bool ok = ((long)run.n_hits == expect) && (min_d > zd) &&
                    (max_d + zd < ND) && (min_r > rz);
    char note[288];
    snprintf(note, sizeof note,
             "pass-through, zero_dopp=%d range_zero=%d: %d cells reported, %ld expected, "
             "Doppler span %ld..%ld, lowest range bin %ld",
             zd, rz, run.n_hits, expect, min_d, max_d, min_r);
    result("cfar2d edge rule + zero-Doppler blanking", ok, note);
}

static void test_cfar()
{
    // CA is the one with hard numeric requirements.
    CfarOutcome ca256 = cfar_case(256, 256, 0, "CA", 1e-3, true);
    result("cfar2d CA, surveillance split", ca256.ok, ca256.note);

    CfarOutcome ca128 = cfar_case(128, 512, 0, "CA", 1e-3, true);
    result("cfar2d CA, fine-Doppler split", ca128.ok, ca128.note);

    CfarOutcome go = cfar_case(256, 256, 1, "GO", 1e-3, false);
    result("cfar2d GO, detections reported", go.ok, go.note);

    CfarOutcome so = cfar_case(256, 256, 2, "SO", 1e-3, false);
    result("cfar2d SO, detections reported", so.ok, so.note);

    cfar_geometry_case();
}

//============================================================================
// 4. radar_pack
//============================================================================
struct HitRec {
    int      r, dop;
    uint32_t pwr;
    int16_t  v[8];   // v0i v0q v1i v1q v2i v2q v3i v3q
};

static void test_pack()
{
    Vpk* d = new Vpk;

    const int NR = 256, ND = 256;
    const int DR = 4, DD = 4;
    const int NRO = (NR + DR - 1) / DR;
    const int NDO = (ND + DD - 1) / DD;

    const uint32_t FRAME_INDEX = 0xDEADBEEFu;
    const uint64_t TIMESTAMP   = 0x0123456789ABCDEFull;
    const uint32_t NOISE       = 0x00ABCDEFu;
    const uint16_t CFG_FLAGS   = 0x0038;      // TX on, mimo mode 3, bits 0/1 clear

    // A map with a lot of structure, so max-vs-sample decimation is visible:
    // one cell in every 4x4 block is much larger than the rest, and it is
    // never the cell a naive "take every fourth" decimator would pick.
    std::mt19937 rng(0x9E3779B9u);
    std::uniform_int_distribution<uint32_t> small(1u, 1000u);
    std::vector<uint32_t> map((size_t)NR * ND);
    for (int r = 0; r < NR; r++)
        for (int c = 0; c < ND; c++) map[(size_t)r * ND + c] = small(rng);
    for (int br = 0; br < NRO; br++)
        for (int bc = 0; bc < NDO; bc++) {
            const int rr = br * DR + 3;      // last row of the block
            const int cc = bc * DD + 2;      // third column of the block
            if (rr < NR && cc < ND)
                map[(size_t)rr * ND + cc] = 0x40000000u + (uint32_t)(br * 1000 + bc);
        }

    std::vector<uint32_t> expect_map((size_t)NRO * NDO, 0);
    for (int r = 0; r < NR; r++)
        for (int c = 0; c < ND; c++) {
            const size_t o = (size_t)(r / DR) * NDO + (c / DD);
            expect_map[o] = std::max(expect_map[o], map[(size_t)r * ND + c]);
        }

    const HitRec hits[7] = {
        {  7,   3, 0x11111111u, { 100, -100, 200, -200, 300, -300, 400, -400 } },
        { 40, 255, 0x22222222u, { -1, 1, -2, 2, -3, 3, -4, 4 } },
        { 99, 128, 0x33333333u, { 32767, -32768, 1, -1, 0, 0, 123, -321 } },
        {128,   1, 0x44444444u, { 5, 6, 7, 8, 9, 10, 11, 12 } },
        {200, 200, 0x55555555u, { -32768, 32767, -32768, 32767, 0, 1, 2, 3 } },
        {250,  17, 0x66666666u, { 1000, 2000, 3000, 4000, 5000, 6000, 7000, 8000 } },
        {255, 511, 0x77777777u, { -9, -8, -7, -6, -5, -4, -3, -2 } },  // 9-bit Doppler
    };
    const int NHIT = 7;

    d->cfg_map_enable  = 1;
    d->cfg_hits_enable = 1;
    d->cfg_map_decim_r = DR;
    d->cfg_map_decim_d = DD;
    d->cfg_flags       = CFG_FLAGS;
    d->frame_index     = FRAME_INDEX;
    d->timestamp       = TIMESTAMP;
    d->noise           = NOISE;
    d->cfg_n_range     = NR;
    d->cfg_n_doppler   = ND;
    d->map_valid = 0; d->map_pwr = 0;
    d->hit_valid = 0; d->hit_range = 0; d->hit_dopp = 0; d->hit_pwr = 0;
    d->hit_v0_i = 0; d->hit_v0_q = 0; d->hit_v1_i = 0; d->hit_v1_q = 0;
    d->hit_v2_i = 0; d->hit_v2_q = 0; d->hit_v3_i = 0; d->hit_v3_q = 0;
    d->n_hits = NHIT; d->frame_start = 0; d->frame_end = 0; d->m_ready = 0;

    hold_reset(d, 4);

    // ---- frame_start
    d->frame_start = 1; d->eval(); edge(d);
    d->frame_start = 0;

    // ---- stream the map, dropping the seven detections in as we go
    const long N = (long)NR * ND;
    std::mt19937 brng(4242u);
    std::vector<uint32_t> stream;
    long frame_end_cycle = -1, first_word_cycle = -1;
    long cycle = 0;
    int  hit_i = 0;
    bool saw_last = false;

    for (long k = 0; k < N; k++) {
        d->map_valid = 1;
        d->map_pwr   = map[k];
        if (hit_i < NHIT && (k % 3000) == 500) {
            const HitRec& h = hits[hit_i];
            d->hit_valid = 1;
            d->hit_range = h.r;
            d->hit_dopp  = h.dop;
            d->hit_pwr   = h.pwr;
            d->hit_v0_i = h.v[0]; d->hit_v0_q = h.v[1];
            d->hit_v1_i = h.v[2]; d->hit_v1_q = h.v[3];
            d->hit_v2_i = h.v[4]; d->hit_v2_q = h.v[5];
            d->hit_v3_i = h.v[6]; d->hit_v3_q = h.v[7];
            hit_i++;
        } else {
            d->hit_valid = 0;
        }
        d->m_ready = 0;
        d->eval();
        edge(d);
        cycle++;
    }
    d->map_valid = 0; d->hit_valid = 0;
    for (int i = 0; i < 4; i++) { d->eval(); edge(d); cycle++; }

    // ---- frame_end, then drain the packet with random backpressure
    d->frame_end = 1; d->eval(); frame_end_cycle = cycle; edge(d); cycle++;
    d->frame_end = 0;

    const long DRAIN = (long)NRO * NDO + NHIT * 6 + 64 + 20000;
    for (long i = 0; i < DRAIN && !saw_last; i++) {
        d->m_ready = (brng() % 5) != 0;       // 20% backpressure
        d->eval();
        if (d->m_valid && d->m_ready) {
            if (first_word_cycle < 0) first_word_cycle = cycle;
            stream.push_back(d->m_data);
            if (d->m_last) saw_last = true;
        }
        edge(d);
        cycle++;
    }

    // ---- parse
    bool ok = true;
    std::string why;
    auto check = [&](bool cond, const char* msg) {
        if (!cond && ok) { ok = false; why = msg; }
    };

    check(saw_last, "no m_last / packet never terminated");

    const size_t want_words = 8 + (size_t)NRO * NDO + (size_t)NHIT * 6 + 1;
    check(stream.size() == want_words, "wrong total word count");

    if (ok) {
        check(stream[0] == 0x52414452u, "magic word wrong");
        check((stream[1] >> 16) == 1u, "format version wrong");
        const uint16_t flags = (uint16_t)(stream[1] & 0xFFFF);
        check((flags & 1u) == 1u, "map-present flag not set");
        check((flags & 2u) == 2u, "hits-present flag not set");
        check((flags & 0xFFFCu) == (CFG_FLAGS & 0xFFFCu), "upper flag bits not passed through");
        check(stream[2] == FRAME_INDEX, "frame index wrong");
        check((stream[3] >> 16) == (uint32_t)NRO, "n_range_out wrong");
        check((stream[3] & 0xFFFFu) == (uint32_t)NDO, "n_doppler_out wrong");
        check((stream[4] >> 16) == (uint32_t)NHIT, "n_hits wrong");
        check((stream[4] & 0xFFFFu) == 0u, "reserved half of word 4 not zero");
        check(stream[5] == NOISE, "noise word wrong");
        check(stream[6] == (uint32_t)(TIMESTAMP >> 32), "timestamp high word wrong");
        check(stream[7] == (uint32_t)(TIMESTAMP & 0xFFFFFFFFull), "timestamp low word wrong");
    }

    int map_mismatch = 0;
    long first_bad = -1;
    if (ok) {
        for (size_t i = 0; i < expect_map.size(); i++) {
            if (stream[8 + i] != expect_map[i]) {
                if (first_bad < 0) first_bad = (long)i;
                map_mismatch++;
            }
        }
        check(map_mismatch == 0, "decimated map does not match the block maxima");
    }

    if (ok) {
        const size_t base = 8 + (size_t)NRO * NDO;
        for (int h = 0; h < NHIT && ok; h++) {
            const HitRec& e = hits[h];
            const uint32_t* w = &stream[base + (size_t)h * 6];
            const uint32_t w0 = ((uint32_t)(e.dop & 0x100) << 8) |
                                ((uint32_t)(e.dop & 0xFF) << 8) | (uint32_t)e.r;
            check(w[0] == w0, "detection word +0 wrong");
            check(w[1] == e.pwr, "detection power wrong");
            for (int v = 0; v < 4 && ok; v++) {
                const uint32_t ew = ((uint32_t)(uint16_t)e.v[2 * v] << 16) |
                                    (uint32_t)(uint16_t)e.v[2 * v + 1];
                check(w[2 + v] == ew, "detection virtual-channel word wrong");
            }
        }
        check(stream.back() == 0x454E4452u, "end marker wrong");
    }

    // Prove the decimation really is a maximum: a sampling decimator would
    // have produced these instead, and they must differ everywhere.
    int would_differ = 0;
    for (int br = 0; br < NRO; br++)
        for (int bc = 0; bc < NDO; bc++) {
            const uint32_t sampled = map[(size_t)(br * DR) * ND + (bc * DD)];
            if (sampled != expect_map[(size_t)br * NDO + bc]) would_differ++;
        }
    check(would_differ == NRO * NDO, "test map is not sensitive to max-vs-sample");

    char note[320];
    snprintf(note, sizeof note,
             "%zu words (8 hdr + %d map + %d hit + 1 end), %d map cells checked, "
             "%d differ from a sampling decimator, first word %ld clk after "
             "frame_end, 20%% backpressure %s",
             stream.size(), NRO * NDO, NHIT * 6, (int)expect_map.size(),
             would_differ, first_word_cycle - frame_end_cycle, why.c_str());
    result("pack packet format, radar_pkg.svh section 7", ok, note);

    delete d;
}

//============================================================================
int main(int argc, char** argv)
{
    Verilated::commandArgs(argc, argv);

    printf("== radar back end, Verilator %s ==\n\n", Verilated::productVersion());

    test_corner_turn();
    test_power();
    test_cfar();
    test_pack();

    printf("\n%s: %d test(s) failed\n", g_fail ? "OVERALL FAIL" : "OVERALL PASS", g_fail);
    return g_fail ? 1 : 0;
}
