// Verilated -*- C++ -*-
// DESCRIPTION: Verilator output: Design implementation internals
// See Vradar_seq.h for the primary calling header

#include "Vradar_seq__pch.h"

void Vradar_seq___024root___ctor_var_reset(Vradar_seq___024root* vlSelf);

Vradar_seq___024root::Vradar_seq___024root(Vradar_seq__Syms* symsp, const char* namep)
 {
    vlSymsp = symsp;
    vlNamep = strdup(namep);
    // Reset structure values
    Vradar_seq___024root___ctor_var_reset(this);
}

void Vradar_seq___024root::__Vconfigure(bool first) {
    (void)first;  // Prevent unused variable warning
}

Vradar_seq___024root::~Vradar_seq___024root() {
    VL_DO_DANGLING(std::free(const_cast<char*>(vlNamep)), vlNamep);
}
