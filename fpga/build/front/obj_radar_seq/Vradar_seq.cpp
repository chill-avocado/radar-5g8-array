// Verilated -*- C++ -*-
// DESCRIPTION: Verilator output: Model implementation (design independent parts)

#include "Vradar_seq__pch.h"

//============================================================
// Constructors

Vradar_seq::Vradar_seq(VerilatedContext* _vcontextp__, const char* _vcname__)
    : VerilatedModel{*_vcontextp__}
    , vlSymsp{new Vradar_seq__Syms(contextp(), _vcname__, this)}
    , clk{vlSymsp->TOP.clk}
    , rst{vlSymsp->TOP.rst}
    , enable{vlSymsp->TOP.enable}
    , mimo_mode{vlSymsp->TOP.mimo_mode}
    , tx_enable{vlSymsp->TOP.tx_enable}
    , nco_restart{vlSymsp->TOP.nco_restart}
    , nco_ena{vlSymsp->TOP.nco_ena}
    , tx0_ena{vlSymsp->TOP.tx0_ena}
    , tx1_ena{vlSymsp->TOP.tx1_ena}
    , tx_invert{vlSymsp->TOP.tx_invert}
    , adc_gate{vlSymsp->TOP.adc_gate}
    , tx_sel{vlSymsp->TOP.tx_sel}
    , frame_start{vlSymsp->TOP.frame_start}
    , frame_end{vlSymsp->TOP.frame_end}
    , running{vlSymsp->TOP.running}
    , t_sweep{vlSymsp->TOP.t_sweep}
    , t_pri{vlSymsp->TOP.t_pri}
    , n_chirp{vlSymsp->TOP.n_chirp}
    , sample_idx{vlSymsp->TOP.sample_idx}
    , chirp_idx{vlSymsp->TOP.chirp_idx}
    , rootp{&(vlSymsp->TOP)}
{
    // Register model with the context
    contextp()->addModel(this);
}

Vradar_seq::Vradar_seq(const char* _vcname__)
    : Vradar_seq(Verilated::threadContextp(), _vcname__)
{
}

//============================================================
// Destructor

Vradar_seq::~Vradar_seq() {
    delete vlSymsp;
}

//============================================================
// Evaluation function

#ifdef VL_DEBUG
void Vradar_seq___024root___eval_debug_assertions(Vradar_seq___024root* vlSelf);
#endif  // VL_DEBUG
void Vradar_seq___024root___eval_static(Vradar_seq___024root* vlSelf);
void Vradar_seq___024root___eval_initial(Vradar_seq___024root* vlSelf);
void Vradar_seq___024root___eval_settle(Vradar_seq___024root* vlSelf);
void Vradar_seq___024root___eval(Vradar_seq___024root* vlSelf);

void Vradar_seq::eval_step() {
    VL_DEBUG_IF(VL_DBG_MSGF("+++++TOP Evaluate Vradar_seq::eval_step\n"); );
#ifdef VL_DEBUG
    // Debug assertions
    Vradar_seq___024root___eval_debug_assertions(&(vlSymsp->TOP));
#endif  // VL_DEBUG
    vlSymsp->__Vm_deleter.deleteAll();
    if (VL_UNLIKELY(!vlSymsp->__Vm_didInit)) {
        VL_DEBUG_IF(VL_DBG_MSGF("+ Initial\n"););
        Vradar_seq___024root___eval_static(&(vlSymsp->TOP));
        Vradar_seq___024root___eval_initial(&(vlSymsp->TOP));
        Vradar_seq___024root___eval_settle(&(vlSymsp->TOP));
        vlSymsp->__Vm_didInit = true;
    }
    VL_DEBUG_IF(VL_DBG_MSGF("+ Eval\n"););
    Vradar_seq___024root___eval(&(vlSymsp->TOP));
    // Evaluate cleanup
    Verilated::endOfEval(vlSymsp->__Vm_evalMsgQp);
}

//============================================================
// Events and timing
bool Vradar_seq::eventsPending() { return false; }

uint64_t Vradar_seq::nextTimeSlot() {
    VL_FATAL_MT(__FILE__, __LINE__, "", "No delays in the design");
    return 0;
}

//============================================================
// Utilities

const char* Vradar_seq::name() const {
    return vlSymsp->name();
}

//============================================================
// Invoke final blocks

void Vradar_seq___024root___eval_final(Vradar_seq___024root* vlSelf);

VL_ATTR_COLD void Vradar_seq::final() {
    contextp()->executingFinal(true);
    Vradar_seq___024root___eval_final(&(vlSymsp->TOP));
    contextp()->executingFinal(false);
}

//============================================================
// Implementations of abstract methods from VerilatedModel

const char* Vradar_seq::hierName() const { return vlSymsp->name(); }
const char* Vradar_seq::modelName() const { return "Vradar_seq"; }
unsigned Vradar_seq::threads() const { return 1; }
void Vradar_seq::prepareClone() const { contextp()->prepareClone(); }
void Vradar_seq::atClone() const {
    contextp()->threadPoolpOnClone();
}
