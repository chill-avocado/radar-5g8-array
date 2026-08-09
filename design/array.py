"""Assemble the four elements into the 2x2 MIMO board and emit board.json.

Array geometry
--------------
Transmit  : 2 RHCP patches side by side, 25.844 mm apart in x  (azimuth baseline)
Receive   : 2 LHCP patches stacked,      25.844 mm apart in y  (elevation baseline)

Those two orthogonal baselines convolve into a 2 x 2 virtual array with
half-wave spacing in BOTH planes, which is what gives the radar azimuth and
elevation on the same measurement.  A constant offset between the transmit and
receive blocks only adds a phase common to every virtual element, so the two
blocks can be pushed as far apart as the board allows purely to buy transmit
to receive isolation -- it costs the angle measurement nothing.

The receive element is the transmit element transposed about the line y = x.
That single reflection does two jobs at once: it reverses the polarisation
from right to left hand, and it turns the feed so the receive cables leave the
left edge while the transmit cables leave the bottom edge.  Every electrical
length is preserved exactly, so both arrays are electrically identical.
"""

import json
import math
import os

from element2 import Element
from geom import rect, polys_bbox

HERE = os.path.dirname(os.path.abspath(__file__))
C0 = 299792458.0
F0 = 5.80e9
PITCH = C0 / F0 / 2.0 * 1e3            # 25.8442 mm

# ---------------------------------------------------------------- board plan
BW, BH = 76.0, 176.0                   # board outline, mm
EDGE_KEEP = 0.30                       # ground pull-back from the routed edge
PULLBACK = 0.15                        # signal pull-back at an end launch

TX_Y = 31.00                           # transmit patch centreline
TX_X1 = 23.00                          # left transmit patch
RX_X = 36.00                           # receive patch column
RX_Y2 = 148.000                        # upper receive patch
RX_Y1 = RX_Y2 - PITCH                  # lower receive patch

VIA_D, VIA_PAD = 0.40, 0.75            # stitching via drill / pad
MOUNT_D, MOUNT_PAD = 3.20, 6.00        # M3 clearance hole
# Square 0402 land.  A rectangular pad carries its own rotation, and KiCad
# compares that against the library without normalising for the parent
# footprint, so a part placed at 90 degrees can never match its own
# library entry.  Square pads make the question disappear and are well
# inside normal 0402 practice.
R0402 = dict(w=1.30, h=1.00, gap=0.80)
PROT_X = 7.00          # protection site, as close to the connector as the
                       # launch ground pads allow
PROT_STUB = 0.50       # stub width: narrow, so an unfitted site adds almost
                       # no shunt capacitance to the receive line
FID_D, FID_MASK = 1.00, 2.00


def transpose(polys):
    """Reflect about y = x: flips CP handedness and rotates the feed 90 deg."""
    return [[(y, x) for (x, y) in p][::-1] for p in polys]


def shift(polys, dx, dy):
    return [[(x + dx, y + dy) for (x, y) in p] for p in polys]


class Board:
    def __init__(self, cfg, launch_gap):
        self.cfg = cfg
        self.launch_gap = launch_gap
        self.top = []            # signal copper polygons
        self.top_net = []        # net name parallel to self.top
        self.gnd_top = []        # top-side ground pour polygons
        self.vias = []           # (x, y, drill, pad)
        self.mounts = []         # (x, y, drill, pad)
        self.pads = []           # (ref, x, y, w, h, net)
        self.labels = []         # (x, y, text, size, layer)
        self.ports = []          # (name, x, y, direction)
        self.elements = []       # bookkeeping for the report
        self.launch_traces = []  # indices exempt from the edge rule
        self.mask = []           # F.Mask windows over the RF copper

    # ------------------------------------------------------------------
    def add_element(self, name, cx, cy, rx=False, mirror=False):
        el = Element(self.cfg, mirror=mirror)
        polys = el.build()
        ip, iso = el.input_pt, el.iso_pt
        if rx:
            polys = transpose(polys)
            ip, iso = (ip[1], ip[0]), (iso[1], iso[0])
        polys = shift(polys, cx, cy)
        ip = (ip[0] + cx, ip[1] + cy)
        iso = (iso[0] + cx, iso[1] + cy)
        self.top += polys
        self.top_net += [name] * len(polys)
        self.elements.append({"name": name, "centre": [cx, cy],
                              "hand": "LHCP" if rx else "RHCP",
                              "mirrored": mirror,
                              "input": list(ip), "term": list(iso),
                              "bbox": [round(v, 3) for v in polys_bbox(polys)]})
        return el, ip, iso

    # ------------------------------------------------------------------
    def _add(self, poly, net):
        self.top.append(poly)
        self.top_net.append(net)

    def feed_to_edge(self, ip, side, w, net="?"):
        """Run 50 ohm line from the element input out to the board edge.

        The trace stops PULLBACK short of the routed edge: the connector's
        centre pin overlaps it anyway, and a small pull-back keeps the copper
        clear of the router bit's tolerance.
        """
        x, y = ip
        self.launch_traces.append(len(self.top))
        if side == "bottom":
            self._add(rect(x - w / 2, PULLBACK, x + w / 2, y), net)
            return (x, 0.0)
        self._add(rect(PULLBACK, y - w / 2, x, y + w / 2), net)
        return (0.0, y)

    # ------------------------------------------------------------------
    def sma_launch(self, name, pt, side, w):
        """Grounded-CPW end-launch pad: coplanar ground either side of the
        signal trace, sized so the launch stays 50 ohm, plus the stitching
        Cinch specify (0.4 mm vias at 1.27 mm pitch along the whole launch)."""
        g = self.launch_gap
        depth, half = 4.30, 5.60         # how far the ground pads reach in
        x, y = pt
        if side == "bottom":
            self.gnd_top.append(rect(x - half, EDGE_KEEP, x - w / 2 - g, depth))
            self.gnd_top.append(rect(x + w / 2 + g, EDGE_KEEP, x + half, depth))
            for i in range(5):
                yy = EDGE_KEEP + 0.9 + i * 1.27
                if yy > depth - 0.7:
                    break
                self.vias += [(x - w / 2 - g - 0.85, yy, VIA_D, VIA_PAD),
                              (x + w / 2 + g + 0.85, yy, VIA_D, VIA_PAD)]
            self.labels.append((x, 3.2, name, 1.3, "silk_b"))
        else:
            self.gnd_top.append(rect(EDGE_KEEP, y - half, depth, y - w / 2 - g))
            self.gnd_top.append(rect(EDGE_KEEP, y + w / 2 + g, depth, y + half))
            for i in range(5):
                xx = EDGE_KEEP + 0.9 + i * 1.27
                if xx > depth - 0.7:
                    break
                self.vias += [(xx, y - w / 2 - g - 0.85, VIA_D, VIA_PAD),
                              (xx, y + w / 2 + g + 0.85, VIA_D, VIA_PAD)]
            self.labels.append((3.4, y, name, 1.3, "silk_b"))
        self.ports.append((name, x, y, side))

    # ------------------------------------------------------------------
    def termination(self, ref, iso, side, w, net="?"):
        """0402 50 ohm land pattern plus its ground return."""
        p, x, y = R0402, iso[0], iso[1]
        c = p["gap"] / 2 + p["w"] / 2                # pad centre offset
        if side == "bottom":
            self.pads.append((ref + "A", x, y - c, p["h"], p["w"], "sig"))
            self.pads.append((ref + "B", x, y - c - p["gap"] - p["w"],
                              p["h"], p["w"], "GND"))
            gy = y - c - p["gap"] - p["w"]
            self.gnd_top.append(rect(x - 1.35, gy - 1.9, x + 1.35, gy + p["w"] / 2))
            self.vias += [(x - 0.75, gy - 1.25, VIA_D, VIA_PAD),
                          (x + 0.75, gy - 1.25, VIA_D, VIA_PAD)]
            self._add(rect(x - w / 2, y - c, x + w / 2, y), net)
            self.labels.append((x + 3.2, y - 2.4, ref, 1.0, "silk"))
        else:
            self.pads.append((ref + "A", x - c, y, p["w"], p["h"], "sig"))
            self.pads.append((ref + "B", x - c - p["gap"] - p["w"], y,
                              p["w"], p["h"], "GND"))
            gx = x - c - p["gap"] - p["w"]
            self.gnd_top.append(rect(gx - 1.9, y - 1.35, gx + p["w"] / 2, y + 1.35))
            self.vias += [(gx - 1.25, y - 0.75, VIA_D, VIA_PAD),
                          (gx - 1.25, y + 0.75, VIA_D, VIA_PAD)]
            self._add(rect(x - w / 2, y - w / 2, x, y + w / 2), net)
            self.labels.append((x - 2.4, y + 3.2, ref, 1.0, "silk"))

    # ------------------------------------------------------------------
    def protection_site(self, ref, x, y, w):
        """Unpopulated shunt site on a receive line.

        The drones this radar looks for carry 5.8 GHz video transmitters of
        up to a watt or two, on our own frequency.  At a metre that puts about
        -10 dBm into the receive port and at a third of a metre it exceeds the
        radio's absolute maximum.  This is where a limiter goes.  The board
        ships with the site empty, so a plain antenna costs nothing; fitting a
        shunt limiter diode across the pads buys protection for a few tenths
        of a decibel of noise figure.
        """
        p = R0402
        ytrace = y - w / 2.0
        ysig = ytrace - PROT_STUB - p["h"] / 2.0
        ygnd = ysig - p["h"] - p["gap"]
        self._add(rect(x - PROT_STUB / 2, ysig, x + PROT_STUB / 2, ytrace),
                  ref.replace("D", "RX"))
        self.pads.append((ref + "A", x, ysig, p["w"], p["h"], "sig"))
        self.pads.append((ref + "B", x, ygnd, p["w"], p["h"], "GND"))
        self.gnd_top.append(rect(x - 1.35, ygnd - 1.9, x + 1.35,
                                 ygnd + p["h"] / 2))
        self.vias += [(x - 0.75, ygnd - 1.25, VIA_D, VIA_PAD),
                      (x + 0.75, ygnd - 1.25, VIA_D, VIA_PAD)]
        self.labels.append((x, ygnd - 2.6, ref + " LIMITER (DNF)",
                            1.0, "silk_b"))
        # No hand-drawn window here.  The two pads are on different nets, so
        # one opening across both is a bridge; and they are real pads, so the
        # layout tool already gives each its own opening with resist between.

    def fiducial(self, x, y):
        """Optical alignment target: a bare copper dot in a mask window.

        Tied to the ground plane through a thin neck and a via.  A floating
        dot would read as unconnected copper; tying it down costs the camera
        nothing.
        """
        n = 24
        self.gnd_top.append(
            [(x + FID_D / 2 * math.cos(2 * math.pi * i / n),
              y + FID_D / 2 * math.sin(2 * math.pi * i / n)) for i in range(n)])
        self.gnd_top.append(rect(x - 0.125, y - 2.6, x + 0.125, y))
        self.gnd_top.append(rect(x - 0.9, y - 3.9, x + 0.9, y - 2.4))
        self.vias.append((x, y - 3.15, VIA_D, VIA_PAD))
        self.mask.append(
            [(x + FID_MASK / 2 * math.cos(2 * math.pi * i / n),
              y + FID_MASK / 2 * math.sin(2 * math.pi * i / n))
             for i in range(n)])

    def chassis_pad(self, x, y):
        """Somewhere to bond the ground plane to an enclosure."""
        self.gnd_top.append(rect(x - 2.0, y - 2.0, x + 2.0, y + 2.0))
        for dx in (-1.0, 1.0):
            for dy in (-1.0, 1.0):
                self.vias.append((x + dx, y + dy, VIA_D, VIA_PAD))
        self.mask.append(rect(x - 2.2, y - 2.2, x + 2.2, y + 2.2))
        self.labels.append((x, y + 3.2, "CHASSIS GND", 1.0, "silk"))

    def isolation_pour(self, y0, y1):
        """Via-stitched top ground between the two arrays.

        It is kept well clear of every radiating edge (>5 mm, about 0.1 of a
        wavelength) so it cannot detune a patch.  It earns its place two ways:
        it ties the region between the arrays hard to the ground plane, and it
        balances the copper so a 0.76 mm board does not bow during reflow.
        """
        m = 2.0
        self.gnd_top.append(rect(m, y0, BW - m, y1))
        x0, x1 = m + 2.0, BW - m - 2.0
        nx = int((x1 - x0) // 4.0)
        ny = int((y1 - y0 - 4.0) // 4.0)
        for i in range(nx + 1):
            for j in range(ny + 1):
                vx, vy = x0 + i * 4.0, y0 + 2.0 + j * 4.0
                # never drill into a mounting hole's clearance
                if any(math.hypot(vx - mx, vy - my) < (mp / 2 + 1.2)
                       for mx, my, md, mp in self.mounts):
                    continue
                self.vias.append((vx, vy, VIA_D, VIA_PAD))


def build(cfg, launch_gap):
    b = Board(cfg, launch_gap)
    w50 = cfg["feed"]["w50_mm"]

    # The second element of each pair is mirrored so the two feed networks
    # face outward.  Measured in openEMS, that lifts the transmit-to-transmit
    # isolation from -16.6 dB to -20.5 dB and, more importantly, keeps the
    # axial ratio inside 3 dB: with both networks pointing the same way, one
    # element's feed riser sits 3 mm from the neighbour's radiating edge and
    # the polarisation collapses to 7 dB.
    for i, xc in enumerate((TX_X1, TX_X1 + PITCH)):
        _, ip, iso = b.add_element(f"TX{i+1}", xc, TX_Y, rx=False,
                                   mirror=(i == 1))
        pt = b.feed_to_edge(ip, "bottom", w50, f"TX{i+1}")
        b.sma_launch(f"TX{i+1}", pt, "bottom", w50)
        b.termination(f"R{i+1}", iso, "bottom", w50, f"TX{i+1}")
    for i, yc in enumerate((RX_Y1, RX_Y2)):
        _, ip, iso = b.add_element(f"RX{i+1}", RX_X, yc, rx=True,
                                   mirror=(i == 1))
        pt = b.feed_to_edge(ip, "left", w50, f"RX{i+1}")
        b.sma_launch(f"RX{i+1}", pt, "left", w50)
        b.termination(f"R{i+3}", iso, "left", w50, f"RX{i+1}")
        b.protection_site(f"D{i+1}", PROT_X, ip[1], w50)

    # Solder mask would sit directly on the patches: it lowers the resonance
    # by about 1 % and adds loss at 5.8 GHz, and Cinch specifically say not to
    # coat an end-launch.  Open a window over every radiating structure and
    # leave the rest of the board masked as normal.
    for e in b.elements:
        x0, y0, x1, y1 = e["bbox"]
        tx, ty = e["term"]
        if e["hand"] == "RHCP":          # termination sits below the element
            b.mask.append(rect(x0 - 1.0, ty + 0.6, x1 + 1.0, y1 + 1.0))
        else:                            # ... and to the left, once transposed
            b.mask.append(rect(tx + 0.6, y0 - 1.0, x1 + 1.0, y1 + 1.0))
    for n, x, y, side in b.ports:
        if side == "bottom":
            b.mask.append(rect(x - 7.0, 0.0, x + 7.0, 7.0))
        else:
            b.mask.append(rect(0.0, y - 7.0, 7.0, y + 7.0))
            # narrow strip over the rest of the run, kept clear of the 0402
            b.mask.append(rect(7.0, y - 2.0, 12.0, y + 2.0))
    for x, y in ((5.5, 48.0), (BW - 5.5, 48.0), (5.5, 96.0),
                 (BW - 5.5, 96.0), (5.5, 170.0), (BW - 5.5, 170.0)):
        b.mounts.append((x, y, MOUNT_D, MOUNT_PAD))
    for fx, fy in ((5.5, 22.0), (BW - 5.5, 22.0), (26.0, 172.0)):
        b.fiducial(fx, fy)
    b.chassis_pad(BW - 8.0, 156.0)
    b.isolation_pour(44.0, 100.0)
    b.labels += [
        (BW / 2, 82.0, "5.8 GHz 2x2 MIMO FMCW RADAR ARRAY", 1.6, "silk"),
        (BW / 2, 77.5, "TX: RHCP  d=25.84 mm AZIMUTH", 1.1, "silk"),
        (BW / 2, 73.5, "RX: LHCP  d=25.84 mm ELEVATION", 1.1, "silk"),
        (BW / 2, 69.5, "RO4350B 0.762 mm / 2L / ENIG", 1.1, "silk"),
        (BW / 2, 41.5, "TX ARRAY (RHCP)  spin: x -> y  looking at this face",
         1.2, "silk"),
        (BW / 2, 39.7, "boresight is out of this face", 1.0, "silk"),
        (BW / 2, 102.0, "RX ARRAY (LHCP)  spin: y -> x  looking at this face",
         1.2, "silk"),
    ]
    return b


def report(b, el0):
    e = {d["name"]: d for d in b.elements}
    t1, r1 = e["TX1"]["centre"], e["RX1"]["centre"]
    sep = math.hypot(t1[0] - r1[0], t1[1] - r1[1])
    lam = C0 / F0 * 1e3
    return {
        "board_mm": [BW, BH],
        "f0_GHz": F0 / 1e9,
        "lambda0_mm": round(lam, 4),
        "element_pitch_mm": round(PITCH, 4),
        "patch_Lx_mm": round(el0.Lx, 4), "patch_Ly_mm": round(el0.Ly, 4),
        "transformer_z_ohm": el0.zq, "transformer_w_mm": round(el0.wq, 4),
        "tx_patches": [e["TX1"]["centre"], e["TX2"]["centre"]],
        "rx_patches": [e["RX1"]["centre"], e["RX2"]["centre"]],
        "tx_spacing_mm": round(abs(e["TX2"]["centre"][0] - e["TX1"]["centre"][0]), 4),
        "rx_spacing_mm": round(abs(e["RX2"]["centre"][1] - e["RX1"]["centre"][1]), 4),
        "nearest_tx_rx_mm": round(sep, 2),
        "nearest_tx_rx_lambda": round(sep / lam, 3),
        "ports": [{"name": n, "x": round(x, 3), "y": round(y, 3), "edge": s}
                  for n, x, y, s in b.ports],
        "n_vias": len(b.vias), "n_mounts": len(b.mounts),
        "copper_bbox": [round(v, 3) for v in polys_bbox(b.top + b.gnd_top)],
    }


if __name__ == "__main__":
    import sys
    VAR = sys.argv[1] if len(sys.argv) > 1 else "RO4350B"
    OUTNAME = "board.json" if VAR == "RO4350B" else "board_fr4.json"
    cfg = json.load(open(os.path.join(HERE, "synthesis.json")))[VAR]
    launch_gap_out = cfg["launch"]["gap_mm"]
    w50_out = cfg["feed"]["w50_mm"]
    b = build(cfg, launch_gap_out)
    rep = report(b, Element(cfg))
    print(json.dumps(rep, indent=2))
    bb = rep["copper_bbox"]
    assert bb[0] >= 0 and bb[1] >= 0 and bb[2] <= BW and bb[3] <= BH, \
        f"copper {bb} outside board {BW}x{BH}"
    import drc
    data = {"top": b.top, "gnd_top": b.gnd_top, "vias": b.vias,
            "mounts": b.mounts, "pads": b.pads, "top_net": b.top_net,
            "term_net": {"R1": "TX1", "R2": "TX2", "R3": "RX1", "R4": "RX2",
                         "D1": "RX1", "D2": "RX2"}}
    viol, close = drc.check(data, BW, BH, edge_exempt=set(b.launch_traces))
    print(f"\nclearance check: {len(viol)} violations")
    for v in viol[:20]:
        print("   VIOLATION", v)
    print("   tightest legal clearances:")
    for c in close[:6]:
        print("     ", c[0], c[3], "mm")
    assert not viol, "geometry violates clearance rules"
    json.dump({"top": b.top, "gnd_top": b.gnd_top, "vias": b.vias,
               "mounts": b.mounts, "pads": b.pads, "labels": b.labels,
               "mask": b.mask,
               "ports": b.ports, "outline": [BW, BH], "report": rep,
               "elements": b.elements, "w50": w50_out,
               "launch_gap": launch_gap_out,
               "top_net": b.top_net,
               "term_net": {"R1": "TX1", "R2": "TX2",
                            "R3": "RX1", "R4": "RX2",
                            "D1": "RX1", "D2": "RX2"},
               "stack": {"name": cfg["substrate"]["name"],
                         "h_mm": cfg["substrate"]["h_mm"],
                         "er": cfg["substrate"]["er"],
                         "tand": cfg["substrate"]["tand"]}},
              open(os.path.join(HERE, OUTNAME), "w"))
    print("\nwrote", OUTNAME)
