// Verilated -*- C++ -*-
// DESCRIPTION: Verilator output: Design implementation internals
// See Vradar_dechirp.h for the primary calling header

#include "Vradar_dechirp__pch.h"

bool Vradar_dechirp___024root___trigger_anySet__act(const VlUnpacked<QData/*63:0*/, 1> &in) {
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vradar_dechirp___024root___trigger_anySet__act\n"); );
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

void Vradar_dechirp___024root___nba_sequent__TOP__0(Vradar_dechirp___024root* vlSelf) {
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vradar_dechirp___024root___nba_sequent__TOP__0\n"); );
    Vradar_dechirp__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    auto& vlSelfRef = std::ref(*vlSelf).get();
    // Locals
    QData/*33:0*/ radar_dechirp__DOT__round_sat__Vstatic__half;
    radar_dechirp__DOT__round_sat__Vstatic__half = 0;
    QData/*33:0*/ radar_dechirp__DOT__round_sat__Vstatic__t;
    radar_dechirp__DOT__round_sat__Vstatic__t = 0;
    SData/*15:0*/ __Vfunc_radar_dechirp__DOT__round_sat__0__Vfuncout;
    __Vfunc_radar_dechirp__DOT__round_sat__0__Vfuncout = 0;
    QData/*33:0*/ __Vfunc_radar_dechirp__DOT__round_sat__0__v;
    __Vfunc_radar_dechirp__DOT__round_sat__0__v = 0;
    CData/*5:0*/ __Vfunc_radar_dechirp__DOT__round_sat__0__sh;
    __Vfunc_radar_dechirp__DOT__round_sat__0__sh = 0;
    SData/*15:0*/ __Vfunc_radar_dechirp__DOT__round_sat__1__Vfuncout;
    __Vfunc_radar_dechirp__DOT__round_sat__1__Vfuncout = 0;
    QData/*33:0*/ __Vfunc_radar_dechirp__DOT__round_sat__1__v;
    __Vfunc_radar_dechirp__DOT__round_sat__1__v = 0;
    CData/*5:0*/ __Vfunc_radar_dechirp__DOT__round_sat__1__sh;
    __Vfunc_radar_dechirp__DOT__round_sat__1__sh = 0;
    // Body
    vlSelfRef.out_valid = ((~ (IData)(vlSelfRef.rst)) 
                           & (IData)(vlSelfRef.radar_dechirp__DOT__v_s3));
    __Vfunc_radar_dechirp__DOT__round_sat__0__sh = vlSelfRef.radar_dechirp__DOT__sh_s3;
    __Vfunc_radar_dechirp__DOT__round_sat__0__v = vlSelfRef.radar_dechirp__DOT__rr_s3;
    radar_dechirp__DOT__round_sat__Vstatic__half = 
        (0x00000003ffffffffULL & (1ULL << (0x0000003fU 
                                           & ((IData)(__Vfunc_radar_dechirp__DOT__round_sat__0__sh) 
                                              - (IData)(1U)))));
    radar_dechirp__DOT__round_sat__Vstatic__t = (0x00000003ffffffffULL 
                                                 & VL_SHIFTRS_QQI(34,34,6, 
                                                                  (0x00000003ffffffffULL 
                                                                   & (__Vfunc_radar_dechirp__DOT__round_sat__0__v 
                                                                      + radar_dechirp__DOT__round_sat__Vstatic__half)), (IData)(__Vfunc_radar_dechirp__DOT__round_sat__0__sh)));
    __Vfunc_radar_dechirp__DOT__round_sat__0__Vfuncout 
        = (VL_LTS_IQQ(34, 0x0000000000007fffULL, radar_dechirp__DOT__round_sat__Vstatic__t)
            ? 0x00007fffU : (VL_GTS_IQQ(34, 0x00000003ffff8000ULL, radar_dechirp__DOT__round_sat__Vstatic__t)
                              ? 0x00008000U : (0x0000ffffU 
                                               & (IData)(radar_dechirp__DOT__round_sat__Vstatic__t))));
    vlSelfRef.out_i = __Vfunc_radar_dechirp__DOT__round_sat__0__Vfuncout;
    __Vfunc_radar_dechirp__DOT__round_sat__1__sh = vlSelfRef.radar_dechirp__DOT__sh_s3;
    __Vfunc_radar_dechirp__DOT__round_sat__1__v = vlSelfRef.radar_dechirp__DOT__ii_s3;
    radar_dechirp__DOT__round_sat__Vstatic__half = 
        (0x00000003ffffffffULL & (1ULL << (0x0000003fU 
                                           & ((IData)(__Vfunc_radar_dechirp__DOT__round_sat__1__sh) 
                                              - (IData)(1U)))));
    radar_dechirp__DOT__round_sat__Vstatic__t = (0x00000003ffffffffULL 
                                                 & VL_SHIFTRS_QQI(34,34,6, 
                                                                  (0x00000003ffffffffULL 
                                                                   & (__Vfunc_radar_dechirp__DOT__round_sat__1__v 
                                                                      + radar_dechirp__DOT__round_sat__Vstatic__half)), (IData)(__Vfunc_radar_dechirp__DOT__round_sat__1__sh)));
    __Vfunc_radar_dechirp__DOT__round_sat__1__Vfuncout 
        = (VL_LTS_IQQ(34, 0x0000000000007fffULL, radar_dechirp__DOT__round_sat__Vstatic__t)
            ? 0x00007fffU : (VL_GTS_IQQ(34, 0x00000003ffff8000ULL, radar_dechirp__DOT__round_sat__Vstatic__t)
                              ? 0x00008000U : (0x0000ffffU 
                                               & (IData)(radar_dechirp__DOT__round_sat__Vstatic__t))));
    vlSelfRef.out_q = __Vfunc_radar_dechirp__DOT__round_sat__1__Vfuncout;
    vlSelfRef.radar_dechirp__DOT__v_s3 = ((~ (IData)(vlSelfRef.rst)) 
                                          & (IData)(vlSelfRef.radar_dechirp__DOT__v_s2));
    vlSelfRef.radar_dechirp__DOT__sh_s3 = vlSelfRef.radar_dechirp__DOT__sh_s2;
    vlSelfRef.radar_dechirp__DOT__rr_s3 = (0x00000003ffffffffULL 
                                           & ((((QData)((IData)(
                                                                (3U 
                                                                 & (- (IData)(
                                                                              (vlSelfRef.radar_dechirp__DOT__p_ii_s2 
                                                                               >> 0x0000001fU)))))) 
                                                << 0x00000020U) 
                                               | (QData)((IData)(vlSelfRef.radar_dechirp__DOT__p_ii_s2))) 
                                              + (((QData)((IData)(
                                                                  (3U 
                                                                   & (- (IData)(
                                                                                (vlSelfRef.radar_dechirp__DOT__p_qq_s2 
                                                                                >> 0x0000001fU)))))) 
                                                  << 0x00000020U) 
                                                 | (QData)((IData)(vlSelfRef.radar_dechirp__DOT__p_qq_s2)))));
    vlSelfRef.radar_dechirp__DOT__ii_s3 = (0x00000003ffffffffULL 
                                           & ((((QData)((IData)(
                                                                (3U 
                                                                 & (- (IData)(
                                                                              (vlSelfRef.radar_dechirp__DOT__p_iq_s2 
                                                                               >> 0x0000001fU)))))) 
                                                << 0x00000020U) 
                                               | (QData)((IData)(vlSelfRef.radar_dechirp__DOT__p_iq_s2))) 
                                              - (((QData)((IData)(
                                                                  (3U 
                                                                   & (- (IData)(
                                                                                (vlSelfRef.radar_dechirp__DOT__p_qi_s2 
                                                                                >> 0x0000001fU)))))) 
                                                  << 0x00000020U) 
                                                 | (QData)((IData)(vlSelfRef.radar_dechirp__DOT__p_qi_s2)))));
    vlSelfRef.radar_dechirp__DOT__v_s2 = ((~ (IData)(vlSelfRef.rst)) 
                                          & (IData)(vlSelfRef.radar_dechirp__DOT__v_s1));
    vlSelfRef.radar_dechirp__DOT__sh_s2 = vlSelfRef.radar_dechirp__DOT__sh_s1;
    vlSelfRef.radar_dechirp__DOT__p_ii_s2 = VL_MULS_III(32, 
                                                        VL_EXTENDS_II(32,16, (IData)(vlSelfRef.radar_dechirp__DOT__a_i_s1)), 
                                                        VL_EXTENDS_II(32,16, (IData)(vlSelfRef.radar_dechirp__DOT__b_i_s1)));
    vlSelfRef.radar_dechirp__DOT__p_qq_s2 = VL_MULS_III(32, 
                                                        VL_EXTENDS_II(32,16, (IData)(vlSelfRef.radar_dechirp__DOT__a_q_s1)), 
                                                        VL_EXTENDS_II(32,16, (IData)(vlSelfRef.radar_dechirp__DOT__b_q_s1)));
    vlSelfRef.radar_dechirp__DOT__p_iq_s2 = VL_MULS_III(32, 
                                                        VL_EXTENDS_II(32,16, (IData)(vlSelfRef.radar_dechirp__DOT__a_i_s1)), 
                                                        VL_EXTENDS_II(32,16, (IData)(vlSelfRef.radar_dechirp__DOT__b_q_s1)));
    vlSelfRef.radar_dechirp__DOT__p_qi_s2 = VL_MULS_III(32, 
                                                        VL_EXTENDS_II(32,16, (IData)(vlSelfRef.radar_dechirp__DOT__a_q_s1)), 
                                                        VL_EXTENDS_II(32,16, (IData)(vlSelfRef.radar_dechirp__DOT__b_i_s1)));
    vlSelfRef.radar_dechirp__DOT__v_s1 = ((~ (IData)(vlSelfRef.rst)) 
                                          & (IData)(vlSelfRef.in_valid));
    vlSelfRef.radar_dechirp__DOT__sh_s1 = (0x0000003fU 
                                           & ((IData)(0x0fU) 
                                              + (IData)(vlSelfRef.shift)));
    vlSelfRef.radar_dechirp__DOT__a_i_s1 = vlSelfRef.in_i;
    vlSelfRef.radar_dechirp__DOT__b_q_s1 = vlSelfRef.ref_q;
    vlSelfRef.radar_dechirp__DOT__a_q_s1 = vlSelfRef.in_q;
    vlSelfRef.radar_dechirp__DOT__b_i_s1 = vlSelfRef.ref_i;
}

void Vradar_dechirp___024root___trigger_orInto__act_vec_vec(VlUnpacked<QData/*63:0*/, 1> &out, const VlUnpacked<QData/*63:0*/, 1> &in) {
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vradar_dechirp___024root___trigger_orInto__act_vec_vec\n"); );
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
VL_ATTR_COLD void Vradar_dechirp___024root___dump_triggers__act(const VlUnpacked<QData/*63:0*/, 1> &triggers, const std::string &tag);
#endif  // VL_DEBUG

bool Vradar_dechirp___024root___eval_phase__act(Vradar_dechirp___024root* vlSelf) {
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vradar_dechirp___024root___eval_phase__act\n"); );
    Vradar_dechirp__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
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
        Vradar_dechirp___024root___dump_triggers__act(vlSelfRef.__VactTriggered, "act"s);
    }
#endif
    Vradar_dechirp___024root___trigger_orInto__act_vec_vec(vlSelfRef.__VnbaTriggered, vlSelfRef.__VactTriggered);
    return (0U);
}

void Vradar_dechirp___024root___trigger_clear__act(VlUnpacked<QData/*63:0*/, 1> &out) {
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vradar_dechirp___024root___trigger_clear__act\n"); );
    // Locals
    IData/*31:0*/ n;
    // Body
    n = 0U;
    do {
        out[n] = 0ULL;
        n = ((IData)(1U) + n);
    } while ((1U > n));
}

bool Vradar_dechirp___024root___eval_phase__nba(Vradar_dechirp___024root* vlSelf) {
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vradar_dechirp___024root___eval_phase__nba\n"); );
    Vradar_dechirp__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    auto& vlSelfRef = std::ref(*vlSelf).get();
    // Locals
    CData/*0:0*/ __VnbaExecute;
    // Body
    __VnbaExecute = Vradar_dechirp___024root___trigger_anySet__act(vlSelfRef.__VnbaTriggered);
    if (__VnbaExecute) {
        {
            // Inlined CFunc: _eval_nba
            if ((1ULL & vlSelfRef.__VnbaTriggered[0U])) {
                Vradar_dechirp___024root___nba_sequent__TOP__0(vlSelf);
            }
        }
        Vradar_dechirp___024root___trigger_clear__act(vlSelfRef.__VnbaTriggered);
    }
    return (__VnbaExecute);
}

void Vradar_dechirp___024root___eval(Vradar_dechirp___024root* vlSelf) {
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vradar_dechirp___024root___eval\n"); );
    Vradar_dechirp__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    auto& vlSelfRef = std::ref(*vlSelf).get();
    // Locals
    IData/*31:0*/ __VnbaIterCount;
    // Body
    __VnbaIterCount = 0U;
    do {
        if (VL_UNLIKELY(((0x00002710U < __VnbaIterCount)))) {
#ifdef VL_DEBUG
            Vradar_dechirp___024root___dump_triggers__act(vlSelfRef.__VnbaTriggered, "nba"s);
#endif
            VL_FATAL_MT("/Users/lucasnaylor/radar-5g8-array/fpga/sim/../rtl/radar_dechirp.v", 46, "", "DIDNOTCONVERGE: NBA region did not converge after '--converge-limit' of 10000 tries");
        }
        __VnbaIterCount = ((IData)(1U) + __VnbaIterCount);
        vlSelfRef.__VactIterCount = 0U;
        do {
            if (VL_UNLIKELY(((0x00002710U < vlSelfRef.__VactIterCount)))) {
#ifdef VL_DEBUG
                Vradar_dechirp___024root___dump_triggers__act(vlSelfRef.__VactTriggered, "act"s);
#endif
                VL_FATAL_MT("/Users/lucasnaylor/radar-5g8-array/fpga/sim/../rtl/radar_dechirp.v", 46, "", "DIDNOTCONVERGE: Active region did not converge after '--converge-limit' of 10000 tries");
            }
            vlSelfRef.__VactIterCount = ((IData)(1U) 
                                         + vlSelfRef.__VactIterCount);
            vlSelfRef.__VactPhaseResult = Vradar_dechirp___024root___eval_phase__act(vlSelf);
        } while (vlSelfRef.__VactPhaseResult);
        vlSelfRef.__VnbaPhaseResult = Vradar_dechirp___024root___eval_phase__nba(vlSelf);
    } while (vlSelfRef.__VnbaPhaseResult);
}

#ifdef VL_DEBUG
void Vradar_dechirp___024root___eval_debug_assertions(Vradar_dechirp___024root* vlSelf) {
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vradar_dechirp___024root___eval_debug_assertions\n"); );
    Vradar_dechirp__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
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
    if (VL_UNLIKELY(((vlSelfRef.shift & 0xf0U)))) {
        Verilated::overWidthError("shift");
    }
}
#endif  // VL_DEBUG
