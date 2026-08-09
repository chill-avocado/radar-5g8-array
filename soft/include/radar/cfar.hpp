//============================================================================
// cfar.hpp -- constant-false-alarm-rate detection on the range-Doppler map
//
// The FPGA runs a cell-averaging CFAR in hardware because that is what fits in
// a Kintex-7 at line rate.  This is the host's version of the same test.  It
// exists for three reasons: it is the detector used in simulation and in
// replay, where there is no FPGA in the loop; it is the bit-level reference the
// RTL is checked against; and it supports the variants the hardware cannot
// afford, in particular the ordered-statistic test, which is the one that
// survives a scene where two targets sit inside each other's training window.
//
// The threshold is set from the desired false-alarm probability rather than
// from a signal-to-noise figure.  That is the whole point of CFAR: the noise
// level drifts with temperature, with receiver gain and with how much clutter
// is in the beam, and a fixed threshold either drowns in false alarms or goes
// deaf.  Estimating the noise from the cells around the one under test makes
// the false-alarm rate independent of the level.
//============================================================================
#pragma once

// core.hpp uses std::memset in a member that this header instantiates, so the
// declaration has to be in scope before it.
#include <cstring>

#include "radar/config.hpp"
#include "radar/core.hpp"
#include "radar/types.hpp"

#include <atomic>
#include <vector>

namespace radar {

class Cfar2D {
public:
    explicit Cfar2D(const Config& cfg);

    /// Test every cell of `in.power` and append the survivors to `out`, sorted
    /// by descending power and truncated to Config::max_hits.
    ///
    /// Thread-safe: a single Cfar2D may be used by several threads on
    /// different frames at once.  All working storage is per-thread.
    void detect(const RdFrame& in, std::vector<Hit>& out) const;

    /// Threshold multiplier for a cell with the full, unclipped training
    /// window.  Cells near the first and last range bins have fewer training
    /// cells and get their own multiplier, computed the same way.
    double alpha() const { return alpha_nominal_; }

    /// Mean training level over the cells tested by the most recent detect()
    /// on any thread, in the same linear power units as RdFrame::power.
    double noise_floor() const { return noise_floor_.load(std::memory_order_relaxed); }

    //-- introspection, used by the self-test and the status page -------------
    CfarKind kind()            const { return kind_; }
    int      n_ref_nominal()   const { return n_ref_nominal_; }
    int      os_rank_nominal() const { return os_rank_nominal_; }
    /// The false-alarm probability the nominal multiplier actually realises.
    /// Equal to Config::pfa to within the bisection tolerance; a way to see
    /// that the inverse solved rather than a way to change anything.
    double   realised_pfa()    const { return realised_pfa_; }

private:
    struct Scratch;
    Scratch& scratch(int nr, int nd) const;

    CfarKind kind_;
    int      guard_r_, guard_d_, train_r_, train_d_;
    int      halo_r_, halo_d_;      ///< guard + training, each axis
    int      max_hits_;
    int      zero_dopp_blank_, range_zero_bin_;
    double   pfa_;

    int    n_ref_nominal_   = 0;
    int    os_rank_nominal_ = 0;
    double alpha_nominal_   = 0;
    double realised_pfa_    = 0;

    mutable std::atomic<double> noise_floor_{0.0};
};

//----------------------------------------------------------------------------
// Threshold multipliers.  Exposed because the self-test checks them against a
// Monte-Carlo run and the RTL check compares them with the hardware's table.
//
// In every case the clutter is modelled as exponentially distributed power,
// which is what the squared magnitude of complex Gaussian noise is, and the
// multiplier is the number that makes the probability of a noise-only cell
// exceeding the threshold equal to `pfa`.
//----------------------------------------------------------------------------

/// Cell averaging over `n` reference cells.  Closed form, no iteration.
double cfar_alpha_ca(double pfa, int n);

/// Greatest-of: two independent half-windows of `m` cells each, threshold set
/// from the larger of the two means.  Solved numerically from the exact
/// expression; there is no closed form.
double cfar_alpha_go(double pfa, int m);

/// Smallest-of, same construction with the smaller mean.
double cfar_alpha_so(double pfa, int m);

/// Ordered statistic: threshold set from the k-th smallest of n reference
/// cells (k is one-based).  Exact, solved numerically.
double cfar_alpha_os(double pfa, int n, int k);

/// Forward direction -- the false-alarm probability a multiplier produces.
/// Used to report what the detector is really doing rather than what was asked
/// for, and by the self-test.
double cfar_pfa_ca(double alpha, int n);
double cfar_pfa_go(double alpha, int m);
double cfar_pfa_so(double alpha, int m);
double cfar_pfa_os(double alpha, int n, int k);

} // namespace radar
