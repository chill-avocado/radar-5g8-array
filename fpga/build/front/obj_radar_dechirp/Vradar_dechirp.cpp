// Verilated -*- C++ -*-
// DESCRIPTION: Verilator output: Model implementation (design independent parts)

#include "Vradar_dechirp__pch.h"

//============================================================
// Constructors

Vradar_dechirp::Vradar_dechirp(VerilatedContext* _vcontextp__, const char* _vcname__)
    : VerilatedModel{*_vcontextp__}
    , vlSymsp{new Vradar_dechirp__Syms(contextp(), _vcname__, this)}
    , clk{vlSymsp->TOP.clk}
    , rst{vlSymsp->TOP.rst}
    , in_valid{vlSymsp->TOP.in_valid}
    , shift{vlSymsp->TOP.shift}
    , out_valid{vlSymsp->TOP.out_valid}
    , in_i{vlSymsp->TOP.in_i}
    , in_q{vlSymsp->TOP.in_q}
    , ref_i{vlSymsp->TOP.ref_i}
    , ref_q{vlSymsp->TOP.ref_q}
    , out_i{vlSymsp->TOP.out_i}
    , out_q{vlSymsp->TOP.out_q}
    , rootp{&(vlSymsp->TOP)}
{
    // Register model with the context
    contextp()->addModel(this);
}

Vradar_dechirp::Vradar_dechirp(const char* _vcname__)
    : Vradar_dechirp(Verilated::threadContextp(), _vcname__)
{
}

//============================================================
// Destructor

Vradar_dechirp::~Vradar_dechirp() {
    delete vlSymsp;
}

//============================================================
// Evaluation function

#ifdef VL_DEBUG
void Vradar_dechirp___024root___eval_debug_assertions(Vradar_dechirp___024root* vlSelf);
#endif  // VL_DEBUG
void Vradar_dechirp___024root___eval_static(Vradar_dechirp___024root* vlSelf);
void Vradar_dechirp___024root___eval_initial(Vradar_dechirp___024root* vlSelf);
void Vradar_dechirp___024root___eval_settle(Vradar_dechirp___024root* vlSelf);
void Vradar_dechirp___024root___eval(Vradar_dechirp___024root* vlSelf);

void Vradar_dechirp::eval_step() {
    VL_DEBUG_IF(VL_DBG_MSGF("+++++TOP Evaluate Vradar_dechirp::eval_step\n"); );
#ifdef VL_DEBUG
    // Debug assertions
    Vradar_dechirp___024root___eval_debug_assertions(&(vlSymsp->TOP));
#endif  // VL_DEBUG
    vlSymsp->__Vm_deleter.deleteAll();
    if (VL_UNLIKELY(!vlSymsp->__Vm_didInit)) {
        VL_DEBUG_IF(VL_DBG_MSGF("+ Initial\n"););
        Vradar_dechirp___024root___eval_static(&(vlSymsp->TOP));
        Vradar_dechirp___024root___eval_initial(&(vlSymsp->TOP));
        Vradar_dechirp___024root___eval_settle(&(vlSymsp->TOP));
        vlSymsp->__Vm_didInit = true;
    }
    VL_DEBUG_IF(VL_DBG_MSGF("+ Eval\n"););
    Vradar_dechirp___024root___eval(&(vlSymsp->TOP));
    // Evaluate cleanup
    Verilated::endOfEval(vlSymsp->__Vm_evalMsgQp);
}

//============================================================
// Events and timing
bool Vradar_dechirp::eventsPending() { return false; }

uint64_t Vradar_dechirp::nextTimeSlot() {
    VL_FATAL_MT(__FILE__, __LINE__, "", "No delays in the design");
    return 0;
}

//============================================================
// Utilities

const char* Vradar_dechirp::name() const {
    return vlSymsp->name();
}

//============================================================
// Invoke final blocks

void Vradar_dechirp___024root___eval_final(Vradar_dechirp___024root* vlSelf);

VL_ATTR_COLD void Vradar_dechirp::final() {
    contextp()->executingFinal(true);
    Vradar_dechirp___024root___eval_final(&(vlSymsp->TOP));
    contextp()->executingFinal(false);
}

//============================================================
// Implementations of abstract methods from VerilatedModel

const char* Vradar_dechirp::hierName() const { return vlSymsp->name(); }
const char* Vradar_dechirp::modelName() const { return "Vradar_dechirp"; }
unsigned Vradar_dechirp::threads() const { return 1; }
void Vradar_dechirp::prepareClone() const { contextp()->prepareClone(); }
void Vradar_dechirp::atClone() const {
    contextp()->threadPoolpOnClone();
}
