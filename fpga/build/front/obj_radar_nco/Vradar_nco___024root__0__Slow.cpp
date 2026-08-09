// Verilated -*- C++ -*-
// DESCRIPTION: Verilator output: Design implementation internals
// See Vradar_nco.h for the primary calling header

#include "Vradar_nco__pch.h"

VL_ATTR_COLD void Vradar_nco___024root___eval_static(Vradar_nco___024root* vlSelf) {
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vradar_nco___024root___eval_static\n"); );
    Vradar_nco__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    auto& vlSelfRef = std::ref(*vlSelf).get();
    // Body
    vlSelfRef.__Vtrigprevexpr___TOP__clk__0 = vlSelfRef.clk;
}

VL_ATTR_COLD void Vradar_nco___024root___eval_initial(Vradar_nco___024root* vlSelf) {
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vradar_nco___024root___eval_initial\n"); );
    Vradar_nco__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    auto& vlSelfRef = std::ref(*vlSelf).get();
    // Body
    {
        // Inlined CFunc: _eval_initial__TOP
        IData/*31:0*/ __Vinline_0__eval_initial__TOP_radar_nco__DOT__k;
        __Vinline_0__eval_initial__TOP_radar_nco__DOT__k = 0;
        double __Vinline_0__eval_initial__TOP_radar_nco__DOT__pi_r;
        __Vinline_0__eval_initial__TOP_radar_nco__DOT__pi_r = 0;
        double __Vinline_0__eval_initial__TOP_radar_nco__DOT__amp_r;
        __Vinline_0__eval_initial__TOP_radar_nco__DOT__amp_r = 0;
        double __Vinline_0__eval_initial__TOP_radar_nco__DOT__nph_r;
        __Vinline_0__eval_initial__TOP_radar_nco__DOT__nph_r = 0;
        __Vinline_0__eval_initial__TOP_radar_nco__DOT__pi_r = 3.14159265358979312e+00;
        __Vinline_0__eval_initial__TOP_radar_nco__DOT__amp_r = 3.27670000000000000e+04;
        __Vinline_0__eval_initial__TOP_radar_nco__DOT__nph_r = 4.09600000000000000e+03;
        __Vinline_0__eval_initial__TOP_radar_nco__DOT__k = 0U;
        while (VL_GTS_III(32, 0x00000400U, __Vinline_0__eval_initial__TOP_radar_nco__DOT__k)) {
            vlSelfRef.radar_nco__DOT__ang_r = (2.0 
                                               * (__Vinline_0__eval_initial__TOP_radar_nco__DOT__pi_r 
                                                  * 
                                                  ((5.00000000000000000e-01 
                                                    + 
                                                    VL_ISTOR_D_I(32, __Vinline_0__eval_initial__TOP_radar_nco__DOT__k)) 
                                                   / __Vinline_0__eval_initial__TOP_radar_nco__DOT__nph_r)));
            vlSelfRef.radar_nco__DOT__rv = VL_RTOI_I_D(
                                                       (5.00000000000000000e-01 
                                                        + 
                                                        (__Vinline_0__eval_initial__TOP_radar_nco__DOT__amp_r 
                                                         * 
                                                         sin(vlSelfRef.radar_nco__DOT__ang_r))));
            if (VL_LTS_III(32, 0x00007fffU, vlSelfRef.radar_nco__DOT__rv)) {
                vlSelfRef.radar_nco__DOT__rv = 0x00007fffU;
            }
            vlSelfRef.radar_nco__DOT__sin_rom[(0x000003ffU 
                                               & __Vinline_0__eval_initial__TOP_radar_nco__DOT__k)] 
                = (0x0000ffffU & vlSelfRef.radar_nco__DOT__rv);
            __Vinline_0__eval_initial__TOP_radar_nco__DOT__k 
                = ((IData)(1U) + __Vinline_0__eval_initial__TOP_radar_nco__DOT__k);
        }
    }
}

VL_ATTR_COLD void Vradar_nco___024root___eval_final(Vradar_nco___024root* vlSelf) {
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vradar_nco___024root___eval_final\n"); );
    Vradar_nco__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    auto& vlSelfRef = std::ref(*vlSelf).get();
}

VL_ATTR_COLD void Vradar_nco___024root___eval_settle(Vradar_nco___024root* vlSelf) {
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vradar_nco___024root___eval_settle\n"); );
    Vradar_nco__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    auto& vlSelfRef = std::ref(*vlSelf).get();
}

bool Vradar_nco___024root___trigger_anySet__act(const VlUnpacked<QData/*63:0*/, 1> &in);

#ifdef VL_DEBUG
VL_ATTR_COLD void Vradar_nco___024root___dump_triggers__act(const VlUnpacked<QData/*63:0*/, 1> &triggers, const std::string &tag) {
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vradar_nco___024root___dump_triggers__act\n"); );
    // Body
    if ((1U & (~ (IData)(Vradar_nco___024root___trigger_anySet__act(triggers))))) {
        VL_DBG_MSGS("         No '" + tag + "' region triggers active\n");
    }
    if ((1U & (IData)(triggers[0U]))) {
        VL_DBG_MSGS("         '" + tag + "' region trigger index 0 is active: @(posedge clk)\n");
    }
}
#endif  // VL_DEBUG

VL_ATTR_COLD void Vradar_nco___024root___ctor_var_reset(Vradar_nco___024root* vlSelf) {
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vradar_nco___024root___ctor_var_reset\n"); );
    Vradar_nco__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    auto& vlSelfRef = std::ref(*vlSelf).get();
    // Body
    const uint64_t __VscopeHash = VL_MURMUR64_HASH(vlSelf->vlNamep);
    vlSelf->clk = VL_SCOPED_RAND_RESET_I(1, __VscopeHash, 16707436170211756652ull);
    vlSelf->rst = VL_SCOPED_RAND_RESET_I(1, __VscopeHash, 18209466448985614591ull);
    vlSelf->ena = VL_SCOPED_RAND_RESET_I(1, __VscopeHash, 4194194277674688301ull);
    vlSelf->restart = VL_SCOPED_RAND_RESET_I(1, __VscopeHash, 10121376494823899511ull);
    vlSelf->freq_start = VL_SCOPED_RAND_RESET_I(32, __VscopeHash, 13316347847650001447ull);
    vlSelf->freq_slope = VL_SCOPED_RAND_RESET_I(32, __VscopeHash, 12208978262376677366ull);
    vlSelf->out_i = VL_SCOPED_RAND_RESET_I(16, __VscopeHash, 15869654417598851880ull);
    vlSelf->out_q = VL_SCOPED_RAND_RESET_I(16, __VscopeHash, 2605090817737850913ull);
    vlSelf->out_valid = VL_SCOPED_RAND_RESET_I(1, __VscopeHash, 2886291494070200219ull);
    for (int __Vi0 = 0; __Vi0 < 1024; ++__Vi0) {
        vlSelf->radar_nco__DOT__sin_rom[__Vi0] = VL_SCOPED_RAND_RESET_I(16, __VscopeHash, 6647503477930208901ull);
    }
    vlSelf->radar_nco__DOT__rv = VL_SCOPED_RAND_RESET_I(32, __VscopeHash, 1521431352989555875ull);
    vlSelf->radar_nco__DOT__ang_r = 0;
    vlSelf->radar_nco__DOT__phase = VL_SCOPED_RAND_RESET_I(32, __VscopeHash, 10430129807191625486ull);
    vlSelf->radar_nco__DOT__freq_cur = VL_SCOPED_RAND_RESET_I(32, __VscopeHash, 11867155213642958854ull);
    vlSelf->radar_nco__DOT__addr_i_s1 = VL_SCOPED_RAND_RESET_I(10, __VscopeHash, 12102699202183327152ull);
    vlSelf->radar_nco__DOT__addr_q_s1 = VL_SCOPED_RAND_RESET_I(10, __VscopeHash, 15040485500268885099ull);
    vlSelf->radar_nco__DOT__neg_i_s1 = VL_SCOPED_RAND_RESET_I(1, __VscopeHash, 2315353634353348286ull);
    vlSelf->radar_nco__DOT__neg_q_s1 = VL_SCOPED_RAND_RESET_I(1, __VscopeHash, 13671434337753513919ull);
    vlSelf->radar_nco__DOT__vld_s1 = VL_SCOPED_RAND_RESET_I(1, __VscopeHash, 6229733906908512241ull);
    vlSelf->radar_nco__DOT__rom_i_s2 = VL_SCOPED_RAND_RESET_I(16, __VscopeHash, 6447289039070099953ull);
    vlSelf->radar_nco__DOT__rom_q_s2 = VL_SCOPED_RAND_RESET_I(16, __VscopeHash, 11782029354471708513ull);
    vlSelf->radar_nco__DOT__neg_i_s2 = VL_SCOPED_RAND_RESET_I(1, __VscopeHash, 15950050750225174833ull);
    vlSelf->radar_nco__DOT__neg_q_s2 = VL_SCOPED_RAND_RESET_I(1, __VscopeHash, 8191074152837745078ull);
    vlSelf->radar_nco__DOT__vld_s2 = VL_SCOPED_RAND_RESET_I(1, __VscopeHash, 16996365302018553708ull);
    for (int __Vi0 = 0; __Vi0 < 1; ++__Vi0) {
        vlSelf->__VactTriggered[__Vi0] = 0;
    }
    vlSelf->__Vtrigprevexpr___TOP__clk__0 = 0;
    for (int __Vi0 = 0; __Vi0 < 1; ++__Vi0) {
        vlSelf->__VnbaTriggered[__Vi0] = 0;
    }
}
