// Verilated -*- C++ -*-
// DESCRIPTION: Verilator output: Design internal header
// See Vradar_nco.h for the primary calling header

#ifndef VERILATED_VRADAR_NCO___024ROOT_H_
#define VERILATED_VRADAR_NCO___024ROOT_H_  // guard

#include "verilated.h"


class Vradar_nco__Syms;

class alignas(VL_CACHE_LINE_BYTES) Vradar_nco___024root final {
  public:

    // DESIGN SPECIFIC STATE
    VL_IN8(clk,0,0);
    VL_IN8(rst,0,0);
    VL_IN8(ena,0,0);
    VL_IN8(restart,0,0);
    VL_OUT8(out_valid,0,0);
    CData/*0:0*/ radar_nco__DOT__neg_i_s1;
    CData/*0:0*/ radar_nco__DOT__neg_q_s1;
    CData/*0:0*/ radar_nco__DOT__vld_s1;
    CData/*0:0*/ radar_nco__DOT__neg_i_s2;
    CData/*0:0*/ radar_nco__DOT__neg_q_s2;
    CData/*0:0*/ radar_nco__DOT__vld_s2;
    CData/*0:0*/ __Vtrigprevexpr___TOP__clk__0;
    CData/*0:0*/ __VactPhaseResult;
    CData/*0:0*/ __VnbaPhaseResult;
    VL_OUT16(out_i,15,0);
    VL_OUT16(out_q,15,0);
    SData/*9:0*/ radar_nco__DOT__addr_i_s1;
    SData/*9:0*/ radar_nco__DOT__addr_q_s1;
    SData/*15:0*/ radar_nco__DOT__rom_i_s2;
    SData/*15:0*/ radar_nco__DOT__rom_q_s2;
    VL_IN(freq_start,31,0);
    VL_IN(freq_slope,31,0);
    IData/*31:0*/ radar_nco__DOT__rv;
    IData/*31:0*/ radar_nco__DOT__phase;
    IData/*31:0*/ radar_nco__DOT__freq_cur;
    IData/*31:0*/ __VactIterCount;
    VlUnpacked<SData/*15:0*/, 1024> radar_nco__DOT__sin_rom;
    VlUnpacked<QData/*63:0*/, 1> __VactTriggered;
    VlUnpacked<QData/*63:0*/, 1> __VnbaTriggered;
    double radar_nco__DOT__ang_r;

    // INTERNAL VARIABLES
    Vradar_nco__Syms* vlSymsp;
    const char* vlNamep;

    // CONSTRUCTORS
    Vradar_nco___024root(Vradar_nco__Syms* symsp, const char* namep);
    ~Vradar_nco___024root();
    VL_UNCOPYABLE(Vradar_nco___024root);

    // INTERNAL METHODS
    void __Vconfigure(bool first);
};


#endif  // guard
