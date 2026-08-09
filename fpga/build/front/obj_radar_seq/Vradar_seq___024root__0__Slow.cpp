// Verilated -*- C++ -*-
// DESCRIPTION: Verilator output: Design implementation internals
// See Vradar_seq.h for the primary calling header

#include "Vradar_seq__pch.h"

VL_ATTR_COLD void Vradar_seq___024root___eval_static(Vradar_seq___024root* vlSelf) {
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vradar_seq___024root___eval_static\n"); );
    Vradar_seq__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    auto& vlSelfRef = std::ref(*vlSelf).get();
    // Body
    vlSelfRef.__Vtrigprevexpr___TOP__clk__0 = vlSelfRef.clk;
    vlSelfRef.__Vtrigprevexpr___TOP__rst__0 = vlSelfRef.rst;
    vlSelfRef.__Vtrigprevexpr___TOP__enable__0 = vlSelfRef.enable;
    vlSelfRef.__Vtrigprevexpr___TOP__mimo_mode__0 = vlSelfRef.mimo_mode;
    vlSelfRef.__Vtrigprevexpr___TOP__tx_enable__0 = vlSelfRef.tx_enable;
    vlSelfRef.__Vtrigprevexpr___TOP__t_sweep__0 = vlSelfRef.t_sweep;
    vlSelfRef.__Vtrigprevexpr___TOP__t_pri__0 = vlSelfRef.t_pri;
    vlSelfRef.__Vtrigprevexpr___TOP__n_chirp__0 = vlSelfRef.n_chirp;
    vlSelfRef.__Vtrigprevexpr___TOP__clk__1 = vlSelfRef.clk;
}

VL_ATTR_COLD void Vradar_seq___024root___eval_initial(Vradar_seq___024root* vlSelf) {
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vradar_seq___024root___eval_initial\n"); );
    Vradar_seq__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    auto& vlSelfRef = std::ref(*vlSelf).get();
}

VL_ATTR_COLD void Vradar_seq___024root___eval_final(Vradar_seq___024root* vlSelf) {
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vradar_seq___024root___eval_final\n"); );
    Vradar_seq__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    auto& vlSelfRef = std::ref(*vlSelf).get();
}

#ifdef VL_DEBUG
VL_ATTR_COLD void Vradar_seq___024root___dump_triggers__stl(const VlUnpacked<QData/*63:0*/, 1> &triggers, const std::string &tag);
#endif  // VL_DEBUG
VL_ATTR_COLD bool Vradar_seq___024root___eval_phase__stl(Vradar_seq___024root* vlSelf);

VL_ATTR_COLD void Vradar_seq___024root___eval_settle(Vradar_seq___024root* vlSelf) {
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vradar_seq___024root___eval_settle\n"); );
    Vradar_seq__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    auto& vlSelfRef = std::ref(*vlSelf).get();
    // Locals
    IData/*31:0*/ __VstlIterCount;
    // Body
    __VstlIterCount = 0U;
    vlSelfRef.__VstlFirstIteration = 1U;
    do {
        if (VL_UNLIKELY(((0x00002710U < __VstlIterCount)))) {
#ifdef VL_DEBUG
            Vradar_seq___024root___dump_triggers__stl(vlSelfRef.__VstlTriggered, "stl"s);
#endif
            VL_FATAL_MT("/Users/lucasnaylor/radar-5g8-array/fpga/sim/../rtl/radar_seq.v", 60, "", "DIDNOTCONVERGE: Settle region did not converge after '--converge-limit' of 10000 tries");
        }
        __VstlIterCount = ((IData)(1U) + __VstlIterCount);
        vlSelfRef.__VstlPhaseResult = Vradar_seq___024root___eval_phase__stl(vlSelf);
        vlSelfRef.__VstlFirstIteration = 0U;
    } while (vlSelfRef.__VstlPhaseResult);
}

VL_ATTR_COLD bool Vradar_seq___024root___trigger_anySet__stl(const VlUnpacked<QData/*63:0*/, 1> &in);

#ifdef VL_DEBUG
VL_ATTR_COLD void Vradar_seq___024root___dump_triggers__stl(const VlUnpacked<QData/*63:0*/, 1> &triggers, const std::string &tag) {
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vradar_seq___024root___dump_triggers__stl\n"); );
    // Body
    if ((1U & (~ (IData)(Vradar_seq___024root___trigger_anySet__stl(triggers))))) {
        VL_DBG_MSGS("         No '" + tag + "' region triggers active\n");
    }
    if ((1U & (IData)(triggers[0U]))) {
        VL_DBG_MSGS("         '" + tag + "' region trigger index 0 is active: Internal 'stl' trigger - first iteration\n");
    }
}
#endif  // VL_DEBUG

VL_ATTR_COLD bool Vradar_seq___024root___trigger_anySet__stl(const VlUnpacked<QData/*63:0*/, 1> &in) {
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vradar_seq___024root___trigger_anySet__stl\n"); );
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

VL_ATTR_COLD bool Vradar_seq___024root___eval_phase__stl(Vradar_seq___024root* vlSelf) {
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vradar_seq___024root___eval_phase__stl\n"); );
    Vradar_seq__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
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
        Vradar_seq___024root___dump_triggers__stl(vlSelfRef.__VstlTriggered, "stl"s);
    }
#endif
    __VstlExecute = Vradar_seq___024root___trigger_anySet__stl(vlSelfRef.__VstlTriggered);
    if (__VstlExecute) {
        {
            // Inlined CFunc: _eval_stl
            if ((1ULL & vlSelfRef.__VstlTriggered[0U])) {
                {
                    // Inlined CFunc: _stl_sequent__TOP__0
                    IData/*16:0*/ __Vinline_0__eval_stl___Vinline_0__stl_sequent__TOP__0_radar_seq__DOT__n_total;
                    __Vinline_0__eval_stl___Vinline_0__stl_sequent__TOP__0_radar_seq__DOT__n_total = 0;
                    __Vinline_0__eval_stl___Vinline_0__stl_sequent__TOP__0_radar_seq__DOT__n_total 
                        = ((0U == (IData)(vlSelfRef.mimo_mode))
                            ? ((IData)(vlSelfRef.n_chirp) 
                               << 1U) : (IData)(vlSelfRef.n_chirp));
                    vlSelfRef.radar_seq__DOT__t_pri_m1 
                        = (0x0000ffffU & (((IData)(vlSelfRef.t_pri) 
                                           - (IData)(1U)) 
                                          & (- (IData)(
                                                       (0U 
                                                        != (IData)(vlSelfRef.t_pri))))));
                    vlSelfRef.radar_seq__DOT__n_total_m1 
                        = (0x0001ffffU & ((__Vinline_0__eval_stl___Vinline_0__stl_sequent__TOP__0_radar_seq__DOT__n_total 
                                           - (IData)(1U)) 
                                          & (- (IData)(
                                                       (0U 
                                                        != __Vinline_0__eval_stl___Vinline_0__stl_sequent__TOP__0_radar_seq__DOT__n_total)))));
                    vlSelfRef.radar_seq__DOT__nx_chirp 
                        = vlSelfRef.radar_seq__DOT__chirp_cnt;
                    if (vlSelfRef.running) {
                        vlSelfRef.radar_seq__DOT__nx_running = 1U;
                        if (((IData)(vlSelfRef.radar_seq__DOT__pri_cnt) 
                             >= (IData)(vlSelfRef.radar_seq__DOT__t_pri_m1))) {
                            vlSelfRef.radar_seq__DOT__nx_pri = 0U;
                            if ((vlSelfRef.radar_seq__DOT__chirp_cnt 
                                 >= vlSelfRef.radar_seq__DOT__n_total_m1)) {
                                vlSelfRef.radar_seq__DOT__nx_chirp = 0U;
                                vlSelfRef.radar_seq__DOT__nx_running 
                                    = vlSelfRef.enable;
                            } else {
                                vlSelfRef.radar_seq__DOT__nx_chirp 
                                    = (0x0001ffffU 
                                       & ((IData)(1U) 
                                          + vlSelfRef.radar_seq__DOT__chirp_cnt));
                            }
                        } else {
                            vlSelfRef.radar_seq__DOT__nx_pri 
                                = (0x0000ffffU & ((IData)(1U) 
                                                  + (IData)(vlSelfRef.radar_seq__DOT__pri_cnt)));
                        }
                    } else {
                        vlSelfRef.radar_seq__DOT__nx_running = 0U;
                        vlSelfRef.radar_seq__DOT__nx_pri = 0U;
                        vlSelfRef.radar_seq__DOT__nx_chirp = 0U;
                        vlSelfRef.radar_seq__DOT__nx_running 
                            = vlSelfRef.enable;
                    }
                    vlSelfRef.radar_seq__DOT__nx_first 
                        = ((IData)(vlSelfRef.radar_seq__DOT__nx_running) 
                           & (0U == (IData)(vlSelfRef.radar_seq__DOT__nx_pri)));
                    vlSelfRef.radar_seq__DOT__nx_in_sweep 
                        = ((IData)(vlSelfRef.radar_seq__DOT__nx_running) 
                           & ((IData)(vlSelfRef.radar_seq__DOT__nx_pri) 
                              < (IData)(vlSelfRef.t_sweep)));
                    vlSelfRef.__VdfgRegularize_hebeb780c_0_0 
                        = (((IData)(vlSelfRef.radar_seq__DOT__nx_running) 
                            << 4U) | ((8U & (vlSelfRef.radar_seq__DOT__nx_chirp 
                                             << 3U)) 
                                      | ((((IData)(vlSelfRef.tx_enable) 
                                           & (IData)(vlSelfRef.radar_seq__DOT__nx_in_sweep)) 
                                          << 2U) | (IData)(vlSelfRef.mimo_mode))));
                }
            }
        }
    }
    return (__VstlExecute);
}

bool Vradar_seq___024root___trigger_anySet__ico(const VlUnpacked<QData/*63:0*/, 2> &in);

#ifdef VL_DEBUG
VL_ATTR_COLD void Vradar_seq___024root___dump_triggers__ico(const VlUnpacked<QData/*63:0*/, 2> &triggers, const std::string &tag) {
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vradar_seq___024root___dump_triggers__ico\n"); );
    // Body
    if ((1U & (~ (IData)(Vradar_seq___024root___trigger_anySet__ico(triggers))))) {
        VL_DBG_MSGS("         No '" + tag + "' region triggers active\n");
    }
    if ((1U & (IData)(triggers[0U]))) {
        VL_DBG_MSGS("         '" + tag + "' region trigger index 0 is active: @( clk)\n");
    }
    if ((1U & (IData)((triggers[0U] >> 1U)))) {
        VL_DBG_MSGS("         '" + tag + "' region trigger index 1 is active: @( rst)\n");
    }
    if ((1U & (IData)((triggers[0U] >> 2U)))) {
        VL_DBG_MSGS("         '" + tag + "' region trigger index 2 is active: @( enable)\n");
    }
    if ((1U & (IData)((triggers[0U] >> 3U)))) {
        VL_DBG_MSGS("         '" + tag + "' region trigger index 3 is active: @( mimo_mode)\n");
    }
    if ((1U & (IData)((triggers[0U] >> 4U)))) {
        VL_DBG_MSGS("         '" + tag + "' region trigger index 4 is active: @( tx_enable)\n");
    }
    if ((1U & (IData)((triggers[0U] >> 5U)))) {
        VL_DBG_MSGS("         '" + tag + "' region trigger index 5 is active: @( t_sweep)\n");
    }
    if ((1U & (IData)((triggers[0U] >> 6U)))) {
        VL_DBG_MSGS("         '" + tag + "' region trigger index 6 is active: @( t_pri)\n");
    }
    if ((1U & (IData)((triggers[0U] >> 7U)))) {
        VL_DBG_MSGS("         '" + tag + "' region trigger index 7 is active: @( n_chirp)\n");
    }
    if ((1U & (IData)(triggers[1U]))) {
        VL_DBG_MSGS("         '" + tag + "' region trigger index 64 is active: Internal 'ico' trigger - first iteration\n");
    }
}
#endif  // VL_DEBUG

bool Vradar_seq___024root___trigger_anySet__act(const VlUnpacked<QData/*63:0*/, 1> &in);

#ifdef VL_DEBUG
VL_ATTR_COLD void Vradar_seq___024root___dump_triggers__act(const VlUnpacked<QData/*63:0*/, 1> &triggers, const std::string &tag) {
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vradar_seq___024root___dump_triggers__act\n"); );
    // Body
    if ((1U & (~ (IData)(Vradar_seq___024root___trigger_anySet__act(triggers))))) {
        VL_DBG_MSGS("         No '" + tag + "' region triggers active\n");
    }
    if ((1U & (IData)(triggers[0U]))) {
        VL_DBG_MSGS("         '" + tag + "' region trigger index 0 is active: @(posedge clk)\n");
    }
}
#endif  // VL_DEBUG

VL_ATTR_COLD void Vradar_seq___024root___ctor_var_reset(Vradar_seq___024root* vlSelf) {
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vradar_seq___024root___ctor_var_reset\n"); );
    Vradar_seq__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    auto& vlSelfRef = std::ref(*vlSelf).get();
    // Body
    const uint64_t __VscopeHash = VL_MURMUR64_HASH(vlSelf->vlNamep);
    vlSelf->clk = VL_SCOPED_RAND_RESET_I(1, __VscopeHash, 16707436170211756652ull);
    vlSelf->rst = VL_SCOPED_RAND_RESET_I(1, __VscopeHash, 18209466448985614591ull);
    vlSelf->enable = VL_SCOPED_RAND_RESET_I(1, __VscopeHash, 11030669854614834172ull);
    vlSelf->mimo_mode = VL_SCOPED_RAND_RESET_I(2, __VscopeHash, 14259921642291688024ull);
    vlSelf->tx_enable = VL_SCOPED_RAND_RESET_I(1, __VscopeHash, 11129962599077753393ull);
    vlSelf->t_sweep = VL_SCOPED_RAND_RESET_I(16, __VscopeHash, 16985786760741074857ull);
    vlSelf->t_pri = VL_SCOPED_RAND_RESET_I(16, __VscopeHash, 10889341233623587760ull);
    vlSelf->n_chirp = VL_SCOPED_RAND_RESET_I(16, __VscopeHash, 12016554950783984648ull);
    vlSelf->nco_restart = VL_SCOPED_RAND_RESET_I(1, __VscopeHash, 7437618833982315369ull);
    vlSelf->nco_ena = VL_SCOPED_RAND_RESET_I(1, __VscopeHash, 13634767938691745904ull);
    vlSelf->tx0_ena = VL_SCOPED_RAND_RESET_I(1, __VscopeHash, 4800732196460811406ull);
    vlSelf->tx1_ena = VL_SCOPED_RAND_RESET_I(1, __VscopeHash, 11144332298263341125ull);
    vlSelf->tx_invert = VL_SCOPED_RAND_RESET_I(1, __VscopeHash, 15725067240952426008ull);
    vlSelf->adc_gate = VL_SCOPED_RAND_RESET_I(1, __VscopeHash, 11900786248379604381ull);
    vlSelf->sample_idx = VL_SCOPED_RAND_RESET_I(16, __VscopeHash, 17670071048999531770ull);
    vlSelf->chirp_idx = VL_SCOPED_RAND_RESET_I(16, __VscopeHash, 2354737269828633831ull);
    vlSelf->tx_sel = VL_SCOPED_RAND_RESET_I(1, __VscopeHash, 5389145495251426247ull);
    vlSelf->frame_start = VL_SCOPED_RAND_RESET_I(1, __VscopeHash, 10257969906964673985ull);
    vlSelf->frame_end = VL_SCOPED_RAND_RESET_I(1, __VscopeHash, 10763152428090563105ull);
    vlSelf->running = VL_SCOPED_RAND_RESET_I(1, __VscopeHash, 14883573590287949202ull);
    vlSelf->radar_seq__DOT__n_total_m1 = VL_SCOPED_RAND_RESET_I(17, __VscopeHash, 389254059957651226ull);
    vlSelf->radar_seq__DOT__t_pri_m1 = VL_SCOPED_RAND_RESET_I(16, __VscopeHash, 7432723284152977646ull);
    vlSelf->radar_seq__DOT__pri_cnt = VL_SCOPED_RAND_RESET_I(16, __VscopeHash, 2919304500365567427ull);
    vlSelf->radar_seq__DOT__chirp_cnt = VL_SCOPED_RAND_RESET_I(17, __VscopeHash, 14335230854205186206ull);
    vlSelf->radar_seq__DOT__nx_running = VL_SCOPED_RAND_RESET_I(1, __VscopeHash, 9226614756968659988ull);
    vlSelf->radar_seq__DOT__nx_pri = VL_SCOPED_RAND_RESET_I(16, __VscopeHash, 12295432884080186027ull);
    vlSelf->radar_seq__DOT__nx_chirp = VL_SCOPED_RAND_RESET_I(17, __VscopeHash, 1131473360795869377ull);
    vlSelf->radar_seq__DOT__nx_in_sweep = VL_SCOPED_RAND_RESET_I(1, __VscopeHash, 1747375563015955516ull);
    vlSelf->radar_seq__DOT__nx_first = VL_SCOPED_RAND_RESET_I(1, __VscopeHash, 1781830216411523221ull);
    vlSelf->__VdfgRegularize_hebeb780c_0_0 = 0;
    for (int __Vi0 = 0; __Vi0 < 1; ++__Vi0) {
        vlSelf->__VstlTriggered[__Vi0] = 0;
    }
    for (int __Vi0 = 0; __Vi0 < 2; ++__Vi0) {
        vlSelf->__VicoTriggered[__Vi0] = 0;
    }
    vlSelf->__Vtrigprevexpr___TOP__clk__0 = 0;
    vlSelf->__Vtrigprevexpr___TOP__rst__0 = 0;
    vlSelf->__Vtrigprevexpr___TOP__enable__0 = 0;
    vlSelf->__Vtrigprevexpr___TOP__mimo_mode__0 = 0;
    vlSelf->__Vtrigprevexpr___TOP__tx_enable__0 = 0;
    vlSelf->__Vtrigprevexpr___TOP__t_sweep__0 = 0;
    vlSelf->__Vtrigprevexpr___TOP__t_pri__0 = 0;
    vlSelf->__Vtrigprevexpr___TOP__n_chirp__0 = 0;
    vlSelf->__VicoDidInit = 0;
    for (int __Vi0 = 0; __Vi0 < 1; ++__Vi0) {
        vlSelf->__VactTriggered[__Vi0] = 0;
    }
    vlSelf->__Vtrigprevexpr___TOP__clk__1 = 0;
    for (int __Vi0 = 0; __Vi0 < 1; ++__Vi0) {
        vlSelf->__VnbaTriggered[__Vi0] = 0;
    }
}
