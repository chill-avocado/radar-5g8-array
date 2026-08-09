//============================================================================
// radar_regs.v -- settings-bus register file.
//
// The B2xx settings bus is 8-bit address, 32-bit data, WRITE ONLY: the host
// calls uhd::usrp::multi_usrp::set_user_register(addr, data) and there is no
// read path at all.  So this file is a bank of flops with a write decoder and
// nothing else, and every piece of status the host needs comes back inside
// the receive stream header instead.  Addresses and field positions come from
// fpga/rtl/radar_pkg.svh section 6, which is the contract; they are not
// restated here.
//
// Two of the writes are events rather than values:
//   ctrl_soft_reset  one clock, self-clearing, whenever REG_CTRL is written
//                    with bit 1 set.  Flushes every pipeline and buffer.
//   win_we           one clock, whenever REG_WIN_WDATA is written.  The
//                    address comes from the separately written REG_WIN_WADDR.
//                    One bus write loads both tables at that address:
//                    win_data[15:0] is the range window and win_data[31:16]
//                    the Doppler window, also presented split as win_data_r
//                    and win_data_d so neither consumer has to slice.
//   version_stb      one clock, whenever REG_VERSION is written.  Stamps the
//                    build's version word into the next frame header.
//
// RESET DEFAULTS are the surveillance operating point of radar_pkg.svh: a
// 50 MHz sweep over 3072 clocks (50 us) on a 3840-clock PRI, 128 chirps per
// transmitter, TDM, 256 range bins by 256 chirps.  freq_start/freq_slope are
// the phase increments for that ramp: -25 MHz start, +1137778 per clock, which
// is 49.984 MHz of swept bandwidth by the last sample.
//
// tx_enable DEFAULTS TO ZERO.  This is a licensed 5.8 GHz transmitter; it does
// not come up radiating because a register happened to power up set.  The host
// asks for transmit explicitly.
//
// GEOMETRY.  n_range_log2 + n_chirp_log2 must equal RADAR_CT_WORDS_LOG2, since
// the corner-turn buffer is a fixed 65536 words per channel.  This file
// presents both fields; refusing to start when they do not add up is the
// controller's job, not the register file's.
//
// LATENCY
//   1 clock.  A write presented with set_stb high at cycle t appears on the
//   output ports at cycle t+1.  Pulse outputs are high for exactly that one
//   cycle.
//
// RESOURCES on XC7K325T
//   ~330 FF, ~90 slice LUT (the address decoder), 0 DSP, 0 BRAM.
//============================================================================
`timescale 1ns / 1ps
`include "radar_pkg.svh"
`default_nettype none

module radar_regs #(
    parameter [31:0] DEF_CTRL       = 32'h0000_0060,  // map + hits on, TX off
    parameter [31:0] DEF_FREQ_START = 32'h97D5_5555,  // -1747626667, -25 MHz
    parameter [31:0] DEF_FREQ_SLOPE = 32'h0011_5C72,  //     1137778, 1e12 Hz/s
    parameter [15:0] DEF_T_SWEEP    = 16'd3072,
    parameter [15:0] DEF_T_PRI      = 16'd3840,
    parameter [15:0] DEF_N_CHIRP    = 16'd128,        // per transmitter
    parameter [15:0] DEF_TX_GAIN    = 16'h7FFF,
    parameter [3:0]  DEF_DECHIRP_SH = 4'd0,
    parameter [19:0] DEF_FFT_SCALE_R= 20'h5_5555,     // 1 bit per stage
    parameter [15:0] DEF_FFT_SCALE_D= 16'h5555,
    parameter [31:0] DEF_CFAR_CFG   = 32'h0000_8822,  // 2 guard, 8 training, CA
    parameter [31:0] DEF_CFAR_ALPHA = 32'h000A_0000,  // 10.0 in Q16.16
    parameter [15:0] DEF_RANGE_ZERO = 16'd0,
    parameter [15:0] DEF_MAP_DECIM  = 16'h0101,       // no map decimation
    parameter [15:0] DEF_MAX_HITS   = 16'd64,
    parameter [7:0]  DEF_ZERO_DOPP  = 8'd2,
    parameter [7:0]  DEF_GEOM       = 8'h88,          // 256 range x 256 chirps
    parameter [31:0] DEF_TEST_TONE  = 32'h042A_AAAB   // 1 MHz
) (
    input  wire                 clk,
    input  wire                 rst,            // synchronous, active high
    input  wire                 set_stb,
    input  wire        [7:0]    set_addr,
    input  wire        [31:0]   set_data,

    // REG_CTRL
    output reg                  ctrl_enable,
    output reg                  ctrl_soft_reset,    // 1-cycle pulse
    output reg         [1:0]    ctrl_mimo_mode,
    output reg                  ctrl_tx_enable,
    output reg                  ctrl_map_enable,
    output reg                  ctrl_hits_enable,
    output reg                  ctrl_loopback,
    output reg         [15:0]   ctrl_frame_limit,

    // waveform and frame geometry
    output reg  signed [31:0]   freq_start,
    output reg  signed [31:0]   freq_slope,
    output reg         [15:0]   t_sweep,
    output reg         [15:0]   t_pri,
    output reg         [15:0]   n_chirp,
    output reg         [15:0]   tx_gain,
    output reg         [3:0]    dechirp_sh,

    // transforms
    output reg         [19:0]   fft_scale_r,
    output reg         [15:0]   fft_scale_d,
    output reg                  win_we,             // 1-cycle pulse
    output reg         [15:0]   win_addr,
    output reg         [31:0]   win_data,           // [31:16] Doppler, [15:0] range
    output wire signed [15:0]   win_data_r,         // the range half
    output wire signed [15:0]   win_data_d,         // the Doppler half

    // detection
    output reg         [3:0]    cfar_guard_range,
    output reg         [3:0]    cfar_guard_dopp,
    output reg         [3:0]    cfar_train_range,
    output reg         [3:0]    cfar_train_dopp,
    output reg         [1:0]    cfar_kind,
    output reg         [31:0]   cfar_alpha,
    output reg         [15:0]   range_zero,
    output reg         [7:0]    map_decim_r,
    output reg         [7:0]    map_decim_d,
    output reg         [15:0]   max_hits,
    output reg         [7:0]    zero_dopp,

    // runtime geometry split of the fixed-size corner-turn buffer
    output reg         [3:0]    geom_n_range_log2,
    output reg         [3:0]    geom_n_chirp_log2,

    // built-in test
    output reg  signed [31:0]   test_tone,
    output reg                  version_stb         // 1-cycle pulse
);

    // The two window tables share one 32-bit data register; presenting the
    // halves saves every consumer from slicing it and getting the order wrong.
    assign win_data_r = win_data[15:0];
    assign win_data_d = win_data[31:16];

    always @(posedge clk) begin
        if (rst) begin
            ctrl_enable       <= DEF_CTRL[0];
            ctrl_soft_reset   <= 1'b0;
            ctrl_mimo_mode    <= DEF_CTRL[3:2];
            ctrl_tx_enable    <= DEF_CTRL[4];
            ctrl_map_enable   <= DEF_CTRL[5];
            ctrl_hits_enable  <= DEF_CTRL[6];
            ctrl_loopback     <= DEF_CTRL[7];
            ctrl_frame_limit  <= DEF_CTRL[31:16];

            freq_start        <= $signed(DEF_FREQ_START);
            freq_slope        <= $signed(DEF_FREQ_SLOPE);
            t_sweep           <= DEF_T_SWEEP;
            t_pri             <= DEF_T_PRI;
            n_chirp           <= DEF_N_CHIRP;
            tx_gain           <= DEF_TX_GAIN;
            dechirp_sh        <= DEF_DECHIRP_SH;

            fft_scale_r       <= DEF_FFT_SCALE_R;
            fft_scale_d       <= DEF_FFT_SCALE_D;
            win_we            <= 1'b0;
            win_addr          <= 16'd0;
            win_data          <= 32'd0;

            cfar_guard_range  <= DEF_CFAR_CFG[3:0];
            cfar_guard_dopp   <= DEF_CFAR_CFG[7:4];
            cfar_train_range  <= DEF_CFAR_CFG[11:8];
            cfar_train_dopp   <= DEF_CFAR_CFG[15:12];
            cfar_kind         <= DEF_CFAR_CFG[17:16];
            cfar_alpha        <= DEF_CFAR_ALPHA;
            range_zero        <= DEF_RANGE_ZERO;
            map_decim_r       <= DEF_MAP_DECIM[7:0];
            map_decim_d       <= DEF_MAP_DECIM[15:8];
            max_hits          <= DEF_MAX_HITS;
            zero_dopp         <= DEF_ZERO_DOPP;

            geom_n_range_log2 <= DEF_GEOM[3:0];
            geom_n_chirp_log2 <= DEF_GEOM[7:4];

            test_tone         <= $signed(DEF_TEST_TONE);
            version_stb       <= 1'b0;
        end else begin
            // events default low, so every one of them is a single clock
            ctrl_soft_reset <= 1'b0;
            win_we          <= 1'b0;
            version_stb     <= 1'b0;

            if (set_stb) begin
                case (set_addr)
                    `RADAR_REG_CTRL: begin
                        ctrl_enable      <= set_data[0];
                        ctrl_soft_reset  <= set_data[1];
                        ctrl_mimo_mode   <= set_data[3:2];
                        ctrl_tx_enable   <= set_data[4];
                        ctrl_map_enable  <= set_data[5];
                        ctrl_hits_enable <= set_data[6];
                        ctrl_loopback    <= set_data[7];
                        ctrl_frame_limit <= set_data[31:16];
                    end
                    `RADAR_REG_FREQ_START : freq_start  <= $signed(set_data);
                    `RADAR_REG_FREQ_SLOPE : freq_slope  <= $signed(set_data);
                    `RADAR_REG_T_SWEEP    : t_sweep     <= set_data[15:0];
                    `RADAR_REG_T_PRI      : t_pri       <= set_data[15:0];
                    `RADAR_REG_N_CHIRP    : n_chirp     <= set_data[15:0];
                    `RADAR_REG_TX_GAIN    : tx_gain     <= set_data[15:0];
                    `RADAR_REG_DECHIRP_SH : dechirp_sh  <= set_data[3:0];
                    `RADAR_REG_FFT_SCALE_R: fft_scale_r <= set_data[19:0];
                    `RADAR_REG_FFT_SCALE_D: fft_scale_d <= set_data[15:0];
                    `RADAR_REG_WIN_WADDR  : win_addr    <= set_data[15:0];
                    `RADAR_REG_WIN_WDATA  : begin
                        win_data <= set_data;
                        win_we   <= 1'b1;
                    end
                    `RADAR_REG_CFAR_CFG: begin
                        cfar_guard_range <= set_data[3:0];
                        cfar_guard_dopp  <= set_data[7:4];
                        cfar_train_range <= set_data[11:8];
                        cfar_train_dopp  <= set_data[15:12];
                        cfar_kind        <= set_data[17:16];
                    end
                    `RADAR_REG_CFAR_ALPHA : cfar_alpha  <= set_data;
                    `RADAR_REG_RANGE_ZERO : range_zero  <= set_data[15:0];
                    `RADAR_REG_MAP_DECIM  : begin
                        map_decim_r <= set_data[7:0];
                        map_decim_d <= set_data[15:8];
                    end
                    `RADAR_REG_MAX_HITS   : max_hits    <= set_data[15:0];
                    `RADAR_REG_ZERO_DOPP  : zero_dopp   <= set_data[7:0];
                    `RADAR_REG_GEOM       : begin
                        geom_n_range_log2 <= set_data[3:0];
                        geom_n_chirp_log2 <= set_data[7:4];
                    end
                    `RADAR_REG_VERSION    : version_stb <= 1'b1;
                    `RADAR_REG_TEST_TONE  : test_tone   <= $signed(set_data);
                    default: ;
                endcase
            end
        end
    end

endmodule

`default_nettype wire
