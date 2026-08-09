// Verilated -*- C++ -*-
// DESCRIPTION: Verilator output: Design implementation internals
// See Vradar_nco.h for the primary calling header

#include "Vradar_nco__pch.h"

bool Vradar_nco___024root___trigger_anySet__act(const VlUnpacked<QData/*63:0*/, 1> &in) {
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vradar_nco___024root___trigger_anySet__act\n"); );
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

void Vradar_nco___024root___trigger_orInto__act_vec_vec(VlUnpacked<QData/*63:0*/, 1> &out, const VlUnpacked<QData/*63:0*/, 1> &in) {
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vradar_nco___024root___trigger_orInto__act_vec_vec\n"); );
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
VL_ATTR_COLD void Vradar_nco___024root___dump_triggers__act(const VlUnpacked<QData/*63:0*/, 1> &triggers, const std::string &tag);
#endif  // VL_DEBUG

bool Vradar_nco___024root___eval_phase__act(Vradar_nco___024root* vlSelf) {
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vradar_nco___024root___eval_phase__act\n"); );
    Vradar_nco__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
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
        Vradar_nco___024root___dump_triggers__act(vlSelfRef.__VactTriggered, "act"s);
    }
#endif
    Vradar_nco___024root___trigger_orInto__act_vec_vec(vlSelfRef.__VnbaTriggered, vlSelfRef.__VactTriggered);
    return (0U);
}

void Vradar_nco___024root___trigger_clear__act(VlUnpacked<QData/*63:0*/, 1> &out) {
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vradar_nco___024root___trigger_clear__act\n"); );
    // Locals
    IData/*31:0*/ n;
    // Body
    n = 0U;
    do {
        out[n] = 0ULL;
        n = ((IData)(1U) + n);
    } while ((1U > n));
}

bool Vradar_nco___024root___eval_phase__nba(Vradar_nco___024root* vlSelf) {
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vradar_nco___024root___eval_phase__nba\n"); );
    Vradar_nco__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    auto& vlSelfRef = std::ref(*vlSelf).get();
    // Locals
    CData/*0:0*/ __VnbaExecute;
    // Body
    __VnbaExecute = Vradar_nco___024root___trigger_anySet__act(vlSelfRef.__VnbaTriggered);
    if (__VnbaExecute) {
        {
            // Inlined CFunc: _eval_nba
            if ((1ULL & vlSelfRef.__VnbaTriggered[0U])) {
                {
                    // Inlined CFunc: _nba_sequent__TOP__0
                    IData/*31:0*/ __Vinline_0__eval_nba___Vinline_0__nba_sequent__TOP__0___Vdly__radar_nco__DOT__phase;
                    __Vinline_0__eval_nba___Vinline_0__nba_sequent__TOP__0___Vdly__radar_nco__DOT__phase = 0;
                    IData/*31:0*/ __Vinline_0__eval_nba___Vinline_0__nba_sequent__TOP__0___Vdly__radar_nco__DOT__freq_cur;
                    __Vinline_0__eval_nba___Vinline_0__nba_sequent__TOP__0___Vdly__radar_nco__DOT__freq_cur = 0;
                    __Vinline_0__eval_nba___Vinline_0__nba_sequent__TOP__0___Vdly__radar_nco__DOT__phase 
                        = vlSelfRef.radar_nco__DOT__phase;
                    __Vinline_0__eval_nba___Vinline_0__nba_sequent__TOP__0___Vdly__radar_nco__DOT__freq_cur 
                        = vlSelfRef.radar_nco__DOT__freq_cur;
                    vlSelfRef.out_valid = ((1U & (~ (IData)(vlSelfRef.rst))) 
                                           && (IData)(vlSelfRef.radar_nco__DOT__vld_s2));
                    if (vlSelfRef.rst) {
                        __Vinline_0__eval_nba___Vinline_0__nba_sequent__TOP__0___Vdly__radar_nco__DOT__phase = 0U;
                        __Vinline_0__eval_nba___Vinline_0__nba_sequent__TOP__0___Vdly__radar_nco__DOT__freq_cur = 0U;
                    } else if (vlSelfRef.restart) {
                        __Vinline_0__eval_nba___Vinline_0__nba_sequent__TOP__0___Vdly__radar_nco__DOT__phase = 0U;
                        __Vinline_0__eval_nba___Vinline_0__nba_sequent__TOP__0___Vdly__radar_nco__DOT__freq_cur 
                            = vlSelfRef.freq_start;
                    } else if (vlSelfRef.ena) {
                        __Vinline_0__eval_nba___Vinline_0__nba_sequent__TOP__0___Vdly__radar_nco__DOT__phase 
                            = (vlSelfRef.radar_nco__DOT__phase 
                               + vlSelfRef.radar_nco__DOT__freq_cur);
                        __Vinline_0__eval_nba___Vinline_0__nba_sequent__TOP__0___Vdly__radar_nco__DOT__freq_cur 
                            = (vlSelfRef.radar_nco__DOT__freq_cur 
                               + vlSelfRef.freq_slope);
                    }
                    vlSelfRef.radar_nco__DOT__freq_cur 
                        = __Vinline_0__eval_nba___Vinline_0__nba_sequent__TOP__0___Vdly__radar_nco__DOT__freq_cur;
                    vlSelfRef.radar_nco__DOT__vld_s2 
                        = ((~ (IData)(vlSelfRef.rst)) 
                           & (IData)(vlSelfRef.radar_nco__DOT__vld_s1));
                    vlSelfRef.out_q = ((IData)(vlSelfRef.rst)
                                        ? 0U : (0x0000ffffU 
                                                & ((IData)(vlSelfRef.radar_nco__DOT__neg_q_s2)
                                                    ? 
                                                   (- (IData)(vlSelfRef.radar_nco__DOT__rom_q_s2))
                                                    : (IData)(vlSelfRef.radar_nco__DOT__rom_q_s2))));
                    vlSelfRef.radar_nco__DOT__neg_q_s2 
                        = vlSelfRef.radar_nco__DOT__neg_q_s1;
                    vlSelfRef.radar_nco__DOT__rom_q_s2 
                        = vlSelfRef.radar_nco__DOT__sin_rom
                        [vlSelfRef.radar_nco__DOT__addr_q_s1];
                    vlSelfRef.out_i = ((IData)(vlSelfRef.rst)
                                        ? 0U : (0x0000ffffU 
                                                & ((IData)(vlSelfRef.radar_nco__DOT__neg_i_s2)
                                                    ? 
                                                   (- (IData)(vlSelfRef.radar_nco__DOT__rom_i_s2))
                                                    : (IData)(vlSelfRef.radar_nco__DOT__rom_i_s2))));
                    vlSelfRef.radar_nco__DOT__neg_i_s2 
                        = vlSelfRef.radar_nco__DOT__neg_i_s1;
                    vlSelfRef.radar_nco__DOT__rom_i_s2 
                        = vlSelfRef.radar_nco__DOT__sin_rom
                        [vlSelfRef.radar_nco__DOT__addr_i_s1];
                    if (vlSelfRef.rst) {
                        vlSelfRef.radar_nco__DOT__addr_q_s1 = 0U;
                        vlSelfRef.radar_nco__DOT__addr_i_s1 = 0U;
                    } else {
                        vlSelfRef.radar_nco__DOT__addr_q_s1 
                            = (0x000003ffU & ((0x40000000U 
                                               & vlSelfRef.radar_nco__DOT__phase)
                                               ? (~ 
                                                  (vlSelfRef.radar_nco__DOT__phase 
                                                   >> 0x00000014U))
                                               : (vlSelfRef.radar_nco__DOT__phase 
                                                  >> 0x00000014U)));
                        vlSelfRef.radar_nco__DOT__addr_i_s1 
                            = (0x000003ffU & ((0x00000400U 
                                               & ((IData)(0x0400U) 
                                                  + 
                                                  (vlSelfRef.radar_nco__DOT__phase 
                                                   >> 0x00000014U)))
                                               ? (~ 
                                                  (vlSelfRef.radar_nco__DOT__phase 
                                                   >> 0x00000014U))
                                               : (vlSelfRef.radar_nco__DOT__phase 
                                                  >> 0x00000014U)));
                    }
                    vlSelfRef.radar_nco__DOT__vld_s1 
                        = ((1U & (~ (IData)(vlSelfRef.rst))) 
                           && (IData)(vlSelfRef.ena));
                    vlSelfRef.radar_nco__DOT__neg_q_s1 
                        = ((1U & (~ (IData)(vlSelfRef.rst))) 
                           && (vlSelfRef.radar_nco__DOT__phase 
                               >> 0x0000001fU));
                    vlSelfRef.radar_nco__DOT__neg_i_s1 
                        = ((1U & (~ (IData)(vlSelfRef.rst))) 
                           && (1U & (((IData)(0x0400U) 
                                      + (vlSelfRef.radar_nco__DOT__phase 
                                         >> 0x00000014U)) 
                                     >> 0x0bU)));
                    vlSelfRef.radar_nco__DOT__phase 
                        = __Vinline_0__eval_nba___Vinline_0__nba_sequent__TOP__0___Vdly__radar_nco__DOT__phase;
                }
            }
        }
        Vradar_nco___024root___trigger_clear__act(vlSelfRef.__VnbaTriggered);
    }
    return (__VnbaExecute);
}

void Vradar_nco___024root___eval(Vradar_nco___024root* vlSelf) {
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vradar_nco___024root___eval\n"); );
    Vradar_nco__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    auto& vlSelfRef = std::ref(*vlSelf).get();
    // Locals
    IData/*31:0*/ __VnbaIterCount;
    // Body
    __VnbaIterCount = 0U;
    do {
        if (VL_UNLIKELY(((0x00002710U < __VnbaIterCount)))) {
#ifdef VL_DEBUG
            Vradar_nco___024root___dump_triggers__act(vlSelfRef.__VnbaTriggered, "nba"s);
#endif
            VL_FATAL_MT("/Users/lucasnaylor/radar-5g8-array/fpga/sim/../rtl/radar_nco.v", 65, "", "DIDNOTCONVERGE: NBA region did not converge after '--converge-limit' of 10000 tries");
        }
        __VnbaIterCount = ((IData)(1U) + __VnbaIterCount);
        vlSelfRef.__VactIterCount = 0U;
        do {
            if (VL_UNLIKELY(((0x00002710U < vlSelfRef.__VactIterCount)))) {
#ifdef VL_DEBUG
                Vradar_nco___024root___dump_triggers__act(vlSelfRef.__VactTriggered, "act"s);
#endif
                VL_FATAL_MT("/Users/lucasnaylor/radar-5g8-array/fpga/sim/../rtl/radar_nco.v", 65, "", "DIDNOTCONVERGE: Active region did not converge after '--converge-limit' of 10000 tries");
            }
            vlSelfRef.__VactIterCount = ((IData)(1U) 
                                         + vlSelfRef.__VactIterCount);
            vlSelfRef.__VactPhaseResult = Vradar_nco___024root___eval_phase__act(vlSelf);
        } while (vlSelfRef.__VactPhaseResult);
        vlSelfRef.__VnbaPhaseResult = Vradar_nco___024root___eval_phase__nba(vlSelf);
    } while (vlSelfRef.__VnbaPhaseResult);
}

#ifdef VL_DEBUG
void Vradar_nco___024root___eval_debug_assertions(Vradar_nco___024root* vlSelf) {
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vradar_nco___024root___eval_debug_assertions\n"); );
    Vradar_nco__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    auto& vlSelfRef = std::ref(*vlSelf).get();
    // Body
    if (VL_UNLIKELY(((vlSelfRef.clk & 0xfeU)))) {
        Verilated::overWidthError("clk");
    }
    if (VL_UNLIKELY(((vlSelfRef.rst & 0xfeU)))) {
        Verilated::overWidthError("rst");
    }
    if (VL_UNLIKELY(((vlSelfRef.ena & 0xfeU)))) {
        Verilated::overWidthError("ena");
    }
    if (VL_UNLIKELY(((vlSelfRef.restart & 0xfeU)))) {
        Verilated::overWidthError("restart");
    }
}
#endif  // VL_DEBUG
