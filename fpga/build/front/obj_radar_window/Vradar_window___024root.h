// Verilated -*- C++ -*-
// DESCRIPTION: Verilator output: Design internal header
// See Vradar_window.h for the primary calling header

#ifndef VERILATED_VRADAR_WINDOW___024ROOT_H_
#define VERILATED_VRADAR_WINDOW___024ROOT_H_  // guard

#include "verilated.h"


class Vradar_window__Syms;

class alignas(VL_CACHE_LINE_BYTES) Vradar_window___024root final {
  public:

    // DESIGN SPECIFIC STATE
    VL_IN8(clk,0,0);
    VL_IN8(rst,0,0);
    VL_IN8(in_valid,0,0);
    VL_IN8(coef_we,0,0);
    VL_OUT8(out_valid,0,0);
    CData/*0:0*/ radar_window__DOT__oob_s1;
    CData/*0:0*/ radar_window__DOT__v_s1;
    CData/*0:0*/ radar_window__DOT__oob_s2;
    CData/*0:0*/ radar_window__DOT__v_s2;
    CData/*0:0*/ __Vtrigprevexpr___TOP__clk__0;
    CData/*0:0*/ __VactPhaseResult;
    CData/*0:0*/ __VnbaPhaseResult;
    VL_IN16(in_i,15,0);
    VL_IN16(in_q,15,0);
    VL_IN16(sample_idx,15,0);
    VL_IN16(coef_addr,15,0);
    VL_IN16(coef_data,15,0);
    VL_OUT16(out_i,15,0);
    VL_OUT16(out_q,15,0);
    SData/*15:0*/ radar_window__DOT____VlemCond_3;
    SData/*15:0*/ radar_window__DOT____VlemCall_2__round_sat;
    SData/*15:0*/ radar_window__DOT____VlemCond_1;
    SData/*15:0*/ radar_window__DOT____VlemCall_0__round_sat;
    SData/*15:0*/ radar_window__DOT__coef_s1;
    SData/*15:0*/ radar_window__DOT__d_i_s1;
    SData/*15:0*/ radar_window__DOT__d_q_s1;
    IData/*31:0*/ radar_window__DOT__round_sat__Vstatic__t;
    IData/*31:0*/ radar_window__DOT__p_i_s2;
    IData/*31:0*/ radar_window__DOT__p_q_s2;
    IData/*31:0*/ __VactIterCount;
    VlUnpacked<SData/*15:0*/, 1024> radar_window__DOT__win;
    VlUnpacked<QData/*63:0*/, 1> __VactTriggered;
    VlUnpacked<QData/*63:0*/, 1> __VnbaTriggered;

    // INTERNAL VARIABLES
    Vradar_window__Syms* vlSymsp;
    const char* vlNamep;

    // CONSTRUCTORS
    Vradar_window___024root(Vradar_window__Syms* symsp, const char* namep);
    ~Vradar_window___024root();
    VL_UNCOPYABLE(Vradar_window___024root);

    // INTERNAL METHODS
    void __Vconfigure(bool first);
};


#endif  // guard
