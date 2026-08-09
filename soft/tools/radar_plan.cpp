//============================================================================
// radar-plan -- choose the operating point, and show the reasoning
//
// Six things fight each other on this hardware, and picking a waveform by
// intuition gets at most three of them right:
//
//   1. Sweep bandwidth sets range resolution and is capped by the AD9361's
//      analogue filter and by the sample rate.
//   2. Sample rate sets how much the FPGA has to chew and, if raw IQ ever
//      crossed USB, would cap everything -- it does not here, because the
//      chirp and the de-chirp both live in the fabric.
//   3. Chirp length sets energy on target, and with it detection range.
//   4. Pulse repetition interval sets unambiguous velocity.
//   5. Chirps per interval set velocity resolution and frame rate, which
//      trade directly against each other.
//   6. The on-chip corner-turn buffer fixes range bins x chirps at 65536,
//      because the XC7K325T has 2.05 MB of block RAM and no external DRAM.
//
// This sweeps the space, applies every constraint, scores what survives
// against the job the radar is for -- finding a small drone -- and prints the
// trade so the choice is arguable rather than asserted.
//
//   radar-plan                      the trade study and the recommendation
//   radar-plan --write radar.json   also write the winning configuration
//   radar-plan --target 0.01 --range 400   optimise for a different job
//============================================================================
#include "radar/config.hpp"
#include "radar/core.hpp"
#include "radar/log.hpp"

#include <algorithm>
#include <cstring>
#include <cstdio>
#include <string>
#include <vector>

using namespace radar;

namespace {

//----------------------------------------------------------------------------
// Hardware limits.  Each is a measured or datasheet fact, not a guess, and
// each is labelled with where it comes from.
//----------------------------------------------------------------------------
struct Hardware {
    double fs_max_hz        = 61.44e6;  // AD9361 master clock on the B210
    double analogue_bw_hz   = 56.0e6;   // AD9361 baseband filter, maximum
    double bw_usable_frac   = 0.90;     // flat portion of that filter
    double tx_power_dbm     = 10.0;     // B210 at 5.8 GHz, measured typical
    double nf_db            = 8.0;      // B210 receive noise figure
    double loss_db          = 3.0;      // cable, connector, mismatch, processing
    int    ct_words_log2    = 16;       // corner-turn buffer, see radar_pkg.svh
    double element_gain_dbi = array_geom::element_gain_dbi;   // measured, openEMS
    double isolation_db     = array_geom::tx_rx_isolation_db; // measured, openEMS
};

struct Job {
    double rcs_m2      = 0.01;   // small quadcopter at 5.8 GHz
    double want_range_m = 400.0;
    double max_speed_ms = 60.0;  // a fast racing drone
    double snr_min_db  = 13.0;   // detection threshold
    double min_fps     = 10.0;   // enough to track something manoeuvring
};

struct Point {
    int    n_sweep = 0, n_pri = 0, n_chirp = 0, n_range = 0;
    double sweep_bw_hz = 0;
    double lna_gain_db = 0;

    // Derived
    double range_res_m = 0, range_max_m = 0;
    double vel_res_ms = 0, vel_max_ms = 0;
    double fps = 0, cpi_s = 0, duty = 0;
    double det_range_m = 0;
    double score = 0;
    std::string reject;
};

double detection_range_m(const Hardware& hw, const Job& job, double cpi_s, double nf_db,
                         double tx_dbm) {
    // The standard radar equation with coherent integration gain.  MIMO gain
    // is n_tx * n_rx because every transmitter is heard by every receiver and
    // the virtual channels combine coherently.
    const double lambda   = array_geom::lambda_m;
    const double mimo_db  = 10.0 * std::log10(double(array_geom::n_virt));
    const double gains_db = 2.0 * hw.element_gain_dbi + mimo_db;
    const double pt_w     = std::pow(10.0, tx_dbm / 10.0) / 1e3;

    const double num = pt_w * std::pow(10.0, gains_db / 10.0) * lambda * lambda
                     * job.rcs_m2 * cpi_s;
    const double den = std::pow(4.0 * kPi, 3.0) * phys::k_boltz * phys::T0
                     * std::pow(10.0, (nf_db + hw.loss_db + job.snr_min_db) / 10.0);
    return std::pow(num / den, 0.25);
}

double cascade_nf_db(double lna_nf_db, double lna_gain_db, double back_nf_db) {
    if (lna_gain_db <= 0.0) return back_nf_db;
    const double f1 = std::pow(10.0, lna_nf_db / 10.0);
    const double g1 = std::pow(10.0, lna_gain_db / 10.0);
    const double f2 = std::pow(10.0, back_nf_db / 10.0);
    return 10.0 * std::log10(f1 + (f2 - 1.0) / g1);
}

void evaluate(Point& p, const Hardware& hw, const Job& job) {
    const double fs      = hw.fs_max_hz;
    const double lambda  = array_geom::lambda_m;
    const int    decim   = 4;
    const int    nfft    = 1024;

    p.cpi_s       = double(p.n_chirp * 2) * p.n_pri / fs;   // TDM: two per position
    p.fps         = 1.0 / p.cpi_s;
    p.duty        = double(p.n_sweep) / p.n_pri;
    p.range_res_m = phys::c0 / (2.0 * p.sweep_bw_hz);

    const double t_sweep = p.n_sweep / fs;
    const double slope   = p.sweep_bw_hz / t_sweep;
    const double fs_dec  = fs / decim;
    const double bin_hz  = fs_dec / nfft;
    const double bin_m   = bin_hz * phys::c0 / (2.0 * slope);
    p.range_max_m        = p.n_range * bin_m;

    const double pri_eff = 2.0 * p.n_pri / fs;              // TDM
    p.vel_max_ms         = lambda / (4.0 * pri_eff);
    p.vel_res_ms         = lambda / (2.0 * p.cpi_s);

    // The receive amplifier the boards were designed around. 15 dB is the most
    // that fits inside the measured -41.1 dB antenna isolation at +10 dBm
    // transmit without the radio compressing.
    const double nf = cascade_nf_db(1.0, p.lna_gain_db, hw.nf_db);
    p.det_range_m   = detection_range_m(hw, job, p.cpi_s, nf, hw.tx_power_dbm);

    //-- Hard constraints -------------------------------------------------
    if (p.sweep_bw_hz > hw.analogue_bw_hz * hw.bw_usable_frac)
        p.reject = "sweep wider than the AD9361 filter passes flat";
    else if (p.n_sweep >= p.n_pri)
        p.reject = "no time between chirps to retrace and switch transmitter";
    else if (p.duty > 0.85)
        p.reject = "duty cycle leaves no settling time";
    else if ((p.n_sweep / decim) > nfft)
        p.reject = "decimated sweep does not fit the range transform";
    else if (p.n_range > nfft / 2)
        p.reject = "range bins beyond the transform's unambiguous half";
    else if (p.n_range * (p.n_chirp * 2) != (1 << hw.ct_words_log2))
        p.reject = "corner-turn buffer is not exactly 65536 words";
    else if (p.vel_max_ms < job.max_speed_ms)
        p.reject = "unambiguous velocity below the fastest target of interest";
    else if (p.fps < job.min_fps)
        p.reject = "frame rate too low to track a manoeuvring target";
    else if (p.range_max_m < job.want_range_m)
        p.reject = "range coverage short of the requirement";

    if (!p.reject.empty()) { p.score = -1e9; return; }

    //-- Score.  Everything is expressed as "how many times better than the
    //   requirement", so no unit dominates by accident.
    const double reach = p.det_range_m / job.want_range_m;
    const double res   = 10.0 / p.range_res_m;              // 10 m is adequate
    const double vres  = 2.0 / p.vel_res_ms;                // 2 m/s is adequate
    const double rate  = p.fps / 25.0;                      // 25 fps is plenty
    const double cover = std::min(2.0, p.range_max_m / job.want_range_m);

    // Detection range dominates: a radar that cannot see the target has no use
    // for its resolution. Frame rate saturates -- past about 25 a second it
    // stops buying anything for a target that manoeuvres on a timescale of
    // tenths of a second.
    p.score = 3.0 * std::log(reach) + 1.0 * std::log(res) + 0.7 * std::log(vres)
            + 0.5 * std::log(std::min(1.0, rate) + 1e-9) + 0.4 * std::log(cover);
}

void print_row(const Point& p, bool best) {
    std::printf("  %c %5.1f %5d %5d %5d %4d  %6.2f %7.0f %6.2f %6.0f %6.1f %8.0f",
                best ? '*' : ' ',
                p.sweep_bw_hz / 1e6, p.n_sweep, p.n_pri, p.n_chirp, p.n_range,
                p.range_res_m, p.range_max_m, p.vel_res_ms, p.vel_max_ms,
                p.fps, p.det_range_m);
    if (!p.reject.empty()) std::printf("   x %s", p.reject.c_str());
    std::printf("\n");
}

} // namespace

int main(int argc, char** argv) {
    log_set_level(LogLevel::Info);

    Hardware hw;
    Job      job;
    std::string write_path;
    bool        show_rejected = false;
    double      lna_gain_db   = 15.0;

    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto next = [&](double d) { return (i + 1 < argc) ? std::atof(argv[++i]) : d; };
        if      (a == "--write" && i + 1 < argc) write_path = argv[++i];
        if      (a == "--target")   job.rcs_m2       = next(job.rcs_m2);
        else if (a == "--range")    job.want_range_m = next(job.want_range_m);
        else if (a == "--speed")    job.max_speed_ms = next(job.max_speed_ms);
        else if (a == "--fps")      job.min_fps      = next(job.min_fps);
        else if (a == "--lna")      lna_gain_db      = next(lna_gain_db);
        else if (a == "--all")      show_rejected    = true;
        else if (a == "--help" || a == "-h") {
            std::printf(
                "radar-plan -- choose the radar's operating point\n\n"
                "  --target <m2>    radar cross-section to design for   (default 0.01)\n"
                "  --range  <m>     range that must be covered          (default 400)\n"
                "  --speed  <m/s>   fastest target to stay unambiguous  (default 60)\n"
                "  --fps    <n>     minimum frames per second           (default 10)\n"
                "  --lna    <dB>    receive amplifier gain fitted       (default 15)\n"
                "  --all            show rejected points and why\n"
                "  --write  <file>  write the winning configuration\n");
            return 0;
        }
    }

    std::printf("\n5.8 GHz MIMO radar -- operating point trade study\n");
    std::printf("================================================\n\n");
    std::printf("Hardware, all measured or datasheet:\n");
    std::printf("  sample rate ceiling      %.2f MSps   AD9361 master clock\n", hw.fs_max_hz / 1e6);
    std::printf("  flat analogue bandwidth  %.1f MHz    %.0f%% of the %.0f MHz filter\n",
                hw.analogue_bw_hz * hw.bw_usable_frac / 1e6, hw.bw_usable_frac * 100,
                hw.analogue_bw_hz / 1e6);
    std::printf("  element gain             %.1f dBi    openEMS, both elements present\n",
                hw.element_gain_dbi);
    std::printf("  transmit-receive isolation %.1f dB   openEMS, whole board\n", hw.isolation_db);
    std::printf("  transmit power           %+.0f dBm    B210 at 5.8 GHz\n", hw.tx_power_dbm);
    std::printf("  receive noise figure     %.1f dB     %.2f dB with the %.0f dB amplifier\n",
                hw.nf_db, cascade_nf_db(1.0, lna_gain_db, hw.nf_db), lna_gain_db);
    std::printf("  corner-turn buffer       %d words   fixes range bins x chirps\n",
                1 << hw.ct_words_log2);
    std::printf("\nJob: find a %.3f m2 target out to %.0f m, up to %.0f m/s, at %.0f fps or better\n\n",
                job.rcs_m2, job.want_range_m, job.max_speed_ms, job.min_fps);

    //-- Sweep the space --------------------------------------------------
    std::vector<Point> pts;
    for (double bw_mhz : {20.0, 25.0, 30.0, 40.0, 50.0}) {
        for (int n_sweep : {1024, 1536, 2048, 3072, 4096, 6144}) {
            for (int n_pri : {n_sweep + 256, n_sweep + 768, n_sweep + 1024, 2 * n_sweep}) {
                for (int shift = 7; shift <= 9; ++shift) {
                    Point p;
                    p.sweep_bw_hz = bw_mhz * 1e6;
                    p.n_sweep     = n_sweep;
                    p.n_pri       = n_pri;
                    p.n_chirp     = 1 << (shift - 1);           // per transmitter
                    p.n_range     = 1 << (hw.ct_words_log2 - shift);
                    p.lna_gain_db = lna_gain_db;
                    if (p.n_range > 512) continue;
                    evaluate(p, hw, job);
                    pts.push_back(p);
                }
            }
        }
    }

    std::stable_sort(pts.begin(), pts.end(),
                     [](const Point& a, const Point& b) { return a.score > b.score; });

    std::printf("    sweep  n_swp  n_pri  chirp rng   res_m  cover_m  dv_m/s  vmax    fps   detect_m\n");
    std::printf("    -----  -----  -----  ----- ---   -----  -------  ------  ----    ---   --------\n");

    int shown = 0;
    for (const auto& p : pts) {
        if (!p.reject.empty() && !show_rejected) continue;
        if (++shown > (show_rejected ? 40 : 12)) break;
        print_row(p, shown == 1 && p.reject.empty());
    }

    const Point* best = nullptr;
    for (const auto& p : pts) if (p.reject.empty()) { best = &p; break; }

    if (!best) {
        std::printf("\nNothing in the space satisfies that job. The binding constraint is usually\n"
                    "detection range: relax --range, raise --lna, or fit the transmit amplifier.\n");
        return 1;
    }

    std::printf("\nRecommended operating point\n");
    std::printf("---------------------------\n");
    std::printf("  sweep bandwidth        %.1f MHz\n", best->sweep_bw_hz / 1e6);
    std::printf("  chirp                  %d samples, %.1f us, %.1f%% duty\n",
                best->n_sweep, best->n_sweep / hw.fs_max_hz * 1e6, best->duty * 100);
    std::printf("  repetition interval    %d samples, %.1f us\n",
                best->n_pri, best->n_pri / hw.fs_max_hz * 1e6);
    std::printf("  chirps per transmitter %d   (%d per coherent interval, TDM)\n",
                best->n_chirp, best->n_chirp * 2);
    std::printf("  range bins             %d\n", best->n_range);
    std::printf("\n  range resolution       %.2f m\n", best->range_res_m);
    std::printf("  range covered          %.0f m\n", best->range_max_m);
    std::printf("  velocity resolution    %.2f m/s\n", best->vel_res_ms);
    std::printf("  velocity unambiguous   +/- %.0f m/s\n", best->vel_max_ms);
    std::printf("  frame rate             %.1f per second  (%.1f ms interval)\n",
                best->fps, best->cpi_s * 1e3);
    std::printf("  detection range        %.0f m on a %.3f m2 target\n",
                best->det_range_m, job.rcs_m2);

    //-- What the choice cost, stated plainly -----------------------------
    std::printf("\nWhat the alternatives would have given up\n");
    std::printf("-----------------------------------------\n");
    for (const auto& p : pts) {
        if (!p.reject.empty()) continue;
        if (&p == best) continue;
        if (p.n_range == best->n_range && std::abs(p.sweep_bw_hz - best->sweep_bw_hz) < 1e3) continue;
        std::printf("  %.0f MHz / %d bins / %d chirps: ", p.sweep_bw_hz / 1e6, p.n_range, p.n_chirp);
        std::printf("%+.0f%% detection range, %+.0f%% range resolution, %+.0f%% frame rate\n",
                    100.0 * (p.det_range_m / best->det_range_m - 1.0),
                    100.0 * (best->range_res_m / p.range_res_m - 1.0),
                    100.0 * (p.fps / best->fps - 1.0));
        static int n = 0;
        if (++n >= 5) break;
    }

    //-- Write it out -----------------------------------------------------
    if (!write_path.empty()) {
        Config c;
        c.sweep_bw_hz = best->sweep_bw_hz;
        c.n_sweep     = best->n_sweep;
        c.n_pri       = best->n_pri;
        c.n_chirp     = best->n_chirp;
        c.n_range     = best->n_range;
        c.n_doppler   = best->n_chirp;
        c.derive();
        const std::string bad = c.validate();
        if (!bad.empty()) {
            std::printf("\nthe winning point does not validate: %s\n", bad.c_str());
            return 1;
        }
        save_config(c, write_path);
        std::printf("\nwrote %s\n", write_path.c_str());
    } else {
        std::printf("\n(--write <file> to save this as a configuration)\n");
    }
    return 0;
}
