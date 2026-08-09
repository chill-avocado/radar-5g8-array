// Verilated -*- C++ -*-
// DESCRIPTION: Verilator output: Design implementation internals
// See Vradar_regs.h for the primary calling header

#include "Vradar_regs__pch.h"

void Vradar_regs___024root___ctor_var_reset(Vradar_regs___024root* vlSelf);

Vradar_regs___024root::Vradar_regs___024root(Vradar_regs__Syms* symsp, const char* namep)
 {
    vlSymsp = symsp;
    vlNamep = strdup(namep);
    // Reset structure values
    Vradar_regs___024root___ctor_var_reset(this);
}

void Vradar_regs___024root::__Vconfigure(bool first) {
    (void)first;  // Prevent unused variable warning
}

Vradar_regs___024root::~Vradar_regs___024root() {
    VL_DO_DANGLING(std::free(const_cast<char*>(vlNamep)), vlNamep);
}
