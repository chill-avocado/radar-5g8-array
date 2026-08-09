//============================================================================
// radar_cfar2d.v -- streaming two-dimensional CFAR over the range-Doppler map
//
// PURPOSE
//   Decide, cell by cell, whether the integrated power at a (range, Doppler)
//   cell is bigger than its surroundings by enough to call it a target.
//   "Constant false alarm rate" means the threshold is not a fixed number: it
//   is measured from the neighbours of every cell, so a map that is noisy all
//   over and a map that is quiet all over produce the same small trickle of
//   false alarms.  Around each cell under test sits a ring of GUARD cells,
//   ignored because a real target spills into them, and outside that a ring of
//   TRAINING cells, averaged to give the local noise level.
//
// COST PER CELL IS CONSTANT -- TWO NESTED INCREMENTAL SUMS
//   Adding the window up at every cell would be 625 additions per cell at the
//   largest setting, 37 million per frame.  This does eleven, whatever the
//   window size, by never recomputing a sum it already holds.
//
//   First direction (range).  Three per-Doppler-column running sums are kept
//   over the rows in the line buffer:
//       A[d]  the train_r lagging training rows
//       B[d]  the 2*guard_r+1 guard rows, cell under test in the middle
//       C[d]  the train_r leading training rows
//   When a new row arrives every column moves on by one row: C takes in the
//   new sample and hands its oldest row to B, B hands its oldest to A, and A
//   drops the row that has fallen out of the window.  Three adds and three
//   subtracts per column per row, however tall the window is.
//
//   Second direction (Doppler).  Those column sums are then slid sideways:
//       SA, SB, SC   over the full 2*(guard_d+train_d)+1 columns
//       SBi          over the 2*guard_d+1 guard columns only
//   each kept by adding the column entering the window and subtracting the
//   column leaving it, taken from a short delay line of recent columns.
//
//   From those four numbers:
//       outer window  = SA + SB + SC
//       guard window  = SBi                 (inclusive of the cell under test)
//       training sum  = outer - guard       <- the specified definition
//       lagging half  = SA,  leading half   = SC   (the range split GO/SO use)
//
// THE THREE FLAVOURS
//   CA  cell averaging  noise = training sum / number of training cells.  The
//                       best estimator when the neighbourhood really is
//                       uniform, and the worst when it is not.
//   GO  greatest of     the greater of the leading and lagging range halves.
//                       Lifts the threshold at a clutter edge so the radar
//                       does not light up all along the boundary of a treeline.
//   SO  smallest of     the lesser of the two halves.  Holds the threshold
//                       down when a second target sits in the training cells,
//                       which is how two aircraft in formation stay resolved.
//   3   pass-through    every tested cell is reported; for dumping raw maps.
//   The GO/SO halves are the two range training bands.  The training cells
//   that sit beside the cell under test in Doppler belong to neither half --
//   they are neither leading nor lagging in range -- so they are used by CA
//   and left out of the GO/SO comparison.
//
// DIVISION WITHOUT A DIVIDER IN THE DATAPATH
//   The number of reference cells changes only when the configuration does, so
//   its reciprocal is worked out once by a small sequential divider and the
//   per-cell "division" becomes a multiply by round(2**24 / n_ref).  That
//   reciprocal is good to better than 4e-5 for every legal window size.  The
//   same divider produces the frame's mean noise for the packet header.
//
// FRAME BOUNDARIES COST NOTHING
//   There is no input ready signal: a sample must be accepted on every clock.
//   So the running sums are never cleared between frames.  Instead, for the
//   first rows of a frame every term that would reach back into the previous
//   frame is forced to zero, and each band sum is forced to zero until its
//   first real row arrives -- which flushes the stale content in the same pass.
//
// EDGES
//   A cell is tested only when the whole window fits inside the map: range
//   Wr..n_range-1-Wr and Doppler Wd..n_doppler-1-Wd, with Wr = guard_r+train_r
//   and Wd = guard_d+train_d.  Cells within cfg_zero_dopp of DC are suppressed
//   (cfg_zero_dopp = 0 suppresses Doppler bin 0 alone, and the blanking wraps
//   around, so it covers the negative-Doppler bins at the top of the map too).
//   Emission stops after cfg_max_hits, counting does not: n_hits is the true
//   number of detections in the frame.
//
// CONFIGURATION
//   The window, the flavour, alpha, the map size and the blanking are captured
//   whenever the pipeline is empty -- at power-up and in the gap between
//   frames -- and held for the whole of a frame, so a register write can never
//   change the rules half way through a map.
//
// LATENCY
//   The cell under test trails the newest sample by Wr rows plus Wd columns.
//   That is what a centred window costs and is not an implementation choice.
//   On top of that the pipeline is 6 clocks from a sample entering to that
//   cell's verdict on hit_valid.
//   frame_done pulses about 70 clocks after the last sample of a frame, once
//   the sequential divider has finished the frame's mean noise, so n_hits and
//   noise_out are final by the time it fires.
//   Throughput: one cell per clock, sustained, never stalls.
//
// RESOURCE ESTIMATE, XC7K325T
//   BRAM36: 13 + 13 for the two line-buffer copies (25 rows x 512 words x 32
//           bits each; two copies because three different rows must be read in
//           one clock and a block RAM has two ports),
//            8 for the delay memory that carries the cell under test forward,
//            3 for the A/B/C column sums (512 x 38 bits each)
//           = 37 tiles, 8% of the 445 on the device.
//   DSP48 : ~6 for training_sum x reciprocal, 4 for alpha x noise, 2 for the
//           small window-size products = ~12.
//   Logic : ~2600 LUT, ~2300 FF.
//============================================================================
`timescale 1ns / 1ps
`default_nettype none

module radar_cfar2d #(
    parameter integer N_RANGE   = 256,   // largest range extent supported
    parameter integer N_DOPPLER = 512,   // largest Doppler extent supported
    parameter integer MAX_GUARD = 4,
    parameter integer MAX_TRAIN = 8
) (
    input  wire        clk,
    input  wire        rst,

    // Map geometry for this frame.
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
    function integer clog2;
        input integer v;
        integer i;
        begin
            clog2 = 0;
            for (i = v - 1; i > 0; i = i >> 1) clog2 = clog2 + 1;
        end
    endfunction

    localparam integer LMAX    = 2*(MAX_GUARD+MAX_TRAIN) + 1;   // 25 rows
    localparam integer DSHIFT  = clog2(N_DOPPLER);              // 9
    localparam integer NDMAX   = 1 << DSHIFT;                   // 512
    localparam integer LBDEPTH = LMAX * NDMAX;                  // 12800 words
    localparam integer LBAW    = clog2(LBDEPTH);                // 14
    localparam integer CUTMAX  = (MAX_GUARD+MAX_TRAIN)*NDMAX + (MAX_GUARD+MAX_TRAIN);
    localparam integer CUTAW   = clog2(CUTMAX + 1);             // 13
    localparam integer TCW     = clog2(N_RANGE*NDMAX) + 1;      // tested-cell count
    localparam integer COLW    = 38;                            // column band sums
    localparam integer ACCW    = 48;                            // RADAR_CFAR_ACC_W
    localparam integer DLD     = 32;                            // Doppler delay line
    localparam integer RECW    = 26;                            // reciprocal, 24 frac
    localparam integer NUMW    = 56;                            // divider numerator
    localparam [6:0]   DIV_LAST = 7'd55;                        // NUMW-1

    localparam [COLW-1:0] COLZ = {COLW{1'b0}};
    localparam [ACCW-1:0] ACCZ = {ACCW{1'b0}};

    //------------------------------------------------------------------------
    // Clamp the window to what the buffers hold.  A silly register write must
    // not be able to address outside the line buffer.
    //------------------------------------------------------------------------
    wire [3:0] g_r = (cfg_guard_r > MAX_GUARD[3:0]) ? MAX_GUARD[3:0] : cfg_guard_r;
    wire [3:0] g_d = (cfg_guard_d > MAX_GUARD[3:0]) ? MAX_GUARD[3:0] : cfg_guard_d;
    wire [3:0] t_r = (cfg_train_r > MAX_TRAIN[3:0]) ? MAX_TRAIN[3:0] : cfg_train_r;
    wire [3:0] t_d = (cfg_train_d > MAX_TRAIN[3:0]) ? MAX_TRAIN[3:0] : cfg_train_d;

    //------------------------------------------------------------------------
    // Configuration held for the duration of a frame
    //------------------------------------------------------------------------
    reg  [3:0]  q_gr, q_gd, q_tr, q_td;
    reg  [1:0]  q_kind;
    reg  [31:0] q_alpha;
    reg  [7:0]  q_zd;
    reg  [15:0] q_maxhits;
    reg  [8:0]  q_nr;
    reg  [9:0]  q_nd;
    reg  [CUTAW-1:0] cut_delay;                     // Wr*n_doppler + Wd

    wire [4:0] w_r     = {1'b0, q_gr} + {1'b0, q_tr};          // Wr <= 12
    wire [4:0] w_d     = {1'b0, q_gd} + {1'b0, q_td};          // Wd <= 12
    wire [5:0] l_act   = {1'b0, w_r} + {1'b0, w_r} + 6'd1;     // 2*Wr+1 <= 25
    wire [5:0] wd_span = {1'b0, w_d} + {1'b0, w_d} + 6'd1;     // 2*Wd+1 <= 25
    wire [5:0] gr_span = {2'd0, q_gr} + {2'd0, q_gr} + 6'd1;
    wire [5:0] gd_span = {2'd0, q_gd} + {2'd0, q_gd} + 6'd1;

    // Row offsets from the newest row of the two band boundaries.
    wire [5:0] off_bc = {2'd0, q_tr};                                        // C->B
    wire [5:0] off_ab = {2'd0, q_tr} + {2'd0, q_gr} + {2'd0, q_gr} + 6'd1;   // B->A

    // Reference-cell counts.  Tiny products, one set per frame.
    wire [11:0] wr12    = {6'd0, l_act};
    wire [11:0] wd12    = {6'd0, wd_span};
    wire [11:0] gr12    = {6'd0, gr_span};
    wire [11:0] gd12    = {6'd0, gd_span};
    wire [11:0] tr12    = {8'd0, q_tr};
    wire [11:0] n_outer = wr12 * wd12;                    // <= 625
    wire [11:0] n_guard = gr12 * gd12;                    // <= 81
    wire [11:0] n_ref_f = n_outer - n_guard;              // CA training cells
    wire [11:0] n_ref_h = tr12 * wd12;                    // one GO/SO half

    //------------------------------------------------------------------------
    // Frame sequencing.  No backpressure: a sample is taken every clock it is
    // offered.
    //------------------------------------------------------------------------
    reg [8:0]  r_cnt;
    reg [9:0]  d_cnt;
    reg [5:0]  wr_slot;
    reg [CUTAW-1:0] cut_wp;

    wire take    = in_valid;
    wire row_end = (d_cnt == (q_nd - 10'd1));

    //------------------------------------------------------------------------
    // Line buffer.  Two identical copies: copy 0's write port also returns the
    // row falling out of the window (read before write), copy 0's second port
    // reads the C->B boundary row and copy 1's second port the B->A boundary
    // row.  Three rows in one clock needs more ports than one block RAM has.
    //------------------------------------------------------------------------
    reg [31:0] lb0 [LBDEPTH-1:0];
    reg [31:0] lb1 [LBDEPTH-1:0];

    // slot(offset) = (wr_slot - offset) mod l_act
    function [4:0] slot_of;
        input [5:0] off;
        reg   [5:0] t;
        begin
            t = (wr_slot >= off) ? (wr_slot - off) : (wr_slot + l_act - off);
            slot_of = t[4:0];
        end
    endfunction

    wire [LBAW-1:0] lb_addr_w  = {wr_slot[4:0],       d_cnt[DSHIFT-1:0]};
    wire [LBAW-1:0] lb_addr_bc = {slot_of(off_bc),    d_cnt[DSHIFT-1:0]};
    wire [LBAW-1:0] lb_addr_ab = {slot_of(off_ab),    d_cnt[DSHIFT-1:0]};

    reg [31:0] q_leave;   // the row that has just fallen out of the window
    reg [31:0] q_bc;      // the row crossing from C into B
    reg [31:0] q_ab;      // the row crossing from B into A

    always @(posedge clk) begin
        if (take) begin
            q_leave        <= lb0[lb_addr_w];   // read before write: the old row
            lb0[lb_addr_w] <= in_pwr;
            lb1[lb_addr_w] <= in_pwr;
            q_bc           <= lb0[lb_addr_bc];
            q_ab           <= lb1[lb_addr_ab];
        end
    end

    //------------------------------------------------------------------------
    // Delay memory.  Carries the cell under test forward so its own power
    // arrives on the same clock as the statistics gathered around it.
    //------------------------------------------------------------------------
    reg  [31:0]      cutmem [(1<<CUTAW)-1:0];
    wire [CUTAW-1:0] cut_ra = cut_wp - cut_delay;
    reg  [31:0]      q_cut;

    always @(posedge clk) begin
        if (take) begin
            q_cut          <= cutmem[cut_ra];
            cutmem[cut_wp] <= in_pwr;
        end
    end

    //------------------------------------------------------------------------
    // Column band sums.  Read at stage 0, written back at stage 2; the same
    // column is not revisited for n_doppler clocks, so there is no hazard.
    //------------------------------------------------------------------------
    reg [COLW-1:0] colA [NDMAX-1:0];
    reg [COLW-1:0] colB [NDMAX-1:0];
    reg [COLW-1:0] colC [NDMAX-1:0];
    reg [COLW-1:0] qA, qB, qC;

    always @(posedge clk) begin
        if (take) begin
            qA <= colA[d_cnt[DSHIFT-1:0]];
            qB <= colB[d_cnt[DSHIFT-1:0]];
            qC <= colC[d_cnt[DSHIFT-1:0]];
        end
    end

    //------------------------------------------------------------------------
    // Stage 1 -- memory results are ready
    //------------------------------------------------------------------------
    reg        v1, lst1;
    reg [31:0] pwr1;
    reg [8:0]  r1;
    reg [9:0]  d1;

    always @(posedge clk) begin
        if (rst) v1 <= 1'b0;
        else     v1 <= take;
        if (take) begin
            pwr1 <= in_pwr;
            r1   <= r_cnt;
            d1   <= d_cnt;
            lst1 <= in_last;
        end
    end

    //------------------------------------------------------------------------
    // Stage 2 -- advance the three column band sums.
    //
    // Any term reaching back before the start of the frame is forced to zero,
    // and a band is held at zero until its first real row arrives.  That
    // flushes the previous frame's residue without a single flush cycle.
    //------------------------------------------------------------------------
    wire have_leave = (r1 >= {3'd0, l_act});
    wire have_ab    = (r1 >= {3'd0, off_ab});
    wire have_bc    = (r1 >= {3'd0, off_bc});
    wire tr_zero    = (q_tr == 4'd0);

    wire [COLW-1:0] e_leave = have_leave ? {{(COLW-32){1'b0}}, q_leave} : COLZ;
    wire [COLW-1:0] e_ab    = have_ab    ? {{(COLW-32){1'b0}}, q_ab   } : COLZ;
    wire [COLW-1:0] e_bc    = have_bc    ? {{(COLW-32){1'b0}}, q_bc   } : COLZ;
    wire [COLW-1:0] e_new   = {{(COLW-32){1'b0}}, pwr1};

    wire [COLW-1:0] A_new = (tr_zero || !have_ab) ? COLZ : (qA + e_ab - e_leave);
    wire [COLW-1:0] B_new = (!have_bc)            ? COLZ : (qB + e_bc - e_ab);
    wire [COLW-1:0] C_new = tr_zero      ? COLZ :
                            (r1 == 9'd0) ? e_new : (qC + e_new - e_bc);

    reg        v2, lst2;
    reg [8:0]  r2;
    reg [9:0]  d2;
    reg [COLW-1:0] A2, B2, C2;

    always @(posedge clk) begin
        if (rst) v2 <= 1'b0;
        else     v2 <= v1;
        if (v1) begin
            r2   <= r1;
            d2   <= d1;
            lst2 <= lst1;
            A2   <= A_new;
            B2   <= B_new;
            C2   <= C_new;
            colA[d1[DSHIFT-1:0]] <= A_new;
            colB[d1[DSHIFT-1:0]] <= B_new;
            colC[d1[DSHIFT-1:0]] <= C_new;
        end
    end

    //------------------------------------------------------------------------
    // Stage 3 -- slide the four Doppler sums
    //------------------------------------------------------------------------
    reg [COLW-1:0] dlA [DLD-1:0];
    reg [COLW-1:0] dlB [DLD-1:0];
    reg [COLW-1:0] dlC [DLD-1:0];
    reg [4:0]      dl_wp;
    reg [5:0]      push_cnt;
    reg            frame_first;

    wire [5:0] tap_out   = wd_span;                                  // 2*Wd+1
    wire [5:0] tap_bi_in = {2'd0, q_td};                             // train_d
    wire [5:0] tap_bi_o  = {1'b0, w_d} + {2'd0, q_gd} + 6'd1;        // Wd+guard_d+1

    wire [4:0] ia_out = dl_wp - tap_out[4:0];
    wire [4:0] ib_in  = dl_wp - tap_bi_in[4:0];
    wire [4:0] ib_out = dl_wp - tap_bi_o[4:0];

    wire sub_out = (push_cnt >= tap_out);
    wire add_bi  = (push_cnt >= tap_bi_in);
    wire sub_bi  = (push_cnt >= tap_bi_o);

    reg [ACCW-1:0] SA, SB, SC, SBi;

    wire [ACCW-1:0] xA2  = {{(ACCW-COLW){1'b0}}, A2};
    wire [ACCW-1:0] xB2  = {{(ACCW-COLW){1'b0}}, B2};
    wire [ACCW-1:0] xC2  = {{(ACCW-COLW){1'b0}}, C2};
    wire [ACCW-1:0] xAo  = sub_out ? {{(ACCW-COLW){1'b0}}, dlA[ia_out]} : ACCZ;
    wire [ACCW-1:0] xBo  = sub_out ? {{(ACCW-COLW){1'b0}}, dlB[ia_out]} : ACCZ;
    wire [ACCW-1:0] xCo  = sub_out ? {{(ACCW-COLW){1'b0}}, dlC[ia_out]} : ACCZ;
    wire [ACCW-1:0] xBii = (tap_bi_in == 6'd0) ? xB2 :
                           (add_bi ? {{(ACCW-COLW){1'b0}}, dlB[ib_in]} : ACCZ);
    wire [ACCW-1:0] xBio = sub_bi ? {{(ACCW-COLW){1'b0}}, dlB[ib_out]} : ACCZ;

    wire [ACCW-1:0] SA_n  = frame_first ? xA2 : (SA  + xA2  - xAo);
    wire [ACCW-1:0] SB_n  = frame_first ? xB2 : (SB  + xB2  - xBo);
    wire [ACCW-1:0] SC_n  = frame_first ? xC2 : (SC  + xC2  - xCo);
    wire [ACCW-1:0] SBi_n = frame_first ? ((tap_bi_in == 6'd0) ? xB2 : ACCZ)
                                        : (SBi + xBii - xBio);

    reg        v3, lst3;
    reg [8:0]  r3;
    reg [9:0]  d3;
    reg [ACCW-1:0] SA3, SC3, tr3;

    always @(posedge clk) begin
        if (rst) begin
            v3          <= 1'b0;
            SA          <= ACCZ;
            SB          <= ACCZ;
            SC          <= ACCZ;
            SBi         <= ACCZ;
            dl_wp       <= 5'd0;
            push_cnt    <= 6'd0;
            frame_first <= 1'b1;
        end else begin
            v3 <= v2;
            if (v2) begin
                SA         <= SA_n;
                SB         <= SB_n;
                SC         <= SC_n;
                SBi        <= SBi_n;
                dlA[dl_wp] <= A2;
                dlB[dl_wp] <= B2;
                dlC[dl_wp] <= C2;
                dl_wp      <= dl_wp + 5'd1;
                push_cnt   <= frame_first ? 6'd1 :
                              ((push_cnt == 6'd63) ? 6'd63 : (push_cnt + 6'd1));
                frame_first <= lst2;
            end
        end
        if (v2) begin
            r3   <= r2;
            d3   <= d2;
            lst3 <= lst2;
            SA3  <= SA_n;
            SC3  <= SC_n;
            tr3  <= (SA_n + SB_n + SC_n) - SBi_n;
        end
    end

    //------------------------------------------------------------------------
    // Stage 4 -- pick the numerator the configured flavour wants and multiply
    // by the matching reciprocal
    //------------------------------------------------------------------------
    wire is_ca = (q_kind == 2'd0) || (q_kind == 2'd3);
    wire a_ge  = (SA3 >= SC3);

    reg [RECW-1:0] recip_f, recip_h;

    wire [ACCW-1:0] num4 = is_ca            ? tr3 :
                           (q_kind == 2'd1) ? (a_ge ? SA3 : SC3)
                                            : (a_ge ? SC3 : SA3);
    wire [RECW-1:0] rec4 = is_ca ? recip_f : recip_h;

    /* verilator lint_off UNUSEDSIGNAL */
    wire [ACCW+RECW-1:0] prod_full = num4 * rec4;  // low 24 bits are the
                                                   // fraction and are dropped
    /* verilator lint_on UNUSEDSIGNAL */

    reg        v4, lst4;
    reg [8:0]  r4;
    reg [9:0]  d4;
    reg [ACCW+RECW-25:0] prod4;

    always @(posedge clk) begin
        if (rst) v4 <= 1'b0;
        else     v4 <= v3;
        if (v3) begin
            r4    <= r3;
            d4    <= d3;
            lst4  <= lst3;
            prod4 <= prod_full[ACCW+RECW-1:24];
        end
    end

    //------------------------------------------------------------------------
    // Stage 5 -- noise, saturated to u32, and threshold = (alpha*noise) >> 16
    //------------------------------------------------------------------------
    wire [31:0] noise5 = (|prod4[ACCW+RECW-25:32]) ? 32'hFFFFFFFF : prod4[31:0];

    /* verilator lint_off UNUSEDSIGNAL */
    wire [63:0] thr_full = q_alpha * noise5;       // Q16.16 * u32, low 16 bits
                                                   // are the fraction
    /* verilator lint_on UNUSEDSIGNAL */

    reg        v5, lst5;
    reg [8:0]  r5;
    reg [9:0]  d5;
    reg [31:0] noise5_r;
    reg [47:0] thr5;

    always @(posedge clk) begin
        if (rst) v5 <= 1'b0;
        else     v5 <= v4;
        if (v4) begin
            r5       <= r4;
            d5       <= d4;
            lst5     <= lst4;
            noise5_r <= noise5;
            thr5     <= thr_full[63:16];
        end
    end

    // The cell under test's own power, walked down beside the pipeline.
    reg [31:0] cut_p1, cut_p2, cut_p3, cut_p4;
    always @(posedge clk) begin
        if (v1) cut_p1 <= q_cut;
        if (v2) cut_p2 <= cut_p1;
        if (v3) cut_p3 <= cut_p2;
        if (v4) cut_p4 <= cut_p3;
    end

    //------------------------------------------------------------------------
    // Stage 6 -- threshold, edge and zero-Doppler masks, emit
    //------------------------------------------------------------------------
    wire [7:0] cut_r = r5[7:0] - {3'd0, w_r};
    wire [9:0] cut_d = d5 - {5'd0, w_d};

    wire in_body = (r5 >= ({4'd0, w_r} + {4'd0, w_r})) &&
                   (d5 >= ({5'd0, w_d} + {5'd0, w_d}));
    wire blanked = (cut_d <= {2'd0, q_zd}) ||
                   ((cut_d + {2'd0, q_zd}) >= q_nd);
    wire tested  = v5 && in_body && !blanked;
    wire over    = ({16'd0, cut_p4} > thr5);
    wire hit_now = tested && ((q_kind == 2'd3) || over);

    reg [15:0]     hit_count;
    reg [NUMW-1:0] noise_sum;
    reg [TCW-1:0]  tested_cnt;
    reg            frame_clr;

    always @(posedge clk) begin
        if (rst) begin
            hit_valid  <= 1'b0;
            hit_range  <= 8'd0;
            hit_dopp   <= 9'd0;
            hit_pwr    <= 32'd0;
            hit_count  <= 16'd0;
            noise_sum  <= {NUMW{1'b0}};
            tested_cnt <= {TCW{1'b0}};
        end else begin
            hit_valid <= 1'b0;
            if (frame_clr) begin
                hit_count  <= 16'd0;
                noise_sum  <= {NUMW{1'b0}};
                tested_cnt <= {TCW{1'b0}};
            end else begin
                if (tested) begin
                    noise_sum  <= noise_sum + {{(NUMW-32){1'b0}}, noise5_r};
                    tested_cnt <= tested_cnt + {{(TCW-1){1'b0}}, 1'b1};
                end
                if (hit_now && (hit_count != 16'hFFFF))
                    hit_count <= hit_count + 16'd1;
            end
            if (hit_now && (hit_count < q_maxhits)) begin
                hit_valid <= 1'b1;
                hit_range <= cut_r;
                hit_dopp  <= cut_d[8:0];
                hit_pwr   <= cut_p4;
            end
        end
    end

    //------------------------------------------------------------------------
    // Configuration capture and the end-of-frame handshake.
    //
    // The configuration is only ever taken on board while the pipeline is
    // empty, so no frame is ever judged by two different sets of rules.
    //------------------------------------------------------------------------
    reg       last_seen;
    reg [3:0] drain;
    reg       need_recip, need_mean;
    reg       recip_done, mean_done;

    wire pipe_busy = take | v1 | v2 | v3 | v4 | v5 | last_seen;

    wire cfg_changed = (q_gr != g_r) || (q_gd != g_d) || (q_tr != t_r) ||
                       (q_td != t_d) || (q_nd != n_doppler);

    always @(posedge clk) begin
        if (rst) begin
            q_gr <= 4'd2; q_gd <= 4'd2; q_tr <= 4'd4; q_td <= 4'd4;
            q_kind <= 2'd0; q_alpha <= 32'd65536; q_zd <= 8'd0;
            q_maxhits <= 16'd0; q_nr <= 9'd256; q_nd <= 10'd256;
            cut_delay  <= {CUTAW{1'b0}};
            need_recip <= 1'b1;
            r_cnt      <= 9'd0;
            d_cnt      <= 10'd0;
            wr_slot    <= 6'd0;
            cut_wp     <= {CUTAW{1'b0}};
            last_seen  <= 1'b0;
            drain      <= 4'd0;
            frame_clr  <= 1'b0;
            need_mean  <= 1'b0;
        end else begin
            frame_clr <= 1'b0;

            if (!pipe_busy) begin
                q_gr <= g_r; q_gd <= g_d; q_tr <= t_r; q_td <= t_d;
                q_kind    <= cfg_kind;
                q_alpha   <= cfg_alpha;
                q_zd      <= cfg_zero_dopp;
                q_maxhits <= cfg_max_hits;
                q_nr      <= n_range;
                q_nd      <= n_doppler;
                cut_delay <= ({{(CUTAW-5){1'b0}}, w_r} * {{(CUTAW-10){1'b0}}, n_doppler})
                           + {{(CUTAW-5){1'b0}}, w_d};
                if (cfg_changed) need_recip <= 1'b1;
            end

            if (take) begin
                cut_wp <= cut_wp + {{(CUTAW-1){1'b0}}, 1'b1};
                if (in_last) begin
                    r_cnt     <= 9'd0;
                    d_cnt     <= 10'd0;
                    wr_slot   <= 6'd0;
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
                if (drain == 4'd12) begin
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
    // One small restoring divider shared by three jobs: the two reciprocals
    // and the frame's mean noise.  It runs between frames or inside the long
    // untestable margin at the start of one, never in the datapath.
    //------------------------------------------------------------------------
    localparam [1:0] JOB_RF = 2'd0, JOB_RH = 2'd1, JOB_MEAN = 2'd2;

    reg            div_busy, div_fin, rf_done;
    reg [1:0]      div_job;
    reg [NUMW-1:0] div_num, div_quo;
    reg [31:0]     div_den, div_rem;
    reg [6:0]      div_i;

    wire [32:0] rem_sh  = {div_rem, div_num[NUMW-1]};
    wire        rem_ge  = (rem_sh >= {1'b0, div_den});
    wire [31:0] rem_dif = rem_sh[31:0] - div_den;   // exact: rem always < den

    // round(2**24 / n) = (2**24 + n/2) / n, and n/2 rounds down to n>>1
    wire [NUMW-1:0] num_rf = {{(NUMW-25){1'b0}}, 1'b1, 24'd0}
                           + {{(NUMW-11){1'b0}}, n_ref_f[11:1]};
    wire [NUMW-1:0] num_rh = {{(NUMW-25){1'b0}}, 1'b1, 24'd0}
                           + {{(NUMW-11){1'b0}}, n_ref_h[11:1]};

    always @(posedge clk) begin
        if (rst) begin
            div_busy   <= 1'b0;
            div_fin    <= 1'b0;
            rf_done    <= 1'b0;
            div_job    <= JOB_RF;
            div_num    <= {NUMW{1'b0}};
            div_quo    <= {NUMW{1'b0}};
            div_den    <= 32'd1;
            div_rem    <= 32'd0;
            div_i      <= 7'd0;
            recip_done <= 1'b0;
            mean_done  <= 1'b0;
            recip_f    <= {RECW{1'b0}};
            recip_h    <= {RECW{1'b0}};
            noise_out  <= 32'd0;
            n_hits     <= 16'd0;
            frame_done <= 1'b0;
        end else begin
            recip_done <= 1'b0;
            mean_done  <= 1'b0;
            frame_done <= 1'b0;

            if (div_fin) begin
                div_fin <= 1'b0;
                case (div_job)
                    JOB_RF: begin
                        recip_f <= (|div_quo[NUMW-1:RECW]) ? {RECW{1'b1}}
                                                           : div_quo[RECW-1:0];
                        rf_done <= 1'b1;
                    end
                    JOB_RH: begin
                        recip_h <= (|div_quo[NUMW-1:RECW]) ? {RECW{1'b1}}
                                                           : div_quo[RECW-1:0];
                        rf_done    <= 1'b0;
                        recip_done <= 1'b1;
                    end
                    default: begin
                        noise_out  <= (|div_quo[NUMW-1:32]) ? 32'hFFFFFFFF
                                                            : div_quo[31:0];
                        n_hits     <= hit_count;
                        mean_done  <= 1'b1;
                        frame_done <= 1'b1;
                    end
                endcase
            end else if (!div_busy) begin
                div_quo <= {NUMW{1'b0}};
                div_rem <= 32'd0;
                div_i   <= 7'd0;
                if (need_recip && !rf_done) begin
                    div_busy <= 1'b1;
                    div_job  <= JOB_RF;
                    div_num  <= num_rf;
                    div_den  <= (n_ref_f == 12'd0) ? 32'd1 : {20'd0, n_ref_f};
                end else if (need_recip && rf_done) begin
                    div_busy <= 1'b1;
                    div_job  <= JOB_RH;
                    div_num  <= num_rh;
                    div_den  <= (n_ref_h == 12'd0) ? 32'd1 : {20'd0, n_ref_h};
                end else if (need_mean) begin
                    div_busy <= 1'b1;
                    div_job  <= JOB_MEAN;
                    div_num  <= noise_sum;
                    div_den  <= (tested_cnt == {TCW{1'b0}}) ? 32'd1
                                                            : {{(32-TCW){1'b0}}, tested_cnt};
                end
            end else begin
                div_rem <= rem_ge ? rem_dif : rem_sh[31:0];
                div_quo <= {div_quo[NUMW-2:0], rem_ge};
                div_num <= {div_num[NUMW-2:0], 1'b0};
                if (div_i == DIV_LAST) begin
                    div_busy <= 1'b0;
                    div_fin  <= 1'b1;
                end else begin
                    div_i <= div_i + 7'd1;
                end
            end
        end
    end

endmodule

`default_nettype wire
