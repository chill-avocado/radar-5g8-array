"""One circularly-polarised element with the patch stood on its corner.

Local frame: patch centred on (0, 0), millimetres, +y up.

                                    /\\
        input o====[ ring ]====TrA=/  \\
                      |             \\  \\        the patch, turned 45 degrees
                      |              \\  \\
        51 ohm o======'=========TrB===\\  /
                                       \\/

WHY TURN IT
-----------
A square patch rings two ways at once, and circular polarisation needs both
to answer identically.  Straight ahead they do.  Off to one side they do not,
because each way of ringing throws its energy in a slightly different shape,
and the two shapes only agree on boresight.  Measured on the flat-on version:
the polarisation is 2.6 dB round dead ahead and 6.2 dB by the edge of the
coverage, which costs about 7 dB of the rejection that tells a drone from
rain.

Turn the same square 45 degrees and neither way of ringing points along a
direction the radar measures in -- both sit symmetrically either side of it.
Look off-axis and you are at the same angle to both, so they fall off
together and cannot drift apart.  It is a symmetry, not a tuning.

WHAT IT BUYS BESIDES
--------------------
The two feeds become mirror images of each other about the horizontal centre
line, so their lengths match because the shape says so.  The old layout had a
dogleg in one route and a trim knob to null what was left; both are gone.
Whatever the feed network itself radiates is then shared evenly between the
two modes instead of landing mostly on one.  And the board gets about a fifth
smaller.

HOW IT IS FED
-------------
The two edges facing left are fed, so the whole network sits outboard and the
two patches face each other across clear board.  The matching transformers
run straight in horizontally and meet those edges obliquely -- 55 per cent
along, near enough the middle for the edge impedance.  The coupler ring is
turned a quarter turn from the flat-on version so its two outputs come off
one above the other, level with the two transformers, and feed them directly
with no routing at all between.  Input and the 51 ohm load sit on the ring's
far side, out of the way.
"""

import json
import math
import os

from geom45 import stroke, seg, diamond, mitre_cut

HERE = os.path.dirname(os.path.abspath(__file__))
JOIN = 0.05


class Element:
    """The diamond element.  Mirroring it flips the handedness, nothing else."""

    def __init__(self, cfg, dLx=None, dLy=None, mirror=False, launch_y=None,
                 route=None, route_delta=0.0):
        t = cfg.get("tuned", {})
        self.mirror = mirror
        L = t.get("L", 14.0835)
        dLx = t.get("dLx", 0.0) if dLx is None else dLx
        dLy = t.get("dLy", 0.0) if dLy is None else dLy
        f, h = cfg["feed"], cfg["hybrid"]
        self.hsub = cfg["substrate"]["h_mm"]
        # A run of 50 ohm line between the ring and the transformer.  The
        # flat-on element has 18 mm of it and I had treated that as clutter
        # the diamond was better off without.  A line near half a guided
        # wavelength rotates the patch's impedance as frequency moves, in a
        # direction that partly cancels the patch's own swing -- so those
        # routes may be buying the flat-on element its bandwidth.
        self.route = t.get("route_mm", 0.0) if route is None else route
        # A deliberate difference between the two feeds.  Perfect symmetry
        # gives the two modes equal drive, which is right, but it also
        # leaves no way to make up the coupler's own quadrature shortfall
        # -- that correction is asymmetric by nature.
        self.route_delta = t.get("route_delta_mm", 0.0) if route_delta == 0.0 \
            else route_delta
        self.wq = t.get("wq", 0.3414)
        self.lq = t.get("lq", 8.5422)
        self.wf = f["w50_mm"]
        self.lam_g = f["lam_g50_mm"]
        self.cut = mitre_cut(self.wf, self.hsub)

        # The two ways it rings run along the diamond's two diagonals, so the
        # trims act on the diagonals too.
        self.Lu, self.Lv = L + dLx, L + dLy
        self.L = L
        self.Du = self.Lu * math.sqrt(2.0)          # corner to corner
        self.Dv = self.Lv * math.sqrt(2.0)

        # the ring, turned a quarter turn: its long arms now lie horizontally
        self.a = t["arm_series"]                    # horizontal arms
        self.b = t["arm_shunt"]                     # vertical arms
        self.wah = t.get("w_arm_v", h["w_series_mm"])   # horizontal arm width
        self.wav = t.get("w_arm_h", h["w_shunt_mm"])    # vertical arm width
        self.zq = t.get("zq_ohm", 110.0)
        self._plan()

    # ------------------------------------------------------------------
    def _plan(self):
        hy = self.b / 2.0                   # the ring's two outputs sit here
        # The feed MUST land on the middle of its edge.  That is exactly where
        # the other mode has its null, and it is the only place a feed drives
        # one mode without driving the other.  Running the transformer
        # horizontally put it 0.46 mm off centre -- 54.6 per cent along -- and
        # measured 12 dB axial ratio on a patch whose two modes are identical
        # to four decimals.  A perfect patch, wrecked by where it was fed.
        #
        # So the transformer runs from the ring's output to the edge midpoint
        # instead, sloping about three degrees.  Electrically that is nothing;
        # geometrically it is the whole difference.
        self.edge_x = -self.Du / 4.0
        self.edge_y = self.Dv / 4.0
        dy = self.edge_y - hy
        self.xT = self.edge_x - math.sqrt(max(self.lq ** 2 - dy ** 2, 0.01))
        self.xR = self.xT - self.route      # ring sits back by the route
        self.feed_y = hy
        self.xr = self.xR                       # ring's right edge
        self.xl = self.xr - self.a
        # Ports are settled here, once.  build() used to flip them, which
        # meant calling it twice flipped them back.
        sx = -1.0 if self.mirror else 1.0
        self.input_pt = (sx * self.xl, +hy)
        self.iso_pt = (sx * self.xl, -hy)
        # how far along the edge the feed lands, from the top vertex;
        # 0.5 is dead centre and that is where the edge impedance is right
        self.edge_frac = 0.5          # dead centre, by construction

    # ------------------------------------------------------------------
    def build(self):
        hy, wq, w = self.feed_y, self.wq, self.wf
        polys = [diamond(0.0, 0.0, self.Lu)]                     # the patch
        for s in (+1, -1):
            polys.append(seg((self.xT - JOIN, s * hy),
                             (self.edge_x + JOIN * 0.7,
                              s * (self.edge_y - JOIN * 0.7)), wq))
            if self.route > 0.01:
                extra = self.route_delta if s > 0 else 0.0
                polys.append(seg((self.xR - JOIN - extra, s * hy),
                                 (self.xT + JOIN, s * hy), self.wf))
        # the ring: two horizontal arms, two vertical
        polys.append(seg((self.xl, +hy), (self.xr, +hy), self.wah))
        polys.append(seg((self.xl, -hy), (self.xr, -hy), self.wah))
        polys.append(seg((self.xl, -hy), (self.xl, +hy), self.wav))
        polys.append(seg((self.xr, -hy), (self.xr, +hy), self.wav))
        if self.mirror:
            polys = [[(-x, y) for (x, y) in p][::-1] for p in polys]
        return polys

    def bbox(self):
        P = self.build()
        xs = [q[0] for p in P for q in p]
        ys = [q[1] for p in P for q in p]
        return (min(xs), min(ys), max(xs), max(ys))

    def report(self):
        bb = self.bbox()
        return dict(topology="diamond",
                    patch_side_mm=round(self.Lu, 4),
                    patch_across_mm=round(self.Du, 4),
                    feed_height_mm=round(self.feed_y, 4),
                    feed_along_edge=round(self.edge_frac, 4),
                    transformer_w_mm=round(self.wq, 4),
                    transformer_l_mm=round(self.lq, 4),
                    ring_mm=[round(self.a, 4), round(self.b, 4)],
                    extent_mm=[round(bb[2] - bb[0], 3), round(bb[3] - bb[1], 3)],
                    input_pt_mm=[round(v, 3) for v in self.input_pt],
                    iso_pt_mm=[round(v, 3) for v in self.iso_pt])


if __name__ == "__main__":
    cfg = json.load(open(os.path.join(HERE, "synthesis.json")))["ZYF300CA"]
    for m in (False, True):
        e = Element(cfg, mirror=m)
        r = e.report()
        print(f"  mirror={str(m):5}  " +
              "  ".join(f"{k}={v}" for k, v in r.items() if k in
                        ("patch_across_mm", "extent_mm", "feed_along_edge",
                         "input_pt_mm", "iso_pt_mm")))
