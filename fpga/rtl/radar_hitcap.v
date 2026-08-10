//============================================================================
// radar_hitcap.v -- hold each cell's four complex channel values until CFAR
//                   has decided whether that cell is a detection
//
// THE PROBLEM
//   The angle of a target comes from the relative phase of the four virtual
//   channels at that cell.  But CFAR cannot judge a cell until the training
//   cells BEYOND it have arrived, so its verdict lags the data by
//
//       (guard_r + train_r) rows * n_doppler + (guard_d + train_d) cells
//
//   plus its own pipeline.  By the time a hit is announced, the complex values
//   that produced it are long gone from the transform outputs.
//
// WHY NOT JUST BUFFER THE CUBE
//   256 range x 128 Doppler x 4 channels x 32 bits is four megabits, and the
//   corner-turn buffer has already taken half the device.  There is no room,
//   and this board has no external memory.
//
// WHAT THIS DOES INSTEAD
//   A circular delay exactly as long as the CFAR's lag.  The four channel
//   values stream in; when a hit is announced, the values standing at the
//   delay's output are, by construction, the ones for that cell.  One buffer
//   of 4096 entries covers every configuration the core allows, and it costs
//   14 block RAMs instead of 114.
//
//   If the configured window is too deep for the delay -- which needs more
//   than seven range rows at the widest Doppler setting -- cfg_error goes high
//   and stays high, rather than the core quietly attaching the wrong complex
//   samples to every detection.  A wrong angle that looks plausible is far
//   more dangerous than a refusal.
//
// LATENCY
//   Combinational read from the buffer, registered out: hit values are valid
//   one clock after hit_stb.
//
// RESOURCES on XC7K325T
//   4096 x 128 bits = 524288 bits = 14 x RAMB36 (simple dual port)
//   ~120 slice LUT, ~150 FF, 0 DSP48
//============================================================================
`timescale 1ns / 1ps
`default_nettype none

module radar_hitcap #(
    parameter integer N_DOPPLER = 128,   // used only for the fit check
    parameter integer DEPTH     = 4096,
    parameter integer ADDR_W    = 12,    // log2(DEPTH)
    parameter integer PIPE      = 8      // CFAR's own pipeline, in cells
) (
    input  wire                 clk,
    input  wire                 rst,

    input  wire [3:0]           cfg_guard_r,
    input  wire [3:0]           cfg_train_r,
    input  wire [3:0]           cfg_guard_d,
    input  wire [3:0]           cfg_train_d,
    output reg                  cfg_error,

    input  wire                 in_valid,
    input  wire signed [15:0]   in_v0_i, in_v0_q,
    input  wire signed [15:0]   in_v1_i, in_v1_q,
    input  wire signed [15:0]   in_v2_i, in_v2_q,
    input  wire signed [15:0]   in_v3_i, in_v3_q,

    input  wire                 hit_stb,
    output reg  signed [15:0]   out_v0_i, out_v0_q,
    output reg  signed [15:0]   out_v1_i, out_v1_q,
    output reg  signed [15:0]   out_v2_i, out_v2_q,
    output reg  signed [15:0]   out_v3_i, out_v3_q
);

    //------------------------------------------------------------------
    // How far behind CFAR is, in cells.
    //------------------------------------------------------------------
    wire [4:0]  halo_r = {1'b0, cfg_guard_r} + {1'b0, cfg_train_r};
    wire [4:0]  halo_d = {1'b0, cfg_guard_d} + {1'b0, cfg_train_d};

    // Everything widened explicitly to 20 bits. The implicit-width rules would
    // give the right answer here, but a silent truncation in an address
    // calculation is exactly the bug that would attach the wrong complex
    // samples to a detection, so it is spelled out.
    localparam [19:0] ND20   = N_DOPPLER[19:0];
    localparam [19:0] PIPE20 = PIPE[19:0];
    localparam [19:0] DEPTH20 = DEPTH[19:0];

    wire [19:0] lag = ({15'd0, halo_r} * ND20) + {15'd0, halo_d} + PIPE20;

    always @(posedge clk) begin
        if (rst) cfg_error <= 1'b0;
        else     cfg_error <= (lag >= DEPTH20);
    end

    //------------------------------------------------------------------
    // The delay itself.  A plain circular buffer: write at the head, read
    // `lag` behind it.  Both addresses advance on the same strobe, so the
    // spacing is exact and does not depend on anything downstream.
    //------------------------------------------------------------------
    (* ram_style = "block" *)
    reg [127:0] buf_mem [0:DEPTH-1];

    reg [ADDR_W-1:0] wr_addr;
    wire [ADDR_W-1:0] rd_addr = wr_addr - lag[ADDR_W-1:0];

    wire [127:0] wr_word = {in_v0_i, in_v0_q, in_v1_i, in_v1_q,
                            in_v2_i, in_v2_q, in_v3_i, in_v3_q};

    reg [127:0] rd_word;

    always @(posedge clk) begin
        if (rst) begin
            wr_addr <= {ADDR_W{1'b0}};
        end else if (in_valid) begin
            buf_mem[wr_addr] <= wr_word;
            wr_addr          <= wr_addr + 1'b1;
        end
        // Read every cycle so the value is always one clock fresh; the read
        // port is free in a simple dual-port block RAM.
        rd_word <= buf_mem[rd_addr];
    end

    //------------------------------------------------------------------
    // Present the cell's values when CFAR announces it.
    //------------------------------------------------------------------
    always @(posedge clk) begin
        if (rst) begin
            out_v0_i <= 16'sd0; out_v0_q <= 16'sd0;
            out_v1_i <= 16'sd0; out_v1_q <= 16'sd0;
            out_v2_i <= 16'sd0; out_v2_q <= 16'sd0;
            out_v3_i <= 16'sd0; out_v3_q <= 16'sd0;
        end else if (hit_stb) begin
            out_v0_i <= rd_word[127:112]; out_v0_q <= rd_word[111:96];
            out_v1_i <= rd_word[ 95: 80]; out_v1_q <= rd_word[ 79:64];
            out_v2_i <= rd_word[ 63: 48]; out_v2_q <= rd_word[ 47:32];
            out_v3_i <= rd_word[ 31: 16]; out_v3_q <= rd_word[ 15: 0];
        end
    end

endmodule

`default_nettype wire
