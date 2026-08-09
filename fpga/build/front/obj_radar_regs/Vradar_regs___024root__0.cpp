// Verilated -*- C++ -*-
// DESCRIPTION: Verilator output: Design implementation internals
// See Vradar_regs.h for the primary calling header

#include "Vradar_regs__pch.h"

bool Vradar_regs___024root___trigger_anySet__act(const VlUnpacked<QData/*63:0*/, 1> &in) {
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vradar_regs___024root___trigger_anySet__act\n"); );
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

void Vradar_regs___024root___nba_sequent__TOP__0(Vradar_regs___024root* vlSelf) {
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vradar_regs___024root___nba_sequent__TOP__0\n"); );
    Vradar_regs__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    auto& vlSelfRef = std::ref(*vlSelf).get();
    // Body
    if (vlSelfRef.rst) {
        vlSelfRef.win_we = 0U;
        vlSelfRef.version_stb = 0U;
        vlSelfRef.ctrl_soft_reset = 0U;
        vlSelfRef.ctrl_enable = 0U;
        vlSelfRef.ctrl_mimo_mode = 0U;
        vlSelfRef.ctrl_tx_enable = 0U;
        vlSelfRef.ctrl_map_enable = 1U;
        vlSelfRef.ctrl_hits_enable = 1U;
        vlSelfRef.ctrl_loopback = 0U;
        vlSelfRef.ctrl_frame_limit = 0U;
        vlSelfRef.freq_slope = 0x00115c72U;
        vlSelfRef.freq_start = 0x97d55555U;
        vlSelfRef.t_sweep = 0x0c00U;
        vlSelfRef.t_pri = 0x0f00U;
        vlSelfRef.n_chirp = 0x0080U;
        vlSelfRef.tx_gain = 0x7fffU;
        vlSelfRef.dechirp_sh = 0U;
        vlSelfRef.fft_scale_r = 0x00055555U;
        vlSelfRef.fft_scale_d = 0x5555U;
        vlSelfRef.win_addr = 0U;
        vlSelfRef.win_data = 0U;
        vlSelfRef.cfar_guard_range = 2U;
        vlSelfRef.cfar_guard_dopp = 2U;
        vlSelfRef.cfar_train_range = 8U;
        vlSelfRef.cfar_train_dopp = 8U;
        vlSelfRef.cfar_kind = 0U;
        vlSelfRef.cfar_alpha = 0x000a0000U;
        vlSelfRef.range_zero = 0U;
        vlSelfRef.map_decim_r = 1U;
        vlSelfRef.map_decim_d = 1U;
        vlSelfRef.max_hits = 0x0040U;
        vlSelfRef.zero_dopp = 2U;
        vlSelfRef.geom_n_range_log2 = 8U;
        vlSelfRef.geom_n_chirp_log2 = 8U;
        vlSelfRef.test_tone = 0x042aaaabU;
    } else {
        vlSelfRef.win_we = 0U;
        vlSelfRef.version_stb = 0U;
        vlSelfRef.ctrl_soft_reset = 0U;
        if (vlSelfRef.set_stb) {
            if ((1U & (~ ((IData)(vlSelfRef.set_addr) 
                          >> 7U)))) {
                if ((1U & (~ ((IData)(vlSelfRef.set_addr) 
                              >> 6U)))) {
                    if ((1U & (~ ((IData)(vlSelfRef.set_addr) 
                                  >> 5U)))) {
                        if ((1U & (~ ((IData)(vlSelfRef.set_addr) 
                                      >> 4U)))) {
                            if ((8U & (IData)(vlSelfRef.set_addr))) {
                                if ((1U & (~ ((IData)(vlSelfRef.set_addr) 
                                              >> 2U)))) {
                                    if ((2U & (IData)(vlSelfRef.set_addr))) {
                                        if ((1U & (IData)(vlSelfRef.set_addr))) {
                                            vlSelfRef.win_we = 1U;
                                            vlSelfRef.win_data 
                                                = vlSelfRef.set_data;
                                        }
                                        if ((1U & (~ (IData)(vlSelfRef.set_addr)))) {
                                            vlSelfRef.win_addr 
                                                = (0x0000ffffU 
                                                   & vlSelfRef.set_data);
                                        }
                                    }
                                    if ((1U & (~ ((IData)(vlSelfRef.set_addr) 
                                                  >> 1U)))) {
                                        if ((1U & (~ (IData)(vlSelfRef.set_addr)))) {
                                            vlSelfRef.fft_scale_r 
                                                = (0x000fffffU 
                                                   & vlSelfRef.set_data);
                                        }
                                        if ((1U & (IData)(vlSelfRef.set_addr))) {
                                            vlSelfRef.fft_scale_d 
                                                = (0x0000ffffU 
                                                   & vlSelfRef.set_data);
                                        }
                                    }
                                }
                                if ((4U & (IData)(vlSelfRef.set_addr))) {
                                    if ((1U & (~ ((IData)(vlSelfRef.set_addr) 
                                                  >> 1U)))) {
                                        if ((1U & (~ (IData)(vlSelfRef.set_addr)))) {
                                            vlSelfRef.cfar_guard_range 
                                                = (0x0000000fU 
                                                   & vlSelfRef.set_data);
                                            vlSelfRef.cfar_guard_dopp 
                                                = (0x0000000fU 
                                                   & (vlSelfRef.set_data 
                                                      >> 4U));
                                            vlSelfRef.cfar_train_range 
                                                = (0x0000000fU 
                                                   & (vlSelfRef.set_data 
                                                      >> 8U));
                                            vlSelfRef.cfar_train_dopp 
                                                = (0x0000000fU 
                                                   & (vlSelfRef.set_data 
                                                      >> 0x0cU));
                                            vlSelfRef.cfar_kind 
                                                = (3U 
                                                   & (vlSelfRef.set_data 
                                                      >> 0x10U));
                                        }
                                        if ((1U & (IData)(vlSelfRef.set_addr))) {
                                            vlSelfRef.cfar_alpha 
                                                = vlSelfRef.set_data;
                                        }
                                    }
                                    if ((2U & (IData)(vlSelfRef.set_addr))) {
                                        if ((1U & (~ (IData)(vlSelfRef.set_addr)))) {
                                            vlSelfRef.range_zero 
                                                = (0x0000ffffU 
                                                   & vlSelfRef.set_data);
                                        }
                                        if ((1U & (IData)(vlSelfRef.set_addr))) {
                                            vlSelfRef.map_decim_r 
                                                = (0x000000ffU 
                                                   & vlSelfRef.set_data);
                                            vlSelfRef.map_decim_d 
                                                = (0x000000ffU 
                                                   & (vlSelfRef.set_data 
                                                      >> 8U));
                                        }
                                    }
                                }
                            }
                            if ((1U & (~ ((IData)(vlSelfRef.set_addr) 
                                          >> 3U)))) {
                                if ((1U & (~ ((IData)(vlSelfRef.set_addr) 
                                              >> 2U)))) {
                                    if ((1U & (~ ((IData)(vlSelfRef.set_addr) 
                                                  >> 1U)))) {
                                        if ((1U & (~ (IData)(vlSelfRef.set_addr)))) {
                                            vlSelfRef.ctrl_soft_reset 
                                                = (1U 
                                                   & (vlSelfRef.set_data 
                                                      >> 1U));
                                            vlSelfRef.ctrl_enable 
                                                = (1U 
                                                   & vlSelfRef.set_data);
                                            vlSelfRef.ctrl_mimo_mode 
                                                = (3U 
                                                   & (vlSelfRef.set_data 
                                                      >> 2U));
                                            vlSelfRef.ctrl_tx_enable 
                                                = (1U 
                                                   & (vlSelfRef.set_data 
                                                      >> 4U));
                                            vlSelfRef.ctrl_map_enable 
                                                = (1U 
                                                   & (vlSelfRef.set_data 
                                                      >> 5U));
                                            vlSelfRef.ctrl_hits_enable 
                                                = (1U 
                                                   & (vlSelfRef.set_data 
                                                      >> 6U));
                                            vlSelfRef.ctrl_loopback 
                                                = (1U 
                                                   & (vlSelfRef.set_data 
                                                      >> 7U));
                                            vlSelfRef.ctrl_frame_limit 
                                                = (vlSelfRef.set_data 
                                                   >> 0x10U);
                                        }
                                        if ((1U & (IData)(vlSelfRef.set_addr))) {
                                            vlSelfRef.freq_start 
                                                = vlSelfRef.set_data;
                                        }
                                    }
                                    if ((2U & (IData)(vlSelfRef.set_addr))) {
                                        if ((1U & (~ (IData)(vlSelfRef.set_addr)))) {
                                            vlSelfRef.freq_slope 
                                                = vlSelfRef.set_data;
                                        }
                                        if ((1U & (IData)(vlSelfRef.set_addr))) {
                                            vlSelfRef.t_sweep 
                                                = (0x0000ffffU 
                                                   & vlSelfRef.set_data);
                                        }
                                    }
                                }
                                if ((4U & (IData)(vlSelfRef.set_addr))) {
                                    if ((1U & (~ ((IData)(vlSelfRef.set_addr) 
                                                  >> 1U)))) {
                                        if ((1U & (~ (IData)(vlSelfRef.set_addr)))) {
                                            vlSelfRef.t_pri 
                                                = (0x0000ffffU 
                                                   & vlSelfRef.set_data);
                                        }
                                        if ((1U & (IData)(vlSelfRef.set_addr))) {
                                            vlSelfRef.n_chirp 
                                                = (0x0000ffffU 
                                                   & vlSelfRef.set_data);
                                        }
                                    }
                                    if ((2U & (IData)(vlSelfRef.set_addr))) {
                                        if ((1U & (~ (IData)(vlSelfRef.set_addr)))) {
                                            vlSelfRef.tx_gain 
                                                = (0x0000ffffU 
                                                   & vlSelfRef.set_data);
                                        }
                                        if ((1U & (IData)(vlSelfRef.set_addr))) {
                                            vlSelfRef.dechirp_sh 
                                                = (0x0000000fU 
                                                   & vlSelfRef.set_data);
                                        }
                                    }
                                }
                            }
                        }
                        if ((0x00000010U & (IData)(vlSelfRef.set_addr))) {
                            if ((1U & (~ ((IData)(vlSelfRef.set_addr) 
                                          >> 3U)))) {
                                if ((1U & (~ ((IData)(vlSelfRef.set_addr) 
                                              >> 2U)))) {
                                    if ((2U & (IData)(vlSelfRef.set_addr))) {
                                        if ((1U & (IData)(vlSelfRef.set_addr))) {
                                            vlSelfRef.version_stb = 1U;
                                        }
                                        if ((1U & (~ (IData)(vlSelfRef.set_addr)))) {
                                            vlSelfRef.geom_n_range_log2 
                                                = (0x0000000fU 
                                                   & vlSelfRef.set_data);
                                            vlSelfRef.geom_n_chirp_log2 
                                                = (0x0000000fU 
                                                   & (vlSelfRef.set_data 
                                                      >> 4U));
                                        }
                                    }
                                    if ((1U & (~ ((IData)(vlSelfRef.set_addr) 
                                                  >> 1U)))) {
                                        if ((1U & (~ (IData)(vlSelfRef.set_addr)))) {
                                            vlSelfRef.max_hits 
                                                = (0x0000ffffU 
                                                   & vlSelfRef.set_data);
                                        }
                                        if ((1U & (IData)(vlSelfRef.set_addr))) {
                                            vlSelfRef.zero_dopp 
                                                = (0x000000ffU 
                                                   & vlSelfRef.set_data);
                                        }
                                    }
                                }
                                if ((4U & (IData)(vlSelfRef.set_addr))) {
                                    if ((1U & (~ ((IData)(vlSelfRef.set_addr) 
                                                  >> 1U)))) {
                                        if ((1U & (~ (IData)(vlSelfRef.set_addr)))) {
                                            vlSelfRef.test_tone 
                                                = vlSelfRef.set_data;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

void Vradar_regs___024root___trigger_orInto__act_vec_vec(VlUnpacked<QData/*63:0*/, 1> &out, const VlUnpacked<QData/*63:0*/, 1> &in) {
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vradar_regs___024root___trigger_orInto__act_vec_vec\n"); );
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
VL_ATTR_COLD void Vradar_regs___024root___dump_triggers__act(const VlUnpacked<QData/*63:0*/, 1> &triggers, const std::string &tag);
#endif  // VL_DEBUG

bool Vradar_regs___024root___eval_phase__act(Vradar_regs___024root* vlSelf) {
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vradar_regs___024root___eval_phase__act\n"); );
    Vradar_regs__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    auto& vlSelfRef = std::ref(*vlSelf).get();
    // Body
    {
        // Inlined CFunc: _eval_triggers_vec__act
        vlSelfRef.__VactTriggered[0U] = (QData)((IData)(
                                                        ((IData)(vlSelfRef.clk) 
                                                         & (~ (IData)(vlSelfRef.__Vtrigprevexpr___TOP__clk__0)))));
        vlSelfRef.__Vtrigprevexpr___TOP__clk__0 = vlSelfRef.clk;
    }
#ifdef VL_DEBUG
    if (VL_UNLIKELY(vlSymsp->_vm_contextp__->debug())) {
        Vradar_regs___024root___dump_triggers__act(vlSelfRef.__VactTriggered, "act"s);
    }
#endif
    Vradar_regs___024root___trigger_orInto__act_vec_vec(vlSelfRef.__VnbaTriggered, vlSelfRef.__VactTriggered);
    return (0U);
}

void Vradar_regs___024root___trigger_clear__act(VlUnpacked<QData/*63:0*/, 1> &out) {
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vradar_regs___024root___trigger_clear__act\n"); );
    // Locals
    IData/*31:0*/ n;
    // Body
    n = 0U;
    do {
        out[n] = 0ULL;
        n = ((IData)(1U) + n);
    } while ((1U > n));
}

bool Vradar_regs___024root___eval_phase__nba(Vradar_regs___024root* vlSelf) {
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vradar_regs___024root___eval_phase__nba\n"); );
    Vradar_regs__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    auto& vlSelfRef = std::ref(*vlSelf).get();
    // Locals
    CData/*0:0*/ __VnbaExecute;
    // Body
    __VnbaExecute = Vradar_regs___024root___trigger_anySet__act(vlSelfRef.__VnbaTriggered);
    if (__VnbaExecute) {
        {
            // Inlined CFunc: _eval_nba
            if ((1ULL & vlSelfRef.__VnbaTriggered[0U])) {
                Vradar_regs___024root___nba_sequent__TOP__0(vlSelf);
            }
        }
        Vradar_regs___024root___trigger_clear__act(vlSelfRef.__VnbaTriggered);
    }
    return (__VnbaExecute);
}

void Vradar_regs___024root___eval(Vradar_regs___024root* vlSelf) {
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vradar_regs___024root___eval\n"); );
    Vradar_regs__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    auto& vlSelfRef = std::ref(*vlSelf).get();
    // Locals
    IData/*31:0*/ __VnbaIterCount;
    // Body
    __VnbaIterCount = 0U;
    do {
        if (VL_UNLIKELY(((0x00002710U < __VnbaIterCount)))) {
#ifdef VL_DEBUG
            Vradar_regs___024root___dump_triggers__act(vlSelfRef.__VnbaTriggered, "nba"s);
#endif
            VL_FATAL_MT("/Users/lucasnaylor/radar-5g8-array/fpga/sim/../rtl/radar_regs.v", 49, "", "DIDNOTCONVERGE: NBA region did not converge after '--converge-limit' of 10000 tries");
        }
        __VnbaIterCount = ((IData)(1U) + __VnbaIterCount);
        vlSelfRef.__VactIterCount = 0U;
        do {
            if (VL_UNLIKELY(((0x00002710U < vlSelfRef.__VactIterCount)))) {
#ifdef VL_DEBUG
                Vradar_regs___024root___dump_triggers__act(vlSelfRef.__VactTriggered, "act"s);
#endif
                VL_FATAL_MT("/Users/lucasnaylor/radar-5g8-array/fpga/sim/../rtl/radar_regs.v", 49, "", "DIDNOTCONVERGE: Active region did not converge after '--converge-limit' of 10000 tries");
            }
            vlSelfRef.__VactIterCount = ((IData)(1U) 
                                         + vlSelfRef.__VactIterCount);
            vlSelfRef.__VactPhaseResult = Vradar_regs___024root___eval_phase__act(vlSelf);
        } while (vlSelfRef.__VactPhaseResult);
        vlSelfRef.__VnbaPhaseResult = Vradar_regs___024root___eval_phase__nba(vlSelf);
    } while (vlSelfRef.__VnbaPhaseResult);
}

#ifdef VL_DEBUG
void Vradar_regs___024root___eval_debug_assertions(Vradar_regs___024root* vlSelf) {
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vradar_regs___024root___eval_debug_assertions\n"); );
    Vradar_regs__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    auto& vlSelfRef = std::ref(*vlSelf).get();
    // Body
    if (VL_UNLIKELY(((vlSelfRef.clk & 0xfeU)))) {
        Verilated::overWidthError("clk");
    }
    if (VL_UNLIKELY(((vlSelfRef.rst & 0xfeU)))) {
        Verilated::overWidthError("rst");
    }
    if (VL_UNLIKELY(((vlSelfRef.set_stb & 0xfeU)))) {
        Verilated::overWidthError("set_stb");
    }
}
#endif  // VL_DEBUG
