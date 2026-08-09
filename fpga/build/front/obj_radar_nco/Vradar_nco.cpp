// Verilated -*- C++ -*-
// DESCRIPTION: Verilator output: Model implementation (design independent parts)

#include "Vradar_nco__pch.h"

//============================================================
// Constructors

Vradar_nco::Vradar_nco(VerilatedContext* _vcontextp__, const char* _vcname__)
    : VerilatedModel{*_vcontextp__}
    , vlSymsp{new Vradar_nco__Syms(contextp(), _vcname__, this)}
    , clk{vlSymsp->TOP.clk}
    , rst{vlSymsp->TOP.rst}
    , ena{vlSymsp->TOP.ena}
    , restart{vlSymsp->TOP.restart}
    , out_valid{vlSymsp->TOP.out_valid}
    , out_i{vlSymsp->TOP.out_i}
    , out_q{vlSymsp->TOP.out_q}
    , freq_start{vlSymsp->TOP.freq_start}
    , freq_slope{vlSymsp->TOP.freq_slope}
    , rootp{&(vlSymsp->TOP)}
{
    // Register model with the context
    contextp()->addModel(this);
}

Vradar_nco::Vradar_nco(const char* _vcname__)
    : Vradar_nco(Verilated::threadContextp(), _vcname__)
{
}

//============================================================
// Destructor

Vradar_nco::~Vradar_nco() {
    delete vlSymsp;
}

//============================================================
// Evaluation function

#ifdef VL_DEBUG
void Vradar_nco___024root___eval_debug_assertions(Vradar_nco___024root* vlSelf);
#endif  // VL_DEBUG
void Vradar_nco___024root___eval_static(Vradar_nco___024root* vlSelf);
void Vradar_nco___024root___eval_initial(Vradar_nco___024root* vlSelf);
void Vradar_nco___024root___eval_settle(Vradar_nco___024root* vlSelf);
void Vradar_nco___024root___eval(Vradar_nco___024root* vlSelf);

void Vradar_nco::eval_step() {
    VL_DEBUG_IF(VL_DBG_MSGF("+++++TOP Evaluate Vradar_nco::eval_step\n"); );
#ifdef VL_DEBUG
    // Debug assertions
    Vradar_nco___024root___eval_debug_assertions(&(vlSymsp->TOP));
#endif  // VL_DEBUG
    vlSymsp->__Vm_deleter.deleteAll();
    if (VL_UNLIKELY(!vlSymsp->__Vm_didInit)) {
        VL_DEBUG_IF(VL_DBG_MSGF("+ Initial\n"););
        Vradar_nco___024root___eval_static(&(vlSymsp->TOP));
        Vradar_nco___024root___eval_initial(&(vlSymsp->TOP));
        Vradar_nco___024root___eval_settle(&(vlSymsp->TOP));
        vlSymsp->__Vm_didInit = true;
    }
    VL_DEBUG_IF(VL_DBG_MSGF("+ Eval\n"););
    Vradar_nco___024root___eval(&(vlSymsp->TOP));
    // Evaluate cleanup
    Verilated::endOfEval(vlSymsp->__Vm_evalMsgQp);
}

//============================================================
// Events and timing
bool Vradar_nco::eventsPending() { return false; }

uint64_t Vradar_nco::nextTimeSlot() {
    VL_FATAL_MT(__FILE__, __LINE__, "", "No delays in the design");
    return 0;
}

//============================================================
// Utilities

const char* Vradar_nco::name() const {
    return vlSymsp->name();
}

//============================================================
// Invoke final blocks

void Vradar_nco___024root___eval_final(Vradar_nco___024root* vlSelf);

VL_ATTR_COLD void Vradar_nco::final() {
    contextp()->executingFinal(true);
    Vradar_nco___024root___eval_final(&(vlSymsp->TOP));
    contextp()->executingFinal(false);
}

//============================================================
// Implementations of abstract methods from VerilatedModel

const char* Vradar_nco::hierName() const { return vlSymsp->name(); }
const char* Vradar_nco::modelName() const { return "Vradar_nco"; }
unsigned Vradar_nco::threads() const { return 1; }
void Vradar_nco::prepareClone() const { contextp()->prepareClone(); }
void Vradar_nco::atClone() const {
    contextp()->threadPoolpOnClone();
}
