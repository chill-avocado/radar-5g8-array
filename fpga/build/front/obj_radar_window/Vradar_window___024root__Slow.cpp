// Verilated -*- C++ -*-
// DESCRIPTION: Verilator output: Design implementation internals
// See Vradar_window.h for the primary calling header

#include "Vradar_window__pch.h"

void Vradar_window___024root___ctor_var_reset(Vradar_window___024root* vlSelf);

Vradar_window___024root::Vradar_window___024root(Vradar_window__Syms* symsp, const char* namep)
 {
    vlSymsp = symsp;
    vlNamep = strdup(namep);
    // Reset structure values
    Vradar_window___024root___ctor_var_reset(this);
}

void Vradar_window___024root::__Vconfigure(bool first) {
    (void)first;  // Prevent unused variable warning
}

Vradar_window___024root::~Vradar_window___024root() {
    VL_DO_DANGLING(std::free(const_cast<char*>(vlNamep)), vlNamep);
}
