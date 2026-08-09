// Verilated -*- C++ -*-
// DESCRIPTION: Verilator output: Design internal header
// See Vradar_seq.h for the primary calling header

#ifndef VERILATED_VRADAR_SEQ___024ROOT_H_
#define VERILATED_VRADAR_SEQ___024ROOT_H_  // guard

#include "verilated.h"


class Vradar_seq__Syms;

class alignas(VL_CACHE_LINE_BYTES) Vradar_seq___024root final {
  public:

    // DESIGN SPECIFIC STATE
    VL_IN8(clk,0,0);
    VL_IN8(rst,0,0);
    VL_IN8(enable,0,0);
    VL_IN8(mimo_mode,1,0);
    VL_IN8(tx_enable,0,0);
    VL_OUT8(nco_restart,0,0);
    VL_OUT8(nco_ena,0,0);
    VL_OUT8(tx0_ena,0,0);
    VL_OUT8(tx1_ena,0,0);
    VL_OUT8(tx_invert,0,0);
    VL_OUT8(adc_gate,0,0);
    VL_OUT8(tx_sel,0,0);
    VL_OUT8(frame_start,0,0);
    VL_OUT8(frame_end,0,0);
    VL_OUT8(running,0,0);
    CData/*0:0*/ radar_seq__DOT__nx_running;
    CData/*0:0*/ radar_seq__DOT__nx_in_sweep;
    CData/*0:0*/ radar_seq__DOT__nx_first;
    CData/*4:0*/ __VdfgRegularize_hebeb780c_0_0;
    CData/*0:0*/ __VstlFirstIteration;
    CData/*0:0*/ __VstlPhaseResult;
    CData/*0:0*/ __Vtrigprevexpr___TOP__clk__0;
    CData/*0:0*/ __Vtrigprevexpr___TOP__rst__0;
    CData/*0:0*/ __Vtrigprevexpr___TOP__enable__0;
    CData/*1:0*/ __Vtrigprevexpr___TOP__mimo_mode__0;
    CData/*0:0*/ __Vtrigprevexpr___TOP__tx_enable__0;
    CData/*0:0*/ __VicoDidInit;
    CData/*0:0*/ __VicoPhaseResult;
    CData/*0:0*/ __Vtrigprevexpr___TOP__clk__1;
    CData/*0:0*/ __VactPhaseResult;
    CData/*0:0*/ __VnbaPhaseResult;
    VL_IN16(t_sweep,15,0);
    VL_IN16(t_pri,15,0);
    VL_IN16(n_chirp,15,0);
    VL_OUT16(sample_idx,15,0);
    VL_OUT16(chirp_idx,15,0);
    SData/*15:0*/ radar_seq__DOT__t_pri_m1;
    SData/*15:0*/ radar_seq__DOT__pri_cnt;
    SData/*15:0*/ radar_seq__DOT__nx_pri;
    SData/*15:0*/ __Vtrigprevexpr___TOP__t_sweep__0;
    SData/*15:0*/ __Vtrigprevexpr___TOP__t_pri__0;
    SData/*15:0*/ __Vtrigprevexpr___TOP__n_chirp__0;
    IData/*16:0*/ radar_seq__DOT__n_total_m1;
    IData/*16:0*/ radar_seq__DOT__chirp_cnt;
    IData/*16:0*/ radar_seq__DOT__nx_chirp;
    IData/*31:0*/ __VactIterCount;
    VlUnpacked<QData/*63:0*/, 1> __VstlTriggered;
    VlUnpacked<QData/*63:0*/, 2> __VicoTriggered;
    VlUnpacked<QData/*63:0*/, 1> __VactTriggered;
    VlUnpacked<QData/*63:0*/, 1> __VnbaTriggered;

    // INTERNAL VARIABLES
    Vradar_seq__Syms* vlSymsp;
    const char* vlNamep;

    // CONSTRUCTORS
    Vradar_seq___024root(Vradar_seq__Syms* symsp, const char* namep);
    ~Vradar_seq___024root();
    VL_UNCOPYABLE(Vradar_seq___024root);

    // INTERNAL METHODS
    void __Vconfigure(bool first);
};


#endif  // guard
