// Verilated -*- C++ -*-
// DESCRIPTION: Verilator output: Model implementation (design independent parts)

#include "Vradar_window__pch.h"

//============================================================
// Constructors

Vradar_window::Vradar_window(VerilatedContext* _vcontextp__, const char* _vcname__)
    : VerilatedModel{*_vcontextp__}
    , vlSymsp{new Vradar_window__Syms(contextp(), _vcname__, this)}
    , clk{vlSymsp->TOP.clk}
    , rst{vlSymsp->TOP.rst}
    , in_valid{vlSymsp->TOP.in_valid}
    , coef_we{vlSymsp->TOP.coef_we}
    , out_valid{vlSymsp->TOP.out_valid}
    , in_i{vlSymsp->TOP.in_i}
    , in_q{vlSymsp->TOP.in_q}
    , sample_idx{vlSymsp->TOP.sample_idx}
    , coef_addr{vlSymsp->TOP.coef_addr}
    , coef_data{vlSymsp->TOP.coef_data}
    , out_i{vlSymsp->TOP.out_i}
    , out_q{vlSymsp->TOP.out_q}
    , rootp{&(vlSymsp->TOP)}
{
    // Register model with the context
    contextp()->addModel(this);
}

Vradar_window::Vradar_window(const char* _vcname__)
    : Vradar_window(Verilated::threadContextp(), _vcname__)
{
}

//============================================================
// Destructor

Vradar_window::~Vradar_window() {
    delete vlSymsp;
}

//============================================================
// Evaluation function

#ifdef VL_DEBUG
void Vradar_window___024root___eval_debug_assertions(Vradar_window___024root* vlSelf);
#endif  // VL_DEBUG
void Vradar_window___024root___eval_static(Vradar_window___024root* vlSelf);
void Vradar_window___024root___eval_initial(Vradar_window___024root* vlSelf);
void Vradar_window___024root___eval_settle(Vradar_window___024root* vlSelf);
void Vradar_window___024root___eval(Vradar_window___024root* vlSelf);

void Vradar_window::eval_step() {
    VL_DEBUG_IF(VL_DBG_MSGF("+++++TOP Evaluate Vradar_window::eval_step\n"); );
#ifdef VL_DEBUG
    // Debug assertions
    Vradar_window___024root___eval_debug_assertions(&(vlSymsp->TOP));
#endif  // VL_DEBUG
    vlSymsp->__Vm_deleter.deleteAll();
    if (VL_UNLIKELY(!vlSymsp->__Vm_didInit)) {
        VL_DEBUG_IF(VL_DBG_MSGF("+ Initial\n"););
        Vradar_window___024root___eval_static(&(vlSymsp->TOP));
        Vradar_window___024root___eval_initial(&(vlSymsp->TOP));
        Vradar_window___024root___eval_settle(&(vlSymsp->TOP));
        vlSymsp->__Vm_didInit = true;
    }
    VL_DEBUG_IF(VL_DBG_MSGF("+ Eval\n"););
    Vradar_window___024root___eval(&(vlSymsp->TOP));
    // Evaluate cleanup
    Verilated::endOfEval(vlSymsp->__Vm_evalMsgQp);
}

//============================================================
// Events and timing
bool Vradar_window::eventsPending() { return false; }

uint64_t Vradar_window::nextTimeSlot() {
    VL_FATAL_MT(__FILE__, __LINE__, "", "No delays in the design");
    return 0;
}

//============================================================
// Utilities

const char* Vradar_window::name() const {
    return vlSymsp->name();
}

//============================================================
// Invoke final blocks

void Vradar_window___024root___eval_final(Vradar_window___024root* vlSelf);

VL_ATTR_COLD void Vradar_window::final() {
    contextp()->executingFinal(true);
    Vradar_window___024root___eval_final(&(vlSymsp->TOP));
    contextp()->executingFinal(false);
}

//============================================================
// Implementations of abstract methods from VerilatedModel

const char* Vradar_window::hierName() const { return vlSymsp->name(); }
const char* Vradar_window::modelName() const { return "Vradar_window"; }
unsigned Vradar_window::threads() const { return 1; }
void Vradar_window::prepareClone() const { contextp()->prepareClone(); }
void Vradar_window::atClone() const {
    contextp()->threadPoolpOnClone();
}
