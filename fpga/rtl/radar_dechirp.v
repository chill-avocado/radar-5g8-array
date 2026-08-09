//============================================================================
// radar_dechirp.v -- the de-chirp: conjugate complex multiply of the received
//                    sample against the reference chirp.
//
// WHAT IT DOES
//   Bit-exactly what radar::fx::cmul_conj_q15(in, ref, 15 + shift) does in
//   soft/include/radar/core.hpp:
//
//       rr = in_i*ref_i + in_q*ref_q
//       ii = in_i*ref_q - in_q*ref_i
//       out = round_sat(rr or ii, 15 + shift, 16)
//
//   round_sat is round-half-up then clamp: add (1 << (sh-1)), arithmetic shift
//   right by sh, THEN saturate to s16.  Ties go towards positive infinity.
//   The order matters -- rounding before clamping is what the C++ does, and a
//   value that only overflows because of the rounding increment must still
//   clamp, not wrap.
//
//   Conjugating the RECEIVED sample (not the reference) is what puts a target
//   at positive beat frequency, so range grows with FFT bin index.
//
//   The extra `shift` beyond 15 is attenuation headroom.  With shift = 0 the
//   multiply has unit gain; each extra bit divides by two, which is how a
//   strong transmit-leakage return is kept out of saturation without touching
//   the analogue gain.
//
// PIPELINE / LATENCY
//   4 clocks, fixed, independent of `shift`.
//     s1  register in/ref/shift                 (DSP48 A and B registers)
//     s2  four 16x16 signed multiplies          (DSP48 P register)
//     s3  the two 34-bit sums
//     s4  round, variable arithmetic shift, saturate
//   in_valid at cycle t produces out_valid at t+4.  `shift` travels with its
//   own sample, so changing it mid-stream never corrupts a sample in flight.
//
// RESOURCES on XC7K325T (defaults, one instance)
//   4 x DSP48E1 (16x16 signed, P register used)
//   ~150 slice LUT (two 34-bit adders, a 34-bit barrel shifter, saturation)
//   ~180 FF
//   Critical path is the barrel shift plus saturate in s4, ~4 ns; radio_clk
//   is 16.28 ns, so there is better than 4x margin.
//============================================================================
`timescale 1ns / 1ps
`default_nettype none

module radar_dechirp #(
    parameter integer DATA_W  = 16,   // s16 Q0.15 in and out
    parameter integer SHIFT_W = 4,    // width of the extra-headroom field
    parameter integer BASE_SH = 15    // Q1.30 product -> Q0.15 costs 15 bits
) (
    input  wire                         clk,
    input  wire                         rst,        // synchronous, active high
    input  wire                         in_valid,
    input  wire signed [DATA_W-1:0]     in_i,       // from the ADC
    input  wire signed [DATA_W-1:0]     in_q,
    input  wire signed [DATA_W-1:0]     ref_i,      // from radar_nco
    input  wire signed [DATA_W-1:0]     ref_q,
    input  wire        [SHIFT_W-1:0]    shift,
    output reg                          out_valid,
    output reg  signed [DATA_W-1:0]     out_i,
    output reg  signed [DATA_W-1:0]     out_q
);

    localparam integer PROD_W = 2 * DATA_W;         // 32
    localparam integer ACC_W  = 2 * DATA_W + 2;     // 34, room for the sum
                                                    // of two products plus the
                                                    // rounding increment
    localparam integer SH_W   = 6;                  // holds 15..30
    localparam [SH_W-1:0] BASE_SH_V = BASE_SH;

    localparam signed [ACC_W-1:0] SAT_HI =
        {{(ACC_W-DATA_W+1){1'b0}}, {(DATA_W-1){1'b1}}};   //  32767
    localparam signed [ACC_W-1:0] SAT_LO =
        {{(ACC_W-DATA_W+1){1'b1}}, {(DATA_W-1){1'b0}}};   // -32768

    //------------------------------------------------------------------------
    // Round-half-up by `sh`, then clamp to DATA_W bits.  Exactly
    // radar::fx::round_sat(v, sh, DATA_W).
    //------------------------------------------------------------------------
    function signed [DATA_W-1:0] round_sat;
        input signed [ACC_W-1:0] v;
        input        [SH_W-1:0]  sh;
        reg   signed [ACC_W-1:0] half;
        reg   signed [ACC_W-1:0] t;
        begin
            half = $signed({{(ACC_W-1){1'b0}}, 1'b1} << (sh - {{(SH_W-1){1'b0}}, 1'b1}));
            t    = (v + half) >>> sh;
            if      (t > SAT_HI) round_sat = SAT_HI[DATA_W-1:0];
            else if (t < SAT_LO) round_sat = SAT_LO[DATA_W-1:0];
            else                 round_sat = t[DATA_W-1:0];
        end
    endfunction

    //------------------------------------------------------------------------
    // s1 : input registers
    //------------------------------------------------------------------------
    reg signed [DATA_W-1:0] a_i_s1, a_q_s1, b_i_s1, b_q_s1;
    reg        [SH_W-1:0]   sh_s1;
    reg                     v_s1;

    wire [SH_W-1:0] sh_in = {{(SH_W-SHIFT_W){1'b0}}, shift} + BASE_SH[SH_W-1:0];

    always @(posedge clk) begin
        a_i_s1 <= in_i;
        a_q_s1 <= in_q;
        b_i_s1 <= ref_i;
        b_q_s1 <= ref_q;
        sh_s1  <= sh_in;
        if (rst) v_s1 <= 1'b0;
        else     v_s1 <= in_valid;
    end

    //------------------------------------------------------------------------
    // s2 : four signed multiplies -- one DSP48E1 each
    //------------------------------------------------------------------------
    reg signed [PROD_W-1:0] p_ii_s2, p_qq_s2, p_iq_s2, p_qi_s2;
    reg        [SH_W-1:0]   sh_s2;
    reg                     v_s2;

    always @(posedge clk) begin
        p_ii_s2 <= a_i_s1 * b_i_s1;
        p_qq_s2 <= a_q_s1 * b_q_s1;
        p_iq_s2 <= a_i_s1 * b_q_s1;
        p_qi_s2 <= a_q_s1 * b_i_s1;
        sh_s2   <= sh_s1;
        if (rst) v_s2 <= 1'b0;
        else     v_s2 <= v_s1;
    end

    //------------------------------------------------------------------------
    // s3 : the conjugate-product sums
    //------------------------------------------------------------------------
    reg signed [ACC_W-1:0] rr_s3, ii_s3;
    reg        [SH_W-1:0]  sh_s3;
    reg                    v_s3;

    wire signed [ACC_W-1:0] p_ii_x = {{(ACC_W-PROD_W){p_ii_s2[PROD_W-1]}}, p_ii_s2};
    wire signed [ACC_W-1:0] p_qq_x = {{(ACC_W-PROD_W){p_qq_s2[PROD_W-1]}}, p_qq_s2};
    wire signed [ACC_W-1:0] p_iq_x = {{(ACC_W-PROD_W){p_iq_s2[PROD_W-1]}}, p_iq_s2};
    wire signed [ACC_W-1:0] p_qi_x = {{(ACC_W-PROD_W){p_qi_s2[PROD_W-1]}}, p_qi_s2};

    always @(posedge clk) begin
        rr_s3 <= p_ii_x + p_qq_x;
        ii_s3 <= p_iq_x - p_qi_x;
        sh_s3 <= sh_s2;
        if (rst) v_s3 <= 1'b0;
        else     v_s3 <= v_s2;
    end

    //------------------------------------------------------------------------
    // s4 : round-half-up, saturate
    //------------------------------------------------------------------------
    always @(posedge clk) begin
        out_i <= round_sat(rr_s3, sh_s3);
        out_q <= round_sat(ii_s3, sh_s3);
        if (rst) out_valid <= 1'b0;
        else     out_valid <= v_s3;
    end

endmodule

`default_nettype wire
