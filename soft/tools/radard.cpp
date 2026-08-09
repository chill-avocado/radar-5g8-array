//============================================================================
// radard -- the radar daemon
//
// Runs the pipeline, serves the operator display, and gets out of the way.
//
//   radard                                  simulated scene, display on :8730
//   radard --source uhd                     a real B210 running the gateware
//   radard --replay flight.rdr              a recording
//   radard --profile fine                   the fine-Doppler operating point
//   radard --scene scenes/three_drones.json a specific simulated scene
//   radard --record flight.rdr --cap 500M   capture while running
//   radard --headless --frames 100          no display, for scripting
//
// The display is at http://127.0.0.1:8730 and is bound to the loopback
// interface unless --allow-remote is given deliberately.
//============================================================================
#include "radar/config.hpp"
#include "radar/json.hpp"
#include "radar/log.hpp"
#include "radar/net.hpp"
#include "radar/pipeline.hpp"
#include "radar/proto.hpp"

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using namespace radar;
using namespace radar::proto;

namespace {

std::atomic<bool> g_stop{false};
void on_signal(int) { g_stop.store(true); }

u64 now_ns() {
    using namespace std::chrono;
    return u64(duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count());
}

/// Accepts 500M, 2G, 1500000. Disk on this machine is scarce enough that a
/// recording without a cap is a hazard, so the cap is always explicit.
u64 parse_size(const std::string& s) {
    char*  end  = nullptr;
    double v    = std::strtod(s.c_str(), &end);
    if (end && *end) {
        switch (*end) {
            case 'k': case 'K': v *= 1e3; break;
            case 'm': case 'M': v *= 1e6; break;
            case 'g': case 'G': v *= 1e9; break;
            default: break;
        }
    }
    return u64(v);
}

void usage() {
    std::printf(
"radard -- 5.8 GHz MIMO radar daemon\n"
"\n"
"  --source sim|uhd|file   where samples or frames come from     (default sim)\n"
"  --config <file>         load a configuration written by radar-plan\n"
"  --profile <name>        fast | fine | wide | long | passive   (default fast)\n"
"  --scene <file>          simulated scene description\n"
"  --replay <file>         replay a recording (implies --source file)\n"
"  --record <file>         record while running\n"
"  --cap <size>            recording size cap, e.g. 500M         (default 250M)\n"
"  --calib <file>          load a calibration\n"
"  --port <n>              display port                          (default 8730)\n"
"  --allow-remote          listen on all interfaces, not just loopback\n"
"  --headless              do not serve a display\n"
"  --frames <n>            stop after n frames\n"
"  --duration <s>          stop after s seconds\n"
"  --threads <n>           worker threads                        (default 3)\n"
"  --no-realtime           do not ask for real-time scheduling\n"
"  --quiet | --verbose     less or more logging\n"
"  --log <file>            also write the log to a capped file\n"
"  --help\n");
}

} // namespace

int main(int argc, char** argv) {
    Config      cfg;
    std::string config_path, scene_path, replay_path, record_path, calib_path, log_path;
    std::string profile_name = "fast";
    u64         record_cap   = 250u * 1000u * 1000u;
    bool        headless = false, allow_remote = false;
    long        stop_after_frames = 0;
    double      stop_after_s      = 0;

    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto need = [&](const char* what) -> std::string {
            if (i + 1 >= argc) { std::fprintf(stderr, "%s needs a value\n", what); std::exit(2); }
            return argv[++i];
        };
        if      (a == "--help" || a == "-h") { usage(); return 0; }
        else if (a == "--config")   config_path  = need("--config");
        else if (a == "--profile")  profile_name = need("--profile");
        else if (a == "--scene")    scene_path   = need("--scene");
        else if (a == "--replay")   replay_path  = need("--replay");
        else if (a == "--record")   record_path  = need("--record");
        else if (a == "--cap")      record_cap   = parse_size(need("--cap"));
        else if (a == "--calib")    calib_path   = need("--calib");
        else if (a == "--log")      log_path     = need("--log");
        else if (a == "--port")     cfg.http_port = std::atoi(need("--port").c_str());
        else if (a == "--threads")  cfg.worker_threads = std::atoi(need("--threads").c_str());
        else if (a == "--frames")   stop_after_frames = std::atol(need("--frames").c_str());
        else if (a == "--duration") stop_after_s = std::atof(need("--duration").c_str());
        else if (a == "--allow-remote") allow_remote = true;
        else if (a == "--headless")     headless = true;
        else if (a == "--no-realtime")  cfg.realtime = false;
        else if (a == "--quiet")        log_set_level(LogLevel::Warn);
        else if (a == "--verbose")      log_set_level(LogLevel::Debug);
        else if (a == "--source") {
            const std::string s = need("--source");
            if      (s == "sim"  || s == "simulate") cfg.source = SourceKind::Simulate;
            else if (s == "uhd"  || s == "b210")     cfg.source = SourceKind::Uhd;
            else if (s == "file" || s == "replay")   cfg.source = SourceKind::File;
            else { std::fprintf(stderr, "unknown source '%s'\n", s.c_str()); return 2; }
        } else {
            std::fprintf(stderr, "unknown option '%s' -- try --help\n", a.c_str());
            return 2;
        }
    }

    if (!log_path.empty()) log_set_file(log_path);

    //-- Configuration -----------------------------------------------------
    // Order matters: profile is the base, a config file overrides it, and
    // command-line switches override both. Anything else surprises people.
    {
        const int    port    = cfg.http_port;
        const int    threads = cfg.worker_threads;
        const bool   rt      = cfg.realtime;
        const auto   source  = cfg.source;
        try {
            cfg = profile(profile_name);
        } catch (const std::exception& e) {
            std::fprintf(stderr, "%s\n", e.what());
            return 2;
        }
        if (!config_path.empty()) {
            try {
                cfg = load_config(config_path);
            } catch (const std::exception& e) {
                std::fprintf(stderr, "could not load %s: %s\n", config_path.c_str(), e.what());
                return 2;
            }
        }
        cfg.http_port      = port;
        cfg.worker_threads = threads;
        cfg.realtime       = rt;
        if (source != SourceKind::Simulate || !replay_path.empty()) cfg.source = source;
    }
    if (!replay_path.empty()) { cfg.source = SourceKind::File; cfg.record_path = replay_path; }
    if (!calib_path.empty())  cfg.calib_path = calib_path;
    if (!scene_path.empty())  cfg.scene_path = scene_path;
    cfg.derive();

    if (const std::string bad = cfg.validate(); !bad.empty()) {
        std::fprintf(stderr, "this configuration will not work: %s\n", bad.c_str());
        return 2;
    }

    //-- What we are about to do, in numbers -------------------------------
    LOG_I("5.8 GHz MIMO radar");
    LOG_I("  source            %s",
          cfg.source == SourceKind::Simulate ? "simulated scene"
        : cfg.source == SourceKind::Uhd      ? "USRP B210 with radar gateware"
                                             : "recording");
    LOG_I("  carrier           %.4f GHz, %.1f MHz sweep", cfg.centre_freq_hz / 1e9,
          cfg.sweep_bw_hz / 1e6);
    LOG_I("  range             %.2f m resolution, %.0f m covered in %d bins",
          cfg.d.range_res_m, cfg.d.range_max_m, cfg.n_range);
    LOG_I("  velocity          %.2f m/s resolution, +/- %.0f m/s unambiguous",
          cfg.d.vel_res_ms, cfg.d.vel_max_ms);
    LOG_I("  frame             %.1f ms, %.1f per second", cfg.d.t_cpi_s * 1e3, cfg.d.frame_rate_hz);
    LOG_I("  host link         %.2f MB/s", cfg.d.usb_bytes_s / 1e6);

    //-- Build -------------------------------------------------------------
    Pipeline pipe(cfg);
    if (!calib_path.empty()) {
        std::string msg;
        if (!pipe.calibration_load(calib_path, msg)) LOG_W("calibration: %s", msg.c_str());
    }

    std::unique_ptr<WebServer> web;
    if (!headless) {
        web = std::make_unique<WebServer>(cfg.http_port);
        web->set_allow_remote(allow_remote);
        if (!cfg.web_root.empty()) web->set_web_root(cfg.web_root);

        //-- Control from the browser --------------------------------------
        web->on_message([&](const std::string& text) {
            Json m;
            try { m = Json::parse(text); }
            catch (const std::exception& e) { LOG_W("bad control message: %s", e.what()); return; }
            const std::string cmd = m["cmd"].str();
            if      (cmd == "pfa")        pipe.set_pfa(m["value"].num(1e-5));
            else if (cmd == "zero_dopp")  pipe.set_zero_dopp_blank(m["value"].integer(2));
            else if (cmd == "dynrange")   pipe.set_dynamic_range_db(m["value"].num(60));
            else if (cmd == "freeze")     pipe.freeze(m["value"].boolean(false));
            else if (cmd == "cfar") {
                const std::string k = m["value"].str("ca");
                pipe.set_cfar_kind(k == "os" ? CfarKind::Os : k == "go" ? CfarKind::Go
                                 : k == "so" ? CfarKind::So : k == "none" ? CfarKind::None
                                                                          : CfarKind::Ca);
            } else if (cmd == "aoa") {
                const std::string k = m["value"].str("music");
                pipe.set_aoa_method(k == "monopulse" ? AoaMethod::Monopulse
                                  : k == "bartlett"  ? AoaMethod::Bartlett
                                  : k == "capon"     ? AoaMethod::Capon : AoaMethod::Music);
            } else if (cmd == "calibrate_range_zero") {
                std::string msg; pipe.calibrate_range_zero(msg); LOG_I("%s", msg.c_str());
            } else if (cmd == "calibrate_boresight") {
                std::string msg; pipe.calibrate_boresight(msg); LOG_I("%s", msg.c_str());
            } else {
                LOG_W("unknown control command '%s'", cmd.c_str());
            }
        });

        //-- Status for curl, so the radar can be checked without a browser --
        web->on_get("/status", [&](const std::string&) {
            const Stats s = pipe.stats();
            Json j = Json::object();
            j.set("source",        pipe.source_name());
            j.set("running",       pipe.running());
            j.set("frames",        double(s.frames));
            j.set("frame_rate_hz", s.frame_rate_hz);
            j.set("cpu_frac",      s.cpu_frac);
            j.set("overflows",     double(s.overflows));
            j.set("dropped",       double(s.dropped));
            j.set("tracks",        s.n_tracks);
            j.set("bytes_in",      double(s.bytes_in));
            Json st = Json::object();
            static const char* names[8] = {"dechirp", "range_fft", "corner_turn", "doppler_fft",
                                           "cfar", "angle", "cluster", "track"};
            for (int i = 0; i < 8; ++i) st.set(names[i], s.stage_ms[i]);
            j.set("stage_ms", st);
            Json c = Json::object();
            c.set("centre_freq_hz", cfg.centre_freq_hz);
            c.set("sweep_bw_hz",    cfg.sweep_bw_hz);
            c.set("n_range",        cfg.n_range);
            c.set("n_doppler",      cfg.n_doppler);
            c.set("range_res_m",    cfg.d.range_res_m);
            c.set("range_max_m",    cfg.d.range_max_m);
            c.set("vel_res_ms",     cfg.d.vel_res_ms);
            c.set("vel_max_ms",     cfg.d.vel_max_ms);
            j.set("config", c);
            return j.dump(2);
        });

        std::string err;
        if (!web->start(err)) {
            LOG_E("display server: %s", err.c_str());
            return 1;
        }
        LOG_I("  display           %s", web->url().c_str());
    }

    //-- Wire frames to the display ---------------------------------------
    const u64          t0 = now_ns();
    std::vector<u8>    buf;
    std::atomic<u64>   frames_done{0};
    bool               sent_config = false;
    double             dyn_range   = 70.0;

    pipe.on_frame([&](const RdFrame& f, const std::vector<Track>& tracks, const Stats& s) {
        frames_done.store(f.index + 1, std::memory_order_relaxed);
        if (!web) return;

        buf.clear();
        const u64 t = now_ns();
        if (!sent_config) {
            encode_hello(buf, t0, t, "radar5g8");
            encode_config(buf, cfg, f.index, t);
            sent_config = true;
        }
        encode_rdmap(buf, f, t, auto_range(f, dyn_range));
        encode_hits(buf, f, t);
        encode_tracks(buf, tracks, f.index, t);

        NetTally net;
        net.clients = web->clients();
        net.bytes_sent = web->bytes_sent();
        net.dropped_frames = web->dropped_frames();
        encode_stats(buf, s, net, cfg.source, f.index, t);

        // One track's micro-Doppler per frame, round-robin, so the link is not
        // dominated by spectrograms when several targets are being tracked.
        if (!tracks.empty()) {
            const std::size_t pick = std::size_t(f.index) % tracks.size();
            const double hz_per_bin = tracks[pick].spec_freq > 0
                ? 1.0 / cfg.d.t_pri_s / 2.0 / tracks[pick].spec_freq : 1.0;
            encode_spectrogram(buf, tracks[pick], f.index, t,
                               f32(hz_per_bin), f32(cfg.d.t_cpi_s));
        }
        web->broadcast(buf.data(), buf.size());
    });

    if (!record_path.empty()) {
        std::string err;
        if (!pipe.record_start(record_path, record_cap, err)) {
            LOG_E("recording: %s", err.c_str());
            return 1;
        }
        LOG_I("  recording         %s, capped at %.0f MB", record_path.c_str(), record_cap / 1e6);
    }

    //-- Run ---------------------------------------------------------------
    std::signal(SIGINT, on_signal);
    std::signal(SIGTERM, on_signal);

    std::string err;
    if (!pipe.start(err)) {
        LOG_E("could not start: %s", err.c_str());
        return 1;
    }
    LOG_I("running -- ctrl-C to stop");

    const double t_start   = double(now_ns()) * 1e-9;
    double       next_line = t_start + 2.0;

    while (!g_stop.load() && pipe.running()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        const double now = double(now_ns()) * 1e-9;

        if (now >= next_line) {
            next_line = now + 2.0;
            const Stats s = pipe.stats();
            char viewers[32] = "";
            if (web) std::snprintf(viewers, sizeof(viewers), "  %d viewing", web->clients());
            LOG_I("%6llu frames  %5.1f fps  cpu %4.1f%%  %2d tracks  "
                  "%llu overflow  %llu dropped%s",
                  (unsigned long long)s.frames, s.frame_rate_hz, s.cpu_frac * 100.0,
                  s.n_tracks, (unsigned long long)s.overflows,
                  (unsigned long long)s.dropped, viewers);
        }
        if (stop_after_frames && long(frames_done.load()) >= stop_after_frames) break;
        if (stop_after_s > 0 && (now - t_start) >= stop_after_s) break;
    }

    LOG_I("stopping");
    pipe.record_stop();
    pipe.stop();
    if (web) web->stop();

    const Stats s = pipe.stats();
    LOG_I("%llu frames, %llu overflows, %llu dropped, %.1f fps average",
          (unsigned long long)s.frames, (unsigned long long)s.overflows,
          (unsigned long long)s.dropped, s.frame_rate_hz);
    if (s.overflows || s.dropped) {
        LOG_W("this run lost data -- treat any measurement from it as provisional");
        return 3;
    }
    return 0;
}
