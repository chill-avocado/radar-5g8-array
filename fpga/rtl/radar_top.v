//============================================================================
// radar_top.v -- the radar core, assembled
//
// Target: Xilinx Kintex-7 XC7K325T inside a USRP B210 clone.
// Clock : radio_clk, the AD9361 sample clock, 61.44 MHz.
//
// ---------------------------------------------------------------------------
// WHAT IT DOES
// ---------------------------------------------------------------------------
// Everything between the converters and the host link:
//
//   chirp generator ---------------------------------> TX0 / TX1 to the DAC
//        |  (same phase accumulator, one clock apart)
//        v
//   RX0 --> de-chirp --> decimate /4 --> window --> range FFT --> corner turn
//   RX1 --> de-chirp --> decimate /4 --> window --> range FFT --> corner turn
//                                                          |
//                             four Doppler FFTs, one per virtual channel
//                                                          |
//                          integrate power over the four --+--> 2D CFAR
//                                                          |        |
//                                              matched delay        |
//                                                          |        v
//                                                          +---> packer --> host
//
// The transmitted waveform and the de-chirp reference come out of ONE phase
// accumulator, so there is no unknown delay between what was sent and what we
// correlate against.  That is the difference between this and host-generated
// FMCW on an SDR, where USB jitter leaves the range origin floating and every
// range measurement drifts.  The only residual is the fixed analogue group
// delay through the AD9361, which appears as the transmit-leakage peak and is
// removed once, at calibration.
//
// It also means no transmit samples cross USB, and what does cross is a
// range-Doppler map and a detection list rather than raw IQ: about 4.4 MB/s
// where raw streaming would need 491 MB/s.
//
// ---------------------------------------------------------------------------
// WHAT DELIBERATELY IS NOT HERE
// ---------------------------------------------------------------------------
// The TDM Doppler phase correction.  Alternating the transmitter every chirp
// means TX1's chirps happen half a TDM interval after TX0's, which puts a
// phase exp(-j*pi*m_d/P) on TX1's virtual channels at Doppler bin m_d.  It
// would cost a complex multiplier and a twiddle ROM here.  It costs nothing on
// the host, because the correction does not change |x| and therefore does not
// change the power map or the CFAR decision -- it only matters for the angle,
// which is computed on the host from the handful of complex samples attached
// to each detection.  So it is applied there.  See soft/src/aoa.cpp.
//
// Angle, clustering and tracking are host work for the same reason: they are
// matrix arithmetic on a few hundred detections, not a 61.44 MSps stream.
//
// ---------------------------------------------------------------------------
// RESOURCES, estimated, XC7K325T (203800 LUT, 840 DSP48E1, 445 BRAM36)
// ---------------------------------------------------------------------------
//   corner turn, 2 channels, ping-pong        228 BRAM36   51%
//   range FFT   1024-pt R2SDF, 2 instances     ~8 BRAM36, ~54 DSP
//   Doppler FFT  128-pt R2SDF, 4 instances     ~2 BRAM36, ~48 DSP
//   de-chirp    2 instances                              8 DSP
//   decimator   2 instances                             ~24 DSP
//   CFAR line buffers                          ~13 BRAM36
//   hit capture delay + hit buffer             ~14 BRAM36
//   NCO, sequencer, registers, packer          ~2 BRAM36, ~2 DSP
//   ------------------------------------------------------------------
//   total                                     ~267 BRAM36 (60%), ~136 DSP (16%)
// which leaves the stock b200 radio core its share with room to spare.
//============================================================================
`timescale 1ns / 1ps
`include "radar_pkg.svh"
`default_nettype none

module radar_top #(
    parameter integer N_RANGE_FFT     = `RADAR_N_RANGE_FFT,
    parameter integer N_RANGE_FFT_LOG2= `RADAR_N_RANGE_FFT_LOG2,
    parameter integer N_DOPPLER       = `RADAR_N_DOPPLER,
    parameter integer N_DOPPLER_LOG2  = `RADAR_N_DOPPLER_LOG2,
    parameter integer CT_WORDS_LOG2   = `RADAR_CT_WORDS_LOG2,
    parameter integer SAMP_W          = `RADAR_SAMP_W
) (
    input  wire                 radio_clk,
    input  wire                 radio_rst,      // synchronous, active high

    // Settings bus, as the b200 design already provides it.
    input  wire                 set_stb,
    input  wire [7:0]           set_addr,
    input  wire [31:0]          set_data,

    // Receive samples, one complex sample per strobe per channel.
    input  wire signed [15:0]   rx0_i,
    input  wire signed [15:0]   rx0_q,
    input  wire signed [15:0]   rx1_i,
    input  wire signed [15:0]   rx1_q,
    input  wire                 rx_stb,

    // Transmit samples to the DAC.
    output wire signed [15:0]   tx0_i,
    output wire signed [15:0]   tx0_q,
    output wire signed [15:0]   tx1_i,
    output wire signed [15:0]   tx1_q,
    output wire                 tx_valid,

    // Output frame stream, 32-bit words, see radar_pkg.svh section 7.
    output wire [31:0]          out_data,
    output wire                 out_valid,
    input  wire                 out_ready,
    output wire                 out_last,

    // Status
    output wire                 enabled,        // low = core is transparent
    output wire                 overflow,
    output wire                 cfg_error,
    input  wire [63:0]          vita_time
);

    //------------------------------------------------------------------
    // Registers
    //------------------------------------------------------------------
    wire        ctrl_enable, ctrl_soft_reset, ctrl_tx_enable;
    wire        ctrl_map_enable, ctrl_hits_enable, ctrl_loopback;
    wire [1:0]  ctrl_mimo_mode;
    wire [15:0] ctrl_frame_limit;
    wire signed [31:0] freq_start, freq_slope;
    wire [15:0] t_sweep, t_pri, n_chirp, tx_gain;
    wire [3:0]  dechirp_sh;
    wire [19:0] fft_scale_r;
    wire [15:0] fft_scale_d;
    wire        win_we;
    wire [15:0] win_addr;
    wire signed [15:0] win_data_r, win_data_d;
    wire [3:0]  cfar_guard_r, cfar_guard_d, cfar_train_r, cfar_train_d;
    wire [1:0]  cfar_kind;
    wire [31:0] cfar_alpha;
    wire [15:0] range_zero;
    wire [7:0]  map_decim_r, map_decim_d;
    wire [15:0] max_hits;
    wire [7:0]  zero_dopp;
    wire [3:0]  geom_n_range_log2, geom_n_chirp_log2;
    wire signed [31:0] test_tone;
    wire        version_stb;

    radar_regs u_regs (
        .clk(radio_clk), .rst(radio_rst),
        .set_stb(set_stb), .set_addr(set_addr), .set_data(set_data),
        .ctrl_enable(ctrl_enable), .ctrl_soft_reset(ctrl_soft_reset),
        .ctrl_mimo_mode(ctrl_mimo_mode), .ctrl_tx_enable(ctrl_tx_enable),
        .ctrl_map_enable(ctrl_map_enable), .ctrl_hits_enable(ctrl_hits_enable),
        .ctrl_loopback(ctrl_loopback), .ctrl_frame_limit(ctrl_frame_limit),
        .freq_start(freq_start), .freq_slope(freq_slope),
        .t_sweep(t_sweep), .t_pri(t_pri), .n_chirp(n_chirp), .tx_gain(tx_gain),
        .dechirp_sh(dechirp_sh),
        .fft_scale_r(fft_scale_r), .fft_scale_d(fft_scale_d),
        .win_we(win_we), .win_addr(win_addr), .win_data(),
        .win_data_r(win_data_r), .win_data_d(win_data_d),
        .version_stb(version_stb),
        .cfar_guard_range(cfar_guard_r), .cfar_guard_dopp(cfar_guard_d),
        .cfar_train_range(cfar_train_r), .cfar_train_dopp(cfar_train_d),
        .cfar_kind(cfar_kind), .cfar_alpha(cfar_alpha),
        .range_zero(range_zero),
        .map_decim_r(map_decim_r), .map_decim_d(map_decim_d),
        .max_hits(max_hits), .zero_dopp(zero_dopp),
        .geom_n_range_log2(geom_n_range_log2),
        .geom_n_chirp_log2(geom_n_chirp_log2),
        .test_tone(test_tone)
    );

    // A soft reset from the host flushes every pipeline and both buffers, so
    // a configuration change can never produce a frame that was processed
    // half under the old settings and half under the new.
    wire rst = radio_rst | ctrl_soft_reset;
    assign enabled   = ctrl_enable;
    assign cfg_error = ct0_cfg_error | ct1_cfg_error | hitcap_cfg_error;

    //------------------------------------------------------------------
    // Sequencer and chirp generator
    //------------------------------------------------------------------
    wire        nco_restart, nco_ena;
    wire        tx0_ena, tx1_ena, tx_invert, adc_gate;
    wire [15:0] sample_idx;
    wire [15:0] chirp_idx;
    wire        tx_sel, frame_start, frame_end, running;

    radar_seq u_seq (
        .clk(radio_clk), .rst(rst),
        .enable(ctrl_enable), .mimo_mode(ctrl_mimo_mode), .tx_enable(ctrl_tx_enable),
        .t_sweep(t_sweep), .t_pri(t_pri), .n_chirp(n_chirp),
        .nco_restart(nco_restart), .nco_ena(nco_ena),
        .tx0_ena(tx0_ena), .tx1_ena(tx1_ena), .tx_invert(tx_invert),
        .adc_gate(adc_gate), .sample_idx(sample_idx), .chirp_idx(chirp_idx),
        .tx_sel(tx_sel), .frame_start(frame_start), .frame_end(frame_end),
        .running(running)
    );

    wire signed [15:0] nco_i, nco_q;
    wire               nco_valid;

    radar_nco u_nco (
        .clk(radio_clk), .rst(rst),
        .ena(nco_ena), .restart(nco_restart),
        .freq_start(freq_start), .freq_slope(freq_slope),
        .out_i(nco_i), .out_q(nco_q), .out_valid(nco_valid)
    );

    //------------------------------------------------------------------
    // Transmit path.  Gain, then the DDM sign flip, then the per-transmitter
    // enable.  Amplitude scaling is a plain rounded multiply; the sequencer
    // guarantees the enables change only between chirps, so no transmitter is
    // ever switched mid-sweep.
    //------------------------------------------------------------------
    reg signed [15:0] tx_gi, tx_gq;
    always @(posedge radio_clk) begin
        if (rst) begin
            tx_gi <= 16'sd0;
            tx_gq <= 16'sd0;
        end else begin
            tx_gi <= $signed({1'b0, tx_gain}) * nco_i >>> 15;
            tx_gq <= $signed({1'b0, tx_gain}) * nco_q >>> 15;
        end
    end

    reg tx0_en_d, tx1_en_d, tx_inv_d, tx_val_d;
    always @(posedge radio_clk) begin
        tx0_en_d <= tx0_ena;
        tx1_en_d <= tx1_ena;
        tx_inv_d <= tx_invert;
        tx_val_d <= nco_valid & ctrl_tx_enable;
    end

    assign tx0_i    = tx0_en_d ? tx_gi : 16'sd0;
    assign tx0_q    = tx0_en_d ? tx_gq : 16'sd0;
    assign tx1_i    = tx1_en_d ? (tx_inv_d ? -tx_gi : tx_gi) : 16'sd0;
    assign tx1_q    = tx1_en_d ? (tx_inv_d ? -tx_gq : tx_gq) : 16'sd0;
    assign tx_valid = tx_val_d;

    //------------------------------------------------------------------
    // Receive front ends.  Two identical chains, generated rather than
    // copied so they cannot drift apart.
    //------------------------------------------------------------------
    wire signed [15:0] rxsel_i [0:1];
    wire signed [15:0] rxsel_q [0:1];

    // Built-in test: drive the de-chirp from the chirp generator instead of
    // the converter, which puts a target at exactly zero range and proves the
    // whole chain end to end with no RF present.
    assign rxsel_i[0] = ctrl_loopback ? nco_i : rx0_i;
    assign rxsel_q[0] = ctrl_loopback ? nco_q : rx0_q;
    assign rxsel_i[1] = ctrl_loopback ? nco_i : rx1_i;
    assign rxsel_q[1] = ctrl_loopback ? nco_q : rx1_q;

    wire               fe_valid [0:1];
    wire signed [15:0] fe_i     [0:1];
    wire signed [15:0] fe_q     [0:1];
    wire               fe_last  [0:1];
    wire [N_RANGE_FFT_LOG2-1:0] fe_idx [0:1];
    wire               fe_ovf   [0:1];

    genvar ch;
    generate
    for (ch = 0; ch < 2; ch = ch + 1) begin : g_rx
        // De-chirp: conj(received) * reference, so a target's beat frequency
        // is positive and range increases with transform bin.
        wire               dc_valid;
        wire signed [15:0] dc_i, dc_q;
        radar_dechirp u_dechirp (
            .clk(radio_clk), .rst(rst),
            .in_valid(rx_stb & adc_gate),
            .in_i(rxsel_i[ch]), .in_q(rxsel_q[ch]),
            .ref_i(nco_i), .ref_q(nco_q),
            .shift(dechirp_sh),
            .out_valid(dc_valid), .out_i(dc_i), .out_q(dc_q)
        );

        // Decimate by four.  Everything of interest sits below the beat
        // frequency of the furthest range wanted, so three quarters of the
        // band is thrown away before the transform, which is what makes a
        // 1024-point transform enough.  Flushed between chirps so one chirp's
        // filter tail cannot contaminate the next chirp's head -- they are
        // independent coherent records.
        wire               de_valid;
        wire signed [15:0] de_i, de_q;
        radar_decim4 u_decim (
            .clk(radio_clk), .rst(rst),
            .flush(nco_restart),
            .in_valid(dc_valid), .in_i(dc_i), .in_q(dc_q),
            .out_valid(de_valid), .out_i(de_i), .out_q(de_q)
        );

        // Count position within the decimated record, for the window table.
        reg [15:0] win_idx;
        always @(posedge radio_clk) begin
            if (rst || nco_restart) win_idx <= 16'd0;
            else if (de_valid)      win_idx <= win_idx + 16'd1;
        end

        wire               wn_valid;
        wire signed [15:0] wn_i, wn_q;
        radar_window u_window (
            .clk(radio_clk), .rst(rst),
            .in_valid(de_valid), .in_i(de_i), .in_q(de_q),
            .sample_idx(win_idx),
            .coef_we(win_we), .coef_addr(win_addr), .coef_data(win_data_r),
            .out_valid(wn_valid), .out_i(wn_i), .out_q(wn_q)
        );

        // The window module emits zero past the table depth, which is how the
        // 768-sample record is zero-padded to the 1024-point transform.
        reg [N_RANGE_FFT_LOG2-1:0] fft_fill;
        wire fft_last = (fft_fill == N_RANGE_FFT - 1);
        always @(posedge radio_clk) begin
            if (rst)           fft_fill <= 0;
            else if (wn_valid) fft_fill <= fft_last ? 0 : fft_fill + 1'b1;
        end

        radar_fft #(
            .N(N_RANGE_FFT), .NLOG2(N_RANGE_FFT_LOG2), .NATURAL_OUT(1)
        ) u_range_fft (
            .clk(radio_clk), .rst(rst),
            .in_valid(wn_valid), .in_i(wn_i), .in_q(wn_q), .in_last(fft_last),
            .scale_sch(fft_scale_r),
            .out_valid(fe_valid[ch]), .out_i(fe_i[ch]), .out_q(fe_q[ch]),
            .out_last(fe_last[ch]), .out_idx(fe_idx[ch]), .overflow(fe_ovf[ch])
        );
    end
    endgenerate

    //------------------------------------------------------------------
    // Keep only the low range bins and corner turn.
    //
    // The transform produces 1024 bins; only the first n_range are within the
    // range of interest, and the rest are discarded here rather than being
    // carried through a buffer that would not fit.
    //------------------------------------------------------------------
    wire [15:0] n_range_max = 16'd1 << geom_n_range_log2;

    wire        ct_s_valid [0:1];
    wire [31:0] ct_s_data  [0:1];
    wire        ct_s_ready [0:1];
    wire        ct_s_last  [0:1];
    wire        ct_m_valid [0:1];
    wire [31:0] ct_m_data  [0:1];
    wire        ct_m_last  [0:1];
    wire        ct_frame_done [0:1];
    wire        ct_overflow   [0:1];
    wire        ct0_cfg_error, ct1_cfg_error;

    assign ct_s_valid[0] = fe_valid[0] && (fe_idx[0] < n_range_max[N_RANGE_FFT_LOG2-1:0]);
    assign ct_s_data [0] = {fe_i[0], fe_q[0]};
    assign ct_s_last [0] = fe_valid[0] && (fe_idx[0] == n_range_max[N_RANGE_FFT_LOG2-1:0] - 1);
    assign ct_s_valid[1] = fe_valid[1] && (fe_idx[1] < n_range_max[N_RANGE_FFT_LOG2-1:0]);
    assign ct_s_data [1] = {fe_i[1], fe_q[1]};
    assign ct_s_last [1] = fe_valid[1] && (fe_idx[1] == n_range_max[N_RANGE_FFT_LOG2-1:0] - 1);

    radar_corner_turn #(.CT_WORDS_LOG2(CT_WORDS_LOG2)) u_ct0 (
        .clk(radio_clk), .rst(rst),
        .n_range_log2(geom_n_range_log2), .n_chirp_log2(geom_n_chirp_log2),
        .cfg_error(ct0_cfg_error),
        .s_valid(ct_s_valid[0]), .s_data(ct_s_data[0]),
        .s_ready(ct_s_ready[0]), .s_last(ct_s_last[0]),
        .m_valid(ct_m_valid[0]), .m_data(ct_m_data[0]),
        .m_ready(1'b1), .m_last(ct_m_last[0]),
        .frame_done(ct_frame_done[0]), .overflow(ct_overflow[0])
    );

    radar_corner_turn #(.CT_WORDS_LOG2(CT_WORDS_LOG2)) u_ct1 (
        .clk(radio_clk), .rst(rst),
        .n_range_log2(geom_n_range_log2), .n_chirp_log2(geom_n_chirp_log2),
        .cfg_error(ct1_cfg_error),
        .s_valid(ct_s_valid[1]), .s_data(ct_s_data[1]),
        .s_ready(ct_s_ready[1]), .s_last(ct_s_last[1]),
        .m_valid(ct_m_valid[1]), .m_data(ct_m_data[1]),
        .m_ready(1'b1), .m_last(ct_m_last[1]),
        .frame_done(ct_frame_done[1]), .overflow(ct_overflow[1])
    );

    //------------------------------------------------------------------
    // Doppler transforms.  Four of them: one per virtual channel.
    //
    // The corner turn hands back, for each range bin, every chirp of the
    // interval in order.  In time-division mode the transmitters alternate
    // chirp by chirp, so the even samples belong to transmitter 0 and the odd
    // ones to transmitter 1.  Splitting them here and running two shorter
    // transforms is the whole of the MIMO de-multiplexing.
    //------------------------------------------------------------------
    reg [1:0] dopp_phase [0:1];
    always @(posedge radio_clk) begin
        if (rst) begin
            dopp_phase[0] <= 2'd0;
            dopp_phase[1] <= 2'd0;
        end else begin
            if (ct_m_valid[0]) dopp_phase[0] <= ct_m_last[0] ? 2'd0 : dopp_phase[0] + 2'd1;
            if (ct_m_valid[1]) dopp_phase[1] <= ct_m_last[1] ? 2'd0 : dopp_phase[1] + 2'd1;
        end
    end

    wire               dv_valid [0:3];
    wire signed [15:0] dv_i     [0:3];
    wire signed [15:0] dv_q     [0:3];
    wire               dv_last  [0:3];
    wire               dv_ovf   [0:3];

    genvar rxi, txi;
    generate
    for (rxi = 0; rxi < 2; rxi = rxi + 1) begin : g_dr
      for (txi = 0; txi < 2; txi = txi + 1) begin : g_dt
        // Virtual channel numbering is (transmitter * 2 + receiver), matching
        // radar::array_geom::virt_xy in soft/include/radar/core.hpp, which in
        // turn matches the built boards: the transmit pair sits side by side
        // so it sets the azimuth coordinate, and the receive pair is stacked
        // so it sets elevation.  Getting this order wrong scrambles every
        // angle the radar reports while leaving the range-Doppler map looking
        // perfectly healthy, so it is stated here rather than inferred.
        localparam integer VC = txi * 2 + rxi;
        // In time division the transmitter alternates every chirp; in Doppler
        // division both transmit every chirp and the separation happens in the
        // Doppler domain instead, so every sample belongs to both.
        wire take = (ctrl_mimo_mode == `RADAR_MIMO_TDM)
                    ? (dopp_phase[rxi][0] == txi[0]) : 1'b1;

        reg [N_DOPPLER_LOG2-1:0] fill;
        wire dlast = (fill == N_DOPPLER - 1);
        always @(posedge radio_clk) begin
            if (rst)                          fill <= 0;
            else if (ct_m_valid[rxi] && take) fill <= dlast ? 0 : fill + 1'b1;
        end

        radar_fft #(
            .N(N_DOPPLER), .NLOG2(N_DOPPLER_LOG2), .NATURAL_OUT(1)
        ) u_dopp (
            .clk(radio_clk), .rst(rst),
            .in_valid(ct_m_valid[rxi] & take),
            .in_i(ct_m_data[rxi][31:16]), .in_q(ct_m_data[rxi][15:0]),
            .in_last(dlast),
            .scale_sch({4'd0, fft_scale_d}),
            .out_valid(dv_valid[VC]), .out_i(dv_i[VC]), .out_q(dv_q[VC]),
            .out_last(dv_last[VC]), .out_idx(), .overflow(dv_ovf[VC])
        );
      end
    end
    endgenerate

    //------------------------------------------------------------------
    // Integrate power over the four virtual channels.
    //
    // Non-coherent, because the target's angle is not known yet -- coherent
    // combining would need a steering direction and would suppress everything
    // else.  The four transforms run in lock step, so their outputs for the
    // same cell are aligned and only need serialising.
    //------------------------------------------------------------------
    reg [1:0] ser;
    always @(posedge radio_clk) begin
        if (rst) ser <= 2'd0;
        else if (dv_valid[0]) ser <= ser + 2'd1;
    end

    wire signed [15:0] ser_i = (ser == 2'd0) ? dv_i[0] : (ser == 2'd1) ? dv_i[1]
                             : (ser == 2'd2) ? dv_i[2] : dv_i[3];
    wire signed [15:0] ser_q = (ser == 2'd0) ? dv_q[0] : (ser == 2'd1) ? dv_q[1]
                             : (ser == 2'd2) ? dv_q[2] : dv_q[3];

    wire        pw_valid, pw_last;
    wire [31:0] pw_pwr;

    radar_power u_power (
        .clk(radio_clk), .rst(rst),
        .in_valid(dv_valid[0]), .in_i(ser_i), .in_q(ser_q),
        .in_ch(ser), .in_last(dv_last[0]),
        .out_shift(4'd2),
        .out_valid(pw_valid), .out_pwr(pw_pwr), .out_last(pw_last)
    );

    //------------------------------------------------------------------
    // Detection, and capturing the complex samples that go with each hit.
    //
    // CFAR cannot decide about a cell until the training cells beyond it have
    // arrived, so its verdict lags the data.  The four complex channel values
    // for each cell are pushed into a delay of exactly that length, so that
    // when a hit is announced the values standing at the delay's output are
    // the ones that belong to it.  Buffering the whole complex cube instead
    // would need four megabits and there is no room for it.
    //------------------------------------------------------------------
    wire        hit_valid;
    wire [7:0]  hit_range;
    wire [8:0]  hit_dopp;
    wire [31:0] hit_pwr;
    wire [31:0] noise_out;
    wire [15:0] n_hits;
    wire        cfar_frame_done;

    radar_cfar2d u_cfar (
        .clk(radio_clk), .rst(rst),
        .n_range(n_range_max[8:0]), .n_doppler(N_DOPPLER[9:0]),
        .cfg_guard_r(cfar_guard_r), .cfg_guard_d(cfar_guard_d),
        .cfg_train_r(cfar_train_r), .cfg_train_d(cfar_train_d),
        .cfg_kind(cfar_kind), .cfg_alpha(cfar_alpha),
        // The transmit leakage sits at zero Doppler, so cfg_zero_dopp already
        // suppresses it and no separate range gate is needed. range_zero is a
        // host-side quantity: it moves the range ORIGIN, it does not blank a cell.
        .cfg_zero_dopp(zero_dopp), .cfg_max_hits(max_hits),
        .in_valid(pw_valid), .in_pwr(pw_pwr), .in_last(pw_last),
        .hit_valid(hit_valid), .hit_range(hit_range), .hit_dopp(hit_dopp),
        .hit_pwr(hit_pwr),
        .noise_out(noise_out), .n_hits(n_hits), .frame_done(cfar_frame_done)
    );

    wire        hitcap_cfg_error;
    wire signed [15:0] cap_i [0:3];
    wire signed [15:0] cap_q [0:3];

    radar_hitcap #(.N_DOPPLER(N_DOPPLER)) u_hitcap (
        .clk(radio_clk), .rst(rst),
        .cfg_guard_r(cfar_guard_r), .cfg_train_r(cfar_train_r),
        .cfg_guard_d(cfar_guard_d), .cfg_train_d(cfar_train_d),
        .cfg_error(hitcap_cfg_error),
        .in_valid(dv_valid[0]),
        .in_v0_i(dv_i[0]), .in_v0_q(dv_q[0]),
        .in_v1_i(dv_i[1]), .in_v1_q(dv_q[1]),
        .in_v2_i(dv_i[2]), .in_v2_q(dv_q[2]),
        .in_v3_i(dv_i[3]), .in_v3_q(dv_q[3]),
        .hit_stb(hit_valid),
        .out_v0_i(cap_i[0]), .out_v0_q(cap_q[0]),
        .out_v1_i(cap_i[1]), .out_v1_q(cap_q[1]),
        .out_v2_i(cap_i[2]), .out_v2_q(cap_q[2]),
        .out_v3_i(cap_i[3]), .out_v3_q(cap_q[3])
    );

    //------------------------------------------------------------------
    // Pack and ship
    //------------------------------------------------------------------
    reg [31:0] frame_index;
    always @(posedge radio_clk) begin
        if (rst)                   frame_index <= 32'd0;
        else if (cfar_frame_done)  frame_index <= frame_index + 32'd1;
    end

    wire sticky_ovf = ct_overflow[0] | ct_overflow[1]
                    | fe_ovf[0] | fe_ovf[1]
                    | dv_ovf[0] | dv_ovf[1] | dv_ovf[2] | dv_ovf[3];

    reg ovf_latch;
    always @(posedge radio_clk) begin
        if (rst)                  ovf_latch <= 1'b0;
        else if (cfar_frame_done) ovf_latch <= 1'b0;
        else if (sticky_ovf)      ovf_latch <= 1'b1;
    end
    assign overflow = ovf_latch;

    radar_pack u_pack (
        .clk(radio_clk), .rst(rst),
        .cfg_map_enable(ctrl_map_enable), .cfg_hits_enable(ctrl_hits_enable),
        .cfg_map_decim_r(map_decim_r), .cfg_map_decim_d(map_decim_d),
        .n_range(n_range_max[8:0]), .n_doppler(N_DOPPLER[9:0]),
        .cfg_flags({8'd0, ctrl_mimo_mode, ctrl_tx_enable, ovf_latch,
                    ctrl_hits_enable, ctrl_map_enable, 2'b00}),
        .frame_index(frame_index), .timestamp(vita_time), .noise(noise_out),
        .map_valid(pw_valid), .map_pwr(pw_pwr),
        .hit_valid(hit_valid), .hit_range(hit_range), .hit_dopp(hit_dopp),
        .hit_pwr(hit_pwr),
        .hit_v0_i(cap_i[0]), .hit_v0_q(cap_q[0]),
        .hit_v1_i(cap_i[1]), .hit_v1_q(cap_q[1]),
        .hit_v2_i(cap_i[2]), .hit_v2_q(cap_q[2]),
        .hit_v3_i(cap_i[3]), .hit_v3_q(cap_q[3]),
        .n_hits(n_hits),
        .frame_start(frame_start), .frame_end(cfar_frame_done),
        .m_valid(out_valid), .m_data(out_data), .m_ready(out_ready), .m_last(out_last)
    );

    // Tie off what is read but not used, so lint stays clean and a future
    // reader can see the omission is deliberate.
    wire _unused = &{1'b0, ct_s_ready[0], ct_s_ready[1], ct_frame_done[0], ct_frame_done[1],
                     fe_last[0], fe_last[1], dv_last[1], dv_last[2], dv_last[3],
                     tx_sel, frame_end, running, sample_idx, chirp_idx,
                     ctrl_frame_limit, test_tone, win_data_d, version_stb, 1'b0};

endmodule

`default_nettype wire
