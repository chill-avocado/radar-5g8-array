//============================================================================
// source.hpp -- where the radar's data comes from
//
// Three things can feed the pipeline and they are deliberately interchangeable:
//
//   simulate   a scene model that produces the same s16 samples the AD9361
//              would, so the whole radar runs and demonstrates on a laptop
//   uhd        a real B210 running the radar gateware
//   file       a recording, replayed at its original rate or as fast as the
//              disk will go
//
// Two shapes of data cross the interface, because the work is split differently
// depending on the source.  With the gateware in the loop the FPGA has already
// done the transforms and the detection, and the host receives finished frames.
// Without it, the host receives raw ADC samples and does everything itself.  A
// source declares which it produces with gives_frames(); the pipeline asks the
// right question and never has to know which source it is talking to.
//
// Nothing here throws.  Every failure sets last_error() to a sentence a person
// can act on and returns false.
//============================================================================
#pragma once

#include "radar/config.hpp"
#include "radar/core.hpp"
#include "radar/types.hpp"

#include <cstdio>
#include <memory>
#include <string>
#include <vector>

namespace radar {

//============================================================================
// Raw samples for one coherent processing interval
//============================================================================

/// A block of raw ADC samples for one coherent processing interval, or an
/// already-processed frame if the FPGA did the work.
///
/// Layout is [rx][chirp][sample], receiver major, because the de-chirp and the
/// range transform walk one receiver's chirps back to back and want them
/// contiguous.  A whole interval at the default operating point is
/// 2 x 512 x 3072 complex samples, 12.6 MB, which is why this is moved and
/// never copied.
struct IqCpi {
    u64    index        = 0;
    double timestamp_s  = 0;
    bool   overflow     = false;   ///< the radio lost samples before this block

    int n_rx          = 0;
    int n_chirp_total = 0;         ///< chirps in the interval across all transmitters
    int n_sweep       = 0;         ///< samples in one chirp's active sweep

    AlignedBuffer<ci16> samples;   ///< [rx][chirp][sample], rx major

    /// First sample of one chirp on one receiver.  No bounds checking: this is
    /// on the hot path and every caller has already looped over valid indices.
    ci16* chirp(int rx, int c) {
        return samples.data() + (std::size_t(rx) * n_chirp_total + c) * n_sweep;
    }
    const ci16* chirp(int rx, int c) const {
        return samples.data() + (std::size_t(rx) * n_chirp_total + c) * n_sweep;
    }

    std::size_t sample_count() const {
        return std::size_t(n_rx) * n_chirp_total * n_sweep;
    }

    /// Make the buffer the right size for this geometry.  Allocates only when
    /// the geometry changes, so reusing one IqCpi over and over never touches
    /// the allocator after the first interval.
    void allocate(int rx, int chirps, int sweep) {
        n_rx = rx; n_chirp_total = chirps; n_sweep = sweep;
        samples.resize(std::size_t(rx) * chirps * sweep);
    }
};

//============================================================================
// The interface
//============================================================================

class IqSource {
public:
    virtual ~IqSource() = default;

    virtual bool open(const Config&) = 0;
    virtual void close()             = 0;
    virtual bool running() const     = 0;

    /// Blocking, with timeout. false on timeout or end of stream.
    virtual bool next_raw(IqCpi& out, double timeout_s) = 0;

    /// Sources that receive already-processed frames from the FPGA override this and
    /// return true; the pipeline then calls next_frame instead of next_raw.
    virtual bool gives_frames() const { return false; }
    virtual bool next_frame(RdFrame& out, double timeout_s) { (void)out; (void)timeout_s; return false; }

    virtual const char* name() const = 0;
    virtual Stats stats() const { return {}; }

    /// Empty until something fails, then a sentence describing what.  Cleared
    /// at the start of every call that can fail, so it always refers to the
    /// most recent one.
    virtual const std::string& last_error() const { return err_; }

    /// True when the stream has genuinely ended -- a recording ran out, a
    /// frame limit was reached -- as opposed to merely timing out.
    virtual bool ended() const { return false; }

protected:
    void set_error(std::string s) { err_ = std::move(s); }
    void clear_error()            { err_.clear(); }
    std::string err_;
};

/// Build the source Config::source names.  Never returns null; if the
/// requested source cannot be built the returned object fails its open() with
/// an explanatory last_error().
std::unique_ptr<IqSource> make_source(const Config&);

//============================================================================
// Scene description for the simulator
//============================================================================

/// One thing in front of the radar.
///
/// Geometry convention, the same one radar::Target uses: boresight is +y,
/// right is +x, up is +z.  Azimuth is measured in the horizontal plane from
/// boresight towards +x; elevation is measured up from that plane.
///
/// Motion is given as a ground speed and a heading rather than a radial
/// velocity, so a target that crosses the beam slows in Doppler and speeds up
/// again by itself instead of having a constant made-up range rate pasted on.
struct SimObject {
    double range_m       = 100.0;
    double azimuth_deg   = 0.0;
    double elevation_deg = 0.0;

    double speed_ms      = 0.0;   ///< horizontal ground speed
    double heading_deg   = 180.0; ///< direction of travel, same frame as azimuth;
                                  ///< 180 means straight at the radar
    double climb_ms      = 0.0;   ///< vertical, positive up

    double rcs_m2        = 0.01;  ///< radar cross section, square metres

    /// Rotor micro-Doppler.  Zero blades means a plain point target.
    int    blades        = 0;     ///< scattering blades in total, all rotors
    double blade_hz      = 0.0;   ///< revolutions per second
    double blade_len_m   = 0.06;  ///< tip radius
    double blade_rcs_m2  = 0.0005;///< per blade, at flash

    /// Body Doppler spread, m/s rms.  A walking person's limbs, a vehicle's
    /// wheels and vibration.  Zero for a rigid body.
    double micro_spread_ms = 0.0;

    std::string label;            ///< only for logging
};

/// Everything the simulator needs to know about the world.
struct SimScene {
    std::vector<SimObject> objects;

    //-- ground clutter -----------------------------------------------------
    bool   clutter_on        = true;
    int    clutter_count     = 96;     ///< stationary scatterers
    double clutter_max_m     = 400.0;
    double clutter_sigma0_db = -25.0;  ///< reflectivity of grass at 5.8 GHz,
                                       ///< low grazing angle, square metre of
                                       ///< cross section per square metre of ground
    double clutter_wind_ms   = 0.15;   ///< rms internal motion, gives the clutter
                                       ///< a Doppler width instead of a line

    //-- receiver -----------------------------------------------------------
    bool   noise_on          = true;
    bool   leakage_on        = true;
    bool   phase_noise_on    = true;
    u32    seed              = 20250810u;

    /// Wall-clock rate to produce intervals at.  0 means as fast as possible,
    /// which is what the tests and the offline runs want.
    double play_rate_hz      = 0.0;
};

/// Three drones at different ranges, speeds and angles, one of them with
/// rotors turning, plus a walking person and a vehicle.  This is what the
/// simulator uses when nothing else is given.
SimScene default_scene();

/// Parse a scene from JSON.  Returns false and fills `err` on a malformed
/// document; the scene is left untouched.  See sim_source.cpp for the schema.
bool parse_scene(const std::string& json_text, SimScene& out, std::string& err);

/// Read a scene from a file.  Same rules.
bool load_scene(const std::string& path, SimScene& out, std::string& err);

/// Build a simulator directly, so a test can hand it a scene without going
/// through a config file.  Equivalent to make_source() with a Simulate config
/// when `scene` is the default one.
std::unique_ptr<IqSource> make_sim_source(const Config&, const SimScene& scene);

//============================================================================
// The gateware's wire format
//
// Section 7 of fpga/rtl/radar_pkg.svh.  One logical frame per interval as a
// run of 32-bit words:
//
//   0        0x52414452  "RADR"
//   1        [31:16] format version, [15:0] flags
//   2        frame index, wrapping u32
//   3        [31:16] n_range_out, [15:0] n_doppler_out
//   4        [31:16] n_hits,      [15:0] reserved
//   5        u32 noise floor
//   6        u64 radio timestamp, low 32 bits
//   7        u64 radio timestamp, high 32 bits
//   ...      n_range_out * n_doppler_out words of integrated power, range major
//   ...      n_hits * 6 words of detection record
//   last     0x454E4452  "ENDR"
//
// Two orderings the package leaves implicit and this code fixes:
//   - where a word holds two 16-bit fields, the first one named occupies the
//     high half, matching word 1 where the version is explicitly [31:16];
//   - the 64-bit timestamp is little-endian by word, low half first, matching
//     the little-endian byte order of everything else on the USB path.
//============================================================================
namespace wire {

constexpr u32 kMagic       = 0x52414452u;   ///< "RADR"
constexpr u32 kEndMark     = 0x454E4452u;   ///< "ENDR"
constexpr u16 kFmtVersion  = 1;
constexpr int kHdrWords    = 8;
constexpr int kHitWords    = 6;

/// Flags in the low half of word 1.
enum Flags : u16 {
    kFlagMap      = 1u << 0,
    kFlagHits     = 1u << 1,
    kFlagOverflow = 1u << 2,
    kFlagTxOn     = 1u << 3,
    kFlagMimoShift = 4,          ///< two bits at [5:4]
};

/// Feed 32-bit words; returns true and fills `out` when a complete frame has
/// been parsed.  Resynchronises on the magic word after any corruption.
///
/// A single call can only return one frame, so when it returns true the caller
/// must advance its input pointer by consumed() and call again with the rest.
/// When it returns false every word was absorbed and consumed() equals n.
class FrameDecoder {
public:
    explicit FrameDecoder(const Config&);

    bool feed(const u32* words, std::size_t n, RdFrame& out);

    /// Words of the last feed() that were used.  Only meaningful immediately
    /// after feed() returns.
    std::size_t consumed() const { return consumed_; }

    u64 resyncs() const     { return resyncs_; }
    u64 bad_frames() const  { return bad_frames_; }
    u64 frames() const      { return frames_; }
    /// Words thrown away while hunting for a magic word.
    u64 skipped_words() const { return skipped_; }

    void reset();

private:
    bool step(RdFrame& out);      ///< examine the buffer; true when a frame came out
    void resync();                ///< discard the current candidate, hunt for a magic
    bool header_plausible() const;
    void emit(RdFrame& out) const;

    // Limits taken from the configuration, so a corrupted length is rejected
    // instead of being believed and used to size an allocation.
    int  max_range_   = 0;
    int  max_dopp_    = 0;
    int  max_hits_    = 0;
    double range_bin_m_ = 0;
    double vel_res_ms_  = 0;
    int    range_zero_  = 0;

    std::vector<u32> buf_;        ///< current candidate frame, capacity fixed at construction
    std::size_t have_     = 0;
    std::size_t want_     = 0;    ///< total words in this frame once the header is known
    std::size_t consumed_ = 0;

    u64 resyncs_    = 0;
    u64 bad_frames_ = 0;
    u64 frames_     = 0;
    u64 skipped_    = 0;
};

/// The inverse, so recordings and the simulator can produce the same bytes.
void encode(const RdFrame&, std::vector<u32>& out, const Config&);

/// Words one frame of this geometry occupies.  Useful for sizing buffers.
std::size_t frame_words(const Config&, int n_hits);

} // namespace wire

//============================================================================
// Recording
//============================================================================

/// Writes frames to disk in the format file_source reads back.
///
/// Disk on the development machine is scarce, so this always has a cap and
/// stops cleanly when it is reached rather than filling the volume.  The full
/// complex range-Doppler cube is 4 x 256 x 256 x 8 bytes = 2 MB per frame,
/// 65 MB per second, so it is left out unless explicitly asked for.
class FileRecorder {
public:
    FileRecorder() = default;
    ~FileRecorder();
    FileRecorder(const FileRecorder&)            = delete;
    FileRecorder& operator=(const FileRecorder&) = delete;

    /// Cap in bytes, applied to the whole file including the header.  Default
    /// is 512 MB.  Set before open().
    void set_max_bytes(u64 n) { max_bytes_ = n; }
    /// Include the per-virtual-channel complex cube.  Off by default.
    void set_write_cube(bool on) { write_cube_ = on; }

    bool open(const Config&, const std::string& path);
    bool open(const Config&, const std::string& path, u64 max_bytes, bool with_cube);
    void write(const RdFrame&);
    void close();

    u64  bytes() const   { return bytes_; }
    u64  frames() const  { return frames_; }
    /// True once the cap stopped it.  Not an error; the file is still valid.
    bool capped() const  { return capped_; }
    const std::string& last_error() const { return err_; }

private:
    std::FILE*  f_          = nullptr;
    u64         bytes_      = 0;
    u64         frames_     = 0;
    u64         max_bytes_  = 512ull * 1024 * 1024;
    bool        write_cube_ = false;
    bool        capped_     = false;
    std::string err_;
    std::vector<unsigned char> scratch_;   ///< serialisation buffer, grown once
};

/// How a recording should be played back.
struct FileSourceOptions {
    bool   loop      = false;
    bool   as_fast   = false;   ///< ignore the original timing
    double rate_mult = 1.0;     ///< 2.0 plays at twice the recorded rate
};

/// Replay a recording.  The path may carry the options inline as
/// "shot.rdr?fast&loop", which is how a Config that only has a path string can
/// still express them.
std::unique_ptr<IqSource> make_file_source(const Config&, const std::string& path,
                                           const FileSourceOptions&);

} // namespace radar
