// Verilated -*- C++ -*-
// DESCRIPTION: Verilator output: Design internal header
// See Vradar_dechirp.h for the primary calling header

#ifndef VERILATED_VRADAR_DECHIRP___024ROOT_H_
#define VERILATED_VRADAR_DECHIRP___024ROOT_H_  // guard

#include "verilated.h"


class Vradar_dechirp__Syms;

class alignas(VL_CACHE_LINE_BYTES) Vradar_dechirp___024root final {
  public:

    // DESIGN SPECIFIC STATE
    VL_IN8(clk,0,0);
    VL_IN8(rst,0,0);
    VL_IN8(in_valid,0,0);
    VL_IN8(shift,3,0);
    VL_OUT8(out_valid,0,0);
    CData/*5:0*/ radar_dechirp__DOT__sh_s1;
    CData/*0:0*/ radar_dechirp__DOT__v_s1;
    CData/*5:0*/ radar_dechirp__DOT__sh_s2;
    CData/*0:0*/ radar_dechirp__DOT__v_s2;
    CData/*5:0*/ radar_dechirp__DOT__sh_s3;
    CData/*0:0*/ radar_dechirp__DOT__v_s3;
    CData/*0:0*/ __Vtrigprevexpr___TOP__clk__0;
    CData/*0:0*/ __VactPhaseResult;
    CData/*0:0*/ __VnbaPhaseResult;
    VL_IN16(in_i,15,0);
    VL_IN16(in_q,15,0);
    VL_IN16(ref_i,15,0);
    VL_IN16(ref_q,15,0);
    VL_OUT16(out_i,15,0);
    VL_OUT16(out_q,15,0);
    SData/*15:0*/ radar_dechirp__DOT__a_i_s1;
    SData/*15:0*/ radar_dechirp__DOT__a_q_s1;
    SData/*15:0*/ radar_dechirp__DOT__b_i_s1;
    SData/*15:0*/ radar_dechirp__DOT__b_q_s1;
    IData/*31:0*/ radar_dechirp__DOT__p_ii_s2;
    IData/*31:0*/ radar_dechirp__DOT__p_qq_s2;
    IData/*31:0*/ radar_dechirp__DOT__p_iq_s2;
    IData/*31:0*/ radar_dechirp__DOT__p_qi_s2;
    IData/*31:0*/ __VactIterCount;
    QData/*33:0*/ radar_dechirp__DOT__rr_s3;
    QData/*33:0*/ radar_dechirp__DOT__ii_s3;
    VlUnpacked<QData/*63:0*/, 1> __VactTriggered;
    VlUnpacked<QData/*63:0*/, 1> __VnbaTriggered;

    // INTERNAL VARIABLES
    Vradar_dechirp__Syms* vlSymsp;
    const char* vlNamep;

    // CONSTRUCTORS
    Vradar_dechirp___024root(Vradar_dechirp__Syms* symsp, const char* namep);
    ~Vradar_dechirp___024root();
    VL_UNCOPYABLE(Vradar_dechirp___024root);

    // INTERNAL METHODS
    void __Vconfigure(bool first);
};


#endif  // guard
