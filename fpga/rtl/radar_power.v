//============================================================================
// radar_power.v -- non-coherent integration across the four virtual channels
//
// PURPOSE
//   The 2x2 MIMO array gives four virtual elements looking at the same
//   (range, Doppler) cell.  Their phases carry the angle information and are
//   used later; their magnitudes carry the detection information and are
//   summed here.  Summing power rather than voltage is "non-coherent": it
//   throws away the phase but needs no knowledge of the target's angle, so a
//   target is found first and its angle measured afterwards.  Four channels
//   summed this way buy about 6 dB of detection sensitivity.
//
// ARITHMETIC -- BIT-EXACT WITH radar::fx::power AND radar::fx::round_sat
//   Per channel:  p = i*i + q*q, unsigned, exact, never rounded, never
//   saturated.  For s16 inputs the largest value is 2*32768^2 = 2^31, which
//   is exactly representable in u32, so this is lossless by construction.
//   Four channels sum to at most 2^33, held in a u34 accumulator.
//   The result is then shifted right by out_shift with round-half-up -- add
//   (1 << (out_shift-1)), shift, then clamp -- and saturated to u32.  This is
//   the unsigned twin of radar::fx::round_sat(); ties go towards +infinity in
//   both, which is what keeps a DC-heavy map from drifting.
//   out_shift defaults to 2 in the register map: dividing the four-channel sum
//   by four returns the map to a per-channel scale, so the noise floor does
//   not move when a channel is disabled.
//
// PROTOCOL
//   The four channels of a cell arrive consecutively on in_ch = 0,1,2,3.
//   in_last may be carried on any word of the group; it is ORed across the
//   group and presented with that cell's output.  A group that starts again
//   at in_ch == 0 restarts the accumulator, so a broken sequence costs one
//   cell and never corrupts the following ones.
//
// LATENCY
//   4 clocks from the in_ch == 3 sample to out_valid
//   (input register -> multiply -> accumulate -> round/saturate).
//   7 clocks from the in_ch == 0 sample of the same cell.
//   Fully pipelined: one channel sample per clock in, one cell per four
//   clocks out, no stalls, no backpressure needed.
//
// RESOURCE ESTIMATE, XC7K325T
//   2 DSP48E1 (one for i*i, one for q*q; a 16x16 signed product fits the
//   25x18 multiplier with room to spare), ~130 LUT, ~150 FF, 0 BRAM.
//============================================================================
`timescale 1ns / 1ps
`default_nettype none

module radar_power (
    input  wire               clk,
    input  wire               rst,

    input  wire               in_valid,
    input  wire signed [15:0] in_i,
    input  wire signed [15:0] in_q,
    input  wire        [1:0]  in_ch,
    input  wire               in_last,

    // Right shift applied to the four-channel sum.  Register-map default 2.
    input  wire        [3:0]  out_shift,

    output reg                out_valid,
    output reg         [31:0] out_pwr,
    output reg                out_last
);

    //------------------------------------------------------------------------
    // Stage A -- register the inputs (this is the DSP48 input register).
    //------------------------------------------------------------------------
    reg               a_valid;
    reg signed [15:0] a_i, a_q;
    reg        [1:0]  a_ch;
    reg               a_last;

    always @(posedge clk) begin
        if (rst) begin
            a_valid <= 1'b0;
            a_i     <= 16'sd0;
            a_q     <= 16'sd0;
            a_ch    <= 2'd0;
            a_last  <= 1'b0;
        end else begin
            a_valid <= in_valid;
            a_i     <= in_i;
            a_q     <= in_q;
            a_ch    <= in_ch;
            a_last  <= in_last;
        end
    end

    //------------------------------------------------------------------------
    // Stage B -- exact per-channel power, i*i + q*q.
    //   Each square is at most 2^30, the sum at most 2^31: both fit u32 with
    //   no rounding and no saturation, exactly as radar::fx::power promises.
    //------------------------------------------------------------------------
    wire signed [31:0] sq_i = a_i * a_i;
    wire signed [31:0] sq_q = a_q * a_q;
    wire        [31:0] pwr_ch = $unsigned(sq_i) + $unsigned(sq_q);

    reg        [31:0] b_pwr;
    reg               b_valid;
    reg        [1:0]  b_ch;
    reg               b_last;

    always @(posedge clk) begin
        if (rst) begin
            b_valid <= 1'b0;
            b_pwr   <= 32'd0;
            b_ch    <= 2'd0;
            b_last  <= 1'b0;
        end else begin
            b_valid <= a_valid;
            b_pwr   <= pwr_ch;
            b_ch    <= a_ch;
            b_last  <= a_last;
        end
    end

    //------------------------------------------------------------------------
    // Stage C -- accumulate the four channels into u34.
    //------------------------------------------------------------------------
    reg [33:0] acc;
    reg        acc_last;
    reg        c_valid;
    reg        c_last;

    wire       ch_first = (b_ch == 2'd0);
    wire       ch_final = (b_ch == 2'd3);

    always @(posedge clk) begin
        if (rst) begin
            acc      <= 34'd0;
            acc_last <= 1'b0;
            c_valid  <= 1'b0;
            c_last   <= 1'b0;
        end else begin
            c_valid <= b_valid & ch_final;
            if (b_valid) begin
                if (ch_first) begin
                    acc      <= {2'b00, b_pwr};
                    acc_last <= b_last;
                    c_last   <= b_last;
                end else begin
                    acc      <= acc + {2'b00, b_pwr};
                    acc_last <= acc_last | b_last;
                    c_last   <= acc_last | b_last;
                end
            end
        end
    end

    //------------------------------------------------------------------------
    // Stage D -- round-half-up shift, saturate to u32.
    //------------------------------------------------------------------------
    wire [4:0]  sh      = {1'b0, out_shift};
    wire [4:0]  sh_m1   = sh - 5'd1;
    wire [35:0] acc_ext = {2'b00, acc};
    wire [35:0] rnd     = (sh == 5'd0) ? 36'd0 : (36'd1 << sh_m1);
    wire [35:0] shifted = (acc_ext + rnd) >> sh;
    wire [31:0] sat_res = (|shifted[35:32]) ? 32'hFFFFFFFF : shifted[31:0];

    always @(posedge clk) begin
        if (rst) begin
            out_valid <= 1'b0;
            out_pwr   <= 32'd0;
            out_last  <= 1'b0;
        end else begin
            out_valid <= c_valid;
            out_pwr   <= sat_res;
            out_last  <= c_last;
        end
    end

endmodule

`default_nettype wire
