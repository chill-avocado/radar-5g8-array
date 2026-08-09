// Verilated -*- C++ -*-
// DESCRIPTION: Verilator output: Design implementation internals
// See Vradar_dechirp.h for the primary calling header

#include "Vradar_dechirp__pch.h"

VL_ATTR_COLD void Vradar_dechirp___024root___eval_static(Vradar_dechirp___024root* vlSelf) {
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vradar_dechirp___024root___eval_static\n"); );
    Vradar_dechirp__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    auto& vlSelfRef = std::ref(*vlSelf).get();
    // Body
    {
        // Inlined CFunc: _eval_static__TOP
        QData/*33:0*/ __Vinline_0__eval_static__TOP_radar_dechirp__DOT__round_sat__Vstatic__half;
        __Vinline_0__eval_static__TOP_radar_dechirp__DOT__round_sat__Vstatic__half = 0;
        QData/*33:0*/ __Vinline_0__eval_static__TOP_radar_dechirp__DOT__round_sat__Vstatic__t;
        __Vinline_0__eval_static__TOP_radar_dechirp__DOT__round_sat__Vstatic__t = 0;
        __Vinline_0__eval_static__TOP_radar_dechirp__DOT__round_sat__Vstatic__half = 0;
        __Vinline_0__eval_static__TOP_radar_dechirp__DOT__round_sat__Vstatic__t = 0;
    }
    vlSelfRef.__Vtrigprevexpr___TOP__clk__0 = vlSelfRef.clk;
}

VL_ATTR_COLD void Vradar_dechirp___024root___eval_initial(Vradar_dechirp___024root* vlSelf) {
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vradar_dechirp___024root___eval_initial\n"); );
    Vradar_dechirp__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    auto& vlSelfRef = std::ref(*vlSelf).get();
}

VL_ATTR_COLD void Vradar_dechirp___024root___eval_final(Vradar_dechirp___024root* vlSelf) {
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vradar_dechirp___024root___eval_final\n"); );
    Vradar_dechirp__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    auto& vlSelfRef = std::ref(*vlSelf).get();
}

VL_ATTR_COLD void Vradar_dechirp___024root___eval_settle(Vradar_dechirp___024root* vlSelf) {
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vradar_dechirp___024root___eval_settle\n"); );
    Vradar_dechirp__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    auto& vlSelfRef = std::ref(*vlSelf).get();
}

bool Vradar_dechirp___024root___trigger_anySet__act(const VlUnpacked<QData/*63:0*/, 1> &in);

#ifdef VL_DEBUG
VL_ATTR_COLD void Vradar_dechirp___024root___dump_triggers__act(const VlUnpacked<QData/*63:0*/, 1> &triggers, const std::string &tag) {
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vradar_dechirp___024root___dump_triggers__act\n"); );
    // Body
    if ((1U & (~ (IData)(Vradar_dechirp___024root___trigger_anySet__act(triggers))))) {
        VL_DBG_MSGS("         No '" + tag + "' region triggers active\n");
    }
    if ((1U & (IData)(triggers[0U]))) {
        VL_DBG_MSGS("         '" + tag + "' region trigger index 0 is active: @(posedge clk)\n");
    }
}
#endif  // VL_DEBUG

VL_ATTR_COLD void Vradar_dechirp___024root___ctor_var_reset(Vradar_dechirp___024root* vlSelf) {
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vradar_dechirp___024root___ctor_var_reset\n"); );
    Vradar_dechirp__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    auto& vlSelfRef = std::ref(*vlSelf).get();
    // Body
    const uint64_t __VscopeHash = VL_MURMUR64_HASH(vlSelf->vlNamep);
    vlSelf->clk = VL_SCOPED_RAND_RESET_I(1, __VscopeHash, 16707436170211756652ull);
    vlSelf->rst = VL_SCOPED_RAND_RESET_I(1, __VscopeHash, 18209466448985614591ull);
    vlSelf->in_valid = VL_SCOPED_RAND_RESET_I(1, __VscopeHash, 2339549897027650563ull);
    vlSelf->in_i = VL_SCOPED_RAND_RESET_I(16, __VscopeHash, 10846626665951073823ull);
    vlSelf->in_q = VL_SCOPED_RAND_RESET_I(16, __VscopeHash, 5883516748077548659ull);
    vlSelf->ref_i = VL_SCOPED_RAND_RESET_I(16, __VscopeHash, 15391989574906886157ull);
    vlSelf->ref_q = VL_SCOPED_RAND_RESET_I(16, __VscopeHash, 13194073008019969109ull);
    vlSelf->shift = VL_SCOPED_RAND_RESET_I(4, __VscopeHash, 16810897957468562620ull);
    vlSelf->out_valid = VL_SCOPED_RAND_RESET_I(1, __VscopeHash, 2886291494070200219ull);
    vlSelf->out_i = VL_SCOPED_RAND_RESET_I(16, __VscopeHash, 15869654417598851880ull);
    vlSelf->out_q = VL_SCOPED_RAND_RESET_I(16, __VscopeHash, 2605090817737850913ull);
    vlSelf->radar_dechirp__DOT__a_i_s1 = VL_SCOPED_RAND_RESET_I(16, __VscopeHash, 9465680843442542122ull);
    vlSelf->radar_dechirp__DOT__a_q_s1 = VL_SCOPED_RAND_RESET_I(16, __VscopeHash, 10768842920566888841ull);
    vlSelf->radar_dechirp__DOT__b_i_s1 = VL_SCOPED_RAND_RESET_I(16, __VscopeHash, 18318990706780157210ull);
    vlSelf->radar_dechirp__DOT__b_q_s1 = VL_SCOPED_RAND_RESET_I(16, __VscopeHash, 5076541177148125698ull);
    vlSelf->radar_dechirp__DOT__sh_s1 = VL_SCOPED_RAND_RESET_I(6, __VscopeHash, 18428613969439172279ull);
    vlSelf->radar_dechirp__DOT__v_s1 = VL_SCOPED_RAND_RESET_I(1, __VscopeHash, 2758539375958904402ull);
    vlSelf->radar_dechirp__DOT__p_ii_s2 = VL_SCOPED_RAND_RESET_I(32, __VscopeHash, 17477409260638235350ull);
    vlSelf->radar_dechirp__DOT__p_qq_s2 = VL_SCOPED_RAND_RESET_I(32, __VscopeHash, 16462135689829204330ull);
    vlSelf->radar_dechirp__DOT__p_iq_s2 = VL_SCOPED_RAND_RESET_I(32, __VscopeHash, 15572834940252311539ull);
    vlSelf->radar_dechirp__DOT__p_qi_s2 = VL_SCOPED_RAND_RESET_I(32, __VscopeHash, 6273626294898968781ull);
    vlSelf->radar_dechirp__DOT__sh_s2 = VL_SCOPED_RAND_RESET_I(6, __VscopeHash, 5228825557295269319ull);
    vlSelf->radar_dechirp__DOT__v_s2 = VL_SCOPED_RAND_RESET_I(1, __VscopeHash, 10854406885623668396ull);
    vlSelf->radar_dechirp__DOT__rr_s3 = VL_SCOPED_RAND_RESET_Q(34, __VscopeHash, 7796375966236200272ull);
    vlSelf->radar_dechirp__DOT__ii_s3 = VL_SCOPED_RAND_RESET_Q(34, __VscopeHash, 7538857329315602824ull);
    vlSelf->radar_dechirp__DOT__sh_s3 = VL_SCOPED_RAND_RESET_I(6, __VscopeHash, 16909237966299754698ull);
    vlSelf->radar_dechirp__DOT__v_s3 = VL_SCOPED_RAND_RESET_I(1, __VscopeHash, 14025257454160527779ull);
    for (int __Vi0 = 0; __Vi0 < 1; ++__Vi0) {
        vlSelf->__VactTriggered[__Vi0] = 0;
    }
    vlSelf->__Vtrigprevexpr___TOP__clk__0 = 0;
    for (int __Vi0 = 0; __Vi0 < 1; ++__Vi0) {
        vlSelf->__VnbaTriggered[__Vi0] = 0;
    }
}
