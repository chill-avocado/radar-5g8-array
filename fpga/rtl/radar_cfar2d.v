//============================================================================
// radar_cfar2d.v -- streaming two-dimensional CFAR over the range-Doppler map
//
// PURPOSE
//   Decide, cell by cell, whether the integrated power at a (range, Doppler)
//   cell is bigger than the surrounding clutter by enough to call it a target.
//   "Constant false alarm rate" means the threshold is not a fixed number: it
//   is measured from the neighbours of every cell, so a map that is noisy
//   everywhere and a map that is quiet everywhere both produce the same
//   trickle of false alarms.  Around each cell under test sit a ring of GUARD
//   cells, which are ignored because a real target spills into them, and
//   outside that a ring of TRAINING cells, which are averaged to give the
//   local noise level.
//
// COST PER CELL IS CONSTANT -- TWO NESTED INCREMENTAL SUMS
//   A naive implementation adds up the window at every cell: 625 additions per
//   cell at the largest setting, 37 million per frame.  This one does eleven,
//   whatever the window size, by never recomputing a sum it already has.
//
//   First direction (range).  Three per-Doppler-column running sums are kept
//   across the rows held in the line buffer:
//       A[d]  the train_r lagging training rows
//       B[d]  the 2*guard_r+1 guard rows, the cell under test in the middle
//       C[d]  the train_r leading training rows
//   When a new row arrives each column moves one row on: C takes in the new
//   sample and hands its oldest row to B, B hands its oldest to A, and A drops
//   the row that has fallen out of the window.  Three adds and three
//   subtracts, per column, per row, regardless of how tall the window is.
//
//   Second direction (Doppler).  Those column sums are then slid sideways:
//       SA, SB, SC   over the full 2*(guard_d+train_d)+1 columns
//       SBi          over the 2*guard_d+1 guard columns only
//   each maintained by adding the column entering the window and subtracting
//   the column leaving it, taken from a short delay line of past columns.
//
//   From those four numbers:
//       outer window  = SA + SB + SC
//       guard window  = SBi                (inclusive of the cell under test)
//       training sum  = outer - guard      <- exactly the spec's definition
//       lagging half  = SA,  leading half  = SC   (the range split GO/SO need)
//
// THE THREE FLAVOURS
//   CA  cell averaging   noise = training sum / number of training cells.
//                        The best estimator when the neighbourhood really is
//                        uniform, and the worst when it is not.
//   GO  greatest of      the greater of the leading and lagging range halves.
//                        Raises the threshold at a clutter edge, so the radar
//                        does not light up along the boundary of a treeline.
//   SO  smallest of      the lesser of the two halves.  Keeps the threshold
//                        down when a second target sits in the training cells,
//                        which is how two aircraft in formation stay resolved.
//   3   pass-through     every tested cell is reported; used to dump raw maps.
//
// DIVISION WITHOUT A DIVIDER IN THE DATAPATH
//   The number of reference cells changes only when the configuration changes,
//   so its reciprocal is computed once by a small sequential divider and the
//   per-cell "division" is a multiply by round(2**24 / n_ref).  The relative
//   error of that reciprocal is under 4e-5 for every legal window size.
//   The same divider produces the frame's mean noise for the packet header.
//
// FRAME BOUNDARIES COST NOTHING
//   There is no input ready signal: this block must accept a sample on every
//   clock.  So the running sums are not cleared between frames.  Instead, for
//   the first rows of a frame the terms that would refer to the previous frame
//   are forced to zero and the band sums are forced to zero until their first
//   real row arrives, which flushes the stale content in the same pass.
//
// EDGES
//   A cell is tested only when the whole window fits inside the map, which is
//   range Wr..n_range-1-Wr and Doppler Wd..n_doppler-1-Wd, with
//   Wr = guard_r+train_r and Wd = guard_d+train_d.  Cells within cfg_zero_dopp
//   of DC are suppressed (cfg_zero_dopp = 0 suppresses bin 0 alone).
//   Emission stops after cfg_max_hits, but counting does not: n_hits is the
//   true number of detections in the frame.
//
// LATENCY
//   The cell under test trails the newest sample by Wr rows plus Wd columns --
//   that is inherent to a centred window, not an implementation cost.
//   On top of that the pipeline is 7 clocks from a sample entering to that
//   cell's decision appearing on hit_valid.
//   frame_done pulses about 60 clocks after the last sample of a frame, once
//   the sequential divider has finished the frame's mean noise, so n_hits and
//   noise_out are final when it fires.
//   Throughput: one cell per clock, sustained, no stalls.
//
// RESOURCE ESTIMATE, XC7K325T
//   BRAM36: 13 + 13 for the two line-buffer copies (25 rows x 512 words x 32
//           bits each; two copies because three different rows must be read
//           in the same clock and a block RAM has two ports),
//            8 for the delay memory that carries the cell under test forward,
//            3 for the A/B/C column sums (512 x 38 bits each)
//           = 37 tiles, 8% of the device's 445.
//   DSP48 : ~6 for training_sum x reciprocal, ~4 for alpha x noise,
//           ~2 for the small window-size products = ~12.
//   Logic : ~2600 LUT, ~2200 FF.
//============================================================================
`timescale 1ns / 1ps
`default_nettype none

module radar_cfar2d #(
    parameter integer N_RANGE     = 256,   // largest range extent supported
    parameter integer N_DOPPLER   = 512,   // largest Doppler extent supported
    parameter integer MAX_GUARD   = 4,
    parameter integer MAX_TRAIN   = 8
) (
    input  wire        clk,
    input  wire        rst,

    // Map geometry for this frame (latched on the first sample of a frame).
    input  wire [8:0]  n_range,
    input  wire [9:0]  n_doppler,

    // CFAR configuration.
    input  wire [3:0]  cfg_guard_r,
    input  wire [3:0]  cfg_guard_d,
    input  wire [3:0]  cfg_train_r,
    input  wire [3:0]  cfg_train_d,
    input  wire [1:0]  cfg_kind,       // 0 CA, 1 GO, 2 SO, 3 pass-through
    input  wire [31:0] cfg_alpha,      // Q16.16 threshold multiplier
    input  wire [7:0]  cfg_zero_dopp,  // Doppler bins each side of DC to blank
    input  wire [15:0] cfg_max_hits,

    // Integrated power map, range major.
    input  wire        in_valid,
    input  wire [31:0] in_pwr,
    input  wire        in_last,        // last cell of the frame

    // Detections.
    output reg         hit_valid,
    output reg  [7:0]  hit_range,
    output reg  [8:0]  hit_dopp,
    output reg  [31:0] hit_pwr,

    // Per-frame results, final when frame_done pulses.
    output reg  [31:0] noise_out,
    output reg  [15:0] n_hits,
    output reg         frame_done
);

    //------------------------------------------------------------------------
    // Fixed sizes
    //------------------------------------------------------------------------
    localparam integer LMAX    = 2*(MAX_GUARD+MAX_TRAIN) + 1;  // 25 rows
    localparam integer DSHIFT  = 9;                            // 512 columns
    localparam integer LBDEPTH = LMAX * (1<<DSHIFT);           // 12800 words
    localparam integer LBAW    = 14;
    localparam integer CUTAW   = 13;                           // 8192 words
    localparam integer COLW    = 38;                           // column sums
    localparam integer ACCW    = 48;                           // RADAR_CFAR_ACC_W
    localparam integer DLD     = 32;                           // Doppler delay line
    localparam integer RECW    = 26;                           // reciprocal, 24 frac
    localparam integer NUMW    = 56;                           // divider numerator

    localparam [COLW-1:0] COLZ = {COLW{1'b0}};
    localparam [ACCW-1:0] ACCZ = {ACCW{1'b0}};

    //------------------------------------------------------------------------
    // Clamp the window configuration to what the buffers can hold.  A silly
    // register write must not be able to address outside the line buffer.
    //------------------------------------------------------------------------
    wire [3:0] g_r = (cfg_guard_r > MAX_GUARD[3:0]) ? MAX_GUARD[3:0] : cfg_guard_r;
    wire [3:0] g_d = (cfg_guard_d > MAX_GUARD[3:0]) ? MAX_GUARD[3:0] : cfg_guard_d;
    wire [3:0] t_r = (cfg_train_r > MAX_TRAIN[3:0]) ? MAX_TRAIN[3:0] : cfg_train_r;
    wire [3:0] t_d = (cfg_train_d > MAX_TRAIN[3:0]) ? MAX_TRAIN[3:0] : cfg_train_d;

    //------------------------------------------------------------------------
    // Configuration latched for the duration of a frame.
    //------------------------------------------------------------------------
    reg  [3:0]  q_gr, q_gd, q_tr, q_td;
    reg  [1:0]  q_kind;
    reg  [31:0] q_alpha;
    reg  [7:0]  q_zd;
    reg  [15:0] q_maxhits;
    reg  [8:0]  q_nr;
    reg  [9:0]  q_nd;

    wire [4:0] w_r     = {1'b0, q_gr} + {1'b0, q_tr};     // Wr, <= 12
    wire [4:0] w_d     = {1'b0, q_gd} + {1'b0, q_td};     // Wd, <= 12
    wire [5:0] l_act   = {1'b0, w_r} + {1'b0, w_r} + 6'd1; // 2*Wr+1, <= 25
    wire [5:0] wr_span = l_act;
    wire [5:0] wd_span = {1'b0, w_d} + {1'b0, w_d} + 6'd1; // 2*Wd+1, <= 25
    wire [5:0] gr_span = {2'b0, q_gr} + {2'b0, q_gr} + 6'd1;
    wire [5:0] gd_span = {2'b0, q_gd} + {2'b0, q_gd} + 6'd1;

    // Row offsets, from the newest row, of the two band boundaries.
    wire [5:0] off_bc  = {2'b0, q_tr};                                  // C -> B
    wire [5:0] off_ab  = {2'b0, q_tr} + {2'b0, q_gr} + {2'b0, q_gr} + 6'd1; // B -> A

    // Reference-cell counts.  Small products, evaluated once per frame.
    wire [11:0] n_outer = wr_span * wd_span;              // <= 625
    wire [11:0] n_guard = gr_span * gd_span;              // <= 81
    wire [11:0] n_ref_f = n_outer - n_guard;              // CA training cells
    wire [11:0] n_ref_h = {6'd0, q_tr} * wd_span;         // one GO/SO half

    //------------------------------------------------------------------------
    // Frame sequencing.  No backpressure: a sample is taken every clock it is
    // offered, and the geometry is sampled on the first sample of a frame.
    //------------------------------------------------------------------------
    reg        frame_first;
    reg [8:0]  r_cnt;
    reg [9:0]  d_cnt;
    reg [5:0]  wr_slot;
    reg [CUTAW-1:0] cut_wp;
    reg [5:0]  push_cnt;      // pushes into the Doppler delay line this frame

    wire       row_end   = (d_cnt == (q_nd - 10'd1));
    wire       take      = in_valid;

    //------------------------------------------------------------------------
    // Line buffer.  Two identical copies: copy 0's write port also returns the
    // row falling out of the window (read-before-write), copy 0's second port
    // reads the C->B boundary row and copy 1's second port the B->A boundary
    // row.  Three rows in one clock needs more than one block RAM's two ports.
    //------------------------------------------------------------------------
    reg [31:0] lb0 [LBDEPTH-1:0];
    reg [31:0] lb1 [LBDEPTH-1:0];

    // slot(offset) = (wr_slot - offset) mod l_act
    function [5:0] slot_of;
        input [5:0] off;
        begin
            slot_of = (wr_slot >= off) ? (wr_slot - off) : (wr_slot + l_act - off);
        end
    endfunction

    wire [5:0] slot_w  = wr_slot;
    wire [5:0] slot_bc = slot_of(off_bc);
    wire [5:0] slot_ab = slot_of(off_ab);

    wire [LBAW-1:0] lb_addr_w  = {slot_w [4:0], d_cnt[DSHIFT-1:0]};
    wire [LBAW-1:0] lb_addr_bc = {slot_bc[4:0], d_cnt[DSHIFT-1:0]};
    wire [LBAW-1:0] lb_addr_ab = {slot_ab[4:0], d_cnt[DSHIFT-1:0]};

    reg [31:0] q_leave;   // row that has just fallen out of the window
    reg [31:0] q_bc;      // row crossing from C into B
    reg [31:0] q_ab;      // row crossing from B into A

    always @(posedge clk) begin
        if (take) begin
            q_leave <= lb0[lb_addr_w];      // read before write: the old row
            lb0[lb_addr_w] <= in_pwr;
            lb1[lb_addr_w] <= in_pwr;
            q_bc <= lb0[lb_addr_bc];
            q_ab <= lb1[lb_addr_ab];
        end
    end

    //------------------------------------------------------------------------
    // Delay memory: carries the cell under test forward so its power arrives
    // at the same clock as the window statistics computed around it.
    //------------------------------------------------------------------------
    reg  [31:0]      cutmem [(1<<CUTAW)-1:0];
    reg  [CUTAW-1:0] cut_delay;                 // Wr*n_doppler + Wd
    wire [CUTAW-1:0] cut_ra = cut_wp - cut_delay;
    reg  [31:0]      q_cut;

    always @(posedge clk) begin
        if (take) begin
            q_cut <= cutmem[cut_ra];
            cutmem[cut_wp] <= in_pwr;
        end
    end

    //------------------------------------------------------------------------
    // Column band sums A, B, C.  Read at stage 0, written back at stage 2; the
    // same column is not revisited for n_doppler clocks so there is no hazard.
    //------------------------------------------------------------------------
    reg [COLW-1:0] colA [(1<<DSHIFT)-1:0];
    reg [COLW-1:0] colB [(1<<DSHIFT)-1:0];
    reg [COLW-1:0] colC [(1<<DSHIFT)-1:0];

    reg [COLW-1:0] qA, qB, qC;

    always @(posedge clk) begin
        if (take) begin
            qA <= colA[d_cnt[DSHIFT-1:0]];
            qB <= colB[d_cnt[DSHIFT-1:0]];
            qC <= colC[d_cnt[DSHIFT-1:0]];
        end
    end

    //------------------------------------------------------------------------
    // Pipeline stage 1 registers
    //------------------------------------------------------------------------
    reg        v1;
    reg [31:0] pwr1;
    reg [8:0]  r1;
    reg [9:0]  d1;

    always @(posedge clk) begin
        if (rst) begin
            v1 <= 1'b0;
        end else begin
            v1 <= take;
        end
        if (take) begin
            pwr1 <= in_pwr;
            r1   <= r_cnt;
            d1   <= d_cnt;
        end
    end

    //------------------------------------------------------------------------
    // Stage 2: advance the three column band sums.
    //
    // Terms that would reach back before the start of the frame are forced to
    // zero, and each band is forced to zero until its first real row arrives.
    // That flushes the previous frame's residue with no flush cycles.
    //------------------------------------------------------------------------
    wire have_leave = (r1 >= {3'd0, l_act});   // row r1-l_act exists this frame
    wire have_ab    = (r1 >= {3'd0, off_ab});
    wire have_bc    = (r1 >= {3'd0, off_bc});
    wire tr_zero    = (q_tr == 4'd0);

    wire [COLW-1:0] e_leave = have_leave ? {{(COLW-32){1'b0}}, q_leave} : COLZ;
    wire [COLW-1:0] e_ab    = have_ab    ? {{(COLW-32){1'b0}}, q_ab   } : COLZ;
    wire [COLW-1:0] e_bc    = have_bc    ? {{(COLW-32){1'b0}}, q_bc   } : COLZ;
    wire [COLW-1:0] e_new   = {{(COLW-32){1'b0}}, pwr1};

    wire [COLW-1:0] A_new = (tr_zero || !have_ab) ? COLZ : (qA + e_ab  - e_leave);
    wire [COLW-1:0] B_new = (!have_bc)            ? COLZ : (qB + e_bc  - e_ab);
    wire [COLW-1:0] C_new = tr_zero ? COLZ :
                            ((r1 == 9'd0) ? e_new : (qC + e_new - e_bc));

    reg        v2;
    reg [8:0]  r2;
    reg [9:0]  d2;
    reg [COLW-1:0] A2, B2, C2;

    always @(posedge clk) begin
        if (rst) v2 <= 1'b0;
        else     v2 <= v1;
        if (v1) begin
            r2 <= r1;
            d2 <= d1;
            A2 <= A_new;
            B2 <= B_new;
            C2 <= C_new;
            colA[d1[DSHIFT-1:0]] <= A_new;
            colB[d1[DSHIFT-1:0]] <= B_new;
            colC[d1[DSHIFT-1:0]] <= C_new;
        end
    end

    //------------------------------------------------------------------------
    // Stage 3: slide the four Doppler sums.
    //------------------------------------------------------------------------
    reg [COLW-1:0] dlA [DLD-1:0];
    reg [COLW-1:0] dlB [DLD-1:0];
    reg [COLW-1:0] dlC [DLD-1:0];
    reg [4:0]      dl_wp;

    wire [5:0] tap_out   = wd_span;                       // 2*Wd+1
    wire [5:0] tap_bi_in = {2'd0, q_td};                  // train_d
    wire [5:0] tap_bi_o  = {1'b0, w_d} + {2'd0, q_gd} + 6'd1;

    wire [4:0] ia_out  = dl_wp - tap_out[4:0];
    wire [4:0] ib_in   = dl_wp - tap_bi_in[4:0];
    wire [4:0] ib_out  = dl_wp - tap_bi_o[4:0];

    wire [COLW-1:0] tA_out = dlA[ia_out];
    wire [COLW-1:0] tB_out = dlB[ia_out];
    wire [COLW-1:0] tC_out = dlC[ia_out];
    wire [COLW-1:0] tBi_in = (tap_bi_in == 6'd0) ? B2 : dlB[ib_in];
    wire [COLW-1:0] tBi_o  = dlB[ib_out];

    wire sub_out = (push_cnt >= tap_out[5:0]);
    wire add_bi  = (push_cnt >= tap_bi_in);
    wire sub_bi  = (push_cnt >= tap_bi_o);

    reg [ACCW-1:0] SA, SB, SC, SBi;

    wire [ACCW-1:0] xA2  = {{(ACCW-COLW){1'b0}}, A2};
    wire [ACCW-1:0] xB2  = {{(ACCW-COLW){1'b0}}, B2};
    wire [ACCW-1:0] xC2  = {{(ACCW-COLW){1'b0}}, C2};
    wire [ACCW-1:0] xAo  = sub_out ? {{(ACCW-COLW){1'b0}}, tA_out} : ACCZ;
    wire [ACCW-1:0] xBo  = sub_out ? {{(ACCW-COLW){1'b0}}, tB_out} : ACCZ;
    wire [ACCW-1:0] xCo  = sub_out ? {{(ACCW-COLW){1'b0}}, tC_out} : ACCZ;
    wire [ACCW-1:0] xBii = add_bi  ? {{(ACCW-COLW){1'b0}}, tBi_in} : ACCZ;
    wire [ACCW-1:0] xBio = sub_bi  ? {{(ACCW-COLW){1'b0}}, tBi_o } : ACCZ;

    reg        v3;
    reg [8:0]  r3;
    reg [9:0]  d3;
    reg [ACCW-1:0] SA3, SC3, tr3;

    always @(posedge clk) begin
        if (rst) begin
            v3    <= 1'b0;
            SA    <= ACCZ;
            SB    <= ACCZ;
            SC    <= ACCZ;
            SBi   <= ACCZ;
            dl_wp <= 5'd0;
        end else begin
            v3 <= v2;
            if (v2) begin
                if (frame_first) begin
                    SA  <= xA2;
                    SB  <= xB2;
                    SC  <= xC2;
                    SBi <= (tap_bi_in == 6'd0) ? xB2 : ACCZ;
                end else begin
                    SA  <= SA  + xA2 - xAo;
                    SB  <= SB  + xB2 - xBo;
                    SC  <= SC  + xC2 - xCo;
                    SBi <= SBi + xBii - xBio;
                end
                dlA[dl_wp] <= A2;
                dlB[dl_wp] <= B2;
                dlC[dl_wp] <= C2;
                dl_wp      <= dl_wp + 5'd1;
            end
        end
        if (v2) begin
            r3  <= r2;
            d3  <= d2;
            SA3 <= frame_first ? xA2 : (SA + xA2 - xAo);
            SC3 <= frame_first ? xC2 : (SC + xC2 - xCo);
            tr3 <= (frame_first ? (xA2 + xB2 + xC2)
                                : ((SA + xA2 - xAo) + (SB + xB2 - xBo) + (SC + xC2 - xCo)))
                 - (frame_first ? ((tap_bi_in == 6'd0) ? xB2 : ACCZ)
                                : (SBi + xBii - xBio));
        end
    end

    // push_cnt and frame_first advance with stage 2 (the delay-line writer).
    always @(posedge clk) begin
        if (rst) begin
            push_cnt    <= 6'd0;
            frame_first <= 1'b1;
        end else if (v2) begin
            frame_first <= 1'b0;
            if (push_cnt != 6'd63) push_cnt <= push_cnt + 6'd1;
        end
    end

    //------------------------------------------------------------------------
    // Stage 4: pick the numerator the configured flavour wants, and the
    // matching reciprocal.
    //------------------------------------------------------------------------
    wire is_ca   = (q_kind == 2'd0) || (q_kind == 2'd3);
    wire ge      = (SA3 >= SC3);
    wire [ACCW-1:0] half_go = ge ? SA3 : SC3;
    wire [ACCW-1:0] half_so = ge ? SC3 : SA3;

    reg  [RECW-1:0] recip_f, recip_h;

    wire [ACCW-1:0] num4 = is_ca            ? tr3 :
                           (q_kind == 2'd1) ? half_go : half_so;
    wire [RECW-1:0] rec4 = is_ca ? recip_f : recip_h;

    reg        v4;
    reg [8:0]  r4;
    reg [9:0]  d4;
    reg [ACCW+RECW-1:0] prod4;

    always @(posedge clk) begin
        if (rst) v4 <= 1'b0;
        else     v4 <= v3;
        if (v3) begin
            r4    <= r3;
            d4    <= d3;
            prod4 <= num4 * rec4;
        end
    end

    //------------------------------------------------------------------------
    // Stage 5: noise = product >> 24, saturated to u32; threshold = alpha*noise
    //------------------------------------------------------------------------
    wire [ACCW+RECW-25:0] noise_wide = prod4[ACCW+RECW-1:24];
    wire [31:0] noise5 = (|noise_wide[ACCW+RECW-25:32]) ? 32'hFFFFFFFF
                                                        : noise_wide[31:0];

    reg        v5;
    reg [8:0]  r5;
    reg [9:0]  d5;
    reg [31:0] noise5_r;
    reg [63:0] thr5;

    always @(posedge clk) begin
        if (rst) v5 <= 1'b0;
        else     v5 <= v4;
        if (v4) begin
            r5       <= r4;
            d5       <= d4;
            noise5_r <= noise5;
            thr5     <= q_alpha * noise5;
        end
    end

    //------------------------------------------------------------------------
    // Stage 6: threshold, edge and zero-Doppler masks, emit.
    //
    // The cell under test trails the newest sample by Wr rows and Wd columns.
    //------------------------------------------------------------------------
    wire [8:0] cut_r = r5 - {4'd0, w_r};
    wire [9:0] cut_d = d5 - {5'd0, w_d};

    wire in_body = (r5 >= {4'd0, w_r} + {4'd0, w_r}) &&
                   (d5 >= {5'd0, w_d} + {5'd0, w_d});
    wire blanked = (cut_d <= {2'd0, q_zd}) ||
                   ((cut_d + {2'd0, q_zd}) >= q_nd);
    wire tested  = v5 && in_body && !blanked;

    wire [47:0] thr48  = thr5[63:16];
    wire        over   = ({16'd0, q_cut_d6} > thr48);

    // q_cut has to be delayed to line up with stage 5.
    reg [31:0] cut_p1, cut_p2, cut_p3, cut_p4, q_cut_d6;
    always @(posedge clk) begin
        cut_p1 <= q_cut;      // stage 1 output of the delay memory
        cut_p2 <= cut_p1;
        cut_p3 <= cut_p2;
        cut_p4 <= cut_p3;
        q_cut_d6 <= cut_p4;
    end

    wire hit_now = tested && ((q_kind == 2'd3) || over);

    reg [15:0] hit_count;
    reg [NUMW-1:0] noise_sum;
    reg [17:0] tested_cnt;

    always @(posedge clk) begin
        if (rst) begin
            hit_valid  <= 1'b0;
            hit_range  <= 8'd0;
            hit_dopp   <= 9'd0;
            hit_pwr    <= 32'd0;
            hit_count  <= 16'd0;
            noise_sum  <= {NUMW{1'b0}};
            tested_cnt <= 18'd0;
        end else begin
            hit_valid <= 1'b0;
            if (frame_clr) begin
                hit_count  <= 16'd0;
                noise_sum  <= {NUMW{1'b0}};
                tested_cnt <= 18'd0;
            end
            if (tested) begin
                noise_sum  <= noise_sum + {{(NUMW-32){1'b0}}, noise5_r};
                tested_cnt <= tested_cnt + 18'd1;
            end
            if (hit_now) begin
                if (hit_count != 16'hFFFF) hit_count <= hit_count + 16'd1;
                if (hit_count < q_maxhits) begin
                    hit_valid <= 1'b1;
                    hit_range <= cut_r[7:0];
                    hit_dopp  <= cut_d[8:0];
                    hit_pwr   <= q_cut_d6;
                end
            end
        end
    end

    //------------------------------------------------------------------------
    // Configuration capture, and the end-of-frame handshake.
    //------------------------------------------------------------------------
    reg        last_seen;      // in_last has arrived, pipeline draining
    reg [3:0]  drain;
    reg        frame_clr;      // clear the per-frame accumulators
    reg        need_recip;
    reg        need_mean;

    wire cfg_changed = (q_gr != g_r) || (q_gd != g_d) || (q_tr != t_r) ||
                       (q_td != t_d) || (q_kind != cfg_kind) ||
                       (q_nd != n_doppler) || (q_nr != n_range);

    always @(posedge clk) begin
        if (rst) begin
            q_gr <= 4'd2; q_gd <= 4'd2; q_tr <= 4'd4; q_td <= 4'd4;
            q_kind <= 2'd0; q_alpha <= 32'd65536; q_zd <= 8'd0;
            q_maxhits <= 16'd0; q_nr <= 9'd256; q_nd <= 10'd256;
            cut_delay <= {CUTAW{1'b0}};
            need_recip <= 1'b1;
            r_cnt <= 9'd0; d_cnt <= 10'd0; wr_slot <= 6'd0;
            cut_wp <= {CUTAW{1'b0}};
            last_seen <= 1'b0; drain <= 4'd0; frame_clr <= 1'b0;
            need_mean <= 1'b0;
        end else begin
            frame_clr <= 1'b0;
            if (take) begin
                cut_wp <= cut_wp + {{(CUTAW-1){1'b0}}, 1'b1};
                if (in_last) begin
                    r_cnt   <= 9'd0;
                    d_cnt   <= 10'd0;
                    wr_slot <= 6'd0;
                    // Latch the configuration for the frame that starts next.
                    q_gr <= g_r; q_gd <= g_d; q_tr <= t_r; q_td <= t_d;
                    q_kind <= cfg_kind; q_alpha <= cfg_alpha;
                    q_zd <= cfg_zero_dopp; q_maxhits <= cfg_max_hits;
                    q_nr <= n_range; q_nd <= n_doppler;
                    cut_delay <= (w_r * n_doppler) + {{(CUTAW-5){1'b0}}, w_d};
                    if (cfg_changed) need_recip <= 1'b1;
                    last_seen <= 1'b1;
                    drain     <= 4'd0;
                end else if (row_end) begin
                    d_cnt   <= 10'd0;
                    r_cnt   <= r_cnt + 9'd1;
                    wr_slot <= (wr_slot == (l_act - 6'd1)) ? 6'd0 : (wr_slot + 6'd1);
                end else begin
                    d_cnt <= d_cnt + 10'd1;
                end
            end

            // Let the pipeline empty, then ask for the frame's mean noise.
            if (last_seen) begin
                if (drain == 4'd10) begin
                    last_seen <= 1'b0;
                    need_mean <= 1'b1;
                end else begin
                    drain <= drain + 4'd1;
                end
            end
            if (mean_done) begin
                need_mean <= 1'b0;
                frame_clr <= 1'b1;
            end
            if (recip_done) need_recip <= 1'b0;
        end
    end

    //------------------------------------------------------------------------
    // One small sequential divider, shared by three jobs: the two reciprocals
    // and the frame's mean noise.  It runs between frames or during the long
    // untestable margin at the start of one, never in the datapath.
    //------------------------------------------------------------------------
    localparam [1:0] JOB_RF = 2'd0, JOB_RH = 2'd1, JOB_MEAN = 2'd2;

    reg        div_busy;
    reg [1:0]  div_job;
    reg [NUMW-1:0] div_num, div_quo;
    reg [31:0] div_den;
    reg [32:0] div_rem;
    reg [6:0]  div_i;
    reg        recip_done, mean_done;
    reg        rf_done;

    wire [32:0] rem_sh  = {div_rem[31:0], div_num[NUMW-1]};
    wire        rem_ge  = (rem_sh >= {1'b0, div_den});
    wire [32:0] rem_nxt = rem_ge ? (rem_sh - {1'b0, div_den}) : rem_sh;

    // round(2**24 / n) = (2**24 + n/2) / n
    wire [NUMW-1:0] num_rf = {{(NUMW-25){1'b0}}, 1'b1, 24'd0} +
                             {{(NUMW-12){1'b0}}, n_ref_f} ;
    wire [NUMW-1:0] num_rh = {{(NUMW-25){1'b0}}, 1'b1, 24'd0} +
                             {{(NUMW-12){1'b0}}, n_ref_h} ;

    always @(posedge clk) begin
        if (rst) begin
            div_busy   <= 1'b0;
            div_job    <= JOB_RF;
            div_num    <= {NUMW{1'b0}};
            div_quo    <= {NUMW{1'b0}};
            div_den    <= 32'd1;
            div_rem    <= 33'd0;
            div_i      <= 7'd0;
            recip_done <= 1'b0;
            mean_done  <= 1'b0;
            rf_done    <= 1'b0;
            recip_f    <= {RECW{1'b0}};
            recip_h    <= {RECW{1'b0}};
            noise_out  <= 32'd0;
            n_hits     <= 16'd0;
            frame_done <= 1'b0;
        end else begin
            recip_done <= 1'b0;
            mean_done  <= 1'b0;
            frame_done <= 1'b0;

            if (!div_busy) begin
                if (need_recip && !rf_done) begin
                    div_busy <= 1'b1; div_job <= JOB_RF;
                    div_num  <= {num_rf[NUMW-2:0], 1'b0};
                    div_den  <= (n_ref_f == 12'd0) ? 32'd1 : {20'd0, n_ref_f};
                    div_rem  <= {32'd0, num_rf[NUMW-1]};
                    div_quo  <= {NUMW{1'b0}};
                    div_i    <= 7'd0;
                end else if (need_recip && rf_done) begin
                    div_busy <= 1'b1; div_job <= JOB_RH;
                    div_num  <= {num_rh[NUMW-2:0], 1'b0};
                    div_den  <= (n_ref_h == 12'd0) ? 32'd1 : {20'd0, n_ref_h};
                    div_rem  <= {32'd0, num_rh[NUMW-1]};
                    div_quo  <= {NUMW{1'b0}};
                    div_i    <= 7'd0;
                end else if (need_mean) begin
                    div_busy <= 1'b1; div_job <= JOB_MEAN;
                    div_num  <= {noise_sum[NUMW-2:0], 1'b0};
                    div_den  <= (tested_cnt == 18'd0) ? 32'd1 : {14'd0, tested_cnt};
                    div_rem  <= {32'd0, noise_sum[NUMW-1]};
                    div_quo  <= {NUMW{1'b0}};
                    div_i    <= 7'd0;
                end
            end else begin
                div_rem <= rem_nxt;
                div_quo <= {div_quo[NUMW-2:0], rem_ge};
                div_num <= {div_num[NUMW-2:0], 1'b0};
                if (div_i == NUMW[6:0]-7'd1) begin
                    div_busy <= 1'b0;
                    case (div_job)
                        JOB_RF: begin
                            recip_f <= (|div_quo[NUMW-2:RECW-1]) ? {RECW{1'b1}}
                                       : {div_quo[RECW-2:0], rem_ge};
                            rf_done <= 1'b1;
                        end
                        JOB_RH: begin
                            recip_h <= (|div_quo[NUMW-2:RECW-1]) ? {RECW{1'b1}}
                                       : {div_quo[RECW-2:0], rem_ge};
                            rf_done    <= 1'b0;
                            recip_done <= 1'b1;
                        end
                        default: begin
                            noise_out  <= (|div_quo[NUMW-2:31]) ? 32'hFFFFFFFF
                                          : {div_quo[30:0], rem_ge};
                            n_hits     <= hit_count;
                            mean_done  <= 1'b1;
                            frame_done <= 1'b1;
                        end
                    endcase
                end else begin
                    div_i <= div_i + 7'd1;
                end
            end
        end
    end

endmodule

`default_nettype wire
