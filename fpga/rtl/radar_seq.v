//============================================================================
// radar_seq.v -- the deterministic TDM / DDM chirp sequencer.
//
// This is the timing heart of the radar.  Everything downstream assumes that
// chirp k began exactly k * t_pri clocks after chirp 0 and that the sweep is
// exactly t_sweep clocks long, because that assumption IS the coherent
// processing interval: a single clock of jitter between chirps puts a phase
// error of 2*pi*f_beat/f_s on that chirp's sample of the Doppler transform,
// which raises the Doppler sidelobe floor and hides slow targets under fast
// ones.  So there is one free-running counter, everything is decoded from it,
// and there is no handshake anywhere that could stall it.
//
// STATE
//   pri_cnt    0 .. t_pri-1     position inside the current chirp
//   chirp_cnt  0 .. n_total-1   position inside the coherent interval
//   n_total  = n_chirp * 2 in TDM (n_chirp per transmitter), n_chirp otherwise
//
// MIMO MODES
//   TDM  chirp k transmits on TX(k & 1), the other transmitter is off.  Clean
//        separation; costs half the unambiguous velocity and 3 dB of energy.
//   DDM  both transmitters fire on every chirp and TX1 is inverted on odd
//        chirps, so TX1's echo lands half a Doppler band away from TX0's and
//        the two are separated in Doppler rather than in time.  Full duty on
//        both transmitters, full PRF for Doppler.
//   TX0 / TX1  one transmitter always, for diagnostics and for measuring
//        transmit-receive isolation.
//
// OUTPUT TIMING  (all outputs are registered; the decode is taken from the
// NEXT-state counters so that every output is exactly aligned with the cycle
// it describes -- there is no combinational path from the counters to a pin)
//   nco_restart  1 clock, on the first cycle of every sweep
//   adc_gate     high for exactly t_sweep cycles, starting on that same cycle
//   nco_ena      the same window; the NCO's fixed 3-clock latency is absorbed
//                by delaying the ADC path, not by starting the NCO early, so
//                the sweep origin stays exact
//   sample_idx   0 .. t_sweep-1 while adc_gate is high, 0 otherwise
//   frame_start  1 clock, on the first cycle of chirp 0 of a coherent interval
//   frame_end    1 clock, on the last cycle of the last chirp
//   tx_invert    the DDM sign flag for TX1; high for the whole of every odd
//                chirp in DDM, low in every other mode
//
// ENABLE
//   Dropping `enable` mid-interval lets the current interval FINISH and then
//   stops.  A truncated coherent interval is worse than no interval at all --
//   its Doppler transform is a different length from every other one.
//   Comparisons are >= rather than == so that a mid-flight change of t_pri,
//   t_sweep or n_chirp cannot leave the counters stranded.
//
// LATENCY
//   1 clock from `enable` rising to `running`, nco_restart, frame_start and
//   adc_gate for chirp 0.  Zero latency thereafter -- it is a free-running
//   counter, and the whole interval is scheduled from the one start event.
//
// RESOURCES on XC7K325T
//   ~120 slice LUT, ~90 FF, 0 DSP, 0 BRAM.
//============================================================================
`timescale 1ns / 1ps
`default_nettype none

module radar_seq #(
    parameter integer CNT_W    = 16,      // t_sweep / t_pri / sample_idx
    parameter integer CHIRP_W  = 16,      // n_chirp / chirp_idx
    parameter [1:0]   MIMO_TDM = 2'd0,
    parameter [1:0]   MIMO_DDM = 2'd1,
    parameter [1:0]   MIMO_TX0 = 2'd2
) (
    input  wire                     clk,
    input  wire                     rst,          // synchronous, active high
    input  wire                     enable,
    input  wire        [1:0]        mimo_mode,
    input  wire                     tx_enable,
    input  wire        [CNT_W-1:0]  t_sweep,
    input  wire        [CNT_W-1:0]  t_pri,
    input  wire        [CHIRP_W-1:0] n_chirp,     // per transmitter
    output reg                      nco_restart,
    output reg                      nco_ena,
    output reg                      tx0_ena,
    output reg                      tx1_ena,
    output reg                      tx_invert,
    output reg                      adc_gate,
    output reg         [CNT_W-1:0]  sample_idx,
    output reg         [CHIRP_W-1:0] chirp_idx,
    output reg                      tx_sel,
    output reg                      frame_start,
    output reg                      frame_end,
    output reg                      running
);

    localparam integer CW = CHIRP_W + 1;         // n_chirp*2 needs one more bit

    localparam [CNT_W-1:0] CNT_ZERO = {CNT_W{1'b0}};
    localparam [CNT_W-1:0] CNT_ONE  = {{(CNT_W-1){1'b0}}, 1'b1};
    localparam [CW-1:0]    CW_ZERO  = {CW{1'b0}};
    localparam [CW-1:0]    CW_ONE   = {{(CW-1){1'b0}}, 1'b1};

    //------------------------------------------------------------------------
    // Interval geometry.  Guarded so that a zero setting degrades to one,
    // never to a counter that free-runs for 65536 cycles.
    //------------------------------------------------------------------------
    wire [CW-1:0] n_total = (mimo_mode == MIMO_TDM) ? {n_chirp, 1'b0}
                                                    : {1'b0, n_chirp};
    wire [CW-1:0] n_total_m1 = (n_total == CW_ZERO)  ? CW_ZERO  : (n_total - CW_ONE);
    wire [CNT_W-1:0] t_pri_m1 = (t_pri == CNT_ZERO)  ? CNT_ZERO : (t_pri - CNT_ONE);

    //------------------------------------------------------------------------
    // Counters
    //------------------------------------------------------------------------
    reg  [CNT_W-1:0] pri_cnt;
    reg  [CW-1:0]    chirp_cnt;

    reg              nx_running;
    reg  [CNT_W-1:0] nx_pri;
    reg  [CW-1:0]    nx_chirp;

    always @(*) begin
        nx_running = running;
        nx_pri     = pri_cnt;
        nx_chirp   = chirp_cnt;
        if (!running) begin
            nx_pri     = CNT_ZERO;
            nx_chirp   = CW_ZERO;
            nx_running = enable;
        end else if (pri_cnt >= t_pri_m1) begin
            nx_pri = CNT_ZERO;
            if (chirp_cnt >= n_total_m1) begin
                nx_chirp   = CW_ZERO;
                nx_running = enable;        // end of interval: stop here if the
            end else begin                  // host has dropped enable
                nx_chirp = chirp_cnt + CW_ONE;
            end
        end else begin
            nx_pri = pri_cnt + CNT_ONE;
        end
    end

    //------------------------------------------------------------------------
    // Decode of the cycle that is about to happen
    //------------------------------------------------------------------------
    wire nx_in_sweep = nx_running && (nx_pri < t_sweep);
    wire nx_first    = nx_running && (nx_pri == CNT_ZERO);
    wire nx_last     = nx_running && (nx_pri >= t_pri_m1)
                                  && (nx_chirp >= n_total_m1);
    wire nx_odd      = nx_chirp[0];
    wire nx_txact    = nx_in_sweep && tx_enable;

    reg nx_tx0, nx_tx1, nx_inv, nx_sel;

    always @(*) begin
        case (mimo_mode)
            MIMO_TDM: begin
                nx_tx0 = nx_txact && !nx_odd;
                nx_tx1 = nx_txact &&  nx_odd;
                nx_inv = 1'b0;
                nx_sel = nx_odd;
            end
            MIMO_DDM: begin
                nx_tx0 = nx_txact;
                nx_tx1 = nx_txact;
                nx_inv = nx_running && nx_odd;
                nx_sel = 1'b0;
            end
            MIMO_TX0: begin
                nx_tx0 = nx_txact;
                nx_tx1 = 1'b0;
                nx_inv = 1'b0;
                nx_sel = 1'b0;
            end
            default: begin                  // MIMO_TX1
                nx_tx0 = 1'b0;
                nx_tx1 = nx_txact;
                nx_inv = 1'b0;
                nx_sel = 1'b1;
            end
        endcase
    end

    //------------------------------------------------------------------------
    // Registers.  Counters and outputs come off the same next-state, so the
    // outputs always describe the cycle the counters are showing.
    //------------------------------------------------------------------------
    always @(posedge clk) begin
        if (rst) begin
            running     <= 1'b0;
            pri_cnt     <= CNT_ZERO;
            chirp_cnt   <= CW_ZERO;
            nco_restart <= 1'b0;
            nco_ena     <= 1'b0;
            adc_gate    <= 1'b0;
            tx0_ena     <= 1'b0;
            tx1_ena     <= 1'b0;
            tx_invert   <= 1'b0;
            tx_sel      <= 1'b0;
            sample_idx  <= CNT_ZERO;
            chirp_idx   <= {CHIRP_W{1'b0}};
            frame_start <= 1'b0;
            frame_end   <= 1'b0;
        end else begin
            running     <= nx_running;
            pri_cnt     <= nx_pri;
            chirp_cnt   <= nx_chirp;
            nco_restart <= nx_first;
            nco_ena     <= nx_in_sweep;
            adc_gate    <= nx_in_sweep;
            tx0_ena     <= nx_tx0;
            tx1_ena     <= nx_tx1;
            tx_invert   <= nx_inv;
            tx_sel      <= nx_sel;
            sample_idx  <= nx_in_sweep ? nx_pri : CNT_ZERO;
            chirp_idx   <= nx_chirp[CHIRP_W-1:0];
            frame_start <= nx_first && (nx_chirp == CW_ZERO);
            frame_end   <= nx_last;
        end
    end

endmodule

`default_nettype wire
