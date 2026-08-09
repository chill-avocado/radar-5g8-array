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
/// and ignored otherwise.  Taylor additionally uses nbar = 5.
std::vector<float> make_window(WindowKind k, int n, double param = 0.0);

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
