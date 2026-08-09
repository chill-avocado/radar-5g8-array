// Verilated -*- C++ -*-
// DESCRIPTION: Verilator output: Design implementation internals
// See Vradar_window.h for the primary calling header

#include "Vradar_window__pch.h"

VL_ATTR_COLD void Vradar_window___024root___eval_static(Vradar_window___024root* vlSelf) {
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vradar_window___024root___eval_static\n"); );
    Vradar_window__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    auto& vlSelfRef = std::ref(*vlSelf).get();
    // Body
    {
        // Inlined CFunc: _eval_static__TOP
        const uint64_t __VscopeHash = VL_MURMUR64_HASH(vlSelf->vlNamep);
        vlSelfRef.radar_window__DOT__round_sat__Vstatic__t = VL_SCOPED_RAND_RESET_I(32, __VscopeHash, 10334283340721237566ull);
    }
    vlSelfRef.__Vtrigprevexpr___TOP__clk__0 = vlSelfRef.clk;
}

VL_ATTR_COLD void Vradar_window___024root___eval_initial(Vradar_window___024root* vlSelf) {
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vradar_window___024root___eval_initial\n"); );
    Vradar_window__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    auto& vlSelfRef = std::ref(*vlSelf).get();
    // Body
    {
        // Inlined CFunc: _eval_initial__TOP
        IData/*31:0*/ __Vinline_0__eval_initial__TOP_radar_window__DOT__wi;
        __Vinline_0__eval_initial__TOP_radar_window__DOT__wi = 0;
        __Vinline_0__eval_initial__TOP_radar_window__DOT__wi = 0U;
        while (VL_GTS_III(32, 0x00000400U, __Vinline_0__eval_initial__TOP_radar_window__DOT__wi)) {
            vlSelfRef.radar_window__DOT__win[(0x000003ffU 
                                              & __Vinline_0__eval_initial__TOP_radar_window__DOT__wi)] 
                = (VL_GTS_III(32, 0x00000300U, __Vinline_0__eval_initial__TOP_radar_window__DOT__wi)
                    ? 0x7fffU : 0U);
            __Vinline_0__eval_initial__TOP_radar_window__DOT__wi 
                = ((IData)(1U) + __Vinline_0__eval_initial__TOP_radar_window__DOT__wi);
        }
    }
}

VL_ATTR_COLD void Vradar_window___024root___eval_final(Vradar_window___024root* vlSelf) {
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vradar_window___024root___eval_final\n"); );
    Vradar_window__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    auto& vlSelfRef = std::ref(*vlSelf).get();
}

VL_ATTR_COLD void Vradar_window___024root___eval_settle(Vradar_window___024root* vlSelf) {
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vradar_window___024root___eval_settle\n"); );
    Vradar_window__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    auto& vlSelfRef = std::ref(*vlSelf).get();
}

bool Vradar_window___024root___trigger_anySet__act(const VlUnpacked<QData/*63:0*/, 1> &in);

#ifdef VL_DEBUG
VL_ATTR_COLD void Vradar_window___024root___dump_triggers__act(const VlUnpacked<QData/*63:0*/, 1> &triggers, const std::string &tag) {
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vradar_window___024root___dump_triggers__act\n"); );
    // Body
    if ((1U & (~ (IData)(Vradar_window___024root___trigger_anySet__act(triggers))))) {
        VL_DBG_MSGS("         No '" + tag + "' region triggers active\n");
    }
    if ((1U & (IData)(triggers[0U]))) {
        VL_DBG_MSGS("         '" + tag + "' region trigger index 0 is active: @(posedge clk)\n");
    }
}
#endif  // VL_DEBUG

VL_ATTR_COLD void Vradar_window___024root___ctor_var_reset(Vradar_window___024root* vlSelf) {
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vradar_window___024root___ctor_var_reset\n"); );
    Vradar_window__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    auto& vlSelfRef = std::ref(*vlSelf).get();
    // Body
    const uint64_t __VscopeHash = VL_MURMUR64_HASH(vlSelf->vlNamep);
    vlSelf->clk = VL_SCOPED_RAND_RESET_I(1, __VscopeHash, 16707436170211756652ull);
    vlSelf->rst = VL_SCOPED_RAND_RESET_I(1, __VscopeHash, 18209466448985614591ull);
    vlSelf->in_valid = VL_SCOPED_RAND_RESET_I(1, __VscopeHash, 2339549897027650563ull);
    vlSelf->in_i = VL_SCOPED_RAND_RESET_I(16, __VscopeHash, 10846626665951073823ull);
    vlSelf->in_q = VL_SCOPED_RAND_RESET_I(16, __VscopeHash, 5883516748077548659ull);
    vlSelf->sample_idx = VL_SCOPED_RAND_RESET_I(16, __VscopeHash, 17670071048999531770ull);
    vlSelf->coef_we = VL_SCOPED_RAND_RESET_I(1, __VscopeHash, 18324404074801927116ull);
    vlSelf->coef_addr = VL_SCOPED_RAND_RESET_I(16, __VscopeHash, 11097765260257993570ull);
    vlSelf->coef_data = VL_SCOPED_RAND_RESET_I(16, __VscopeHash, 5483680415497960545ull);
    vlSelf->out_valid = VL_SCOPED_RAND_RESET_I(1, __VscopeHash, 2886291494070200219ull);
    vlSelf->out_i = VL_SCOPED_RAND_RESET_I(16, __VscopeHash, 15869654417598851880ull);
    vlSelf->out_q = VL_SCOPED_RAND_RESET_I(16, __VscopeHash, 2605090817737850913ull);
    for (int __Vi0 = 0; __Vi0 < 1024; ++__Vi0) {
        vlSelf->radar_window__DOT__win[__Vi0] = VL_SCOPED_RAND_RESET_I(16, __VscopeHash, 4891273302989791709ull);
    }
    vlSelf->radar_window__DOT__coef_s1 = VL_SCOPED_RAND_RESET_I(16, __VscopeHash, 2684729403188297111ull);
    vlSelf->radar_window__DOT__d_i_s1 = VL_SCOPED_RAND_RESET_I(16, __VscopeHash, 9799567060125895972ull);
    vlSelf->radar_window__DOT__d_q_s1 = VL_SCOPED_RAND_RESET_I(16, __VscopeHash, 4040593240426698146ull);
    vlSelf->radar_window__DOT__oob_s1 = VL_SCOPED_RAND_RESET_I(1, __VscopeHash, 11167615891693812349ull);
    vlSelf->radar_window__DOT__v_s1 = VL_SCOPED_RAND_RESET_I(1, __VscopeHash, 2698493159740087560ull);
    vlSelf->radar_window__DOT__p_i_s2 = VL_SCOPED_RAND_RESET_I(32, __VscopeHash, 1040962256820761817ull);
    vlSelf->radar_window__DOT__p_q_s2 = VL_SCOPED_RAND_RESET_I(32, __VscopeHash, 9799790966042018639ull);
    vlSelf->radar_window__DOT__oob_s2 = VL_SCOPED_RAND_RESET_I(1, __VscopeHash, 11730150133833426644ull);
    vlSelf->radar_window__DOT__v_s2 = VL_SCOPED_RAND_RESET_I(1, __VscopeHash, 16128584619095117594ull);
    for (int __Vi0 = 0; __Vi0 < 1; ++__Vi0) {
        vlSelf->__VactTriggered[__Vi0] = 0;
    }
    vlSelf->__Vtrigprevexpr___TOP__clk__0 = 0;
    for (int __Vi0 = 0; __Vi0 < 1; ++__Vi0) {
        vlSelf->__VnbaTriggered[__Vi0] = 0;
    }
}
