//============================================================================
// config.cpp -- the operating point: what follows from what, and what will
// not work
//
// derive() is the only place in the project that turns sweep widths and clock
// counts into metres and metres per second.  Everything downstream reads
// Config::d rather than doing the arithmetic again, which is why a range scale
// cannot end up different in two places.
//
// validate() is written to be read by whoever is standing at the radio at two
// in the morning.  Each message says what was asked for, why the hardware
// cannot do it, and which number to change -- not merely that something is
// out of range.
//
// register_writes() is the other half of the contract with the fabric.  The
// order matters: reset first, then every parameter, then the window tables,
// then the version stamp, and only then the enable bit.  The core must never
// see a half-programmed configuration.
//============================================================================
#include "radar/config.hpp"

#include "radar/json.hpp"
#include "radar/refmodel.hpp"
#include "radar/waveform.hpp"
#include "radar/window.hpp"

#include <algorithm>
#include <cstdio>
#include <stdexcept>
#include <string>

namespace radar {

namespace {

//----------------------------------------------------------------------------
// Frame geometry, from radar_pkg.svh section 7.
//----------------------------------------------------------------------------
constexpr int kHdrWords  = 8;    // magic, flags, index, sizes, hits, noise, time
constexpr int kHitWords  = 6;    // one detection record
constexpr int kEndWords  = 1;    // the end marker

// The corner-turn buffer, radar_pkg.svh: n_range * n_chirp_total is fixed.
constexpr int kCornerTurnWords = 65536;

// Map decimation.  The full map is 32768 words in every legal split, which the
// host link carries comfortably, so both axes are streamed whole.  The
// register exists so a slow link can be traded against display resolution
// later without touching the gateware.
constexpr u32 kMapDecimRange = 1;
constexpr u32 kMapDecimDopp  = 1;

// The de-chirp product is Q0.15 times Q0.15, so fifteen bits come back off to
// restore unit gain.
constexpr u32 kDechirpShift = 15;

std::string fmt(const char* f, double v) {
    char buf[64];
    std::snprintf(buf, sizeof buf, f, v);
    return buf;
}
std::string fmti(const char* f, long long v) {
    char buf[64];
    std::snprintf(buf, sizeof buf, f, v);
    return buf;
}

const char* mimo_name(MimoMode m) {
    switch (m) {
        case MimoMode::Tdm:     return "tdm";
        case MimoMode::Ddm:     return "ddm";
        case MimoMode::Tx0Only: return "tx0";
        case MimoMode::Tx1Only: return "tx1";
    }
    return "tdm";
}
MimoMode mimo_from(const std::string& s, MimoMode def) {
    if (s == "tdm") return MimoMode::Tdm;
    if (s == "ddm") return MimoMode::Ddm;
    if (s == "tx0" || s == "tx0only") return MimoMode::Tx0Only;
    if (s == "tx1" || s == "tx1only") return MimoMode::Tx1Only;
    return def;
}

const char* cfar_name(CfarKind k) {
    switch (k) {
        case CfarKind::Ca:   return "ca";
        case CfarKind::Go:   return "go";
        case CfarKind::So:   return "so";
        case CfarKind::Os:   return "os";
        case CfarKind::None: return "none";
    }
    return "ca";
}
CfarKind cfar_from(const std::string& s, CfarKind def) {
    if (s == "ca")   return CfarKind::Ca;
    if (s == "go")   return CfarKind::Go;
    if (s == "so")   return CfarKind::So;
    if (s == "os")   return CfarKind::Os;
    if (s == "none" || s == "off") return CfarKind::None;
    return def;
}

WindowKind window_from(const std::string& s, WindowKind def) {
    if (s == "rect" || s == "rectangular") return WindowKind::Rect;
    if (s == "hann" || s == "hanning")     return WindowKind::Hann;
    if (s == "hamming")                    return WindowKind::Hamming;
    if (s == "blackman-harris" || s == "blackmanharris" || s == "bh")
                                           return WindowKind::BlackmanHarris;
    if (s == "taylor")                     return WindowKind::Taylor;
    if (s == "chebyshev" || s == "cheby")  return WindowKind::Chebyshev;
    return def;
}

const char* aoa_name(AoaMethod a) {
    switch (a) {
        case AoaMethod::Monopulse: return "monopulse";
        case AoaMethod::Bartlett:  return "bartlett";
        case AoaMethod::Capon:     return "capon";
        case AoaMethod::Music:     return "music";
    }
    return "music";
}
AoaMethod aoa_from(const std::string& s, AoaMethod def) {
    if (s == "monopulse") return AoaMethod::Monopulse;
    if (s == "bartlett")  return AoaMethod::Bartlett;
    if (s == "capon")     return AoaMethod::Capon;
    if (s == "music")     return AoaMethod::Music;
    return def;
}

const char* source_name(SourceKind s) {
    switch (s) {
        case SourceKind::Simulate: return "simulate";
        case SourceKind::Uhd:      return "uhd";
        case SourceKind::File:     return "file";
    }
    return "simulate";
}
SourceKind source_from(const std::string& s, SourceKind def) {
    if (s == "simulate" || s == "sim") return SourceKind::Simulate;
    if (s == "uhd" || s == "b210" || s == "hardware") return SourceKind::Uhd;
    if (s == "file" || s == "replay") return SourceKind::File;
    return def;
}

/// Training cells in one two-dimensional cell-averaging window.
int cfar_training_cells(const Config& c) {
    const int wr = 2 * (c.guard_range + c.train_range) + 1;
    const int wd = 2 * (c.guard_dopp  + c.train_dopp)  + 1;
    const int gr = 2 * c.guard_range + 1;
    const int gd = 2 * c.guard_dopp  + 1;
    return wr * wd - gr * gd;
}

} // namespace

//============================================================================
// derive
//============================================================================
void Config::derive() {
    const double fs = sample_rate_hz > 0.0 ? sample_rate_hz : 1.0;

    d.lambda_m  = phys::c0 / (centre_freq_hz > 0.0 ? centre_freq_hz : 1.0);
    d.t_sweep_s = double(n_sweep) / fs;
    d.t_pri_s   = double(n_pri)   / fs;
    d.chirp_slope_hz_s = d.t_sweep_s > 0.0 ? sweep_bw_hz / d.t_sweep_s : 0.0;

    d.n_chirp_total = n_chirp * (mimo == MimoMode::Tdm ? 2 : 1);
    d.t_cpi_s       = double(d.n_chirp_total) * d.t_pri_s;

    d.fs_dec_hz   = fs / double(decim > 0 ? decim : 1);
    d.n_sweep_dec = decim > 0 ? n_sweep / decim : n_sweep;

    d.range_res_m = sweep_bw_hz > 0.0 ? phys::c0 / (2.0 * sweep_bw_hz) : 0.0;
    d.range_bin_m = (d.chirp_slope_hz_s > 0.0 && n_range_fft > 0)
                  ? (d.fs_dec_hz / double(n_range_fft)) * phys::c0 / (2.0 * d.chirp_slope_hz_s)
                  : 0.0;
    d.range_max_m = double(n_range) * d.range_bin_m;

    // Time multiplexing means a given transmitter only gets every other slot,
    // so its effective interval between looks is twice the sequencer's.
    const double eff_pri = d.t_pri_s * (mimo == MimoMode::Tdm ? 2.0 : 1.0);
    d.vel_max_ms = eff_pri   > 0.0 ? d.lambda_m / (4.0 * eff_pri)   : 0.0;
    d.vel_res_ms = d.t_cpi_s > 0.0 ? d.lambda_m / (2.0 * d.t_cpi_s) : 0.0;
    d.frame_rate_hz = d.t_cpi_s > 0.0 ? 1.0 / d.t_cpi_s : 0.0;

    d.n_virt = array_geom::n_virt;

    // What actually crosses the link, from the packet layout in section 7 of
    // radar_pkg.svh: a fixed header, the decimated power map, one record per
    // reported detection, and the end marker.
    const int nr_out = int(std::max<u32>(1, u32(n_range)   / kMapDecimRange));
    const int nd_out = int(std::max<u32>(1, u32(n_doppler) / kMapDecimDopp));
    const double words = double(kHdrWords)
                       + double(nr_out) * double(nd_out)
                       + double(max_hits) * kHitWords
                       + double(kEndWords);
    d.usb_bytes_s = 4.0 * words * d.frame_rate_hz;
}

//============================================================================
// validate
//============================================================================
std::string Config::validate() const {
    Config t = *this;
    t.derive();

    if (sweep_bw_hz > sample_rate_hz) {
        return "The sweep is " + fmt("%.2f", sweep_bw_hz / 1e6) +
               " MHz wide but the radio only samples at " + fmt("%.2f", sample_rate_hz / 1e6) +
               " MHz, so the ends of the sweep would fold back on top of the middle. "
               "Narrow the sweep to " + fmt("%.2f", sample_rate_hz / 1e6) +
               " MHz or less, or run the radio faster.";
    }
    if (n_sweep > n_pri) {
        return "Each sweep lasts " + fmti("%lld", n_sweep) +
               " radio clocks but a new one starts every " + fmti("%lld", n_pri) +
               ", so a sweep would begin before the one before it had finished. "
               "Give the sweeps at least " + fmti("%lld", n_sweep) + " clocks apart.";
    }
    if (!is_pow2(std::size_t(n_range_fft))) {
        return "The range transform is set to " + fmti("%lld", n_range_fft) +
               " points, and the hardware only does powers of two. Use " +
               fmti("%lld", 1LL << log2i(std::size_t(n_range_fft))) + " instead.";
    }
    if (!is_pow2(std::size_t(n_doppler))) {
        return "The Doppler transform is set to " + fmti("%lld", n_doppler) +
               " points, and the hardware only does powers of two.";
    }
    if (t.d.n_sweep_dec > n_range_fft) {
        return "After decimating by " + fmti("%lld", decim) + " a sweep is " +
               fmti("%lld", t.d.n_sweep_dec) + " samples long, which will not fit in a " +
               fmti("%lld", n_range_fft) + "-point range transform. Either enlarge the "
               "transform, decimate harder, or shorten the sweep.";
    }
    if (n_range > n_range_fft / 2) {
        return "Keeping " + fmti("%lld", n_range) + " range bins from a " +
               fmti("%lld", n_range_fft) + "-point transform reaches past the halfway "
               "point, where the spectrum turns back on itself and every bin is a mirror "
               "of a lower one. Keep at most " + fmti("%lld", n_range_fft / 2) + ".";
    }

    // Ahead of the corner-turn arithmetic, because an odd chirp count fails
    // that too and this says the useful thing about why.
    if (mimo == MimoMode::Ddm && (n_chirp & 1)) {
        return "Doppler-division needs an even number of chirps, because transmitter 1 is "
               "inverted on every other one and an odd count leaves the pattern unbalanced. " +
               fmti("%lld", n_chirp) + " is odd.";
    }

    // The one the whole operating point is built around.
    if (!is_pow2(std::size_t(n_range)) || !is_pow2(std::size_t(t.d.n_chirp_total))) {
        return "Range bins and chirps per interval both have to be powers of two, because "
               "the corner-turn buffer addresses them with a shift rather than a multiplier. "
               + fmti("%lld", n_range) + " by " + fmti("%lld", t.d.n_chirp_total) + " is not.";
    }
    if (n_range * t.d.n_chirp_total != kCornerTurnWords) {
        const double mb = double(n_range) * double(t.d.n_chirp_total) * 2.0 * 4.0 * 2.0 / 1048576.0;
        return fmti("%lld", n_range) + " range bins by " + fmti("%lld", t.d.n_chirp_total) +
               " chirps needs " + fmt("%.2f", mb) + " MB of on-chip memory to turn the corner, "
               "and the whole device has 2.05 MB. The product has to be exactly " +
               fmti("%lld", kCornerTurnWords) + ": halve one of them and double the other. "
               "256 by 256 gives 576 m of range at 62.5 maps a second, 128 by 512 gives "
               "288 m at 31.2 maps a second with twice the Doppler resolution.";
    }

    const int cfar_r = 2 * (guard_range + train_range) + 1;
    const int cfar_d = 2 * (guard_dopp  + train_dopp)  + 1;
    if (cfar_r > n_range || cfar_d > n_doppler) {
        return "The detection window is " + fmti("%lld", cfar_r) + " bins across in range and " +
               fmti("%lld", cfar_d) + " in Doppler, which does not fit inside a map that is only " +
               fmti("%lld", n_range) + " by " + fmti("%lld", n_doppler) +
               ". Shrink the guard or training cells.";
    }
    if (t.d.vel_max_ms < 40.0) {
        return "Anything moving faster than " + fmt("%.1f", t.d.vel_max_ms) +
               " m/s would be reported at the wrong speed, and a small drone does better than "
               "that. Shorten the interval between sweeps, or drop out of time-multiplexed "
               "transmit, to widen the unambiguous span past 40 m/s.";
    }
    if (t.d.frame_rate_hz < 5.0) {
        return "At " + fmt("%.2f", t.d.frame_rate_hz) + " maps a second the picture updates "
               "roughly every " + fmt("%.2f", 1.0 / std::max(1e-9, t.d.frame_rate_hz)) +
               " seconds, which is too slow to hold a track on anything that manoeuvres. "
               "Use fewer chirps per interval, or a shorter interval.";
    }
    return std::string();
}

//============================================================================
// register_writes
//============================================================================
std::vector<std::pair<u8, u32>> Config::register_writes() const {
    Config t = *this;
    t.derive();

    std::vector<std::pair<u8, u32>> w;
    auto add = [&w](u8 addr, u32 data) { w.emplace_back(addr, data); };

    // Hold the core in reset while everything else is programmed.
    add(0, 1u << 1);

    add(1, u32(nco_freq_start_inc(t)));
    add(2, u32(nco_freq_slope_inc(t)));
    add(3, u32(t.n_sweep)  & 0xFFFFu);
    add(4, u32(t.n_pri)    & 0xFFFFu);
    add(5, u32(t.n_chirp)  & 0xFFFFu);
    add(6, t.tx_enable ? 32767u : 0u);          // Q0.15 digital transmit scale
    add(7, kDechirpShift);

    const std::vector<i16> rwin = range_window_table(t);
    const std::vector<i16> dwin = dopp_window_table(t);
    add(8, fft_scale_word(fft_scale_stages(rwin, t.n_range_fft)));
    add(9, fft_scale_word(fft_scale_stages(dwin, t.n_doppler)));

    // Detection.  The register carries two bits of kind: cell averaging,
    // greatest-of, smallest-of, and pass-everything.  Ordered statistic has no
    // fabric implementation -- it needs a sort -- so it is programmed as
    // pass-everything and run on the host over the streamed map.
    u32 kind = 0;
    switch (t.cfar_kind) {
        case CfarKind::Ca:   kind = 0; break;
        case CfarKind::Go:   kind = 1; break;
        case CfarKind::So:   kind = 2; break;
        case CfarKind::Os:   kind = 3; break;
        case CfarKind::None: kind = 3; break;
    }
    const u32 cfar_cfg = (u32(t.guard_range) & 0xFu)
                       | ((u32(t.guard_dopp)  & 0xFu) << 4)
                       | ((u32(t.train_range) & 0xFu) << 8)
                       | ((u32(t.train_dopp)  & 0xFu) << 12)
                       | ((kind & 0x3u) << 16);
    add(12, cfar_cfg);

    // Threshold multiplier for a cell-averaging detector: with N training
    // cells, a false-alarm rate of pfa needs a threshold of
    // N * (pfa^(-1/N) - 1) times the training mean.  Written as Q16.16.
    u32 alpha_q = 0;
    if (t.cfar_kind != CfarKind::None) {
        const int n = cfar_training_cells(t);
        if (n > 0 && t.pfa > 0.0 && t.pfa < 1.0) {
            const double a = double(n) * (std::pow(t.pfa, -1.0 / double(n)) - 1.0);
            const double q = std::floor(a * 65536.0 + 0.5);
            alpha_q = u32(clampv(q, 0.0, 4294967295.0));
        }
    }
    add(13, alpha_q);

    add(14, u32(t.range_zero_bin)   & 0xFFFFu);
    add(15, (kMapDecimDopp << 8) | kMapDecimRange);
    add(16, u32(t.max_hits)         & 0xFFFFu);
    add(17, u32(t.zero_dopp_blank)  & 0xFFu);

    // Which way the fixed corner-turn budget was split this time.
    const u32 geom = (u32(log2i(std::size_t(t.n_range))) & 0xFu)
                   | ((u32(log2i(std::size_t(t.d.n_chirp_total))) & 0xFu) << 4);
    add(18, geom);

    add(20, 0u);                                 // loopback test tone, off

    // Window coefficients.  One address write and one data write per entry;
    // the Doppler table rides in the top half of the word and the range table
    // in the bottom, and whichever table is shorter is padded with zeros.
    const std::size_t wn = std::max(rwin.size(), dwin.size());
    for (std::size_t i = 0; i < wn; ++i) {
        const u32 rv = u32(u16(i < rwin.size() ? rwin[i] : 0));
        const u32 dv = u32(u16(i < dwin.size() ? dwin[i] : 0));
        add(10, u32(i));
        add(11, (dv << 16) | rv);
    }

    add(19, 1u);                                 // stamp the format version

    u32 ctrl = 1u                                // enable
             | ((u32(int(t.mimo)) & 0x3u) << 2)
             | ((t.tx_enable ? 1u : 0u) << 4)
             | (1u << 5)                         // stream the map
             | (1u << 6);                        // stream the detections
    add(0, ctrl);                                // free running: frame_limit 0

    return w;
}

//============================================================================
// Files
//============================================================================
Config load_config(const std::string& path) {
    std::string err;
    const Json j = Json::parse_file(path, &err);
    if (!err.empty()) throw std::runtime_error("cannot read " + path + ": " + err);

    Config c;
    c.source          = source_from(j["source"].str(source_name(c.source)), c.source);
    c.device_args     = j["device_args"].str(c.device_args);
    c.centre_freq_hz  = j["centre_freq_hz"].num(c.centre_freq_hz);
    c.sample_rate_hz  = j["sample_rate_hz"].num(c.sample_rate_hz);
    c.tx_gain_db      = j["tx_gain_db"].num(c.tx_gain_db);
    c.rx_gain_db      = j["rx_gain_db"].num(c.rx_gain_db);
    c.rx_bandwidth_hz = j["rx_bandwidth_hz"].num(c.rx_bandwidth_hz);
    c.clock_source    = j["clock_source"].str(c.clock_source);
    c.time_source     = j["time_source"].str(c.time_source);

    c.sweep_bw_hz = j["sweep_bw_hz"].num(c.sweep_bw_hz);
    c.n_sweep     = j["n_sweep"].integer(c.n_sweep);
    c.n_pri       = j["n_pri"].integer(c.n_pri);
    c.n_chirp     = j["n_chirp"].integer(c.n_chirp);
    c.mimo        = mimo_from(j["mimo"].str(mimo_name(c.mimo)), c.mimo);
    c.tx_enable   = j["tx_enable"].boolean(c.tx_enable);

    c.decim        = j["decim"].integer(c.decim);
    c.n_range_fft  = j["n_range_fft"].integer(c.n_range_fft);
    c.n_range      = j["n_range"].integer(c.n_range);
    c.n_doppler    = j["n_doppler"].integer(c.n_doppler);
    c.range_window = window_from(j["range_window"].str(window_name(c.range_window)), c.range_window);
    c.dopp_window  = window_from(j["dopp_window"].str(window_name(c.dopp_window)),  c.dopp_window);

    c.cfar_kind       = cfar_from(j["cfar_kind"].str(cfar_name(c.cfar_kind)), c.cfar_kind);
    c.guard_range     = j["guard_range"].integer(c.guard_range);
    c.guard_dopp      = j["guard_dopp"].integer(c.guard_dopp);
    c.train_range     = j["train_range"].integer(c.train_range);
    c.train_dopp      = j["train_dopp"].integer(c.train_dopp);
    c.pfa             = j["pfa"].num(c.pfa);
    c.max_hits        = j["max_hits"].integer(c.max_hits);
    c.zero_dopp_blank = j["zero_dopp_blank"].integer(c.zero_dopp_blank);
    c.range_zero_bin  = j["range_zero_bin"].integer(c.range_zero_bin);

    c.aoa             = aoa_from(j["aoa"].str(aoa_name(c.aoa)), c.aoa);
    c.aoa_az_bins     = j["aoa_az_bins"].integer(c.aoa_az_bins);
    c.aoa_el_bins     = j["aoa_el_bins"].integer(c.aoa_el_bins);
    c.aoa_az_span_deg = j["aoa_az_span_deg"].num(c.aoa_az_span_deg);
    c.aoa_el_span_deg = j["aoa_el_span_deg"].num(c.aoa_el_span_deg);
    c.cluster_eps_m   = j["cluster_eps_m"].num(c.cluster_eps_m);
    c.cluster_eps_ms  = j["cluster_eps_ms"].num(c.cluster_eps_ms);
    c.cluster_eps_deg = j["cluster_eps_deg"].num(c.cluster_eps_deg);
    c.cluster_min_pts = j["cluster_min_pts"].integer(c.cluster_min_pts);
    c.track_gate_chi2 = j["track_gate_chi2"].num(c.track_gate_chi2);
    c.track_confirm_n = j["track_confirm_n"].integer(c.track_confirm_n);
    c.track_drop_n    = j["track_drop_n"].integer(c.track_drop_n);
    c.track_q_accel   = j["track_q_accel"].num(c.track_q_accel);

    c.worker_threads = j["worker_threads"].integer(c.worker_threads);
    c.realtime       = j["realtime"].boolean(c.realtime);
    c.http_port      = j["http_port"].integer(c.http_port);
    c.web_root       = j["web_root"].str(c.web_root);
    c.record_path    = j["record_path"].str(c.record_path);
    c.calib_path     = j["calib_path"].str(c.calib_path);

    c.derive();
    return c;
}

void save_config(const Config& c, const std::string& path) {
    Config t = c;
    t.derive();

    Json j = Json::object();
    j.set("source",          std::string(source_name(t.source)));
    j.set("device_args",     t.device_args);
    j.set("centre_freq_hz",  t.centre_freq_hz);
    j.set("sample_rate_hz",  t.sample_rate_hz);
    j.set("tx_gain_db",      t.tx_gain_db);
    j.set("rx_gain_db",      t.rx_gain_db);
    j.set("rx_bandwidth_hz", t.rx_bandwidth_hz);
    j.set("clock_source",    t.clock_source);
    j.set("time_source",     t.time_source);

    j.set("sweep_bw_hz", t.sweep_bw_hz);
    j.set("n_sweep",     t.n_sweep);
    j.set("n_pri",       t.n_pri);
    j.set("n_chirp",     t.n_chirp);
    j.set("mimo",        std::string(mimo_name(t.mimo)));
    j.set("tx_enable",   t.tx_enable);

    j.set("decim",        t.decim);
    j.set("n_range_fft",  t.n_range_fft);
    j.set("n_range",      t.n_range);
    j.set("n_doppler",    t.n_doppler);
    j.set("range_window", std::string(window_name(t.range_window)));
    j.set("dopp_window",  std::string(window_name(t.dopp_window)));

    j.set("cfar_kind",       std::string(cfar_name(t.cfar_kind)));
    j.set("guard_range",     t.guard_range);
    j.set("guard_dopp",      t.guard_dopp);
    j.set("train_range",     t.train_range);
    j.set("train_dopp",      t.train_dopp);
    j.set("pfa",             t.pfa);
    j.set("max_hits",        t.max_hits);
    j.set("zero_dopp_blank", t.zero_dopp_blank);
    j.set("range_zero_bin",  t.range_zero_bin);

    j.set("aoa",             std::string(aoa_name(t.aoa)));
    j.set("aoa_az_bins",     t.aoa_az_bins);
    j.set("aoa_el_bins",     t.aoa_el_bins);
    j.set("aoa_az_span_deg", t.aoa_az_span_deg);
    j.set("aoa_el_span_deg", t.aoa_el_span_deg);
    j.set("cluster_eps_m",   t.cluster_eps_m);
    j.set("cluster_eps_ms",  t.cluster_eps_ms);
    j.set("cluster_eps_deg", t.cluster_eps_deg);
    j.set("cluster_min_pts", t.cluster_min_pts);
    j.set("track_gate_chi2", t.track_gate_chi2);
    j.set("track_confirm_n", t.track_confirm_n);
    j.set("track_drop_n",    t.track_drop_n);
    j.set("track_q_accel",   t.track_q_accel);

    j.set("worker_threads", t.worker_threads);
    j.set("realtime",       t.realtime);
    j.set("http_port",      t.http_port);
    j.set("web_root",       t.web_root);
    j.set("record_path",    t.record_path);
    j.set("calib_path",     t.calib_path);

    // Everything below follows from the settings above.  It is written out so
    // that a saved file can be read by a person or by the web page without
    // running the radar, and it is ignored on the way back in.
    Json dv = Json::object();
    dv.set("lambda_m",         t.d.lambda_m);
    dv.set("chirp_slope_hz_s", t.d.chirp_slope_hz_s);
    dv.set("t_sweep_s",        t.d.t_sweep_s);
    dv.set("t_pri_s",          t.d.t_pri_s);
    dv.set("t_cpi_s",          t.d.t_cpi_s);
    dv.set("fs_dec_hz",        t.d.fs_dec_hz);
    dv.set("range_res_m",      t.d.range_res_m);
    dv.set("range_bin_m",      t.d.range_bin_m);
    dv.set("range_max_m",      t.d.range_max_m);
    dv.set("vel_res_ms",       t.d.vel_res_ms);
    dv.set("vel_max_ms",       t.d.vel_max_ms);
    dv.set("frame_rate_hz",    t.d.frame_rate_hz);
    dv.set("usb_bytes_s",      t.d.usb_bytes_s);
    dv.set("n_virt",           t.d.n_virt);
    dv.set("n_chirp_total",    t.d.n_chirp_total);
    dv.set("n_sweep_dec",      t.d.n_sweep_dec);
    j.set("derived", dv);

    if (!j.dump_file(path, 2)) throw std::runtime_error("cannot write " + path);
}

//============================================================================
// Named operating points
//============================================================================
Config profile(const std::string& name) {
    Config c;   // the defaults are the surveillance point

    if (name == "fast") {
        // 256 range bins by 256 chirps: 576 m of range, 62.5 maps a second.
    } else if (name == "fine") {
        // Trade half the range for twice the coherent interval: 288 m,
        // 31.2 maps a second, 0.81 m/s of Doppler resolution.
        c.n_range   = 128;
        c.n_chirp   = 256;
        c.n_doppler = 256;
    } else if (name == "wide") {
        // The widest sweep the radio can actually carry.  The sample rate
        // allows 61.44 MHz and the AD9361's analogue filter passes 56 MHz, so
        // 56 MHz is the real limit; beyond it the ends of the sweep come back
        // attenuated and the range profile tilts.
        c.sweep_bw_hz = std::min(c.sample_rate_hz, c.rx_bandwidth_hz);
    } else if (name == "long") {
        // Four times the coherent interval for micro-Doppler work.  The
        // corner-turn budget is fixed, so the chirps are paid for in range
        // bins: 64 bins is 144 m, which is close in but enough for reading the
        // blade modulation off a quadcopter.
        c.n_range   = 64;
        c.n_chirp   = 512;
        c.n_doppler = 512;
    } else if (name == "passive") {
        // Receive only: listen before transmitting, or watch someone else's
        // illumination. The chirp generator still runs, because the de-chirp
        // reference comes out of it; only the transmit output is muted.
        c.tx_enable = false;
    } else {
        throw std::runtime_error(
            "there is no operating point called \"" + name + "\". The ones that exist are "
            "\"fast\" (256 range bins by 256 chirps, 576 m, 62.5 maps a second), "
            "\"fine\" (128 by 512, 288 m, 31.2 maps a second, twice the Doppler resolution), "
            "\"wide\" (the widest sweep the radio can carry, for the finest range resolution), "
            "\"long\" (64 by 1024, four times the coherent interval, for micro-Doppler) and "
            "\"passive\" (receive only, nothing transmitted).");
    }

    c.derive();
    return c;
}

} // namespace radar
