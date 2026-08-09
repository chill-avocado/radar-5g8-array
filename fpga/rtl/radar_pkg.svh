//============================================================================
// radar_pkg.svh -- master contract for the 5.8 GHz TDM-MIMO FMCW radar core
//
// Target : Xilinx Kintex-7 XC7K325T (confirmed by IDCODE 0x03651093 in the
//          OpenSourceSDRLab B210 vendor bitstream)
// Radio  : AD9361, 2 TX / 2 RX, 61.44 MSps master clock
// Antenna: 5.8 GHz 2x2 circularly-polarised MIMO array (this repository)
//
// This header is the single source of truth for
//   - waveform and frame geometry
//   - fixed-point formats at every interface
//   - the settings-bus register map
// Every RTL module and the C++ bit-exact reference model are built against it.
// Nothing here may be duplicated elsewhere; include this file instead.
//
// ---------------------------------------------------------------------------
// WHY THE CHIRP IS GENERATED IN THE FABRIC
// ---------------------------------------------------------------------------
// The de-chirp reference and the transmitted waveform come out of the SAME
// phase accumulator, one clock apart.  There is therefore no unknown delay
// between "what was sent" and "what we correlate against" -- the classic
// failure mode of host-generated FMCW on an SDR, where USB jitter and driver
// buffering leave the range origin floating.  Here the range origin is fixed
// by construction and the only residual is the constant analogue group delay
// through the AD9361, which shows up as the transmit-leakage peak and is
// removed once at calibration (REG_RANGE_ZERO).
//
// It also means no transmit samples cross USB.  The host link carries range-
// Doppler maps and detections instead of raw IQ: 4.4 MB/s where raw streaming
// needs 491 MB/s.
//============================================================================
`ifndef RADAR_PKG_SVH
`define RADAR_PKG_SVH

//----------------------------------------------------------------------------
// 1. CLOCKING AND SAMPLE GEOMETRY
//----------------------------------------------------------------------------
// radio_clk runs at the AD9361 sample rate.  One complex sample per clock per
// receive channel.  Two independent datapath instances, one per RX channel.
`define RADAR_FS_HZ            61440000      // 61.44 MSps, AD9361 master clock
`define RADAR_CLK_NS           16.276        // 1/61.44 MHz, for constraints

// Chirp geometry, in radio_clk cycles.
//   sweep  3072 cycles = 50.000 us of active linear-FM ramp
//   idle    768 cycles = 12.500 us of retrace + PLL settle + TX switch
//   PRI    3840 cycles = 62.500 us
`define RADAR_N_SWEEP          3072
`define RADAR_N_IDLE           768
`define RADAR_N_PRI            4096          // power of two >= sweep+idle; the
                                             // sequencer counts to REG_T_PRI,
                                             // this is only the counter width
`define RADAR_PRI_W            13            // bits for a PRI-length counter
`define RADAR_SWEEP_W          13

//----------------------------------------------------------------------------
// 2. WAVEFORM
//----------------------------------------------------------------------------
// Sweep bandwidth 50 MHz inside the AD9361's 56 MHz analogue passband.
//   range resolution      c / (2B)            = 3.00 m
//   chirp slope mu        B / T_sweep         = 1.0e12 Hz/s
//   beat frequency        2*R*mu/c            = 6666.7 Hz per metre
`define RADAR_B_SWEEP_HZ       50000000
`define RADAR_RANGE_RES_MM     2998            // c/(2B), millimetres

//----------------------------------------------------------------------------
// 3. DECIMATION AND TRANSFORM SIZES
//----------------------------------------------------------------------------
// After de-chirp all the wanted signal sits below the beat frequency of the
// furthest range of interest, so the stream is decimated by 4 in two halfband
// stages before the range transform.  Halfband, not CIC: the wanted band fills
// half the output Nyquist and CIC droop would tilt the range profile.
//   61.44 MSps --HB1--> 30.72 MSps --HB2--> 15.36 MSps
`define RADAR_DECIM            4
`define RADAR_FS_DEC_HZ        15360000

// 3072 sweep samples / 4 = 768, zero-padded to 1024 for the range transform.
// 1.33 bins per resolution cell keeps straddle loss under 0.6 dB.
//   range bin spacing   fs_dec / N_FFT / 6666.7 = 2.25 m
`define RADAR_N_SWEEP_DEC      768
`define RADAR_N_RANGE_FFT      1024
`define RADAR_N_RANGE_FFT_LOG2 10
`define RADAR_RANGE_BIN_MM     2250

// ---------------------------------------------------------------------------
// THE MEMORY CONSTRAINT THAT SETS THE OPERATING POINT
// ---------------------------------------------------------------------------
// The corner-turn buffer holds a whole coherent processing interval on chip,
// twice over so that one frame can be read out while the next is written:
//     n_range * n_chirp_total * n_rx * 4 bytes * 2 buffers
// The XC7K325T has 445 BRAM36K tiles, 2.051 MB.  256 range bins by 512 chirps
// would need 2.048 MB -- 102% of the device, before the radio core takes its
// share.  It does not fit, and there is no external DRAM on this board.
//
// So the PRODUCT is fixed at 65536 words per receive channel per buffer
// (1.0 MB total, 228 tiles, 51%) and the SPLIT is chosen at runtime.  Both
// useful operating points come out of one bitstream:
//
//   surveillance   256 range (576 m) x 256 chirps  -> 62.5 maps/s, 1.62 m/s
//   fine Doppler   128 range (288 m) x 512 chirps  -> 31.2 maps/s, 0.81 m/s
//
// Both are powers of two, so the corner-turn address arithmetic stays a
// variable shift and never needs a multiplier.
`define RADAR_CT_WORDS_LOG2    16            // n_range_log2 + n_chirp_log2
`define RADAR_CT_WORDS         65536

// Defaults: the surveillance point.  62.5 maps a second matters more for
// tracking an agile quadcopter than 0.8 m/s of Doppler resolution does, and
// 576 m of range covers everything the link budget can reach.
`define RADAR_N_RANGE          256
`define RADAR_N_RANGE_LOG2     8
`define RADAR_RANGE_W          8

// Doppler transform: 128 chirps per transmitter in TDM, so 256 chirps total.
//   CPI          = 256 chirps * 62.5 us = 16.0 ms  -> 62.5 maps/second
//   v unambiguous= +/- lambda/(4*T_PRI_eff)        -> +/- 103.4 m/s
//   v resolution = lambda/(2*CPI)                  -> 1.62 m/s
`define RADAR_N_CHIRP          128           // per transmitter
`define RADAR_N_CHIRP_LOG2     7
`define RADAR_N_CHIRP_TOTAL    256           // across transmitters, TDM
`define RADAR_N_DOPPLER        128
`define RADAR_N_DOPPLER_LOG2   7
`define RADAR_CHIRP_W          10

// Virtual array: 2 transmitters x 2 receivers = 4 virtual elements at
// (+/- lambda/4, +/- lambda/4), i.e. lambda/2 spacing on each axis.
`define RADAR_N_TX             2
`define RADAR_N_RX             2
`define RADAR_N_VIRT           4

//----------------------------------------------------------------------------
// 4. FIXED-POINT FORMATS
//----------------------------------------------------------------------------
// Notation Qm.n : one sign bit, m integer bits, n fractional bits.
// Every interface below is signed two's complement unless marked unsigned.
//
//   ADC / DAC sample        s16  Q0.15   full scale = +/- 1.0
//   NCO output              s16  Q0.15
//   de-chirp product        s16  Q0.15   (s32 internal, round-half-up, sat)
//   halfband coefficients   s18  Q0.17
//   halfband accumulator    s40          (round-half-up, sat to s16)
//   window coefficients     s16  Q0.15
//   FFT butterfly data      s16  Q0.15   with per-stage scaling schedule
//   FFT twiddle             s16  Q0.15
//   power  I*I + Q*Q        u32  Q0.32   unsigned, exact, no rounding
//   CFAR accumulator        u48          unsigned
`define RADAR_SAMP_W           16
`define RADAR_HB_COEF_W        18
`define RADAR_HB_ACC_W         40
`define RADAR_WIN_W            16
`define RADAR_TW_W             16
`define RADAR_PWR_W            32
`define RADAR_CFAR_ACC_W       48

// Rounding convention used by every saturating truncation in this design:
// add (1 << (shift-1)) then arithmetic-shift-right, then clamp to the target
// width.  The C++ reference model implements exactly this in
// radar::fx::round_sat().  Any divergence is a bug in one of the two.

//----------------------------------------------------------------------------
// 5. MIMO SCHEME
//----------------------------------------------------------------------------
// TDM  : one transmitter per chirp, alternating.  Clean separation, costs a
//        factor of two in unambiguous velocity and 3 dB in radiated energy.
// DDM  : both transmitters every chirp, TX1 inverted on odd chirps.  TX1's
//        echo lands half a Doppler band away from TX0's, so the two are
//        separated in Doppler instead of in time.  Full duty cycle on both
//        transmitters (+3 dB) and the full PRF for Doppler, at the cost of
//        halving each transmitter's unambiguous Doppler span.
`define RADAR_MIMO_TDM         2'd0
`define RADAR_MIMO_DDM         2'd1
`define RADAR_MIMO_TX0         2'd2          // single transmitter, diagnostics
`define RADAR_MIMO_TX1         2'd3

//----------------------------------------------------------------------------
// 6. SETTINGS-BUS REGISTER MAP
//----------------------------------------------------------------------------
// Reached from the host with uhd::usrp::multi_usrp::set_user_register(addr,
// data).  The B2xx settings bus is 8-bit address, 32-bit data, write-only, so
// status comes back inside the receive stream header instead of by readback.
`define RADAR_REG_BASE         8'd0

`define RADAR_REG_CTRL         8'd0
//   [0]     enable        1 = run the sequencer
//   [1]     soft reset    self-clearing, flushes every pipeline and buffer
//   [2:3]   mimo_mode     see section 5
//   [4]     tx_enable     0 = receive only (passive / listen-before-transmit)
//   [5]     map_enable    1 = stream the range-Doppler power map
//   [6]     hits_enable   1 = stream the CFAR detection list
//   [7]     loopback      1 = drive the de-chirp input from the chirp
//                             generator instead of the ADC (built-in test)
//   [15:8]  reserved
//   [31:16] frame_limit   0 = free running, otherwise stop after N frames

`define RADAR_REG_FREQ_START   8'd1   // s32, NCO phase increment at sweep start
`define RADAR_REG_FREQ_SLOPE   8'd2   // s32, increment added every radio clock
`define RADAR_REG_T_SWEEP      8'd3   // u16 sweep length, radio clocks
`define RADAR_REG_T_PRI        8'd4   // u16 pulse repetition interval, clocks
`define RADAR_REG_N_CHIRP      8'd5   // u16 chirps per transmitter per CPI
`define RADAR_REG_TX_GAIN      8'd6   // u16 Q0.15 transmit amplitude scale
`define RADAR_REG_DECHIRP_SH   8'd7   // u4  post-multiply right shift
`define RADAR_REG_FFT_SCALE_R  8'd8   // u20 range FFT scaling schedule, 2 bits
                                      //     per stage, stage 0 in bits [1:0]
`define RADAR_REG_FFT_SCALE_D  8'd9   // u16 Doppler FFT scaling schedule
`define RADAR_REG_WIN_WADDR    8'd10  // window coefficient write address
`define RADAR_REG_WIN_WDATA    8'd11  // window coefficient write data, s16
                                      //     [31:16] Doppler table, [15:0] range
`define RADAR_REG_CFAR_CFG     8'd12
//   [3:0]   guard_range   guard cells each side in range
//   [7:4]   guard_dopp    guard cells each side in Doppler
//   [11:8]  train_range   training cells each side in range
//   [15:12] train_dopp    training cells each side in Doppler
//   [17:16] cfar_kind     0 = CA, 1 = GO, 2 = SO, 3 = disabled (pass all)
//   [31:18] reserved
`define RADAR_REG_CFAR_ALPHA   8'd13  // u32 Q16.16 threshold multiplier
`define RADAR_REG_RANGE_ZERO   8'd14  // u16 range bin declared to be zero range
`define RADAR_REG_MAP_DECIM    8'd15  // [7:0] range decim, [15:8] Doppler decim
`define RADAR_REG_MAX_HITS     8'd16  // u16 detections reported per CPI
`define RADAR_REG_ZERO_DOPP    8'd17  // u8  Doppler bins around zero to blank
`define RADAR_REG_TEST_TONE    8'd18  // s32 loopback test tone phase increment
`define RADAR_REG_VERSION      8'd19  // write anything: stamps the version word
                                      //   into the next frame header

//----------------------------------------------------------------------------
// 7. OUTPUT PACKET FORMAT
//----------------------------------------------------------------------------
// The core emits one logical frame per CPI as a stream of 32-bit words, which
// the b200 radio front-end packetises into CHDR exactly as it would sample
// data.  The host resynchronises on the magic word, so a dropped packet costs
// one frame and not the stream.
//
//   word  0        0x52414452  "RADR" magic
//   word  1        [31:16] format version (currently 1)
//                  [15:0]  flags: [0] map present, [1] hits present,
//                                 [2] overflow since last frame,
//                                 [3] TX was enabled, [5:4] mimo mode
//   word  2        frame index, wrapping u32
//   word  3        u16 n_range_out, u16 n_doppler_out
//   word  4        u16 n_hits, u16 reserved
//   word  5        u32 noise floor estimate, mean CFAR training power
//   word  6..7     u64 radio timestamp of the first sample of the CPI
//   then n_range_out*n_doppler_out words of u32 integrated power, range major
//   then n_hits * 6 words of detection record:
//        +0  [31:24] flags  [23:16] reserved  [15:8] doppler bin  [7:0] range bin
//        +1  u32 integrated power
//        +2  virtual channel 0, [31:16] I, [15:0] Q
//        +3  virtual channel 1
//        +4  virtual channel 2
//        +5  virtual channel 3
//   final word     0x454E4452  "ENDR" end marker
`define RADAR_MAGIC            32'h52414452
`define RADAR_ENDMARK          32'h454E4452
`define RADAR_FMT_VERSION      16'd1
`define RADAR_HIT_WORDS        6
`define RADAR_HDR_WORDS        8

`endif // RADAR_PKG_SVH
