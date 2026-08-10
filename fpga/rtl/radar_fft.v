//============================================================================
// radar_fft.v -- radix-2 single-path delay-feedback (R2SDF) streaming FFT
//
// WHAT IT DOES
//   One complex sample in per enabled clock, one complex spectral sample out
//   per enabled clock, for ever.  There is no load phase, no unload phase and
//   no gap between frames: frame m+1 is being fed while frame m is still
//   coming out.  Forward transform, W_N^m = exp(-j*2*pi*m/N).
//
// STRUCTURE
//   NLOG2 stages.  Stage k owns a feedback delay line of depth N>>(k+1), one
//   radix-2 butterfly and one twiddle multiplier.  The classic R2SDF schedule:
//   for the first N>>(k+1) samples of its block the stage pushes the input into
//   the delay line and passes the line's output straight through; for the next
//   N>>(k+1) it emits (delayed + input) and stores (delayed - input).  The
//   stored differences reappear a half-block later and are twiddled on the way
//   out of the feedback path.  That is why a single delay line does the work of
//   a whole memory bank -- the sums leave immediately and only the differences
//   are held.
//
//   Decimation in frequency, so the results leave in bit-reversed order.  With
//   NATURAL_OUT = 1 a radar_bitrev ping-pong buffer puts them back in order.
//
//   The stage counters free-run.  They need no start-of-frame signal because
//   stage k-1's own start-up delay is exactly one full period of stage k's
//   counter (N>>k == 2*(N>>(k+1))), so every stage wakes up already in phase.
//   in_last is still honoured: it reloads stage 0's counter, which lets the
//   core re-synchronise to the upstream framing after any disturbance.
//
// SCALING
//   scale_sch carries two bits per stage, stage 0 in bits [1:0], giving a right
//   shift of 0..3 applied to both butterfly outputs.  The shift is round-half-
//   up then saturate to DATA_W, bit-identical to radar::fx::round_sat(v, s, 16)
//   in soft/include/radar/core.hpp.  All-ones (>>1 every stage) gives an
//   overall gain of 1/N; in that mode the butterfly provably cannot saturate,
//   because (a+b+1)>>1 of two DATA_W values is itself a DATA_W value.
//   `overflow` is sticky until rst and is armed once the pipeline is primed,
//   so the start-up flush never trips it.
//
// TRIVIAL STAGES
//   A stage whose delay depth is 2 needs only W_4^0 = +1 and W_4^1 = -j, and
//   the final stage (depth 1) needs only +1.  Neither builds a multiplier: -j
//   is a swap and a saturating negate.  So the last two stages are free.
//
// LATENCY, in clocks from the first in_valid to the first out_valid with
//   in_valid held high.  Every figure below is measured by fpga/sim/tb_fft.cpp,
//   not predicted.
//     NATURAL_OUT = 1 : 3*NLOG2 + 2*N - 1
//       N=1024 -> 2077   N=512 -> 1050   N=256 -> 535   N=128 -> 276
//     NATURAL_OUT = 0 : 3*NLOG2 + N - 2      (the reorder buffer costs N+1)
//       N=1024 -> 1052
//   Of that, N-1 clocks are the delay-line flush, 3*NLOG2 are pipeline
//   registers and, with NATURAL_OUT = 1, N+1 are the reorder buffer.
//
// RESOURCE ESTIMATE  (DATA_W = TW_W = 16, XC7K325T, NATURAL_OUT = 1)
//   A complex sample is 32 bits.  Feedback lines of depth >= 16 and the
//   reorder buffer carry ram_style = "block", so their mapping is fixed, not
//   left to the tool.  A 16x16 product is one DSP48E1, so a complex product is
//   four.  Stages of depth 2 and 1 build no multiplier at all.
//
//                            N = 1024   N = 512   N = 256   N = 128
//   feedback depths >= 16      512..16    256..16   128..16    64..16
//     as RAMB18 (32 b wide)          6          5         4         3
//   feedback depths < 16       8,4,2,1    8,4,2,1   8,4,2,1   8,4,2,1
//     as SRL16 (480 b)              30         30        30        30
//   twiddle ROM entries           1020       508        252       124
//     bits (32 b per entry)      32640     16256       8064      3968
//     as RAMB18                        2         1         1         0
//     remainder as LUT ROM         ~250      ~130       ~120       ~60
//   reorder buffer, 2N x 32 b  2xRAMB36  1xRAMB36  1xRAMB18  1xRAMB18
//   --------------------------------------------------------------------
//   BRAM, in RAMB18 equivalents     12         8         6         4
//   DSP48E1                         32        28         24        20
//   registers                    ~2400      ~2150      ~1850     ~1600
//
//   One receive channel needs a 1024-point range transform and one Doppler
//   transform of 512, 256 or 128, so two channels come to 112 DSP48E1 of the
//   device's 840 and about 10 RAMB36 of 445 -- the corner-turn buffer between
//   them, at 228 tiles, is what actually costs memory here.
//
// Widths and rounding come from fpga/rtl/radar_pkg.svh, which is the single
// source of truth for this design.
//============================================================================
`timescale 1ns / 1ps
`default_nettype none

`include "radar_pkg.svh"

module radar_fft #(
    parameter integer N           = `RADAR_N_RANGE_FFT,       // 1024
    parameter integer NLOG2       = `RADAR_N_RANGE_FFT_LOG2,  // 10
    parameter integer DATA_W      = `RADAR_SAMP_W,            // 16
    parameter integer TW_W        = `RADAR_TW_W,              // 16
    parameter integer NATURAL_OUT = 1
) (
    input  wire                      clk,
    input  wire                      rst,        // synchronous, active high

    input  wire                      in_valid,
    input  wire signed [DATA_W-1:0]  in_i,
    input  wire signed [DATA_W-1:0]  in_q,
    input  wire                      in_last,    // marks sample N-1 of a frame

    input  wire [2*NLOG2-1:0]        scale_sch,  // 2 bits per stage, stage 0 low

    output wire                      out_valid,
    output wire signed [DATA_W-1:0]  out_i,
    output wire signed [DATA_W-1:0]  out_q,
    output wire                      out_last,   // marks bin N-1 of a frame
    output wire        [NLOG2-1:0]   out_idx,    // frequency bin of this sample
    output reg                       overflow    // sticky until rst
);

    localparam [NLOG2-1:0] IDX_ZERO  = 0;
    localparam [NLOG2-1:0] IDX_ONE   = 1;
    localparam [NLOG2-1:0] IDX_LAST  = {NLOG2{1'b1}};              // N-1
    localparam [NLOG2-1:0] PRIME_END = {{(NLOG2-1){1'b1}}, 1'b0};  // N-2

    //------------------------------------------------------------------------
    // The stage chain.  Element 0 is the module input, element NLOG2 is the
    // last butterfly's output.
    //------------------------------------------------------------------------
    wire                     st_valid [0:NLOG2];
    wire                     st_sync  [0:NLOG2];
    wire signed [DATA_W-1:0] st_i     [0:NLOG2];
    wire signed [DATA_W-1:0] st_q     [0:NLOG2];
    wire [NLOG2-1:0]         st_ovf;

    assign st_valid[0] = in_valid;
    assign st_sync[0]  = in_last;
    assign st_i[0]     = in_i;
    assign st_q[0]     = in_q;

    genvar gk;
    generate
        for (gk = 0; gk < NLOG2; gk = gk + 1) begin : g_stage
            radar_fft_stage #(
                .N      (N),
                .NLOG2  (NLOG2),
                .STAGE  (gk),
                .DATA_W (DATA_W),
                .TW_W   (TW_W)
            ) u_stage (
                .clk     (clk),
                .rst     (rst),
                .shift   (scale_sch[2*gk+1 -: 2]),
                .s_valid (st_valid[gk]),
                .s_sync  (st_sync[gk]),
                .s_i     (st_i[gk]),
                .s_q     (st_q[gk]),
                .m_valid (st_valid[gk+1]),
                .m_sync  (st_sync[gk+1]),
                .m_i     (st_i[gk+1]),
                .m_q     (st_q[gk+1]),
                .ovf     (st_ovf[gk])
            );
        end
    endgenerate

    //------------------------------------------------------------------------
    // Priming and output indexing.
    //
    // The delay lines make the first N-1 samples out of the last butterfly a
    // flush of whatever the memories powered up holding.  Exactly N-1, because
    // the flush is the sum of every stage's depth: N/2 + N/4 + ... + 1.  After
    // those the very first complete transform -- of input frame 0 -- arrives,
    // in bit-reversed order, and never stops.
    //------------------------------------------------------------------------
    wire core_raw = st_valid[NLOG2];

    reg [NLOG2-1:0] pcnt;
    reg             primed;
    reg [NLOG2-1:0] ocnt;

    always @(posedge clk) begin
        if (rst) begin
            pcnt   <= IDX_ZERO;
            primed <= 1'b0;
            ocnt   <= IDX_ZERO;
        end else if (core_raw) begin
            if (!primed) begin
                if (pcnt == PRIME_END) primed <= 1'b1;
                pcnt <= pcnt + IDX_ONE;
            end else begin
                ocnt <= (ocnt == IDX_LAST) ? IDX_ZERO : (ocnt + IDX_ONE);
            end
        end
    end

    wire core_valid = core_raw && primed;
    wire core_last  = (ocnt == IDX_LAST);

    //------------------------------------------------------------------------
    // Sticky overflow, armed once the flush is out of the pipeline.
    //------------------------------------------------------------------------
    always @(posedge clk) begin
        if (rst)                        overflow <= 1'b0;
        else if (primed && (|st_ovf))   overflow <= 1'b1;
    end

    //------------------------------------------------------------------------
    // Output ordering.
    //------------------------------------------------------------------------
    generate
        if (NATURAL_OUT != 0) begin : g_natural
            radar_bitrev #(
                .N      (N),
                .NLOG2  (NLOG2),
                .DATA_W (DATA_W)
            ) u_bitrev (
                .clk       (clk),
                .rst       (rst),
                .in_valid  (core_valid),
                .in_i      (st_i[NLOG2]),
                .in_q      (st_q[NLOG2]),
                .in_last   (core_last),
                .out_valid (out_valid),
                .out_i     (out_i),
                .out_q     (out_q),
                .out_last  (out_last),
                .out_idx   (out_idx)
            );
        end else begin : g_reversed
            // Straight out of the last butterfly.  out_idx still names the
            // frequency bin, which is the bit reverse of the position.
            wire [NLOG2-1:0] ocnt_rev;
            genvar gr;
            for (gr = 0; gr < NLOG2; gr = gr + 1) begin : g_orev
                assign ocnt_rev[gr] = ocnt[NLOG2-1-gr];
            end
            assign out_valid = core_valid;
            assign out_i     = st_i[NLOG2];
            assign out_q     = st_q[NLOG2];
            assign out_last  = core_last;
            assign out_idx   = ocnt_rev;
        end
    endgenerate

endmodule


//============================================================================
// radar_fft_stage -- one R2SDF stage: delay line, butterfly, twiddle
//
// Latency 3 enabled clocks, the same for every stage whether or not it builds
// a multiplier, so the whole pipeline is 3*NLOG2 plus the delay-line flush.
//   1 : butterfly result and twiddle ROM lookup
//   2 : the four partial products (or the trivial twiddle)
//   3 : product sum, round, saturate
//============================================================================
module radar_fft_stage #(
    parameter integer N      = 1024,
    parameter integer NLOG2  = 10,
    parameter integer STAGE  = 0,
    parameter integer DATA_W = 16,
    parameter integer TW_W   = 16
) (
    input  wire                     clk,
    input  wire                     rst,
    input  wire [1:0]               shift,    // 0..3 bits of right shift

    input  wire                     s_valid,
    input  wire                     s_sync,   // this sample ends a block
    input  wire signed [DATA_W-1:0] s_i,
    input  wire signed [DATA_W-1:0] s_q,

    output reg                      m_valid,
    output reg                      m_sync,   // rides with the same sample
    output reg  signed [DATA_W-1:0] m_i,
    output reg  signed [DATA_W-1:0] m_q,
    output wire                     ovf
);

    localparam integer DEPTH = N >> (STAGE + 1);  // feedback delay, in samples
    localparam integer CW    = NLOG2 - STAGE;     // block counter width, 2*DEPTH
    localparam integer BW    = DATA_W + 2;        // butterfly working width
    localparam integer MW    = 2*DATA_W + 2;      // product working width

    localparam [CW-1:0]        CNT_ZERO = 0;
    localparam [CW-1:0]        CNT_ONE  = 1;
    localparam signed [BW-1:0] LIM_HI   =  ((1 << (DATA_W-1)) - 1);
    localparam signed [BW-1:0] LIM_LO   = -(1 << (DATA_W-1));
    localparam signed [MW-1:0] MLIM_HI  =  ((1 << (DATA_W-1)) - 1);
    localparam signed [MW-1:0] MLIM_LO  = -(1 << (DATA_W-1));
    localparam signed [MW-1:0] MHALF    =  (1 << (TW_W-2));   // 0.5 LSB of Q0.15
    localparam signed [BW-1:0] HALF1    = 1;
    localparam signed [BW-1:0] HALF2    = 2;
    localparam signed [BW-1:0] HALF3    = 4;
    localparam signed [BW-1:0] HALF0    = 0;
    localparam signed [DATA_W-1:0] SAT_P =  ((1 << (DATA_W-1)) - 1);
    localparam signed [DATA_W-1:0] SAT_N = -(1 << (DATA_W-1));

    //------------------------------------------------------------------------
    // radar::fx::round_sat, split into the shift and the clamp so the clamp can
    // also report that it fired.
    //------------------------------------------------------------------------
    function signed [BW-1:0] rnd_shift;
        input signed [BW-1:0] v;
        input [1:0]           sh;
        reg   signed [BW-1:0] half;
        begin
            case (sh)
                2'd0:    half = HALF0;
                2'd1:    half = HALF1;
                2'd2:    half = HALF2;
                default: half = HALF3;
            endcase
            rnd_shift = (v + half) >>> sh;
        end
    endfunction

    function signed [DATA_W-1:0] sat_b;
        input signed [BW-1:0] v;
        begin
            if      (v > LIM_HI) sat_b = SAT_P;
            else if (v < LIM_LO) sat_b = SAT_N;
            else                 sat_b = v[DATA_W-1:0];
        end
    endfunction

    function ovf_b;
        input signed [BW-1:0] v;
        begin
            ovf_b = (v > LIM_HI) || (v < LIM_LO);
        end
    endfunction

    function signed [DATA_W-1:0] sat_m;
        input signed [MW-1:0] v;
        begin
            if      (v > MLIM_HI) sat_m = SAT_P;
            else if (v < MLIM_LO) sat_m = SAT_N;
            else                  sat_m = v[DATA_W-1:0];
        end
    endfunction

    function ovf_m;
        input signed [MW-1:0] v;
        begin
            ovf_m = (v > MLIM_HI) || (v < MLIM_LO);
        end
    endfunction

    //------------------------------------------------------------------------
    // Block counter.  Top bit selects the half of the block: 0 = fill the
    // delay line and pass it through, 1 = butterfly.
    //------------------------------------------------------------------------
    reg [CW-1:0] cnt;
    always @(posedge clk) begin
        if (rst)           cnt <= CNT_ZERO;
        else if (s_valid)  cnt <= s_sync ? CNT_ZERO : (cnt + CNT_ONE);
    end
    wire sel = cnt[CW-1];

    //------------------------------------------------------------------------
    // Feedback delay line, exactly DEPTH enabled clocks deep.
    //------------------------------------------------------------------------
    wire [2*DATA_W-1:0] sr_in;
    wire [2*DATA_W-1:0] sr_out;

    radar_fft_delay #(
        .W     (2*DATA_W),
        .DEPTH (DEPTH)
    ) u_delay (
        .clk  (clk),
        .en   (s_valid),
        .din  (sr_in),
        .dout (sr_out)
    );

    wire signed [DATA_W-1:0] fb_i = sr_out[DATA_W-1:0];
    wire signed [DATA_W-1:0] fb_q = sr_out[2*DATA_W-1:DATA_W];

    //------------------------------------------------------------------------
    // Butterfly.  The delay line output is the earlier sample, so sum and
    // difference are (delayed +/- incoming), which is the right sense for
    // decimation in frequency.
    //------------------------------------------------------------------------
    wire signed [BW-1:0] ai = {{2{fb_i[DATA_W-1]}}, fb_i};
    wire signed [BW-1:0] aq = {{2{fb_q[DATA_W-1]}}, fb_q};
    wire signed [BW-1:0] bi = {{2{s_i [DATA_W-1]}}, s_i };
    wire signed [BW-1:0] bq = {{2{s_q [DATA_W-1]}}, s_q };

    wire signed [BW-1:0] sum_i_s = rnd_shift(ai + bi, shift);
    wire signed [BW-1:0] sum_q_s = rnd_shift(aq + bq, shift);
    wire signed [BW-1:0] dif_i_s = rnd_shift(ai - bi, shift);
    wire signed [BW-1:0] dif_q_s = rnd_shift(aq - bq, shift);

    wire signed [DATA_W-1:0] sum_i_c = sat_b(sum_i_s);
    wire signed [DATA_W-1:0] sum_q_c = sat_b(sum_q_s);
    wire signed [DATA_W-1:0] dif_i_c = sat_b(dif_i_s);
    wire signed [DATA_W-1:0] dif_q_c = sat_b(dif_q_s);

    wire bf_ovf = sel && (ovf_b(sum_i_s) || ovf_b(sum_q_s) ||
                          ovf_b(dif_i_s) || ovf_b(dif_q_s));

    assign sr_in = sel ? {dif_q_c, dif_i_c} : {s_q, s_i};

    wire signed [DATA_W-1:0] bf_i = sel ? sum_i_c : fb_i;
    wire signed [DATA_W-1:0] bf_q = sel ? sum_q_c : fb_q;

    //------------------------------------------------------------------------
    // Pipeline stage 1: butterfly result.
    //------------------------------------------------------------------------
    reg                     p1_valid;
    // The depth-1 stage has no twiddle at all, so it never reads p1_sel.
    /* verilator lint_off UNUSEDSIGNAL */
    reg                     p1_sel;
    /* verilator lint_on UNUSEDSIGNAL */
    reg signed [DATA_W-1:0] p1_i, p1_q;

    always @(posedge clk) begin
        if (rst) begin
            p1_valid <= 1'b0;
            p1_sel   <= 1'b0;
            p1_i     <= {DATA_W{1'b0}};
            p1_q     <= {DATA_W{1'b0}};
        end else begin
            p1_valid <= s_valid;
            if (s_valid) begin
                p1_sel <= sel;
                p1_i   <= bf_i;
                p1_q   <= bf_q;
            end
        end
    end

    //------------------------------------------------------------------------
    // The valid and end-of-block flags run down the middle of the stage,
    // shared by all three twiddle variants, so every stage is exactly three
    // enabled clocks long whether or not it builds a multiplier.
    //
    // s_sync rides with its sample rather than being regenerated, so a frame
    // boundary that moves upstream moves every stage's block counter together.
    // Realigning only the first stage would shear the pipeline.
    //------------------------------------------------------------------------
    reg p2_valid;
    reg p1_sync, p2_sync;

    always @(posedge clk) begin
        if (rst) begin
            p2_valid <= 1'b0;
            m_valid  <= 1'b0;
            p1_sync  <= 1'b0;
            p2_sync  <= 1'b0;
            m_sync   <= 1'b0;
        end else begin
            p2_valid <= p1_valid;
            m_valid  <= p2_valid;
            if (s_valid)  p1_sync <= s_sync;
            if (p1_valid) p2_sync <= p1_sync;
            if (p2_valid) m_sync  <= p2_sync;
        end
    end

    //------------------------------------------------------------------------
    // Pipeline stages 2 and 3: the twiddle.  p1_sel = 1 is the sum half, which
    // carries no twiddle at all; p1_sel = 0 is the differences coming back out
    // of the delay line, which do.
    //------------------------------------------------------------------------
    wire mul_ovf;

    generate
    if (DEPTH >= 4) begin : g_mult
        //--------------------------------------------------------------------
        // Full complex multiplier.  Twiddle m of this stage is W_L^m with
        // L = 2*DEPTH, i.e. exp(-j*2*pi*m/L), s16 Q0.15.
        //--------------------------------------------------------------------
        // Cosine and sine share one array: they are always read together, at
        // the same address, so this is one memory and one address decode.
        reg [2*TW_W-1:0] rom [0:DEPTH-1];

        integer t;
        // Only the low TW_W bits of the rounded value are kept; it is already
        // known to fit, so the upper bits are deliberately discarded.
        /* verilator lint_off UNUSEDSIGNAL */
        integer ci, si;
        /* verilator lint_on UNUSEDSIGNAL */
        real    ang, cv, sv;
        initial begin
            for (t = 0; t < DEPTH; t = t + 1) begin
                ang = (-2.0 * 3.14159265358979323846 * t) / (2.0 * DEPTH);
                cv  = 32767.0 * $cos(ang);
                sv  = 32767.0 * $sin(ang);
                // round half away from zero
                ci  = (cv >= 0.0) ? $rtoi(cv + 0.5) : $rtoi(cv - 0.5);
                si  = (sv >= 0.0) ? $rtoi(sv + 0.5) : $rtoi(sv - 0.5);
                rom[t] = {si[TW_W-1:0], ci[TW_W-1:0]};
            end
        end

        reg [2*TW_W-1:0] p1_tw;
        always @(posedge clk) begin
            if (s_valid) p1_tw <= rom[cnt[CW-2:0]];
        end
        wire signed [TW_W-1:0] p1_tc = p1_tw[TW_W-1:0];
        wire signed [TW_W-1:0] p1_ts = p1_tw[2*TW_W-1:TW_W];

        reg                       p2_valid, p2_sel;
        reg signed [2*DATA_W-1:0] p2_rr, p2_ii, p2_ri, p2_ir;
        reg signed [DATA_W-1:0]   p2_bi, p2_bq;

        always @(posedge clk) begin
            if (rst) begin
                p2_valid <= 1'b0;
                p2_sel   <= 1'b0;
                p2_rr    <= {(2*DATA_W){1'b0}};
                p2_ii    <= {(2*DATA_W){1'b0}};
                p2_ri    <= {(2*DATA_W){1'b0}};
                p2_ir    <= {(2*DATA_W){1'b0}};
                p2_bi    <= {DATA_W{1'b0}};
                p2_bq    <= {DATA_W{1'b0}};
            end else begin
                p2_valid <= p1_valid;
                if (p1_valid) begin
                    p2_sel <= p1_sel;
                    p2_rr  <= p1_i * p1_tc;
                    p2_ii  <= p1_q * p1_ts;
                    p2_ri  <= p1_i * p1_ts;
                    p2_ir  <= p1_q * p1_tc;
                    p2_bi  <= p1_i;
                    p2_bq  <= p1_q;
                end
            end
        end

        wire signed [MW-1:0] mr = {{2{p2_rr[2*DATA_W-1]}}, p2_rr}
                                - {{2{p2_ii[2*DATA_W-1]}}, p2_ii};
        wire signed [MW-1:0] mi = {{2{p2_ri[2*DATA_W-1]}}, p2_ri}
                                + {{2{p2_ir[2*DATA_W-1]}}, p2_ir};
        wire signed [MW-1:0] mr_s = (mr + MHALF) >>> (TW_W-1);
        wire signed [MW-1:0] mi_s = (mi + MHALF) >>> (TW_W-1);

        always @(posedge clk) begin
            if (rst) begin
                m_valid <= 1'b0;
                m_i     <= {DATA_W{1'b0}};
                m_q     <= {DATA_W{1'b0}};
            end else begin
                m_valid <= p2_valid;
                if (p2_valid) begin
                    m_i <= p2_sel ? p2_bi : sat_m(mr_s);
                    m_q <= p2_sel ? p2_bq : sat_m(mi_s);
                end
            end
        end

        assign mul_ovf = p2_valid && !p2_sel && (ovf_m(mr_s) || ovf_m(mi_s));

    end else if (DEPTH == 2) begin : g_trivial
        //--------------------------------------------------------------------
        // Twiddles are W_4^0 = +1 and W_4^1 = -j.  No multiplier: -j turns
        // (i,q) into (q,-i).  The negate saturates, which is the only way this
        // stage can overflow.
        //--------------------------------------------------------------------
        reg p1_tw;
        always @(posedge clk) begin
            if (s_valid) p1_tw <= cnt[CW-2];
        end

        wire                     rot = !p1_sel && p1_tw;
        wire signed [DATA_W-1:0] neg_i = (p1_i == SAT_N) ? SAT_P : -p1_i;

        reg                     p2_valid;
        reg signed [DATA_W-1:0] p2_i, p2_q;
        always @(posedge clk) begin
            if (rst) begin
                p2_valid <= 1'b0;
                p2_i     <= {DATA_W{1'b0}};
                p2_q     <= {DATA_W{1'b0}};
            end else begin
                p2_valid <= p1_valid;
                if (p1_valid) begin
                    p2_i <= rot ? p1_q  : p1_i;
                    p2_q <= rot ? neg_i : p1_q;
                end
            end
        end

        always @(posedge clk) begin
            if (rst) begin
                m_valid <= 1'b0;
                m_i     <= {DATA_W{1'b0}};
                m_q     <= {DATA_W{1'b0}};
            end else begin
                m_valid <= p2_valid;
                if (p2_valid) begin
                    m_i <= p2_i;
                    m_q <= p2_q;
                end
            end
        end

        assign mul_ovf = p1_valid && rot && (p1_i == SAT_N);

    end else begin : g_none
        //--------------------------------------------------------------------
        // Final stage, depth 1.  The only twiddle is W_2^0 = +1, so this is
        // two delay registers that keep the stage latency uniform.
        //--------------------------------------------------------------------
        reg                     p2_valid;
        reg signed [DATA_W-1:0] p2_i, p2_q;
        always @(posedge clk) begin
            if (rst) begin
                p2_valid <= 1'b0;
                p2_i     <= {DATA_W{1'b0}};
                p2_q     <= {DATA_W{1'b0}};
            end else begin
                p2_valid <= p1_valid;
                if (p1_valid) begin
                    p2_i <= p1_i;
                    p2_q <= p1_q;
                end
            end
        end

        always @(posedge clk) begin
            if (rst) begin
                m_valid <= 1'b0;
                m_i     <= {DATA_W{1'b0}};
                m_q     <= {DATA_W{1'b0}};
            end else begin
                m_valid <= p2_valid;
                if (p2_valid) begin
                    m_i <= p2_i;
                    m_q <= p2_q;
                end
            end
        end

        assign mul_ovf = 1'b0;
    end
    endgenerate

    assign ovf = (s_valid && bf_ovf) || mul_ovf;

endmodule


//============================================================================
// radar_fft_delay -- delay of exactly DEPTH enabled clocks
//
// Depth 16 and above becomes a block RAM: the address counter is shared, the
// read address runs one ahead of the write address, so the value read back is
// the one written DEPTH-1 clocks ago and the output register makes up the last
// clock.  Read and write addresses never collide, which is what lets Vivado
// map it to a simple dual-port RAMB.  The array itself is never reset -- that
// is deliberate, a reset on the array would force it into fabric registers.
//
// Below 16 it is a plain shift register, which Vivado packs into SRL16/SRL32.
//============================================================================
module radar_fft_delay #(
    parameter integer W     = 32,
    parameter integer DEPTH = 512
) (
    input  wire         clk,
    input  wire         en,
    input  wire [W-1:0] din,
    output wire [W-1:0] dout
);

    generate
    if (DEPTH >= 16) begin : g_bram
        localparam integer AW = $clog2(DEPTH);
        localparam [AW-1:0] A_ONE = 1;

        (* ram_style = "block" *)
        reg [W-1:0]  mem [0:DEPTH-1];
        reg [AW-1:0] wa;
        reg [W-1:0]  dor;

        wire [AW-1:0] ra = wa + A_ONE;   // DEPTH is a power of two, so it wraps

        always @(posedge clk) begin
            if (en) begin
                dor     <= mem[ra];
                mem[wa] <= din;
                wa      <= wa + A_ONE;
            end
        end

        assign dout = dor;

    end else begin : g_srl
        reg [W-1:0] sr [0:DEPTH-1];
        integer j;
        always @(posedge clk) begin
            if (en) begin
                sr[0] <= din;
                for (j = 1; j < DEPTH; j = j + 1) sr[j] <= sr[j-1];
            end
        end
        assign dout = sr[DEPTH-1];
    end
    endgenerate

endmodule

`default_nettype wire
