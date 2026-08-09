// Verilated -*- C++ -*-
// DESCRIPTION: Verilator output: Design implementation internals
// See Vradar_regs.h for the primary calling header

#include "Vradar_regs__pch.h"

VL_ATTR_COLD void Vradar_regs___024root___eval_static(Vradar_regs___024root* vlSelf) {
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vradar_regs___024root___eval_static\n"); );
    Vradar_regs__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    auto& vlSelfRef = std::ref(*vlSelf).get();
    // Body
    vlSelfRef.__Vtrigprevexpr___TOP__clk__0 = vlSelfRef.clk;
}

VL_ATTR_COLD void Vradar_regs___024root___eval_initial(Vradar_regs___024root* vlSelf) {
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vradar_regs___024root___eval_initial\n"); );
    Vradar_regs__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    auto& vlSelfRef = std::ref(*vlSelf).get();
}

VL_ATTR_COLD void Vradar_regs___024root___eval_final(Vradar_regs___024root* vlSelf) {
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vradar_regs___024root___eval_final\n"); );
    Vradar_regs__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    auto& vlSelfRef = std::ref(*vlSelf).get();
}

VL_ATTR_COLD void Vradar_regs___024root___eval_settle(Vradar_regs___024root* vlSelf) {
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vradar_regs___024root___eval_settle\n"); );
    Vradar_regs__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    auto& vlSelfRef = std::ref(*vlSelf).get();
}

bool Vradar_regs___024root___trigger_anySet__act(const VlUnpacked<QData/*63:0*/, 1> &in);

#ifdef VL_DEBUG
VL_ATTR_COLD void Vradar_regs___024root___dump_triggers__act(const VlUnpacked<QData/*63:0*/, 1> &triggers, const std::string &tag) {
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vradar_regs___024root___dump_triggers__act\n"); );
    // Body
    if ((1U & (~ (IData)(Vradar_regs___024root___trigger_anySet__act(triggers))))) {
        VL_DBG_MSGS("         No '" + tag + "' region triggers active\n");
    }
    if ((1U & (IData)(triggers[0U]))) {
        VL_DBG_MSGS("         '" + tag + "' region trigger index 0 is active: @(posedge clk)\n");
    }
}
#endif  // VL_DEBUG

VL_ATTR_COLD void Vradar_regs___024root___ctor_var_reset(Vradar_regs___024root* vlSelf) {
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vradar_regs___024root___ctor_var_reset\n"); );
    Vradar_regs__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    auto& vlSelfRef = std::ref(*vlSelf).get();
    // Body
    const uint64_t __VscopeHash = VL_MURMUR64_HASH(vlSelf->vlNamep);
    vlSelf->clk = VL_SCOPED_RAND_RESET_I(1, __VscopeHash, 16707436170211756652ull);
    vlSelf->rst = VL_SCOPED_RAND_RESET_I(1, __VscopeHash, 18209466448985614591ull);
    vlSelf->set_stb = VL_SCOPED_RAND_RESET_I(1, __VscopeHash, 14234607486782602480ull);
    vlSelf->set_addr = VL_SCOPED_RAND_RESET_I(8, __VscopeHash, 13774424473268055117ull);
    vlSelf->set_data = VL_SCOPED_RAND_RESET_I(32, __VscopeHash, 5103980457571133353ull);
    vlSelf->ctrl_enable = VL_SCOPED_RAND_RESET_I(1, __VscopeHash, 10003822029857624607ull);
    vlSelf->ctrl_soft_reset = VL_SCOPED_RAND_RESET_I(1, __VscopeHash, 4302175319429225574ull);
    vlSelf->ctrl_mimo_mode = VL_SCOPED_RAND_RESET_I(2, __VscopeHash, 15196637843835880624ull);
    vlSelf->ctrl_tx_enable = VL_SCOPED_RAND_RESET_I(1, __VscopeHash, 10467245493193958518ull);
    vlSelf->ctrl_map_enable = VL_SCOPED_RAND_RESET_I(1, __VscopeHash, 9938368511094517091ull);
    vlSelf->ctrl_hits_enable = VL_SCOPED_RAND_RESET_I(1, __VscopeHash, 16351366013832975357ull);
    vlSelf->ctrl_loopback = VL_SCOPED_RAND_RESET_I(1, __VscopeHash, 4232940249142284616ull);
    vlSelf->ctrl_frame_limit = VL_SCOPED_RAND_RESET_I(16, __VscopeHash, 16240405486278929914ull);
    vlSelf->freq_start = VL_SCOPED_RAND_RESET_I(32, __VscopeHash, 13316347847650001447ull);
    vlSelf->freq_slope = VL_SCOPED_RAND_RESET_I(32, __VscopeHash, 12208978262376677366ull);
    vlSelf->t_sweep = VL_SCOPED_RAND_RESET_I(16, __VscopeHash, 16985786760741074857ull);
    vlSelf->t_pri = VL_SCOPED_RAND_RESET_I(16, __VscopeHash, 10889341233623587760ull);
    vlSelf->n_chirp = VL_SCOPED_RAND_RESET_I(16, __VscopeHash, 12016554950783984648ull);
    vlSelf->tx_gain = VL_SCOPED_RAND_RESET_I(16, __VscopeHash, 10316176915558660220ull);
    vlSelf->dechirp_sh = VL_SCOPED_RAND_RESET_I(4, __VscopeHash, 16815013781033781163ull);
    vlSelf->fft_scale_r = VL_SCOPED_RAND_RESET_I(20, __VscopeHash, 14708166611380693692ull);
    vlSelf->fft_scale_d = VL_SCOPED_RAND_RESET_I(16, __VscopeHash, 17674436713191512918ull);
    vlSelf->win_we = VL_SCOPED_RAND_RESET_I(1, __VscopeHash, 18403350018434381785ull);
    vlSelf->win_addr = VL_SCOPED_RAND_RESET_I(16, __VscopeHash, 10638848447694187476ull);
    vlSelf->win_data = VL_SCOPED_RAND_RESET_I(32, __VscopeHash, 10380941373526311735ull);
    vlSelf->cfar_guard_range = VL_SCOPED_RAND_RESET_I(4, __VscopeHash, 15894866134060442837ull);
    vlSelf->cfar_guard_dopp = VL_SCOPED_RAND_RESET_I(4, __VscopeHash, 4315520302272586677ull);
    vlSelf->cfar_train_range = VL_SCOPED_RAND_RESET_I(4, __VscopeHash, 4788133045186950867ull);
    vlSelf->cfar_train_dopp = VL_SCOPED_RAND_RESET_I(4, __VscopeHash, 3801028550347556552ull);
    vlSelf->cfar_kind = VL_SCOPED_RAND_RESET_I(2, __VscopeHash, 14562041795142143445ull);
    vlSelf->cfar_alpha = VL_SCOPED_RAND_RESET_I(32, __VscopeHash, 117899563943267484ull);
    vlSelf->range_zero = VL_SCOPED_RAND_RESET_I(16, __VscopeHash, 13076057408443441518ull);
    vlSelf->map_decim_r = VL_SCOPED_RAND_RESET_I(8, __VscopeHash, 3089190061614830083ull);
    vlSelf->map_decim_d = VL_SCOPED_RAND_RESET_I(8, __VscopeHash, 17377296794552598346ull);
    vlSelf->max_hits = VL_SCOPED_RAND_RESET_I(16, __VscopeHash, 16610757387192835098ull);
    vlSelf->zero_dopp = VL_SCOPED_RAND_RESET_I(8, __VscopeHash, 3226769754086127329ull);
    vlSelf->geom_n_range_log2 = VL_SCOPED_RAND_RESET_I(4, __VscopeHash, 9844628273695871242ull);
    vlSelf->geom_n_chirp_log2 = VL_SCOPED_RAND_RESET_I(4, __VscopeHash, 11174265533744934382ull);
    vlSelf->test_tone = VL_SCOPED_RAND_RESET_I(32, __VscopeHash, 13934232463617975816ull);
    vlSelf->version_stb = VL_SCOPED_RAND_RESET_I(1, __VscopeHash, 12245013926381597629ull);
    for (int __Vi0 = 0; __Vi0 < 1; ++__Vi0) {
        vlSelf->__VactTriggered[__Vi0] = 0;
    }
    vlSelf->__Vtrigprevexpr___TOP__clk__0 = 0;
    for (int __Vi0 = 0; __Vi0 < 1; ++__Vi0) {
        vlSelf->__VnbaTriggered[__Vi0] = 0;
    }
}
