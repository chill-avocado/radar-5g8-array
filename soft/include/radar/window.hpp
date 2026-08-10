//============================================================================
// window.hpp -- transform windows
//
// Sidelobe control is not cosmetic here.  The transmit leakage sits at zero
// range roughly 40 dB above the noise, and ground clutter sits at zero
// Doppler far above that.  A window whose sidelobes are not below the
// smallest target hides real detections underneath a large one, so the
// default is Blackman-Harris at -92 dB rather than anything narrower.
//============================================================================
#pragma once

#include "radar/config.hpp"
#include "radar/core.hpp"
#include <vector>

namespace radar {

/// `param` is the sidelobe level in dB (positive) for Taylor and Chebyshev,
/// and ignored otherwise.  Taylor additionally needs nbar, the number of
/// sidelobes held down at the design level, and make_window() picks it from
/// the sidelobe level -- see make_taylor_window() below for why it cannot be a
/// fixed number.
std::vector<float> make_window(WindowKind k, int n, double param = 0.0);

/// Taylor with nbar named explicitly, for a caller who wants to override the
/// choice make_window() makes.
///
/// nbar cannot be fixed.  A Taylor window only holds its design sidelobe level
/// out to the nbar-th sidelobe; past that the pattern falls away like a
/// rectangular window's, from wherever the nbar-th one sits.  Ask for a deeper
/// sidelobe than the transition can reach and the far lobes simply stay where
/// they were, so the window quietly misses its target -- 18 dB short at 80 dB
/// with nbar = 5.  The requirement is nbar >= 2*A^2 + 0.5, where
/// A = acosh(10^(sidelobe/20)) / pi, which is 8 at 45 dB and 21 at 80 dB.
std::vector<float> make_taylor_window(int n, double sidelobe_db, int nbar);

/// The smallest nbar that can actually hold `sidelobe_db`, never below 4.
int taylor_nbar_for(double sidelobe_db);

/// Amplitude gain applied to a coherent tone: mean of the coefficients.
double window_coherent_gain(const std::vector<float>& w);

/// Equivalent noise bandwidth, in bins.  Noise power out = enbw * noise in.
double window_enbw(const std::vector<float>& w);

/// Worst-case loss when a tone lands between bins, in dB.
double window_scallop_loss_db(const std::vector<float>& w);

/// Peak sidelobe level of the window's own transform, in dB below the peak.
double window_peak_sidelobe_db(const std::vector<float>& w);

/// Quantise to the s16 Q0.15 table the FPGA holds, using the same rounding.
std::vector<i16> quantise_window(const std::vector<float>& w);

const char* window_name(WindowKind k);

} // namespace radar
