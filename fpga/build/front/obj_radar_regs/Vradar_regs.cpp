// Verilated -*- C++ -*-
// DESCRIPTION: Verilator output: Model implementation (design independent parts)

#include "Vradar_regs__pch.h"

//============================================================
// Constructors

Vradar_regs::Vradar_regs(VerilatedContext* _vcontextp__, const char* _vcname__)
    : VerilatedModel{*_vcontextp__}
    , vlSymsp{new Vradar_regs__Syms(contextp(), _vcname__, this)}
    , clk{vlSymsp->TOP.clk}
    , rst{vlSymsp->TOP.rst}
    , set_stb{vlSymsp->TOP.set_stb}
    , set_addr{vlSymsp->TOP.set_addr}
    , ctrl_enable{vlSymsp->TOP.ctrl_enable}
    , ctrl_soft_reset{vlSymsp->TOP.ctrl_soft_reset}
    , ctrl_mimo_mode{vlSymsp->TOP.ctrl_mimo_mode}
    , ctrl_tx_enable{vlSymsp->TOP.ctrl_tx_enable}
    , ctrl_map_enable{vlSymsp->TOP.ctrl_map_enable}
    , ctrl_hits_enable{vlSymsp->TOP.ctrl_hits_enable}
    , ctrl_loopback{vlSymsp->TOP.ctrl_loopback}
    , dechirp_sh{vlSymsp->TOP.dechirp_sh}
    , win_we{vlSymsp->TOP.win_we}
    , cfar_guard_range{vlSymsp->TOP.cfar_guard_range}
    , cfar_guard_dopp{vlSymsp->TOP.cfar_guard_dopp}
    , cfar_train_range{vlSymsp->TOP.cfar_train_range}
    , cfar_train_dopp{vlSymsp->TOP.cfar_train_dopp}
    , cfar_kind{vlSymsp->TOP.cfar_kind}
    , map_decim_r{vlSymsp->TOP.map_decim_r}
    , map_decim_d{vlSymsp->TOP.map_decim_d}
    , zero_dopp{vlSymsp->TOP.zero_dopp}
    , geom_n_range_log2{vlSymsp->TOP.geom_n_range_log2}
    , geom_n_chirp_log2{vlSymsp->TOP.geom_n_chirp_log2}
    , version_stb{vlSymsp->TOP.version_stb}
    , ctrl_frame_limit{vlSymsp->TOP.ctrl_frame_limit}
    , t_sweep{vlSymsp->TOP.t_sweep}
    , t_pri{vlSymsp->TOP.t_pri}
    , n_chirp{vlSymsp->TOP.n_chirp}
    , tx_gain{vlSymsp->TOP.tx_gain}
    , fft_scale_d{vlSymsp->TOP.fft_scale_d}
    , win_addr{vlSymsp->TOP.win_addr}
    , range_zero{vlSymsp->TOP.range_zero}
    , max_hits{vlSymsp->TOP.max_hits}
    , set_data{vlSymsp->TOP.set_data}
    , freq_start{vlSymsp->TOP.freq_start}
    , freq_slope{vlSymsp->TOP.freq_slope}
    , fft_scale_r{vlSymsp->TOP.fft_scale_r}
    , win_data{vlSymsp->TOP.win_data}
    , cfar_alpha{vlSymsp->TOP.cfar_alpha}
    , test_tone{vlSymsp->TOP.test_tone}
    , rootp{&(vlSymsp->TOP)}
{
    // Register model with the context
    contextp()->addModel(this);
}

Vradar_regs::Vradar_regs(const char* _vcname__)
    : Vradar_regs(Verilated::threadContextp(), _vcname__)
{
}

//============================================================
// Destructor

Vradar_regs::~Vradar_regs() {
    delete vlSymsp;
}

//============================================================
// Evaluation function

#ifdef VL_DEBUG
void Vradar_regs___024root___eval_debug_assertions(Vradar_regs___024root* vlSelf);
#endif  // VL_DEBUG
void Vradar_regs___024root___eval_static(Vradar_regs___024root* vlSelf);
void Vradar_regs___024root___eval_initial(Vradar_regs___024root* vlSelf);
void Vradar_regs___024root___eval_settle(Vradar_regs___024root* vlSelf);
void Vradar_regs___024root___eval(Vradar_regs___024root* vlSelf);

void Vradar_regs::eval_step() {
    VL_DEBUG_IF(VL_DBG_MSGF("+++++TOP Evaluate Vradar_regs::eval_step\n"); );
#ifdef VL_DEBUG
    // Debug assertions
    Vradar_regs___024root___eval_debug_assertions(&(vlSymsp->TOP));
#endif  // VL_DEBUG
    vlSymsp->__Vm_deleter.deleteAll();
    if (VL_UNLIKELY(!vlSymsp->__Vm_didInit)) {
        VL_DEBUG_IF(VL_DBG_MSGF("+ Initial\n"););
        Vradar_regs___024root___eval_static(&(vlSymsp->TOP));
        Vradar_regs___024root___eval_initial(&(vlSymsp->TOP));
        Vradar_regs___024root___eval_settle(&(vlSymsp->TOP));
        vlSymsp->__Vm_didInit = true;
    }
    VL_DEBUG_IF(VL_DBG_MSGF("+ Eval\n"););
    Vradar_regs___024root___eval(&(vlSymsp->TOP));
    // Evaluate cleanup
    Verilated::endOfEval(vlSymsp->__Vm_evalMsgQp);
}

//============================================================
// Events and timing
bool Vradar_regs::eventsPending() { return false; }

uint64_t Vradar_regs::nextTimeSlot() {
    VL_FATAL_MT(__FILE__, __LINE__, "", "No delays in the design");
    return 0;
}

//============================================================
// Utilities

const char* Vradar_regs::name() const {
    return vlSymsp->name();
}

//============================================================
// Invoke final blocks

void Vradar_regs___024root___eval_final(Vradar_regs___024root* vlSelf);

VL_ATTR_COLD void Vradar_regs::final() {
    contextp()->executingFinal(true);
    Vradar_regs___024root___eval_final(&(vlSymsp->TOP));
    contextp()->executingFinal(false);
}

//============================================================
// Implementations of abstract methods from VerilatedModel

const char* Vradar_regs::hierName() const { return vlSymsp->name(); }
const char* Vradar_regs::modelName() const { return "Vradar_regs"; }
unsigned Vradar_regs::threads() const { return 1; }
void Vradar_regs::prepareClone() const { contextp()->prepareClone(); }
void Vradar_regs::atClone() const {
    contextp()->threadPoolpOnClone();
}
