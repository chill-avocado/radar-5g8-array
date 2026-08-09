// Verilated -*- C++ -*-
// DESCRIPTION: Verilator output: Design internal header
// See Vradar_decim4.h for the primary calling header

#ifndef VERILATED_VRADAR_DECIM4___024ROOT_H_
#define VERILATED_VRADAR_DECIM4___024ROOT_H_  // guard

#include "verilated.h"


class Vradar_decim4__Syms;

class alignas(VL_CACHE_LINE_BYTES) Vradar_decim4___024root final {
  public:

    // DESIGN SPECIFIC STATE
    VL_IN8(clk,0,0);
    VL_IN8(rst,0,0);
    VL_IN8(flush,0,0);
    VL_IN8(in_valid,0,0);
    VL_OUT8(out_valid,0,0);
    CData/*0:0*/ radar_decim4__DOT__clr;
    CData/*0:0*/ radar_decim4__DOT__mid_valid;
    CData/*0:0*/ radar_decim4__DOT__u_hb2__DOT__dphase;
    CData/*0:0*/ radar_decim4__DOT__u_hb2__DOT__due_s1;
    CData/*0:0*/ radar_decim4__DOT__u_hb2__DOT__v_s2;
    CData/*0:0*/ radar_decim4__DOT__u_hb2__DOT__v_s3;
    CData/*0:0*/ radar_decim4__DOT__u_hb1__DOT__dphase;
    CData/*0:0*/ radar_decim4__DOT__u_hb1__DOT__due_s1;
    CData/*0:0*/ radar_decim4__DOT__u_hb1__DOT__v_s2;
    CData/*0:0*/ radar_decim4__DOT__u_hb1__DOT__v_s3;
    CData/*0:0*/ __VstlFirstIteration;
    CData/*0:0*/ __VstlPhaseResult;
    CData/*0:0*/ __Vtrigprevexpr___TOP__clk__0;
    CData/*0:0*/ __Vtrigprevexpr___TOP__rst__0;
    CData/*0:0*/ __Vtrigprevexpr___TOP__flush__0;
    CData/*0:0*/ __Vtrigprevexpr___TOP__in_valid__0;
    CData/*0:0*/ __VicoDidInit;
    CData/*0:0*/ __VicoPhaseResult;
    CData/*0:0*/ __Vtrigprevexpr___TOP__clk__1;
    CData/*0:0*/ __VactPhaseResult;
    CData/*0:0*/ __VnbaPhaseResult;
    VL_IN16(in_i,15,0);
    VL_IN16(in_q,15,0);
    VL_OUT16(out_i,15,0);
    VL_OUT16(out_q,15,0);
    SData/*15:0*/ radar_decim4__DOT__mid_i;
    SData/*15:0*/ radar_decim4__DOT__mid_q;
    SData/*15:0*/ __Vtrigprevexpr___TOP__in_i__0;
    SData/*15:0*/ __Vtrigprevexpr___TOP__in_q__0;
    IData/*31:0*/ __VactIterCount;
    QData/*39:0*/ radar_decim4__DOT__u_hb2__DOT__acc_i_s3;
    QData/*39:0*/ radar_decim4__DOT__u_hb2__DOT__acc_q_s3;
    QData/*39:0*/ radar_decim4__DOT__u_hb1__DOT__acc_i_s3;
    QData/*39:0*/ radar_decim4__DOT__u_hb1__DOT__acc_q_s3;
    VlUnpacked<IData/*17:0*/, 10> radar_decim4__DOT__u_hb2__DOT__coef;
    VlUnpacked<SData/*15:0*/, 35> radar_decim4__DOT__u_hb2__DOT__dl_i;
    VlUnpacked<SData/*15:0*/, 35> radar_decim4__DOT__u_hb2__DOT__dl_q;
    VlUnpacked<IData/*16:0*/, 10> radar_decim4__DOT__u_hb2__DOT__psum_i;
    VlUnpacked<IData/*16:0*/, 10> radar_decim4__DOT__u_hb2__DOT__psum_q;
    VlUnpacked<QData/*34:0*/, 10> radar_decim4__DOT__u_hb2__DOT__pi_s2;
    VlUnpacked<QData/*34:0*/, 10> radar_decim4__DOT__u_hb2__DOT__pq_s2;
    VlUnpacked<QData/*39:0*/, 10> radar_decim4__DOT__u_hb2__DOT__chain_i;
    VlUnpacked<QData/*39:0*/, 10> radar_decim4__DOT__u_hb2__DOT__chain_q;
    VlUnpacked<IData/*17:0*/, 7> radar_decim4__DOT__u_hb1__DOT__coef;
    VlUnpacked<SData/*15:0*/, 23> radar_decim4__DOT__u_hb1__DOT__dl_i;
    VlUnpacked<SData/*15:0*/, 23> radar_decim4__DOT__u_hb1__DOT__dl_q;
    VlUnpacked<IData/*16:0*/, 7> radar_decim4__DOT__u_hb1__DOT__psum_i;
    VlUnpacked<IData/*16:0*/, 7> radar_decim4__DOT__u_hb1__DOT__psum_q;
    VlUnpacked<QData/*34:0*/, 7> radar_decim4__DOT__u_hb1__DOT__pi_s2;
    VlUnpacked<QData/*34:0*/, 7> radar_decim4__DOT__u_hb1__DOT__pq_s2;
    VlUnpacked<QData/*39:0*/, 7> radar_decim4__DOT__u_hb1__DOT__chain_i;
    VlUnpacked<QData/*39:0*/, 7> radar_decim4__DOT__u_hb1__DOT__chain_q;
    VlUnpacked<QData/*63:0*/, 1> __VstlTriggered;
    VlUnpacked<QData/*63:0*/, 2> __VicoTriggered;
    VlUnpacked<QData/*63:0*/, 1> __VactTriggered;
    VlUnpacked<QData/*63:0*/, 1> __VnbaTriggered;

    // INTERNAL VARIABLES
    Vradar_decim4__Syms* vlSymsp;
    const char* vlNamep;

    // CONSTRUCTORS
    Vradar_decim4___024root(Vradar_decim4__Syms* symsp, const char* namep);
    ~Vradar_decim4___024root();
    VL_UNCOPYABLE(Vradar_decim4___024root);

    // INTERNAL METHODS
    void __Vconfigure(bool first);
};


#endif  // guard
