//============================================================================
// radar_bitrev.v -- ping-pong bit-reverse reorder buffer for radar_fft
//
// WHAT IT DOES
//   A radix-2 decimation-in-frequency pipeline emits its N results in
//   bit-reversed frequency order: the p-th sample out of the last butterfly is
//   X[bitrev(p)].  This block turns that back into 0,1,2,...,N-1.
//
//   It holds two N-deep buffers.  While one is being filled with the frame now
//   arriving, the other is being emptied with the frame that arrived before it.
//   The banks swap every N samples.  Writes go to bit-reversed addresses and
//   reads come out sequentially, so one sample goes in and one comes out on
//   every enabled clock and consecutive frames never leave a gap.
//
//   Because the write address is bitrev(k) and the read address is k, one
//   counter serves both sides; they advance and wrap together.
//
// LATENCY
//   N + 1 clocks from the first in_valid to the first out_valid, with in_valid
//   held high.  (N to fill the first bank, 1 for the memory output register.)
//
// RESOURCES  (DATA_W = 16, so 32 bits per complex sample)
//   N = 1024 : 2048 x 32 bit  -> 2 x RAMB36E1, ~40 LUT / ~40 FF of control
//   N =  256 :  512 x 32 bit  -> 1 x RAMB18E1, ~35 LUT / ~35 FF of control
//   No DSP48.
//
// The memory array is never reset, which is what lets Vivado map it to block
// RAM; the `primed` flag holds out_valid low until the first bank is full, so
// the undefined power-up contents are never presented as data.
//============================================================================
`timescale 1ns / 1ps
`default_nettype none

module radar_bitrev #(
    parameter integer N      = 1024,   // transform size, power of two
    parameter integer NLOG2  = 10,     // log2(N)
    parameter integer DATA_W = 16      // bits per real component
) (
    input  wire                     clk,
    input  wire                     rst,        // synchronous, active high

    input  wire                     in_valid,
    input  wire signed [DATA_W-1:0] in_i,
    input  wire signed [DATA_W-1:0] in_q,
    input  wire                     in_last,    // marks sample N-1 of a frame

    output reg                      out_valid,
    output reg  signed [DATA_W-1:0] out_i,
    output reg  signed [DATA_W-1:0] out_q,
    output reg                      out_last,   // marks sample N-1 of a frame
    output reg         [NLOG2-1:0]  out_idx     // 0 .. N-1, natural order
);

    localparam integer         W        = 2 * DATA_W;          // packed sample
    localparam [NLOG2-1:0]     IDX_LAST = {NLOG2{1'b1}};   // N-1
    localparam [NLOG2-1:0]     IDX_ONE  = 1;
    localparam [NLOG2-1:0]     IDX_ZERO = 0;

    //------------------------------------------------------------------------
    // Shared read / write counter.  in_last re-aligns it so an upstream frame
    // boundary always lands on index 0 even after a glitch; the compare against
    // IDX_LAST keeps it wrapping if in_last is never driven.
    //------------------------------------------------------------------------
    reg [NLOG2-1:0] cnt;
    reg             wbank;      // bank currently being written
    reg             primed;     // one full frame has been captured

    wire wrap = in_last || (cnt == IDX_LAST);

    always @(posedge clk) begin
        if (rst) begin
            cnt    <= IDX_ZERO;
            wbank  <= 1'b0;
            primed <= 1'b0;
        end else if (in_valid) begin
            if (wrap) begin
                cnt    <= IDX_ZERO;
                wbank  <= ~wbank;
                primed <= 1'b1;
            end else begin
                cnt <= cnt + IDX_ONE;
            end
        end
    end

    //------------------------------------------------------------------------
    // Bit reversal of the counter.  Pure wiring, no logic.
    //------------------------------------------------------------------------
    wire [NLOG2-1:0] cnt_rev;
    genvar gb;
    generate
        for (gb = 0; gb < NLOG2; gb = gb + 1) begin : g_rev
            assign cnt_rev[gb] = cnt[NLOG2-1-gb];
        end
    endgenerate

    //------------------------------------------------------------------------
    // Dual-bank memory.  One write port, one read port, addresses always in
    // different banks, so this is a simple dual-port block RAM.
    //------------------------------------------------------------------------
    (* ram_style = "block" *)
    reg [W-1:0] mem [0:2*N-1];

    always @(posedge clk) begin
        if (in_valid) begin
            mem[{wbank, cnt_rev}] <= {in_q, in_i};
        end
    end

    always @(posedge clk) begin
        if (rst) begin
            out_valid <= 1'b0;
            out_i     <= {DATA_W{1'b0}};
            out_q     <= {DATA_W{1'b0}};
            out_last  <= 1'b0;
            out_idx   <= IDX_ZERO;
        end else begin
            out_valid <= in_valid && primed;
            if (in_valid) begin
                {out_q, out_i} <= mem[{~wbank, cnt}];
                out_idx        <= cnt;
                out_last       <= wrap;
            end
        end
    end

endmodule

`default_nettype wire
