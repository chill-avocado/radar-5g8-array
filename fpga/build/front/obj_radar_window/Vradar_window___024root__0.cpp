// Verilated -*- C++ -*-
// DESCRIPTION: Verilator output: Design implementation internals
// See Vradar_window.h for the primary calling header

#include "Vradar_window__pch.h"

bool Vradar_window___024root___trigger_anySet__act(const VlUnpacked<QData/*63:0*/, 1> &in) {
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vradar_window___024root___trigger_anySet__act\n"); );
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

void Vradar_window___024root___trigger_orInto__act_vec_vec(VlUnpacked<QData/*63:0*/, 1> &out, const VlUnpacked<QData/*63:0*/, 1> &in) {
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vradar_window___024root___trigger_orInto__act_vec_vec\n"); );
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
VL_ATTR_COLD void Vradar_window___024root___dump_triggers__act(const VlUnpacked<QData/*63:0*/, 1> &triggers, const std::string &tag);
#endif  // VL_DEBUG

bool Vradar_window___024root___eval_phase__act(Vradar_window___024root* vlSelf) {
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vradar_window___024root___eval_phase__act\n"); );
    Vradar_window__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
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
        Vradar_window___024root___dump_triggers__act(vlSelfRef.__VactTriggered, "act"s);
    }
#endif
    Vradar_window___024root___trigger_orInto__act_vec_vec(vlSelfRef.__VnbaTriggered, vlSelfRef.__VactTriggered);
    return (0U);
}

void Vradar_window___024root___trigger_clear__act(VlUnpacked<QData/*63:0*/, 1> &out) {
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vradar_window___024root___trigger_clear__act\n"); );
    // Locals
    IData/*31:0*/ n;
    // Body
    n = 0U;
    do {
        out[n] = 0ULL;
        n = ((IData)(1U) + n);
    } while ((1U > n));
}

bool Vradar_window___024root___eval_phase__nba(Vradar_window___024root* vlSelf) {
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vradar_window___024root___eval_phase__nba\n"); );
    Vradar_window__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    auto& vlSelfRef = std::ref(*vlSelf).get();
    // Locals
    CData/*0:0*/ __VnbaExecute;
    // Body
    __VnbaExecute = Vradar_window___024root___trigger_anySet__act(vlSelfRef.__VnbaTriggered);
    if (__VnbaExecute) {
        {
            // Inlined CFunc: _eval_nba
            if ((1ULL & vlSelfRef.__VnbaTriggered[0U])) {
                {
                    // Inlined CFunc: _nba_sequent__TOP__0
                    IData/*31:0*/ __Vinline_0__eval_nba___Vinline_0__nba_sequent__TOP__0___Vfunc_radar_window__DOT__round_sat__0__v;
                    __Vinline_0__eval_nba___Vinline_0__nba_sequent__TOP__0___Vfunc_radar_window__DOT__round_sat__0__v = 0;
                    IData/*31:0*/ __Vinline_0__eval_nba___Vinline_0__nba_sequent__TOP__0___Vfunc_radar_window__DOT__round_sat__1__v;
                    __Vinline_0__eval_nba___Vinline_0__nba_sequent__TOP__0___Vfunc_radar_window__DOT__round_sat__1__v = 0;
                    SData/*15:0*/ __Vinline_0__eval_nba___Vinline_0__nba_sequent__TOP__0___VdlyVal__radar_window__DOT__win__v0;
                    __Vinline_0__eval_nba___Vinline_0__nba_sequent__TOP__0___VdlyVal__radar_window__DOT__win__v0 = 0;
                    SData/*9:0*/ __Vinline_0__eval_nba___Vinline_0__nba_sequent__TOP__0___VdlyDim0__radar_window__DOT__win__v0;
                    __Vinline_0__eval_nba___Vinline_0__nba_sequent__TOP__0___VdlyDim0__radar_window__DOT__win__v0 = 0;
                    CData/*0:0*/ __Vinline_0__eval_nba___Vinline_0__nba_sequent__TOP__0___VdlySet__radar_window__DOT__win__v0;
                    __Vinline_0__eval_nba___Vinline_0__nba_sequent__TOP__0___VdlySet__radar_window__DOT__win__v0 = 0;
                    __Vinline_0__eval_nba___Vinline_0__nba_sequent__TOP__0___VdlySet__radar_window__DOT__win__v0 = 0U;
                    if (((IData)(vlSelfRef.coef_we) 
                         & (0x0400U > (IData)(vlSelfRef.coef_addr)))) {
                        __Vinline_0__eval_nba___Vinline_0__nba_sequent__TOP__0___VdlyVal__radar_window__DOT__win__v0 
                            = vlSelfRef.coef_data;
                        __Vinline_0__eval_nba___Vinline_0__nba_sequent__TOP__0___VdlyDim0__radar_window__DOT__win__v0 
                            = (0x000003ffU & (IData)(vlSelfRef.coef_addr));
                        __Vinline_0__eval_nba___Vinline_0__nba_sequent__TOP__0___VdlySet__radar_window__DOT__win__v0 = 1U;
                    }
                    vlSelfRef.out_valid = ((~ (IData)(vlSelfRef.rst)) 
                                           & (IData)(vlSelfRef.radar_window__DOT__v_s2));
                    if (vlSelfRef.radar_window__DOT__oob_s2) {
                        vlSelfRef.radar_window__DOT____VlemCond_1 = 0U;
                        vlSelfRef.radar_window__DOT____VlemCond_3 = 0U;
                    } else {
                        __Vinline_0__eval_nba___Vinline_0__nba_sequent__TOP__0___Vfunc_radar_window__DOT__round_sat__0__v 
                            = vlSelfRef.radar_window__DOT__p_i_s2;
                        vlSelfRef.radar_window__DOT__round_sat__Vstatic__t 
                            = VL_SHIFTRS_III(32,32,32, 
                                             ((IData)(0x00004000U) 
                                              + __Vinline_0__eval_nba___Vinline_0__nba_sequent__TOP__0___Vfunc_radar_window__DOT__round_sat__0__v), 0x0000000fU);
                        vlSelfRef.radar_window__DOT____VlemCall_0__round_sat 
                            = (VL_LTS_III(32, 0x00007fffU, vlSelfRef.radar_window__DOT__round_sat__Vstatic__t)
                                ? 0x00007fffU : (VL_GTS_III(32, 0xffff8000U, vlSelfRef.radar_window__DOT__round_sat__Vstatic__t)
                                                  ? 0x00008000U
                                                  : 
                                                 (0x0000ffffU 
                                                  & vlSelfRef.radar_window__DOT__round_sat__Vstatic__t)));
                        vlSelfRef.radar_window__DOT____VlemCond_1 
                            = vlSelfRef.radar_window__DOT____VlemCall_0__round_sat;
                        __Vinline_0__eval_nba___Vinline_0__nba_sequent__TOP__0___Vfunc_radar_window__DOT__round_sat__1__v 
                            = vlSelfRef.radar_window__DOT__p_q_s2;
                        vlSelfRef.radar_window__DOT__round_sat__Vstatic__t 
                            = VL_SHIFTRS_III(32,32,32, 
                                             ((IData)(0x00004000U) 
                                              + __Vinline_0__eval_nba___Vinline_0__nba_sequent__TOP__0___Vfunc_radar_window__DOT__round_sat__1__v), 0x0000000fU);
                        vlSelfRef.radar_window__DOT____VlemCall_2__round_sat 
                            = (VL_LTS_III(32, 0x00007fffU, vlSelfRef.radar_window__DOT__round_sat__Vstatic__t)
                                ? 0x00007fffU : (VL_GTS_III(32, 0xffff8000U, vlSelfRef.radar_window__DOT__round_sat__Vstatic__t)
                                                  ? 0x00008000U
                                                  : 
                                                 (0x0000ffffU 
                                                  & vlSelfRef.radar_window__DOT__round_sat__Vstatic__t)));
                        vlSelfRef.radar_window__DOT____VlemCond_3 
                            = vlSelfRef.radar_window__DOT____VlemCall_2__round_sat;
                    }
                    vlSelfRef.out_i = vlSelfRef.radar_window__DOT____VlemCond_1;
                    vlSelfRef.out_q = vlSelfRef.radar_window__DOT____VlemCond_3;
                    vlSelfRef.radar_window__DOT__v_s2 
                        = ((~ (IData)(vlSelfRef.rst)) 
                           & (IData)(vlSelfRef.radar_window__DOT__v_s1));
                    vlSelfRef.radar_window__DOT__oob_s2 
                        = vlSelfRef.radar_window__DOT__oob_s1;
                    vlSelfRef.radar_window__DOT__p_i_s2 
                        = VL_MULS_III(32, VL_EXTENDS_II(32,16, (IData)(vlSelfRef.radar_window__DOT__d_i_s1)), 
                                      VL_EXTENDS_II(32,16, (IData)(vlSelfRef.radar_window__DOT__coef_s1)));
                    vlSelfRef.radar_window__DOT__p_q_s2 
                        = VL_MULS_III(32, VL_EXTENDS_II(32,16, (IData)(vlSelfRef.radar_window__DOT__d_q_s1)), 
                                      VL_EXTENDS_II(32,16, (IData)(vlSelfRef.radar_window__DOT__coef_s1)));
                    vlSelfRef.radar_window__DOT__v_s1 
                        = ((~ (IData)(vlSelfRef.rst)) 
                           & (IData)(vlSelfRef.in_valid));
                    vlSelfRef.radar_window__DOT__oob_s1 
                        = (0x0400U <= (IData)(vlSelfRef.sample_idx));
                    vlSelfRef.radar_window__DOT__d_i_s1 
                        = vlSelfRef.in_i;
                    vlSelfRef.radar_window__DOT__d_q_s1 
                        = vlSelfRef.in_q;
                    vlSelfRef.radar_window__DOT__coef_s1 
                        = vlSelfRef.radar_window__DOT__win
                        [(0x000003ffU & (IData)(vlSelfRef.sample_idx))];
                    if (__Vinline_0__eval_nba___Vinline_0__nba_sequent__TOP__0___VdlySet__radar_window__DOT__win__v0) {
                        vlSelfRef.radar_window__DOT__win[__Vinline_0__eval_nba___Vinline_0__nba_sequent__TOP__0___VdlyDim0__radar_window__DOT__win__v0] 
                            = __Vinline_0__eval_nba___Vinline_0__nba_sequent__TOP__0___VdlyVal__radar_window__DOT__win__v0;
                    }
                }
            }
        }
        Vradar_window___024root___trigger_clear__act(vlSelfRef.__VnbaTriggered);
    }
    return (__VnbaExecute);
}

void Vradar_window___024root___eval(Vradar_window___024root* vlSelf) {
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vradar_window___024root___eval\n"); );
    Vradar_window__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    auto& vlSelfRef = std::ref(*vlSelf).get();
    // Locals
    IData/*31:0*/ __VnbaIterCount;
    // Body
    __VnbaIterCount = 0U;
    do {
        if (VL_UNLIKELY(((0x00002710U < __VnbaIterCount)))) {
#ifdef VL_DEBUG
            Vradar_window___024root___dump_triggers__act(vlSelfRef.__VnbaTriggered, "nba"s);
#endif
            VL_FATAL_MT("/Users/lucasnaylor/radar-5g8-array/fpga/sim/../rtl/radar_window.v", 46, "", "DIDNOTCONVERGE: NBA region did not converge after '--converge-limit' of 10000 tries");
        }
        __VnbaIterCount = ((IData)(1U) + __VnbaIterCount);
        vlSelfRef.__VactIterCount = 0U;
        do {
            if (VL_UNLIKELY(((0x00002710U < vlSelfRef.__VactIterCount)))) {
#ifdef VL_DEBUG
                Vradar_window___024root___dump_triggers__act(vlSelfRef.__VactTriggered, "act"s);
#endif
                VL_FATAL_MT("/Users/lucasnaylor/radar-5g8-array/fpga/sim/../rtl/radar_window.v", 46, "", "DIDNOTCONVERGE: Active region did not converge after '--converge-limit' of 10000 tries");
            }
            vlSelfRef.__VactIterCount = ((IData)(1U) 
                                         + vlSelfRef.__VactIterCount);
            vlSelfRef.__VactPhaseResult = Vradar_window___024root___eval_phase__act(vlSelf);
        } while (vlSelfRef.__VactPhaseResult);
        vlSelfRef.__VnbaPhaseResult = Vradar_window___024root___eval_phase__nba(vlSelf);
    } while (vlSelfRef.__VnbaPhaseResult);
}

#ifdef VL_DEBUG
void Vradar_window___024root___eval_debug_assertions(Vradar_window___024root* vlSelf) {
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vradar_window___024root___eval_debug_assertions\n"); );
    Vradar_window__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    auto& vlSelfRef = std::ref(*vlSelf).get();
    // Body
    if (VL_UNLIKELY(((vlSelfRef.clk & 0xfeU)))) {
        Verilated::overWidthError("clk");
    }
    if (VL_UNLIKELY(((vlSelfRef.rst & 0xfeU)))) {
        Verilated::overWidthError("rst");
    }
    if (VL_UNLIKELY(((vlSelfRef.in_valid & 0xfeU)))) {
        Verilated::overWidthError("in_valid");
    }
    if (VL_UNLIKELY(((vlSelfRef.coef_we & 0xfeU)))) {
        Verilated::overWidthError("coef_we");
    }
}
#endif  // VL_DEBUG
