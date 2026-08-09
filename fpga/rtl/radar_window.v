//============================================================================
// radar_window.v -- apply the range window from a host-writable table.
//
// WHAT IT DOES
//   Multiplies the decimated stream by a coefficient looked up by position
//   within the record, then rounds and saturates back to s16 exactly as
//   radar::fx::round_sat(x * w, 15, 16) does.
//
//   The table lives in one block RAM, 1024 x s16 Q0.15, written over the
//   settings bus (REG_WIN_WADDR then REG_WIN_WDATA).  Putting the window in
//   RAM rather than in a ROM is what lets the sidelobe/resolution trade be
//   changed from the host without a rebuild: a Hann window buys -31 dB
//   sidelobes for a 1.5x wider main lobe, a Blackman-Harris buys -92 dB for
//   2x, and which one you want depends on whether the scene has one big
//   target or several small ones.
//
//   The record is 768 real samples zero-padded to 1024 for the range
//   transform, and the padding is done here: the host writes zeros into
//   entries 768..1023, and anything at all beyond the table depth comes out
//   zero regardless of what is in the table.  So the transform feeder can
//   stream a fixed 1024 whatever the sweep length, and a mis-set t_sweep can
//   never push stale samples into the transform.
//
//   Power-on default is a rectangular window over DEF_ACTIVE samples with a
//   zero tail, so the core produces a sane, if sidelobe-rich, range profile
//   before the host has written anything.
//
// LATENCY
//   3 clocks.  in_valid at cycle t produces out_valid at t+3.
//     +1  block RAM read, data and out-of-range flag delayed alongside
//     +1  multiply           (DSP48E1 P register)
//     +1  round, saturate, force zero when out of range
//   The coefficient write port is independent and may be written at any time;
//   a write and a read to the same address in the same cycle returns the OLD
//   coefficient (read-first), which is the block RAM's natural behaviour and
//   costs nothing because the table is never rewritten mid-record.
//
// RESOURCES on XC7K325T (defaults, one complex channel)
//   1 x RAMB18   (1024 x 16, simple dual port)
//   2 x DSP48E1  (16x16 signed, one per arm)
//   ~90 slice LUT, ~130 FF
//============================================================================
`timescale 1ns / 1ps
`default_nettype none

module radar_window #(
    parameter integer DATA_W     = 16,    // s16 Q0.15 sample
    parameter integer COEF_W     = 16,    // s16 Q0.15 window coefficient
    parameter integer IDX_W      = 16,    // sample index / write address
    parameter integer DEPTH      = 1024,  // table entries
    parameter integer ADDR_W     = 10,    // log2(DEPTH)
    parameter integer OUT_SH     = 15,    // Q0.15 x Q0.15 -> Q0.15
    parameter integer DEF_ACTIVE = 768    // power-on rectangular window length
) (
    input  wire                     clk,
    input  wire                     rst,        // synchronous, active high
    input  wire                     in_valid,
    input  wire signed [DATA_W-1:0] in_i,
    input  wire signed [DATA_W-1:0] in_q,
    input  wire        [IDX_W-1:0]  sample_idx, // position within the record
    input  wire                     coef_we,
    input  wire        [IDX_W-1:0]  coef_addr,
    input  wire signed [COEF_W-1:0] coef_data,
    output reg                      out_valid,
    output reg  signed [DATA_W-1:0] out_i,
    output reg  signed [DATA_W-1:0] out_q
);

    localparam integer PROD_W = DATA_W + COEF_W;        // 32

    // widened so the depth comparisons are done against the integer parameter
    // at its own width, with no truncation anywhere
    wire [31:0] idx32   = {{(32-IDX_W){1'b0}}, sample_idx};
    wire [31:0] waddr32 = {{(32-IDX_W){1'b0}}, coef_addr};

    localparam signed [PROD_W-1:0] SAT_HI =
        {{(PROD_W-DATA_W+1){1'b0}}, {(DATA_W-1){1'b1}}};
    localparam signed [PROD_W-1:0] SAT_LO =
        {{(PROD_W-DATA_W+1){1'b1}}, {(DATA_W-1){1'b0}}};
    localparam signed [PROD_W-1:0] RND_HALF =
        {{(PROD_W-OUT_SH){1'b0}}, 1'b1, {(OUT_SH-1){1'b0}}};

    function signed [DATA_W-1:0] round_sat;
        input signed [PROD_W-1:0] v;
        reg   signed [PROD_W-1:0] t;
        begin
            t = (v + RND_HALF) >>> OUT_SH;
            if      (t > SAT_HI) round_sat = SAT_HI[DATA_W-1:0];
            else if (t < SAT_LO) round_sat = SAT_LO[DATA_W-1:0];
            else                 round_sat = t[DATA_W-1:0];
        end
    endfunction

    //------------------------------------------------------------------------
    // Coefficient table.  Rectangular with a zero tail until the host writes.
    //------------------------------------------------------------------------
    reg signed [COEF_W-1:0] win [0:DEPTH-1];

    integer wi;
    initial begin
        for (wi = 0; wi < DEPTH; wi = wi + 1)
            win[wi] = (wi < DEF_ACTIVE) ? {1'b0, {(COEF_W-1){1'b1}}}
                                        : {COEF_W{1'b0}};
    end

    //------------------------------------------------------------------------
    // Stage 1 : table read, plus the data and the out-of-range flag alongside
    //------------------------------------------------------------------------
    reg signed [COEF_W-1:0] coef_s1;
    reg signed [DATA_W-1:0] d_i_s1, d_q_s1;
    reg                     oob_s1;
    reg                     v_s1;

    always @(posedge clk) begin
        if (coef_we && (waddr32 < DEPTH))
            win[coef_addr[ADDR_W-1:0]] <= coef_data;
        coef_s1 <= win[sample_idx[ADDR_W-1:0]];
        d_i_s1  <= in_i;
        d_q_s1  <= in_q;
        oob_s1  <= (idx32 >= DEPTH);
        if (rst) v_s1 <= 1'b0;
        else     v_s1 <= in_valid;
    end

    //------------------------------------------------------------------------
    // Stage 2 : multiply
    //------------------------------------------------------------------------
    reg signed [PROD_W-1:0] p_i_s2, p_q_s2;
    reg                     oob_s2;
    reg                     v_s2;

    always @(posedge clk) begin
        p_i_s2 <= d_i_s1 * coef_s1;
        p_q_s2 <= d_q_s1 * coef_s1;
        oob_s2 <= oob_s1;
        if (rst) v_s2 <= 1'b0;
        else     v_s2 <= v_s1;
    end

    //------------------------------------------------------------------------
    // Stage 3 : round, saturate, zero anything past the end of the table
    //------------------------------------------------------------------------
    always @(posedge clk) begin
        out_i <= oob_s2 ? {DATA_W{1'b0}} : round_sat(p_i_s2);
        out_q <= oob_s2 ? {DATA_W{1'b0}} : round_sat(p_q_s2);
        if (rst) out_valid <= 1'b0;
        else     out_valid <= v_s2;
    end

endmodule

`default_nettype wire
