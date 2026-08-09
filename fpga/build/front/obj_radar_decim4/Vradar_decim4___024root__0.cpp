// Verilated -*- C++ -*-
// DESCRIPTION: Verilator output: Design implementation internals
// See Vradar_decim4.h for the primary calling header

#include "Vradar_decim4__pch.h"

bool Vradar_decim4___024root___trigger_anySet__ico(const VlUnpacked<QData/*63:0*/, 2> &in) {
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vradar_decim4___024root___trigger_anySet__ico\n"); );
    // Locals
    IData/*31:0*/ n;
    // Body
    n = 0U;
    do {
        if (in[n]) {
            return (1U);
        }
        n = ((IData)(1U) + n);
    } while ((2U > n));
    return (0U);
}

#ifdef VL_DEBUG
VL_ATTR_COLD void Vradar_decim4___024root___dump_triggers__ico(const VlUnpacked<QData/*63:0*/, 2> &triggers, const std::string &tag);
#endif  // VL_DEBUG

bool Vradar_decim4___024root___eval_phase__ico(Vradar_decim4___024root* vlSelf) {
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vradar_decim4___024root___eval_phase__ico\n"); );
    Vradar_decim4__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    auto& vlSelfRef = std::ref(*vlSelf).get();
    // Locals
    CData/*0:0*/ __VicoExecute;
    // Body
    {
        // Inlined CFunc: _eval_triggers_vec__ico
        vlSelfRef.__VicoTriggered[0U] = (QData)((IData)(
                                                        (((((IData)(vlSelfRef.in_q) 
                                                            != (IData)(vlSelfRef.__Vtrigprevexpr___TOP__in_q__0)) 
                                                           << 5U) 
                                                          | (((IData)(vlSelfRef.in_i) 
                                                              != (IData)(vlSelfRef.__Vtrigprevexpr___TOP__in_i__0)) 
                                                             << 4U)) 
                                                         | (((((IData)(vlSelfRef.in_valid) 
                                                               != (IData)(vlSelfRef.__Vtrigprevexpr___TOP__in_valid__0)) 
                                                              << 3U) 
                                                             | (((IData)(vlSelfRef.flush) 
                                                                 != (IData)(vlSelfRef.__Vtrigprevexpr___TOP__flush__0)) 
                                                                << 2U)) 
                                                            | ((((IData)(vlSelfRef.rst) 
                                                                 != (IData)(vlSelfRef.__Vtrigprevexpr___TOP__rst__0)) 
                                                                << 1U) 
                                                               | ((IData)(vlSelfRef.clk) 
                                                                  != (IData)(vlSelfRef.__Vtrigprevexpr___TOP__clk__0)))))));
        vlSelfRef.__Vtrigprevexpr___TOP__clk__0 = vlSelfRef.clk;
        vlSelfRef.__Vtrigprevexpr___TOP__rst__0 = vlSelfRef.rst;
        vlSelfRef.__Vtrigprevexpr___TOP__flush__0 = vlSelfRef.flush;
        vlSelfRef.__Vtrigprevexpr___TOP__in_valid__0 
            = vlSelfRef.in_valid;
        vlSelfRef.__Vtrigprevexpr___TOP__in_i__0 = vlSelfRef.in_i;
        vlSelfRef.__Vtrigprevexpr___TOP__in_q__0 = vlSelfRef.in_q;
        if (VL_UNLIKELY(((1U & (~ (IData)(vlSelfRef.__VicoDidInit)))))) {
            vlSelfRef.__VicoDidInit = 1U;
            vlSelfRef.__VicoTriggered[0U] = (1ULL | vlSelfRef.__VicoTriggered[0U]);
            vlSelfRef.__VicoTriggered[0U] = (2ULL | vlSelfRef.__VicoTriggered[0U]);
            vlSelfRef.__VicoTriggered[0U] = (4ULL | vlSelfRef.__VicoTriggered[0U]);
            vlSelfRef.__VicoTriggered[0U] = (8ULL | vlSelfRef.__VicoTriggered[0U]);
            vlSelfRef.__VicoTriggered[0U] = (0x0000000000000010ULL 
                                             | vlSelfRef.__VicoTriggered[0U]);
            vlSelfRef.__VicoTriggered[0U] = (0x0000000000000020ULL 
                                             | vlSelfRef.__VicoTriggered[0U]);
        }
    }
#ifdef VL_DEBUG
    if (VL_UNLIKELY(vlSymsp->_vm_contextp__->debug())) {
        Vradar_decim4___024root___dump_triggers__ico(vlSelfRef.__VicoTriggered, "ico"s);
    }
#endif
    __VicoExecute = Vradar_decim4___024root___trigger_anySet__ico(vlSelfRef.__VicoTriggered);
    if (__VicoExecute) {
        {
            // Inlined CFunc: _eval_ico
            if ((6ULL & vlSelfRef.__VicoTriggered[0U])) {
                {
                    // Inlined CFunc: _ico_comb__TOP__0
                    vlSelfRef.radar_decim4__DOT__clr 
                        = ((IData)(vlSelfRef.rst) | (IData)(vlSelfRef.flush));
                }
            }
        }
    }
    return (__VicoExecute);
}

bool Vradar_decim4___024root___trigger_anySet__act(const VlUnpacked<QData/*63:0*/, 1> &in) {
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vradar_decim4___024root___trigger_anySet__act\n"); );
    // Locals
    IData/*31:0*/ n;
    // Body
    n = 0U;
    do {
        if (in[n]) {
            return (1U);
        }
        n = ((IData)(1U) + n);
    } while ((1U > n));
    return (0U);
}

void Vradar_decim4___024root___nba_sequent__TOP__0(Vradar_decim4___024root* vlSelf) {
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vradar_decim4___024root___nba_sequent__TOP__0\n"); );
    Vradar_decim4__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    auto& vlSelfRef = std::ref(*vlSelf).get();
    // Locals
    QData/*39:0*/ radar_decim4__DOT__u_hb2__DOT__round_sat__Vstatic__t;
    radar_decim4__DOT__u_hb2__DOT__round_sat__Vstatic__t = 0;
    QData/*39:0*/ radar_decim4__DOT__u_hb2__DOT__g_acc__BRA__0__KET____DOT__ext_i;
    radar_decim4__DOT__u_hb2__DOT__g_acc__BRA__0__KET____DOT__ext_i = 0;
    QData/*39:0*/ radar_decim4__DOT__u_hb2__DOT__g_acc__BRA__0__KET____DOT__ext_q;
    radar_decim4__DOT__u_hb2__DOT__g_acc__BRA__0__KET____DOT__ext_q = 0;
    QData/*39:0*/ radar_decim4__DOT__u_hb1__DOT__round_sat__Vstatic__t;
    radar_decim4__DOT__u_hb1__DOT__round_sat__Vstatic__t = 0;
    QData/*39:0*/ radar_decim4__DOT__u_hb1__DOT__g_acc__BRA__0__KET____DOT__ext_i;
    radar_decim4__DOT__u_hb1__DOT__g_acc__BRA__0__KET____DOT__ext_i = 0;
    QData/*39:0*/ radar_decim4__DOT__u_hb1__DOT__g_acc__BRA__0__KET____DOT__ext_q;
    radar_decim4__DOT__u_hb1__DOT__g_acc__BRA__0__KET____DOT__ext_q = 0;
    SData/*15:0*/ __Vfunc_radar_decim4__DOT__u_hb2__DOT__round_sat__2__Vfuncout;
    __Vfunc_radar_decim4__DOT__u_hb2__DOT__round_sat__2__Vfuncout = 0;
    QData/*39:0*/ __Vfunc_radar_decim4__DOT__u_hb2__DOT__round_sat__2__v;
    __Vfunc_radar_decim4__DOT__u_hb2__DOT__round_sat__2__v = 0;
    SData/*15:0*/ __Vfunc_radar_decim4__DOT__u_hb2__DOT__round_sat__3__Vfuncout;
    __Vfunc_radar_decim4__DOT__u_hb2__DOT__round_sat__3__Vfuncout = 0;
    QData/*39:0*/ __Vfunc_radar_decim4__DOT__u_hb2__DOT__round_sat__3__v;
    __Vfunc_radar_decim4__DOT__u_hb2__DOT__round_sat__3__v = 0;
    SData/*15:0*/ __Vfunc_radar_decim4__DOT__u_hb1__DOT__round_sat__6__Vfuncout;
    __Vfunc_radar_decim4__DOT__u_hb1__DOT__round_sat__6__Vfuncout = 0;
    QData/*39:0*/ __Vfunc_radar_decim4__DOT__u_hb1__DOT__round_sat__6__v;
    __Vfunc_radar_decim4__DOT__u_hb1__DOT__round_sat__6__v = 0;
    SData/*15:0*/ __Vfunc_radar_decim4__DOT__u_hb1__DOT__round_sat__7__Vfuncout;
    __Vfunc_radar_decim4__DOT__u_hb1__DOT__round_sat__7__Vfuncout = 0;
    QData/*39:0*/ __Vfunc_radar_decim4__DOT__u_hb1__DOT__round_sat__7__v;
    __Vfunc_radar_decim4__DOT__u_hb1__DOT__round_sat__7__v = 0;
    QData/*39:0*/ __VdfgRegularize_hebeb780c_0_0;
    __VdfgRegularize_hebeb780c_0_0 = 0;
    QData/*39:0*/ __VdfgRegularize_hebeb780c_0_1;
    __VdfgRegularize_hebeb780c_0_1 = 0;
    QData/*39:0*/ __VdfgRegularize_hebeb780c_0_2;
    __VdfgRegularize_hebeb780c_0_2 = 0;
    QData/*39:0*/ __VdfgRegularize_hebeb780c_0_3;
    __VdfgRegularize_hebeb780c_0_3 = 0;
    QData/*39:0*/ __VdfgRegularize_hebeb780c_0_4;
    __VdfgRegularize_hebeb780c_0_4 = 0;
    QData/*39:0*/ __VdfgRegularize_hebeb780c_0_5;
    __VdfgRegularize_hebeb780c_0_5 = 0;
    QData/*39:0*/ __VdfgRegularize_hebeb780c_0_6;
    __VdfgRegularize_hebeb780c_0_6 = 0;
    QData/*39:0*/ __VdfgRegularize_hebeb780c_0_7;
    __VdfgRegularize_hebeb780c_0_7 = 0;
    QData/*39:0*/ __VdfgRegularize_hebeb780c_0_8;
    __VdfgRegularize_hebeb780c_0_8 = 0;
    QData/*39:0*/ __VdfgRegularize_hebeb780c_0_9;
    __VdfgRegularize_hebeb780c_0_9 = 0;
    QData/*39:0*/ __VdfgRegularize_hebeb780c_0_10;
    __VdfgRegularize_hebeb780c_0_10 = 0;
    QData/*39:0*/ __VdfgRegularize_hebeb780c_0_11;
    __VdfgRegularize_hebeb780c_0_11 = 0;
    QData/*39:0*/ __VdfgRegularize_hebeb780c_0_12;
    __VdfgRegularize_hebeb780c_0_12 = 0;
    QData/*39:0*/ __VdfgRegularize_hebeb780c_0_13;
    __VdfgRegularize_hebeb780c_0_13 = 0;
    QData/*39:0*/ __VdfgRegularize_hebeb780c_0_14;
    __VdfgRegularize_hebeb780c_0_14 = 0;
    QData/*39:0*/ __VdfgRegularize_hebeb780c_0_15;
    __VdfgRegularize_hebeb780c_0_15 = 0;
    QData/*39:0*/ __VdfgRegularize_hebeb780c_0_16;
    __VdfgRegularize_hebeb780c_0_16 = 0;
    QData/*39:0*/ __VdfgRegularize_hebeb780c_0_17;
    __VdfgRegularize_hebeb780c_0_17 = 0;
    QData/*39:0*/ __VdfgRegularize_hebeb780c_0_18;
    __VdfgRegularize_hebeb780c_0_18 = 0;
    QData/*39:0*/ __VdfgRegularize_hebeb780c_0_19;
    __VdfgRegularize_hebeb780c_0_19 = 0;
    QData/*39:0*/ __VdfgRegularize_hebeb780c_0_20;
    __VdfgRegularize_hebeb780c_0_20 = 0;
    QData/*39:0*/ __VdfgRegularize_hebeb780c_0_21;
    __VdfgRegularize_hebeb780c_0_21 = 0;
    QData/*39:0*/ __VdfgRegularize_hebeb780c_0_22;
    __VdfgRegularize_hebeb780c_0_22 = 0;
    QData/*39:0*/ __VdfgRegularize_hebeb780c_0_23;
    __VdfgRegularize_hebeb780c_0_23 = 0;
    QData/*39:0*/ __VdfgRegularize_hebeb780c_0_24;
    __VdfgRegularize_hebeb780c_0_24 = 0;
    QData/*39:0*/ __VdfgRegularize_hebeb780c_0_25;
    __VdfgRegularize_hebeb780c_0_25 = 0;
    CData/*0:0*/ __Vdly__radar_decim4__DOT__u_hb2__DOT__dphase;
    __Vdly__radar_decim4__DOT__u_hb2__DOT__dphase = 0;
    CData/*0:0*/ __Vdly__radar_decim4__DOT__u_hb1__DOT__dphase;
    __Vdly__radar_decim4__DOT__u_hb1__DOT__dphase = 0;
    CData/*0:0*/ __VdlySet__radar_decim4__DOT__u_hb2__DOT__dl_q__v0;
    __VdlySet__radar_decim4__DOT__u_hb2__DOT__dl_q__v0 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v35;
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v35 = 0;
    CData/*0:0*/ __VdlySet__radar_decim4__DOT__u_hb2__DOT__dl_q__v35;
    __VdlySet__radar_decim4__DOT__u_hb2__DOT__dl_q__v35 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v36;
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v36 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v37;
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v37 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v38;
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v38 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v39;
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v39 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v40;
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v40 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v41;
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v41 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v42;
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v42 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v43;
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v43 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v44;
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v44 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v45;
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v45 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v46;
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v46 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v47;
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v47 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v48;
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v48 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v49;
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v49 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v50;
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v50 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v51;
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v51 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v52;
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v52 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v53;
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v53 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v54;
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v54 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v55;
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v55 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v56;
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v56 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v57;
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v57 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v58;
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v58 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v59;
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v59 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v60;
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v60 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v61;
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v61 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v62;
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v62 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v63;
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v63 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v64;
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v64 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v65;
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v65 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v66;
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v66 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v67;
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v67 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v68;
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v68 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v69;
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v69 = 0;
    CData/*0:0*/ __VdlySet__radar_decim4__DOT__u_hb2__DOT__dl_i__v0;
    __VdlySet__radar_decim4__DOT__u_hb2__DOT__dl_i__v0 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v35;
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v35 = 0;
    CData/*0:0*/ __VdlySet__radar_decim4__DOT__u_hb2__DOT__dl_i__v35;
    __VdlySet__radar_decim4__DOT__u_hb2__DOT__dl_i__v35 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v36;
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v36 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v37;
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v37 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v38;
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v38 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v39;
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v39 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v40;
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v40 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v41;
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v41 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v42;
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v42 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v43;
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v43 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v44;
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v44 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v45;
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v45 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v46;
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v46 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v47;
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v47 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v48;
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v48 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v49;
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v49 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v50;
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v50 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v51;
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v51 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v52;
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v52 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v53;
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v53 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v54;
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v54 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v55;
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v55 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v56;
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v56 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v57;
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v57 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v58;
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v58 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v59;
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v59 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v60;
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v60 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v61;
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v61 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v62;
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v62 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v63;
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v63 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v64;
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v64 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v65;
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v65 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v66;
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v66 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v67;
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v67 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v68;
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v68 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v69;
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v69 = 0;
    QData/*34:0*/ __VdlyVal__radar_decim4__DOT__u_hb2__DOT__pi_s2__v0;
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__pi_s2__v0 = 0;
    QData/*34:0*/ __VdlyVal__radar_decim4__DOT__u_hb2__DOT__pi_s2__v1;
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__pi_s2__v1 = 0;
    QData/*34:0*/ __VdlyVal__radar_decim4__DOT__u_hb2__DOT__pi_s2__v2;
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__pi_s2__v2 = 0;
    QData/*34:0*/ __VdlyVal__radar_decim4__DOT__u_hb2__DOT__pi_s2__v3;
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__pi_s2__v3 = 0;
    QData/*34:0*/ __VdlyVal__radar_decim4__DOT__u_hb2__DOT__pi_s2__v4;
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__pi_s2__v4 = 0;
    QData/*34:0*/ __VdlyVal__radar_decim4__DOT__u_hb2__DOT__pi_s2__v5;
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__pi_s2__v5 = 0;
    QData/*34:0*/ __VdlyVal__radar_decim4__DOT__u_hb2__DOT__pi_s2__v6;
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__pi_s2__v6 = 0;
    QData/*34:0*/ __VdlyVal__radar_decim4__DOT__u_hb2__DOT__pi_s2__v7;
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__pi_s2__v7 = 0;
    QData/*34:0*/ __VdlyVal__radar_decim4__DOT__u_hb2__DOT__pi_s2__v8;
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__pi_s2__v8 = 0;
    QData/*34:0*/ __VdlyVal__radar_decim4__DOT__u_hb2__DOT__pi_s2__v9;
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__pi_s2__v9 = 0;
    QData/*34:0*/ __VdlyVal__radar_decim4__DOT__u_hb2__DOT__pq_s2__v0;
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__pq_s2__v0 = 0;
    QData/*34:0*/ __VdlyVal__radar_decim4__DOT__u_hb2__DOT__pq_s2__v1;
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__pq_s2__v1 = 0;
    QData/*34:0*/ __VdlyVal__radar_decim4__DOT__u_hb2__DOT__pq_s2__v2;
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__pq_s2__v2 = 0;
    QData/*34:0*/ __VdlyVal__radar_decim4__DOT__u_hb2__DOT__pq_s2__v3;
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__pq_s2__v3 = 0;
    QData/*34:0*/ __VdlyVal__radar_decim4__DOT__u_hb2__DOT__pq_s2__v4;
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__pq_s2__v4 = 0;
    QData/*34:0*/ __VdlyVal__radar_decim4__DOT__u_hb2__DOT__pq_s2__v5;
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__pq_s2__v5 = 0;
    QData/*34:0*/ __VdlyVal__radar_decim4__DOT__u_hb2__DOT__pq_s2__v6;
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__pq_s2__v6 = 0;
    QData/*34:0*/ __VdlyVal__radar_decim4__DOT__u_hb2__DOT__pq_s2__v7;
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__pq_s2__v7 = 0;
    QData/*34:0*/ __VdlyVal__radar_decim4__DOT__u_hb2__DOT__pq_s2__v8;
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__pq_s2__v8 = 0;
    QData/*34:0*/ __VdlyVal__radar_decim4__DOT__u_hb2__DOT__pq_s2__v9;
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__pq_s2__v9 = 0;
    CData/*0:0*/ __VdlySet__radar_decim4__DOT__u_hb1__DOT__dl_q__v0;
    __VdlySet__radar_decim4__DOT__u_hb1__DOT__dl_q__v0 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_q__v23;
    __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_q__v23 = 0;
    CData/*0:0*/ __VdlySet__radar_decim4__DOT__u_hb1__DOT__dl_q__v23;
    __VdlySet__radar_decim4__DOT__u_hb1__DOT__dl_q__v23 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_q__v24;
    __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_q__v24 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_q__v25;
    __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_q__v25 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_q__v26;
    __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_q__v26 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_q__v27;
    __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_q__v27 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_q__v28;
    __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_q__v28 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_q__v29;
    __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_q__v29 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_q__v30;
    __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_q__v30 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_q__v31;
    __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_q__v31 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_q__v32;
    __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_q__v32 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_q__v33;
    __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_q__v33 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_q__v34;
    __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_q__v34 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_q__v35;
    __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_q__v35 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_q__v36;
    __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_q__v36 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_q__v37;
    __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_q__v37 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_q__v38;
    __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_q__v38 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_q__v39;
    __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_q__v39 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_q__v40;
    __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_q__v40 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_q__v41;
    __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_q__v41 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_q__v42;
    __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_q__v42 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_q__v43;
    __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_q__v43 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_q__v44;
    __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_q__v44 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_q__v45;
    __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_q__v45 = 0;
    CData/*0:0*/ __VdlySet__radar_decim4__DOT__u_hb1__DOT__dl_i__v0;
    __VdlySet__radar_decim4__DOT__u_hb1__DOT__dl_i__v0 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_i__v23;
    __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_i__v23 = 0;
    CData/*0:0*/ __VdlySet__radar_decim4__DOT__u_hb1__DOT__dl_i__v23;
    __VdlySet__radar_decim4__DOT__u_hb1__DOT__dl_i__v23 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_i__v24;
    __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_i__v24 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_i__v25;
    __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_i__v25 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_i__v26;
    __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_i__v26 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_i__v27;
    __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_i__v27 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_i__v28;
    __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_i__v28 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_i__v29;
    __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_i__v29 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_i__v30;
    __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_i__v30 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_i__v31;
    __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_i__v31 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_i__v32;
    __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_i__v32 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_i__v33;
    __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_i__v33 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_i__v34;
    __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_i__v34 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_i__v35;
    __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_i__v35 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_i__v36;
    __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_i__v36 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_i__v37;
    __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_i__v37 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_i__v38;
    __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_i__v38 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_i__v39;
    __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_i__v39 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_i__v40;
    __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_i__v40 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_i__v41;
    __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_i__v41 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_i__v42;
    __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_i__v42 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_i__v43;
    __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_i__v43 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_i__v44;
    __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_i__v44 = 0;
    SData/*15:0*/ __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_i__v45;
    __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_i__v45 = 0;
    QData/*34:0*/ __VdlyVal__radar_decim4__DOT__u_hb1__DOT__pi_s2__v0;
    __VdlyVal__radar_decim4__DOT__u_hb1__DOT__pi_s2__v0 = 0;
    QData/*34:0*/ __VdlyVal__radar_decim4__DOT__u_hb1__DOT__pi_s2__v1;
    __VdlyVal__radar_decim4__DOT__u_hb1__DOT__pi_s2__v1 = 0;
    QData/*34:0*/ __VdlyVal__radar_decim4__DOT__u_hb1__DOT__pi_s2__v2;
    __VdlyVal__radar_decim4__DOT__u_hb1__DOT__pi_s2__v2 = 0;
    QData/*34:0*/ __VdlyVal__radar_decim4__DOT__u_hb1__DOT__pi_s2__v3;
    __VdlyVal__radar_decim4__DOT__u_hb1__DOT__pi_s2__v3 = 0;
    QData/*34:0*/ __VdlyVal__radar_decim4__DOT__u_hb1__DOT__pi_s2__v4;
    __VdlyVal__radar_decim4__DOT__u_hb1__DOT__pi_s2__v4 = 0;
    QData/*34:0*/ __VdlyVal__radar_decim4__DOT__u_hb1__DOT__pi_s2__v5;
    __VdlyVal__radar_decim4__DOT__u_hb1__DOT__pi_s2__v5 = 0;
    QData/*34:0*/ __VdlyVal__radar_decim4__DOT__u_hb1__DOT__pi_s2__v6;
    __VdlyVal__radar_decim4__DOT__u_hb1__DOT__pi_s2__v6 = 0;
    QData/*34:0*/ __VdlyVal__radar_decim4__DOT__u_hb1__DOT__pq_s2__v0;
    __VdlyVal__radar_decim4__DOT__u_hb1__DOT__pq_s2__v0 = 0;
    QData/*34:0*/ __VdlyVal__radar_decim4__DOT__u_hb1__DOT__pq_s2__v1;
    __VdlyVal__radar_decim4__DOT__u_hb1__DOT__pq_s2__v1 = 0;
    QData/*34:0*/ __VdlyVal__radar_decim4__DOT__u_hb1__DOT__pq_s2__v2;
    __VdlyVal__radar_decim4__DOT__u_hb1__DOT__pq_s2__v2 = 0;
    QData/*34:0*/ __VdlyVal__radar_decim4__DOT__u_hb1__DOT__pq_s2__v3;
    __VdlyVal__radar_decim4__DOT__u_hb1__DOT__pq_s2__v3 = 0;
    QData/*34:0*/ __VdlyVal__radar_decim4__DOT__u_hb1__DOT__pq_s2__v4;
    __VdlyVal__radar_decim4__DOT__u_hb1__DOT__pq_s2__v4 = 0;
    QData/*34:0*/ __VdlyVal__radar_decim4__DOT__u_hb1__DOT__pq_s2__v5;
    __VdlyVal__radar_decim4__DOT__u_hb1__DOT__pq_s2__v5 = 0;
    QData/*34:0*/ __VdlyVal__radar_decim4__DOT__u_hb1__DOT__pq_s2__v6;
    __VdlyVal__radar_decim4__DOT__u_hb1__DOT__pq_s2__v6 = 0;
    // Body
    __Vdly__radar_decim4__DOT__u_hb1__DOT__dphase = vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dphase;
    __Vdly__radar_decim4__DOT__u_hb2__DOT__dphase = vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dphase;
    __VdlySet__radar_decim4__DOT__u_hb1__DOT__dl_q__v0 = 0U;
    __VdlySet__radar_decim4__DOT__u_hb1__DOT__dl_q__v23 = 0U;
    __VdlySet__radar_decim4__DOT__u_hb1__DOT__dl_i__v0 = 0U;
    __VdlySet__radar_decim4__DOT__u_hb1__DOT__dl_i__v23 = 0U;
    __VdlySet__radar_decim4__DOT__u_hb2__DOT__dl_q__v0 = 0U;
    __VdlySet__radar_decim4__DOT__u_hb2__DOT__dl_q__v35 = 0U;
    __VdlySet__radar_decim4__DOT__u_hb2__DOT__dl_i__v0 = 0U;
    __VdlySet__radar_decim4__DOT__u_hb2__DOT__dl_i__v35 = 0U;
    __VdlyVal__radar_decim4__DOT__u_hb1__DOT__pi_s2__v0 
        = (0x00000007ffffffffULL & VL_MULS_QQQ(35, 
                                               (0x00000007ffffffffULL 
                                                & VL_EXTENDS_QI(35,17, vlSelfRef.radar_decim4__DOT__u_hb1__DOT__psum_i[0U])), 
                                               (0x00000007ffffffffULL 
                                                & VL_EXTENDS_QI(35,18, vlSelfRef.radar_decim4__DOT__u_hb1__DOT__coef[0U]))));
    __VdlyVal__radar_decim4__DOT__u_hb1__DOT__pi_s2__v1 
        = (0x00000007ffffffffULL & VL_MULS_QQQ(35, 
                                               (0x00000007ffffffffULL 
                                                & VL_EXTENDS_QI(35,17, vlSelfRef.radar_decim4__DOT__u_hb1__DOT__psum_i[1U])), 
                                               (0x00000007ffffffffULL 
                                                & VL_EXTENDS_QI(35,18, vlSelfRef.radar_decim4__DOT__u_hb1__DOT__coef[1U]))));
    __VdlyVal__radar_decim4__DOT__u_hb1__DOT__pi_s2__v2 
        = (0x00000007ffffffffULL & VL_MULS_QQQ(35, 
                                               (0x00000007ffffffffULL 
                                                & VL_EXTENDS_QI(35,17, vlSelfRef.radar_decim4__DOT__u_hb1__DOT__psum_i[2U])), 
                                               (0x00000007ffffffffULL 
                                                & VL_EXTENDS_QI(35,18, vlSelfRef.radar_decim4__DOT__u_hb1__DOT__coef[2U]))));
    __VdlyVal__radar_decim4__DOT__u_hb1__DOT__pi_s2__v3 
        = (0x00000007ffffffffULL & VL_MULS_QQQ(35, 
                                               (0x00000007ffffffffULL 
                                                & VL_EXTENDS_QI(35,17, vlSelfRef.radar_decim4__DOT__u_hb1__DOT__psum_i[3U])), 
                                               (0x00000007ffffffffULL 
                                                & VL_EXTENDS_QI(35,18, vlSelfRef.radar_decim4__DOT__u_hb1__DOT__coef[3U]))));
    __VdlyVal__radar_decim4__DOT__u_hb1__DOT__pi_s2__v4 
        = (0x00000007ffffffffULL & VL_MULS_QQQ(35, 
                                               (0x00000007ffffffffULL 
                                                & VL_EXTENDS_QI(35,17, vlSelfRef.radar_decim4__DOT__u_hb1__DOT__psum_i[4U])), 
                                               (0x00000007ffffffffULL 
                                                & VL_EXTENDS_QI(35,18, vlSelfRef.radar_decim4__DOT__u_hb1__DOT__coef[4U]))));
    __VdlyVal__radar_decim4__DOT__u_hb1__DOT__pi_s2__v5 
        = (0x00000007ffffffffULL & VL_MULS_QQQ(35, 
                                               (0x00000007ffffffffULL 
                                                & VL_EXTENDS_QI(35,17, vlSelfRef.radar_decim4__DOT__u_hb1__DOT__psum_i[5U])), 
                                               (0x00000007ffffffffULL 
                                                & VL_EXTENDS_QI(35,18, vlSelfRef.radar_decim4__DOT__u_hb1__DOT__coef[5U]))));
    __VdlyVal__radar_decim4__DOT__u_hb1__DOT__pi_s2__v6 
        = (0x00000007ffffffffULL & VL_MULS_QQQ(35, 
                                               (0x00000007ffffffffULL 
                                                & VL_EXTENDS_QI(35,17, vlSelfRef.radar_decim4__DOT__u_hb1__DOT__psum_i[6U])), 
                                               (0x00000007ffffffffULL 
                                                & VL_EXTENDS_QI(35,18, vlSelfRef.radar_decim4__DOT__u_hb1__DOT__coef[6U]))));
    __VdlyVal__radar_decim4__DOT__u_hb1__DOT__pq_s2__v0 
        = (0x00000007ffffffffULL & VL_MULS_QQQ(35, 
                                               (0x00000007ffffffffULL 
                                                & VL_EXTENDS_QI(35,17, vlSelfRef.radar_decim4__DOT__u_hb1__DOT__psum_q[0U])), 
                                               (0x00000007ffffffffULL 
                                                & VL_EXTENDS_QI(35,18, vlSelfRef.radar_decim4__DOT__u_hb1__DOT__coef[0U]))));
    __VdlyVal__radar_decim4__DOT__u_hb1__DOT__pq_s2__v1 
        = (0x00000007ffffffffULL & VL_MULS_QQQ(35, 
                                               (0x00000007ffffffffULL 
                                                & VL_EXTENDS_QI(35,17, vlSelfRef.radar_decim4__DOT__u_hb1__DOT__psum_q[1U])), 
                                               (0x00000007ffffffffULL 
                                                & VL_EXTENDS_QI(35,18, vlSelfRef.radar_decim4__DOT__u_hb1__DOT__coef[1U]))));
    __VdlyVal__radar_decim4__DOT__u_hb1__DOT__pq_s2__v2 
        = (0x00000007ffffffffULL & VL_MULS_QQQ(35, 
                                               (0x00000007ffffffffULL 
                                                & VL_EXTENDS_QI(35,17, vlSelfRef.radar_decim4__DOT__u_hb1__DOT__psum_q[2U])), 
                                               (0x00000007ffffffffULL 
                                                & VL_EXTENDS_QI(35,18, vlSelfRef.radar_decim4__DOT__u_hb1__DOT__coef[2U]))));
    __VdlyVal__radar_decim4__DOT__u_hb1__DOT__pq_s2__v3 
        = (0x00000007ffffffffULL & VL_MULS_QQQ(35, 
                                               (0x00000007ffffffffULL 
                                                & VL_EXTENDS_QI(35,17, vlSelfRef.radar_decim4__DOT__u_hb1__DOT__psum_q[3U])), 
                                               (0x00000007ffffffffULL 
                                                & VL_EXTENDS_QI(35,18, vlSelfRef.radar_decim4__DOT__u_hb1__DOT__coef[3U]))));
    __VdlyVal__radar_decim4__DOT__u_hb1__DOT__pq_s2__v4 
        = (0x00000007ffffffffULL & VL_MULS_QQQ(35, 
                                               (0x00000007ffffffffULL 
                                                & VL_EXTENDS_QI(35,17, vlSelfRef.radar_decim4__DOT__u_hb1__DOT__psum_q[4U])), 
                                               (0x00000007ffffffffULL 
                                                & VL_EXTENDS_QI(35,18, vlSelfRef.radar_decim4__DOT__u_hb1__DOT__coef[4U]))));
    __VdlyVal__radar_decim4__DOT__u_hb1__DOT__pq_s2__v5 
        = (0x00000007ffffffffULL & VL_MULS_QQQ(35, 
                                               (0x00000007ffffffffULL 
                                                & VL_EXTENDS_QI(35,17, vlSelfRef.radar_decim4__DOT__u_hb1__DOT__psum_q[5U])), 
                                               (0x00000007ffffffffULL 
                                                & VL_EXTENDS_QI(35,18, vlSelfRef.radar_decim4__DOT__u_hb1__DOT__coef[5U]))));
    __VdlyVal__radar_decim4__DOT__u_hb1__DOT__pq_s2__v6 
        = (0x00000007ffffffffULL & VL_MULS_QQQ(35, 
                                               (0x00000007ffffffffULL 
                                                & VL_EXTENDS_QI(35,17, vlSelfRef.radar_decim4__DOT__u_hb1__DOT__psum_q[6U])), 
                                               (0x00000007ffffffffULL 
                                                & VL_EXTENDS_QI(35,18, vlSelfRef.radar_decim4__DOT__u_hb1__DOT__coef[6U]))));
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__pi_s2__v0 
        = (0x00000007ffffffffULL & VL_MULS_QQQ(35, 
                                               (0x00000007ffffffffULL 
                                                & VL_EXTENDS_QI(35,17, vlSelfRef.radar_decim4__DOT__u_hb2__DOT__psum_i[0U])), 
                                               (0x00000007ffffffffULL 
                                                & VL_EXTENDS_QI(35,18, vlSelfRef.radar_decim4__DOT__u_hb2__DOT__coef[0U]))));
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__pi_s2__v1 
        = (0x00000007ffffffffULL & VL_MULS_QQQ(35, 
                                               (0x00000007ffffffffULL 
                                                & VL_EXTENDS_QI(35,17, vlSelfRef.radar_decim4__DOT__u_hb2__DOT__psum_i[1U])), 
                                               (0x00000007ffffffffULL 
                                                & VL_EXTENDS_QI(35,18, vlSelfRef.radar_decim4__DOT__u_hb2__DOT__coef[1U]))));
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__pi_s2__v2 
        = (0x00000007ffffffffULL & VL_MULS_QQQ(35, 
                                               (0x00000007ffffffffULL 
                                                & VL_EXTENDS_QI(35,17, vlSelfRef.radar_decim4__DOT__u_hb2__DOT__psum_i[2U])), 
                                               (0x00000007ffffffffULL 
                                                & VL_EXTENDS_QI(35,18, vlSelfRef.radar_decim4__DOT__u_hb2__DOT__coef[2U]))));
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__pi_s2__v3 
        = (0x00000007ffffffffULL & VL_MULS_QQQ(35, 
                                               (0x00000007ffffffffULL 
                                                & VL_EXTENDS_QI(35,17, vlSelfRef.radar_decim4__DOT__u_hb2__DOT__psum_i[3U])), 
                                               (0x00000007ffffffffULL 
                                                & VL_EXTENDS_QI(35,18, vlSelfRef.radar_decim4__DOT__u_hb2__DOT__coef[3U]))));
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__pi_s2__v4 
        = (0x00000007ffffffffULL & VL_MULS_QQQ(35, 
                                               (0x00000007ffffffffULL 
                                                & VL_EXTENDS_QI(35,17, vlSelfRef.radar_decim4__DOT__u_hb2__DOT__psum_i[4U])), 
                                               (0x00000007ffffffffULL 
                                                & VL_EXTENDS_QI(35,18, vlSelfRef.radar_decim4__DOT__u_hb2__DOT__coef[4U]))));
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__pi_s2__v5 
        = (0x00000007ffffffffULL & VL_MULS_QQQ(35, 
                                               (0x00000007ffffffffULL 
                                                & VL_EXTENDS_QI(35,17, vlSelfRef.radar_decim4__DOT__u_hb2__DOT__psum_i[5U])), 
                                               (0x00000007ffffffffULL 
                                                & VL_EXTENDS_QI(35,18, vlSelfRef.radar_decim4__DOT__u_hb2__DOT__coef[5U]))));
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__pi_s2__v6 
        = (0x00000007ffffffffULL & VL_MULS_QQQ(35, 
                                               (0x00000007ffffffffULL 
                                                & VL_EXTENDS_QI(35,17, vlSelfRef.radar_decim4__DOT__u_hb2__DOT__psum_i[6U])), 
                                               (0x00000007ffffffffULL 
                                                & VL_EXTENDS_QI(35,18, vlSelfRef.radar_decim4__DOT__u_hb2__DOT__coef[6U]))));
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__pi_s2__v7 
        = (0x00000007ffffffffULL & VL_MULS_QQQ(35, 
                                               (0x00000007ffffffffULL 
                                                & VL_EXTENDS_QI(35,17, vlSelfRef.radar_decim4__DOT__u_hb2__DOT__psum_i[7U])), 
                                               (0x00000007ffffffffULL 
                                                & VL_EXTENDS_QI(35,18, vlSelfRef.radar_decim4__DOT__u_hb2__DOT__coef[7U]))));
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__pi_s2__v8 
        = (0x00000007ffffffffULL & VL_MULS_QQQ(35, 
                                               (0x00000007ffffffffULL 
                                                & VL_EXTENDS_QI(35,17, vlSelfRef.radar_decim4__DOT__u_hb2__DOT__psum_i[8U])), 
                                               (0x00000007ffffffffULL 
                                                & VL_EXTENDS_QI(35,18, vlSelfRef.radar_decim4__DOT__u_hb2__DOT__coef[8U]))));
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__pi_s2__v9 
        = (0x00000007ffffffffULL & VL_MULS_QQQ(35, 
                                               (0x00000007ffffffffULL 
                                                & VL_EXTENDS_QI(35,17, vlSelfRef.radar_decim4__DOT__u_hb2__DOT__psum_i[9U])), 
                                               (0x00000007ffffffffULL 
                                                & VL_EXTENDS_QI(35,18, vlSelfRef.radar_decim4__DOT__u_hb2__DOT__coef[9U]))));
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__pq_s2__v0 
        = (0x00000007ffffffffULL & VL_MULS_QQQ(35, 
                                               (0x00000007ffffffffULL 
                                                & VL_EXTENDS_QI(35,17, vlSelfRef.radar_decim4__DOT__u_hb2__DOT__psum_q[0U])), 
                                               (0x00000007ffffffffULL 
                                                & VL_EXTENDS_QI(35,18, vlSelfRef.radar_decim4__DOT__u_hb2__DOT__coef[0U]))));
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__pq_s2__v1 
        = (0x00000007ffffffffULL & VL_MULS_QQQ(35, 
                                               (0x00000007ffffffffULL 
                                                & VL_EXTENDS_QI(35,17, vlSelfRef.radar_decim4__DOT__u_hb2__DOT__psum_q[1U])), 
                                               (0x00000007ffffffffULL 
                                                & VL_EXTENDS_QI(35,18, vlSelfRef.radar_decim4__DOT__u_hb2__DOT__coef[1U]))));
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__pq_s2__v2 
        = (0x00000007ffffffffULL & VL_MULS_QQQ(35, 
                                               (0x00000007ffffffffULL 
                                                & VL_EXTENDS_QI(35,17, vlSelfRef.radar_decim4__DOT__u_hb2__DOT__psum_q[2U])), 
                                               (0x00000007ffffffffULL 
                                                & VL_EXTENDS_QI(35,18, vlSelfRef.radar_decim4__DOT__u_hb2__DOT__coef[2U]))));
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__pq_s2__v3 
        = (0x00000007ffffffffULL & VL_MULS_QQQ(35, 
                                               (0x00000007ffffffffULL 
                                                & VL_EXTENDS_QI(35,17, vlSelfRef.radar_decim4__DOT__u_hb2__DOT__psum_q[3U])), 
                                               (0x00000007ffffffffULL 
                                                & VL_EXTENDS_QI(35,18, vlSelfRef.radar_decim4__DOT__u_hb2__DOT__coef[3U]))));
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__pq_s2__v4 
        = (0x00000007ffffffffULL & VL_MULS_QQQ(35, 
                                               (0x00000007ffffffffULL 
                                                & VL_EXTENDS_QI(35,17, vlSelfRef.radar_decim4__DOT__u_hb2__DOT__psum_q[4U])), 
                                               (0x00000007ffffffffULL 
                                                & VL_EXTENDS_QI(35,18, vlSelfRef.radar_decim4__DOT__u_hb2__DOT__coef[4U]))));
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__pq_s2__v5 
        = (0x00000007ffffffffULL & VL_MULS_QQQ(35, 
                                               (0x00000007ffffffffULL 
                                                & VL_EXTENDS_QI(35,17, vlSelfRef.radar_decim4__DOT__u_hb2__DOT__psum_q[5U])), 
                                               (0x00000007ffffffffULL 
                                                & VL_EXTENDS_QI(35,18, vlSelfRef.radar_decim4__DOT__u_hb2__DOT__coef[5U]))));
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__pq_s2__v6 
        = (0x00000007ffffffffULL & VL_MULS_QQQ(35, 
                                               (0x00000007ffffffffULL 
                                                & VL_EXTENDS_QI(35,17, vlSelfRef.radar_decim4__DOT__u_hb2__DOT__psum_q[6U])), 
                                               (0x00000007ffffffffULL 
                                                & VL_EXTENDS_QI(35,18, vlSelfRef.radar_decim4__DOT__u_hb2__DOT__coef[6U]))));
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__pq_s2__v7 
        = (0x00000007ffffffffULL & VL_MULS_QQQ(35, 
                                               (0x00000007ffffffffULL 
                                                & VL_EXTENDS_QI(35,17, vlSelfRef.radar_decim4__DOT__u_hb2__DOT__psum_q[7U])), 
                                               (0x00000007ffffffffULL 
                                                & VL_EXTENDS_QI(35,18, vlSelfRef.radar_decim4__DOT__u_hb2__DOT__coef[7U]))));
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__pq_s2__v8 
        = (0x00000007ffffffffULL & VL_MULS_QQQ(35, 
                                               (0x00000007ffffffffULL 
                                                & VL_EXTENDS_QI(35,17, vlSelfRef.radar_decim4__DOT__u_hb2__DOT__psum_q[8U])), 
                                               (0x00000007ffffffffULL 
                                                & VL_EXTENDS_QI(35,18, vlSelfRef.radar_decim4__DOT__u_hb2__DOT__coef[8U]))));
    __VdlyVal__radar_decim4__DOT__u_hb2__DOT__pq_s2__v9 
        = (0x00000007ffffffffULL & VL_MULS_QQQ(35, 
                                               (0x00000007ffffffffULL 
                                                & VL_EXTENDS_QI(35,17, vlSelfRef.radar_decim4__DOT__u_hb2__DOT__psum_q[9U])), 
                                               (0x00000007ffffffffULL 
                                                & VL_EXTENDS_QI(35,18, vlSelfRef.radar_decim4__DOT__u_hb2__DOT__coef[9U]))));
    __Vfunc_radar_decim4__DOT__u_hb2__DOT__round_sat__2__v 
        = vlSelfRef.radar_decim4__DOT__u_hb2__DOT__acc_i_s3;
    radar_decim4__DOT__u_hb2__DOT__round_sat__Vstatic__t 
        = (0x000000ffffffffffULL & VL_SHIFTRS_QQI(40,40,32, 
                                                  (0x000000ffffffffffULL 
                                                   & (0x0000000000010000ULL 
                                                      + __Vfunc_radar_decim4__DOT__u_hb2__DOT__round_sat__2__v)), 0x00000011U));
    __Vfunc_radar_decim4__DOT__u_hb2__DOT__round_sat__2__Vfuncout 
        = (VL_LTS_IQQ(40, 0x0000000000007fffULL, radar_decim4__DOT__u_hb2__DOT__round_sat__Vstatic__t)
            ? 0x00007fffU : (VL_GTS_IQQ(40, 0x000000ffffff8000ULL, radar_decim4__DOT__u_hb2__DOT__round_sat__Vstatic__t)
                              ? 0x00008000U : (0x0000ffffU 
                                               & (IData)(radar_decim4__DOT__u_hb2__DOT__round_sat__Vstatic__t))));
    vlSelfRef.out_i = __Vfunc_radar_decim4__DOT__u_hb2__DOT__round_sat__2__Vfuncout;
    __Vfunc_radar_decim4__DOT__u_hb2__DOT__round_sat__3__v 
        = vlSelfRef.radar_decim4__DOT__u_hb2__DOT__acc_q_s3;
    radar_decim4__DOT__u_hb2__DOT__round_sat__Vstatic__t 
        = (0x000000ffffffffffULL & VL_SHIFTRS_QQI(40,40,32, 
                                                  (0x000000ffffffffffULL 
                                                   & (0x0000000000010000ULL 
                                                      + __Vfunc_radar_decim4__DOT__u_hb2__DOT__round_sat__3__v)), 0x00000011U));
    __Vfunc_radar_decim4__DOT__u_hb2__DOT__round_sat__3__Vfuncout 
        = (VL_LTS_IQQ(40, 0x0000000000007fffULL, radar_decim4__DOT__u_hb2__DOT__round_sat__Vstatic__t)
            ? 0x00007fffU : (VL_GTS_IQQ(40, 0x000000ffffff8000ULL, radar_decim4__DOT__u_hb2__DOT__round_sat__Vstatic__t)
                              ? 0x00008000U : (0x0000ffffU 
                                               & (IData)(radar_decim4__DOT__u_hb2__DOT__round_sat__Vstatic__t))));
    vlSelfRef.out_q = __Vfunc_radar_decim4__DOT__u_hb2__DOT__round_sat__3__Vfuncout;
    vlSelfRef.out_valid = ((~ (IData)(vlSelfRef.radar_decim4__DOT__clr)) 
                           & (IData)(vlSelfRef.radar_decim4__DOT__u_hb2__DOT__v_s3));
    if (vlSelfRef.radar_decim4__DOT__clr) {
        __VdlySet__radar_decim4__DOT__u_hb1__DOT__dl_q__v0 = 1U;
    } else if (vlSelfRef.in_valid) {
        __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_q__v23 
            = vlSelfRef.in_q;
        __VdlySet__radar_decim4__DOT__u_hb1__DOT__dl_q__v23 = 1U;
        __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_q__v24 
            = vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_q[0U];
        __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_q__v25 
            = vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_q[1U];
        __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_q__v26 
            = vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_q[2U];
        __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_q__v27 
            = vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_q[3U];
        __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_q__v28 
            = vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_q[4U];
        __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_q__v29 
            = vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_q[5U];
        __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_q__v30 
            = vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_q[6U];
        __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_q__v31 
            = vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_q[7U];
        __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_q__v32 
            = vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_q[8U];
        __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_q__v33 
            = vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_q[9U];
        __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_q__v34 
            = vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_q[10U];
        __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_q__v35 
            = vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_q[11U];
        __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_q__v36 
            = vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_q[12U];
        __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_q__v37 
            = vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_q[13U];
        __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_q__v38 
            = vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_q[14U];
        __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_q__v39 
            = vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_q[15U];
        __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_q__v40 
            = vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_q[16U];
        __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_q__v41 
            = vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_q[17U];
        __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_q__v42 
            = vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_q[18U];
        __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_q__v43 
            = vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_q[19U];
        __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_q__v44 
            = vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_q[20U];
        __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_q__v45 
            = vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_q[21U];
    }
    if (__VdlySet__radar_decim4__DOT__u_hb1__DOT__dl_q__v0) {
        vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_q[0U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_q[1U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_q[2U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_q[3U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_q[4U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_q[5U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_q[6U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_q[7U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_q[8U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_q[9U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_q[10U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_q[11U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_q[12U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_q[13U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_q[14U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_q[15U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_q[16U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_q[17U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_q[18U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_q[19U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_q[20U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_q[21U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_q[22U] = 0U;
    }
    if (__VdlySet__radar_decim4__DOT__u_hb1__DOT__dl_q__v23) {
        vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_q[0U] 
            = __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_q__v23;
        vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_q[1U] 
            = __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_q__v24;
        vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_q[2U] 
            = __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_q__v25;
        vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_q[3U] 
            = __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_q__v26;
        vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_q[4U] 
            = __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_q__v27;
        vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_q[5U] 
            = __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_q__v28;
        vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_q[6U] 
            = __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_q__v29;
        vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_q[7U] 
            = __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_q__v30;
        vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_q[8U] 
            = __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_q__v31;
        vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_q[9U] 
            = __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_q__v32;
        vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_q[10U] 
            = __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_q__v33;
        vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_q[11U] 
            = __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_q__v34;
        vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_q[12U] 
            = __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_q__v35;
        vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_q[13U] 
            = __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_q__v36;
        vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_q[14U] 
            = __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_q__v37;
        vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_q[15U] 
            = __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_q__v38;
        vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_q[16U] 
            = __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_q__v39;
        vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_q[17U] 
            = __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_q__v40;
        vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_q[18U] 
            = __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_q__v41;
        vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_q[19U] 
            = __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_q__v42;
        vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_q[20U] 
            = __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_q__v43;
        vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_q[21U] 
            = __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_q__v44;
        vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_q[22U] 
            = __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_q__v45;
    }
    if (vlSelfRef.radar_decim4__DOT__clr) {
        __VdlySet__radar_decim4__DOT__u_hb1__DOT__dl_i__v0 = 1U;
    } else if (vlSelfRef.in_valid) {
        __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_i__v23 
            = vlSelfRef.in_i;
        __VdlySet__radar_decim4__DOT__u_hb1__DOT__dl_i__v23 = 1U;
        __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_i__v24 
            = vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_i[0U];
        __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_i__v25 
            = vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_i[1U];
        __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_i__v26 
            = vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_i[2U];
        __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_i__v27 
            = vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_i[3U];
        __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_i__v28 
            = vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_i[4U];
        __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_i__v29 
            = vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_i[5U];
        __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_i__v30 
            = vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_i[6U];
        __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_i__v31 
            = vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_i[7U];
        __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_i__v32 
            = vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_i[8U];
        __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_i__v33 
            = vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_i[9U];
        __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_i__v34 
            = vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_i[10U];
        __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_i__v35 
            = vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_i[11U];
        __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_i__v36 
            = vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_i[12U];
        __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_i__v37 
            = vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_i[13U];
        __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_i__v38 
            = vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_i[14U];
        __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_i__v39 
            = vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_i[15U];
        __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_i__v40 
            = vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_i[16U];
        __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_i__v41 
            = vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_i[17U];
        __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_i__v42 
            = vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_i[18U];
        __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_i__v43 
            = vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_i[19U];
        __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_i__v44 
            = vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_i[20U];
        __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_i__v45 
            = vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_i[21U];
    }
    if (__VdlySet__radar_decim4__DOT__u_hb1__DOT__dl_i__v0) {
        vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_i[0U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_i[1U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_i[2U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_i[3U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_i[4U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_i[5U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_i[6U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_i[7U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_i[8U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_i[9U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_i[10U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_i[11U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_i[12U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_i[13U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_i[14U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_i[15U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_i[16U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_i[17U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_i[18U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_i[19U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_i[20U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_i[21U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_i[22U] = 0U;
    }
    if (__VdlySet__radar_decim4__DOT__u_hb1__DOT__dl_i__v23) {
        vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_i[0U] 
            = __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_i__v23;
        vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_i[1U] 
            = __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_i__v24;
        vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_i[2U] 
            = __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_i__v25;
        vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_i[3U] 
            = __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_i__v26;
        vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_i[4U] 
            = __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_i__v27;
        vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_i[5U] 
            = __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_i__v28;
        vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_i[6U] 
            = __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_i__v29;
        vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_i[7U] 
            = __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_i__v30;
        vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_i[8U] 
            = __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_i__v31;
        vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_i[9U] 
            = __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_i__v32;
        vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_i[10U] 
            = __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_i__v33;
        vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_i[11U] 
            = __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_i__v34;
        vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_i[12U] 
            = __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_i__v35;
        vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_i[13U] 
            = __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_i__v36;
        vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_i[14U] 
            = __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_i__v37;
        vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_i[15U] 
            = __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_i__v38;
        vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_i[16U] 
            = __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_i__v39;
        vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_i[17U] 
            = __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_i__v40;
        vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_i[18U] 
            = __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_i__v41;
        vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_i[19U] 
            = __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_i__v42;
        vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_i[20U] 
            = __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_i__v43;
        vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_i[21U] 
            = __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_i__v44;
        vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_i[22U] 
            = __VdlyVal__radar_decim4__DOT__u_hb1__DOT__dl_i__v45;
    }
    vlSelfRef.radar_decim4__DOT__u_hb1__DOT__pi_s2[0U] 
        = __VdlyVal__radar_decim4__DOT__u_hb1__DOT__pi_s2__v0;
    vlSelfRef.radar_decim4__DOT__u_hb1__DOT__pi_s2[1U] 
        = __VdlyVal__radar_decim4__DOT__u_hb1__DOT__pi_s2__v1;
    vlSelfRef.radar_decim4__DOT__u_hb1__DOT__pi_s2[2U] 
        = __VdlyVal__radar_decim4__DOT__u_hb1__DOT__pi_s2__v2;
    vlSelfRef.radar_decim4__DOT__u_hb1__DOT__pi_s2[3U] 
        = __VdlyVal__radar_decim4__DOT__u_hb1__DOT__pi_s2__v3;
    vlSelfRef.radar_decim4__DOT__u_hb1__DOT__pi_s2[4U] 
        = __VdlyVal__radar_decim4__DOT__u_hb1__DOT__pi_s2__v4;
    vlSelfRef.radar_decim4__DOT__u_hb1__DOT__pi_s2[5U] 
        = __VdlyVal__radar_decim4__DOT__u_hb1__DOT__pi_s2__v5;
    vlSelfRef.radar_decim4__DOT__u_hb1__DOT__pi_s2[6U] 
        = __VdlyVal__radar_decim4__DOT__u_hb1__DOT__pi_s2__v6;
    vlSelfRef.radar_decim4__DOT__u_hb1__DOT__pq_s2[0U] 
        = __VdlyVal__radar_decim4__DOT__u_hb1__DOT__pq_s2__v0;
    vlSelfRef.radar_decim4__DOT__u_hb1__DOT__pq_s2[1U] 
        = __VdlyVal__radar_decim4__DOT__u_hb1__DOT__pq_s2__v1;
    vlSelfRef.radar_decim4__DOT__u_hb1__DOT__pq_s2[2U] 
        = __VdlyVal__radar_decim4__DOT__u_hb1__DOT__pq_s2__v2;
    vlSelfRef.radar_decim4__DOT__u_hb1__DOT__pq_s2[3U] 
        = __VdlyVal__radar_decim4__DOT__u_hb1__DOT__pq_s2__v3;
    vlSelfRef.radar_decim4__DOT__u_hb1__DOT__pq_s2[4U] 
        = __VdlyVal__radar_decim4__DOT__u_hb1__DOT__pq_s2__v4;
    vlSelfRef.radar_decim4__DOT__u_hb1__DOT__pq_s2[5U] 
        = __VdlyVal__radar_decim4__DOT__u_hb1__DOT__pq_s2__v5;
    vlSelfRef.radar_decim4__DOT__u_hb1__DOT__pq_s2[6U] 
        = __VdlyVal__radar_decim4__DOT__u_hb1__DOT__pq_s2__v6;
    if (vlSelfRef.radar_decim4__DOT__clr) {
        __VdlySet__radar_decim4__DOT__u_hb2__DOT__dl_q__v0 = 1U;
    } else if (vlSelfRef.radar_decim4__DOT__mid_valid) {
        __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v35 
            = vlSelfRef.radar_decim4__DOT__mid_q;
        __VdlySet__radar_decim4__DOT__u_hb2__DOT__dl_q__v35 = 1U;
        __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v36 
            = vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[0U];
        __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v37 
            = vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[1U];
        __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v38 
            = vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[2U];
        __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v39 
            = vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[3U];
        __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v40 
            = vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[4U];
        __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v41 
            = vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[5U];
        __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v42 
            = vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[6U];
        __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v43 
            = vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[7U];
        __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v44 
            = vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[8U];
        __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v45 
            = vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[9U];
        __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v46 
            = vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[10U];
        __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v47 
            = vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[11U];
        __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v48 
            = vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[12U];
        __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v49 
            = vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[13U];
        __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v50 
            = vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[14U];
        __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v51 
            = vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[15U];
        __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v52 
            = vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[16U];
        __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v53 
            = vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[17U];
        __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v54 
            = vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[18U];
        __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v55 
            = vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[19U];
        __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v56 
            = vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[20U];
        __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v57 
            = vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[21U];
        __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v58 
            = vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[22U];
        __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v59 
            = vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[23U];
        __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v60 
            = vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[24U];
        __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v61 
            = vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[25U];
        __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v62 
            = vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[26U];
        __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v63 
            = vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[27U];
        __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v64 
            = vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[28U];
        __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v65 
            = vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[29U];
        __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v66 
            = vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[30U];
        __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v67 
            = vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[31U];
        __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v68 
            = vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[32U];
        __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v69 
            = vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[33U];
    }
    if (__VdlySet__radar_decim4__DOT__u_hb2__DOT__dl_q__v0) {
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[0U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[1U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[2U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[3U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[4U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[5U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[6U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[7U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[8U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[9U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[10U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[11U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[12U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[13U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[14U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[15U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[16U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[17U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[18U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[19U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[20U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[21U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[22U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[23U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[24U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[25U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[26U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[27U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[28U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[29U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[30U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[31U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[32U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[33U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[34U] = 0U;
    }
    if (__VdlySet__radar_decim4__DOT__u_hb2__DOT__dl_q__v35) {
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[0U] 
            = __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v35;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[1U] 
            = __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v36;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[2U] 
            = __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v37;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[3U] 
            = __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v38;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[4U] 
            = __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v39;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[5U] 
            = __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v40;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[6U] 
            = __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v41;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[7U] 
            = __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v42;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[8U] 
            = __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v43;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[9U] 
            = __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v44;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[10U] 
            = __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v45;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[11U] 
            = __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v46;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[12U] 
            = __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v47;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[13U] 
            = __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v48;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[14U] 
            = __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v49;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[15U] 
            = __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v50;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[16U] 
            = __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v51;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[17U] 
            = __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v52;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[18U] 
            = __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v53;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[19U] 
            = __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v54;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[20U] 
            = __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v55;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[21U] 
            = __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v56;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[22U] 
            = __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v57;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[23U] 
            = __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v58;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[24U] 
            = __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v59;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[25U] 
            = __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v60;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[26U] 
            = __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v61;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[27U] 
            = __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v62;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[28U] 
            = __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v63;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[29U] 
            = __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v64;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[30U] 
            = __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v65;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[31U] 
            = __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v66;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[32U] 
            = __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v67;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[33U] 
            = __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v68;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[34U] 
            = __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_q__v69;
    }
    if (vlSelfRef.radar_decim4__DOT__clr) {
        __VdlySet__radar_decim4__DOT__u_hb2__DOT__dl_i__v0 = 1U;
    } else if (vlSelfRef.radar_decim4__DOT__mid_valid) {
        __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v35 
            = vlSelfRef.radar_decim4__DOT__mid_i;
        __VdlySet__radar_decim4__DOT__u_hb2__DOT__dl_i__v35 = 1U;
        __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v36 
            = vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[0U];
        __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v37 
            = vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[1U];
        __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v38 
            = vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[2U];
        __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v39 
            = vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[3U];
        __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v40 
            = vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[4U];
        __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v41 
            = vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[5U];
        __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v42 
            = vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[6U];
        __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v43 
            = vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[7U];
        __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v44 
            = vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[8U];
        __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v45 
            = vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[9U];
        __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v46 
            = vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[10U];
        __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v47 
            = vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[11U];
        __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v48 
            = vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[12U];
        __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v49 
            = vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[13U];
        __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v50 
            = vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[14U];
        __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v51 
            = vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[15U];
        __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v52 
            = vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[16U];
        __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v53 
            = vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[17U];
        __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v54 
            = vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[18U];
        __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v55 
            = vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[19U];
        __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v56 
            = vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[20U];
        __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v57 
            = vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[21U];
        __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v58 
            = vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[22U];
        __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v59 
            = vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[23U];
        __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v60 
            = vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[24U];
        __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v61 
            = vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[25U];
        __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v62 
            = vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[26U];
        __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v63 
            = vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[27U];
        __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v64 
            = vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[28U];
        __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v65 
            = vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[29U];
        __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v66 
            = vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[30U];
        __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v67 
            = vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[31U];
        __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v68 
            = vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[32U];
        __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v69 
            = vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[33U];
    }
    if (__VdlySet__radar_decim4__DOT__u_hb2__DOT__dl_i__v0) {
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[0U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[1U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[2U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[3U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[4U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[5U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[6U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[7U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[8U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[9U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[10U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[11U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[12U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[13U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[14U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[15U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[16U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[17U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[18U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[19U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[20U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[21U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[22U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[23U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[24U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[25U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[26U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[27U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[28U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[29U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[30U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[31U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[32U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[33U] = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[34U] = 0U;
    }
    if (__VdlySet__radar_decim4__DOT__u_hb2__DOT__dl_i__v35) {
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[0U] 
            = __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v35;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[1U] 
            = __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v36;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[2U] 
            = __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v37;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[3U] 
            = __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v38;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[4U] 
            = __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v39;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[5U] 
            = __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v40;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[6U] 
            = __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v41;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[7U] 
            = __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v42;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[8U] 
            = __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v43;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[9U] 
            = __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v44;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[10U] 
            = __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v45;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[11U] 
            = __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v46;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[12U] 
            = __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v47;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[13U] 
            = __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v48;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[14U] 
            = __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v49;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[15U] 
            = __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v50;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[16U] 
            = __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v51;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[17U] 
            = __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v52;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[18U] 
            = __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v53;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[19U] 
            = __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v54;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[20U] 
            = __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v55;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[21U] 
            = __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v56;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[22U] 
            = __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v57;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[23U] 
            = __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v58;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[24U] 
            = __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v59;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[25U] 
            = __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v60;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[26U] 
            = __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v61;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[27U] 
            = __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v62;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[28U] 
            = __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v63;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[29U] 
            = __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v64;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[30U] 
            = __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v65;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[31U] 
            = __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v66;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[32U] 
            = __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v67;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[33U] 
            = __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v68;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[34U] 
            = __VdlyVal__radar_decim4__DOT__u_hb2__DOT__dl_i__v69;
    }
    vlSelfRef.radar_decim4__DOT__u_hb2__DOT__pi_s2[0U] 
        = __VdlyVal__radar_decim4__DOT__u_hb2__DOT__pi_s2__v0;
    vlSelfRef.radar_decim4__DOT__u_hb2__DOT__pi_s2[1U] 
        = __VdlyVal__radar_decim4__DOT__u_hb2__DOT__pi_s2__v1;
    vlSelfRef.radar_decim4__DOT__u_hb2__DOT__pi_s2[2U] 
        = __VdlyVal__radar_decim4__DOT__u_hb2__DOT__pi_s2__v2;
    vlSelfRef.radar_decim4__DOT__u_hb2__DOT__pi_s2[3U] 
        = __VdlyVal__radar_decim4__DOT__u_hb2__DOT__pi_s2__v3;
    vlSelfRef.radar_decim4__DOT__u_hb2__DOT__pi_s2[4U] 
        = __VdlyVal__radar_decim4__DOT__u_hb2__DOT__pi_s2__v4;
    vlSelfRef.radar_decim4__DOT__u_hb2__DOT__pi_s2[5U] 
        = __VdlyVal__radar_decim4__DOT__u_hb2__DOT__pi_s2__v5;
    vlSelfRef.radar_decim4__DOT__u_hb2__DOT__pi_s2[6U] 
        = __VdlyVal__radar_decim4__DOT__u_hb2__DOT__pi_s2__v6;
    vlSelfRef.radar_decim4__DOT__u_hb2__DOT__pi_s2[7U] 
        = __VdlyVal__radar_decim4__DOT__u_hb2__DOT__pi_s2__v7;
    vlSelfRef.radar_decim4__DOT__u_hb2__DOT__pi_s2[8U] 
        = __VdlyVal__radar_decim4__DOT__u_hb2__DOT__pi_s2__v8;
    vlSelfRef.radar_decim4__DOT__u_hb2__DOT__pi_s2[9U] 
        = __VdlyVal__radar_decim4__DOT__u_hb2__DOT__pi_s2__v9;
    vlSelfRef.radar_decim4__DOT__u_hb2__DOT__pq_s2[0U] 
        = __VdlyVal__radar_decim4__DOT__u_hb2__DOT__pq_s2__v0;
    vlSelfRef.radar_decim4__DOT__u_hb2__DOT__pq_s2[1U] 
        = __VdlyVal__radar_decim4__DOT__u_hb2__DOT__pq_s2__v1;
    vlSelfRef.radar_decim4__DOT__u_hb2__DOT__pq_s2[2U] 
        = __VdlyVal__radar_decim4__DOT__u_hb2__DOT__pq_s2__v2;
    vlSelfRef.radar_decim4__DOT__u_hb2__DOT__pq_s2[3U] 
        = __VdlyVal__radar_decim4__DOT__u_hb2__DOT__pq_s2__v3;
    vlSelfRef.radar_decim4__DOT__u_hb2__DOT__pq_s2[4U] 
        = __VdlyVal__radar_decim4__DOT__u_hb2__DOT__pq_s2__v4;
    vlSelfRef.radar_decim4__DOT__u_hb2__DOT__pq_s2[5U] 
        = __VdlyVal__radar_decim4__DOT__u_hb2__DOT__pq_s2__v5;
    vlSelfRef.radar_decim4__DOT__u_hb2__DOT__pq_s2[6U] 
        = __VdlyVal__radar_decim4__DOT__u_hb2__DOT__pq_s2__v6;
    vlSelfRef.radar_decim4__DOT__u_hb2__DOT__pq_s2[7U] 
        = __VdlyVal__radar_decim4__DOT__u_hb2__DOT__pq_s2__v7;
    vlSelfRef.radar_decim4__DOT__u_hb2__DOT__pq_s2[8U] 
        = __VdlyVal__radar_decim4__DOT__u_hb2__DOT__pq_s2__v8;
    vlSelfRef.radar_decim4__DOT__u_hb2__DOT__pq_s2[9U] 
        = __VdlyVal__radar_decim4__DOT__u_hb2__DOT__pq_s2__v9;
    vlSelfRef.radar_decim4__DOT__u_hb1__DOT__psum_q[0U] 
        = (0x0001ffffU & (((0x00010000U & ((vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_q[0U] 
                                            >> 0x0000000fU) 
                                           << 0x00000010U)) 
                           | vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_q[0U]) 
                          + ((0x00010000U & ((vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_q[22U] 
                                              >> 0x0000000fU) 
                                             << 0x00000010U)) 
                             | vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_q[22U])));
    vlSelfRef.radar_decim4__DOT__u_hb1__DOT__psum_q[1U] 
        = (0x0001ffffU & (((0x00010000U & ((vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_q[2U] 
                                            >> 0x0000000fU) 
                                           << 0x00000010U)) 
                           | vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_q[2U]) 
                          + ((0x00010000U & ((vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_q[20U] 
                                              >> 0x0000000fU) 
                                             << 0x00000010U)) 
                             | vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_q[20U])));
    vlSelfRef.radar_decim4__DOT__u_hb1__DOT__psum_q[2U] 
        = (0x0001ffffU & (((0x00010000U & ((vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_q[4U] 
                                            >> 0x0000000fU) 
                                           << 0x00000010U)) 
                           | vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_q[4U]) 
                          + ((0x00010000U & ((vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_q[18U] 
                                              >> 0x0000000fU) 
                                             << 0x00000010U)) 
                             | vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_q[18U])));
    vlSelfRef.radar_decim4__DOT__u_hb1__DOT__psum_q[3U] 
        = (0x0001ffffU & (((0x00010000U & ((vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_q[6U] 
                                            >> 0x0000000fU) 
                                           << 0x00000010U)) 
                           | vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_q[6U]) 
                          + ((0x00010000U & ((vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_q[16U] 
                                              >> 0x0000000fU) 
                                             << 0x00000010U)) 
                             | vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_q[16U])));
    vlSelfRef.radar_decim4__DOT__u_hb1__DOT__psum_q[4U] 
        = (0x0001ffffU & (((0x00010000U & ((vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_q[8U] 
                                            >> 0x0000000fU) 
                                           << 0x00000010U)) 
                           | vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_q[8U]) 
                          + ((0x00010000U & ((vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_q[14U] 
                                              >> 0x0000000fU) 
                                             << 0x00000010U)) 
                             | vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_q[14U])));
    vlSelfRef.radar_decim4__DOT__u_hb1__DOT__psum_q[5U] 
        = (0x0001ffffU & (((0x00010000U & ((vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_q[10U] 
                                            >> 0x0000000fU) 
                                           << 0x00000010U)) 
                           | vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_q[10U]) 
                          + ((0x00010000U & ((vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_q[12U] 
                                              >> 0x0000000fU) 
                                             << 0x00000010U)) 
                             | vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_q[12U])));
    vlSelfRef.radar_decim4__DOT__u_hb1__DOT__psum_q[6U] 
        = ((0x00010000U & ((vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_q[11U] 
                            >> 0x0000000fU) << 0x00000010U)) 
           | vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_q[11U]);
    vlSelfRef.radar_decim4__DOT__u_hb1__DOT__psum_i[0U] 
        = (0x0001ffffU & (((0x00010000U & ((vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_i[0U] 
                                            >> 0x0000000fU) 
                                           << 0x00000010U)) 
                           | vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_i[0U]) 
                          + ((0x00010000U & ((vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_i[22U] 
                                              >> 0x0000000fU) 
                                             << 0x00000010U)) 
                             | vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_i[22U])));
    vlSelfRef.radar_decim4__DOT__u_hb1__DOT__psum_i[1U] 
        = (0x0001ffffU & (((0x00010000U & ((vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_i[2U] 
                                            >> 0x0000000fU) 
                                           << 0x00000010U)) 
                           | vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_i[2U]) 
                          + ((0x00010000U & ((vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_i[20U] 
                                              >> 0x0000000fU) 
                                             << 0x00000010U)) 
                             | vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_i[20U])));
    vlSelfRef.radar_decim4__DOT__u_hb1__DOT__psum_i[2U] 
        = (0x0001ffffU & (((0x00010000U & ((vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_i[4U] 
                                            >> 0x0000000fU) 
                                           << 0x00000010U)) 
                           | vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_i[4U]) 
                          + ((0x00010000U & ((vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_i[18U] 
                                              >> 0x0000000fU) 
                                             << 0x00000010U)) 
                             | vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_i[18U])));
    vlSelfRef.radar_decim4__DOT__u_hb1__DOT__psum_i[3U] 
        = (0x0001ffffU & (((0x00010000U & ((vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_i[6U] 
                                            >> 0x0000000fU) 
                                           << 0x00000010U)) 
                           | vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_i[6U]) 
                          + ((0x00010000U & ((vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_i[16U] 
                                              >> 0x0000000fU) 
                                             << 0x00000010U)) 
                             | vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_i[16U])));
    vlSelfRef.radar_decim4__DOT__u_hb1__DOT__psum_i[4U] 
        = (0x0001ffffU & (((0x00010000U & ((vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_i[8U] 
                                            >> 0x0000000fU) 
                                           << 0x00000010U)) 
                           | vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_i[8U]) 
                          + ((0x00010000U & ((vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_i[14U] 
                                              >> 0x0000000fU) 
                                             << 0x00000010U)) 
                             | vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_i[14U])));
    vlSelfRef.radar_decim4__DOT__u_hb1__DOT__psum_i[5U] 
        = (0x0001ffffU & (((0x00010000U & ((vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_i[10U] 
                                            >> 0x0000000fU) 
                                           << 0x00000010U)) 
                           | vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_i[10U]) 
                          + ((0x00010000U & ((vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_i[12U] 
                                              >> 0x0000000fU) 
                                             << 0x00000010U)) 
                             | vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_i[12U])));
    vlSelfRef.radar_decim4__DOT__u_hb1__DOT__psum_i[6U] 
        = ((0x00010000U & ((vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_i[11U] 
                            >> 0x0000000fU) << 0x00000010U)) 
           | vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dl_i[11U]);
    radar_decim4__DOT__u_hb1__DOT__g_acc__BRA__0__KET____DOT__ext_i 
        = (((QData)((IData)((0x0000001fU & (- (IData)(
                                                      (1U 
                                                       & (IData)(
                                                                 (vlSelfRef.radar_decim4__DOT__u_hb1__DOT__pi_s2[0U] 
                                                                  >> 0x00000022U)))))))) 
            << 0x00000023U) | vlSelfRef.radar_decim4__DOT__u_hb1__DOT__pi_s2[0U]);
    radar_decim4__DOT__u_hb1__DOT__g_acc__BRA__0__KET____DOT__ext_q 
        = (((QData)((IData)((0x0000001fU & (- (IData)(
                                                      (1U 
                                                       & (IData)(
                                                                 (vlSelfRef.radar_decim4__DOT__u_hb1__DOT__pq_s2[0U] 
                                                                  >> 0x00000022U)))))))) 
            << 0x00000023U) | vlSelfRef.radar_decim4__DOT__u_hb1__DOT__pq_s2[0U]);
    vlSelfRef.radar_decim4__DOT__u_hb2__DOT__psum_q[0U] 
        = (0x0001ffffU & (((0x00010000U & ((vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[0U] 
                                            >> 0x0000000fU) 
                                           << 0x00000010U)) 
                           | vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[0U]) 
                          + ((0x00010000U & ((vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[34U] 
                                              >> 0x0000000fU) 
                                             << 0x00000010U)) 
                             | vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[34U])));
    vlSelfRef.radar_decim4__DOT__u_hb2__DOT__psum_q[1U] 
        = (0x0001ffffU & (((0x00010000U & ((vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[2U] 
                                            >> 0x0000000fU) 
                                           << 0x00000010U)) 
                           | vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[2U]) 
                          + ((0x00010000U & ((vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[32U] 
                                              >> 0x0000000fU) 
                                             << 0x00000010U)) 
                             | vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[32U])));
    vlSelfRef.radar_decim4__DOT__u_hb2__DOT__psum_q[2U] 
        = (0x0001ffffU & (((0x00010000U & ((vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[4U] 
                                            >> 0x0000000fU) 
                                           << 0x00000010U)) 
                           | vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[4U]) 
                          + ((0x00010000U & ((vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[30U] 
                                              >> 0x0000000fU) 
                                             << 0x00000010U)) 
                             | vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[30U])));
    vlSelfRef.radar_decim4__DOT__u_hb2__DOT__psum_q[3U] 
        = (0x0001ffffU & (((0x00010000U & ((vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[6U] 
                                            >> 0x0000000fU) 
                                           << 0x00000010U)) 
                           | vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[6U]) 
                          + ((0x00010000U & ((vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[28U] 
                                              >> 0x0000000fU) 
                                             << 0x00000010U)) 
                             | vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[28U])));
    vlSelfRef.radar_decim4__DOT__u_hb2__DOT__psum_q[4U] 
        = (0x0001ffffU & (((0x00010000U & ((vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[8U] 
                                            >> 0x0000000fU) 
                                           << 0x00000010U)) 
                           | vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[8U]) 
                          + ((0x00010000U & ((vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[26U] 
                                              >> 0x0000000fU) 
                                             << 0x00000010U)) 
                             | vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[26U])));
    vlSelfRef.radar_decim4__DOT__u_hb2__DOT__psum_q[5U] 
        = (0x0001ffffU & (((0x00010000U & ((vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[10U] 
                                            >> 0x0000000fU) 
                                           << 0x00000010U)) 
                           | vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[10U]) 
                          + ((0x00010000U & ((vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[24U] 
                                              >> 0x0000000fU) 
                                             << 0x00000010U)) 
                             | vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[24U])));
    vlSelfRef.radar_decim4__DOT__u_hb2__DOT__psum_q[6U] 
        = (0x0001ffffU & (((0x00010000U & ((vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[12U] 
                                            >> 0x0000000fU) 
                                           << 0x00000010U)) 
                           | vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[12U]) 
                          + ((0x00010000U & ((vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[22U] 
                                              >> 0x0000000fU) 
                                             << 0x00000010U)) 
                             | vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[22U])));
    vlSelfRef.radar_decim4__DOT__u_hb2__DOT__psum_q[7U] 
        = (0x0001ffffU & (((0x00010000U & ((vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[14U] 
                                            >> 0x0000000fU) 
                                           << 0x00000010U)) 
                           | vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[14U]) 
                          + ((0x00010000U & ((vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[20U] 
                                              >> 0x0000000fU) 
                                             << 0x00000010U)) 
                             | vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[20U])));
    vlSelfRef.radar_decim4__DOT__u_hb2__DOT__psum_q[8U] 
        = (0x0001ffffU & (((0x00010000U & ((vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[16U] 
                                            >> 0x0000000fU) 
                                           << 0x00000010U)) 
                           | vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[16U]) 
                          + ((0x00010000U & ((vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[18U] 
                                              >> 0x0000000fU) 
                                             << 0x00000010U)) 
                             | vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[18U])));
    vlSelfRef.radar_decim4__DOT__u_hb2__DOT__psum_q[9U] 
        = ((0x00010000U & ((vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[17U] 
                            >> 0x0000000fU) << 0x00000010U)) 
           | vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_q[17U]);
    vlSelfRef.radar_decim4__DOT__u_hb2__DOT__psum_i[0U] 
        = (0x0001ffffU & (((0x00010000U & ((vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[0U] 
                                            >> 0x0000000fU) 
                                           << 0x00000010U)) 
                           | vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[0U]) 
                          + ((0x00010000U & ((vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[34U] 
                                              >> 0x0000000fU) 
                                             << 0x00000010U)) 
                             | vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[34U])));
    vlSelfRef.radar_decim4__DOT__u_hb2__DOT__psum_i[1U] 
        = (0x0001ffffU & (((0x00010000U & ((vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[2U] 
                                            >> 0x0000000fU) 
                                           << 0x00000010U)) 
                           | vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[2U]) 
                          + ((0x00010000U & ((vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[32U] 
                                              >> 0x0000000fU) 
                                             << 0x00000010U)) 
                             | vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[32U])));
    vlSelfRef.radar_decim4__DOT__u_hb2__DOT__psum_i[2U] 
        = (0x0001ffffU & (((0x00010000U & ((vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[4U] 
                                            >> 0x0000000fU) 
                                           << 0x00000010U)) 
                           | vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[4U]) 
                          + ((0x00010000U & ((vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[30U] 
                                              >> 0x0000000fU) 
                                             << 0x00000010U)) 
                             | vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[30U])));
    vlSelfRef.radar_decim4__DOT__u_hb2__DOT__psum_i[3U] 
        = (0x0001ffffU & (((0x00010000U & ((vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[6U] 
                                            >> 0x0000000fU) 
                                           << 0x00000010U)) 
                           | vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[6U]) 
                          + ((0x00010000U & ((vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[28U] 
                                              >> 0x0000000fU) 
                                             << 0x00000010U)) 
                             | vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[28U])));
    vlSelfRef.radar_decim4__DOT__u_hb2__DOT__psum_i[4U] 
        = (0x0001ffffU & (((0x00010000U & ((vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[8U] 
                                            >> 0x0000000fU) 
                                           << 0x00000010U)) 
                           | vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[8U]) 
                          + ((0x00010000U & ((vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[26U] 
                                              >> 0x0000000fU) 
                                             << 0x00000010U)) 
                             | vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[26U])));
    vlSelfRef.radar_decim4__DOT__u_hb2__DOT__psum_i[5U] 
        = (0x0001ffffU & (((0x00010000U & ((vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[10U] 
                                            >> 0x0000000fU) 
                                           << 0x00000010U)) 
                           | vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[10U]) 
                          + ((0x00010000U & ((vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[24U] 
                                              >> 0x0000000fU) 
                                             << 0x00000010U)) 
                             | vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[24U])));
    vlSelfRef.radar_decim4__DOT__u_hb2__DOT__psum_i[6U] 
        = (0x0001ffffU & (((0x00010000U & ((vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[12U] 
                                            >> 0x0000000fU) 
                                           << 0x00000010U)) 
                           | vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[12U]) 
                          + ((0x00010000U & ((vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[22U] 
                                              >> 0x0000000fU) 
                                             << 0x00000010U)) 
                             | vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[22U])));
    vlSelfRef.radar_decim4__DOT__u_hb2__DOT__psum_i[7U] 
        = (0x0001ffffU & (((0x00010000U & ((vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[14U] 
                                            >> 0x0000000fU) 
                                           << 0x00000010U)) 
                           | vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[14U]) 
                          + ((0x00010000U & ((vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[20U] 
                                              >> 0x0000000fU) 
                                             << 0x00000010U)) 
                             | vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[20U])));
    vlSelfRef.radar_decim4__DOT__u_hb2__DOT__psum_i[8U] 
        = (0x0001ffffU & (((0x00010000U & ((vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[16U] 
                                            >> 0x0000000fU) 
                                           << 0x00000010U)) 
                           | vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[16U]) 
                          + ((0x00010000U & ((vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[18U] 
                                              >> 0x0000000fU) 
                                             << 0x00000010U)) 
                             | vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[18U])));
    vlSelfRef.radar_decim4__DOT__u_hb2__DOT__psum_i[9U] 
        = ((0x00010000U & ((vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[17U] 
                            >> 0x0000000fU) << 0x00000010U)) 
           | vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dl_i[17U]);
    __Vfunc_radar_decim4__DOT__u_hb1__DOT__round_sat__6__v 
        = vlSelfRef.radar_decim4__DOT__u_hb1__DOT__acc_i_s3;
    radar_decim4__DOT__u_hb1__DOT__round_sat__Vstatic__t 
        = (0x000000ffffffffffULL & VL_SHIFTRS_QQI(40,40,32, 
                                                  (0x000000ffffffffffULL 
                                                   & (0x0000000000010000ULL 
                                                      + __Vfunc_radar_decim4__DOT__u_hb1__DOT__round_sat__6__v)), 0x00000011U));
    __Vfunc_radar_decim4__DOT__u_hb1__DOT__round_sat__6__Vfuncout 
        = (VL_LTS_IQQ(40, 0x0000000000007fffULL, radar_decim4__DOT__u_hb1__DOT__round_sat__Vstatic__t)
            ? 0x00007fffU : (VL_GTS_IQQ(40, 0x000000ffffff8000ULL, radar_decim4__DOT__u_hb1__DOT__round_sat__Vstatic__t)
                              ? 0x00008000U : (0x0000ffffU 
                                               & (IData)(radar_decim4__DOT__u_hb1__DOT__round_sat__Vstatic__t))));
    vlSelfRef.radar_decim4__DOT__mid_i = __Vfunc_radar_decim4__DOT__u_hb1__DOT__round_sat__6__Vfuncout;
    __Vfunc_radar_decim4__DOT__u_hb1__DOT__round_sat__7__v 
        = vlSelfRef.radar_decim4__DOT__u_hb1__DOT__acc_q_s3;
    radar_decim4__DOT__u_hb1__DOT__round_sat__Vstatic__t 
        = (0x000000ffffffffffULL & VL_SHIFTRS_QQI(40,40,32, 
                                                  (0x000000ffffffffffULL 
                                                   & (0x0000000000010000ULL 
                                                      + __Vfunc_radar_decim4__DOT__u_hb1__DOT__round_sat__7__v)), 0x00000011U));
    __Vfunc_radar_decim4__DOT__u_hb1__DOT__round_sat__7__Vfuncout 
        = (VL_LTS_IQQ(40, 0x0000000000007fffULL, radar_decim4__DOT__u_hb1__DOT__round_sat__Vstatic__t)
            ? 0x00007fffU : (VL_GTS_IQQ(40, 0x000000ffffff8000ULL, radar_decim4__DOT__u_hb1__DOT__round_sat__Vstatic__t)
                              ? 0x00008000U : (0x0000ffffU 
                                               & (IData)(radar_decim4__DOT__u_hb1__DOT__round_sat__Vstatic__t))));
    vlSelfRef.radar_decim4__DOT__mid_q = __Vfunc_radar_decim4__DOT__u_hb1__DOT__round_sat__7__Vfuncout;
    radar_decim4__DOT__u_hb2__DOT__g_acc__BRA__0__KET____DOT__ext_i 
        = (((QData)((IData)((0x0000001fU & (- (IData)(
                                                      (1U 
                                                       & (IData)(
                                                                 (vlSelfRef.radar_decim4__DOT__u_hb2__DOT__pi_s2[0U] 
                                                                  >> 0x00000022U)))))))) 
            << 0x00000023U) | vlSelfRef.radar_decim4__DOT__u_hb2__DOT__pi_s2[0U]);
    radar_decim4__DOT__u_hb2__DOT__g_acc__BRA__0__KET____DOT__ext_q 
        = (((QData)((IData)((0x0000001fU & (- (IData)(
                                                      (1U 
                                                       & (IData)(
                                                                 (vlSelfRef.radar_decim4__DOT__u_hb2__DOT__pq_s2[0U] 
                                                                  >> 0x00000022U)))))))) 
            << 0x00000023U) | vlSelfRef.radar_decim4__DOT__u_hb2__DOT__pq_s2[0U]);
    vlSelfRef.radar_decim4__DOT__u_hb2__DOT__acc_i_s3 
        = vlSelfRef.radar_decim4__DOT__u_hb2__DOT__chain_i[9U];
    vlSelfRef.radar_decim4__DOT__u_hb2__DOT__acc_q_s3 
        = vlSelfRef.radar_decim4__DOT__u_hb2__DOT__chain_q[9U];
    vlSelfRef.radar_decim4__DOT__u_hb2__DOT__v_s3 = 
        ((~ (IData)(vlSelfRef.radar_decim4__DOT__clr)) 
         & (IData)(vlSelfRef.radar_decim4__DOT__u_hb2__DOT__v_s2));
    __VdfgRegularize_hebeb780c_0_16 = (0x000000ffffffffffULL 
                                       & (radar_decim4__DOT__u_hb1__DOT__g_acc__BRA__0__KET____DOT__ext_i 
                                          + (((QData)((IData)(
                                                              (0x0000001fU 
                                                               & (- (IData)(
                                                                            (1U 
                                                                             & (IData)(
                                                                                (vlSelfRef.radar_decim4__DOT__u_hb1__DOT__pi_s2[1U] 
                                                                                >> 0x00000022U)))))))) 
                                              << 0x00000023U) 
                                             | vlSelfRef.radar_decim4__DOT__u_hb1__DOT__pi_s2[1U])));
    __VdfgRegularize_hebeb780c_0_21 = (0x000000ffffffffffULL 
                                       & (radar_decim4__DOT__u_hb1__DOT__g_acc__BRA__0__KET____DOT__ext_q 
                                          + (((QData)((IData)(
                                                              (0x0000001fU 
                                                               & (- (IData)(
                                                                            (1U 
                                                                             & (IData)(
                                                                                (vlSelfRef.radar_decim4__DOT__u_hb1__DOT__pq_s2[1U] 
                                                                                >> 0x00000022U)))))))) 
                                              << 0x00000023U) 
                                             | vlSelfRef.radar_decim4__DOT__u_hb1__DOT__pq_s2[1U])));
    __VdfgRegularize_hebeb780c_0_0 = (0x000000ffffffffffULL 
                                      & (radar_decim4__DOT__u_hb2__DOT__g_acc__BRA__0__KET____DOT__ext_i 
                                         + (((QData)((IData)(
                                                             (0x0000001fU 
                                                              & (- (IData)(
                                                                           (1U 
                                                                            & (IData)(
                                                                                (vlSelfRef.radar_decim4__DOT__u_hb2__DOT__pi_s2[1U] 
                                                                                >> 0x00000022U)))))))) 
                                             << 0x00000023U) 
                                            | vlSelfRef.radar_decim4__DOT__u_hb2__DOT__pi_s2[1U])));
    __VdfgRegularize_hebeb780c_0_8 = (0x000000ffffffffffULL 
                                      & (radar_decim4__DOT__u_hb2__DOT__g_acc__BRA__0__KET____DOT__ext_q 
                                         + (((QData)((IData)(
                                                             (0x0000001fU 
                                                              & (- (IData)(
                                                                           (1U 
                                                                            & (IData)(
                                                                                (vlSelfRef.radar_decim4__DOT__u_hb2__DOT__pq_s2[1U] 
                                                                                >> 0x00000022U)))))))) 
                                             << 0x00000023U) 
                                            | vlSelfRef.radar_decim4__DOT__u_hb2__DOT__pq_s2[1U])));
    vlSelfRef.radar_decim4__DOT__u_hb2__DOT__chain_i[0U] 
        = radar_decim4__DOT__u_hb2__DOT__g_acc__BRA__0__KET____DOT__ext_i;
    vlSelfRef.radar_decim4__DOT__u_hb2__DOT__chain_q[0U] 
        = radar_decim4__DOT__u_hb2__DOT__g_acc__BRA__0__KET____DOT__ext_q;
    __VdfgRegularize_hebeb780c_0_20 = (0x000000ffffffffffULL 
                                       & ((((QData)((IData)(
                                                            (0x0000001fU 
                                                             & (- (IData)(
                                                                          (1U 
                                                                           & (IData)(
                                                                                (vlSelfRef.radar_decim4__DOT__u_hb1__DOT__pi_s2[2U] 
                                                                                >> 0x00000022U)))))))) 
                                            << 0x00000023U) 
                                           | vlSelfRef.radar_decim4__DOT__u_hb1__DOT__pi_s2[2U]) 
                                          + __VdfgRegularize_hebeb780c_0_16));
    __VdfgRegularize_hebeb780c_0_25 = (0x000000ffffffffffULL 
                                       & ((((QData)((IData)(
                                                            (0x0000001fU 
                                                             & (- (IData)(
                                                                          (1U 
                                                                           & (IData)(
                                                                                (vlSelfRef.radar_decim4__DOT__u_hb1__DOT__pq_s2[2U] 
                                                                                >> 0x00000022U)))))))) 
                                            << 0x00000023U) 
                                           | vlSelfRef.radar_decim4__DOT__u_hb1__DOT__pq_s2[2U]) 
                                          + __VdfgRegularize_hebeb780c_0_21));
    vlSelfRef.radar_decim4__DOT__u_hb1__DOT__acc_i_s3 
        = vlSelfRef.radar_decim4__DOT__u_hb1__DOT__chain_i[6U];
    vlSelfRef.radar_decim4__DOT__u_hb1__DOT__acc_q_s3 
        = vlSelfRef.radar_decim4__DOT__u_hb1__DOT__chain_q[6U];
    vlSelfRef.radar_decim4__DOT__u_hb2__DOT__chain_i[1U] 
        = __VdfgRegularize_hebeb780c_0_0;
    __VdfgRegularize_hebeb780c_0_7 = (0x000000ffffffffffULL 
                                      & ((((QData)((IData)(
                                                           (0x0000001fU 
                                                            & (- (IData)(
                                                                         (1U 
                                                                          & (IData)(
                                                                                (vlSelfRef.radar_decim4__DOT__u_hb2__DOT__pi_s2[2U] 
                                                                                >> 0x00000022U)))))))) 
                                           << 0x00000023U) 
                                          | vlSelfRef.radar_decim4__DOT__u_hb2__DOT__pi_s2[2U]) 
                                         + __VdfgRegularize_hebeb780c_0_0));
    vlSelfRef.radar_decim4__DOT__u_hb2__DOT__chain_q[1U] 
        = __VdfgRegularize_hebeb780c_0_8;
    __VdfgRegularize_hebeb780c_0_15 = (0x000000ffffffffffULL 
                                       & ((((QData)((IData)(
                                                            (0x0000001fU 
                                                             & (- (IData)(
                                                                          (1U 
                                                                           & (IData)(
                                                                                (vlSelfRef.radar_decim4__DOT__u_hb2__DOT__pq_s2[2U] 
                                                                                >> 0x00000022U)))))))) 
                                            << 0x00000023U) 
                                           | vlSelfRef.radar_decim4__DOT__u_hb2__DOT__pq_s2[2U]) 
                                          + __VdfgRegularize_hebeb780c_0_8));
    vlSelfRef.radar_decim4__DOT__u_hb2__DOT__v_s2 = 
        ((~ (IData)(vlSelfRef.radar_decim4__DOT__clr)) 
         & (IData)(vlSelfRef.radar_decim4__DOT__u_hb2__DOT__due_s1));
    __VdfgRegularize_hebeb780c_0_19 = (0x000000ffffffffffULL 
                                       & ((((QData)((IData)(
                                                            (0x0000001fU 
                                                             & (- (IData)(
                                                                          (1U 
                                                                           & (IData)(
                                                                                (vlSelfRef.radar_decim4__DOT__u_hb1__DOT__pi_s2[3U] 
                                                                                >> 0x00000022U)))))))) 
                                            << 0x00000023U) 
                                           | vlSelfRef.radar_decim4__DOT__u_hb1__DOT__pi_s2[3U]) 
                                          + __VdfgRegularize_hebeb780c_0_20));
    __VdfgRegularize_hebeb780c_0_24 = (0x000000ffffffffffULL 
                                       & ((((QData)((IData)(
                                                            (0x0000001fU 
                                                             & (- (IData)(
                                                                          (1U 
                                                                           & (IData)(
                                                                                (vlSelfRef.radar_decim4__DOT__u_hb1__DOT__pq_s2[3U] 
                                                                                >> 0x00000022U)))))))) 
                                            << 0x00000023U) 
                                           | vlSelfRef.radar_decim4__DOT__u_hb1__DOT__pq_s2[3U]) 
                                          + __VdfgRegularize_hebeb780c_0_25));
    vlSelfRef.radar_decim4__DOT__u_hb1__DOT__chain_i[0U] 
        = radar_decim4__DOT__u_hb1__DOT__g_acc__BRA__0__KET____DOT__ext_i;
    vlSelfRef.radar_decim4__DOT__u_hb1__DOT__chain_i[1U] 
        = __VdfgRegularize_hebeb780c_0_16;
    vlSelfRef.radar_decim4__DOT__u_hb1__DOT__chain_i[2U] 
        = __VdfgRegularize_hebeb780c_0_20;
    vlSelfRef.radar_decim4__DOT__u_hb1__DOT__chain_q[0U] 
        = radar_decim4__DOT__u_hb1__DOT__g_acc__BRA__0__KET____DOT__ext_q;
    vlSelfRef.radar_decim4__DOT__u_hb1__DOT__chain_q[1U] 
        = __VdfgRegularize_hebeb780c_0_21;
    vlSelfRef.radar_decim4__DOT__u_hb1__DOT__chain_q[2U] 
        = __VdfgRegularize_hebeb780c_0_25;
    vlSelfRef.radar_decim4__DOT__u_hb2__DOT__chain_i[2U] 
        = __VdfgRegularize_hebeb780c_0_7;
    __VdfgRegularize_hebeb780c_0_6 = (0x000000ffffffffffULL 
                                      & ((((QData)((IData)(
                                                           (0x0000001fU 
                                                            & (- (IData)(
                                                                         (1U 
                                                                          & (IData)(
                                                                                (vlSelfRef.radar_decim4__DOT__u_hb2__DOT__pi_s2[3U] 
                                                                                >> 0x00000022U)))))))) 
                                           << 0x00000023U) 
                                          | vlSelfRef.radar_decim4__DOT__u_hb2__DOT__pi_s2[3U]) 
                                         + __VdfgRegularize_hebeb780c_0_7));
    vlSelfRef.radar_decim4__DOT__u_hb2__DOT__chain_q[2U] 
        = __VdfgRegularize_hebeb780c_0_15;
    __VdfgRegularize_hebeb780c_0_14 = (0x000000ffffffffffULL 
                                       & ((((QData)((IData)(
                                                            (0x0000001fU 
                                                             & (- (IData)(
                                                                          (1U 
                                                                           & (IData)(
                                                                                (vlSelfRef.radar_decim4__DOT__u_hb2__DOT__pq_s2[3U] 
                                                                                >> 0x00000022U)))))))) 
                                            << 0x00000023U) 
                                           | vlSelfRef.radar_decim4__DOT__u_hb2__DOT__pq_s2[3U]) 
                                          + __VdfgRegularize_hebeb780c_0_15));
    vlSelfRef.radar_decim4__DOT__u_hb1__DOT__chain_i[3U] 
        = __VdfgRegularize_hebeb780c_0_19;
    __VdfgRegularize_hebeb780c_0_18 = (0x000000ffffffffffULL 
                                       & ((((QData)((IData)(
                                                            (0x0000001fU 
                                                             & (- (IData)(
                                                                          (1U 
                                                                           & (IData)(
                                                                                (vlSelfRef.radar_decim4__DOT__u_hb1__DOT__pi_s2[4U] 
                                                                                >> 0x00000022U)))))))) 
                                            << 0x00000023U) 
                                           | vlSelfRef.radar_decim4__DOT__u_hb1__DOT__pi_s2[4U]) 
                                          + __VdfgRegularize_hebeb780c_0_19));
    vlSelfRef.radar_decim4__DOT__u_hb1__DOT__chain_q[3U] 
        = __VdfgRegularize_hebeb780c_0_24;
    __VdfgRegularize_hebeb780c_0_23 = (0x000000ffffffffffULL 
                                       & ((((QData)((IData)(
                                                            (0x0000001fU 
                                                             & (- (IData)(
                                                                          (1U 
                                                                           & (IData)(
                                                                                (vlSelfRef.radar_decim4__DOT__u_hb1__DOT__pq_s2[4U] 
                                                                                >> 0x00000022U)))))))) 
                                            << 0x00000023U) 
                                           | vlSelfRef.radar_decim4__DOT__u_hb1__DOT__pq_s2[4U]) 
                                          + __VdfgRegularize_hebeb780c_0_24));
    vlSelfRef.radar_decim4__DOT__u_hb2__DOT__chain_i[3U] 
        = __VdfgRegularize_hebeb780c_0_6;
    __VdfgRegularize_hebeb780c_0_5 = (0x000000ffffffffffULL 
                                      & ((((QData)((IData)(
                                                           (0x0000001fU 
                                                            & (- (IData)(
                                                                         (1U 
                                                                          & (IData)(
                                                                                (vlSelfRef.radar_decim4__DOT__u_hb2__DOT__pi_s2[4U] 
                                                                                >> 0x00000022U)))))))) 
                                           << 0x00000023U) 
                                          | vlSelfRef.radar_decim4__DOT__u_hb2__DOT__pi_s2[4U]) 
                                         + __VdfgRegularize_hebeb780c_0_6));
    vlSelfRef.radar_decim4__DOT__u_hb2__DOT__chain_q[3U] 
        = __VdfgRegularize_hebeb780c_0_14;
    __VdfgRegularize_hebeb780c_0_13 = (0x000000ffffffffffULL 
                                       & ((((QData)((IData)(
                                                            (0x0000001fU 
                                                             & (- (IData)(
                                                                          (1U 
                                                                           & (IData)(
                                                                                (vlSelfRef.radar_decim4__DOT__u_hb2__DOT__pq_s2[4U] 
                                                                                >> 0x00000022U)))))))) 
                                            << 0x00000023U) 
                                           | vlSelfRef.radar_decim4__DOT__u_hb2__DOT__pq_s2[4U]) 
                                          + __VdfgRegularize_hebeb780c_0_14));
    vlSelfRef.radar_decim4__DOT__u_hb1__DOT__chain_i[4U] 
        = __VdfgRegularize_hebeb780c_0_18;
    __VdfgRegularize_hebeb780c_0_17 = (0x000000ffffffffffULL 
                                       & ((((QData)((IData)(
                                                            (0x0000001fU 
                                                             & (- (IData)(
                                                                          (1U 
                                                                           & (IData)(
                                                                                (vlSelfRef.radar_decim4__DOT__u_hb1__DOT__pi_s2[5U] 
                                                                                >> 0x00000022U)))))))) 
                                            << 0x00000023U) 
                                           | vlSelfRef.radar_decim4__DOT__u_hb1__DOT__pi_s2[5U]) 
                                          + __VdfgRegularize_hebeb780c_0_18));
    vlSelfRef.radar_decim4__DOT__u_hb1__DOT__chain_q[4U] 
        = __VdfgRegularize_hebeb780c_0_23;
    __VdfgRegularize_hebeb780c_0_22 = (0x000000ffffffffffULL 
                                       & ((((QData)((IData)(
                                                            (0x0000001fU 
                                                             & (- (IData)(
                                                                          (1U 
                                                                           & (IData)(
                                                                                (vlSelfRef.radar_decim4__DOT__u_hb1__DOT__pq_s2[5U] 
                                                                                >> 0x00000022U)))))))) 
                                            << 0x00000023U) 
                                           | vlSelfRef.radar_decim4__DOT__u_hb1__DOT__pq_s2[5U]) 
                                          + __VdfgRegularize_hebeb780c_0_23));
    vlSelfRef.radar_decim4__DOT__u_hb2__DOT__chain_i[4U] 
        = __VdfgRegularize_hebeb780c_0_5;
    __VdfgRegularize_hebeb780c_0_4 = (0x000000ffffffffffULL 
                                      & ((((QData)((IData)(
                                                           (0x0000001fU 
                                                            & (- (IData)(
                                                                         (1U 
                                                                          & (IData)(
                                                                                (vlSelfRef.radar_decim4__DOT__u_hb2__DOT__pi_s2[5U] 
                                                                                >> 0x00000022U)))))))) 
                                           << 0x00000023U) 
                                          | vlSelfRef.radar_decim4__DOT__u_hb2__DOT__pi_s2[5U]) 
                                         + __VdfgRegularize_hebeb780c_0_5));
    vlSelfRef.radar_decim4__DOT__u_hb2__DOT__chain_q[4U] 
        = __VdfgRegularize_hebeb780c_0_13;
    __VdfgRegularize_hebeb780c_0_12 = (0x000000ffffffffffULL 
                                       & ((((QData)((IData)(
                                                            (0x0000001fU 
                                                             & (- (IData)(
                                                                          (1U 
                                                                           & (IData)(
                                                                                (vlSelfRef.radar_decim4__DOT__u_hb2__DOT__pq_s2[5U] 
                                                                                >> 0x00000022U)))))))) 
                                            << 0x00000023U) 
                                           | vlSelfRef.radar_decim4__DOT__u_hb2__DOT__pq_s2[5U]) 
                                          + __VdfgRegularize_hebeb780c_0_13));
    if (vlSelfRef.radar_decim4__DOT__clr) {
        __Vdly__radar_decim4__DOT__u_hb2__DOT__dphase = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__due_s1 = 0U;
    } else if (vlSelfRef.radar_decim4__DOT__mid_valid) {
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__due_s1 
            = vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dphase;
        __Vdly__radar_decim4__DOT__u_hb2__DOT__dphase 
            = (1U & (~ (IData)(vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dphase)));
    } else {
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__due_s1 = 0U;
    }
    vlSelfRef.radar_decim4__DOT__u_hb2__DOT__dphase 
        = __Vdly__radar_decim4__DOT__u_hb2__DOT__dphase;
    vlSelfRef.radar_decim4__DOT__u_hb1__DOT__chain_i[5U] 
        = __VdfgRegularize_hebeb780c_0_17;
    vlSelfRef.radar_decim4__DOT__u_hb1__DOT__chain_i[6U] 
        = (0x000000ffffffffffULL & ((((QData)((IData)(
                                                      (0x0000001fU 
                                                       & (- (IData)(
                                                                    (1U 
                                                                     & (IData)(
                                                                               (vlSelfRef.radar_decim4__DOT__u_hb1__DOT__pi_s2[6U] 
                                                                                >> 0x00000022U)))))))) 
                                      << 0x00000023U) 
                                     | vlSelfRef.radar_decim4__DOT__u_hb1__DOT__pi_s2[6U]) 
                                    + __VdfgRegularize_hebeb780c_0_17));
    vlSelfRef.radar_decim4__DOT__u_hb1__DOT__chain_q[5U] 
        = __VdfgRegularize_hebeb780c_0_22;
    vlSelfRef.radar_decim4__DOT__u_hb1__DOT__chain_q[6U] 
        = (0x000000ffffffffffULL & ((((QData)((IData)(
                                                      (0x0000001fU 
                                                       & (- (IData)(
                                                                    (1U 
                                                                     & (IData)(
                                                                               (vlSelfRef.radar_decim4__DOT__u_hb1__DOT__pq_s2[6U] 
                                                                                >> 0x00000022U)))))))) 
                                      << 0x00000023U) 
                                     | vlSelfRef.radar_decim4__DOT__u_hb1__DOT__pq_s2[6U]) 
                                    + __VdfgRegularize_hebeb780c_0_22));
    vlSelfRef.radar_decim4__DOT__u_hb2__DOT__chain_i[5U] 
        = __VdfgRegularize_hebeb780c_0_4;
    __VdfgRegularize_hebeb780c_0_3 = (0x000000ffffffffffULL 
                                      & ((((QData)((IData)(
                                                           (0x0000001fU 
                                                            & (- (IData)(
                                                                         (1U 
                                                                          & (IData)(
                                                                                (vlSelfRef.radar_decim4__DOT__u_hb2__DOT__pi_s2[6U] 
                                                                                >> 0x00000022U)))))))) 
                                           << 0x00000023U) 
                                          | vlSelfRef.radar_decim4__DOT__u_hb2__DOT__pi_s2[6U]) 
                                         + __VdfgRegularize_hebeb780c_0_4));
    vlSelfRef.radar_decim4__DOT__u_hb2__DOT__chain_q[5U] 
        = __VdfgRegularize_hebeb780c_0_12;
    __VdfgRegularize_hebeb780c_0_11 = (0x000000ffffffffffULL 
                                       & ((((QData)((IData)(
                                                            (0x0000001fU 
                                                             & (- (IData)(
                                                                          (1U 
                                                                           & (IData)(
                                                                                (vlSelfRef.radar_decim4__DOT__u_hb2__DOT__pq_s2[6U] 
                                                                                >> 0x00000022U)))))))) 
                                            << 0x00000023U) 
                                           | vlSelfRef.radar_decim4__DOT__u_hb2__DOT__pq_s2[6U]) 
                                          + __VdfgRegularize_hebeb780c_0_12));
    vlSelfRef.radar_decim4__DOT__mid_valid = ((~ (IData)(vlSelfRef.radar_decim4__DOT__clr)) 
                                              & (IData)(vlSelfRef.radar_decim4__DOT__u_hb1__DOT__v_s3));
    vlSelfRef.radar_decim4__DOT__u_hb2__DOT__chain_i[6U] 
        = __VdfgRegularize_hebeb780c_0_3;
    __VdfgRegularize_hebeb780c_0_2 = (0x000000ffffffffffULL 
                                      & ((((QData)((IData)(
                                                           (0x0000001fU 
                                                            & (- (IData)(
                                                                         (1U 
                                                                          & (IData)(
                                                                                (vlSelfRef.radar_decim4__DOT__u_hb2__DOT__pi_s2[7U] 
                                                                                >> 0x00000022U)))))))) 
                                           << 0x00000023U) 
                                          | vlSelfRef.radar_decim4__DOT__u_hb2__DOT__pi_s2[7U]) 
                                         + __VdfgRegularize_hebeb780c_0_3));
    vlSelfRef.radar_decim4__DOT__u_hb2__DOT__chain_q[6U] 
        = __VdfgRegularize_hebeb780c_0_11;
    __VdfgRegularize_hebeb780c_0_10 = (0x000000ffffffffffULL 
                                       & ((((QData)((IData)(
                                                            (0x0000001fU 
                                                             & (- (IData)(
                                                                          (1U 
                                                                           & (IData)(
                                                                                (vlSelfRef.radar_decim4__DOT__u_hb2__DOT__pq_s2[7U] 
                                                                                >> 0x00000022U)))))))) 
                                            << 0x00000023U) 
                                           | vlSelfRef.radar_decim4__DOT__u_hb2__DOT__pq_s2[7U]) 
                                          + __VdfgRegularize_hebeb780c_0_11));
    vlSelfRef.radar_decim4__DOT__u_hb2__DOT__chain_i[7U] 
        = __VdfgRegularize_hebeb780c_0_2;
    __VdfgRegularize_hebeb780c_0_1 = (0x000000ffffffffffULL 
                                      & ((((QData)((IData)(
                                                           (0x0000001fU 
                                                            & (- (IData)(
                                                                         (1U 
                                                                          & (IData)(
                                                                                (vlSelfRef.radar_decim4__DOT__u_hb2__DOT__pi_s2[8U] 
                                                                                >> 0x00000022U)))))))) 
                                           << 0x00000023U) 
                                          | vlSelfRef.radar_decim4__DOT__u_hb2__DOT__pi_s2[8U]) 
                                         + __VdfgRegularize_hebeb780c_0_2));
    vlSelfRef.radar_decim4__DOT__u_hb2__DOT__chain_q[7U] 
        = __VdfgRegularize_hebeb780c_0_10;
    __VdfgRegularize_hebeb780c_0_9 = (0x000000ffffffffffULL 
                                      & ((((QData)((IData)(
                                                           (0x0000001fU 
                                                            & (- (IData)(
                                                                         (1U 
                                                                          & (IData)(
                                                                                (vlSelfRef.radar_decim4__DOT__u_hb2__DOT__pq_s2[8U] 
                                                                                >> 0x00000022U)))))))) 
                                           << 0x00000023U) 
                                          | vlSelfRef.radar_decim4__DOT__u_hb2__DOT__pq_s2[8U]) 
                                         + __VdfgRegularize_hebeb780c_0_10));
    vlSelfRef.radar_decim4__DOT__u_hb1__DOT__v_s3 = 
        ((~ (IData)(vlSelfRef.radar_decim4__DOT__clr)) 
         & (IData)(vlSelfRef.radar_decim4__DOT__u_hb1__DOT__v_s2));
    vlSelfRef.radar_decim4__DOT__u_hb2__DOT__chain_i[8U] 
        = __VdfgRegularize_hebeb780c_0_1;
    vlSelfRef.radar_decim4__DOT__u_hb2__DOT__chain_i[9U] 
        = (0x000000ffffffffffULL & ((((QData)((IData)(
                                                      (0x0000001fU 
                                                       & (- (IData)(
                                                                    (1U 
                                                                     & (IData)(
                                                                               (vlSelfRef.radar_decim4__DOT__u_hb2__DOT__pi_s2[9U] 
                                                                                >> 0x00000022U)))))))) 
                                      << 0x00000023U) 
                                     | vlSelfRef.radar_decim4__DOT__u_hb2__DOT__pi_s2[9U]) 
                                    + __VdfgRegularize_hebeb780c_0_1));
    vlSelfRef.radar_decim4__DOT__u_hb2__DOT__chain_q[8U] 
        = __VdfgRegularize_hebeb780c_0_9;
    vlSelfRef.radar_decim4__DOT__u_hb2__DOT__chain_q[9U] 
        = (0x000000ffffffffffULL & ((((QData)((IData)(
                                                      (0x0000001fU 
                                                       & (- (IData)(
                                                                    (1U 
                                                                     & (IData)(
                                                                               (vlSelfRef.radar_decim4__DOT__u_hb2__DOT__pq_s2[9U] 
                                                                                >> 0x00000022U)))))))) 
                                      << 0x00000023U) 
                                     | vlSelfRef.radar_decim4__DOT__u_hb2__DOT__pq_s2[9U]) 
                                    + __VdfgRegularize_hebeb780c_0_9));
    vlSelfRef.radar_decim4__DOT__u_hb1__DOT__v_s2 = 
        ((~ (IData)(vlSelfRef.radar_decim4__DOT__clr)) 
         & (IData)(vlSelfRef.radar_decim4__DOT__u_hb1__DOT__due_s1));
    if (vlSelfRef.radar_decim4__DOT__clr) {
        __Vdly__radar_decim4__DOT__u_hb1__DOT__dphase = 0U;
        vlSelfRef.radar_decim4__DOT__u_hb1__DOT__due_s1 = 0U;
    } else if (vlSelfRef.in_valid) {
        vlSelfRef.radar_decim4__DOT__u_hb1__DOT__due_s1 
            = vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dphase;
        __Vdly__radar_decim4__DOT__u_hb1__DOT__dphase 
            = (1U & (~ (IData)(vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dphase)));
    } else {
        vlSelfRef.radar_decim4__DOT__u_hb1__DOT__due_s1 = 0U;
    }
    vlSelfRef.radar_decim4__DOT__u_hb1__DOT__dphase 
        = __Vdly__radar_decim4__DOT__u_hb1__DOT__dphase;
}

void Vradar_decim4___024root___trigger_orInto__act_vec_vec(VlUnpacked<QData/*63:0*/, 1> &out, const VlUnpacked<QData/*63:0*/, 1> &in) {
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vradar_decim4___024root___trigger_orInto__act_vec_vec\n"); );
    // Locals
    IData/*31:0*/ n;
    // Body
    n = 0U;
    do {
        out[n] = (out[n] | in[n]);
        n = ((IData)(1U) + n);
    } while ((0U >= n));
}

#ifdef VL_DEBUG
VL_ATTR_COLD void Vradar_decim4___024root___dump_triggers__act(const VlUnpacked<QData/*63:0*/, 1> &triggers, const std::string &tag);
#endif  // VL_DEBUG

bool Vradar_decim4___024root___eval_phase__act(Vradar_decim4___024root* vlSelf) {
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vradar_decim4___024root___eval_phase__act\n"); );
    Vradar_decim4__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    auto& vlSelfRef = std::ref(*vlSelf).get();
    // Body
    {
        // Inlined CFunc: _eval_triggers_vec__act
        vlSelfRef.__VactTriggered[0U] = (QData)((IData)(
                                                        ((IData)(vlSelfRef.clk) 
                                                         & (~ (IData)(vlSelfRef.__Vtrigprevexpr___TOP__clk__1)))));
        vlSelfRef.__Vtrigprevexpr___TOP__clk__1 = vlSelfRef.clk;
    }
#ifdef VL_DEBUG
    if (VL_UNLIKELY(vlSymsp->_vm_contextp__->debug())) {
        Vradar_decim4___024root___dump_triggers__act(vlSelfRef.__VactTriggered, "act"s);
    }
#endif
    Vradar_decim4___024root___trigger_orInto__act_vec_vec(vlSelfRef.__VnbaTriggered, vlSelfRef.__VactTriggered);
    return (0U);
}

void Vradar_decim4___024root___trigger_clear__act(VlUnpacked<QData/*63:0*/, 1> &out) {
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vradar_decim4___024root___trigger_clear__act\n"); );
    // Locals
    IData/*31:0*/ n;
    // Body
    n = 0U;
    do {
        out[n] = 0ULL;
        n = ((IData)(1U) + n);
    } while ((1U > n));
}

bool Vradar_decim4___024root___eval_phase__nba(Vradar_decim4___024root* vlSelf) {
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vradar_decim4___024root___eval_phase__nba\n"); );
    Vradar_decim4__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    auto& vlSelfRef = std::ref(*vlSelf).get();
    // Locals
    CData/*0:0*/ __VnbaExecute;
    // Body
    __VnbaExecute = Vradar_decim4___024root___trigger_anySet__act(vlSelfRef.__VnbaTriggered);
    if (__VnbaExecute) {
        {
            // Inlined CFunc: _eval_nba
            if ((1ULL & vlSelfRef.__VnbaTriggered[0U])) {
                Vradar_decim4___024root___nba_sequent__TOP__0(vlSelf);
            }
        }
        Vradar_decim4___024root___trigger_clear__act(vlSelfRef.__VnbaTriggered);
    }
    return (__VnbaExecute);
}

void Vradar_decim4___024root___eval(Vradar_decim4___024root* vlSelf) {
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vradar_decim4___024root___eval\n"); );
    Vradar_decim4__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    auto& vlSelfRef = std::ref(*vlSelf).get();
    // Locals
    IData/*31:0*/ __VicoIterCount;
    IData/*31:0*/ __VnbaIterCount;
    // Body
    __VicoIterCount = 0U;
    do {
        if (VL_UNLIKELY(((0x00002710U < __VicoIterCount)))) {
#ifdef VL_DEBUG
            Vradar_decim4___024root___dump_triggers__ico(vlSelfRef.__VicoTriggered, "ico"s);
#endif
            VL_FATAL_MT("/Users/lucasnaylor/radar-5g8-array/fpga/sim/../rtl/radar_decim4.v", 48, "", "DIDNOTCONVERGE: Input combinational region did not converge after '--converge-limit' of 10000 tries");
        }
        __VicoIterCount = ((IData)(1U) + __VicoIterCount);
        vlSelfRef.__VicoPhaseResult = Vradar_decim4___024root___eval_phase__ico(vlSelf);
    } while (vlSelfRef.__VicoPhaseResult);
    __VnbaIterCount = 0U;
    do {
        if (VL_UNLIKELY(((0x00002710U < __VnbaIterCount)))) {
#ifdef VL_DEBUG
            Vradar_decim4___024root___dump_triggers__act(vlSelfRef.__VnbaTriggered, "nba"s);
#endif
            VL_FATAL_MT("/Users/lucasnaylor/radar-5g8-array/fpga/sim/../rtl/radar_decim4.v", 48, "", "DIDNOTCONVERGE: NBA region did not converge after '--converge-limit' of 10000 tries");
        }
        __VnbaIterCount = ((IData)(1U) + __VnbaIterCount);
        vlSelfRef.__VactIterCount = 0U;
        do {
            if (VL_UNLIKELY(((0x00002710U < vlSelfRef.__VactIterCount)))) {
#ifdef VL_DEBUG
                Vradar_decim4___024root___dump_triggers__act(vlSelfRef.__VactTriggered, "act"s);
#endif
                VL_FATAL_MT("/Users/lucasnaylor/radar-5g8-array/fpga/sim/../rtl/radar_decim4.v", 48, "", "DIDNOTCONVERGE: Active region did not converge after '--converge-limit' of 10000 tries");
            }
            vlSelfRef.__VactIterCount = ((IData)(1U) 
                                         + vlSelfRef.__VactIterCount);
            vlSelfRef.__VactPhaseResult = Vradar_decim4___024root___eval_phase__act(vlSelf);
        } while (vlSelfRef.__VactPhaseResult);
        vlSelfRef.__VnbaPhaseResult = Vradar_decim4___024root___eval_phase__nba(vlSelf);
    } while (vlSelfRef.__VnbaPhaseResult);
}

#ifdef VL_DEBUG
void Vradar_decim4___024root___eval_debug_assertions(Vradar_decim4___024root* vlSelf) {
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vradar_decim4___024root___eval_debug_assertions\n"); );
    Vradar_decim4__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    auto& vlSelfRef = std::ref(*vlSelf).get();
    // Body
    if (VL_UNLIKELY(((vlSelfRef.clk & 0xfeU)))) {
        Verilated::overWidthError("clk");
    }
    if (VL_UNLIKELY(((vlSelfRef.rst & 0xfeU)))) {
        Verilated::overWidthError("rst");
    }
    if (VL_UNLIKELY(((vlSelfRef.flush & 0xfeU)))) {
        Verilated::overWidthError("flush");
    }
    if (VL_UNLIKELY(((vlSelfRef.in_valid & 0xfeU)))) {
        Verilated::overWidthError("in_valid");
    }
}
#endif  // VL_DEBUG
