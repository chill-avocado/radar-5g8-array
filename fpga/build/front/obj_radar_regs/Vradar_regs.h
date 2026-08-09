// Verilated -*- C++ -*-
// DESCRIPTION: Verilator output: Primary model header
//
// This header should be included by all source files instantiating the design.
// The class here is then constructed to instantiate the design.
// See the Verilator manual for examples.

#ifndef VERILATED_VRADAR_REGS_H_
#define VERILATED_VRADAR_REGS_H_  // guard

#include "verilated.h"

class Vradar_regs__Syms;
class Vradar_regs___024root;

// This class is the main interface to the Verilated model
class alignas(VL_CACHE_LINE_BYTES) Vradar_regs VL_NOT_FINAL : public VerilatedModel {
  private:
    // Symbol table holding complete model state (owned by this class)
    Vradar_regs__Syms* const vlSymsp;

  public:

    // CONSTEXPR CAPABILITIES
    // Verilated with --trace?
    static constexpr bool traceCapable = false;

    // PORTS
    // The application code writes and reads these signals to
    // propagate new values into/out from the Verilated model.
    VL_IN8(&clk,0,0);
    VL_IN8(&rst,0,0);
    VL_IN8(&set_stb,0,0);
    VL_IN8(&set_addr,7,0);
    VL_OUT8(&ctrl_enable,0,0);
    VL_OUT8(&ctrl_soft_reset,0,0);
    VL_OUT8(&ctrl_mimo_mode,1,0);
    VL_OUT8(&ctrl_tx_enable,0,0);
    VL_OUT8(&ctrl_map_enable,0,0);
    VL_OUT8(&ctrl_hits_enable,0,0);
    VL_OUT8(&ctrl_loopback,0,0);
    VL_OUT8(&dechirp_sh,3,0);
    VL_OUT8(&win_we,0,0);
    VL_OUT8(&cfar_guard_range,3,0);
    VL_OUT8(&cfar_guard_dopp,3,0);
    VL_OUT8(&cfar_train_range,3,0);
    VL_OUT8(&cfar_train_dopp,3,0);
    VL_OUT8(&cfar_kind,1,0);
    VL_OUT8(&map_decim_r,7,0);
    VL_OUT8(&map_decim_d,7,0);
    VL_OUT8(&zero_dopp,7,0);
    VL_OUT8(&geom_n_range_log2,3,0);
    VL_OUT8(&geom_n_chirp_log2,3,0);
    VL_OUT8(&version_stb,0,0);
    VL_OUT16(&ctrl_frame_limit,15,0);
    VL_OUT16(&t_sweep,15,0);
    VL_OUT16(&t_pri,15,0);
    VL_OUT16(&n_chirp,15,0);
    VL_OUT16(&tx_gain,15,0);
    VL_OUT16(&fft_scale_d,15,0);
    VL_OUT16(&win_addr,15,0);
    VL_OUT16(&range_zero,15,0);
    VL_OUT16(&max_hits,15,0);
    VL_IN(&set_data,31,0);
    VL_OUT(&freq_start,31,0);
    VL_OUT(&freq_slope,31,0);
    VL_OUT(&fft_scale_r,19,0);
    VL_OUT(&win_data,31,0);
    VL_OUT(&cfar_alpha,31,0);
    VL_OUT(&test_tone,31,0);

    // CELLS
    // Public to allow access to /* verilator public */ items.
    // Otherwise the application code can consider these internals.

    // Root instance pointer to allow access to model internals,
    // including inlined /* verilator public_flat_* */ items.
    Vradar_regs___024root* const rootp;

    // CONSTRUCTORS
    /// Construct the model; called by application code
    /// If contextp is null, then the model will use the default global context
    /// If name is "", then makes a wrapper with a
    /// single model invisible with respect to DPI scope names.
    explicit Vradar_regs(VerilatedContext* contextp, const char* name = "TOP");
    explicit Vradar_regs(const char* name = "TOP");
    /// Destroy the model; called (often implicitly) by application code
    virtual ~Vradar_regs();
  private:
    VL_UNCOPYABLE(Vradar_regs);  ///< Copying not allowed

  public:
    // API METHODS
    /// Evaluate the model.  Application must call when inputs change.
    void eval() { eval_step(); }
    /// Evaluate when calling multiple units/models per time step.
    void eval_step();
    /// Evaluate at end of a timestep for tracing, when using eval_step().
    /// Application must call after all eval() and before time changes.
    void eval_end_step() {}
    /// Simulation complete, run final blocks.  Application must call on completion.
    void final();
    /// Are there scheduled events to handle?
    bool eventsPending();
    /// Returns time at next time slot. Aborts if !eventsPending()
    uint64_t nextTimeSlot();
    /// Trace signals in the model; called by application code
    void trace(VerilatedTraceBaseC* tfp, int levels, int options = 0) { contextp()->trace(tfp, levels, options); }
    /// Retrieve name of this model instance (as passed to constructor).
    const char* name() const;

    // Abstract methods from VerilatedModel
    const char* hierName() const override final;
    const char* modelName() const override final;
    unsigned threads() const override final;
    /// Prepare for cloning the model at the process level (e.g. fork in Linux)
    /// Release necessary resources. Called before cloning.
    void prepareClone() const;
    /// Re-init after cloning the model at the process level (e.g. fork in Linux)
    /// Re-allocate necessary resources. Called after cloning.
    void atClone() const;
  private:
    // Internal functions - trace registration
    void traceBaseModel(VerilatedTraceBaseC* tfp, int levels, int options);
};

#endif  // guard
