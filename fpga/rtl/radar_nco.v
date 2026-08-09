//============================================================================
// radar_nco.v -- linear-FM chirp generator: phase accumulator + quarter-wave
//                sine ROM with quadrant folding.
//
// WHAT IT DOES
//   Two cascaded 32-bit wrapping accumulators produce a linear frequency ramp:
//
//       freq_cur[n+1] = freq_cur[n] + freq_slope
//       phase[n+1]    = phase[n]    + freq_cur[n]        <-- old freq_cur
//
//   so phase[n] = n*freq_start + n(n-1)/2*freq_slope, a linear-FM chirp.
//   Both registers update from their PRE-UPDATE values on the same edge; that
//   is the plain non-blocking hardware reading and the C++ reference model
//   fpga/sim/tb_front.cpp copies it exactly.
//
//   `restart` (one cycle, priority over `ena`) sets phase to 0 and freq_cur to
//   freq_start, so the sweep origin is bit-exact and repeatable.  This is the
//   whole point of generating the chirp in fabric: the de-chirp reference and
//   the transmitted waveform leave the same accumulator, so the range origin
//   cannot drift.
//
// SINE TABLE
//   The top 12 bits of phase index a conceptual 4096-point table
//
//       S[m] = round(32767 * sin(2*pi*(m + 0.5) / 4096))
//
//   Note the +0.5 HALF-SAMPLE OFFSET.  It buys three things: the table never
//   contains an exact zero or an exact +/-32768, so negation can never
//   overflow s16; the quarter-wave symmetry is exact with no shared endpoint;
//   and the DC content of a full cycle is exactly zero.
//
//   Only the first quarter wave (1024 entries) is stored.  With m[11:10] the
//   quadrant and m[9:0] the offset:
//
//       quadrant 0 :  + rom[ offset]        quadrant 1 :  + rom[~offset]
//       quadrant 2 :  - rom[ offset]        quadrant 3 :  - rom[~offset]
//
//   out_q = sin(phase), out_i = cos(phase) = sin(phase + 2^30), i.e. the same
//   ROM read one quarter turn further round.  The table is built by an
//   `initial` block from $sin/$rtoi, so there is no external .mem file; the
//   identical values fall out of C++ as
//       (int)(32767.0 * sin((2.0*PI) * ((k + 0.5) / 4096.0)) + 0.5)
//   (every entry is positive, so truncate-after-adding-half IS round-to-
//   nearest, and both languages evaluate the same double operations in the
//   same order).
//
// LATENCY
//   3 clocks.  A cycle in which `ena` is high produces out_i/out_q/out_valid
//   3 clocks later, carrying the phase that stood in the accumulator on that
//   cycle.  The first sample of a sweep (phase = 0) therefore appears 4 clocks
//   after the cycle on which `restart` was high.  `restart` does not flush the
//   pipeline, so the last 3 samples of a sweep emerge during the first 3
//   cycles of the following retrace, where adc_gate is already low.
//
// RESOURCES on XC7K325T (defaults, one instance)
//   1 x RAMB18 in true-dual-port mode (1024 x 16, two read ports)
//   ~70 slice LUT (two 32-bit adders, fold logic, negation)
//   ~110 FF
//   0 DSP48
//   Comfortably above 200 MHz; radio_clk is 61.44 MHz.
//============================================================================
`timescale 1ns / 1ps
`default_nettype none

module radar_nco #(
    parameter integer PHASE_W    = 32,   // accumulator width, wraps naturally
    parameter integer LUT_ADDR_W = 10,   // quarter wave depth = 2^LUT_ADDR_W
    parameter integer OUT_W      = 16    // s16 Q0.15 output
) (
    input  wire                         clk,
    input  wire                         rst,        // synchronous, active high
    input  wire                         ena,
    input  wire                         restart,    // 1-cycle pulse
    input  wire signed [PHASE_W-1:0]    freq_start,
    input  wire signed [PHASE_W-1:0]    freq_slope,
    output reg  signed [OUT_W-1:0]      out_i,      // cos(phase)
    output reg  signed [OUT_W-1:0]      out_q,      // sin(phase)
    output reg                          out_valid
);

    localparam integer IDX_W   = LUT_ADDR_W + 2;      // 12 bits of phase used
    localparam integer LUT_N   = (1 << LUT_ADDR_W);   // 1024 quarter-wave pts
    localparam integer QUARTER = LUT_N;               // cos = sin + quarter

    //------------------------------------------------------------------------
    // Quarter-wave sine ROM, built at elaboration time.
    //------------------------------------------------------------------------
    reg signed [OUT_W-1:0] sin_rom [0:LUT_N-1];

    integer k, rv;
    real    pi_r, amp_r, nph_r, ang_r;

    initial begin
        pi_r  = 3.14159265358979323846;
        amp_r = (2.0 ** (OUT_W - 1)) - 1.0;      // 32767.0
        nph_r = 2.0 ** IDX_W;                    // 4096.0
        for (k = 0; k < LUT_N; k = k + 1) begin
            ang_r      = (2.0 * pi_r) * ((k + 0.5) / nph_r);
            rv         = $rtoi(amp_r * $sin(ang_r) + 0.5);
            // the quarter wave is strictly inside (0, +2^(OUT_W-1)-1], so the
            // clamp never fires; it is here so that negating a table entry in
            // quadrants 2 and 3 provably cannot overflow s16
            if (rv > AMP_MAX) rv = AMP_MAX;
            sin_rom[k] = rv[OUT_W-1:0];
        end
    end

    //------------------------------------------------------------------------
    // Stage 0 : the two accumulators.
    //------------------------------------------------------------------------
    reg signed [PHASE_W-1:0] phase;
    reg signed [PHASE_W-1:0] freq_cur;

    always @(posedge clk) begin
        if (rst) begin
            phase    <= {PHASE_W{1'b0}};
            freq_cur <= {PHASE_W{1'b0}};
        end else if (restart) begin
            phase    <= {PHASE_W{1'b0}};
            freq_cur <= freq_start;
        end else if (ena) begin
            phase    <= phase + freq_cur;       // pre-update freq_cur
            freq_cur <= freq_cur + freq_slope;
        end
    end

    //------------------------------------------------------------------------
    // Stage 1 : quadrant fold for both the sine and the cosine read.
    //------------------------------------------------------------------------
    wire [IDX_W-1:0] idx_q = phase[PHASE_W-1 -: IDX_W];
    wire [IDX_W-1:0] idx_i = idx_q + QUARTER[IDX_W-1:0];

    wire [LUT_ADDR_W-1:0] fold_q = idx_q[LUT_ADDR_W]
                                 ? ~idx_q[LUT_ADDR_W-1:0] : idx_q[LUT_ADDR_W-1:0];
    wire [LUT_ADDR_W-1:0] fold_i = idx_i[LUT_ADDR_W]
                                 ? ~idx_i[LUT_ADDR_W-1:0] : idx_i[LUT_ADDR_W-1:0];

    reg [LUT_ADDR_W-1:0] addr_i_s1, addr_q_s1;
    reg                  neg_i_s1,  neg_q_s1;
    reg                  vld_s1;

    always @(posedge clk) begin
        if (rst) begin
            addr_i_s1 <= {LUT_ADDR_W{1'b0}};
            addr_q_s1 <= {LUT_ADDR_W{1'b0}};
            neg_i_s1  <= 1'b0;
            neg_q_s1  <= 1'b0;
            vld_s1    <= 1'b0;
        end else begin
            addr_i_s1 <= fold_i;
            addr_q_s1 <= fold_q;
            neg_i_s1  <= idx_i[LUT_ADDR_W+1];
            neg_q_s1  <= idx_q[LUT_ADDR_W+1];
            vld_s1    <= ena;
        end
    end

    //------------------------------------------------------------------------
    // Stage 2 : ROM read (both ports).
    //------------------------------------------------------------------------
    reg signed [OUT_W-1:0] rom_i_s2, rom_q_s2;
    reg                    neg_i_s2, neg_q_s2;
    reg                    vld_s2;

    always @(posedge clk) begin
        rom_i_s2 <= sin_rom[addr_i_s1];
        rom_q_s2 <= sin_rom[addr_q_s1];
        neg_i_s2 <= neg_i_s1;
        neg_q_s2 <= neg_q_s1;
        if (rst) vld_s2 <= 1'b0;
        else     vld_s2 <= vld_s1;
    end

    //------------------------------------------------------------------------
    // Stage 3 : apply the quadrant sign.  Table entries lie in [25, 32767],
    // so the negation can never overflow s16.
    //------------------------------------------------------------------------
    always @(posedge clk) begin
        if (rst) begin
            out_i     <= {OUT_W{1'b0}};
            out_q     <= {OUT_W{1'b0}};
            out_valid <= 1'b0;
        end else begin
            out_i     <= neg_i_s2 ? -rom_i_s2 : rom_i_s2;
            out_q     <= neg_q_s2 ? -rom_q_s2 : rom_q_s2;
            out_valid <= vld_s2;
        end
    end

endmodule

`default_nettype wire
