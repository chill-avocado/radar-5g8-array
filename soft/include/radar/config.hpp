//============================================================================
// config.hpp -- the radar's operating point, in one struct
//
// Mirrors fpga/rtl/radar_pkg.svh.  Anything that would need the FPGA rebuilt
// is a compile-time constant there and a default here; anything settable over
// the settings bus is a field.  derive() fills in every quantity that follows
// from the others, so nothing downstream ever recomputes a range scale or a
// Doppler bin width for itself.
//============================================================================
#pragma once

#include "radar/core.hpp"
#include <string>

namespace radar {

enum class MimoMode : int { Tdm = 0, Ddm = 1, Tx0Only = 2, Tx1Only = 3 };
enum class CfarKind : int { Ca = 0, Go = 1, So = 2, Os = 3, None = 4 };
enum class WindowKind : int { Rect, Hann, Hamming, BlackmanHarris, Taylor, Chebyshev };
enum class AoaMethod : int { Monopulse, Bartlett, Capon, Music };

/// Where the IQ (or already-processed frames) come from.
enum class SourceKind : int {
    Simulate,   ///< synthetic scene through the bit-exact model -- no hardware
    Uhd,        ///< a real B210 running the radar gateware
    File,       ///< replay of a recording
};

struct Config {
    //------------------------------------------------------------------
    // Front end
    //------------------------------------------------------------------
    SourceKind  source          = SourceKind::Simulate;
    std::string device_args     = "type=b200";
    double      centre_freq_hz  = 5.800e9;
    double      sample_rate_hz  = 61.44e6;   ///< AD9361 master clock
    double      tx_gain_db      = 70.0;      ///< B210 TX gain, 0..89.75
    double      rx_gain_db      = 40.0;      ///< B210 RX gain, 0..76
    double      rx_bandwidth_hz = 56.0e6;    ///< AD9361 analogue filter
    std::string clock_source    = "internal";
    std::string time_source     = "internal";

    //------------------------------------------------------------------
    // Waveform.  Defaults are the operating point radar-plan computes as
    // optimal for this array, this radio and a small-drone target.
    //------------------------------------------------------------------
    double   sweep_bw_hz   = 50.0e6;   ///< linear-FM sweep width
    int      n_sweep       = 3072;     ///< sweep length, radio clocks
    int      n_pri         = 3840;     ///< pulse repetition interval, clocks
    int      n_chirp       = 256;      ///< chirps per transmitter per CPI
    MimoMode mimo          = MimoMode::Tdm;
    bool     tx_enable     = true;

    //------------------------------------------------------------------
    // Transform chain
    //------------------------------------------------------------------
    int        decim         = 4;      ///< halfband cascade, 61.44 -> 15.36 MSps
    int        n_range_fft   = 1024;
    int        n_range       = 256;    ///< range bins kept and streamed
    int        n_doppler     = 256;
    WindowKind range_window  = WindowKind::BlackmanHarris;
    WindowKind dopp_window   = WindowKind::BlackmanHarris;

    //------------------------------------------------------------------
    // Detection
    //------------------------------------------------------------------
    CfarKind cfar_kind    = CfarKind::Ca;
    int      guard_range  = 2;
    int      guard_dopp   = 2;
    int      train_range  = 6;
    int      train_dopp   = 6;
    double   pfa          = 1e-5;
    int      max_hits     = 512;
    int      zero_dopp_blank = 2;   ///< Doppler bins each side of DC to ignore
    int      range_zero_bin  = 0;   ///< bin of the transmit-leakage peak

    //------------------------------------------------------------------
    // Angle, clustering, tracking
    //------------------------------------------------------------------
    AoaMethod aoa            = AoaMethod::Music;
    int       aoa_az_bins    = 181;
    int       aoa_el_bins    = 91;
    double    aoa_az_span_deg = 90.0;   ///< +/- this
    double    aoa_el_span_deg = 45.0;
    double    cluster_eps_m  = 8.0;
    double    cluster_eps_ms = 2.0;
    double    cluster_eps_deg = 10.0;
    int       cluster_min_pts = 1;
    double    track_gate_chi2 = 16.0;   ///< 3 dof, ~99.9%
    int       track_confirm_n = 3;
    int       track_drop_n    = 5;
    double    track_q_accel   = 12.0;   ///< process noise, m/s^2, drone-agile

    //------------------------------------------------------------------
    // Host runtime
    //------------------------------------------------------------------
    int         worker_threads = 3;
    bool        realtime       = true;
    int         http_port      = 8730;
    std::string web_root       = "";     ///< empty = use the built-in page
    std::string record_path    = "";
    std::string calib_path     = "";

    //------------------------------------------------------------------
    // Derived.  Filled by derive(); never set these by hand.
    //------------------------------------------------------------------
    struct Derived {
        double lambda_m         = 0;
        double chirp_slope_hz_s = 0;   ///< sweep_bw / T_sweep
        double t_sweep_s        = 0;
        double t_pri_s          = 0;
        double t_cpi_s          = 0;
        double fs_dec_hz        = 0;
        double range_res_m      = 0;   ///< c / (2 B)
        double range_bin_m      = 0;   ///< bin spacing after zero padding
        double range_max_m      = 0;   ///< of the bins actually kept
        double vel_res_ms       = 0;
        double vel_max_ms       = 0;   ///< unambiguous, +/-
        double frame_rate_hz    = 0;
        double usb_bytes_s      = 0;   ///< what the host link actually carries
        int    n_virt           = 4;
        int    n_chirp_total    = 0;   ///< chirps per CPI across transmitters
        int    n_sweep_dec      = 0;
    } d;

    void derive();
    /// Returns an empty string when the configuration is realisable, otherwise
    /// a plain-English description of the first thing that is not.
    std::string validate() const;

    /// Settings-bus words, ready for multi_usrp::set_user_register().
    /// Pairs of (address, data) in the order they must be written.
    std::vector<std::pair<u8, u32>> register_writes() const;
};

/// Load from / save to a JSON file.  Missing keys keep their defaults, so a
/// config file only has to name what it changes.
Config load_config(const std::string& path);
void   save_config(const Config& c, const std::string& path);

/// Named operating points.  "fast" is the default; "wide" trades frame rate
/// for range resolution by hopping the carrier across the ISM band and
/// stitching the sub-bands coherently.
Config profile(const std::string& name);

} // namespace radar
