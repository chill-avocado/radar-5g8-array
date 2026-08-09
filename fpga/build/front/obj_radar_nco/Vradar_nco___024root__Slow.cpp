// Verilated -*- C++ -*-
// DESCRIPTION: Verilator output: Design implementation internals
// See Vradar_nco.h for the primary calling header

#include "Vradar_nco__pch.h"

void Vradar_nco___024root___ctor_var_reset(Vradar_nco___024root* vlSelf);

Vradar_nco___024root::Vradar_nco___024root(Vradar_nco__Syms* symsp, const char* namep)
 {
    vlSymsp = symsp;
    vlNamep = strdup(namep);
    // Reset structure values
    Vradar_nco___024root___ctor_var_reset(this);
}

void Vradar_nco___024root::__Vconfigure(bool first) {
    (void)first;  // Prevent unused variable warning
}

Vradar_nco___024root::~Vradar_nco___024root() {
    VL_DO_DANGLING(std::free(const_cast<char*>(vlNamep)), vlNamep);
}
