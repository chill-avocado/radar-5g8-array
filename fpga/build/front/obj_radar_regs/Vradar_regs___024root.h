// Verilated -*- C++ -*-
// DESCRIPTION: Verilator output: Design internal header
// See Vradar_regs.h for the primary calling header

#ifndef VERILATED_VRADAR_REGS___024ROOT_H_
#define VERILATED_VRADAR_REGS___024ROOT_H_  // guard

#include "verilated.h"


class Vradar_regs__Syms;

class alignas(VL_CACHE_LINE_BYTES) Vradar_regs___024root final {
  public:

    // DESIGN SPECIFIC STATE
    VL_IN8(clk,0,0);
    VL_IN8(rst,0,0);
    VL_IN8(set_stb,0,0);
    VL_IN8(set_addr,7,0);
    VL_OUT8(ctrl_enable,0,0);
    VL_OUT8(ctrl_soft_reset,0,0);
    VL_OUT8(ctrl_mimo_mode,1,0);
    VL_OUT8(ctrl_tx_enable,0,0);
    VL_OUT8(ctrl_map_enable,0,0);
    VL_OUT8(ctrl_hits_enable,0,0);
    VL_OUT8(ctrl_loopback,0,0);
    VL_OUT8(dechirp_sh,3,0);
    VL_OUT8(win_we,0,0);
    VL_OUT8(cfar_guard_range,3,0);
    VL_OUT8(cfar_guard_dopp,3,0);
    VL_OUT8(cfar_train_range,3,0);
    VL_OUT8(cfar_train_dopp,3,0);
    VL_OUT8(cfar_kind,1,0);
    VL_OUT8(map_decim_r,7,0);
    VL_OUT8(map_decim_d,7,0);
    VL_OUT8(zero_dopp,7,0);
    VL_OUT8(geom_n_range_log2,3,0);
    VL_OUT8(geom_n_chirp_log2,3,0);
    VL_OUT8(version_stb,0,0);
    CData/*0:0*/ __Vtrigprevexpr___TOP__clk__0;
    CData/*0:0*/ __VactPhaseResult;
    CData/*0:0*/ __VnbaPhaseResult;
    VL_OUT16(ctrl_frame_limit,15,0);
    VL_OUT16(t_sweep,15,0);
    VL_OUT16(t_pri,15,0);
    VL_OUT16(n_chirp,15,0);
    VL_OUT16(tx_gain,15,0);
    VL_OUT16(fft_scale_d,15,0);
    VL_OUT16(win_addr,15,0);
    VL_OUT16(range_zero,15,0);
    VL_OUT16(max_hits,15,0);
    VL_IN(set_data,31,0);
    VL_OUT(freq_start,31,0);
    VL_OUT(freq_slope,31,0);
    VL_OUT(fft_scale_r,19,0);
    VL_OUT(win_data,31,0);
    VL_OUT(cfar_alpha,31,0);
    VL_OUT(test_tone,31,0);
    IData/*31:0*/ __VactIterCount;
    VlUnpacked<QData/*63:0*/, 1> __VactTriggered;
    VlUnpacked<QData/*63:0*/, 1> __VnbaTriggered;

    // INTERNAL VARIABLES
    Vradar_regs__Syms* vlSymsp;
    const char* vlNamep;

    // CONSTRUCTORS
    Vradar_regs___024root(Vradar_regs__Syms* symsp, const char* namep);
    ~Vradar_regs___024root();
    VL_UNCOPYABLE(Vradar_regs___024root);

    // INTERNAL METHODS
    void __Vconfigure(bool first);
};


#endif  // guard
