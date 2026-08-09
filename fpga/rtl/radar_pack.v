//============================================================================
// radar_pack.v -- assemble one output frame, radar_pkg.svh section 7
//
// PURPOSE
//   Turn a finished range-Doppler map and its detection list into the stream
//   of 32-bit words the host expects, with a magic word at the front and an
//   end marker at the back so a dropped packet costs one frame and not the
//   stream.
//
// WHY THE MAP IS BUFFERED AND NOT STREAMED STRAIGHT THROUGH
//   Word 4 of the header carries the number of detections, and that number is
//   not known until the very last cell of the map has been through the CFAR.
//   A stream is emitted in order, so nothing at all can leave until the frame
//   is complete -- which means the map has to be held somewhere.  It is held
//   decimated, in a pair of buffers, so the map of the frame being sent out
//   and the map of the frame being built never share a word.  The detections
//   are held the same way, for the reason the map is: they appear during the
//   scan and belong after the map in the packet.
//
// MAP DECIMATION TAKES THE MAXIMUM, NOT A SAMPLE
//   Sending every cfg_map_decim_r-th range bin and every cfg_map_decim_d-th
//   Doppler bin would let a one-cell target fall down the gap and vanish from
//   the operator's display.  Instead each output cell is the LARGEST of the
//   block of cells it stands for, so a target survives decimation with its
//   peak intact and only its position is coarsened.  The block maximum is
//   built up incrementally -- one register carries the run along Doppler and a
//   small row accumulator carries the run down range -- so it costs a single
//   comparison per input cell whatever the decimation factor, and each output
//   cell is written to the map buffer exactly once, on the last input row of
//   its block.  n_range_out and n_doppler_out fall out of the block counters,
//   so no division is needed to fill in header word 3.
//
// FIELD PLACEMENT DECISIONS
//   radar_pkg.svh gives explicit bit positions for header word 1 and for the
//   detection record, and both list the most significant field first.  The
//   two words that give no positions follow the same rule:
//       word 3   [31:16] n_range_out    [15:0] n_doppler_out
//       word 4   [31:16] n_hits         [15:0] reserved (zero)
//       word 6   timestamp[63:32]       word 7  timestamp[31:0]
//   The Doppler bin field of a detection record is eight bits wide, which is
//   one short of the 512 bins the fine-Doppler operating point produces.  The
//   ninth bit goes in bit 16 of the same word, the lowest bit of the byte the
//   contract marks reserved, so a map of 256 Doppler bins is bit-identical to
//   what it was before and bits 23:17 stay zero:
//       +0  [31:24] flags  [23:17] zero  [16] doppler[8]
//           [15:8] doppler[7:0]  [7:0] range
//   Header word 4 reports the number of detection RECORDS actually in this
//   packet, so the host can always parse the packet from the header alone.
//   Upstream capping by cfg_max_hits is what limits that number.
//   Flag bits 0 and 1 are driven from what the packet really contains rather
//   than copied from cfg_flags, so they can never disagree with the payload.
//
// LATENCY
//   Emission starts 2 clocks after frame_end and runs at one word per clock
//   with full backpressure; a whole packet is
//   8 + n_range_out*n_doppler_out + 6*n_hits + 1 words.
//   Memory reads are covered by a four-deep output queue, so m_ready going
//   low never loses a word and never costs throughput once it returns.
//   If a frame ends while the previous packet is still going out, that frame's
//   packet is skipped rather than corrupted; at 62.5 maps a second and a
//   16 384-word ceiling on the map there are more than ten times the clocks
//   needed, so this cannot happen in normal operation.
//
// RESOURCE ESTIMATE, XC7K325T
//   BRAM36: 32 for the two map buffers (16384 words x 32 bits each -- 16384
//           words is a 2x2 decimation of the 65536-cell map the corner turn
//           fixes, and the finest decimation worth buffering on this device),
//           12 for the two detection buffers (1024 records x 192 bits each)
//           = 44 tiles, 10% of the 445 on the device.
//   DSP48 : 0.  Logic: ~700 LUT, ~600 FF.
//============================================================================
`timescale 1ns / 1ps
`default_nettype none

module radar_pack #(
    parameter integer MAP_WORDS = 16384,   // decimated map words per buffer
    parameter integer MAX_HITS  = 1024     // detection records per buffer
) (
    input  wire        clk,
    input  wire        rst,

    // Per-frame configuration, captured at frame_start.
    input  wire        cfg_map_enable,
    input  wire        cfg_hits_enable,
    input  wire [7:0]  cfg_map_decim_r,
    input  wire [7:0]  cfg_map_decim_d,
    input  wire [15:0] cfg_flags,
    input  wire [31:0] frame_index,
    input  wire [63:0] timestamp,
    input  wire [31:0] noise,
    input  wire [8:0]  n_range,
    input  wire [9:0]  n_doppler,

    // Integrated power map, range major, one cell per clock.
    input  wire        map_valid,
    input  wire [31:0] map_pwr,

    // Detections, as they come out of the CFAR during the map scan.
    input  wire        hit_valid,
    input  wire [7:0]  hit_range,
    input  wire [8:0]  hit_dopp,
    input  wire [31:0] hit_pwr,
    input  wire signed [15:0] hit_v0_i, hit_v0_q,
    input  wire signed [15:0] hit_v1_i, hit_v1_q,
    input  wire signed [15:0] hit_v2_i, hit_v2_q,
    input  wire signed [15:0] hit_v3_i, hit_v3_q,

    input  wire [15:0] n_hits,
    input  wire        frame_start,
    input  wire        frame_end,

    // Packed frame out.
    output wire        m_valid,
    output wire [31:0] m_data,
    input  wire        m_ready,
    output wire        m_last
);

    //------------------------------------------------------------------------
    // radar_pkg.svh section 7 constants
    //------------------------------------------------------------------------
    localparam [31:0] RADAR_MAGIC   = 32'h52414452;   // "RADR"
    localparam [31:0] RADAR_ENDMARK = 32'h454E4452;   // "ENDR"
    localparam [15:0] RADAR_FMT_VER = 16'd1;

    function integer clog2;
        input integer v;
        integer i;
        begin
            clog2 = 0;
            for (i = v - 1; i > 0; i = i >> 1) clog2 = clog2 + 1;
        end
    endfunction

    localparam integer MAPAW = clog2(MAP_WORDS);
    localparam integer HITAW = clog2(MAX_HITS);
    localparam integer HITW  = 192;                   // six words per record

    //------------------------------------------------------------------------
    // Configuration held for the frame being built
    //------------------------------------------------------------------------
    reg         q_map_en, q_hit_en;
    reg  [7:0]  q_dec_r, q_dec_d;
    reg  [15:0] q_flags;
    reg  [31:0] q_index;
    reg  [63:0] q_time;
    reg  [8:0]  q_nr;
    reg  [9:0]  q_nd;

    wire [7:0]  dec_r = (q_dec_r == 8'd0) ? 8'd1 : q_dec_r;
    wire [7:0]  dec_d = (q_dec_d == 8'd0) ? 8'd1 : q_dec_d;

    localparam [31:0] MAPW32 = MAP_WORDS;
    localparam [31:0] MAXH32 = MAX_HITS;
    wire [MAPAW:0] MAPW_LIM = MAPW32[MAPAW:0];
    wire [HITAW:0] MAXH_LIM = MAXH32[HITAW:0];

    //------------------------------------------------------------------------
    // Buffers.  Each array has exactly one write port and one read port, which
    // is the simple dual-port shape Vivado turns into true block RAM.  The
    // write side always addresses buffer wsel and the read side buffer rsel,
    // and the two are never the same once a frame has been handed over.
    //------------------------------------------------------------------------
    reg [31:0]     mapbuf [(2*MAP_WORDS)-1:0];
    reg [HITW-1:0] hitbuf [(2*MAX_HITS)-1:0];
    reg [31:0]     rowacc [511:0];      // running max down range, per column
    reg            wsel, rsel;

    //------------------------------------------------------------------------
    // MAP WRITER
    //------------------------------------------------------------------------
    reg [8:0]       mr;          // input range row
    reg [9:0]       md;          // input Doppler column
    reg [7:0]       cnt_r;       // position within the range block
    reg [7:0]       cnt_d;       // position within the Doppler block
    reg [9:0]       od;          // output Doppler index
    reg [MAPAW-1:0] row_base;    // first word of the current output row
    reg [31:0]      blk_max;     // running maximum along Doppler
    reg [31:0]      rowacc_q;
    reg [9:0]       nd_out;
    reg [8:0]       nr_out;
    reg             map_ovf;

    wire row_last  = (md == (q_nd - 10'd1));
    wire col_last  = (mr == (q_nr - 9'd1));
    wire d_blk_end = (cnt_d == (dec_d - 8'd1)) || row_last;
    wire r_blk_end = (cnt_r == (dec_r - 8'd1)) || col_last;

    // Maximum along the Doppler run, then down the range run.
    wire [31:0] blk_new  = (cnt_d == 8'd0) ? map_pwr
                         : ((map_pwr > blk_max) ? map_pwr : blk_max);
    wire [31:0] cell_val = (cnt_r == 8'd0) ? blk_new
                         : ((blk_new > rowacc_q) ? blk_new : rowacc_q);

    // Read one column ahead so the accumulator is there on the clock the
    // Doppler block closes, even at a decimation of one.
    wire [9:0] od_next = row_last ? 10'd0 : (d_blk_end ? (od + 10'd1) : od);
    wire [9:0] nd_eff  = (mr == 9'd0) ? (od + 10'd1) : nd_out;

    wire [MAPAW-1:0] w_addr = row_base + od[MAPAW-1:0];
    wire             w_fits = ({1'b0, w_addr} < MAPW_LIM);

    wire cell_close = map_valid && d_blk_end && q_map_en;
    wire cell_final = cell_close && r_blk_end;
    wire cell_hold  = cell_close && !r_blk_end;

    //------------------------------------------------------------------------
    // DETECTION WRITER
    //------------------------------------------------------------------------
    reg [HITAW:0] hit_wcnt;

    wire [31:0] hit_w0 = {8'd0, 7'd0, hit_dopp[8], hit_dopp[7:0], hit_range};
    wire [HITW-1:0] hit_rec = {hit_w0,
                               hit_pwr,
                               hit_v0_i, hit_v0_q,
                               hit_v1_i, hit_v1_q,
                               hit_v2_i, hit_v2_q,
                               hit_v3_i, hit_v3_q};

    wire hit_take = hit_valid && q_hit_en && (hit_wcnt < MAXH_LIM);

    //------------------------------------------------------------------------
    // OUTPUT QUEUE -- four entries, enough to cover the one memory read in
    // flight plus a clock of backpressure, so the stream never bubbles.
    //------------------------------------------------------------------------
    reg [31:0] fifo_d [3:0];
    reg [3:0]  fifo_l;
    reg [1:0]  fifo_wp, fifo_rp;
    reg [2:0]  fifo_cnt;

    assign m_valid = (fifo_cnt != 3'd0);
    assign m_data  = fifo_d[fifo_rp];
    assign m_last  = fifo_l[fifo_rp];

    wire fifo_pop = m_valid & m_ready;

    reg        push;
    reg [31:0] push_d;
    reg        push_l;

    //------------------------------------------------------------------------
    // EMITTER state
    //------------------------------------------------------------------------
    localparam [2:0] S_IDLE = 3'd0, S_HDR = 3'd1, S_MAP = 3'd2,
                     S_HITS = 3'd3, S_END = 3'd4;

    reg [2:0]      st;
    reg [2:0]      hdr_i;
    reg [MAPAW:0]  map_i;
    reg            map_infl;
    reg [31:0]     map_rq;
    reg [HITAW:0]  hit_i;
    reg [3:0]      hit_ph;      // 0 settle, 1 load, 2..7 push words 0..5
    reg [HITW-1:0] hit_rq;
    reg [HITW-1:0] hit_reg;

    reg [15:0]     l_flags;
    reg [31:0]     l_index;
    reg [63:0]     l_time;
    reg [31:0]     l_noise;
    reg [15:0]     l_nhits;
    reg [15:0]     l_nr, l_nd;
    reg [MAPAW:0]  l_mapwords;
    reg            l_map_present, l_hit_present;
    reg            go;

    wire can_push  = (fifo_cnt < 3'd4);
    wire can_issue = ((fifo_cnt + {2'd0, map_infl}) <= 3'd2);
    wire idle_now  = (st == S_IDLE) && (fifo_cnt == 3'd0);

    wire map_present_now = q_map_en && !map_ovf && (row_base != {MAPAW{1'b0}});
    wire hit_present_now = q_hit_en && (hit_wcnt != {(HITAW+1){1'b0}});

    wire [2:0] hw   = hit_ph[2:0] - 3'd2;
    wire [7:0] hoff = {hw, 5'd0};
    wire [7:0] hsel = 8'd191 - hoff;

    reg [31:0] hdr_w;
    always @* begin
        case (hdr_i)
            3'd0:    hdr_w = RADAR_MAGIC;
            3'd1:    hdr_w = {RADAR_FMT_VER, l_flags};
            3'd2:    hdr_w = l_index;
            3'd3:    hdr_w = {l_nr, l_nd};
            3'd4:    hdr_w = {l_nhits, 16'd0};
            3'd5:    hdr_w = l_noise;
            3'd6:    hdr_w = l_time[63:32];
            default: hdr_w = l_time[31:0];
        endcase
    end

    //------------------------------------------------------------------------
    // Memories.  One write port and one read port each, in one block apiece.
    //------------------------------------------------------------------------
    always @(posedge clk) begin
        if (cell_final && w_fits) mapbuf[{wsel, w_addr}] <= cell_val;
        map_rq <= mapbuf[{rsel, map_i[MAPAW-1:0]}];
    end

    always @(posedge clk) begin
        if (hit_take) hitbuf[{wsel, hit_wcnt[HITAW-1:0]}] <= hit_rec;
        hit_rq <= hitbuf[{rsel, hit_i[HITAW-1:0]}];
    end

    always @(posedge clk) begin
        if (cell_hold) rowacc[od[8:0]] <= cell_val;
        if (map_valid) rowacc_q <= rowacc[od_next[8:0]];
    end

    //------------------------------------------------------------------------
    // Map write bookkeeping
    //------------------------------------------------------------------------
    always @(posedge clk) begin
        if (rst) begin
            mr <= 9'd0; md <= 10'd0; cnt_r <= 8'd0; cnt_d <= 8'd0;
            od <= 10'd0; row_base <= {MAPAW{1'b0}}; blk_max <= 32'd0;
            nd_out <= 10'd0; nr_out <= 9'd0; map_ovf <= 1'b0;
            hit_wcnt <= {(HITAW+1){1'b0}};
        end else begin
            if (frame_start) begin
                mr <= 9'd0; md <= 10'd0; cnt_r <= 8'd0; cnt_d <= 8'd0;
                od <= 10'd0; row_base <= {MAPAW{1'b0}};
                nd_out <= 10'd0; nr_out <= 9'd0; map_ovf <= 1'b0;
                hit_wcnt <= {(HITAW+1){1'b0}};
            end else begin
                if (hit_take) hit_wcnt <= hit_wcnt + {{HITAW{1'b0}}, 1'b1};

                if (map_valid) begin
                    blk_max <= blk_new;
                    if (cell_final && !w_fits) map_ovf <= 1'b1;

                    if (row_last) begin
                        md    <= 10'd0;
                        cnt_d <= 8'd0;
                        od    <= 10'd0;
                        if (mr == 9'd0) nd_out <= nd_eff;
                        mr <= col_last ? 9'd0 : (mr + 9'd1);
                        if (r_blk_end) begin
                            cnt_r    <= 8'd0;
                            row_base <= row_base + nd_eff[MAPAW-1:0];
                            nr_out   <= nr_out + 9'd1;
                        end else begin
                            cnt_r <= cnt_r + 8'd1;
                        end
                    end else begin
                        md <= md + 10'd1;
                        if (d_blk_end) begin
                            cnt_d <= 8'd0;
                            od    <= od + 10'd1;
                        end else begin
                            cnt_d <= cnt_d + 8'd1;
                        end
                    end
                end
            end
        end
    end

    //------------------------------------------------------------------------
    // Output queue
    //------------------------------------------------------------------------
    always @(posedge clk) begin
        if (rst) begin
            fifo_wp  <= 2'd0;
            fifo_rp  <= 2'd0;
            fifo_cnt <= 3'd0;
            fifo_l   <= 4'd0;
        end else begin
            if (push) begin
                fifo_d[fifo_wp] <= push_d;
                fifo_l[fifo_wp] <= push_l;
                fifo_wp         <= fifo_wp + 2'd1;
            end
            if (fifo_pop) fifo_rp <= fifo_rp + 2'd1;
            case ({push, fifo_pop})
                2'b10:   fifo_cnt <= fifo_cnt + 3'd1;
                2'b01:   fifo_cnt <= fifo_cnt - 3'd1;
                default: fifo_cnt <= fifo_cnt;
            endcase
        end
    end

    //------------------------------------------------------------------------
    // Frame handover and emitter
    //------------------------------------------------------------------------
    always @(posedge clk) begin
        if (rst) begin
            st       <= S_IDLE;
            hdr_i    <= 3'd0;
            map_i    <= {(MAPAW+1){1'b0}};
            map_infl <= 1'b0;
            hit_i    <= {(HITAW+1){1'b0}};
            hit_ph   <= 4'd0;
            hit_reg  <= {HITW{1'b0}};
            push     <= 1'b0;
            push_d   <= 32'd0;
            push_l   <= 1'b0;
            go       <= 1'b0;
            rsel     <= 1'b0;
            wsel     <= 1'b0;
            l_flags  <= 16'd0;
            l_index  <= 32'd0;
            l_time   <= 64'd0;
            l_noise  <= 32'd0;
            l_nhits  <= 16'd0;
            l_nr     <= 16'd0;
            l_nd     <= 16'd0;
            l_mapwords    <= {(MAPAW+1){1'b0}};
            l_map_present <= 1'b0;
            l_hit_present <= 1'b0;
            q_map_en <= 1'b0;  q_hit_en <= 1'b0;
            q_dec_r  <= 8'd1;  q_dec_d  <= 8'd1;
            q_flags  <= 16'd0; q_index  <= 32'd0; q_time <= 64'd0;
            q_nr     <= 9'd256; q_nd    <= 10'd256;
        end else begin
            push <= 1'b0;
            go   <= 1'b0;

            if (frame_start) begin
                q_map_en <= cfg_map_enable;
                q_hit_en <= cfg_hits_enable;
                q_dec_r  <= cfg_map_decim_r;
                q_dec_d  <= cfg_map_decim_d;
                q_flags  <= cfg_flags;
                q_index  <= frame_index;
                q_time   <= timestamp;
                q_nr     <= n_range;
                q_nd     <= n_doppler;
            end

            // Hand the finished frame to the emitter.  If the previous packet
            // is still going out this frame's packet is skipped rather than
            // corrupted -- the write buffer is simply reused.
            if (frame_end && idle_now) begin
                l_map_present <= map_present_now;
                l_hit_present <= hit_present_now;
                l_flags       <= {q_flags[15:2], hit_present_now, map_present_now};
                l_index       <= q_index;
                l_time        <= q_time;
                l_noise       <= noise;
                l_nhits       <= hit_present_now
                                 ? {{(15-HITAW){1'b0}}, hit_wcnt} : 16'd0;
                l_nr          <= map_present_now ? {7'd0, nr_out} : 16'd0;
                l_nd          <= map_present_now ? {6'd0, nd_out} : 16'd0;
                l_mapwords    <= map_present_now ? {1'b0, row_base}
                                                 : {(MAPAW+1){1'b0}};
                rsel          <= wsel;
                wsel          <= ~wsel;
                go            <= 1'b1;
            end

            case (st)
                S_IDLE: begin
                    hdr_i    <= 3'd0;
                    map_i    <= {(MAPAW+1){1'b0}};
                    hit_i    <= {(HITAW+1){1'b0}};
                    hit_ph   <= 4'd0;
                    map_infl <= 1'b0;
                    if (go) st <= S_HDR;
                end

                S_HDR: begin
                    if (can_push) begin
                        push   <= 1'b1;
                        push_d <= hdr_w;
                        push_l <= 1'b0;
                        if (hdr_i == 3'd7) begin
                            st <= (l_map_present && (l_mapwords != {(MAPAW+1){1'b0}}))
                                    ? S_MAP : (l_hit_present ? S_HITS : S_END);
                        end else begin
                            hdr_i <= hdr_i + 3'd1;
                        end
                    end
                end

                S_MAP: begin
                    if (map_infl) begin
                        push     <= 1'b1;
                        push_d   <= map_rq;
                        push_l   <= 1'b0;
                        map_infl <= 1'b0;
                    end
                    if (can_issue && (map_i < l_mapwords)) begin
                        map_infl <= 1'b1;
                        map_i    <= map_i + {{MAPAW{1'b0}}, 1'b1};
                    end else if (!map_infl && (map_i >= l_mapwords)) begin
                        st <= l_hit_present ? S_HITS : S_END;
                    end
                end

                S_HITS: begin
                    if (hit_ph == 4'd0) begin
                        hit_ph <= 4'd1;                 // let the address settle
                    end else if (hit_ph == 4'd1) begin
                        hit_reg <= hit_rq;
                        hit_ph  <= 4'd2;
                    end else if (can_push) begin
                        push   <= 1'b1;
                        push_d <= hit_reg[hsel -: 32];
                        push_l <= 1'b0;
                        if (hit_ph == 4'd7) begin
                            hit_ph <= 4'd0;
                            if ((hit_i + {{HITAW{1'b0}}, 1'b1}) >= l_nhits[HITAW:0]) begin
                                st <= S_END;
                            end else begin
                                hit_i <= hit_i + {{HITAW{1'b0}}, 1'b1};
                            end
                        end else begin
                            hit_ph <= hit_ph + 4'd1;
                        end
                    end
                end

                default: begin   // S_END
                    if (can_push) begin
                        push   <= 1'b1;
                        push_d <= RADAR_ENDMARK;
                        push_l <= 1'b1;
                        st     <= S_IDLE;
                    end
                end
            endcase
        end
    end

endmodule

`default_nettype wire
