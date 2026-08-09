//============================================================================
// radar_corner_turn.v -- ping-pong corner turn (matrix transpose) in block RAM
//
// PURPOSE
//   The range transform delivers one chirp at a time: range bin 0..n_range-1
//   in order, then the next chirp.  The Doppler transform needs the opposite
//   order: every chirp of range bin 0, then every chirp of range bin 1.  This
//   module holds a whole coherent processing interval and hands it back
//   transposed.  Two buffers: one is being filled by the incoming chirps while
//   the other is read out, so the stream never has to stop.
//
// RUNTIME GEOMETRY (radar_pkg.svh, "THE MEMORY CONSTRAINT")
//   The PRODUCT of range bins and chirps is fixed at 2**CT_WORDS_LOG2 = 65536
//   words per buffer per receive channel; the SPLIT is chosen at run time by
//   n_range_log2 / n_chirp_log2.  One bitstream, two operating points:
//     surveillance   256 range x 256 chirps   (n_range_log2=8, n_chirp_log2=8)
//     fine Doppler   128 range x 512 chirps   (n_range_log2=7, n_chirp_log2=9)
//   If the two do not sum to CT_WORDS_LOG2 the module raises cfg_error and
//   holds itself in reset rather than addressing outside the buffer.  The
//   geometry is sampled on the first word of a write frame and held for the
//   rest of it, and each buffer carries the geometry it was written with, so a
//   change of split between frames cannot corrupt the frame already in flight.
//
// ADDRESSING -- VARIABLE SHIFT, NEVER A MULTIPLIER
//   Both dimensions are powers of two, so
//     storage address    = (chirp << n_range_log2) | range
//     read-out ordinal   = (range << n_chirp_log2) | chirp
//   The transpose is the traversal order: the reader walks range on the outer
//   loop and chirp on the inner loop, so successive output words are ordinal
//   0,1,2,... while the storage address they come from is the shift above.
//   Both expressions are barrel shifts of a counter by a 4-bit amount; no
//   DSP48 is inferred anywhere in this module.
//
// LATENCY
//   Architectural: one whole frame.  A buffer cannot be read until it is full,
//   which is what a corner turn is.
//   Pipeline, from a read being issued to that word appearing on m_data:
//   2 clocks (1 block-RAM read + 1 output-FIFO register).
//   Throughput: one word per clock in and one word per clock out, sustained,
//   with no gap between consecutive frames.
//
// RESOURCE ESTIMATE, XC7K325T
//   Storage: 65536 words x 32 bits x 2 buffers = 4,194,304 bits per receive
//   channel = 114 BRAM36K tiles per channel, 228 tiles for both channels,
//   51% of the device's 445 tiles.  (Vivado maps a 32-bit-wide array in
//   1024x32 mode and leaves the 4 parity bits of each tile unused, so the
//   placed figure is 128 tiles per channel unless the word is widened to 36
//   bits; the 114/228 numbers above are the raw bit count from radar_pkg.svh.)
//   Logic: ~180 LUT, ~150 FF.  DSP48: 0.
//   The array is written by one port and read by another, both synchronous,
//   with no read-during-write dependency, which is exactly the simple
//   dual-port pattern Vivado infers as true block RAM.
//============================================================================
`timescale 1ns / 1ps
`default_nettype none

module radar_corner_turn #(
    parameter integer DATA_W        = 32,
    parameter integer CT_WORDS_LOG2 = 16
) (
    input  wire                  clk,
    input  wire                  rst,

    // Runtime geometry.  Must sum to CT_WORDS_LOG2.
    input  wire [3:0]            n_range_log2,
    input  wire [3:0]            n_chirp_log2,
    output wire                  cfg_error,

    // Range-FFT input, chirp by chirp, range bin 0..n_range-1 within a chirp.
    input  wire                  s_valid,
    input  wire [DATA_W-1:0]     s_data,
    output wire                  s_ready,
    input  wire                  s_last,    // last range bin of this chirp

    // Transposed output: every chirp of range bin 0, then range bin 1, ...
    output wire                  m_valid,
    output wire [DATA_W-1:0]     m_data,
    input  wire                  m_ready,
    output wire                  m_last,    // last chirp of this range bin

    output reg                   frame_done,
    output reg                   overflow
);

    //------------------------------------------------------------------------
    // Sizes
    //------------------------------------------------------------------------
    localparam integer AW      = CT_WORDS_LOG2;          // 16
    localparam integer WORDS   = 1 << CT_WORDS_LOG2;     // 65536
    localparam integer FIFO_D  = 4;                      // output skid depth

    localparam [AW-1:0] AONE   = {{(AW-1){1'b0}}, 1'b1};
    localparam [4:0]    SUM_EXP = CT_WORDS_LOG2;

    //------------------------------------------------------------------------
    // Configuration check.  A bad split holds the whole module in reset.
    //------------------------------------------------------------------------
    wire [4:0] log2_sum = {1'b0, n_range_log2} + {1'b0, n_chirp_log2};

    assign cfg_error = (log2_sum != SUM_EXP) ||
                       (n_range_log2 == 4'd0) || (n_chirp_log2 == 4'd0);

    wire core_rst = rst | cfg_error;

    //------------------------------------------------------------------------
    // The store: two buffers in one array, buffer select is the top address
    // bit.  One synchronous write port, one synchronous read port.
    //------------------------------------------------------------------------
    reg [DATA_W-1:0] mem [(2*WORDS)-1:0];

    reg [1:0] buf_full;                 // buffer holds a complete frame
    reg [3:0] buf_nr_log2 [1:0];        // geometry that buffer was written with
    reg [3:0] buf_nc_log2 [1:0];

    //------------------------------------------------------------------------
    // WRITE SIDE
    //------------------------------------------------------------------------
    reg           wr_sel;
    reg  [AW-1:0] wr_range;
    reg  [AW-1:0] wr_chirp;
    reg  [3:0]    wr_nr_log2;           // latched geometry for this frame
    reg  [3:0]    wr_nc_log2;

    // Sample the geometry on the very first word of a frame, hold it after.
    wire          wr_at_start  = (wr_range == {AW{1'b0}}) && (wr_chirp == {AW{1'b0}});
    wire [3:0]    eff_nr_log2  = wr_at_start ? n_range_log2 : wr_nr_log2;
    wire [3:0]    eff_nc_log2  = wr_at_start ? n_chirp_log2 : wr_nc_log2;

    wire [AW-1:0] wr_range_max = (AONE << eff_nr_log2) - AONE;
    wire [AW-1:0] wr_chirp_max = (AONE << eff_nc_log2) - AONE;

    // storage address = (chirp << n_range_log2) | range   -- variable shift
    wire [AW-1:0] wr_addr = (wr_chirp << eff_nr_log2) | wr_range;

    assign s_ready = ~core_rst & ~buf_full[wr_sel];

    wire wr_fire      = s_valid & s_ready;
    wire wr_chirp_end = wr_fire & (s_last | (wr_range == wr_range_max));
    wire wr_frame_end = wr_chirp_end & (wr_chirp == wr_chirp_max);

    always @(posedge clk) begin
        if (core_rst) begin
            wr_sel     <= 1'b0;
            wr_range   <= {AW{1'b0}};
            wr_chirp   <= {AW{1'b0}};
            wr_nr_log2 <= 4'd8;
            wr_nc_log2 <= 4'd8;
            overflow   <= 1'b0;
        end else begin
            if (wr_fire) begin
                wr_nr_log2 <= eff_nr_log2;
                wr_nc_log2 <= eff_nc_log2;
                if (wr_frame_end) begin
                    wr_range <= {AW{1'b0}};
                    wr_chirp <= {AW{1'b0}};
                    wr_sel   <= ~wr_sel;
                end else if (wr_chirp_end) begin
                    wr_range <= {AW{1'b0}};
                    wr_chirp <= wr_chirp + AONE;
                end else begin
                    wr_range <= wr_range + AONE;
                end
            end
            // A sample offered while both buffers are busy is a sample lost.
            if (s_valid & ~s_ready) overflow <= 1'b1;
        end
    end

    //------------------------------------------------------------------------
    // READ SIDE.  Range on the outer loop, chirp on the inner loop: that
    // traversal is the transpose.
    //------------------------------------------------------------------------
    reg           rd_sel;
    reg           rd_active;
    reg  [AW-1:0] rd_range;
    reg  [AW-1:0] rd_chirp;
    reg  [3:0]    rd_nr_log2;
    reg  [3:0]    rd_nc_log2;

    wire [AW-1:0] rd_range_max = (AONE << rd_nr_log2) - AONE;
    wire [AW-1:0] rd_chirp_max = (AONE << rd_nc_log2) - AONE;

    wire [AW-1:0] rd_addr = (rd_chirp << rd_nr_log2) | rd_range;

    wire          rd_row_end   = (rd_chirp == rd_chirp_max);
    wire          rd_frame_end = rd_row_end & (rd_range == rd_range_max);

    //------------------------------------------------------------------------
    // Output FIFO.  Four entries is enough to cover the one read in flight
    // plus a cycle of downstream backpressure, so a read may be issued on
    // every clock and the output sustains one word per clock.
    //------------------------------------------------------------------------
    reg [DATA_W-1:0] fifo_d [FIFO_D-1:0];
    reg [FIFO_D-1:0] fifo_l;            // last chirp of a range bin
    reg [FIFO_D-1:0] fifo_e;            // last word of a frame
    reg [1:0]        fifo_wp, fifo_rp;
    reg [2:0]        fifo_cnt;

    wire fifo_pop  = m_valid & m_ready;
    wire rd_issue  = rd_active & (fifo_cnt < 3'd3) & ~core_rst;

    reg              ram_dv;
    reg              ram_last;
    reg              ram_end;
    reg [DATA_W-1:0] ram_q;

    assign m_valid = (fifo_cnt != 3'd0);
    assign m_data  = fifo_d[fifo_rp];
    assign m_last  = fifo_l[fifo_rp];

    // Block RAM: one write port, one read port, both synchronous.
    always @(posedge clk) begin
        if (wr_fire)  mem[{wr_sel, wr_addr}] <= s_data;
        if (rd_issue) ram_q <= mem[{rd_sel, rd_addr}];
    end

    always @(posedge clk) begin
        if (core_rst) begin
            rd_sel     <= 1'b0;
            rd_active  <= 1'b0;
            rd_range   <= {AW{1'b0}};
            rd_chirp   <= {AW{1'b0}};
            rd_nr_log2 <= 4'd8;
            rd_nc_log2 <= 4'd8;
            buf_full   <= 2'b00;
            ram_dv     <= 1'b0;
            ram_last   <= 1'b0;
            ram_end    <= 1'b0;
            fifo_wp    <= 2'd0;
            fifo_rp    <= 2'd0;
            fifo_cnt   <= 3'd0;
            fifo_l     <= {FIFO_D{1'b0}};
            fifo_e     <= {FIFO_D{1'b0}};
            frame_done <= 1'b0;
        end else begin
            frame_done <= 1'b0;

            // ---- buffer handover ------------------------------------------
            if (wr_frame_end) begin
                buf_full[wr_sel]    <= 1'b1;
                buf_nr_log2[wr_sel] <= eff_nr_log2;
                buf_nc_log2[wr_sel] <= eff_nc_log2;
            end

            if (!rd_active && buf_full[rd_sel]) begin
                rd_active  <= 1'b1;
                rd_range   <= {AW{1'b0}};
                rd_chirp   <= {AW{1'b0}};
                rd_nr_log2 <= buf_nr_log2[rd_sel];
                rd_nc_log2 <= buf_nc_log2[rd_sel];
            end

            // ---- read address sequencing ----------------------------------
            if (rd_issue) begin
                if (rd_frame_end) begin
                    rd_active        <= 1'b0;
                    buf_full[rd_sel] <= 1'b0;
                    rd_sel           <= ~rd_sel;
                    rd_range         <= {AW{1'b0}};
                    rd_chirp         <= {AW{1'b0}};
                end else if (rd_row_end) begin
                    rd_chirp <= {AW{1'b0}};
                    rd_range <= rd_range + AONE;
                end else begin
                    rd_chirp <= rd_chirp + AONE;
                end
            end

            // ---- block RAM output stage -----------------------------------
            ram_dv   <= rd_issue;
            ram_last <= rd_row_end;
            ram_end  <= rd_frame_end;

            // ---- output FIFO ----------------------------------------------
            if (ram_dv) begin
                fifo_d[fifo_wp] <= ram_q;
                fifo_l[fifo_wp] <= ram_last;
                fifo_e[fifo_wp] <= ram_end;
                fifo_wp         <= fifo_wp + 2'd1;
            end
            if (fifo_pop) begin
                fifo_rp    <= fifo_rp + 2'd1;
                frame_done <= fifo_e[fifo_rp];
            end
            case ({ram_dv, fifo_pop})
                2'b10:   fifo_cnt <= fifo_cnt + 3'd1;
                2'b01:   fifo_cnt <= fifo_cnt - 3'd1;
                default: fifo_cnt <= fifo_cnt;
            endcase
        end
    end

endmodule

`default_nettype wire
