// Verilated -*- C++ -*-
// DESCRIPTION: Verilator output: Design implementation internals
// See Vradar_decim4.h for the primary calling header

#include "Vradar_decim4__pch.h"

VL_ATTR_COLD void Vradar_decim4___024root___eval_static(Vradar_decim4___024root* vlSelf) {
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vradar_decim4___024root___eval_static\n"); );
    Vradar_decim4__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    auto& vlSelfRef = std::ref(*vlSelf).get();
    // Body
    {
        // Inlined CFunc: _eval_static__TOP
        QData/*39:0*/ __Vinline_0__eval_static__TOP_radar_decim4__DOT__u_hb2__DOT__round_sat__Vstatic__t;
        __Vinline_0__eval_static__TOP_radar_decim4__DOT__u_hb2__DOT__round_sat__Vstatic__t = 0;
        QData/*39:0*/ __Vinline_0__eval_static__TOP_radar_decim4__DOT__u_hb1__DOT__round_sat__Vstatic__t;
        __Vinline_0__eval_static__TOP_radar_decim4__DOT__u_hb1__DOT__round_sat__Vstatic__t = 0;
        __Vinline_0__eval_static__TOP_radar_decim4__DOT__u_hb2__DOT__round_sat__Vstatic__t = 0;
        __Vinline_0__eval_static__TOP_radar_decim4__DOT__u_hb1__DOT__round_sat__Vstatic__t = 0;
    }
    vlSelfRef.__Vtrigprevexpr___TOP__clk__0 = vlSelfRef.clk;
    vlSelfRef.__Vtrigprevexpr___TOP__rst__0 = vlSelfRef.rst;
    vlSelfRef.__Vtrigprevexpr___TOP__flush__0 = vlSelfRef.flush;
    vlSelfRef.__Vtrigprevexpr___TOP__in_valid__0 = vlSelfRef.in_valid;
    vlSelfRef.__Vtrigprevexpr___TOP__in_i__0 = vlSelfRef.in_i;
    vlSelfRef.__Vtrigprevexpr___TOP__in_q__0 = vlSelfRef.in_q;
    vlSelfRef.__Vtrigprevexpr___TOP__clk__1 = vlSelfRef.clk;
}

VL_ATTR_COLD void Vradar_decim4___024root___eval_initial(Vradar_decim4___024root* vlSelf) {
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vradar_decim4___024root___eval_initial\n"); );
    Vradar_decim4__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    auto& vlSelfRef = std::ref(*vlSelf).get();
    // Body
    {
        // Inlined CFunc: _eval_initial__TOP
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__coef[0U] = 1U;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__coef[1U] = 0x0003ffebU;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__coef[2U] = 0x00000073U;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__coef[3U] = 0x0003fe6aU;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__coef[4U] = 0x00000454U;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__coef[5U] = 0x0003f5f1U;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__coef[6U] = 0x00001570U;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__coef[7U] = 0x0003d130U;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__coef[8U] = 0x0000a052U;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__coef[8U] = 0x0000a052U;
        vlSelfRef.radar_decim4__DOT__u_hb2__DOT__coef[9U] = 0x00010000U;
        vlSelfRef.radar_decim4__DOT__u_hb1__DOT__coef[0U] = 0x0003fffaU;
        vlSelfRef.radar_decim4__DOT__u_hb1__DOT__coef[1U] = 0x000000a7U;
        vlSelfRef.radar_decim4__DOT__u_hb1__DOT__coef[2U] = 0x0003fc2fU;
        vlSelfRef.radar_decim4__DOT__u_hb1__DOT__coef[3U] = 0x00000dacU;
        vlSelfRef.radar_decim4__DOT__u_hb1__DOT__coef[4U] = 0x0003d7d8U;
        vlSelfRef.radar_decim4__DOT__u_hb1__DOT__coef[5U] = 0x00009dadU;
        vlSelfRef.radar_decim4__DOT__u_hb1__DOT__coef[5U] = 0x00009dacU;
        vlSelfRef.radar_decim4__DOT__u_hb1__DOT__coef[6U] = 0x00010000U;
    }
}

VL_ATTR_COLD void Vradar_decim4___024root___eval_final(Vradar_decim4___024root* vlSelf) {
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vradar_decim4___024root___eval_final\n"); );
    Vradar_decim4__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    auto& vlSelfRef = std::ref(*vlSelf).get();
}

#ifdef VL_DEBUG
VL_ATTR_COLD void Vradar_decim4___024root___dump_triggers__stl(const VlUnpacked<QData/*63:0*/, 1> &triggers, const std::string &tag);
#endif  // VL_DEBUG
VL_ATTR_COLD bool Vradar_decim4___024root___eval_phase__stl(Vradar_decim4___024root* vlSelf);

VL_ATTR_COLD void Vradar_decim4___024root___eval_settle(Vradar_decim4___024root* vlSelf) {
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vradar_decim4___024root___eval_settle\n"); );
    Vradar_decim4__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    auto& vlSelfRef = std::ref(*vlSelf).get();
    // Locals
    IData/*31:0*/ __VstlIterCount;
    // Body
    __VstlIterCount = 0U;
    vlSelfRef.__VstlFirstIteration = 1U;
    do {
        if (VL_UNLIKELY(((0x00002710U < __VstlIterCount)))) {
#ifdef VL_DEBUG
            Vradar_decim4___024root___dump_triggers__stl(vlSelfRef.__VstlTriggered, "stl"s);
#endif
            VL_FATAL_MT("/Users/lucasnaylor/radar-5g8-array/fpga/sim/../rtl/radar_decim4.v", 48, "", "DIDNOTCONVERGE: Settle region did not converge after '--converge-limit' of 10000 tries");
        }
        __VstlIterCount = ((IData)(1U) + __VstlIterCount);
        vlSelfRef.__VstlPhaseResult = Vradar_decim4___024root___eval_phase__stl(vlSelf);
        vlSelfRef.__VstlFirstIteration = 0U;
    } while (vlSelfRef.__VstlPhaseResult);
}

VL_ATTR_COLD bool Vradar_decim4___024root___trigger_anySet__stl(const VlUnpacked<QData/*63:0*/, 1> &in);

#ifdef VL_DEBUG
VL_ATTR_COLD void Vradar_decim4___024root___dump_triggers__stl(const VlUnpacked<QData/*63:0*/, 1> &triggers, const std::string &tag) {
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vradar_decim4___024root___dump_triggers__stl\n"); );
    // Body
    if ((1U & (~ (IData)(Vradar_decim4___024root___trigger_anySet__stl(triggers))))) {
        VL_DBG_MSGS("         No '" + tag + "' region triggers active\n");
    }
    if ((1U & (IData)(triggers[0U]))) {
        VL_DBG_MSGS("         '" + tag + "' region trigger index 0 is active: Internal 'stl' trigger - first iteration\n");
    }
}
#endif  // VL_DEBUG

VL_ATTR_COLD bool Vradar_decim4___024root___trigger_anySet__stl(const VlUnpacked<QData/*63:0*/, 1> &in) {
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vradar_decim4___024root___trigger_anySet__stl\n"); );
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

VL_ATTR_COLD void Vradar_decim4___024root___stl_sequent__TOP__0(Vradar_decim4___024root* vlSelf) {
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vradar_decim4___024root___stl_sequent__TOP__0\n"); );
    Vradar_decim4__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    auto& vlSelfRef = std::ref(*vlSelf).get();
    // Locals
    QData/*39:0*/ radar_decim4__DOT__u_hb2__DOT__g_acc__BRA__0__KET____DOT__ext_i;
    radar_decim4__DOT__u_hb2__DOT__g_acc__BRA__0__KET____DOT__ext_i = 0;
    QData/*39:0*/ radar_decim4__DOT__u_hb2__DOT__g_acc__BRA__0__KET____DOT__ext_q;
    radar_decim4__DOT__u_hb2__DOT__g_acc__BRA__0__KET____DOT__ext_q = 0;
    QData/*39:0*/ radar_decim4__DOT__u_hb1__DOT__g_acc__BRA__0__KET____DOT__ext_i;
    radar_decim4__DOT__u_hb1__DOT__g_acc__BRA__0__KET____DOT__ext_i = 0;
    QData/*39:0*/ radar_decim4__DOT__u_hb1__DOT__g_acc__BRA__0__KET____DOT__ext_q;
    radar_decim4__DOT__u_hb1__DOT__g_acc__BRA__0__KET____DOT__ext_q = 0;
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
    // Body
    vlSelfRef.radar_decim4__DOT__clr = ((IData)(vlSelfRef.rst) 
                                        | (IData)(vlSelfRef.flush));
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
    vlSelfRef.radar_decim4__DOT__u_hb1__DOT__chain_i[0U] 
        = radar_decim4__DOT__u_hb1__DOT__g_acc__BRA__0__KET____DOT__ext_i;
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
    vlSelfRef.radar_decim4__DOT__u_hb1__DOT__chain_q[0U] 
        = radar_decim4__DOT__u_hb1__DOT__g_acc__BRA__0__KET____DOT__ext_q;
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
    vlSelfRef.radar_decim4__DOT__u_hb2__DOT__chain_i[0U] 
        = radar_decim4__DOT__u_hb2__DOT__g_acc__BRA__0__KET____DOT__ext_i;
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
    vlSelfRef.radar_decim4__DOT__u_hb2__DOT__chain_q[0U] 
        = radar_decim4__DOT__u_hb2__DOT__g_acc__BRA__0__KET____DOT__ext_q;
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
    vlSelfRef.radar_decim4__DOT__u_hb1__DOT__chain_i[1U] 
        = __VdfgRegularize_hebeb780c_0_16;
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
    vlSelfRef.radar_decim4__DOT__u_hb1__DOT__chain_q[1U] 
        = __VdfgRegularize_hebeb780c_0_21;
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
    vlSelfRef.radar_decim4__DOT__u_hb1__DOT__chain_i[2U] 
        = __VdfgRegularize_hebeb780c_0_20;
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
    vlSelfRef.radar_decim4__DOT__u_hb1__DOT__chain_q[2U] 
        = __VdfgRegularize_hebeb780c_0_25;
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
}

VL_ATTR_COLD bool Vradar_decim4___024root___eval_phase__stl(Vradar_decim4___024root* vlSelf) {
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vradar_decim4___024root___eval_phase__stl\n"); );
    Vradar_decim4__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    auto& vlSelfRef = std::ref(*vlSelf).get();
    // Locals
    CData/*0:0*/ __VstlExecute;
    // Body
    {
        // Inlined CFunc: _eval_triggers_vec__stl
        vlSelfRef.__VstlTriggered[0U] = ((0xfffffffffffffffeULL 
                                          & vlSelfRef.__VstlTriggered[0U]) 
                                         | (IData)((IData)(vlSelfRef.__VstlFirstIteration)));
    }
#ifdef VL_DEBUG
    if (VL_UNLIKELY(vlSymsp->_vm_contextp__->debug())) {
        Vradar_decim4___024root___dump_triggers__stl(vlSelfRef.__VstlTriggered, "stl"s);
    }
#endif
    __VstlExecute = Vradar_decim4___024root___trigger_anySet__stl(vlSelfRef.__VstlTriggered);
    if (__VstlExecute) {
        {
            // Inlined CFunc: _eval_stl
            if ((1ULL & vlSelfRef.__VstlTriggered[0U])) {
                Vradar_decim4___024root___stl_sequent__TOP__0(vlSelf);
            }
        }
    }
    return (__VstlExecute);
}

bool Vradar_decim4___024root___trigger_anySet__ico(const VlUnpacked<QData/*63:0*/, 2> &in);

#ifdef VL_DEBUG
VL_ATTR_COLD void Vradar_decim4___024root___dump_triggers__ico(const VlUnpacked<QData/*63:0*/, 2> &triggers, const std::string &tag) {
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vradar_decim4___024root___dump_triggers__ico\n"); );
    // Body
    if ((1U & (~ (IData)(Vradar_decim4___024root___trigger_anySet__ico(triggers))))) {
        VL_DBG_MSGS("         No '" + tag + "' region triggers active\n");
    }
    if ((1U & (IData)(triggers[0U]))) {
        VL_DBG_MSGS("         '" + tag + "' region trigger index 0 is active: @( clk)\n");
    }
    if ((1U & (IData)((triggers[0U] >> 1U)))) {
        VL_DBG_MSGS("         '" + tag + "' region trigger index 1 is active: @( rst)\n");
    }
    if ((1U & (IData)((triggers[0U] >> 2U)))) {
        VL_DBG_MSGS("         '" + tag + "' region trigger index 2 is active: @( flush)\n");
    }
    if ((1U & (IData)((triggers[0U] >> 3U)))) {
        VL_DBG_MSGS("         '" + tag + "' region trigger index 3 is active: @( in_valid)\n");
    }
    if ((1U & (IData)((triggers[0U] >> 4U)))) {
        VL_DBG_MSGS("         '" + tag + "' region trigger index 4 is active: @( in_i)\n");
    }
    if ((1U & (IData)((triggers[0U] >> 5U)))) {
        VL_DBG_MSGS("         '" + tag + "' region trigger index 5 is active: @( in_q)\n");
    }
    if ((1U & (IData)(triggers[1U]))) {
        VL_DBG_MSGS("         '" + tag + "' region trigger index 64 is active: Internal 'ico' trigger - first iteration\n");
    }
}
#endif  // VL_DEBUG

bool Vradar_decim4___024root___trigger_anySet__act(const VlUnpacked<QData/*63:0*/, 1> &in);

#ifdef VL_DEBUG
VL_ATTR_COLD void Vradar_decim4___024root___dump_triggers__act(const VlUnpacked<QData/*63:0*/, 1> &triggers, const std::string &tag) {
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vradar_decim4___024root___dump_triggers__act\n"); );
    // Body
    if ((1U & (~ (IData)(Vradar_decim4___024root___trigger_anySet__act(triggers))))) {
        VL_DBG_MSGS("         No '" + tag + "' region triggers active\n");
    }
    if ((1U & (IData)(triggers[0U]))) {
        VL_DBG_MSGS("         '" + tag + "' region trigger index 0 is active: @(posedge clk)\n");
    }
}
#endif  // VL_DEBUG

VL_ATTR_COLD void Vradar_decim4___024root___ctor_var_reset(Vradar_decim4___024root* vlSelf) {
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vradar_decim4___024root___ctor_var_reset\n"); );
    Vradar_decim4__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    auto& vlSelfRef = std::ref(*vlSelf).get();
    // Body
    const uint64_t __VscopeHash = VL_MURMUR64_HASH(vlSelf->vlNamep);
    vlSelf->clk = VL_SCOPED_RAND_RESET_I(1, __VscopeHash, 16707436170211756652ull);
    vlSelf->rst = VL_SCOPED_RAND_RESET_I(1, __VscopeHash, 18209466448985614591ull);
    vlSelf->flush = VL_SCOPED_RAND_RESET_I(1, __VscopeHash, 8361382489806169962ull);
    vlSelf->in_valid = VL_SCOPED_RAND_RESET_I(1, __VscopeHash, 2339549897027650563ull);
    vlSelf->in_i = VL_SCOPED_RAND_RESET_I(16, __VscopeHash, 10846626665951073823ull);
    vlSelf->in_q = VL_SCOPED_RAND_RESET_I(16, __VscopeHash, 5883516748077548659ull);
    vlSelf->out_valid = VL_SCOPED_RAND_RESET_I(1, __VscopeHash, 2886291494070200219ull);
    vlSelf->out_i = VL_SCOPED_RAND_RESET_I(16, __VscopeHash, 15869654417598851880ull);
    vlSelf->out_q = VL_SCOPED_RAND_RESET_I(16, __VscopeHash, 2605090817737850913ull);
    vlSelf->radar_decim4__DOT__clr = VL_SCOPED_RAND_RESET_I(1, __VscopeHash, 202706132177496147ull);
    vlSelf->radar_decim4__DOT__mid_valid = VL_SCOPED_RAND_RESET_I(1, __VscopeHash, 17069857891229323220ull);
    vlSelf->radar_decim4__DOT__mid_i = VL_SCOPED_RAND_RESET_I(16, __VscopeHash, 15083932746858763262ull);
    vlSelf->radar_decim4__DOT__mid_q = VL_SCOPED_RAND_RESET_I(16, __VscopeHash, 16253468164593984831ull);
    for (int __Vi0 = 0; __Vi0 < 10; ++__Vi0) {
        vlSelf->radar_decim4__DOT__u_hb2__DOT__coef[__Vi0] = VL_SCOPED_RAND_RESET_I(18, __VscopeHash, 16104825735079330288ull);
    }
    for (int __Vi0 = 0; __Vi0 < 35; ++__Vi0) {
        vlSelf->radar_decim4__DOT__u_hb2__DOT__dl_i[__Vi0] = VL_SCOPED_RAND_RESET_I(16, __VscopeHash, 70935572129407896ull);
    }
    for (int __Vi0 = 0; __Vi0 < 35; ++__Vi0) {
        vlSelf->radar_decim4__DOT__u_hb2__DOT__dl_q[__Vi0] = VL_SCOPED_RAND_RESET_I(16, __VscopeHash, 12999942301395547906ull);
    }
    vlSelf->radar_decim4__DOT__u_hb2__DOT__dphase = VL_SCOPED_RAND_RESET_I(1, __VscopeHash, 16639349752074513915ull);
    vlSelf->radar_decim4__DOT__u_hb2__DOT__due_s1 = VL_SCOPED_RAND_RESET_I(1, __VscopeHash, 963865612758561981ull);
    for (int __Vi0 = 0; __Vi0 < 10; ++__Vi0) {
        vlSelf->radar_decim4__DOT__u_hb2__DOT__psum_i[__Vi0] = VL_SCOPED_RAND_RESET_I(17, __VscopeHash, 14337546902753484917ull);
    }
    for (int __Vi0 = 0; __Vi0 < 10; ++__Vi0) {
        vlSelf->radar_decim4__DOT__u_hb2__DOT__psum_q[__Vi0] = VL_SCOPED_RAND_RESET_I(17, __VscopeHash, 2017389199492734784ull);
    }
    for (int __Vi0 = 0; __Vi0 < 10; ++__Vi0) {
        vlSelf->radar_decim4__DOT__u_hb2__DOT__pi_s2[__Vi0] = VL_SCOPED_RAND_RESET_Q(35, __VscopeHash, 10321425464727141989ull);
    }
    for (int __Vi0 = 0; __Vi0 < 10; ++__Vi0) {
        vlSelf->radar_decim4__DOT__u_hb2__DOT__pq_s2[__Vi0] = VL_SCOPED_RAND_RESET_Q(35, __VscopeHash, 18310850023014329095ull);
    }
    vlSelf->radar_decim4__DOT__u_hb2__DOT__v_s2 = VL_SCOPED_RAND_RESET_I(1, __VscopeHash, 17873234896768283230ull);
    for (int __Vi0 = 0; __Vi0 < 10; ++__Vi0) {
        vlSelf->radar_decim4__DOT__u_hb2__DOT__chain_i[__Vi0] = VL_SCOPED_RAND_RESET_Q(40, __VscopeHash, 8777776362600307407ull);
    }
    for (int __Vi0 = 0; __Vi0 < 10; ++__Vi0) {
        vlSelf->radar_decim4__DOT__u_hb2__DOT__chain_q[__Vi0] = VL_SCOPED_RAND_RESET_Q(40, __VscopeHash, 7805141030442658953ull);
    }
    vlSelf->radar_decim4__DOT__u_hb2__DOT__acc_i_s3 = VL_SCOPED_RAND_RESET_Q(40, __VscopeHash, 2708938916582841778ull);
    vlSelf->radar_decim4__DOT__u_hb2__DOT__acc_q_s3 = VL_SCOPED_RAND_RESET_Q(40, __VscopeHash, 139768213837432377ull);
    vlSelf->radar_decim4__DOT__u_hb2__DOT__v_s3 = VL_SCOPED_RAND_RESET_I(1, __VscopeHash, 17928413094239541153ull);
    for (int __Vi0 = 0; __Vi0 < 7; ++__Vi0) {
        vlSelf->radar_decim4__DOT__u_hb1__DOT__coef[__Vi0] = VL_SCOPED_RAND_RESET_I(18, __VscopeHash, 6170884824878242578ull);
    }
    for (int __Vi0 = 0; __Vi0 < 23; ++__Vi0) {
        vlSelf->radar_decim4__DOT__u_hb1__DOT__dl_i[__Vi0] = VL_SCOPED_RAND_RESET_I(16, __VscopeHash, 9618798391764208387ull);
    }
    for (int __Vi0 = 0; __Vi0 < 23; ++__Vi0) {
        vlSelf->radar_decim4__DOT__u_hb1__DOT__dl_q[__Vi0] = VL_SCOPED_RAND_RESET_I(16, __VscopeHash, 10253038534133807166ull);
    }
    vlSelf->radar_decim4__DOT__u_hb1__DOT__dphase = VL_SCOPED_RAND_RESET_I(1, __VscopeHash, 16497771024444963091ull);
    vlSelf->radar_decim4__DOT__u_hb1__DOT__due_s1 = VL_SCOPED_RAND_RESET_I(1, __VscopeHash, 16631445566953049214ull);
    for (int __Vi0 = 0; __Vi0 < 7; ++__Vi0) {
        vlSelf->radar_decim4__DOT__u_hb1__DOT__psum_i[__Vi0] = VL_SCOPED_RAND_RESET_I(17, __VscopeHash, 6391384858238231091ull);
    }
    for (int __Vi0 = 0; __Vi0 < 7; ++__Vi0) {
        vlSelf->radar_decim4__DOT__u_hb1__DOT__psum_q[__Vi0] = VL_SCOPED_RAND_RESET_I(17, __VscopeHash, 8762261055542532162ull);
    }
    for (int __Vi0 = 0; __Vi0 < 7; ++__Vi0) {
        vlSelf->radar_decim4__DOT__u_hb1__DOT__pi_s2[__Vi0] = VL_SCOPED_RAND_RESET_Q(35, __VscopeHash, 11345127610620150994ull);
    }
    for (int __Vi0 = 0; __Vi0 < 7; ++__Vi0) {
        vlSelf->radar_decim4__DOT__u_hb1__DOT__pq_s2[__Vi0] = VL_SCOPED_RAND_RESET_Q(35, __VscopeHash, 16192810739942726810ull);
    }
    vlSelf->radar_decim4__DOT__u_hb1__DOT__v_s2 = VL_SCOPED_RAND_RESET_I(1, __VscopeHash, 12118229658396221117ull);
    for (int __Vi0 = 0; __Vi0 < 7; ++__Vi0) {
        vlSelf->radar_decim4__DOT__u_hb1__DOT__chain_i[__Vi0] = VL_SCOPED_RAND_RESET_Q(40, __VscopeHash, 10643486294152271368ull);
    }
    for (int __Vi0 = 0; __Vi0 < 7; ++__Vi0) {
        vlSelf->radar_decim4__DOT__u_hb1__DOT__chain_q[__Vi0] = VL_SCOPED_RAND_RESET_Q(40, __VscopeHash, 5999119977658357764ull);
    }
    vlSelf->radar_decim4__DOT__u_hb1__DOT__acc_i_s3 = VL_SCOPED_RAND_RESET_Q(40, __VscopeHash, 14787929658766398604ull);
    vlSelf->radar_decim4__DOT__u_hb1__DOT__acc_q_s3 = VL_SCOPED_RAND_RESET_Q(40, __VscopeHash, 11120905053817702712ull);
    vlSelf->radar_decim4__DOT__u_hb1__DOT__v_s3 = VL_SCOPED_RAND_RESET_I(1, __VscopeHash, 16806067889112182304ull);
    for (int __Vi0 = 0; __Vi0 < 1; ++__Vi0) {
        vlSelf->__VstlTriggered[__Vi0] = 0;
    }
    for (int __Vi0 = 0; __Vi0 < 2; ++__Vi0) {
        vlSelf->__VicoTriggered[__Vi0] = 0;
    }
    vlSelf->__Vtrigprevexpr___TOP__clk__0 = 0;
    vlSelf->__Vtrigprevexpr___TOP__rst__0 = 0;
    vlSelf->__Vtrigprevexpr___TOP__flush__0 = 0;
    vlSelf->__Vtrigprevexpr___TOP__in_valid__0 = 0;
    vlSelf->__Vtrigprevexpr___TOP__in_i__0 = 0;
    vlSelf->__Vtrigprevexpr___TOP__in_q__0 = 0;
    vlSelf->__VicoDidInit = 0;
    for (int __Vi0 = 0; __Vi0 < 1; ++__Vi0) {
        vlSelf->__VactTriggered[__Vi0] = 0;
    }
    vlSelf->__Vtrigprevexpr___TOP__clk__1 = 0;
    for (int __Vi0 = 0; __Vi0 < 1; ++__Vi0) {
        vlSelf->__VnbaTriggered[__Vi0] = 0;
    }
}
