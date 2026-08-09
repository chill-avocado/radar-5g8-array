//============================================================================
// radar-bench -- where the time actually goes
//
// The coherent processing interval is 16 ms.  Everything the host does has to
// fit inside that, on four cores at 1.4 GHz base, or frames get dropped and
// every measurement taken during the run becomes provisional.  This measures
// each stage against that budget rather than reporting abstract throughput.
//
// It also times all three FFT backends, so the one the build chose can be
// justified with a number instead of an assumption.
//
//   radar-bench             everything
//   radar-bench --fft       only the transform comparison
//   radar-bench --repeat 20 more averaging
//============================================================================
#include "radar/aoa.hpp"
#include "radar/cfar.hpp"
#include "radar/cluster.hpp"
#include "radar/config.hpp"
#include "radar/core.hpp"
#include "radar/fft.hpp"
#include "radar/log.hpp"
#include "radar/refmodel.hpp"
#include "radar/thread.hpp"
#include "radar/track.hpp"
#include "radar/types.hpp"
#include "radar/waveform.hpp"
#include "radar/window.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <random>
#include <string>
#include <vector>

using namespace radar;

namespace {

double now_s() {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

/// Median of several runs. The mean is the wrong statistic on a laptop where
/// one run in ten collides with something else the machine is doing.
template <typename F>
double time_median(int reps, F&& fn) {
    std::vector<double> t;
    t.reserve(reps);
    for (int i = 0; i < reps; ++i) {
        const double a = now_s();
        fn();
        t.push_back(now_s() - a);
    }
    std::sort(t.begin(), t.end());
    return t[t.size() / 2];
}

double g_budget_ms = 16.0;

void row(const char* stage, double ms, const char* note = "") {
    const double frac = ms / g_budget_ms * 100.0;
    const char*  col  = frac > 80 ? "\033[31m" : frac > 40 ? "\033[33m" : "\033[32m";
    std::printf("  %-34s %8.3f ms  %s%5.1f%%\033[0m of the frame   %s\n",
                stage, ms, col, frac, note);
}

} // namespace

int main(int argc, char** argv) {
    log_set_level(LogLevel::Warn);
    int  reps    = 7;
    bool fft_only = false;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if      (a == "--fft")    fft_only = true;
        else if (a == "--repeat" && i + 1 < argc) reps = std::atoi(argv[++i]);
        else if (a == "--help") {
            std::printf("radar-bench [--fft] [--repeat n]\n");
            return 0;
        }
    }

    Config c;
    c.derive();
    g_budget_ms = c.d.t_cpi_s * 1e3;

    std::printf("\n\033[1m5.8 GHz MIMO radar -- timing\033[0m\n");
    std::printf("%d physical cores, frame budget %.1f ms (%.1f frames/s)\n",
                rt::physical_cores(), g_budget_ms, c.d.frame_rate_hz);
    std::printf("map %d range x %d Doppler, %d chirps per interval, %d samples per chirp\n\n",
                c.n_range, c.n_doppler, c.d.n_chirp_total, c.n_sweep);

    //-- Transform backends ------------------------------------------------
    std::printf("\033[1mTransform, %s backend\033[0m\n", Fft::backend());
    {
        std::mt19937 g(1);
        std::uniform_real_distribution<float> u(-1, 1);
        for (int n : {128, 256, 1024, 4096}) {
            const int batch = std::max(1, 65536 / n);
            std::vector<cf32> buf(std::size_t(n) * batch);
            for (auto& x : buf) x = cf32(u(g), u(g));
            Fft f(n, false);
            const double t = time_median(reps, [&] { f.run(buf.data(), batch); });
            const double per = t / batch * 1e6;
            // 5 N log2 N is the usual flop count for a complex transform.
            const double gflops = 5.0 * n * std::log2(double(n)) * batch / t / 1e9;
            std::printf("  n = %5d   %8.2f us each   %6.2f GFLOP/s   (%d in a batch)\n",
                        n, per, gflops, batch);
        }
    }
    if (fft_only) { std::printf("\n"); return 0; }

    //-- The datapath the FPGA will own ------------------------------------
    std::printf("\n\033[1mWhat the FPGA takes off the host\033[0m\n");
    std::printf("  These are the stages that move to the fabric. The host only pays\n");
    std::printf("  them in simulation and replay.\n");
    {
        Waveform wf(c);
        RefModel rm(c);
        const int nsw = c.n_sweep, nct = c.d.n_chirp_total;

        std::mt19937 g(2);
        std::uniform_int_distribution<int> ui(-8000, 8000);
        std::vector<std::vector<ci16>> rx(2, std::vector<ci16>(std::size_t(nct) * nsw));
        for (auto& v : rx) for (auto& s : v) s = ci16(i16(ui(g)), i16(ui(g)));

        std::vector<ci16> one_out(c.n_range_fft);
        const double t_chirp = time_median(reps * 4, [&] {
            for (int k = 0; k < 32; ++k) rm.range_chirp(rx[0].data(), nsw, one_out.data());
        }) / 32.0;
        char note[96];
        std::snprintf(note, sizeof(note), "%.1f us per chirp x %d chirps x 2 receivers",
                      t_chirp * 1e6, nct);
        row("de-chirp + decimate + range FFT", t_chirp * 1e3 * nct * 2, note);

        RdFrame f;
        const ci16* ptrs[2] = {rx[0].data(), rx[1].data()};
        const double t_cpi = time_median(std::max(3, reps / 2), [&] {
            rm.process_cpi(ptrs, 2, nct, f);
        });
        row("whole interval, fixed point", t_cpi * 1e3, "this is what simulation costs");
    }

    //-- What stays on the host --------------------------------------------
    std::printf("\n\033[1mWhat the host keeps\033[0m\n");
    {
        RdFrame f;
        f.allocate(c.n_range, c.n_doppler, 4, false);
        std::mt19937 g(3);
        std::exponential_distribution<double> ex(1.0);
        for (std::size_t i = 0; i < f.power.size(); ++i) f.power[i] = float(ex(g));
        for (int k = 0; k < 24; ++k) f.at(20 + 9 * k, 40 + (k % 30)) += 400.0f;

        std::vector<Hit> hits;
        for (auto kind : {CfarKind::Ca, CfarKind::Go, CfarKind::Os}) {
            Config cc = c; cc.cfar_kind = kind; cc.derive();
            Cfar2D cf(cc);
            const double t = time_median(reps, [&] { cf.detect(f, hits); });
            const char* nm = kind == CfarKind::Ca ? "2D CFAR, cell averaging"
                           : kind == CfarKind::Go ? "2D CFAR, greatest of"
                                                  : "2D CFAR, ordered statistic";
            char note[64];
            std::snprintf(note, sizeof(note), "%zu detections", hits.size());
            row(nm, t * 1e3, note);
        }

        Config cc = c; cc.cfar_kind = CfarKind::Ca; cc.derive();
        Cfar2D(cc).detect(f, hits);
        std::mt19937 g2(4);
        std::uniform_real_distribution<float> ur(-1, 1);
        for (auto& h : hits) for (auto& v : h.virt) v = cf32(ur(g2), ur(g2));

        for (auto m : {AoaMethod::Monopulse, AoaMethod::Bartlett, AoaMethod::Capon, AoaMethod::Music}) {
            Config ca = c; ca.aoa = m; ca.derive();
            AoaEngine eng(ca);
            const double t = time_median(reps, [&] {
                for (auto& h : hits) {
                    const auto r = eng.estimate(h.virt);
                    h.azimuth_deg = r.az_deg; h.elevation_deg = r.el_deg; h.angle_valid = r.valid;
                }
            });
            const char* nm = m == AoaMethod::Monopulse ? "angle, monopulse"
                           : m == AoaMethod::Bartlett  ? "angle, Bartlett"
                           : m == AoaMethod::Capon     ? "angle, Capon"
                                                       : "angle, MUSIC";
            char note[80];
            std::snprintf(note, sizeof(note), "%zu detections, %.1f us each",
                          hits.size(), hits.empty() ? 0.0 : t / hits.size() * 1e6);
            row(nm, t * 1e3, note);
        }

        Clusterer cl(c);
        std::vector<Target> tg;
        row("clustering", time_median(reps, [&] { cl.cluster(hits, tg); }) * 1e3, "");

        Tracker tr(c);
        std::vector<Track> tk;
        row("tracking", time_median(reps, [&] { tr.update(tg, c.d.t_cpi_s, tk); }) * 1e3, "");
    }

    //-- Memory ------------------------------------------------------------
    std::printf("\n\033[1mMemory per frame\033[0m\n");
    {
        const double map   = double(c.n_range) * c.n_doppler * sizeof(float);
        const double cube  = double(c.n_range) * c.n_doppler * 4 * sizeof(cf32);
        const double raw   = double(c.d.n_chirp_total) * c.n_sweep * 2 * sizeof(ci16);
        std::printf("  raw samples for one interval   %7.2f MB\n", raw / 1e6);
        std::printf("  power map                      %7.2f MB\n", map / 1e6);
        std::printf("  complex cube, 4 channels       %7.2f MB\n", cube / 1e6);
        std::printf("  host link, hardware source     %7.2f MB/s\n", c.d.usb_bytes_s / 1e6);
        std::printf("  host link if raw IQ crossed it %7.2f MB/s   <- what the FPGA avoids\n",
                    c.sample_rate_hz * 4 * 4 / 1e6);
    }
    std::printf("\n");
    return 0;
}
