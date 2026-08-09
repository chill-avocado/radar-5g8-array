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
//   built up incrementally -- a register for the run along Doppler, a
//   read-modify-write into the map buffer for the run down range -- so it
//   costs one comparison per input cell whatever the decimation factor.
//   n_range_out and n_doppler_out fall out of the block counters, so no
//   division is needed to fill in header word 3.
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

    //------------------------------------------------------------------------
    // Map buffers and detection buffers, one pair of each
    //------------------------------------------------------------------------
    reg [31:0]     mapbuf [(2*MAP_WORDS)-1:0];
    reg [HITW-1:0] hitbuf [(2*MAX_HITS)-1:0];
    reg            wsel, rsel;

    //------------------------------------------------------------------------
    // MAP WRITER -- block maximum, incremental, one comparison per input cell
    //------------------------------------------------------------------------
    reg [8:0]       mr;          // input range row
    reg [9:0]       md;          // input Doppler column
    reg [7:0]       cnt_r;       // position within the range block
    reg [7:0]       cnt_d;       // position within the Doppler block
    reg [9:0]       od;          // output Doppler index
    reg [MAPAW-1:0] row_base;    // first word of the current output row
    reg [31:0]      blk_max;     // running maximum along Doppler
    reg [9:0]       nd_out;
    reg [8:0]       nr_out;
    reg             map_ovf;

    wire row_last  = (md == (q_nd - 10'd1));
    wire d_blk_end = (cnt_d == (dec_d - 8'd1)) || row_last;
    wire col_last  = (mr == (q_nr - 9'd1));
    wire r_blk_end = (cnt_r == (dec_r - 8'd1)) || col_last;

    wire [31:0] blk_new = (cnt_d == 8'd0) ? map_pwr
                        : ((map_pwr > blk_max) ? map_pwr : blk_max);

    wire [MAPAW-1:0] w_addr = row_base + od[MAPAW-1:0];
    wire             w_fits = ({1'b0, w_addr} < MAP_WORDS[MAPAW:0]);

    reg  [31:0]      map_q;      // read-modify-write feedback
    reg              wr_pend;
    reg  [MAPAW-1:0] wr_addr;
    reg  [31:0]      wr_data;
    reg              wr_first;
    reg              wr_sel_d;

    wire [31:0] wr_merged = wr_first ? wr_data
                          : ((wr_data > map_q) ? wr_data : map_q);

    always @(posedge clk) begin
        if (map_valid) map_q <= mapbuf[{wsel, w_addr}];
        if (wr_pend)   mapbuf[{wr_sel_d, wr_addr}] <= wr_merged;
    end

    always @(posedge clk) begin
        if (rst) begin
            mr <= 9'd0; md <= 10'd0; cnt_r <= 8'd0; cnt_d <= 8'd0;
            od <= 10'd0; row_base <= {MAPAW{1'b0}};
            blk_max <= 32'd0; nd_out <= 10'd0; nr_out <= 9'd0;
            map_ovf <= 1'b0; wr_pend <= 1'b0; wr_first <= 1'b0;
            wr_addr <= {MAPAW{1'b0}}; wr_data <= 32'd0; wr_sel_d <= 1'b0;
        end else begin
            wr_pend <= 1'b0;

            if (frame_start) begin
                mr <= 9'd0; md <= 10'd0; cnt_r <= 8'd0; cnt_d <= 8'd0;
                od <= 10'd0; row_base <= {MAPAW{1'b0}};
                nd_out <= 10'd0; nr_out <= 9'd0; map_ovf <= 1'b0;
            end else if (map_valid) begin
                blk_max <= blk_new;

                if (d_blk_end && q_map_en) begin
                    if (w_fits) begin
                        wr_pend  <= 1'b1;
                        wr_addr  <= w_addr;
                        wr_data  <= blk_new;
                        wr_first <= (cnt_r == 8'd0);
                        wr_sel_d <= wsel;
                    end else begin
                        map_ovf <= 1'b1;
                    end
                end

                // Doppler block / row bookkeeping
                if (row_last) begin
                    md    <= 10'd0;
                    cnt_d <= 8'd0;
                    od    <= 10'd0;
                    if (mr == 9'd0) nd_out <= od + 10'd1;
                    // range block / frame bookkeeping
                    if (col_last) begin
                        mr <= 9'd0;
                    end else begin
                        mr <= mr + 9'd1;
                    end
                    if (r_blk_end) begin
                        cnt_r    <= 8'd0;
                        row_base <= row_base + ((mr == 9'd0) ? (od[MAPAW-1:0] + {{(MAPAW-1){1'b0}}, 1'b1})
                                                             : nd_out[MAPAW-1:0]);
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

    wire hit_take = hit_valid && q_hit_en &&
                    (hit_wcnt < MAX_HITS[HITAW:0]);

    always @(posedge clk) begin
        if (hit_take) hitbuf[{wsel, hit_wcnt[HITAW-1:0]}] <= hit_rec;
    end

    always @(posedge clk) begin
        if (rst)              hit_wcnt <= {(HITAW+1){1'b0}};
        else if (frame_start) hit_wcnt <= {(HITAW+1){1'b0}};
        else if (hit_take)    hit_wcnt <= hit_wcnt + {{HITAW{1'b0}}, 1'b1};
    end

    //------------------------------------------------------------------------
    // Frame handover
    //------------------------------------------------------------------------
    reg [15:0]      l_flags;
    reg [31:0]      l_index;
    reg [63:0]      l_time;
    reg [31:0]      l_noise;
    reg [15:0]      l_nhits;
    reg [15:0]      l_nr, l_nd;
    reg [MAPAW:0]   l_mapwords;
    reg             l_map_present, l_hit_present;
    reg             go;

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
    // EMITTER
    //------------------------------------------------------------------------
    localparam [2:0] S_IDLE = 3'd0, S_HDR = 3'd1, S_MAP = 3'd2,
                     S_HITS = 3'd3, S_END = 3'd4;

    reg  [2:0]       st;
    reg  [2:0]       hdr_i;
    reg  [MAPAW:0]   map_i;
    reg              map_infl;
    reg  [31:0]      map_rq;
    reg  [HITAW:0]   hit_i;
    reg  [2:0]       hit_w;
    reg              hit_infl;
    reg  [HITW-1:0]  hit_rq;
    reg  [HITW-1:0]  hit_reg;

    wire can_push  = (fifo_cnt < 3'd4);
    wire can_issue = ((fifo_cnt + {2'd0, map_infl}) <= 3'd2);

    wire idle_now  = (st == S_IDLE) && (fifo_cnt == 3'd0);

    // Header words
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

    wire [31:0] hit_word = hit_reg[HITW-1 - (32*hit_w) -: 32];

    always @(posedge clk) begin
        if (map_valid || (st == S_MAP)) map_rq <= mapbuf[{rsel, map_i[MAPAW-1:0]}];
        hit_rq <= hitbuf[{rsel, hit_i[HITAW-1:0]}];
    end

    always @(posedge clk) begin
        if (rst) begin
            st       <= S_IDLE;
            hdr_i    <= 3'd0;
            map_i    <= {(MAPAW+1){1'b0}};
            map_infl <= 1'b0;
            hit_i    <= {(HITAW+1){1'b0}};
            hit_w    <= 3'd0;
            hit_infl <= 1'b0;
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
            q_map_en <= 1'b0; q_hit_en <= 1'b0;
            q_dec_r  <= 8'd1; q_dec_d  <= 8'd1;
            q_flags  <= 16'd0; q_index <= 32'd0; q_time <= 64'd0;
            q_nr     <= 9'd256; q_nd   <= 10'd256;
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

            //---- hand the finished frame to the emitter ---------------------
            if (frame_end) begin
                if (idle_now) begin
                    l_map_present <= q_map_en && !map_ovf && (row_base != {MAPAW{1'b0}});
                    l_hit_present <= q_hit_en && (hit_wcnt != {(HITAW+1){1'b0}});
                    l_flags       <= {q_flags[15:2],
                                      q_hit_en && (hit_wcnt != {(HITAW+1){1'b0}}),
                                      q_map_en && !map_ovf && (row_base != {MAPAW{1'b0}})};
                    l_index       <= q_index;
                    l_time        <= q_time;
                    l_noise       <= noise;
                    l_nhits       <= (q_hit_en) ? {{(16-HITAW-1){1'b0}}, hit_wcnt} : 16'd0;
                    l_nr          <= (q_map_en && !map_ovf) ? {7'd0, nr_out} : 16'd0;
                    l_nd          <= (q_map_en && !map_ovf) ? {6'd0, nd_out} : 16'd0;
                    l_mapwords    <= (q_map_en && !map_ovf) ? {1'b0, row_base}
                                                            : {(MAPAW+1){1'b0}};
                    rsel          <= wsel;
                    wsel          <= ~wsel;
                    go            <= 1'b1;
                end
                // else: the previous packet is still going out.  Skip this
                // frame's packet rather than corrupt it; the write buffer is
                // reused, so nothing is left half written.
            end

            //---- emitter state machine --------------------------------------
            case (st)
                S_IDLE: begin
                    hdr_i    <= 3'd0;
                    map_i    <= {(MAPAW+1){1'b0}};
                    hit_i    <= {(HITAW+1){1'b0}};
                    hit_w    <= 3'd0;
                    map_infl <= 1'b0;
                    hit_infl <= 1'b0;
                    if (go) st <= S_HDR;
                end

                S_HDR: begin
                    if (can_push) begin
                        push   <= 1'b1;
                        push_d <= hdr_w;
                        push_l <= 1'b0;
                        if (hdr_i == 3'd7) begin
                            st <= (l_map_present && (l_mapwords != {(MAPAW+1){1'b0}}))
                                    ? S_MAP
                                    : (l_hit_present ? S_HITS : S_END);
                        end else begin
                            hdr_i <= hdr_i + 3'd1;
                        end
                    end
                end

                S_MAP: begin
                    // one read in flight; the queue absorbs it if m_ready drops
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
                    if (!hit_infl) begin
                        hit_infl <= 1'b1;          // hit_rq is loaded every clock
                    end else if (hit_w == 3'd0) begin
                        hit_reg <= hit_rq;
                        hit_w   <= 3'd1;
                    end else if (can_push) begin
                        push   <= 1'b1;
                        push_d <= (hit_w == 3'd1) ? hit_reg[HITW-1 -: 32] : hit_word;
                        push_l <= 1'b0;
                        if (hit_w == 3'd6) begin
                            hit_w    <= 3'd0;
                            hit_infl <= 1'b0;
                            if ((hit_i + {{HITAW{1'b0}}, 1'b1}) >= l_nhits[HITAW:0]) begin
                                st <= S_END;
                            end else begin
                                hit_i <= hit_i + {{HITAW{1'b0}}, 1'b1};
                            end
                        end else begin
                            hit_w <= hit_w + 3'd1;
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
