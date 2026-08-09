//============================================================================
// cluster.cpp -- see cluster.hpp
//============================================================================
#include "radar/cluster.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace radar {

namespace {

/// Reserved grid coordinate for "this detection has no angle".  Well outside
/// anything a real scaled coordinate reaches, so it can never collide with a
/// genuine cell.
constexpr int kNoAngle = -32768;

inline u64 pack4(int a, int b, int c, int d) {
    auto q = [](int v) { return u64(u16(i16(clampv(v, -32768, 32767)))); };
    return q(a) | (q(b) << 16) | (q(c) << 32) | (q(d) << 48);
}
inline u64 pack2(int a, int b) { return pack4(a, b, kNoAngle, kNoAngle); }

/// A bucketed index built by sorting rather than hashing.
///
/// Sorting into a flat array and binary-searching it keeps every allocation in
/// one place and lets the whole structure be reused frame after frame without
/// touching the allocator, which a hash map of vectors cannot do.  For a few
/// hundred detections it is also simply faster.
struct CellIndex {
    std::vector<u64> keys;     ///< unique cell keys, ascending
    std::vector<int> starts;   ///< offset into `items` per unique key, plus end
    std::vector<int> items;    ///< point indices, grouped by cell

    std::vector<std::pair<u64, int>> tmp;

    void build(const std::vector<u64>& key_of, const std::vector<int>& subset) {
        tmp.clear();
        tmp.reserve(subset.size());
        for (int i : subset) tmp.emplace_back(key_of[std::size_t(i)], i);
        std::sort(tmp.begin(), tmp.end(),
                  [](const std::pair<u64, int>& a, const std::pair<u64, int>& b) {
                      return a.first < b.first;
                  });
        keys.clear(); starts.clear(); items.clear();
        keys.reserve(tmp.size()); starts.reserve(tmp.size() + 1); items.reserve(tmp.size());
        for (std::size_t i = 0; i < tmp.size(); ++i) {
            if (i == 0 || tmp[i].first != tmp[i - 1].first) {
                keys.push_back(tmp[i].first);
                starts.push_back(int(i));
            }
            items.push_back(tmp[i].second);
        }
        starts.push_back(int(tmp.size()));
    }

    /// Half-open range of `items` belonging to one cell, or an empty range.
    inline void find(u64 key, const int*& first, const int*& last) const {
        const auto it = std::lower_bound(keys.begin(), keys.end(), key);
        if (it == keys.end() || *it != key) { first = last = nullptr; return; }
        const std::size_t k = std::size_t(it - keys.begin());
        first = items.data() + starts[k];
        last  = items.data() + starts[k + 1];
    }
};

} // namespace

//============================================================================
struct Clusterer::Work {
    std::vector<double> s0, s1, s2, s3;    ///< scaled coordinates
    std::vector<char>   has_angle;
    std::vector<int>    c0, c1, c2, c3;    ///< grid cell per axis
    std::vector<u64>    key4, key2;
    std::vector<int>    subset_valid, subset_invalid, subset_all;
    CellIndex           idx4, idx_invalid, idx_all;

    std::vector<int>  labels, neigh, neigh2, seeds;
    std::vector<char> visited, in_seed;

    // Accumulators for turning clusters into targets, kept here so a steady
    // stream of frames of similar size never reaches the allocator.
    std::vector<double> wsum, wang, snr_lin, rmin, rmax, vmin, vmax;
    std::vector<int>    order, remap;
    std::vector<Target> sorted;
};

namespace {
/// One working set per thread, so one Clusterer can serve several worker
/// threads on different frames without a lock and without allocating.
Clusterer::Work& clusterer_tls() {
    static thread_local Clusterer::Work w;
    return w;
}
} // namespace

Clusterer::Work& Clusterer::work(std::size_t n) const {
    Work& w = clusterer_tls();
    w.s0.resize(n); w.s1.resize(n); w.s2.resize(n); w.s3.resize(n);
    w.has_angle.resize(n);
    w.c0.resize(n); w.c1.resize(n); w.c2.resize(n); w.c3.resize(n);
    w.key4.resize(n); w.key2.resize(n);
    w.labels.assign(n, -1);
    w.visited.assign(n, 0);
    w.in_seed.assign(n, 0);
    w.neigh.reserve(n); w.neigh2.reserve(n); w.seeds.reserve(n);
    return w;
}

const std::vector<int>& Clusterer::last_labels() const { return clusterer_tls().labels; }

//============================================================================
Clusterer::Clusterer(const Config& cfg)
    : eps_m_(cfg.cluster_eps_m   > 0 ? cfg.cluster_eps_m   : 1.0),
      eps_ms_(cfg.cluster_eps_ms > 0 ? cfg.cluster_eps_ms  : 1.0),
      eps_deg_(cfg.cluster_eps_deg > 0 ? cfg.cluster_eps_deg : 1.0),
      min_pts_(std::max(1, cfg.cluster_min_pts)) {}

//============================================================================
void Clusterer::cluster(const std::vector<Hit>& hits, std::vector<Target>& out) const {
    out.clear();
    const std::size_t n = hits.size();
    Work& w = work(n);
    if (n == 0) return;

    //-- Scale ---------------------------------------------------------------
    // Dividing each axis by its own tolerance turns four quantities in four
    // different units into one Euclidean distance, and the neighbourhood
    // radius becomes exactly one.  Everything after this point is unitless.
    for (std::size_t i = 0; i < n; ++i) {
        const Hit& h = hits[i];
        w.s0[i] = h.range_m     / eps_m_;
        w.s1[i] = h.velocity_ms / eps_ms_;
        w.s2[i] = h.azimuth_deg / eps_deg_;
        w.s3[i] = h.elevation_deg / eps_deg_;
        w.has_angle[i] = h.angle_valid ? 1 : 0;
        w.c0[i] = int(std::floor(w.s0[i]));
        w.c1[i] = int(std::floor(w.s1[i]));
        w.c2[i] = h.angle_valid ? int(std::floor(w.s2[i])) : kNoAngle;
        w.c3[i] = h.angle_valid ? int(std::floor(w.s3[i])) : kNoAngle;
        w.key4[i] = pack4(w.c0[i], w.c1[i], w.c2[i], w.c3[i]);
        w.key2[i] = pack2(w.c0[i], w.c1[i]);
    }

    w.subset_valid.clear(); w.subset_invalid.clear(); w.subset_all.clear();
    for (int i = 0; i < int(n); ++i) {
        w.subset_all.push_back(i);
        if (w.has_angle[std::size_t(i)]) w.subset_valid.push_back(i);
        else                             w.subset_invalid.push_back(i);
    }
    // Three indices, because a detection with no angle has to be treated as
    // matching every angle.  Ones that do have an angle look each other up in
    // the full four-dimensional grid and pick up the angle-less ones from a
    // separate two-dimensional grid; ones that do not have an angle look
    // everything up in range and velocity alone.  Without this a hit whose
    // angle failed would silently fall out of every cluster.
    w.idx4.build(w.key4, w.subset_valid);
    w.idx_invalid.build(w.key2, w.subset_invalid);
    w.idx_all.build(w.key2, w.subset_all);

    auto dist2_full = [&](int a, int b) {
        const double d0 = w.s0[std::size_t(a)] - w.s0[std::size_t(b)];
        const double d1 = w.s1[std::size_t(a)] - w.s1[std::size_t(b)];
        const double d2 = w.s2[std::size_t(a)] - w.s2[std::size_t(b)];
        const double d3 = w.s3[std::size_t(a)] - w.s3[std::size_t(b)];
        return d0 * d0 + d1 * d1 + d2 * d2 + d3 * d3;
    };
    auto dist2_rv = [&](int a, int b) {
        const double d0 = w.s0[std::size_t(a)] - w.s0[std::size_t(b)];
        const double d1 = w.s1[std::size_t(a)] - w.s1[std::size_t(b)];
        return d0 * d0 + d1 * d1;
    };

    auto region_query = [&](int i, std::vector<int>& res) {
        res.clear();
        const int a0 = w.c0[std::size_t(i)], a1 = w.c1[std::size_t(i)];
        if (w.has_angle[std::size_t(i)]) {
            const int a2 = w.c2[std::size_t(i)], a3 = w.c3[std::size_t(i)];
            for (int x0 = -1; x0 <= 1; ++x0)
              for (int x1 = -1; x1 <= 1; ++x1)
                for (int x2 = -1; x2 <= 1; ++x2)
                  for (int x3 = -1; x3 <= 1; ++x3) {
                      const int* f; const int* l;
                      w.idx4.find(pack4(a0 + x0, a1 + x1, a2 + x2, a3 + x3), f, l);
                      for (; f != l; ++f) if (dist2_full(i, *f) <= 1.0) res.push_back(*f);
                  }
            for (int x0 = -1; x0 <= 1; ++x0)
              for (int x1 = -1; x1 <= 1; ++x1) {
                  const int* f; const int* l;
                  w.idx_invalid.find(pack2(a0 + x0, a1 + x1), f, l);
                  for (; f != l; ++f) if (dist2_rv(i, *f) <= 1.0) res.push_back(*f);
              }
        } else {
            for (int x0 = -1; x0 <= 1; ++x0)
              for (int x1 = -1; x1 <= 1; ++x1) {
                  const int* f; const int* l;
                  w.idx_all.find(pack2(a0 + x0, a1 + x1), f, l);
                  for (; f != l; ++f) if (dist2_rv(i, *f) <= 1.0) res.push_back(*f);
              }
        }
    };

    //-- Density clustering --------------------------------------------------
    int cid = 0;
    for (int i = 0; i < int(n); ++i) {
        if (w.visited[std::size_t(i)]) continue;
        w.visited[std::size_t(i)] = 1;
        region_query(i, w.neigh);
        if (int(w.neigh.size()) < min_pts_) continue;   // stays noise for now

        w.labels[std::size_t(i)] = cid;
        w.seeds.clear();
        std::fill(w.in_seed.begin(), w.in_seed.end(), 0);
        for (int j : w.neigh)
            if (j != i && !w.in_seed[std::size_t(j)]) { w.in_seed[std::size_t(j)] = 1; w.seeds.push_back(j); }

        for (std::size_t s = 0; s < w.seeds.size(); ++s) {
            const int j = w.seeds[s];
            if (!w.visited[std::size_t(j)]) {
                w.visited[std::size_t(j)] = 1;
                region_query(j, w.neigh2);
                // Only a dense point spreads the cluster further.  A point on
                // the rim joins but does not pull anything else in, which is
                // what keeps two objects passing close to each other from
                // merging through a thin bridge of weak detections.
                if (int(w.neigh2.size()) >= min_pts_) {
                    for (int k : w.neigh2)
                        if (!w.in_seed[std::size_t(k)] && w.labels[std::size_t(k)] < 0) {
                            w.in_seed[std::size_t(k)] = 1;
                            w.seeds.push_back(k);
                        }
                }
            }
            if (w.labels[std::size_t(j)] < 0) w.labels[std::size_t(j)] = cid;
        }
        ++cid;
    }
    if (cid == 0) return;

    //-- Condense each cluster into a Target ---------------------------------
    out.resize(std::size_t(cid));
    std::vector<double> wsum(std::size_t(cid), 0.0), wang(std::size_t(cid), 0.0);
    std::vector<double> snr_lin(std::size_t(cid), 0.0);
    std::vector<double> rmin(std::size_t(cid),  std::numeric_limits<double>::infinity());
    std::vector<double> rmax(std::size_t(cid), -std::numeric_limits<double>::infinity());
    std::vector<double> vmin(std::size_t(cid),  std::numeric_limits<double>::infinity());
    std::vector<double> vmax(std::size_t(cid), -std::numeric_limits<double>::infinity());

    for (std::size_t i = 0; i < n; ++i) {
        const int c = w.labels[i];
        if (c < 0) continue;
        const Hit& h = hits[i];
        Target& t = out[std::size_t(c)];
        // Weighting by power rather than counting equally puts the reported
        // position on the body of the object instead of halfway to whichever
        // rotor happened to produce the most detections this frame.
        const double p = std::max(h.power, 0.0);
        t.range_m     += p * h.range_m;
        t.velocity_ms += p * h.velocity_ms;
        wsum[std::size_t(c)] += p;
        if (h.angle_valid) {
            t.azimuth_deg   += p * h.azimuth_deg;
            t.elevation_deg += p * h.elevation_deg;
            wang[std::size_t(c)] += p;
        }
        snr_lin[std::size_t(c)] += undb(h.snr_db);
        rmin[std::size_t(c)] = std::min(rmin[std::size_t(c)], h.range_m);
        rmax[std::size_t(c)] = std::max(rmax[std::size_t(c)], h.range_m);
        vmin[std::size_t(c)] = std::min(vmin[std::size_t(c)], h.velocity_ms);
        vmax[std::size_t(c)] = std::max(vmax[std::size_t(c)], h.velocity_ms);
        ++t.n_hits;
    }

    for (int c = 0; c < cid; ++c) {
        Target& t = out[std::size_t(c)];
        const double ws = wsum[std::size_t(c)];
        if (ws > 0) { t.range_m /= ws; t.velocity_ms /= ws; }
        if (wang[std::size_t(c)] > 0) {
            t.azimuth_deg   /= wang[std::size_t(c)];
            t.elevation_deg /= wang[std::size_t(c)];
        } else {
            // Every detection in this group failed the angle test.  Range and
            // velocity are still good, so the target is kept, but it is
            // reported at boresight and the tracker will find its angle
            // covariance dominated by whatever it already believed.
            t.azimuth_deg = t.elevation_deg = 0.0;
        }
        // Non-coherent integration: several independent looks at one object
        // add in power, so the group is stronger than any single detection.
        t.snr_db = db(snr_lin[std::size_t(c)]);

        const double az = rad(t.azimuth_deg), el = rad(t.elevation_deg);
        t.x = t.range_m * std::cos(el) * std::sin(az);   // right
        t.y = t.range_m * std::cos(el) * std::cos(az);   // boresight
        t.z = t.range_m * std::sin(el);                  // up

        t.extent_m       = (t.n_hits > 0) ? (rmax[std::size_t(c)] - rmin[std::size_t(c)]) : 0.0;
        t.dopp_spread_ms = (t.n_hits > 0) ? (vmax[std::size_t(c)] - vmin[std::size_t(c)]) : 0.0;
    }

    // Strongest first.  The tracker associates greedily, so a deterministic
    // and meaningful order makes its behaviour repeatable frame to frame.
    std::vector<int> order(std::size_t(cid));
    for (int i = 0; i < cid; ++i) order[std::size_t(i)] = i;
    std::sort(order.begin(), order.end(), [&](int a, int b) {
        return out[std::size_t(a)].snr_db > out[std::size_t(b)].snr_db;
    });
    std::vector<Target> sorted(std::size_t(cid));
    std::vector<int>    remap(std::size_t(cid));
    for (int i = 0; i < cid; ++i) {
        sorted[std::size_t(i)] = out[std::size_t(order[std::size_t(i)])];
        remap[std::size_t(order[std::size_t(i)])] = i;
    }
    out.swap(sorted);
    for (std::size_t i = 0; i < n; ++i)
        if (w.labels[i] >= 0) w.labels[i] = remap[std::size_t(w.labels[i])];
}

} // namespace radar
