// Verilated -*- C++ -*-
// DESCRIPTION: Verilator output: Design implementation internals
// See Vradar_dechirp.h for the primary calling header

#include "Vradar_dechirp__pch.h"

void Vradar_dechirp___024root___ctor_var_reset(Vradar_dechirp___024root* vlSelf);

Vradar_dechirp___024root::Vradar_dechirp___024root(Vradar_dechirp__Syms* symsp, const char* namep)
 {
    vlSymsp = symsp;
    vlNamep = strdup(namep);
    // Reset structure values
    Vradar_dechirp___024root___ctor_var_reset(this);
}

void Vradar_dechirp___024root::__Vconfigure(bool first) {
    (void)first;  // Prevent unused variable warning
}

Vradar_dechirp___024root::~Vradar_dechirp___024root() {
    VL_DO_DANGLING(std::free(const_cast<char*>(vlNamep)), vlNamep);
}
