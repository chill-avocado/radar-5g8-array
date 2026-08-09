//============================================================================
// uhd_source.cpp -- a real B210 running the radar gateware
//
// The whole file is behind RADAR_HAVE_UHD.  Without it the stack still builds
// and still links, and make_uhd_source() returns a source whose open() explains
// in one sentence that this build has no radio support -- which is a great deal
// more useful than a missing symbol at link time or a silent fall back to the
// simulator.
//
// WHAT COMES BACK OVER USB
//   Not IQ.  The gateware has already de-chirped, transformed twice, integrated
//   and run the detector, so what crosses the link is the packed frame format
//   in section 7 of fpga/rtl/radar_pkg.svh: 4.4 MB/s of range-Doppler maps and
//   detections instead of 491 MB/s of raw samples.  UHD has no idea about any
//   of this; it is told to stream sc16 and it does, and every pair of 16-bit
//   values is one of the gateware's 32-bit words.
//
//   Byte order: UHD hands back host-order int16 pairs, I first then Q, and the
//   gateware puts the high half of each word in the I slot.  So
//       word = (u16(sample.real()) << 16) | u16(sample.imag())
//   which is the same thing the packer does at the other end, and is checked
//   by the version handshake below before a single frame is believed.
//
// THE HANDSHAKE
//   A B210 with the stock image will happily stream when asked, and will send
//   noise that looks nothing like a frame.  Rather than sit there resynchronising
//   forever, this writes REG_VERSION, waits two seconds for a frame carrying the
//   expected format version, and if none arrives says plainly that the attached
//   device is not running the radar gateware and gives up.
//
// OVERFLOWS
//   An overflow is not an error.  It means the host was late and the radio threw
//   samples away; the right response is to count it, mark the next frame, and
//   keep going.  Only a genuinely broken stream stops the receive thread.
//============================================================================

#include <cstring>

#include "radar/source.hpp"
#include "radar/log.hpp"
#include "radar/ring.hpp"
#include "radar/thread.hpp"

#include <atomic>
#include <chrono>
#include <cmath>
#include <string>
#include <thread>
#include <vector>

#ifdef RADAR_HAVE_UHD

#include <uhd/stream.hpp>
#include <uhd/types/stream_cmd.hpp>
#include <uhd/types/time_spec.hpp>
#include <uhd/types/tune_request.hpp>
#include <uhd/usrp/multi_usrp.hpp>
#include <uhd/utils/thread.hpp>

namespace radar {
namespace {

/// One block of received words, as it comes off the wire.  Sized so a block is
/// a comfortable fraction of a frame: big enough that the ring is not hammered,
/// small enough that a frame is never held up waiting for one to fill.
constexpr std::size_t kBlockWords = 8192;
constexpr std::size_t kRingSlots  = 64;      // power of two, ~2 MB of words

struct Block {
    u32         words[kBlockWords];
    std::size_t n = 0;
    bool        overflow = false;            ///< the radio lost data before this block
    u64         ticks = 0;                   ///< radio time of the first sample
};

/// Compare what the radio gave us with what was asked for, and say so loudly
/// when they differ.  A B210 silently rounds almost everything -- the sample
/// rate to a divisor of the master clock, the gain to a quarter of a decibel,
/// the analogue bandwidth to one of a handful of filter settings -- and a radar
/// calibrated for one number while running at another is wrong in a way that
/// looks like a physics problem.
bool check(const char* what, double asked, double got, double tol, const char* unit) {
    if (std::fabs(asked - got) <= tol) {
        LOG_I("radio: %s %.6g %s", what, got, unit);
        return true;
    }
    LOG_W("radio: asked for %s %.6g %s and the radio gave %.6g %s -- a difference of %.6g %s",
          what, asked, unit, got, unit, got - asked, unit);
    return false;
}

class UhdSource : public IqSource {
public:
    ~UhdSource() override { close(); }

    bool open(const Config& c) override;
    void close() override;
    bool running() const override { return running_.load(std::memory_order_acquire); }

    bool next_raw(IqCpi& out, double timeout_s) override {
        (void)out; (void)timeout_s;
        set_error("radio: the gateware sends processed frames, not raw samples; "
                  "the pipeline should be calling next_frame");
        return false;
    }
    bool gives_frames() const override { return true; }
    bool next_frame(RdFrame& out, double timeout_s) override;

    const char* name() const override { return "uhd"; }
    Stats stats() const override;
    bool  ended() const override { return ended_.load(std::memory_order_acquire); }

private:
    void receive_loop();
    bool wait_for_gateware(double seconds);

    uhd::usrp::multi_usrp::sptr usrp_;
    uhd::rx_streamer::sptr      rx_;
    Config                      cfg_;

    std::thread        thr_;
    std::atomic<bool>  running_{false};
    std::atomic<bool>  ended_{false};
    std::atomic<u64>   overflows_{0};
    std::atomic<u64>   errors_{0};
    std::atomic<u64>   blocks_{0};
    std::atomic<u64>   bytes_{0};
    std::string        thread_err_;
    std::mutex         thread_err_m_;

    SpscRing<Block, kRingSlots> ring_;
    std::unique_ptr<wire::FrameDecoder> dec_;

    Block  pending_;                 ///< block being consumed by next_frame
    std::size_t pending_pos_ = 0;
    bool        pending_live_ = false;
    bool        overflow_pending_ = false;

    u64    frames_ = 0;
    double tick_s_ = 0;
    double t_open_ = 0;
};

//----------------------------------------------------------------------------
// open
//----------------------------------------------------------------------------
bool UhdSource::open(const Config& c) {
    clear_error();
    close();
    cfg_ = c;

    try {
        LOG_I("radio: opening \"%s\"", c.device_args.c_str());
        usrp_ = uhd::usrp::multi_usrp::make(c.device_args);
    } catch (const std::exception& e) {
        set_error(std::string("radio: no device answered to \"") + c.device_args + "\" -- " + e.what());
        return false;
    }

    try {
        //-- references ---------------------------------------------------
        usrp_->set_clock_source(c.clock_source);
        usrp_->set_time_source(c.time_source);
        if (usrp_->get_clock_source(0) != c.clock_source) {
            LOG_W("radio: asked for the %s clock and the radio reports %s",
                  c.clock_source.c_str(), usrp_->get_clock_source(0).c_str());
        }

        //-- rates ---------------------------------------------------------
        // The master clock and the sample rate are the same thing on a B210
        // running flat out; asking for both keeps the AD9361's own dividers out
        // of the picture, which is what makes the chirp geometry exact.
        usrp_->set_master_clock_rate(c.sample_rate_hz);
        check("master clock", c.sample_rate_hz, usrp_->get_master_clock_rate(), 1.0, "Hz");

        usrp_->set_rx_rate(c.sample_rate_hz);
        check("sample rate", c.sample_rate_hz, usrp_->get_rx_rate(), 1.0, "Hz");

        //-- both receive channels ------------------------------------------
        const std::size_t nch = std::min<std::size_t>(usrp_->get_rx_num_channels(),
                                                      std::size_t(array_geom::n_rx));
        if (nch < std::size_t(array_geom::n_rx)) {
            set_error("radio: this device has " + std::to_string(nch) +
                      " receive channels and the radar needs " +
                      std::to_string(array_geom::n_rx));
            return false;
        }
        for (std::size_t ch = 0; ch < nch; ++ch) {
            uhd::tune_request_t tr(c.centre_freq_hz);
            usrp_->set_rx_freq(tr, ch);
            check("centre frequency", c.centre_freq_hz, usrp_->get_rx_freq(ch), 1000.0, "Hz");

            usrp_->set_rx_gain(c.rx_gain_db, ch);
            check("receive gain", c.rx_gain_db, usrp_->get_rx_gain(ch), 0.3, "dB");

            usrp_->set_rx_bandwidth(c.rx_bandwidth_hz, ch);
            check("analogue bandwidth", c.rx_bandwidth_hz, usrp_->get_rx_bandwidth(ch), 1e6, "Hz");
        }

        //-- transmit, for the leakage-free case and for the record ---------
        if (c.tx_enable) {
            const std::size_t ntx = std::min<std::size_t>(usrp_->get_tx_num_channels(),
                                                          std::size_t(array_geom::n_tx));
            for (std::size_t ch = 0; ch < ntx; ++ch) {
                uhd::tune_request_t tr(c.centre_freq_hz);
                usrp_->set_tx_freq(tr, ch);
                usrp_->set_tx_gain(c.tx_gain_db, ch);
                check("transmit gain", c.tx_gain_db, usrp_->get_tx_gain(ch), 0.3, "dB");
            }
        }

        //-- the radar's own registers ---------------------------------------
        // Everything about the waveform, the transforms and the detector lives
        // in the fabric; this is the only way the host tells it what to do.
        const auto writes = c.register_writes();
        for (const auto& w : writes) usrp_->set_user_register(w.first, w.second, 0);
        LOG_I("radio: wrote %d gateware registers", int(writes.size()));

        //-- the stream ------------------------------------------------------
        uhd::stream_args_t sa("sc16", "sc16");
        sa.channels = {0};      // the gateware multiplexes both receivers itself
        rx_ = usrp_->get_rx_stream(sa);

        tick_s_ = 1.0 / usrp_->get_rx_rate();
        usrp_->set_time_now(uhd::time_spec_t(0.0));

        // A timed start, so the timestamp of the first sample is a number we
        // chose rather than a number we have to discover.
        uhd::stream_cmd_t cmd(uhd::stream_cmd_t::STREAM_MODE_START_CONTINUOUS);
        cmd.stream_now = false;
        cmd.time_spec  = uhd::time_spec_t(0.2);
        rx_->issue_stream_cmd(cmd);
    } catch (const std::exception& e) {
        set_error(std::string("radio: setting up the device failed -- ") + e.what());
        usrp_.reset();
        return false;
    }

    dec_.reset(new wire::FrameDecoder(c));
    frames_ = 0;
    overflows_ = 0; errors_ = 0; blocks_ = 0; bytes_ = 0;
    pending_live_ = false;
    pending_pos_  = 0;
    overflow_pending_ = false;
    ended_.store(false, std::memory_order_release);
    running_.store(true, std::memory_order_release);
    t_open_ = rt::now_s();

    thr_ = std::thread([this] { receive_loop(); });

    if (!wait_for_gateware(2.0)) {
        set_error("radio: the device answered and streamed, but no radar frame arrived within "
                  "two seconds -- the attached B210 is not running the radar gateware "
                  "(load the radar image, or run with source=simulate)");
        close();
        return false;
    }
    return true;
}

//----------------------------------------------------------------------------
// The receive thread
//----------------------------------------------------------------------------
void UhdSource::receive_loop() {
    rt::set_name("radar-rx");
    rt::set_affinity(0);
    // One interval is 16 ms at the default operating point and the work per
    // interval is small -- copying words out of a USB buffer -- so the budget
    // asked for is deliberately modest.  Overstating it gets the request
    // refused; understating it gets the thread preempted mid-transfer.
    const double period = std::max(1e-3, cfg_.d.t_cpi_s > 0 ? cfg_.d.t_cpi_s : 0.016);
    rt::ScopedRealtime rt_guard(period, period * 0.20, period * 0.60);
    if (!rt_guard.ok()) {
        LOG_W("radio: the operating system would not grant a real-time policy to the receive "
              "thread; overflows are more likely under load");
    }

    std::vector<std::complex<i16>> buf(kBlockWords);
    uhd::rx_metadata_t             md;
    Block                          blk;

    while (running_.load(std::memory_order_acquire)) {
        std::size_t got = 0;
        try {
            void* ptr = buf.data();
            got = rx_->recv(&ptr, kBlockWords, md, 1.0, false);
        } catch (const std::exception& e) {
            std::lock_guard<std::mutex> lk(thread_err_m_);
            thread_err_ = std::string("radio: the receive stream threw -- ") + e.what();
            errors_.fetch_add(1, std::memory_order_relaxed);
            break;
        }

        switch (md.error_code) {
            case uhd::rx_metadata_t::ERROR_CODE_NONE:
                break;
            case uhd::rx_metadata_t::ERROR_CODE_OVERFLOW:
                // The host was late.  Count it, flag the next block, carry on:
                // an overflow costs a frame and must not cost the stream.
                overflows_.fetch_add(1, std::memory_order_relaxed);
                blk.overflow = true;
                log_rate_limited(LogLevel::Warn, "rx-overflow", 1.0,
                                 "radio: overflow, the host is not keeping up (%llu so far)",
                                 (unsigned long long)overflows_.load());
                if (got == 0) continue;
                break;
            case uhd::rx_metadata_t::ERROR_CODE_TIMEOUT:
                continue;
            default:
                errors_.fetch_add(1, std::memory_order_relaxed);
                log_rate_limited(LogLevel::Error, "rx-error", 1.0,
                                 "radio: receive error %s",
                                 md.strerror().c_str());
                continue;
        }
        if (got == 0) continue;

        // Every complex sample is one gateware word: the high half in I, the
        // low half in Q, which is what the packer wrote and what the format
        // version check below proves.
        for (std::size_t i = 0; i < got; ++i) {
            blk.words[i] = (u32(u16(buf[i].real())) << 16) | u32(u16(buf[i].imag()));
        }
        blk.n     = got;
        blk.ticks = md.has_time_spec ? u64(md.time_spec.to_ticks(cfg_.sample_rate_hz)) : 0;

        if (!ring_.try_push(std::move(blk))) {
            // The pipeline is behind.  Dropping the newest block is the right
            // choice: the decoder resynchronises on the next magic word, so one
            // frame is lost instead of the stream falling permanently behind.
            ring_.drop();
            log_rate_limited(LogLevel::Warn, "rx-ringfull", 1.0,
                             "radio: the pipeline is behind and blocks are being dropped "
                             "(%llu so far)", (unsigned long long)ring_.dropped());
        } else {
            blocks_.fetch_add(1, std::memory_order_relaxed);
            bytes_.fetch_add(got * 4, std::memory_order_relaxed);
        }
        blk.overflow = false;
        blk.n        = 0;
    }
    ended_.store(true, std::memory_order_release);
}

//----------------------------------------------------------------------------
// Frames
//----------------------------------------------------------------------------
bool UhdSource::next_frame(RdFrame& out, double timeout_s) {
    clear_error();
    if (!dec_) { set_error("radio: next_frame called before open"); return false; }

    const double deadline = rt::now_s() + std::max(0.0, timeout_s);
    for (;;) {
        // Finish whatever block is already in hand before asking for another.
        if (pending_live_ && pending_pos_ < pending_.n) {
            if (dec_->feed(pending_.words + pending_pos_, pending_.n - pending_pos_, out)) {
                pending_pos_ += dec_->consumed();
                out.timestamp_s = out.timestamp_s * tick_s_;
                if (overflow_pending_) { out.overflow = true; overflow_pending_ = false; }
                ++frames_;
                return true;
            }
            pending_pos_ = pending_.n;
        }

        if (ring_.try_pop(pending_)) {
            pending_live_ = true;
            pending_pos_  = 0;
            if (pending_.overflow) overflow_pending_ = true;
            continue;
        }

        {
            std::lock_guard<std::mutex> lk(thread_err_m_);
            if (!thread_err_.empty()) { set_error(thread_err_); return false; }
        }
        if (!running_.load(std::memory_order_acquire) || ended_.load(std::memory_order_acquire)) {
            set_error("radio: the receive stream stopped");
            return false;
        }
        if (rt::now_s() >= deadline) return false;          // plain timeout, not an error
        std::this_thread::sleep_for(std::chrono::microseconds(200));
    }
}

bool UhdSource::wait_for_gateware(double seconds) {
    // Ask the fabric to stamp its version into the next frame header, then look
    // for a frame.  A stock image never produces one, and this is how that is
    // discovered in two seconds instead of never.
    try {
        usrp_->set_user_register(19 /* RADAR_REG_VERSION */, 1u, 0);
    } catch (const std::exception& e) {
        LOG_W("radio: could not write the version register -- %s", e.what());
    }
    RdFrame probe;
    const double t_end = rt::now_s() + seconds;
    while (rt::now_s() < t_end) {
        if (next_frame(probe, 0.05)) {
            LOG_I("radio: gateware confirmed, first frame index %llu, %d x %d map, %d detections",
                  (unsigned long long)probe.index, probe.n_range, probe.n_doppler,
                  int(probe.hits.size()));
            return true;
        }
        if (!running_.load(std::memory_order_acquire)) return false;
    }
    LOG_E("radio: no radar frame in %.1f s -- decoder saw %llu words go past without a valid "
          "frame (%llu resynchronisations, %llu rejected)", seconds,
          (unsigned long long)dec_->skipped_words(), (unsigned long long)dec_->resyncs(),
          (unsigned long long)dec_->bad_frames());
    return false;
}

void UhdSource::close() {
    if (running_.exchange(false, std::memory_order_acq_rel)) {
        if (rx_) {
            try {
                rx_->issue_stream_cmd(uhd::stream_cmd_t(uhd::stream_cmd_t::STREAM_MODE_STOP_CONTINUOUS));
            } catch (const std::exception&) { /* going away anyway */ }
        }
    }
    if (thr_.joinable()) thr_.join();
    // Put the fabric back to sleep so the radio is not still radiating.
    if (usrp_) {
        try { usrp_->set_user_register(0 /* RADAR_REG_CTRL */, 0u, 0); }
        catch (const std::exception&) {}
    }
    rx_.reset();
    usrp_.reset();
    dec_.reset();
    pending_live_ = false;
}

Stats UhdSource::stats() const {
    Stats s;
    s.frames    = frames_;
    s.overflows = overflows_.load(std::memory_order_relaxed);
    s.dropped   = ring_.dropped();
    s.bytes_in  = bytes_.load(std::memory_order_relaxed);
    const double wall = rt::now_s() - t_open_;
    s.frame_rate_hz = wall > 0 ? double(frames_) / wall : 0;
    return s;
}

} // namespace

std::unique_ptr<IqSource> make_uhd_source(const Config& c) {
    (void)c;
    return std::unique_ptr<IqSource>(new UhdSource());
}

} // namespace radar

#else  // ---------------------------------------------------------------- no UHD

namespace radar {
namespace {

class NoUhdSource : public IqSource {
public:
    bool open(const Config&) override {
        set_error("radio: this build has no USRP support -- UHD was not found when the stack "
                  "was configured, so a B210 cannot be opened. Rebuild with UHD installed, "
                  "or run with source=simulate.");
        return false;
    }
    void close() override {}
    bool running() const override { return false; }
    bool next_raw(IqCpi&, double) override { return false; }
    bool gives_frames() const override { return true; }
    bool next_frame(RdFrame&, double) override { return false; }
    const char* name() const override { return "uhd (unavailable)"; }
    bool ended() const override { return true; }
};

} // namespace

std::unique_ptr<IqSource> make_uhd_source(const Config& c) {
    (void)c;
    return std::unique_ptr<IqSource>(new NoUhdSource());
}

} // namespace radar

#endif // RADAR_HAVE_UHD
