"""One circularly-polarised patch element (final topology).

Local frame: patch centred on (0, 0), millimetres, +y up.

                +-----------+
                |           |
   TrA =========|   PATCH   |          TrA, TrB: quarter-wave transformers
   (110 ohm)    |           |                    that match the patch's
                +-----||----+                    311 ohm edge to 50 ohm
       |               ||
       |              TrB
       |               ||
    TL o=====[ 50 ohm shunt arm, b ]=====o TR
       |                                 |
    series a                         series a          <- 35.4 ohm arms
       |                                 |
    BL o=====[ 50 ohm shunt arm, b ]=====o BR
       |                                 |
     input                          50 ohm 0402

Hybrid port map, solved numerically for THIS ring orientation (horizontal
arms 50 ohm, vertical arms 35.4 ohm): driving BL puts -90 deg on TL, -180 deg
on TR, and isolates BR.  The y-edge feed therefore lags the x-edge feed by
90 degrees, which is RHCP.  Mirroring the whole element gives LHCP with every
electrical length untouched.

Why an edge feed and a transformer rather than an inset:  measured in openEMS,
a 2.7 mm inset drops the isolation between the two patch feeds from -37.6 dB
to about -4.5 dB at resonance, because the notch in the -y edge sits exactly
on the x-mode's current maximum.  That cross-coupling would wreck the circular
polarisation, so the patch is left whole and the matching is done in the line.
"""

import json
import math
import os

from geom import rect, hseg, vseg

HERE = os.path.dirname(os.path.abspath(__file__))
JOIN = 0.05   # polygon overlap at an impedance step, mm


def mitre_cut(w, h):
    """Chamfer leg for an optimum-mitred right-angle bend (Douville & James)."""
    m = 52.0 + 65.0 * math.exp(-1.35 * w / h)
    return min(m / 100.0 * w * math.sqrt(2.0), 0.98 * w)


def stroke(points, w, cut):
    """Orthogonal polyline -> ONE polygon with mitred outer corners."""
    def off(p, q, s):
        dx, dy = q[0] - p[0], q[1] - p[1]
        n = math.hypot(dx, dy)
        nx, ny = -dy / n, dx / n
        return ((p[0] + nx * s, p[1] + ny * s), (q[0] + nx * s, q[1] + ny * s))

    def inter(a, b):
        (x1, y1), (x2, _) = a
        (x3, y3), (x4, _) = b
        if abs(x2 - x1) < 1e-12:
            return (x1, y3)
        return (x3, y1)

    half = w / 2.0
    sides = {}
    for sgn, key in ((+1, "L"), (-1, "R")):
        segs = [off(points[i], points[i + 1], sgn * half)
                for i in range(len(points) - 1)]
        chain = [segs[0][0]]
        for i in range(len(segs) - 1):
            p = inter(segs[i], segs[i + 1])
            ax = points[i + 1][0] - points[i][0]
            ay = points[i + 1][1] - points[i][1]
            bx = points[i + 2][0] - points[i + 1][0]
            by = points[i + 2][1] - points[i + 1][1]
            crossz = ax * by - ay * bx
            outer = (crossz < 0) if sgn > 0 else (crossz > 0)
            if outer:
                na, nb = math.hypot(ax, ay), math.hypot(bx, by)
                chain.append((p[0] - ax / na * cut, p[1] - ay / na * cut))
                chain.append((p[0] + bx / nb * cut, p[1] + by / nb * cut))
            else:
                chain.append(p)
        chain.append(segs[-1][1])
        sides[key] = chain
    return sides["L"] + sides["R"][::-1]


class Element:
    """Parametric CP element.  `dip_trim` is the EM tuning knob for axial ratio."""

    def __init__(self, cfg, L=12.830, wq=0.2626, lq=7.8608, pre=1.00,
                 yt=-16.00, dip_x=-3.50, stub=1.40, dip_trim=None,
                 trim_series=None, trim_shunt=None,
                 dLx=None, dLy=None, mirror=False, dip_trim_mirror=None):
        """dLx / dLy trim the patch's two dimensions independently.

        The patch would be square if it sat on its own, but the feed network
        is all on one side, so it loads the x mode and the y mode by slightly
        different amounts and splits their resonances.  A few tenths of a
        millimetre of deliberate rectangularity puts both modes back on
        5.8 GHz, which is what the axial ratio actually depends on.
        """
        t = cfg.get("tuned", {})
        self.mirror = mirror
        L = t.get("L", L)
        wq = t.get("wq", wq)
        lq = t.get("lq", lq)
        # NB the default is None so the tuned value is actually reached;
        # with a numeric default this line silently ignored the config.
        dip_trim = t.get("dip_trim", 0.3659) if dip_trim is None else dip_trim
        dLx = t.get("dLx", 0.0) if dLx is None else dLx
        dLy = t.get("dLy", 0.0) if dLy is None else dLy
        f, h = cfg["feed"], cfg["hybrid"]
        self.hsub = cfg["substrate"]["h_mm"]
        self.L, self.wq, self.lq, self.pre = L, wq, lq, pre
        self.wf = f["w50_mm"]
        self.ws = h["w_series_mm"]
        tm = t.get("mirror_arms", {}) if mirror else {}
        self.a = (tm.get("arm_series", t["arm_series"]) if trim_series is None
                  else h["l_series_mm"] + trim_series)   # vertical arms
        self.b = (tm.get("arm_shunt", t["arm_shunt"]) if trim_shunt is None
                  else h["l_shunt_mm"] + trim_shunt)     # horizontal arms
        self.zq = t.get("zq_ohm", 110.0)
        # The ring's own synthesised widths, so the two outputs come out
        # level.  An override here is an amplitude imbalance, and whatever
        # imbalance the coupler has becomes a hard floor under the axial
        # ratio -- the old 40.5/69.2 ohm pair measured +3.1 dB.
        self.wah = tm.get("w_arm_h", t.get("w_arm_h", self.wf))
        self.wav = tm.get("w_arm_v", t.get("w_arm_v", self.ws))
        self.lam_g = f["lam_g50_mm"]
        self.cut = mitre_cut(self.wf, self.hsub)
        self.stub = stub

        self.Lx, self.Ly = L + dLx, L + dLy
        self.Lx2, self.Ly2 = self.Lx / 2.0, self.Ly / 2.0
        L2 = self.Lx2
        self.L2 = L2
        self.xTA = -self.Lx2 - lq            # transformer A starts here
        self.yTB = -self.Ly2 - lq            # transformer B starts here
        self.xl = self.xTA - pre             # ring left edge = route A's riser
        self.xr = self.xl + self.b
        self.yt = yt
        self.yb = yt - self.a
        self.dip_x = dip_x
        # Both elements now take the same trim.  See build() for why the
        # mirrored one no longer needs a detour of its own.
        self.dip_trim = dip_trim if dip_trim_mirror is None or not mirror \
            else dip_trim_mirror
        self._plan()

    # ------------------------------------------------------------------
    def _plan(self):
        """Route the two 50 ohm feeds and solve the dip that matches them."""
        # Route A: straight up the riser, then `pre` mm across into TrA.
        self.rA = [(self.xl, self.yt), (self.xl, 0.0), (self.xTA, 0.0)]
        lenA = abs(self.yt) + self.pre
        # Route B without its dip:  right along yt, then up to TrB.
        base = abs(self.xr) + (self.yTB - self.yt)
        # A dip of depth d adds 2d.  The imbalance is always exactly b, the
        # top-arm length, however the ring is placed -- it is structural.
        self.dip = (lenA - base) / 2.0 + self.dip_trim
        yd = self.yt - self.dip
        self.rB = [(self.xr, self.yt), (self.dip_x, self.yt),
                   (self.dip_x, yd), (0.0, yd), (0.0, self.yTB)]
        self.lenA, self.lenB = lenA, self._plen(self.rB)
        self.phase_err = 360.0 * (self.lenA - self.lenB) / self.lam_g
        self.bends = (len(self.rA) - 2, len(self.rB) - 2)
        self.yd = yd

    @staticmethod
    def _plen(p):
        return sum(math.hypot(p[i + 1][0] - p[i][0], p[i + 1][1] - p[i][1])
                   for i in range(len(p) - 1))

    # ------------------------------------------------------------------
    def build(self):
        L2, w, ws, wq = self.L2, self.wf, self.ws, self.wq
        # Both half-dimensions, so dLy is a real knob.  Using L2 on all four
        # sides drew a square however dLy was set, which silently disabled the
        # only control the axial ratio responds to.
        polys = [rect(-self.Lx2, -self.Ly2, self.Lx2, self.Ly2)]   # patch
        # Transformers overlap their feeding route by JOIN so the two
        # polygons share area rather than merely abutting: a zero-overlap butt
        # joint is fine in Gerber but can read as an open circuit in a meshed
        # field solver.
        polys.append(hseg(self.xTA - JOIN, -self.Lx2, 0.0, wq))  # transformer A
        # Transformer B meets the patch's y edge, so it ends at Ly2.  Ending
        # it at Lx2 left it hanging inside the patch the moment the two
        # dimensions differed, which is the whole point of the trim.
        polys.append(vseg(self.yTB - JOIN, -self.Ly2, 0.0, wq))  # transformer B
        polys.append(hseg(self.xl, self.xr, self.yt, self.wah))   # ring top
        polys.append(hseg(self.xl, self.xr, self.yb, self.wah))   # ring bottom
        polys.append(vseg(self.yb, self.yt, self.xl, self.wav))   # ring left
        polys.append(vseg(self.yb, self.yt, self.xr, self.wav))   # ring right
        polys.append(vseg(self.yb - self.stub, self.yb, self.xl, w))   # input
        polys.append(vseg(self.yb - self.stub, self.yb, self.xr, w))   # iso
        polys.append(stroke(self.rA, w, self.cut))
        polys.append(stroke(self.rB, w, self.cut))
        self.input_pt = (self.xl, self.yb - self.stub)
        self.iso_pt = (self.xr, self.yb - self.stub)
        if self.mirror:
            polys = [[(-x, y) for (x, y) in p][::-1] for p in polys]
            # Reflect, and let the two bottom stubs swap roles: the connector
            # goes on what was the isolated corner.
            #
            # This is the plain mirror, and it is correct again now that the
            # ring's four arms carry their own even split.  It was not correct
            # while the arms were deliberately unbalanced, because then the
            # two bottom corners were not equivalent -- driving the other one
            # inverted the unbalance and took the match from -28.9 to -11.1 dB,
            # which is why a half guided wavelength was being bolted onto
            # route B instead.  An even ring is symmetric about its diagonal,
            # so both bottom corners see the same match and the reflection
            # flips the handedness on its own.  That detour put 16.4 mm of
            # extra line on element 2 alone and left its quadrature drifting
            # further across the band than element 1's; both are now gone.
            self.input_pt, self.iso_pt = ((-self.iso_pt[0], self.iso_pt[1]),
                                          (-self.input_pt[0], self.input_pt[1]))
        return polys

    def bbox(self):
        from geom import polys_bbox
        return polys_bbox(self.build())

    def report(self):
        d = abs(self.phase_err)
        ar = abs(1.0 / math.tan(math.radians((90.0 + d) / 2.0)))
        bb = self.bbox()
        return {
            "patch_Lx_mm": round(self.Lx, 4),
            "patch_Ly_mm": round(self.Ly, 4),
            "transformer": {"Z_ohm": self.zq, "w_mm": round(self.wq, 4),
                            "len_mm": round(self.lq, 4)},
            "ring_arms_mm": {"vertical_len": round(self.a, 4),
                             "horizontal_len": round(self.b, 4),
                             "vertical_w": round(self.wav, 4),
                             "horizontal_w": round(self.wah, 4)},
            "ring_bbox_mm": [round(self.xl, 3), round(self.yb, 3),
                             round(self.xr, 3), round(self.yt, 3)],
            "routeA_len_mm": round(self.lenA, 5),
            "routeB_len_mm": round(self.lenB, 5),
            "dip_depth_mm": round(self.dip, 4),
            "bends": {"routeA": self.bends[0], "routeB": self.bends[1]},
            "residual_phase_error_deg": round(self.phase_err, 5),
            "axial_ratio_from_feed_only_dB":
                round(20 * math.log10(max(ar, 1 / ar)), 4),
            "bbox_mm": [round(v, 3) for v in bb],
            "size_mm": [round(bb[2] - bb[0], 3), round(bb[3] - bb[1], 3)],
            "input_pt_mm": [round(v, 3) for v in self.input_pt],
            "iso_pt_mm": [round(v, 3) for v in self.iso_pt],
        }


if __name__ == "__main__":
    cfg = json.load(open(os.path.join(HERE, "synthesis.json")))["RO4350B"]
    e = Element(cfg)
    e.build()
    print(json.dumps(e.report(), indent=2))
    print("half-wave pitch 25.844 mm -> gap between neighbours:",
          round(25.844 - e.report()["size_mm"][0], 3), "mm")
