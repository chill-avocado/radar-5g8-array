// Verilated -*- C++ -*-
// DESCRIPTION: Verilator output: Design implementation internals
// See Vradar_seq.h for the primary calling header

#include "Vradar_seq__pch.h"

bool Vradar_seq___024root___trigger_anySet__ico(const VlUnpacked<QData/*63:0*/, 2> &in) {
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vradar_seq___024root___trigger_anySet__ico\n"); );
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
VL_ATTR_COLD void Vradar_seq___024root___dump_triggers__ico(const VlUnpacked<QData/*63:0*/, 2> &triggers, const std::string &tag);
#endif  // VL_DEBUG

bool Vradar_seq___024root___eval_phase__ico(Vradar_seq___024root* vlSelf) {
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vradar_seq___024root___eval_phase__ico\n"); );
    Vradar_seq__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    auto& vlSelfRef = std::ref(*vlSelf).get();
    // Locals
    CData/*0:0*/ __VicoExecute;
    // Body
    {
        // Inlined CFunc: _eval_triggers_vec__ico
        vlSelfRef.__VicoTriggered[0U] = (QData)((IData)(
                                                        (((((((IData)(vlSelfRef.n_chirp) 
                                                              != (IData)(vlSelfRef.__Vtrigprevexpr___TOP__n_chirp__0)) 
                                                             << 3U) 
                                                            | (((IData)(vlSelfRef.t_pri) 
                                                                != (IData)(vlSelfRef.__Vtrigprevexpr___TOP__t_pri__0)) 
                                                               << 2U)) 
                                                           | ((((IData)(vlSelfRef.t_sweep) 
                                                                != (IData)(vlSelfRef.__Vtrigprevexpr___TOP__t_sweep__0)) 
                                                               << 1U) 
                                                              | ((IData)(vlSelfRef.tx_enable) 
                                                                 != (IData)(vlSelfRef.__Vtrigprevexpr___TOP__tx_enable__0)))) 
                                                          << 4U) 
                                                         | (((((IData)(vlSelfRef.mimo_mode) 
                                                               != (IData)(vlSelfRef.__Vtrigprevexpr___TOP__mimo_mode__0)) 
                                                              << 3U) 
                                                             | (((IData)(vlSelfRef.enable) 
                                                                 != (IData)(vlSelfRef.__Vtrigprevexpr___TOP__enable__0)) 
                                                                << 2U)) 
                                                            | ((((IData)(vlSelfRef.rst) 
                                                                 != (IData)(vlSelfRef.__Vtrigprevexpr___TOP__rst__0)) 
                                                                << 1U) 
                                                               | ((IData)(vlSelfRef.clk) 
                                                                  != (IData)(vlSelfRef.__Vtrigprevexpr___TOP__clk__0)))))));
        vlSelfRef.__Vtrigprevexpr___TOP__clk__0 = vlSelfRef.clk;
        vlSelfRef.__Vtrigprevexpr___TOP__rst__0 = vlSelfRef.rst;
        vlSelfRef.__Vtrigprevexpr___TOP__enable__0 
            = vlSelfRef.enable;
        vlSelfRef.__Vtrigprevexpr___TOP__mimo_mode__0 
            = vlSelfRef.mimo_mode;
        vlSelfRef.__Vtrigprevexpr___TOP__tx_enable__0 
            = vlSelfRef.tx_enable;
        vlSelfRef.__Vtrigprevexpr___TOP__t_sweep__0 
            = vlSelfRef.t_sweep;
        vlSelfRef.__Vtrigprevexpr___TOP__t_pri__0 = vlSelfRef.t_pri;
        vlSelfRef.__Vtrigprevexpr___TOP__n_chirp__0 
            = vlSelfRef.n_chirp;
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
            vlSelfRef.__VicoTriggered[0U] = (0x0000000000000040ULL 
                                             | vlSelfRef.__VicoTriggered[0U]);
            vlSelfRef.__VicoTriggered[0U] = (0x0000000000000080ULL 
                                             | vlSelfRef.__VicoTriggered[0U]);
        }
    }
#ifdef VL_DEBUG
    if (VL_UNLIKELY(vlSymsp->_vm_contextp__->debug())) {
        Vradar_seq___024root___dump_triggers__ico(vlSelfRef.__VicoTriggered, "ico"s);
    }
#endif
    __VicoExecute = Vradar_seq___024root___trigger_anySet__ico(vlSelfRef.__VicoTriggered);
    if (__VicoExecute) {
        {
            // Inlined CFunc: _eval_ico
            if ((0x0000000000000088ULL & vlSelfRef.__VicoTriggered[0U])) {
                {
                    // Inlined CFunc: _ico_comb__TOP__0
                    IData/*16:0*/ __Vinline_0__eval_ico___Vinline_0__ico_comb__TOP__0_radar_seq__DOT__n_total;
                    __Vinline_0__eval_ico___Vinline_0__ico_comb__TOP__0_radar_seq__DOT__n_total = 0;
                    __Vinline_0__eval_ico___Vinline_0__ico_comb__TOP__0_radar_seq__DOT__n_total 
                        = ((0U == (IData)(vlSelfRef.mimo_mode))
                            ? ((IData)(vlSelfRef.n_chirp) 
                               << 1U) : (IData)(vlSelfRef.n_chirp));
                    vlSelfRef.radar_seq__DOT__n_total_m1 
                        = (0x0001ffffU & ((__Vinline_0__eval_ico___Vinline_0__ico_comb__TOP__0_radar_seq__DOT__n_total 
                                           - (IData)(1U)) 
                                          & (- (IData)(
                                                       (0U 
                                                        != __Vinline_0__eval_ico___Vinline_0__ico_comb__TOP__0_radar_seq__DOT__n_total)))));
                }
            }
            if ((0x0000000000000040ULL & vlSelfRef.__VicoTriggered[0U])) {
                {
                    // Inlined CFunc: _ico_sequent__TOP__0
                    vlSelfRef.radar_seq__DOT__t_pri_m1 
                        = (0x0000ffffU & (((IData)(vlSelfRef.t_pri) 
                                           - (IData)(1U)) 
                                          & (- (IData)(
                                                       (0U 
                                                        != (IData)(vlSelfRef.t_pri))))));
                    vlSelfRef.radar_seq__DOT__nx_pri 
                        = ((IData)(vlSelfRef.running)
                            ? (((IData)(vlSelfRef.radar_seq__DOT__pri_cnt) 
                                >= (IData)(vlSelfRef.radar_seq__DOT__t_pri_m1))
                                ? 0U : (0x0000ffffU 
                                        & ((IData)(1U) 
                                           + (IData)(vlSelfRef.radar_seq__DOT__pri_cnt))))
                            : 0U);
                }
            }
            if ((0x00000000000000c8ULL & vlSelfRef.__VicoTriggered[0U])) {
                {
                    // Inlined CFunc: _ico_comb__TOP__1
                    vlSelfRef.radar_seq__DOT__nx_chirp 
                        = vlSelfRef.radar_seq__DOT__chirp_cnt;
                    if (vlSelfRef.running) {
                        if (((IData)(vlSelfRef.radar_seq__DOT__pri_cnt) 
                             >= (IData)(vlSelfRef.radar_seq__DOT__t_pri_m1))) {
                            vlSelfRef.radar_seq__DOT__nx_chirp 
                                = ((vlSelfRef.radar_seq__DOT__chirp_cnt 
                                    >= vlSelfRef.radar_seq__DOT__n_total_m1)
                                    ? 0U : (0x0001ffffU 
                                            & ((IData)(1U) 
                                               + vlSelfRef.radar_seq__DOT__chirp_cnt)));
                        }
                    } else {
                        vlSelfRef.radar_seq__DOT__nx_chirp = 0U;
                    }
                }
            }
            if ((0x00000000000000ccULL & vlSelfRef.__VicoTriggered[0U])) {
                {
                    // Inlined CFunc: _ico_comb__TOP__2
                    if (vlSelfRef.running) {
                        vlSelfRef.radar_seq__DOT__nx_running = 1U;
                        if (((IData)(vlSelfRef.radar_seq__DOT__pri_cnt) 
                             >= (IData)(vlSelfRef.radar_seq__DOT__t_pri_m1))) {
                            if ((vlSelfRef.radar_seq__DOT__chirp_cnt 
                                 >= vlSelfRef.radar_seq__DOT__n_total_m1)) {
                                vlSelfRef.radar_seq__DOT__nx_running 
                                    = vlSelfRef.enable;
                            }
                        }
                    } else {
                        vlSelfRef.radar_seq__DOT__nx_running = 0U;
                        vlSelfRef.radar_seq__DOT__nx_running 
                            = vlSelfRef.enable;
                    }
                    vlSelfRef.radar_seq__DOT__nx_first 
                        = ((IData)(vlSelfRef.radar_seq__DOT__nx_running) 
                           & (0U == (IData)(vlSelfRef.radar_seq__DOT__nx_pri)));
                }
            }
            if ((0x00000000000000ecULL & vlSelfRef.__VicoTriggered[0U])) {
                {
                    // Inlined CFunc: _ico_comb__TOP__3
                    vlSelfRef.radar_seq__DOT__nx_in_sweep 
                        = ((IData)(vlSelfRef.radar_seq__DOT__nx_running) 
                           & ((IData)(vlSelfRef.radar_seq__DOT__nx_pri) 
                              < (IData)(vlSelfRef.t_sweep)));
                }
            }
            if ((0x00000000000000fcULL & vlSelfRef.__VicoTriggered[0U])) {
                {
                    // Inlined CFunc: _ico_comb__TOP__4
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
    return (__VicoExecute);
}

bool Vradar_seq___024root___trigger_anySet__act(const VlUnpacked<QData/*63:0*/, 1> &in) {
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vradar_seq___024root___trigger_anySet__act\n"); );
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

extern const VlUnpacked<CData/*0:0*/, 32> Vradar_seq__ConstPool__TABLE_h9c1f735d_0;
extern const VlUnpacked<CData/*0:0*/, 32> Vradar_seq__ConstPool__TABLE_hf574f07a_0;
extern const VlUnpacked<CData/*0:0*/, 32> Vradar_seq__ConstPool__TABLE_he3831943_0;
extern const VlUnpacked<CData/*0:0*/, 32> Vradar_seq__ConstPool__TABLE_hec43eb0c_0;

void Vradar_seq___024root___nba_sequent__TOP__0(Vradar_seq___024root* vlSelf) {
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vradar_seq___024root___nba_sequent__TOP__0\n"); );
    Vradar_seq__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    auto& vlSelfRef = std::ref(*vlSelf).get();
    // Body
    vlSelfRef.nco_restart = ((1U & (~ (IData)(vlSelfRef.rst))) 
                             && (IData)(vlSelfRef.radar_seq__DOT__nx_first));
    vlSelfRef.tx0_ena = ((1U & (~ (IData)(vlSelfRef.rst))) 
                         && Vradar_seq__ConstPool__TABLE_h9c1f735d_0
                         [vlSelfRef.__VdfgRegularize_hebeb780c_0_0]);
    vlSelfRef.tx1_ena = ((1U & (~ (IData)(vlSelfRef.rst))) 
                         && Vradar_seq__ConstPool__TABLE_hf574f07a_0
                         [vlSelfRef.__VdfgRegularize_hebeb780c_0_0]);
    vlSelfRef.tx_invert = ((1U & (~ (IData)(vlSelfRef.rst))) 
                           && Vradar_seq__ConstPool__TABLE_he3831943_0
                           [vlSelfRef.__VdfgRegularize_hebeb780c_0_0]);
    vlSelfRef.tx_sel = ((1U & (~ (IData)(vlSelfRef.rst))) 
                        && Vradar_seq__ConstPool__TABLE_hec43eb0c_0
                        [vlSelfRef.__VdfgRegularize_hebeb780c_0_0]);
    vlSelfRef.nco_ena = ((1U & (~ (IData)(vlSelfRef.rst))) 
                         && (IData)(vlSelfRef.radar_seq__DOT__nx_in_sweep));
    vlSelfRef.adc_gate = ((1U & (~ (IData)(vlSelfRef.rst))) 
                          && (IData)(vlSelfRef.radar_seq__DOT__nx_in_sweep));
    if (vlSelfRef.rst) {
        vlSelfRef.chirp_idx = 0U;
        vlSelfRef.sample_idx = 0U;
        vlSelfRef.radar_seq__DOT__chirp_cnt = 0U;
        vlSelfRef.radar_seq__DOT__pri_cnt = 0U;
    } else {
        vlSelfRef.chirp_idx = (0x0000ffffU & vlSelfRef.radar_seq__DOT__nx_chirp);
        vlSelfRef.sample_idx = ((IData)(vlSelfRef.radar_seq__DOT__nx_in_sweep)
                                 ? (IData)(vlSelfRef.radar_seq__DOT__nx_pri)
                                 : 0U);
        vlSelfRef.radar_seq__DOT__chirp_cnt = vlSelfRef.radar_seq__DOT__nx_chirp;
        vlSelfRef.radar_seq__DOT__pri_cnt = vlSelfRef.radar_seq__DOT__nx_pri;
    }
    vlSelfRef.frame_start = ((1U & (~ (IData)(vlSelfRef.rst))) 
                             && ((IData)(vlSelfRef.radar_seq__DOT__nx_first) 
                                 & (0U == vlSelfRef.radar_seq__DOT__nx_chirp)));
    vlSelfRef.running = ((1U & (~ (IData)(vlSelfRef.rst))) 
                         && (IData)(vlSelfRef.radar_seq__DOT__nx_running));
    vlSelfRef.frame_end = ((1U & (~ (IData)(vlSelfRef.rst))) 
                           && ((IData)(vlSelfRef.radar_seq__DOT__nx_running) 
                               & (((IData)(vlSelfRef.radar_seq__DOT__nx_pri) 
                                   >= (IData)(vlSelfRef.radar_seq__DOT__t_pri_m1)) 
                                  & (vlSelfRef.radar_seq__DOT__nx_chirp 
                                     >= vlSelfRef.radar_seq__DOT__n_total_m1))));
    vlSelfRef.radar_seq__DOT__nx_chirp = vlSelfRef.radar_seq__DOT__chirp_cnt;
    if (vlSelfRef.running) {
        vlSelfRef.radar_seq__DOT__nx_running = 1U;
        if (((IData)(vlSelfRef.radar_seq__DOT__pri_cnt) 
             >= (IData)(vlSelfRef.radar_seq__DOT__t_pri_m1))) {
            if ((vlSelfRef.radar_seq__DOT__chirp_cnt 
                 >= vlSelfRef.radar_seq__DOT__n_total_m1)) {
                vlSelfRef.radar_seq__DOT__nx_chirp = 0U;
                vlSelfRef.radar_seq__DOT__nx_running 
                    = vlSelfRef.enable;
            } else {
                vlSelfRef.radar_seq__DOT__nx_chirp 
                    = (0x0001ffffU & ((IData)(1U) + vlSelfRef.radar_seq__DOT__chirp_cnt));
            }
            vlSelfRef.radar_seq__DOT__nx_pri = 0U;
        } else {
            vlSelfRef.radar_seq__DOT__nx_pri = (0x0000ffffU 
                                                & ((IData)(1U) 
                                                   + (IData)(vlSelfRef.radar_seq__DOT__pri_cnt)));
        }
    } else {
        vlSelfRef.radar_seq__DOT__nx_running = 0U;
        vlSelfRef.radar_seq__DOT__nx_chirp = 0U;
        vlSelfRef.radar_seq__DOT__nx_pri = 0U;
        vlSelfRef.radar_seq__DOT__nx_running = vlSelfRef.enable;
    }
    vlSelfRef.radar_seq__DOT__nx_first = ((IData)(vlSelfRef.radar_seq__DOT__nx_running) 
                                          & (0U == (IData)(vlSelfRef.radar_seq__DOT__nx_pri)));
    vlSelfRef.radar_seq__DOT__nx_in_sweep = ((IData)(vlSelfRef.radar_seq__DOT__nx_running) 
                                             & ((IData)(vlSelfRef.radar_seq__DOT__nx_pri) 
                                                < (IData)(vlSelfRef.t_sweep)));
    vlSelfRef.__VdfgRegularize_hebeb780c_0_0 = (((IData)(vlSelfRef.radar_seq__DOT__nx_running) 
                                                 << 4U) 
                                                | ((8U 
                                                    & (vlSelfRef.radar_seq__DOT__nx_chirp 
                                                       << 3U)) 
                                                   | ((((IData)(vlSelfRef.tx_enable) 
                                                        & (IData)(vlSelfRef.radar_seq__DOT__nx_in_sweep)) 
                                                       << 2U) 
                                                      | (IData)(vlSelfRef.mimo_mode))));
}

void Vradar_seq___024root___trigger_orInto__act_vec_vec(VlUnpacked<QData/*63:0*/, 1> &out, const VlUnpacked<QData/*63:0*/, 1> &in) {
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vradar_seq___024root___trigger_orInto__act_vec_vec\n"); );
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
VL_ATTR_COLD void Vradar_seq___024root___dump_triggers__act(const VlUnpacked<QData/*63:0*/, 1> &triggers, const std::string &tag);
#endif  // VL_DEBUG

bool Vradar_seq___024root___eval_phase__act(Vradar_seq___024root* vlSelf) {
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vradar_seq___024root___eval_phase__act\n"); );
    Vradar_seq__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
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
        Vradar_seq___024root___dump_triggers__act(vlSelfRef.__VactTriggered, "act"s);
    }
#endif
    Vradar_seq___024root___trigger_orInto__act_vec_vec(vlSelfRef.__VnbaTriggered, vlSelfRef.__VactTriggered);
    return (0U);
}

void Vradar_seq___024root___trigger_clear__act(VlUnpacked<QData/*63:0*/, 1> &out) {
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vradar_seq___024root___trigger_clear__act\n"); );
    // Locals
    IData/*31:0*/ n;
    // Body
    n = 0U;
    do {
        out[n] = 0ULL;
        n = ((IData)(1U) + n);
    } while ((1U > n));
}

bool Vradar_seq___024root___eval_phase__nba(Vradar_seq___024root* vlSelf) {
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vradar_seq___024root___eval_phase__nba\n"); );
    Vradar_seq__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    auto& vlSelfRef = std::ref(*vlSelf).get();
    // Locals
    CData/*0:0*/ __VnbaExecute;
    // Body
    __VnbaExecute = Vradar_seq___024root___trigger_anySet__act(vlSelfRef.__VnbaTriggered);
    if (__VnbaExecute) {
        {
            // Inlined CFunc: _eval_nba
            if ((1ULL & vlSelfRef.__VnbaTriggered[0U])) {
                Vradar_seq___024root___nba_sequent__TOP__0(vlSelf);
            }
        }
        Vradar_seq___024root___trigger_clear__act(vlSelfRef.__VnbaTriggered);
    }
    return (__VnbaExecute);
}

void Vradar_seq___024root___eval(Vradar_seq___024root* vlSelf) {
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vradar_seq___024root___eval\n"); );
    Vradar_seq__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    auto& vlSelfRef = std::ref(*vlSelf).get();
    // Locals
    IData/*31:0*/ __VicoIterCount;
    IData/*31:0*/ __VnbaIterCount;
    // Body
    __VicoIterCount = 0U;
    do {
        if (VL_UNLIKELY(((0x00002710U < __VicoIterCount)))) {
#ifdef VL_DEBUG
            Vradar_seq___024root___dump_triggers__ico(vlSelfRef.__VicoTriggered, "ico"s);
#endif
            VL_FATAL_MT("/Users/lucasnaylor/radar-5g8-array/fpga/sim/../rtl/radar_seq.v", 60, "", "DIDNOTCONVERGE: Input combinational region did not converge after '--converge-limit' of 10000 tries");
        }
        __VicoIterCount = ((IData)(1U) + __VicoIterCount);
        vlSelfRef.__VicoPhaseResult = Vradar_seq___024root___eval_phase__ico(vlSelf);
    } while (vlSelfRef.__VicoPhaseResult);
    __VnbaIterCount = 0U;
    do {
        if (VL_UNLIKELY(((0x00002710U < __VnbaIterCount)))) {
#ifdef VL_DEBUG
            Vradar_seq___024root___dump_triggers__act(vlSelfRef.__VnbaTriggered, "nba"s);
#endif
            VL_FATAL_MT("/Users/lucasnaylor/radar-5g8-array/fpga/sim/../rtl/radar_seq.v", 60, "", "DIDNOTCONVERGE: NBA region did not converge after '--converge-limit' of 10000 tries");
        }
        __VnbaIterCount = ((IData)(1U) + __VnbaIterCount);
        vlSelfRef.__VactIterCount = 0U;
        do {
            if (VL_UNLIKELY(((0x00002710U < vlSelfRef.__VactIterCount)))) {
#ifdef VL_DEBUG
                Vradar_seq___024root___dump_triggers__act(vlSelfRef.__VactTriggered, "act"s);
#endif
                VL_FATAL_MT("/Users/lucasnaylor/radar-5g8-array/fpga/sim/../rtl/radar_seq.v", 60, "", "DIDNOTCONVERGE: Active region did not converge after '--converge-limit' of 10000 tries");
            }
            vlSelfRef.__VactIterCount = ((IData)(1U) 
                                         + vlSelfRef.__VactIterCount);
            vlSelfRef.__VactPhaseResult = Vradar_seq___024root___eval_phase__act(vlSelf);
        } while (vlSelfRef.__VactPhaseResult);
        vlSelfRef.__VnbaPhaseResult = Vradar_seq___024root___eval_phase__nba(vlSelf);
    } while (vlSelfRef.__VnbaPhaseResult);
}

#ifdef VL_DEBUG
void Vradar_seq___024root___eval_debug_assertions(Vradar_seq___024root* vlSelf) {
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vradar_seq___024root___eval_debug_assertions\n"); );
    Vradar_seq__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    auto& vlSelfRef = std::ref(*vlSelf).get();
    // Body
    if (VL_UNLIKELY(((vlSelfRef.clk & 0xfeU)))) {
        Verilated::overWidthError("clk");
    }
    if (VL_UNLIKELY(((vlSelfRef.rst & 0xfeU)))) {
        Verilated::overWidthError("rst");
    }
    if (VL_UNLIKELY(((vlSelfRef.enable & 0xfeU)))) {
        Verilated::overWidthError("enable");
    }
    if (VL_UNLIKELY(((vlSelfRef.mimo_mode & 0xfcU)))) {
        Verilated::overWidthError("mimo_mode");
    }
    if (VL_UNLIKELY(((vlSelfRef.tx_enable & 0xfeU)))) {
        Verilated::overWidthError("tx_enable");
    }
}
#endif  // VL_DEBUG
