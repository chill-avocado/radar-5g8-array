// Verilated -*- C++ -*-
// DESCRIPTION: Verilator output: Design implementation internals
// See Vradar_decim4.h for the primary calling header

#include "Vradar_decim4__pch.h"

void Vradar_decim4___024root___ctor_var_reset(Vradar_decim4___024root* vlSelf);

Vradar_decim4___024root::Vradar_decim4___024root(Vradar_decim4__Syms* symsp, const char* namep)
 {
    vlSymsp = symsp;
    vlNamep = strdup(namep);
    // Reset structure values
    Vradar_decim4___024root___ctor_var_reset(this);
}

void Vradar_decim4___024root::__Vconfigure(bool first) {
    (void)first;  // Prevent unused variable warning
}

Vradar_decim4___024root::~Vradar_decim4___024root() {
    VL_DO_DANGLING(std::free(const_cast<char*>(vlNamep)), vlNamep);
}
