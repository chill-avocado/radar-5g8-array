// Verilated -*- C++ -*-
// DESCRIPTION: Verilator output: Symbol table internal header
//
// Internal details; most calling programs do not need this header,
// unless using verilator public meta comments.

#ifndef VERILATED_VRADAR_NCO__SYMS_H_
#define VERILATED_VRADAR_NCO__SYMS_H_  // guard

#include "verilated.h"

// INCLUDE MODEL CLASS

#include "Vradar_nco.h"

// INCLUDE MODULE CLASSES
#include "Vradar_nco___024root.h"

// SYMS CLASS (contains all model state)
class alignas(VL_CACHE_LINE_BYTES) Vradar_nco__Syms final : public VerilatedSyms {
  public:
    // INTERNAL STATE
    Vradar_nco* const __Vm_modelp;
    VlDeleter __Vm_deleter;
    bool __Vm_didInit = false;

    // MODULE INSTANCE STATE
    Vradar_nco___024root           TOP;

    // CONSTRUCTORS
    Vradar_nco__Syms(VerilatedContext* contextp, const char* namep, Vradar_nco* modelp);
    ~Vradar_nco__Syms();

    // METHODS
    const char* name() const { return TOP.vlNamep; }
};

#endif  // guard
