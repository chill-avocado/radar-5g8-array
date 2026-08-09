//============================================================================
// cluster.hpp -- gathering detections into objects
//
// One drone does not produce one detection.  Its body, its arms and each of
// its rotors reflect separately, and the range and Doppler resolution here are
// fine enough to tell them apart, so a single quadcopter arrives at this stage
// as a handful of detections a metre or two apart with velocities spread by
// several metres per second.  Handing all of them to the tracker would create
// half a dozen tracks on one aircraft and then lose all of them as the
// detections flicker between frames.
//
// The grouping rule is density-based rather than a fixed partition, because
// nobody knows in advance how many objects are in the beam.  Detections that
// are close together in range, velocity and angle at the same time belong to
// one object; a detection with nothing near it is a false alarm and is
// dropped.  "Close" is measured in units of the four tolerances in Config, so
// the four incompatible axes -- metres, metres per second and two angles --
// become one dimensionless distance.
//============================================================================
#pragma once

#include "radar/config.hpp"
#include "radar/core.hpp"
#include "radar/types.hpp"

#include <cstring>
#include <vector>

namespace radar {

class Clusterer {
public:
    explicit Clusterer(const Config& cfg);

    /// Group `hits` and write one Target per group, strongest first.
    /// Thread-safe: all working storage is per-thread.
    void cluster(const std::vector<Hit>& hits, std::vector<Target>& out) const;

    /// Cluster index per input hit from the most recent cluster() **on this
    /// thread**; -1 means the hit was left as noise.  Same order and length as
    /// the hits that were passed in.  The display draws detections coloured by
    /// this so an operator can see what was grouped with what.
    const std::vector<int>& last_labels() const;

private:
    struct Work;
    /// One working set per thread, so one Clusterer can serve several worker
    /// threads on different frames with no lock and no allocation.
    static Work& tls();
    static Work& work(std::size_t n);

    double eps_m_, eps_ms_, eps_deg_;
    int    min_pts_;
};

} // namespace radar
