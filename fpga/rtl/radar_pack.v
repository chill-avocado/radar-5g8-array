//============================================================================
// radar_pack.v -- assemble one output frame, radar_pkg.svh section 7
//
// PURPOSE
//   Turn a finished range-Doppler map and its detection list into the stream
//   of 32-bit words the host expects, with a magic word at the front and an
//   end marker at the back so a dropped packet costs one frame and not the
//   stream.
//
// WHY THE MAP IS BUFFERED AND NOT STREAMED STRAIGHT THROUGH
//   Word 4 of the header carries the number of detections, and that number is
//   not known until the very last cell of the map has been through the CFAR.
//   A stream is emitted in order, so nothing at all can leave until the frame
//   is complete -- which means the map has to be held somewhere.  It is held
//   decimated, in a pair of buffers, so the map of the frame being sent out
//   and the map of the frame being built never share a word.  The detections
//   are held the same way, for the reason the map is: they appear during the
//   scan and belong after the map in the packet.
//
// MAP DECIMATION TAKES THE MAXIMUM, NOT A SAMPLE
//   Sending every cfg_map_decim_r-th range bin and every cfg_map_decim_d-th
//   Doppler bin would let a one-cell target fall down the gap and vanish from
//   the operator's display.  Instead each output cell is the LARGEST of the
//   block of cells it stands for, so a target survives decimation with its
//   peak intact and only its position is coarsened.  The block maximum is
//   built up incrementally -- one register carries the run along Doppler and a
//   small row accumulator carries the run down range -- so it costs a single
//   comparison per input cell whatever the decimation factor, and each output
//   cell is written to the map buffer exactly once, on the last input row of
//   its block.  n_range_out and n_doppler_out fall out of the block counters,
//   so no division is needed to fill in header word 3.
//
// FIELD PLACEMENT DECISIONS
//   radar_pkg.svh gives explicit bit positions for header word 1 and for the
//   detection record, and both list the most significant field first.  The
//   two words that give no positions follow the same rule:
//       word 3   [31:16] n_range_out    [15:0] n_doppler_out
//       word 4   [31:16] n_hits         [15:0] reserved (zero)
//       word 6   timestamp[63:32]       word 7  timestamp[31:0]
//   The Doppler bin field of a detection record is eight bits wide, which is
//   one short of the 512 bins the fine-Doppler operating point produces.  The
//   ninth bit goes in bit 16 of the same word, the lowest bit of the byte the
//   contract marks reserved, so a map of 256 Doppler bins is bit-identical to
//   what it was before and bits 23:17 stay zero:
//       +0  [31:24] flags  [23:17] zero  [16] doppler[8]
//           [15:8] doppler[7:0]  [7:0] range
//   Header word 4 reports the number of detection RECORDS actually in this
//   packet, so the host can always parse the packet from the header alone.
//   Upstream capping by cfg_max_hits is what limits that number.
//   Flag bits 0 and 1 are driven from what the packet really contains rather
//   than copied from cfg_flags, so they can never disagree with the payload.
//
// LATENCY
//   Emission starts 2 clocks after frame_end and runs at one word per clock
//   with full backpressure; a whole packet is
//   8 + n_range_out*n_doppler_out + 6*n_hits + 1 words.
//   Memory reads are covered by a four-deep output queue, so m_ready going
//   low never loses a word and never costs throughput once it returns.
//   If a frame ends while the previous packet is still going out, that frame's
//   packet is skipped rather than corrupted; at 62.5 maps a second and a
//   16 384-word ceiling on the map there are more than ten times the clocks
//   needed, so this cannot happen in normal operation.
//
// RESOURCE ESTIMATE, XC7K325T
//   BRAM36: 32 for the two map buffers (16384 words x 32 bits each -- 16384
//           words is a 2x2 decimation of the 65536-cell map the corner turn
//           fixes, and the finest decimation worth buffering on this device),
//           12 for the two detection buffers (1024 records x 192 bits each)
//           = 44 tiles, 10% of the 445 on the device.
//   DSP48 : 0.  Logic: ~700 LUT, ~600 FF.
//============================================================================
`timescale 1ns / 1ps
`default_nettype none

module radar_pack #(
    parameter integer MAP_WORDS = 16384,   // decimated map words per buffer
    parameter integer MAX_HITS  = 1024     // detection records per buffer
) (
    input  wire        clk,
    input  wire        rst,

    // Per-frame configuration, captured at frame_start.
    input  wire        cfg_map_enable,
    input  wire        cfg_hits_enable,
    input  wire [7:0]  cfg_map_decim_r,
    input  wire [7:0]  cfg_map_decim_d,
    input  wire [15:0] cfg_flags,
    input  wire [31:0] frame_index,
    input  wire [63:0] timestamp,
    input  wire [31:0] noise,
    input  wire [8:0]  n_range,
    input  wire [9:0]  n_doppler,

    // Integrated power map, range major, one cell per clock.
    input  wire        map_valid,
    input  wire [31:0] map_pwr,

    // Detections, as they come out of the CFAR during the map scan.
    input  wire        hit_valid,
    input  wire [7:0]  hit_range,
    input  wire [8:0]  hit_dopp,
    input  wire [31:0] hit_pwr,
    input  wire signed [15:0] hit_v0_i, hit_v0_q,
    input  wire signed [15:0] hit_v1_i, hit_v1_q,
    input  wire signed [15:0] hit_v2_i, hit_v2_q,
    input  wire signed [15:0] hit_v3_i, hit_v3_q,

    input  wire [15:0] n_hits,
    input  wire        frame_start,
    input  wire        frame_end,

    // Packed frame out.
    output wire        m_valid,
    output wire [31:0] m_data,
    input  wire        m_ready,
    output wire        m_last
);

    //------------------------------------------------------------------------
    // radar_pkg.svh section 7 constants
    //------------------------------------------------------------------------
    localparam [31:0] RADAR_MAGIC   = 32'h52414452;   // "RADR"
    localparam [31:0] RADAR_ENDMARK = 32'h454E4452;   // "ENDR"
    localparam [15:0] RADAR_FMT_VER = 16'd1;

    function integer clog2;
        input integer v;
        integer i;
        begin
            clog2 = 0;
            for (i = v - 1; i > 0; i = i >> 1) clog2 = clog2 + 1;
        end
    endfunction

    localparam integer MAPAW = clog2(MAP_WORDS);
    localparam integer HITAW = clog2(MAX_HITS);
    localparam integer HITW  = 192;                   // six words per record

    //------------------------------------------------------------------------
    // Configuration held for the frame being built
    //------------------------------------------------------------------------
    reg         q_map_en, q_hit_en;
    reg  [7:0]  q_dec_r, q_dec_d;
    reg  [15:0] q_flags;
    reg  [31:0] q_index;
    reg  [63:0] q_time;
    reg  [8:0]  q_nr;
    reg  [9:0]  q_nd;

    wire [7:0]  dec_r = (q_dec_r == 8'd0) ? 8'd1 : q_dec_r;
    wire [7:0]  dec_d = (q_dec_d == 8'd0) ? 8'd1 : q_dec_d;

