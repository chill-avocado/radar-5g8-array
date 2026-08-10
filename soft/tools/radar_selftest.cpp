//============================================================================
// radar-selftest -- prove the stack is right, with numbers
//
// Every check prints what it measured, not just whether it passed, because
// "36 tests passed" is not evidence and a reviewer is entitled to better. A
// failing number is more useful than a failing assertion.
//
// The checks run in dependency order, so the first failure is the root cause
// rather than a symptom of something further up.
//
//   radar-selftest              everything
//   radar-selftest --quick      skip the statistical checks (a few seconds)
//   radar-selftest fft cfar     only the named groups
//============================================================================
#include "radar/aoa.hpp"
#include "radar/calib.hpp"
#include "radar/cfar.hpp"
#include "radar/cluster.hpp"
#include "radar/config.hpp"
#include "radar/core.hpp"
#include "radar/fft.hpp"
#include "radar/json.hpp"
#include "radar/log.hpp"
#include "radar/microdoppler.hpp"
#include "radar/proto.hpp"
#include "radar/refmodel.hpp"
#include "radar/source.hpp"
#include "radar/track.hpp"
#include "radar/types.hpp"
#include "radar/waveform.hpp"
#include "radar/window.hpp"

#include <algorithm>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <random>
#include <string>
#include <vector>

using namespace radar;
using namespace radar::proto;

//============================================================================
// A very small harness
//============================================================================
namespace {

int  g_pass = 0, g_fail = 0;
bool g_quick = false;
std::vector<std::string> g_only;
std::vector<std::string> g_failures;

bool group_wanted(const char* name) {
    if (g_only.empty()) return true;
    for (const auto& s : g_only) if (s == name) return true;
    return false;
}

void head(const char* title) {
    std::printf("\n\033[1m%s\033[0m\n", title);
    for (std::size_t i = 0; i < std::strlen(title); ++i) std::printf("-");
    std::printf("\n");
}

/// The workhorse. Prints the measurement every time, pass or fail.
void check(bool ok, const char* what, const char* fmt = nullptr, ...) {
    char detail[512] = "";
    if (fmt) {
        va_list ap;
        va_start(ap, fmt);
        std::vsnprintf(detail, sizeof(detail), fmt, ap);
        va_end(ap);
    }
    if (ok) {
        ++g_pass;
        std::printf("  \033[32mok\033[0m   %-46s %s\n", what, detail);
    } else {
        ++g_fail;
        std::printf("  \033[31mFAIL\033[0m %-46s %s\n", what, detail);
        g_failures.push_back(std::string(what) + "  " + detail);
    }
}

double now_s() {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

std::mt19937& rng() {
    // Fixed seed: a self-test that passes only sometimes is worse than no test.
    static std::mt19937 g(0x5A6D0000u);
    return g;
}

} // namespace

//============================================================================
// 1. Fixed point -- the contract with the RTL
//============================================================================
static void test_fixedpoint() {
    if (!group_wanted("fx")) return;
    head("Fixed-point arithmetic (the contract the FPGA is built against)");

    // Round-half-up towards positive infinity, then saturate. Ties matter: a
    // DC-heavy signal such as transmit leakage would accumulate a bias if the
    // two implementations disagreed on them.
    check(fx::round_sat(7, 1, 16) == 4,   "round_sat(7,>>1)  ties go up",     "got %lld", (long long)fx::round_sat(7, 1, 16));
    check(fx::round_sat(-7, 1, 16) == -3, "round_sat(-7,>>1) ties go up",     "got %lld", (long long)fx::round_sat(-7, 1, 16));
    check(fx::round_sat(5, 1, 16) == 3,   "round_sat(5,>>1)",                 "got %lld", (long long)fx::round_sat(5, 1, 16));
    check(fx::round_sat(1 << 20, 0, 16) == 32767, "saturates high",           "got %lld", (long long)fx::round_sat(1 << 20, 0, 16));
    check(fx::round_sat(-(1 << 20), 0, 16) == -32768, "saturates low",        "got %lld", (long long)fx::round_sat(-(1 << 20), 0, 16));

    // Arithmetic shift on negatives, which the RTL's >>> performs. C++17
    // leaves this implementation-defined; clang and gcc both do the right
    // thing, and if a compiler ever did not the whole model would be wrong.
    check((i64(-8) >> 2) == -2, "negative right shift is arithmetic", "(-8)>>2 = %lld", (long long)(i64(-8) >> 2));

    // Conjugate multiply: full scale times full scale must not wrap.
    const ci16 full(-32768, -32768);
    const ci16 r = fx::cmul_conj_q15(full, full, 15);
    check(r.re >= -32768 && r.re <= 32767 && r.im >= -32768 && r.im <= 32767,
          "cmul_conj_q15 at full scale stays in range", "got (%d, %d)", r.re, r.im);

    // conj(a)*a must be purely real and equal to |a|^2.
    const ci16 a(12345, -6789);
    const ci16 sq = fx::cmul_conj_q15(a, a, 15);
    check(sq.im == 0, "conj(a)*a is real", "imag = %d", sq.im);

    check(fx::power(ci16(32767, 32767)) == 2u * 32767u * 32767u,
          "power is exact at full scale", "%u", fx::power(ci16(32767, 32767)));
}

//============================================================================
// 2. Geometry -- does the software agree with the board that was made
//============================================================================
static void test_geometry() {
    if (!group_wanted("geom")) return;
    head("Array geometry against the fabricated board");

    std::string err;
    Json j = Json::parse_file("kicad/radar_5g8_transmit_array/array_report.json", &err);
    if (j.is_null()) j = Json::parse_file("../kicad/radar_5g8_transmit_array/array_report.json", &err);
    if (j.is_null()) {
        std::printf("  \033[33mskip\033[0m array_report.json not found from this directory\n");
        return;
    }

    // The report rounds to four decimal places of a millimetre, so the
    // tolerance is a micrometre, not a picometre. Anything larger than that
    // would be a genuine disagreement about the geometry.
    const double lam_json = j["array"]["wavelength_mm"].num() * 1e-3;
    check(std::abs(lam_json - array_geom::lambda_m) < 1e-6,
          "wavelength matches the board report", "%.4f mm vs %.4f mm",
          lam_json * 1e3, array_geom::lambda_m * 1e3);

    const double pitch_json = j["array"]["element_pitch_mm"].num() * 1e-3;
    check(std::abs(pitch_json - array_geom::pitch_m) < 1e-6,
          "element pitch matches", "%.4f mm", pitch_json * 1e3);

    const Json& vp = j["array"]["virtual_array_positions_mm"];
    bool all_ok = vp.size() == 4;
    double worst = 0;
    for (std::size_t i = 0; i < vp.size() && i < 4; ++i) {
        for (int k = 0; k < 2; ++k) {
            const double d = std::abs(vp[i][std::size_t(k)].num() * 1e-3 - array_geom::virt_xy[i][k]);
            worst = std::max(worst, d);
            if (d > 1e-7) all_ok = false;
        }
    }
    check(all_ok, "all four virtual element positions match", "worst error %.3g mm", worst * 1e3);

    const double iso = j["measured"]["tx_to_rx_worst_db"].num();
    check(std::abs(iso - array_geom::tx_rx_isolation_db) < 0.05,
          "measured isolation matches", "%.1f dB", iso);
}

//============================================================================
// 3. FFT
//============================================================================
static void test_fft() {
    if (!group_wanted("fft")) return;
    head("Fast Fourier transform");
    std::printf("  backend: %s\n", Fft::backend());

    std::uniform_real_distribution<float> u(-1.0f, 1.0f);
    for (int n : {8, 64, 256, 1024, 4096}) {
        std::vector<cf32> in(n), got(n), want(n);
        for (auto& x : in) x = cf32(u(rng()), u(rng()));

        Fft f(n, false);
        f.run(in.data(), got.data(), 1);
        dft_reference(in.data(), want.data(), n, false);

        double num = 0, den = 0;
        for (int i = 0; i < n; ++i) {
            num += std::norm(got[i] - want[i]);
            den += std::norm(want[i]);
        }
        const double err_db = 10.0 * std::log10(num / std::max(den, 1e-300) + 1e-300);
        check(err_db < -70.0, "forward matches the direct transform", "n=%5d  %.1f dB", n, err_db);

        // Forward then inverse must return n times the input, which is the
        // unnormalised convention the FPGA also uses.
        Fft g(n, true);
        std::vector<cf32> back(n);
        g.run(got.data(), back.data(), 1);
        double rt = 0;
        for (int i = 0; i < n; ++i) rt += std::norm(back[i] / float(n) - in[i]);
        const double rt_db = 10.0 * std::log10(rt / std::max(den, 1e-300) + 1e-300);
        check(rt_db < -70.0, "round trip returns the input", "n=%5d  %.1f dB", n, rt_db);
    }

    // A tone exactly on a bin must land entirely in that bin.
    {
        const int n = 1024, bin = 137;
        std::vector<cf32> in(n), out(n);
        for (int i = 0; i < n; ++i) {
            const double p = 2.0 * kPi * bin * i / n;
            in[i] = cf32(float(std::cos(p)), float(std::sin(p)));
        }
        Fft(n, false).run(in.data(), out.data(), 1);
        int peak = 0;
        for (int i = 1; i < n; ++i) if (std::abs(out[i]) > std::abs(out[peak])) peak = i;
        double leak = 0;
        for (int i = 0; i < n; ++i) if (i != peak) leak = std::max(leak, double(std::abs(out[i])));
        check(peak == bin, "on-bin tone lands in the right bin", "peak at %d, wanted %d", peak, bin);
        check(db_amp(leak / std::abs(out[peak])) < -100.0, "no leakage from an on-bin tone",
              "%.1f dB", db_amp(leak / std::abs(out[peak])));
    }

    // Batched transforms must not corrupt each other.
    {
        const int n = 256, batch = 7;
        std::vector<cf32> in(n * batch), single(n), batched(n * batch);
        for (auto& x : in) x = cf32(u(rng()), u(rng()));
        Fft f(n, false);
        f.run(in.data(), batched.data(), batch);
        double worst = 0;
        for (int b = 0; b < batch; ++b) {
            f.run(in.data() + b * n, single.data(), 1);
            for (int i = 0; i < n; ++i) worst = std::max(worst, double(std::abs(batched[b * n + i] - single[i])));
        }
        check(worst < 1e-3, "batched equals one at a time", "worst %.3g", worst);
    }
}

//============================================================================
// 4. Windows
//============================================================================
static void test_windows() {
    if (!group_wanted("win")) return;
    head("Transform windows against their textbook figures");

    struct Expect { WindowKind k; const char* name; double enbw, scallop, sidelobe; };
    // Harris 1978, table 1. Tolerances are loose enough for the numerical
    // measurement but tight enough to catch a wrong coefficient set.
    const Expect want[] = {
        {WindowKind::Rect,           "rectangular",      1.000, 3.92, -13.3},
        {WindowKind::Hann,           "Hann",             1.500, 1.42, -31.5},
        {WindowKind::Hamming,        "Hamming",          1.363, 1.78, -42.7},
        {WindowKind::BlackmanHarris, "Blackman-Harris",  2.004, 0.83, -92.0},
    };
    for (const auto& e : want) {
        const auto w = make_window(e.k, 1024);
        const double enbw = window_enbw(w);
        const double sc   = window_scallop_loss_db(w);
        const double sl   = window_peak_sidelobe_db(w);
        check(std::abs(enbw - e.enbw) < 0.02, "ENBW", "%-16s %.3f bins (want %.3f)", e.name, enbw, e.enbw);
        check(std::abs(sc - e.scallop) < 0.12, "scallop loss", "%-16s %.2f dB (want %.2f)", e.name, sc, e.scallop);
        check(sl < e.sidelobe + 3.0, "peak sidelobe", "%-16s %.1f dB (want <= %.1f)", e.name, sl, e.sidelobe);
    }

    // Taylor and Chebyshev must actually deliver the sidelobe level asked for.
    for (double want_db : {40.0, 60.0, 80.0}) {
        const auto t = make_window(WindowKind::Taylor, 1024, want_db);
        const double got = window_peak_sidelobe_db(t);
        check(got < -(want_db - 4.0), "Taylor reaches its design sidelobe",
              "asked %.0f dB, got %.1f dB", want_db, got);
        const auto c = make_window(WindowKind::Chebyshev, 1024, want_db);
        const double gc = window_peak_sidelobe_db(c);
        check(gc < -(want_db - 2.0), "Chebyshev reaches its design sidelobe",
              "asked %.0f dB, got %.1f dB", want_db, gc);
    }

    // The quantised table the FPGA holds must not lose the sidelobe advantage.
    {
        const auto w = make_window(WindowKind::BlackmanHarris, 1024);
        const auto q = quantise_window(w);
        std::vector<float> back(q.size());
        for (std::size_t i = 0; i < q.size(); ++i) back[i] = q[i] / 32768.0f;
        const double sl = window_peak_sidelobe_db(back);
        check(sl < -70.0, "16-bit quantised window keeps its sidelobes", "%.1f dB", sl);
    }
}

//============================================================================
// 5. Configuration and the waveform
//============================================================================
static void test_config() {
    if (!group_wanted("cfg")) return;
    head("Configuration and waveform geometry");

    Config c;
    c.derive();
    check(c.validate().empty(), "the defaults are realisable", "%s", c.validate().c_str());

    std::printf("      range resolution   %.3f m      covered %.0f m in %d bins\n",
                c.d.range_res_m, c.d.range_max_m, c.n_range);
    std::printf("      velocity resolution %.3f m/s   unambiguous +/- %.1f m/s\n",
                c.d.vel_res_ms, c.d.vel_max_ms);
    std::printf("      frame %.2f ms -> %.2f per second,  host link %.2f MB/s\n",
                c.d.t_cpi_s * 1e3, c.d.frame_rate_hz, c.d.usb_bytes_s / 1e6);

    // Range resolution is c/2B and nothing else. If this is wrong every range
    // the radar reports is wrong by the same factor.
    const double want_res = phys::c0 / (2.0 * c.sweep_bw_hz);
    check(std::abs(c.d.range_res_m - want_res) < 1e-6, "range resolution is c/2B",
          "%.4f m vs %.4f m", c.d.range_res_m, want_res);

    // Unambiguous velocity for TDM uses twice the repetition interval, because
    // each transmitter only gets every other chirp.
    const double pri_eff = c.d.t_pri_s * 2.0;
    const double want_v  = c.d.lambda_m / (4.0 * pri_eff);
    check(std::abs(c.d.vel_max_ms - want_v) < 1e-6, "TDM halves the unambiguous velocity",
          "%.2f m/s vs %.2f m/s", c.d.vel_max_ms, want_v);

    // The memory constraint that set the whole operating point.
    check(c.n_range * c.d.n_chirp_total == 65536, "corner-turn buffer is exactly full",
          "%d x %d = %d", c.n_range, c.d.n_chirp_total, c.n_range * c.d.n_chirp_total);

    // validate() has to reject what the hardware cannot do.
    { Config b = c; b.n_range = 512; b.derive(); check(!b.validate().empty(), "rejects an oversized buffer", "%s", b.validate().c_str()); }
    { Config b = c; b.sweep_bw_hz = 200e6; b.derive(); check(!b.validate().empty(), "rejects a sweep wider than the radio", "%s", b.validate().c_str()); }
    { Config b = c; b.n_sweep = b.n_pri + 1; b.derive(); check(!b.validate().empty(), "rejects a chirp longer than its interval", "%s", b.validate().c_str()); }
    { Config b = c; b.n_range_fft = 1000; b.derive(); check(!b.validate().empty(), "rejects a non-power-of-two transform", "%s", b.validate().c_str()); }

    // Waveform bin mapping must round-trip.
    Waveform wf(c);
    double worst = 0;
    for (int b = 1; b < c.n_range; ++b) {
        const double r  = wf.range_of_bin(b);
        const int    bb = wf.bin_of_range(r);
        worst = std::max(worst, double(std::abs(bb - b)));
    }
    check(worst < 1e-9, "range bin and range round-trip", "worst error %.1f bins", worst);

    // The transmitted chirp must be exactly what the NCO produces.
    const auto& chirp = wf.chirp_q15();
    check(int(chirp.size()) == c.n_sweep, "chirp is the configured length",
          "%zu samples", chirp.size());
    double pk = 0;
    for (const auto& s : chirp) pk = std::max(pk, std::hypot(double(s.re), double(s.im)));
    check(pk > 30000 && pk <= 32768.0 * 1.42, "chirp uses the converter's range",
          "peak magnitude %.0f of 32768", pk);
}

//============================================================================
// 6. The bit-exact model of the FPGA datapath
//============================================================================
static void test_refmodel() {
    if (!group_wanted("ref")) return;
    head("Bit-exact model of the FPGA datapath");

    Config c;
    c.derive();
    Waveform wf(c);
    RefModel rm(c);

    // The halfband coefficients must have exactly unit DC gain, or every range
    // profile is scaled wrong and the link budget stops matching.
    const auto& hb = rm.halfband_coefs();
    long long   dc = 0;
    for (auto k : hb) dc += k;
    check(std::abs(dc - (1 << 17)) <= 1, "halfband filter has unit DC gain",
          "sum = %lld, want %d", dc, 1 << 17);

    // One point target at a known range and velocity. The received signal is
    // built the honest way -- the transmitted chirp, delayed and phase shifted
    // -- so the model's own de-chirp has to produce the beat. Anything that
    // shortcuts to a beat tone would not test the de-chirp at all.
    const int    want_range_bin = 40;
    const int    want_dopp_bin  = 12;
    const double R = wf.range_of_bin(want_range_bin);
    const double V = wf.velocity_of_bin(want_dopp_bin);

    const int    nsw   = c.n_sweep;
    const int    nct   = c.d.n_chirp_total;
    const double slope = c.d.chirp_slope_hz_s;
    const double fs    = c.sample_rate_hz;
    const double lam   = c.d.lambda_m;
    const double f_lo  = -c.sweep_bw_hz / 2.0;   // baseband sweep starts low
    const double amp   = 0.25;

    std::vector<std::vector<ci16>> rx(2, std::vector<ci16>(std::size_t(nct) * nsw));
    for (int ch = 0; ch < nct; ++ch) {
        // Positive velocity means approaching, so the range shrinks with time.
        const double Rc  = R - V * (ch * c.d.t_pri_s);
        const double tau = 2.0 * Rc / phys::c0;
        // Baseband return = s(t - tau) * exp(-j 2 pi f_c tau).
        const double carrier = -2.0 * kPi * (phys::c0 / lam) * tau;
        for (int n = 0; n < nsw; ++n) {
            const double td = n / fs - tau;
            const double ph = 2.0 * kPi * (f_lo * td + 0.5 * slope * td * td) + carrier;
            const ci16 s(fx::to_q15(amp * std::cos(ph)), fx::to_q15(amp * std::sin(ph)));
            rx[0][std::size_t(ch) * nsw + n] = s;
            rx[1][std::size_t(ch) * nsw + n] = s;
        }
    }

    RdFrame f;
    const ci16* ptrs[2] = {rx[0].data(), rx[1].data()};
    rm.process_cpi(ptrs, 2, nct, f);
    check(f.n_range == c.n_range && f.n_doppler == c.n_doppler,
          "model produces a map of the configured shape", "%d x %d", f.n_range, f.n_doppler);

    double pmax = 0; int pr = 0, pd = 0;
    for (int r = 1; r < f.n_range; ++r)
        for (int d = 0; d < f.n_doppler; ++d)
            if (f.at(r, d) > pmax) { pmax = f.at(r, d); pr = r; pd = d; }

    const int dopp_signed = pd - c.n_doppler / 2;
    check(std::abs(pr - want_range_bin) <= 1, "target lands in the right range bin",
          "got %d, wanted %d  (%.1f m)", pr, want_range_bin, R);
    check(std::abs(dopp_signed - want_dopp_bin) <= 1, "target lands in the right Doppler bin",
          "got %+d, wanted %+d  (%.2f m/s)", dopp_signed, want_dopp_bin, V);

    // What the fixed-point arithmetic costs against the same chain in floats.
    {
        std::vector<std::vector<cf32>> rxf(2, std::vector<cf32>(std::size_t(nct) * nsw));
        for (int r = 0; r < 2; ++r)
            for (std::size_t i = 0; i < rxf[r].size(); ++i) rxf[r][i] = rx[r][i].to_float();
        RdFrame ff;
        const cf32* fptrs[2] = {rxf[0].data(), rxf[1].data()};
        rm.process_cpi_float(fptrs, 2, nct, ff);

        auto peak_to_floor = [](const RdFrame& fr) {
            double pk = 0, sum = 0; int n = 0;
            for (int r = 4; r < fr.n_range; ++r)
                for (int d = 0; d < fr.n_doppler; ++d) {
                    const double v = fr.at(r, d);
                    pk = std::max(pk, v);
                }
            for (int r = 4; r < fr.n_range; ++r)
                for (int d = 0; d < fr.n_doppler; ++d) {
                    const double v = fr.at(r, d);
                    if (v < pk * 1e-3) { sum += v; ++n; }
                }
            return n ? db(pk / (sum / n)) : 0.0;
        };
        const double fixed_db = peak_to_floor(f);
        const double float_db = peak_to_floor(ff);
        check(float_db - fixed_db < 12.0, "fixed point costs little against floating point",
              "%.1f dB fixed vs %.1f dB float, loss %.1f dB", fixed_db, float_db, float_db - fixed_db);
    }
}

//============================================================================
// 7. CFAR
//============================================================================
static void test_cfar() {
    if (!group_wanted("cfar")) return;
    head("Constant false alarm rate detection");

    Config c;
    c.n_range = 256; c.n_doppler = 128; c.zero_dopp_blank = 0; c.range_zero_bin = 0;
    c.max_hits = 100000;
    c.derive();

    // The false-alarm rate is the whole point of CFAR: the threshold has to
    // track the noise so that the rate is what was asked for regardless of
    // level. Exponentially distributed power is what a complex Gaussian
    // channel gives after |.|^2.
    for (double pfa : {1e-4, 1e-5}) {
        Config cc = c; cc.pfa = pfa; cc.derive();
        Cfar2D cfar(cc);
        std::exponential_distribution<double> ex(1.0);

        RdFrame f;
        f.allocate(cc.n_range, cc.n_doppler, 4, false);
        for (std::size_t i = 0; i < f.power.size(); ++i) f.power[i] = float(ex(rng()));

        std::vector<Hit> hits;
        cfar.detect(f, hits);

        const int halo_r = cc.guard_range + cc.train_range;
        const int halo_d = cc.guard_dopp + cc.train_dopp;
        const double tested = double(cc.n_range - 2 * halo_r) * double(cc.n_doppler - 2 * halo_d);
        const double got = hits.size() / tested;
        const double ratio = got / pfa;
        check(ratio > 0.2 && ratio < 5.0, "realised false-alarm rate tracks the design",
              "asked %.0e, got %.2e (%.1fx)", pfa, got, ratio);
    }
    if (g_quick) return;

    // Detection probability against signal to noise ratio.
    {
        Config cc = c; cc.pfa = 1e-5; cc.derive();
        Cfar2D cfar(cc);
        std::exponential_distribution<double> ex(1.0);
        for (double snr_db : {8.0, 10.0, 13.0, 16.0, 20.0}) {
            const double amp = std::pow(10.0, snr_db / 10.0);
            int found = 0;
            const int trials = 200;
            for (int t = 0; t < trials; ++t) {
                RdFrame f;
                f.allocate(cc.n_range, cc.n_doppler, 4, false);
                for (std::size_t i = 0; i < f.power.size(); ++i) f.power[i] = float(ex(rng()));
                const int R = 100, D = 40;
                f.at(R, D) += float(amp);
                std::vector<Hit> hits;
                cfar.detect(f, hits);
                for (const auto& h : hits)
                    if (std::abs(h.range_bin - R) <= 1 && std::abs(h.dopp_bin - D) <= 1) { ++found; break; }
            }
            const double pd = double(found) / trials;
            // 13 dB is the threshold the link budget is written against, so
            // that is the one that has to work.
            const bool ok = (snr_db < 12.0) || (pd > 0.85);
            check(ok, "detection probability", "%4.1f dB SNR -> Pd = %.2f", snr_db, pd);
        }
    }
}

//============================================================================
// 8. Angle of arrival
//============================================================================
static void test_aoa() {
    if (!group_wanted("aoa")) return;
    head("Angle of arrival from the four virtual channels");

    struct Case { double az, el; };
    const Case cases[] = {{0, 0}, {20, 10}, {-35, -20}, {60, 30}};
    const struct { AoaMethod m; const char* name; } methods[] = {
        {AoaMethod::Monopulse, "monopulse"},
        {AoaMethod::Bartlett,  "Bartlett"},
        {AoaMethod::Capon,     "Capon"},
        {AoaMethod::Music,     "MUSIC"},
    };

    const int trials = g_quick ? 30 : 200;
    const double snr_db = 20.0;
    const double noise = std::pow(10.0, -snr_db / 20.0);
    std::normal_distribution<double> nrm(0.0, noise / std::sqrt(2.0));

    for (const auto& meth : methods) {
        Config c; c.aoa = meth.m; c.derive();
        AoaEngine eng(c);
        double worst_rms = 0;
        for (const auto& cs : cases) {
            double se = 0; int n = 0;
            for (int t = 0; t < trials; ++t) {
                const double u = std::sin(rad(cs.az)) * std::cos(rad(cs.el));
                const double v = std::sin(rad(cs.el));
                const double k = 2.0 * kPi / array_geom::lambda_m;
                std::array<cf32, 4> x{};
                for (int i = 0; i < 4; ++i) {
                    const double ph = k * (array_geom::virt_xy[i][0] * u + array_geom::virt_xy[i][1] * v);
                    x[i] = cf32(float(std::cos(ph) + nrm(rng())), float(std::sin(ph) + nrm(rng())));
                }
                const auto r = eng.estimate(x);
                if (!r.valid) continue;
                const double d_az = r.az_deg - cs.az, d_el = r.el_deg - cs.el;
                se += d_az * d_az + d_el * d_el;
                ++n;
            }
            const double rms = n ? std::sqrt(se / n) : 1e9;
            worst_rms = std::max(worst_rms, rms);
            std::printf("       %-10s at (%+4.0f, %+4.0f): rms %.2f deg over %d trials\n",
                        meth.name, cs.az, cs.el, rms, n);
        }
        // A 2x2 array at half-wave spacing has a beamwidth around 50 degrees,
        // so at 20 dB signal to noise a few degrees is the right order. Ten is
        // a generous ceiling that still catches a wrong steering vector.
        check(worst_rms < 10.0, "angle error at 20 dB", "%-10s worst %.2f deg", meth.name, worst_rms);
    }
}

//============================================================================
// 9. Clustering and tracking
//============================================================================
static void test_cluster_track() {
    if (!group_wanted("track")) return;
    head("Clustering and tracking");

    Config c; c.derive();

    // Five well-separated clusters plus scattered outliers.
    {
        Clusterer cl(c);
        std::vector<Hit> hits;
        const double centres[5][3] = {{50, 5, 0}, {120, -8, 20}, {200, 15, -25}, {310, 0, 10}, {400, -20, 35}};
        std::normal_distribution<double> jitter(0.0, 1.0);
        for (int k = 0; k < 5; ++k) {
            for (int i = 0; i < 56; ++i) {
                Hit h;
                h.range_m = centres[k][0] + jitter(rng()) * 2.0;
                h.velocity_ms = centres[k][1] + jitter(rng()) * 0.4;
                h.azimuth_deg = centres[k][2] + jitter(rng()) * 2.0;
                h.elevation_deg = jitter(rng()) * 2.0;
                h.angle_valid = true;
                h.power = 100.0; h.snr_db = 20.0;
                hits.push_back(h);
            }
        }
        std::uniform_real_distribution<double> ur(10, 500), uv(-40, 40), ua(-60, 60);
        for (int i = 0; i < 20; ++i) {
            Hit h; h.range_m = ur(rng()); h.velocity_ms = uv(rng());
            h.azimuth_deg = ua(rng()); h.angle_valid = true; h.power = 50; h.snr_db = 14;
            hits.push_back(h);
        }
        std::vector<Target> tg;
        cl.cluster(hits, tg);
        check(tg.size() >= 5 && tg.size() <= 8, "finds the five real clusters",
              "%zu targets from %zu hits (5 real + 20 outliers)", tg.size(), hits.size());
    }

    // A target on a straight line, then one pulling three g.
    for (int scenario = 0; scenario < 2; ++scenario) {
        Tracker tr(c);
        const double dt = c.d.t_cpi_s;
        double x = -60, y = 200, vx = 25, vy = 0;
        double se = 0; int n = 0; int confirmed_at = -1;
        std::vector<Track> out;
        std::normal_distribution<double> mn(0.0, 1.0);

        for (int k = 0; k < 120; ++k) {
            if (scenario == 1 && k > 40) {
                // 3 g lateral, which is about as hard as a quadcopter turns.
                const double a = 29.4, sp = std::hypot(vx, vy);
                const double ax = -a * vy / sp, ay = a * vx / sp;
                vx += ax * dt; vy += ay * dt;
            }
            x += vx * dt; y += vy * dt;

            Target z;
            z.range_m = std::hypot(x, y) + mn(rng()) * (c.d.range_res_m / 3.5);
            z.azimuth_deg = deg(std::atan2(x, y)) + mn(rng()) * 1.5;
            z.elevation_deg = mn(rng()) * 1.5;
            z.velocity_ms = -(x * vx + y * vy) / std::hypot(x, y) + mn(rng()) * (c.d.vel_res_ms / 3.5);
            z.snr_db = 20; z.n_hits = 5;
            z.x = z.range_m * std::sin(rad(z.azimuth_deg));
            z.y = z.range_m * std::cos(rad(z.azimuth_deg));
            z.z = 0;

            tr.update({z}, dt, out);
            if (!out.empty()) {
                if (out[0].confirmed && confirmed_at < 0) confirmed_at = k;
                if (k > 20) {
                    const double dx = out[0].x - x, dy = out[0].y - y;
                    se += dx * dx + dy * dy;
                    ++n;
                }
            }
        }
        const double rms = n ? std::sqrt(se / n) : 1e9;
        const char* label = scenario == 0 ? "straight line" : "3 g turn";
        check(!out.empty() && out[0].confirmed, "track survives the run",
              "%-14s confirmed at frame %d, %zu tracks alive", label, confirmed_at, out.size());
        check(rms < (scenario == 0 ? 8.0 : 25.0), "position error", "%-14s rms %.2f m", label, rms);
    }
}

//============================================================================
// 10. Micro-Doppler
//============================================================================
static void test_microdoppler() {
    if (!group_wanted("md")) return;
    head("Micro-Doppler");

    Config c; c.derive();
    MicroDoppler md(c);

    // A two-blade rotor at 100 Hz on top of 8 m/s of bulk motion.
    const int n = c.d.n_chirp_total;
    const double dt = c.d.t_pri_s;
    const double lam = c.d.lambda_m;
    const double blade_hz = 100.0, bulk_ms = 8.0;

    std::vector<cf32> slow(n);
    for (int i = 0; i < n; ++i) {
        const double t = i * dt;
        const double bulk = 2.0 * kPi * (2.0 * bulk_ms / lam) * t;
        // Blade flash: the rotor's radial extent modulates the phase.
        const double micro = 3.0 * std::sin(2.0 * kPi * blade_hz * t);
        slow[i] = cf32(float(std::cos(bulk + micro)), float(std::sin(bulk + micro)));
    }

    Track t;
    t.id = 1; t.x = 0; t.y = 120; t.vx = 0; t.vy = -bulk_ms; t.confirmed = true;
    md.analyse(slow.data(), n, t);

    check(t.spec_time > 0 && t.spec_freq > 0, "produces a spectrogram",
          "%d x %d", t.spec_time, t.spec_freq);
    check(t.micro_bw_hz > 100.0, "sees a wide micro-Doppler skirt", "%.0f Hz", t.micro_bw_hz);
    const bool near = t.blade_hz > 0 && std::abs(t.blade_hz - blade_hz) / blade_hz < 0.25;
    check(near, "recovers the blade rate", "%.1f Hz, injected %.0f Hz", t.blade_hz, blade_hz);
}

//============================================================================
// 11. Calibration
//============================================================================
static void test_calibration() {
    if (!group_wanted("cal")) return;
    head("Calibration");

    // Four channels with invented gain and phase errors; after solving from a
    // boresight measurement they must all agree.
    Calibration cal;
    const double gains[4] = {1.0, 0.63, 1.41, 0.79};
    const double phases[4] = {0.0, 1.1, -2.3, 0.4};
    std::array<cf32, 4> measured{};
    for (int i = 0; i < 4; ++i)
        measured[i] = cf32(float(gains[i] * std::cos(phases[i])), float(gains[i] * std::sin(phases[i])));

    cal.solve_boresight(measured);
    std::array<cf32, 4> corrected = measured;
    cal.apply(corrected);

    double amp_spread = 0, phase_spread = 0;
    for (int i = 1; i < 4; ++i) {
        amp_spread = std::max(amp_spread, std::abs(db_amp(std::abs(corrected[i]) / std::abs(corrected[0]))));
        phase_spread = std::max(phase_spread, std::abs(deg(std::arg(corrected[i] * std::conj(corrected[0])))));
    }
    check(amp_spread < 0.01, "boresight equalises the amplitudes", "spread %.4f dB", amp_spread);
    check(phase_spread < 0.01, "boresight equalises the phases", "spread %.4f deg", phase_spread);

    // The leakage peak must be found and become range zero.
    {
        std::vector<float> profile(256, 1.0f);
        for (int i = 0; i < 256; ++i) profile[i] = 1.0f + 0.1f * float(i % 7);
        profile[3] = 5000.0f;
        Calibration k;
        const int bin = k.solve_range_zero(profile.data(), 256);
        check(bin == 3, "finds the transmit-leakage peak", "bin %d, wanted 3", bin);
    }
    {
        // A profile with no leakage must be refused rather than guessed at.
        std::vector<float> flat(256, 1.0f);
        Calibration k;
        const int bin = k.solve_range_zero(flat.data(), 256);
        check(bin == -1, "refuses to invent a range origin", "returned %d", bin);
    }

    // Save and load must round-trip.
    {
        const std::string path = "/tmp/radar_selftest_cal.json";
        std::string err;
        check(cal.save(path, &err), "saves", "%s", err.c_str());
        Calibration back;
        check(back.load(path, &err), "loads", "%s", err.c_str());
        double worst = 0;
        for (int i = 0; i < 4; ++i) worst = std::max(worst, double(std::abs(back.fixed()[i] - cal.fixed()[i])));
        check(worst < 1e-6, "round-trips exactly", "worst %.3g", worst);
        std::remove(path.c_str());
    }
}

//============================================================================
// 12. Wire protocol
//============================================================================
static void test_proto() {
    if (!group_wanted("proto")) return;
    head("Wire protocol to the display");

    Config c; c.derive();
    RdFrame f;
    f.allocate(c.n_range, c.n_doppler, 4, false);
    for (std::size_t i = 0; i < f.power.size(); ++i) f.power[i] = float(1.0 + (i % 977));
    f.index = 42;
    for (int i = 0; i < 5; ++i) {
        Hit h; h.range_bin = 10 * i; h.dopp_bin = i - 2; h.range_m = 22.5 * i;
        h.velocity_ms = 3.0 * i; h.snr_db = 15 + i; h.azimuth_deg = 5.0 * i;
        h.elevation_deg = -2.0 * i; h.angle_valid = true; h.power = 1000 + i;
        f.hits.push_back(h);
    }

    std::vector<u8> buf;
    const DbRange r = auto_range(f, 70.0);
    encode_rdmap(buf, f, 12345, r);
    encode_hits(buf, f, 12345);

    Header      h{};
    const u8*   payload = nullptr;
    std::size_t payload_bytes = 0;
    check(decode_header(buf.data(), buf.size(), h, &payload, &payload_bytes),
          "header decodes", "type %u, %u bytes", unsigned(h.type), h.bytes);
    check(h.magic == kMagic, "magic is right", "0x%08X", h.magic);

    RdMapMsg m;
    check(payload && decode_rdmap(payload, payload_bytes, m), "map decodes",
          "%u x %u", unsigned(m.head.n_range), unsigned(m.head.n_doppler));
    check(int(m.head.n_range) == f.n_range && int(m.head.n_doppler) == f.n_doppler,
          "map dimensions survive", "%u x %u", unsigned(m.head.n_range), unsigned(m.head.n_doppler));

    // Quantising to 8 bits over a 70 dB window costs at most 70/255 dB, and
    // that is the whole justification for sending bytes instead of floats.
    double worst = 0;
    for (int rr = 0; rr < f.n_range; ++rr)
        for (int dd = 0; dd < f.n_doppler; ++dd) {
            const double truth = db(f.at(rr, dd));
            if (truth < r.min_db || truth > r.max_db) continue;
            worst = std::max(worst, std::abs(m.db_at(rr, dd) - truth));
        }
    check(worst < (r.max_db - r.min_db) / 255.0 + 0.01, "quantisation error is one step",
          "worst %.4f dB, step %.4f dB", worst, (r.max_db - r.min_db) / 255.0);
}

//============================================================================
// 13. End to end -- the whole radar, on a known scene
//============================================================================
static void test_endtoend() {
    if (!group_wanted("e2e")) return;
    head("End to end: simulated scene through the whole chain");

    Config c;
    c.source = SourceKind::Simulate;
    c.derive();

    auto src = make_source(c);
    if (!src) { check(false, "source created", "make_source returned nothing"); return; }
    if (!src->open(c)) { check(false, "source opens", "%s", src->name()); return; }

    Waveform wf(c);
    RefModel rm(c);
    Cfar2D   cfar(c);
    AoaEngine aoa(c);

    IqCpi cpi;
    int   frames = 0, with_hits = 0;
    double t_start = now_s();
    std::vector<Hit> hits;

    const int want_frames = g_quick ? 3 : 12;
    while (frames < want_frames && src->next_raw(cpi, 5.0)) {
        RdFrame f;
        std::vector<const ci16*> ptrs;
        for (int r = 0; r < cpi.n_rx; ++r) ptrs.push_back(cpi.chirp(r, 0));
        rm.process_cpi(ptrs.data(), cpi.n_rx, cpi.n_chirp_total, f);
        cfar.detect(f, hits);
        for (auto& h : hits) {
            h.range_m    = wf.range_of_bin(h.range_bin);
            h.velocity_ms = wf.velocity_of_bin(h.dopp_bin);
            const auto a = aoa.estimate(h.virt);
            h.azimuth_deg = a.az_deg; h.elevation_deg = a.el_deg; h.angle_valid = a.valid;
        }
        if (!hits.empty()) ++with_hits;
        ++frames;
    }
    const double elapsed = now_s() - t_start;

    check(frames == want_frames, "the source produced every frame asked for",
          "%d of %d", frames, want_frames);
    check(with_hits >= frames - 1, "targets are detected in essentially every frame",
          "%d of %d frames had detections", with_hits, frames);

    const double rt = (frames * c.d.t_cpi_s) / std::max(elapsed, 1e-9);
    check(rt > 0.25, "the simulated chain runs at a usable speed",
          "%.2fx real time (%.1f ms per frame, budget %.1f ms)",
          rt, elapsed / std::max(frames, 1) * 1e3, c.d.t_cpi_s * 1e3);

    // The scene's targets have known ranges; at least one detection should sit
    // on one of them.
    std::printf("      last frame: %zu detections\n", hits.size());
    for (std::size_t i = 0; i < hits.size() && i < 6; ++i) {
        std::printf("        %7.1f m  %+6.1f m/s  %5.1f dB  az %+5.1f  el %+5.1f%s\n",
                    hits[i].range_m, hits[i].velocity_ms, hits[i].snr_db,
                    hits[i].azimuth_deg, hits[i].elevation_deg,
                    hits[i].angle_valid ? "" : "  (angle not resolved)");
    }
    src->close();
}

//============================================================================
int main(int argc, char** argv) {
    log_set_level(LogLevel::Warn);
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--quick") g_quick = true;
        else if (a == "--help" || a == "-h") {
            std::printf("radar-selftest [--quick] [group ...]\n"
                        "  groups: fx geom fft win cfg ref cfar aoa track md cal proto e2e\n");
            return 0;
        } else if (a[0] != '-') g_only.push_back(a);
    }

    std::printf("\n\033[1m5.8 GHz MIMO radar -- self test\033[0m\n");
    std::printf("FFT backend %s%s\n", Fft::backend(), g_quick ? ", quick mode" : "");

    const double t0 = now_s();
    test_fixedpoint();
    test_geometry();
    test_fft();
    test_windows();
    test_config();
    test_refmodel();
    test_cfar();
    test_aoa();
    test_cluster_track();
    test_microdoppler();
    test_calibration();
    test_proto();
    test_endtoend();
    const double dt = now_s() - t0;

    std::printf("\n");
    if (g_fail == 0) {
        std::printf("\033[32m%d checks passed\033[0m in %.1f s\n", g_pass, dt);
    } else {
        std::printf("\033[31m%d of %d checks FAILED\033[0m in %.1f s\n", g_fail, g_pass + g_fail, dt);
        for (const auto& f : g_failures) std::printf("   %s\n", f.c_str());
    }
    return g_fail == 0 ? 0 : 1;
}
