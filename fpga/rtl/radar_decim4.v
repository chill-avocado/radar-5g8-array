//============================================================================
// radar_decim4.v -- two halfband stages in series, 61.44 -> 15.36 MSps.
//
//   61.44 MSps --[HB1, 23 taps]--> 30.72 MSps --[HB2, 35 taps]--> 15.36 MSps
//
// WHY TWO STAGES AND WHY THESE LENGTHS
//   Every decimate-by-2 stage only has to guard the band the NEXT stage will
//   keep, so the first stage can be short and the sharpness is bought once, in
//   the second stage, at a quarter of the sample rate.  Doing the same job in
//   one 4:1 filter would need roughly four times the multipliers.
//
//   The wanted signal after de-chirp is the beat frequency, 6666.7 Hz per
//   metre, so 250 m of range is 1.67 MHz.  What matters is that nothing folds
//   ON TOP of +/- 1.9 MHz at the output.  Measured on the built RTL by
//   fpga/sim/run_front.sh:
//     DC gain                                    exactly 1.0, bit for bit
//     1 MHz passband loss                           0.0000 dB
//     16.36 MHz, which folds onto  1 MHz           -113.3 dB
//     22.36 MHz, which folds onto  7 MHz            -75.9 dB
//     worst fold into the +/- 1.9 MHz wanted band   -89.2 dB
//   Frequencies just above the 7.68 MHz output Nyquist sit in the halfband
//   transition and are only 6 dB down -- that is inherent to halfband
//   decimation, and harmless, because they land at the far edge of the output
//   band where no target of interest can be.  A 7 MHz INPUT tone is below that
//   Nyquist, so it does not fold at all and passes at -2.3 dB.
//
// FLUSH
//   Chirps are independent coherent records: the tail of one sweep must not
//   leak into the head of the next, or a strong close target smears in Doppler
//   across the whole coherent interval.  `flush` clears both delay lines, both
//   decimation phases and both valid chains in one clock.  It reaches the
//   halfbands through their synchronous reset, which does precisely that, so
//   radar_halfband keeps its plain port list.
//
//   The sequencer should hold `flush` during the retrace, when adc_gate is
//   low; releasing it at the start of a sweep gives a clean record whose first
//   (NTAPS1 + 2*NTAPS2)/4 output samples are still filling the pipe.  Those
//   land inside the zero-padded tail of the window, so nothing is lost.
//
// LATENCY
//   8 clocks from the input sample that completes an output to that output
//   appearing: 4 through HB1 and 4 more through HB2, both in the same clock
//   domain.  One output per four accepted inputs, exactly.
//
// RESOURCES on XC7K325T (defaults, one complex channel)
//   34 x DSP48E1   (14 for HB1, 20 for HB2)
//   ~1200 slice LUT, ~4000 FF
//   0 BRAM
//============================================================================
`timescale 1ns / 1ps
`default_nettype none

module radar_decim4 #(
    parameter integer NTAPS1 = 23,     // 61.44 -> 30.72 MSps
    parameter integer NTAPS2 = 35,     // 30.72 -> 15.36 MSps
    parameter real    BETA1  = 8.5,
    parameter real    BETA2  = 10.0,
    parameter integer COEF_W = 18,
    parameter integer DATA_W = 16,
    parameter integer ACC_W  = 40
) (
    input  wire                     clk,
    input  wire                     rst,        // synchronous, active high
    input  wire                     flush,      // clear both delay lines
    input  wire                     in_valid,
    input  wire signed [DATA_W-1:0] in_i,
    input  wire signed [DATA_W-1:0] in_q,
    output wire                     out_valid,
    output wire signed [DATA_W-1:0] out_i,
    output wire signed [DATA_W-1:0] out_q
);

    wire clr = rst | flush;

    wire                     mid_valid;
    wire signed [DATA_W-1:0] mid_i;
    wire signed [DATA_W-1:0] mid_q;

    radar_halfband #(
        .NTAPS  (NTAPS1),
        .COEF_W (COEF_W),
        .DATA_W (DATA_W),
        .ACC_W  (ACC_W),
        .BETA   (BETA1)
    ) u_hb1 (
        .clk       (clk),
        .rst       (clr),
        .in_valid  (in_valid),
        .in_i      (in_i),
        .in_q      (in_q),
        .out_valid (mid_valid),
        .out_i     (mid_i),
        .out_q     (mid_q)
    );

    radar_halfband #(
        .NTAPS  (NTAPS2),
        .COEF_W (COEF_W),
        .DATA_W (DATA_W),
        .ACC_W  (ACC_W),
        .BETA   (BETA2)
    ) u_hb2 (
        .clk       (clk),
        .rst       (clr),
        .in_valid  (mid_valid),
        .in_i      (mid_i),
        .in_q      (mid_q),
        .out_valid (out_valid),
        .out_i     (out_i),
        .out_q     (out_q)
    );

endmodule

`default_nettype wire
