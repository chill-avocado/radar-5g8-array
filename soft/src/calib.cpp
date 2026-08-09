#include "radar/calib.hpp"

#include "radar/json.hpp"
#include "radar/log.hpp"

#include <algorithm>
#include <cmath>

namespace radar {

Calibration::Calibration() {
    for (auto& c : fixed_) c = cf32(1.0f, 0.0f);
}

Calibration Calibration::identity() { return Calibration(); }

//----------------------------------------------------------------------------
void Calibration::apply(std::array<cf32, 4>& v) const {
    for (int i = 0; i < 4; ++i) v[i] *= fixed_[i];
}

void Calibration::apply_angular(std::array<cf32, 4>& v, double az_deg, double el_deg) const {
    apply(v);
    if (points_.size() < 3) return;

    // Inverse-distance weighting over the scattered field points, with the
    // exponent chosen so that a point 5 degrees away dominates one 20 degrees
    // away by roughly 16 to 1.  A polynomial fit was the obvious alternative
    // and is wrong here: the residual jumps across the mirrored-element
    // boundary and a smooth fit would smear that step across the whole field.
    double              wsum = 0.0;
    std::array<cf32, 4> acc{};

    for (const auto& p : points_) {
        const double daz = az_deg - p.az_deg;
        const double del = el_deg - p.el_deg;
        const double d2  = daz * daz + del * del;
        if (d2 < 1e-6) {                       // sitting on a measured point
            for (int i = 0; i < 4; ++i) v[i] *= p.correction[i];
            return;
        }
        const double w = 1.0 / (d2 * d2 * 1e-4 + 1e-9);   // ~ 1/d^4
        wsum += w;
        for (int i = 0; i < 4; ++i) acc[i] += p.correction[i] * float(w);
    }
    if (wsum <= 0.0) return;
    for (int i = 0; i < 4; ++i) v[i] *= acc[i] / float(wsum);
}

//----------------------------------------------------------------------------
void Calibration::solve_boresight(const std::array<cf32, 4>& measured) {
    // A boresight target must produce equal amplitude and equal phase on all
    // four virtual channels.  Normalise to the geometric mean amplitude rather
    // than to channel 0, so one weak channel does not drag the whole set and
    // change the overall scale of every subsequent detection.
    double log_amp = 0.0;
    int    n       = 0;
    for (const auto& m : measured) {
        const double a = std::abs(m);
        if (a > 1e-12) { log_amp += std::log(a); ++n; }
    }
    const double ref_amp = (n > 0) ? std::exp(log_amp / n) : 1.0;

    for (int i = 0; i < 4; ++i) {
        const double a = std::abs(measured[i]);
        if (a < 1e-12) {
            fixed_[i] = cf32(1.0f, 0.0f);
            LOG_W("calibration: virtual channel %d had no signal, left uncorrected", i);
            continue;
        }
        // Conjugate cancels the phase; the amplitude ratio equalises the gain.
        const cf32 unit = measured[i] / float(a);
        fixed_[i]       = std::conj(unit) * float(ref_amp / a);
    }
    solved_ = true;

    LOG_I("calibration: boresight solved, channel gains %.2f %.2f %.2f %.2f dB, "
          "phases %+.1f %+.1f %+.1f %+.1f deg",
          db_amp(std::abs(fixed_[0])), db_amp(std::abs(fixed_[1])),
          db_amp(std::abs(fixed_[2])), db_amp(std::abs(fixed_[3])),
          deg(std::arg(fixed_[0])), deg(std::arg(fixed_[1])),
          deg(std::arg(fixed_[2])), deg(std::arg(fixed_[3])));
}

void Calibration::add_field_point(double az_deg, double el_deg, const std::array<cf32, 4>& measured) {
    // Store what remains AFTER the fixed correction, so the field table only
    // ever carries the angle-dependent part.
    std::array<cf32, 4> v = measured;
    apply(v);

    FieldPoint p;
    p.az_deg = az_deg;
    p.el_deg = el_deg;

    // The expected response of an ideal array at this angle, so the stored
    // correction is a residual and not the steering vector itself.
    const double u = std::sin(rad(az_deg)) * std::cos(rad(el_deg));
    const double w = std::sin(rad(el_deg));
    const double k = 2.0 * kPi / array_geom::lambda_m;

    double log_amp = 0.0;
    for (int i = 0; i < 4; ++i) log_amp += std::log(std::max(1e-12, double(std::abs(v[i]))));
    const double ref_amp = std::exp(log_amp / 4.0);

    for (int i = 0; i < 4; ++i) {
        const double ph  = k * (array_geom::virt_xy[i][0] * u + array_geom::virt_xy[i][1] * w);
        const cf32   ideal(float(std::cos(ph)), float(std::sin(ph)));
        const double a   = std::max(1e-12, double(std::abs(v[i])));
        p.correction[i]  = (ideal / (v[i] / float(a))) * float(ref_amp / a);
    }
    points_.push_back(p);
}

void Calibration::clear_field_points() { points_.clear(); }

bool Calibration::solve_field() {
    if (points_.size() < 3) {
        LOG_W("calibration: %zu field points is not enough, need at least 3", points_.size());
        return false;
    }
    // Nothing to fit: the weighting is evaluated at lookup time. Sorting by
    // azimuth only helps a human reading the saved file.
    std::sort(points_.begin(), points_.end(),
              [](const FieldPoint& a, const FieldPoint& b) {
                  return a.az_deg != b.az_deg ? a.az_deg < b.az_deg : a.el_deg < b.el_deg;
              });
    LOG_I("calibration: field table built from %zu points, azimuth %.0f to %.0f degrees",
          points_.size(), points_.front().az_deg, points_.back().az_deg);
    return true;
}

//----------------------------------------------------------------------------
int Calibration::solve_range_zero(const float* profile, int n_bins, double min_prominence_db) {
    if (!profile || n_bins < 8) return -1;

    // The leakage is the strongest thing at short range and it is the only
    // thing there when nothing is in front of the radar.  Search the first
    // eighth of the profile, which at 2.25 m per bin is the first 72 m -- far
    // enough to find a delayed leakage, short enough not to catch a real
    // target at operational range.
    const int   search = std::max(4, std::min(n_bins / 8, 64));
    int         peak   = 0;
    double      pmax   = -1.0;
    for (int i = 0; i < search; ++i) {
        if (profile[i] > pmax) { pmax = profile[i]; peak = i; }
    }
    if (pmax <= 0.0) return -1;

    // Prominence against the median of the rest of the profile.
    std::vector<float> rest(profile + search, profile + n_bins);
    if (rest.empty()) return -1;
    std::nth_element(rest.begin(), rest.begin() + rest.size() / 2, rest.end());
    const double median = std::max(1e-30f, rest[rest.size() / 2]);
    const double prom   = db(pmax / median);
    if (prom < min_prominence_db) {
        LOG_W("calibration: strongest near peak is only %.1f dB above the profile, "
              "not treating it as transmit leakage (wanted %.1f dB)", prom, min_prominence_db);
        return -1;
    }

    // Quadratic fit through the peak and its neighbours, in dB, which is the
    // right domain because a windowed FFT peak is close to parabolic there.
    double frac = 0.0;
    if (peak > 0 && peak + 1 < n_bins) {
        const double a = db(std::max(1e-30f, profile[peak - 1]));
        const double b = db(std::max(1e-30f, profile[peak]));
        const double c = db(std::max(1e-30f, profile[peak + 1]));
        const double d = a - 2.0 * b + c;
        if (std::abs(d) > 1e-9) frac = clampv(0.5 * (a - c) / d, -0.5, 0.5);
    }

    range_zero_bin_  = peak;
    range_zero_frac_ = frac;
    leak_power_      = pmax;
    LOG_I("calibration: transmit leakage at range bin %d%+.2f, %.1f dB above the profile "
          "-- range origin set", peak, frac, prom);
    return peak;
}

//----------------------------------------------------------------------------
std::string Calibration::check(const RdFrame& f) const {
    if (f.hits.empty()) return {};

    // Amplitude spread across the four virtual channels on the strongest hit.
    const Hit* best = &f.hits[0];
    for (const auto& h : f.hits) if (h.power > best->power) best = &h;

    double lo = 1e30, hi = 0.0;
    for (const auto& c : best->virt) {
        const double a = std::abs(c);
        lo = std::min(lo, a);
        hi = std::max(hi, a);
    }
    if (lo <= 0.0) return "one virtual channel is dead -- check the cable and the connector";

    const double spread_db = db_amp(hi / lo);
    if (spread_db > 12.0) {
        char buf[192];
        std::snprintf(buf, sizeof(buf),
                      "virtual channels differ by %.1f dB on the strongest target -- "
                      "expected under 6 dB after calibration", spread_db);
        return buf;
    }

    if (leak_power_ > 0.0 && f.n_range > range_zero_bin_) {
        double here = 0.0;
        for (int d = 0; d < f.n_doppler; ++d) here = std::max(here, double(f.at(range_zero_bin_, d)));
        const double drift = db(std::max(1e-30, here) / leak_power_);
        if (std::abs(drift) > 6.0) {
            char buf[192];
            std::snprintf(buf, sizeof(buf),
                          "transmit leakage has moved %+.1f dB since calibration -- "
                          "the range origin may no longer be right", drift);
            return buf;
        }
    }
    return {};
}

//----------------------------------------------------------------------------
bool Calibration::load(const std::string& path, std::string* err) {
    const std::string text = read_file(path);
    if (text.empty()) {
        if (err) *err = "could not read " + path;
        return false;
    }
    Json j;
    try {
        j = Json::parse(text);
    } catch (const std::exception& e) {
        if (err) *err = std::string("bad calibration file: ") + e.what();
        return false;
    }

    const Json& fx = j["fixed"];
    if (fx.size() == 4) {
        for (int i = 0; i < 4; ++i) {
            fixed_[i] = cf32(float(fx[std::size_t(i)]["re"].as_double(1.0)),
                             float(fx[std::size_t(i)]["im"].as_double(0.0)));
        }
    }
    range_zero_bin_  = int(j["range_zero_bin"].as_int(0));
    range_zero_frac_ = j["range_zero_frac"].as_double(0.0);
    leak_power_      = j["leak_power"].as_double(0.0);

    points_.clear();
    const Json& pts = j["field_points"];
    for (std::size_t p = 0; p < pts.size(); ++p) {
        FieldPoint fp;
        fp.az_deg = pts[p]["az_deg"].as_double();
        fp.el_deg = pts[p]["el_deg"].as_double();
        const Json& cc = pts[p]["correction"];
        for (int i = 0; i < 4 && i < int(cc.size()); ++i) {
            fp.correction[i] = cf32(float(cc[std::size_t(i)]["re"].as_double(1.0)),
                                    float(cc[std::size_t(i)]["im"].as_double(0.0)));
        }
        points_.push_back(fp);
    }
    solved_ = true;
    LOG_I("calibration: loaded %s (%zu field points, range origin bin %d)",
          path.c_str(), points_.size(), range_zero_bin_);
    return true;
}

bool Calibration::save(const std::string& path, std::string* err) const {
    Json j = Json::object();

    Json fx = Json::array();
    for (int i = 0; i < 4; ++i) {
        Json c = Json::object();
        c.set("re", double(fixed_[i].real()));
        c.set("im", double(fixed_[i].imag()));
        fx.push_back(c);
    }
    j.set("fixed", fx);
    j.set("range_zero_bin", range_zero_bin_);
    j.set("range_zero_frac", range_zero_frac_);
    j.set("leak_power", leak_power_);

    Json pts = Json::array();
    for (const auto& p : points_) {
        Json o  = Json::object();
        o.set("az_deg", p.az_deg);
        o.set("el_deg", p.el_deg);
        Json cc = Json::array();
        for (int i = 0; i < 4; ++i) {
            Json c = Json::object();
            c.set("re", double(p.correction[i].real()));
            c.set("im", double(p.correction[i].imag()));
            cc.push_back(c);
        }
        o.set("correction", cc);
        pts.push_back(o);
    }
    j.set("field_points", pts);
    j.set("note", "generated by radard --calibrate; virtual channel order is (tx*2 + rx)");

    if (!write_file(path, j.dump(2) + "\n")) {
        if (err) *err = "could not write " + path;
        return false;
    }
    return true;
}

} // namespace radar
