//============================================================================
// pipeline.hpp -- the radar, assembled
//
// Two shapes, chosen by what the source gives us:
//
//  HARDWARE.  The FPGA has already de-chirped, transformed, integrated and
//  run CFAR.  What arrives is a range-Doppler map and a detection list with
//  each detection's four virtual-channel samples attached.  The host does the
//  parts that want floating point and matrices -- angle, clustering, tracking,
//  micro-Doppler -- and that is a few milliseconds of work per frame.
//
//  SIMULATION or REPLAY.  There is no FPGA, so the host runs a bit-exact model
//  of the FPGA datapath first and then the same back end.  The arithmetic is
//  the same integer arithmetic the fabric performs, so what is measured on the
//  laptop is what the hardware will produce -- not a floating-point idealisation
//  of it.  That costs real time, which is why the range transforms are spread
//  across a small thread pool.
//
// Both shapes end at the same place, so the display, the recorder and the
// tracker cannot tell which one produced a frame.
//============================================================================
#pragma once

#include "radar/calib.hpp"
#include "radar/config.hpp"
#include "radar/types.hpp"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace radar {

class Pipeline {
public:
    explicit Pipeline(const Config& cfg);
    ~Pipeline();
    Pipeline(const Pipeline&)            = delete;
    Pipeline& operator=(const Pipeline&) = delete;

    /// Opens the source, builds every stage, starts the worker threads.
    /// Returns false and fills `err` rather than throwing.
    bool start(std::string& err);
    void stop();
    bool running() const;

    /// Called once per completed frame, on the pipeline thread. Keep it short:
    /// anything slow here shows up as a dropped frame. The daemon uses it to
    /// serialise and broadcast, which is a memcpy and a queue push.
    using FrameFn = std::function<void(const RdFrame&, const std::vector<Track>&, const Stats&)>;
    void on_frame(FrameFn fn);

    Stats         stats() const;
    const Config& config() const;
    const char*   source_name() const;

    //-- Runtime control, safe to call from any thread --------------------
    // These change the next frame, not the one in flight, so the display never
    // shows a map produced under two different settings.
    void set_pfa(double pfa);
    void set_cfar_kind(CfarKind k);
    void set_aoa_method(AoaMethod m);
    void set_zero_dopp_blank(int bins);
    void set_dynamic_range_db(double d);
    void freeze(bool on);
    bool frozen() const;

    //-- Calibration -------------------------------------------------------
    /// Find the transmit-leakage peak in the current frame and make it range
    /// zero. Needs nothing in front of the radar. `msg` explains the outcome
    /// either way.
    bool calibrate_range_zero(std::string& msg);

    /// Take the strongest detection to be a boresight reference target and
    /// solve the fixed per-channel correction from it.
    bool calibrate_boresight(std::string& msg);

    /// Record the strongest detection as an off-boresight field point at the
    /// angle the operator states the reference is at.
    bool calibrate_field_point(double az_deg, double el_deg, std::string& msg);

    bool               calibration_save(const std::string& path, std::string& msg);
    bool               calibration_load(const std::string& path, std::string& msg);
    const Calibration& calibration() const;

    //-- Recording ---------------------------------------------------------
    /// Disk is scarce on this machine, so a recording always has a cap and
    /// stops cleanly when it is reached rather than filling the volume.
    bool record_start(const std::string& path, u64 max_bytes, std::string& err);
    void record_stop();
    bool recording() const;
    u64  recorded_bytes() const;

    /// The last frame produced, copied. For tools that want one frame rather
    /// than a stream. Returns false if none has been produced yet.
    bool latest(RdFrame& out) const;

private:
    struct Impl;
    std::unique_ptr<Impl> p_;
};

} // namespace radar
