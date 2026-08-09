//============================================================================
// radar_halfband.v -- one complex decimate-by-2 halfband FIR.
//
// WHY HALFBAND
//   In a halfband filter every even-offset tap from the centre is exactly
//   zero and the centre tap is exactly 0.5.  For NTAPS = 23 that means only 13
//   of the 23 taps are non-zero, and because the response is symmetric those
//   13 fold down to (NTAPS+1)/4 + 1 = 7 multiplies.  Decimating by two with a
//   general FIR of the same sharpness would cost 12.  Halfband also keeps the
//   passband flat, where a CIC would droop and tilt the finished range
//   profile.
//
// COEFFICIENTS
//   Built at elaboration time by an `initial` block -- no .mem file.  Ideal
//   halfband impulse response (closed form, no $sin needed, because
//   sin(pi*m/2) is just +/-1 for odd m):
//
//       h[centre]     = 0.5
//       h[centre +/- m] = (-1)^((m-1)/2) / (pi*m)     m odd
//       h[centre +/- m] = 0                           m even, m != 0
//
//   windowed by a Kaiser window of shape BETA (its zeroth-order modified
//   Bessel function is evaluated by its own power series, 40 terms, which is
//   converged to double precision for any BETA that is useful here), then
//   quantised to s18 Q0.17.
//
//   DC GAIN IS FORCED TO EXACTLY 1.0: the centre tap is pinned at 2^16 (0.5)
//   and the two or three LSB of quantisation residual are put on the innermost
//   side tap, symmetrically, so the coefficients sum to exactly 2^17.  A
//   filter whose DC gain is 1.0 only to within a few parts per million would
//   drift the de-chirped transmit-leakage level between the two decimator
//   stages, and that shows up as a moving floor in the range profile.
//
// ARITHMETIC
//   s16 data x s18 coefficient, accumulated in 40 bits, then round-half-up by
//   17 (Q0.15 x Q0.17 -> Q0.32, so 17 bits come back off) and saturate to s16.
//   Identical rounding to radar::fx::round_sat(v, 17, 16).
//
// TIMING / LATENCY
//   One complex sample in per `in_valid`; one complex sample out every second
//   `in_valid`.  4 clocks from the input sample that COMPLETES an output pair
//   to that output appearing with out_valid high.
//     +1  delay line update
//     +1  pre-add and multiply   (DSP48E1 pre-adder + P register)
//     +1  accumulate
//     +1  round and saturate
//   `rst` is synchronous and clears the delay line, the decimation phase and
//   the valid chain, which is exactly what radar_decim4 needs between chirps,
//   so no separate flush port is required here.
//
// NTAPS MUST BE 3 MODULO 4
//   Then the centre index is odd, every even index is a non-zero tap, and the
//   fold is the clean (NTAPS+1)/4 pairs plus the centre.  23, 27, 31, 35, 39.
//
// RESOURCES on XC7K325T (NTAPS = 23, complex)
//   14 x DSP48E1  (7 multiplies x 2 arms)
//   ~420 slice LUT, ~1500 FF  (2 x 23 x 16 delay line dominates)
//   0 BRAM
//   NTAPS = 35 costs 20 DSP48E1 and ~2200 FF.
//============================================================================
`timescale 1ns / 1ps
`default_nettype none

module radar_halfband #(
    parameter integer NTAPS  = 23,    // odd, and 3 modulo 4
    parameter integer COEF_W = 18,    // s18 Q0.17
    parameter integer DATA_W = 16,    // s16 Q0.15
    parameter integer ACC_W  = 40,
    parameter real    BETA   = 8.5    // Kaiser shape
) (
    input  wire                     clk,
    input  wire                     rst,        // synchronous, active high
    input  wire                     in_valid,
    input  wire signed [DATA_W-1:0] in_i,
    input  wire signed [DATA_W-1:0] in_q,
    output reg                      out_valid,
    output reg  signed [DATA_W-1:0] out_i,
    output reg  signed [DATA_W-1:0] out_q
);

    localparam integer CENTRE = (NTAPS - 1) / 2;      // 11 for NTAPS = 23
    localparam integer NPAIR  = (NTAPS + 1) / 4;      // 6  symmetric pairs
    localparam integer NMULT  = NPAIR + 1;            // 7  multiplies per arm
    localparam integer PROD_W = DATA_W + 1 + COEF_W;  // 35
    localparam integer OUT_SH = COEF_W - 1;           // 17

    localparam integer SCALE_I     = (1 << (COEF_W - 1));  // 131072 = 1.0
    localparam integer CENTRE_Q_I  = (1 << (COEF_W - 2));  //  65536 = 0.5
    localparam integer HALF_SIDE_I = (1 << (COEF_W - 3));  //  32768 = one side

    localparam signed [ACC_W-1:0] SAT_HI =
        {{(ACC_W-DATA_W+1){1'b0}}, {(DATA_W-1){1'b1}}};
    localparam signed [ACC_W-1:0] SAT_LO =
        {{(ACC_W-DATA_W+1){1'b1}}, {(DATA_W-1){1'b0}}};
    localparam signed [ACC_W-1:0] RND_HALF =
        {{(ACC_W-OUT_SH){1'b0}}, 1'b1, {(OUT_SH-1){1'b0}}};

    //------------------------------------------------------------------------
    // Round-half-up by OUT_SH then clamp to s16 -- radar::fx::round_sat().
    //------------------------------------------------------------------------
    function signed [DATA_W-1:0] round_sat;
        input signed [ACC_W-1:0] v;
        reg   signed [ACC_W-1:0] t;
        begin
            t = (v + RND_HALF) >>> OUT_SH;
            if      (t > SAT_HI) round_sat = SAT_HI[DATA_W-1:0];
            else if (t < SAT_LO) round_sat = SAT_LO[DATA_W-1:0];
            else                 round_sat = t[DATA_W-1:0];
        end
    endfunction

    //------------------------------------------------------------------------
    // Kaiser-windowed halfband coefficients.
    //   coef[0 .. NPAIR-1] : the tap at delay-line index 2j and its mirror
    //   coef[NPAIR]        : the centre tap, pinned at exactly 0.5
    //------------------------------------------------------------------------
    reg signed [COEF_W-1:0] coef [0:NPAIR];

    function real bessel_i0;                  // I0(x) = sum ((x/2)^k / k!)^2
        input real x;
        real    s;
        real    t;
        integer n;
        begin
            s = 1.0;
            t = 1.0;
            for (n = 1; n < 40; n = n + 1) begin
                t = t * (x / (2.0 * n));
                s = s + t * t;
            end
            bessel_i0 = s;
        end
    endfunction

    integer ci, cm, cpar, csum, qv, q_inner;
    real    pi_r, i0b, cr, xr, warg, hd_r, wk_r, scale_r;

    initial begin
        pi_r    = 3.14159265358979323846;
        scale_r = SCALE_I;
        i0b     = bessel_i0(BETA);
        cr      = CENTRE;
        csum    = 0;
        q_inner = 0;
        for (ci = 0; ci < NPAIR; ci = ci + 1) begin
            cm   = CENTRE - 2 * ci;                 // odd, positive
            cpar = ((cm - 1) / 2) % 2;
            hd_r = (cpar == 0) ? (1.0 / (pi_r * cm)) : (-1.0 / (pi_r * cm));
            xr   = (2 * ci) - CENTRE;
            xr   = xr / cr;
            warg = 1.0 - xr * xr;
            if (warg < 0.0) warg = 0.0;
            wk_r = bessel_i0(BETA * $sqrt(warg)) / i0b;
            hd_r = hd_r * wk_r;
            qv   = (hd_r >= 0.0) ?  $rtoi( hd_r * scale_r + 0.5)
                                 : -$rtoi(-hd_r * scale_r + 0.5);
            coef[ci] = qv[COEF_W-1:0];
            csum     = csum + qv;
            if (ci == (NPAIR - 1)) q_inner = qv;
        end
        // exact DC gain: the innermost side tap absorbs the quantisation
        // residual, symmetrically, and the centre tap stays exactly 0.5
        qv            = q_inner + (HALF_SIDE_I - csum);
        coef[NPAIR-1] = qv[COEF_W-1:0];
        qv            = CENTRE_Q_I;
        coef[NPAIR]   = qv[COEF_W-1:0];
    end

    //------------------------------------------------------------------------
    // Delay line and decimation phase.
    //------------------------------------------------------------------------
    reg signed [DATA_W-1:0] dl_i [0:NTAPS-1];
    reg signed [DATA_W-1:0] dl_q [0:NTAPS-1];
    reg                     dphase;      // toggles on every accepted sample
    reg                     due_s1;      // the delay line now holds a full set

    integer n;

    always @(posedge clk) begin
        if (rst) begin
            for (n = 0; n < NTAPS; n = n + 1) begin
                dl_i[n] <= {DATA_W{1'b0}};
                dl_q[n] <= {DATA_W{1'b0}};
            end
            dphase <= 1'b0;
            due_s1 <= 1'b0;
        end else begin
            due_s1 <= in_valid & dphase;
            if (in_valid) begin
                dl_i[0] <= in_i;
                dl_q[0] <= in_q;
                for (n = 1; n < NTAPS; n = n + 1) begin
                    dl_i[n] <= dl_i[n-1];
                    dl_q[n] <= dl_q[n-1];
                end
                dphase <= ~dphase;
            end
        end
    end

    //------------------------------------------------------------------------
    // Pre-add and multiply.  Vivado maps the pre-add into the DSP48E1.
    //------------------------------------------------------------------------
    reg signed [PROD_W-1:0] pi_s2 [0:NMULT-1];
    reg signed [PROD_W-1:0] pq_s2 [0:NMULT-1];
    reg                     v_s2;

    integer j;

    always @(posedge clk) begin
        for (j = 0; j < NPAIR; j = j + 1) begin
            pi_s2[j] <= ($signed({dl_i[2*j][DATA_W-1],           dl_i[2*j]}) +
                         $signed({dl_i[NTAPS-1-2*j][DATA_W-1],   dl_i[NTAPS-1-2*j]}))
                        * coef[j];
            pq_s2[j] <= ($signed({dl_q[2*j][DATA_W-1],           dl_q[2*j]}) +
                         $signed({dl_q[NTAPS-1-2*j][DATA_W-1],   dl_q[NTAPS-1-2*j]}))
                        * coef[j];
        end
        pi_s2[NPAIR] <= dl_i[CENTRE] * coef[NPAIR];
        pq_s2[NPAIR] <= dl_q[CENTRE] * coef[NPAIR];
        if (rst) v_s2 <= 1'b0;
        else     v_s2 <= due_s1;
    end

    //------------------------------------------------------------------------
    // Accumulate.  Written as a chain; the synthesiser balances it into a
    // two-level tree, which is trivial at 61.44 MHz.
    //------------------------------------------------------------------------
    wire signed [ACC_W-1:0] chain_i [0:NMULT-1];
    wire signed [ACC_W-1:0] chain_q [0:NMULT-1];

    genvar g;
    generate
        for (g = 0; g < NMULT; g = g + 1) begin : g_acc
            wire signed [ACC_W-1:0] ext_i =
                {{(ACC_W-PROD_W){pi_s2[g][PROD_W-1]}}, pi_s2[g]};
            wire signed [ACC_W-1:0] ext_q =
                {{(ACC_W-PROD_W){pq_s2[g][PROD_W-1]}}, pq_s2[g]};
            if (g == 0) begin : g_head
                assign chain_i[g] = ext_i;
                assign chain_q[g] = ext_q;
            end else begin : g_tail
                assign chain_i[g] = chain_i[g-1] + ext_i;
                assign chain_q[g] = chain_q[g-1] + ext_q;
            end
        end
    endgenerate

    reg signed [ACC_W-1:0] acc_i_s3, acc_q_s3;
    reg                    v_s3;

    always @(posedge clk) begin
        acc_i_s3 <= chain_i[NMULT-1];
        acc_q_s3 <= chain_q[NMULT-1];
        if (rst) v_s3 <= 1'b0;
        else     v_s3 <= v_s2;
    end

    //------------------------------------------------------------------------
    // Round and saturate.
    //------------------------------------------------------------------------
    always @(posedge clk) begin
        out_i <= round_sat(acc_i_s3);
        out_q <= round_sat(acc_q_s3);
        if (rst) out_valid <= 1'b0;
        else     out_valid <= v_s3;
    end

endmodule

`default_nettype wire
