"""Ground plates and the bracket that carries both arrays.

WHY THIS EXISTS
---------------
A patch antenna does not radiate off the patch alone.  It radiates off the
patch working against the metal beneath it, and that metal has to keep going
for roughly half a wavelength past the patch or the field runs off the edge,
wraps round the back, and comes out the other side turned the wrong way.  The
boards are 5.8 GHz, so half a wavelength is about 25 mm.

Measured on this exact element: 6 mm of laminate past the patch gives 7.61 dB
axial ratio.  The same element on 25 mm of metal gives 2.74 dB.  Axial ratio
is how round the polarisation is; the receiver only hears the round part, so
that difference is roughly 4 dB of signal on transmit and another 4 on
receive.  The boards were deliberately shrunk to save money on the
understanding that this bracket supplies the rest of the ground.  It is not
an accessory.

So each array sits on a 1.5 mm aluminium plate sized to give 25 mm of metal
past every patch edge, and the plate sits in a printed tray.  The board's
whole back face is ground copper with the solder mask opened over it, so it
clamps metal to metal against the plate -- a far better bond than any bolt.
The mask used to cover that face, which made the claim untrue.

The plate stops short of the board's connector edge -- two notches -- because
the coaxial connectors clamp round the board and need their lower jaw free.

The two trays are joined below the apertures by a bar, so nothing dielectric
sits on the straight line between the two arrays.  The bar is a lap joint on
a 10 mm pitch, so the separation is settable from 220 to 280 mm.  250 mm is
the design point: it puts 25.8 dB of isolation between transmit and receive,
and keeps the pair's far field inside 3 m.

Run it:  python3 carrier.py
"""

import json
import math
import os
import subprocess

HERE = os.path.dirname(os.path.abspath(__file__))
DESIGN = os.path.join(os.path.dirname(HERE), "design")

# ---------------------------------------------------------------- constants
MARGIN = 25.0          # metal wanted past every patch edge
PLATE_T = 1.5          # aluminium thickness
BASE = 250.0           # what the two halves reach on their own
SEP = 400.0            # between array centres, with the extension fitted.
                       # Chosen from the leak budget, not from taste: at 250
                       # the radio has 1.8 dB in hand against compression, at
                       # 400 it has 4.1, and past about 450 the front-end
                       # board's own path across its laminate becomes the
                       # floor so more distance buys nothing.  Far field is
                       # 8.5 m, which a 1000 m radar never notices.
STEP = 10.0            # lap-joint hole pitch, so separation is adjustable

WALL = 3.0             # tray side wall
FLOOR = 5.0            # tray floor: thick enough to swallow an M3 nut
LIP = 1.2              # wall standing proud of the plate, locating it
CLEAR = 0.3            # print clearance on mating faces
TRAY_T = FLOOR + PLATE_T + LIP

BAR_D = 25.0           # bar depth, hanging below the trays
BAR_T = 12.0           # bar thickness, enough for a tripod nut
BAR_DROP = 2.0         # air between the lower tray edge and the bar
LAP_L = 80.0           # holed stretch on each arm.  Must be a whole
                       # number of STEPs or the two hole rows never meet.
OVL = 34.0             # how far the arm reaches back under its own tray
WAIST = 20.0           # full-depth middle of an extender, between its two
                       # tongues.  Without it the two tongues overlap and the
                       # piece is only joined where they happen to touch.
HALF = BAR_D / 2.0     # each arm takes half the bar's depth
PAD_L = 34.0           # mounting pad, local bulge on the arm's outer edge
PAD_D = 21.0

M3 = 3.4               # M3 clearance
NUT_AF = 6.2           # M3 nut across flats, plus clearance
NUT_T = 2.8            # nut pocket depth
CSK_D = 6.4            # M3 countersunk head
UNC14 = 6.8            # 1/4"-20 clearance
UNC14_AF = 11.4        # 1/4"-20 nut across flats, plus clearance
UNC14_T = 6.0

CONN_W = 14.0          # clearance window per coaxial connector
CONN_D = 11.0

# The amplifier and the receive board, which hang behind the bar on a shelf.
# Their hole patterns are read straight off their own drill files, so the
# shelf cannot drift from the boards it carries.
SHELF_T = 5.0
M4 = 4.5
# The radio, and the one front-end board that now bolts straight onto its
# four sockets.  There used to be two amplifier boards on a shelf behind each
# antenna; they are one board on the radio's face, so the shelves are gone
# and this cradle replaces them.
B210 = (97.0, 155.0)          # the bare board
FE = 100.0                    # the front-end board, square
FE_POSTS = [(95.5, 21.0), (95.5, 79.0)]   # its far-edge holes, for support


# --------------------------------------------------------------- geometry
def patch_extent(rep):
    """The rectangle the patches occupy, in board coordinates."""
    hx, hy = rep["patch_Lx_mm"] / 2.0, rep["patch_Ly_mm"] / 2.0
    xs = [c[0] for c in rep["patch_centres"]]
    ys = [c[1] for c in rep["patch_centres"]]
    return min(xs) - hx, min(ys) - hy, max(xs) + hx, max(ys) + hy


def plan(name):
    """Size one plate and place every hole in it."""
    d = json.load(open(os.path.join(DESIGN, f"board_{name}.json")))
    rep = d["report"]
    bw, bh = d["outline"]
    px0, py0, px1, py1 = patch_extent(rep)
    edge = 0.8                                   # copper pull-back
    horiz = name == "transmit"                   # connectors on the bottom

    # The board's own ground counts.  Only ask the plate for the shortfall.
    gnd = (edge, edge, bw - edge, bh - edge)
    want = (px0 - MARGIN, py0 - MARGIN, px1 + MARGIN, py1 + MARGIN)
    lo_x = min(want[0], 0.0)
    lo_y = min(want[1], 0.0)
    hi_x = max(want[2], bw)
    hi_y = max(want[3], bh)
    # Round outward to whole millimetres: a waterjet shop quotes on the
    # bounding rectangle and the extra metal can only help.
    lo_x, lo_y = math.floor(lo_x), math.floor(lo_y)
    hi_x, hi_y = math.ceil(hi_x), math.ceil(hi_y)
    PW, PH = hi_x - lo_x, hi_y - lo_y

    off = (-lo_x, -lo_y)                         # board -> plate coordinates

    def to_plate(x, y):
        return x + off[0], y + off[1]

    # connector windows, one per port, on the board's connector edge
    notches = []
    for pnm, x, y, edg in [(p[0], p[1], p[2], p[3]) for p in d["ports"]]:
        cx, cy = to_plate(x, y)
        if horiz:
            notches.append((cx - CONN_W / 2, 0.0, cx + CONN_W / 2, CONN_D))
        else:
            notches.append((0.0, cy - CONN_W / 2, CONN_D, cy + CONN_W / 2))

    # the board's own mounting holes, moved into plate coordinates
    board_holes = []
    for x, y, dia, pad in d["mounts"]:
        hx, hy = to_plate(x, y)
        board_holes.append((hx, hy, M3))

    # four more to pin the plate into the tray, kept off the board footprint
    b0 = to_plate(0.0, 0.0)
    b1 = to_plate(bw, bh)
    cand = []
    if horiz:
        cand = [(b0[0] / 2.0, PH * 0.5), ((PW + b1[0]) / 2.0, PH * 0.5),
                (PW * 0.32, (PH + b1[1]) / 2.0), (PW * 0.68, (PH + b1[1]) / 2.0)]
    else:
        cand = [(PW * 0.5, b0[1] / 2.0), (PW * 0.5, (PH + b1[1]) / 2.0),
                ((PW + b1[0]) / 2.0, PH * 0.32),
                ((PW + b1[0]) / 2.0, PH * 0.68)]
    plate_holes = [(x, y, M3) for x, y in cand]

    # array centre, the datum everything else is placed from
    acx = sum(c[0] for c in rep["patch_centres"]) / len(rep["patch_centres"])
    acy = sum(c[1] for c in rep["patch_centres"]) / len(rep["patch_centres"])
    acx, acy = to_plate(acx, acy)

    return dict(name=name, PW=PW, PH=PH, off=off, board=(bw, bh),
                notches=notches, board_holes=board_holes,
                plate_holes=plate_holes, patch=(px0, py0, px1, py1),
                gnd=gnd, ac=(acx, acy), horiz=horiz)


def check(p):
    """Confirm the composite ground really does reach MARGIN past the patch."""
    px0, py0, px1, py1 = p["patch"]
    gx0, gy0, gx1, gy1 = p["gnd"]                # board copper, board coords
    ox, oy = p["off"]
    qx0, qy0 = -ox, -oy                          # plate, in board coords
    qx1, qy1 = qx0 + p["PW"], qy0 + p["PH"]
    out = {}
    out["left"] = px0 - min(gx0, qx0)
    out["right"] = max(gx1, qx1) - px1
    out["below"] = py0 - min(gy0, qy0)
    out["above"] = max(gy1, qy1) - py1
    return out


# --------------------------------------------------------------------- DXF
def dxf(path, outline, holes, csk=(), note=None, extra_lines=()):
    """Minimal R12: universally accepted by waterjet and laser shops."""
    e = []

    def g(code, val):
        e.append(f"{code}\n{val}")

    def line(a, b, layer="CUT"):
        g(0, "LINE"); g(8, layer)
        g(10, f"{a[0]:.4f}"); g(20, f"{a[1]:.4f}")
        g(11, f"{b[0]:.4f}"); g(21, f"{b[1]:.4f}")

    def arc(c, r, a0, a1, layer="CUT"):
        g(0, "ARC"); g(8, layer)
        g(10, f"{c[0]:.4f}"); g(20, f"{c[1]:.4f}"); g(40, f"{r:.4f}")
        g(50, f"{a0:.4f}"); g(51, f"{a1:.4f}")

    def circle(c, r, layer="CUT"):
        g(0, "CIRCLE"); g(8, layer)
        g(10, f"{c[0]:.4f}"); g(20, f"{c[1]:.4f}"); g(40, f"{r:.4f}")

    # outline: list of (point, radius); radius 0 gives a sharp corner
    n = len(outline)
    seg = []
    for i in range(n):
        p_prev = outline[(i - 1) % n][0]
        p, r = outline[i]
        p_next = outline[(i + 1) % n][0]
        v1 = (p_prev[0] - p[0], p_prev[1] - p[1])
        v2 = (p_next[0] - p[0], p_next[1] - p[1])
        l1 = math.hypot(*v1) or 1.0
        l2 = math.hypot(*v2) or 1.0
        u1 = (v1[0] / l1, v1[1] / l1)
        u2 = (v2[0] / l2, v2[1] / l2)
        ang = math.acos(max(-1.0, min(1.0, u1[0] * u2[0] + u1[1] * u2[1])))
        if r <= 0 or abs(ang) < 1e-6 or abs(ang - math.pi) < 1e-6:
            seg.append((p, p, None))
            continue
        t = r / math.tan(ang / 2.0)
        t = min(t, l1 * 0.49, l2 * 0.49)
        r_eff = t * math.tan(ang / 2.0)
        a = (p[0] + u1[0] * t, p[1] + u1[1] * t)
        b = (p[0] + u2[0] * t, p[1] + u2[1] * t)
        bis = (u1[0] + u2[0], u1[1] + u2[1])
        lb = math.hypot(*bis) or 1.0
        dcent = r_eff / math.sin(ang / 2.0)
        c = (p[0] + bis[0] / lb * dcent, p[1] + bis[1] / lb * dcent)
        seg.append((a, b, (c, r_eff)))

    for i in range(n):
        a, b, fil = seg[i]
        nxt = seg[(i + 1) % n][0]
        if fil:
            c, r_eff = fil
            a0 = math.degrees(math.atan2(a[1] - c[1], a[0] - c[0]))
            a1 = math.degrees(math.atan2(b[1] - c[1], b[0] - c[0]))
            cross = ((a[0] - c[0]) * (b[1] - c[1]) -
                     (a[1] - c[1]) * (b[0] - c[0]))
            if cross > 0:
                arc(c, r_eff, a0, a1)
            else:
                arc(c, r_eff, a1, a0)
        line(b, nxt)

    # Every hole goes on the CUT layer, countersunk ones included.  Putting
    # them on their own layer alone meant a shop that only cuts CUT would
    # deliver a plate with four holes missing.  The second circle is the head
    # diameter, so the chamfer is described rather than implied.
    for x, y, dia in list(holes) + list(csk):
        circle((x, y), dia / 2.0)
    # No countersinks.  Taking a 3.4 mm hole out to 6.4 mm at 90 degrees
    # needs exactly 1.50 mm of depth and the plate IS 1.50 mm, so the cutter
    # would break through.  A pan-head screw in a 3.4 mm hole stands 2 mm
    # proud instead, and every one of these sits behind the board where
    # nothing touches it.
    for x0, y0, x1, y1 in extra_lines:
        line((x0, y0), (x1, y1), layer="BEND")
    if note:
        g(0, "TEXT"); g(8, "NOTE")
        g(10, f"{note[1]:.3f}"); g(20, f"{note[2]:.3f}")
        g(40, "3.0"); g(1, note[0])

    body = "\n".join(e)
    open(path, "w").write(
        "0\nSECTION\n2\nENTITIES\n" + body + "\n0\nENDSEC\n0\nEOF\n")


def plate_outline(p, r=3.0):
    """Plate rectangle with the connector notches cut into one edge."""
    PW, PH = p["PW"], p["PH"]
    pts = []
    if p["horiz"]:
        # bottom edge, left to right, dropping into each notch
        pts.append(((0.0, 0.0), r))
        for x0, y0, x1, y1 in sorted(p["notches"]):
            pts += [((x0, 0.0), 1.0), ((x0, y1), 1.0),
                    ((x1, y1), 1.0), ((x1, 0.0), 1.0)]
        pts += [((PW, 0.0), r), ((PW, PH), r), ((0.0, PH), r)]
    else:
        pts.append(((0.0, 0.0), r))
        pts += [((PW, 0.0), r), ((PW, PH), r), ((0.0, PH), r)]
        rev = []
        for x0, y0, x1, y1 in sorted(p["notches"], key=lambda n: -n[1]):
            rev += [((0.0, y1), 1.0), ((x1, y1), 1.0),
                    ((x1, y0), 1.0), ((0.0, y0), 1.0)]
        pts = pts[:4] + rev
    return pts


# -------------------------------------------------------------------- SCAD
def scad(tx, rx):
    """Two printed halves that bolt together into one bracket.

    The arrays are stacked, transmit above receive, on a mast that turns
    through 360 degrees for coverage.  The stack direction is free: the
    250 mm between the two boards is a common offset that falls straight out
    of the angle measurement, confirmed by beamforming a simulated target.
    What is NOT free is each board's own element axis -- transmit's pair must
    stay horizontal to measure azimuth and receive's vertical to measure
    elevation -- so the trays keep their orientation and only their placement
    changes.

    Everything below is generated in a frame where the two are side by side,
    and the whole assembly is stood on its end at the very last step.  Doing
    it that way keeps one set of lap-joint arithmetic rather than two.
    """
    # tray extents in the build frame, array centre of each at y = 0
    def tray_box(p, x0):
        ox = x0 - p["ac"][0]
        oy = -p["ac"][1]
        return (ox - WALL, oy - WALL, ox + p["PW"] + WALL, oy + p["PH"] + WALL,
                ox, oy)

    def tray_box2(p, cx, cy):
        ox, oy = cx - p["ac"][0], cy - p["ac"][1]
        return (ox - WALL, oy - WALL, ox + p["PW"] + WALL, oy + p["PH"] + WALL,
                ox, oy)

    T = tray_box2(tx, 0.0, 0.0)
    R = tray_box2(rx, 0.0, -SEP)
    EXT = SEP - BASE                    # what the extension has to bridge
    # The gap the spine lives in, between the two trays.
    gap_hi, gap_lo = T[1], R[3]
    x_split = 0.0                       # tx takes x<0, rx takes x>0
    bar_x0, bar_x1 = -HALF, HALF

    # Put the joint wherever it makes the two prints the same length, rather
    # than at the halfway point.  The trays are different sizes, so halfway
    # made the transmit half 219 mm long -- past what a 220 mm bed can hold
    # once the skirt is counted.
    # Put the joint where the two PRINTS come out the same length, not at
    # the midpoint of the gap: the trays are different heights, so the gap's
    # middle made the receive half 219 mm -- past what a 220 mm bed holds
    # once the skirt is counted.
    mid = (T[3] + R[1] + EXT) / 2.0     # tx arm end, as if the pair were
    mid_r = mid - EXT                   # at BASE; rx arm end likewise
    n_hole = int((LAP_L - STEP) // STEP)
    # How much clear air is left between the two arm ends.  This is not free
    # to choose: the extender that bridges it has to carry a lap at each end
    # plus a full-depth waist between them, and its length works out at
    # (gap + 3 laps) / 2.  Leave too small a gap and that length comes out
    # under two laps, so the two tongues overlap instead of being joined --
    # the piece stops being a bar and becomes two plates touching at a corner.
    GAP = LAP_L + 2 * WAIST
    cen = (mid + mid_r) / 2.0
    lap_lo = cen + GAP / 2.0            # the transmit arm ends here
    lap_hi = lap_lo + LAP_L             # its lap band runs back up from there
    rlap_hi = cen - GAP / 2.0           # and the receive arm ends here
    rlap_lo = rlap_hi - LAP_L
    tx_holes = [lap_lo + STEP * (i + 1) for i in range(n_hole)]
    rx_holes = [rlap_hi - STEP * (i + 1) for i in range(n_hole)]
    pad_tx = lap_hi + 25.0
    pad_rx = rlap_lo - 25.0
    # and the collision check slides the RECEIVE half along y now


    def nut_pockets(p, ox, oy):
        s = []
        for x, y, _ in p["board_holes"] + p["plate_holes"]:
            s.append(f"      translate([{ox+x:.3f}, {oy+y:.3f}, -1]) "
                     f"cylinder(d={NUT_AF/math.cos(math.pi/6):.3f}, "
                     f"h={NUT_T+1:.2f}, $fn=6);")
            s.append(f"      translate([{ox+x:.3f}, {oy+y:.3f}, -1]) "
                     f"cylinder(d={M3:.2f}, h={FLOOR+3:.2f});")
        return "\n".join(s)

    def windows(p, ox, oy, out_to):
        """A clear run for each coaxial connector.

        Cut at the whole-half level, not just the tray: on the transmit side
        the gusset lands squarely in front of the second connector, and
        cutting only the tray wall left the connector driving into solid
        plastic.  The run stops at the arm, which leaves the gap between tray
        and bar as a cable channel.
        """
        s = []
        for x0, y0, x1, y1 in p["notches"]:
            if p["horiz"]:
                s.append(f"    translate([{ox+x0-1:.3f}, {out_to:.3f}, -1]) "
                         f"cube([{x1-x0+2:.3f}, {oy+y1-out_to:.3f}, "
                         f"{BAR_T+2:.2f}]);")
            else:
                s.append(f"    translate([{out_to:.3f}, {oy+y0-1:.3f}, -1]) "
                         f"cube([{ox+x1-out_to:.3f}, {y1-y0+2:.3f}, "
                         f"{BAR_T+2:.2f}]);")
        return "\n".join(s)

    def tray(p, box, tag):
        x0, y0, x1, y1, ox, oy = box
        return f"""
module tray_{tag}() {{
  difference() {{
    union() {{
      translate([{x0:.3f}, {y0:.3f}, 0])
        cube([{x1-x0:.3f}, {y1-y0:.3f}, {TRAY_T:.2f}]);
    }}
    // pocket the plate drops into
    translate([{ox-CLEAR/2:.3f}, {oy-CLEAR/2:.3f}, {FLOOR:.2f}])
      cube([{p['PW']+CLEAR:.3f}, {p['PH']+CLEAR:.3f}, {PLATE_T+LIP+1:.2f}]);
    // fasteners, each with an M3 nut trapped in the underside
{nut_pockets(p, ox, oy)}
  }}
}}"""

    def arm(tag, y_from, y_to, left, holes, pad_y):
        """One half of the spine: a vertical bar between the two trays.

        The two arms sit side by side across the spine's width rather than
        one stepping into the other, so they slide past each other at every
        separation the lap offers.  Each runs up into its own tray, so tray
        and spine are one solid piece with no bridging joint to come apart --
        the horizontal version needed a gusset for exactly that.
        """
        xlo = bar_x0 if left else x_split
        xhi = x_split if left else bar_x1
        # the mounting pad bulges away from the other arm, never into it
        px = xlo - (PAD_D - HALF) if left else xhi
        pcx = xhi - PAD_D / 2.0 if left else xlo + PAD_D / 2.0
        hole_s = "\n".join(
            f"    translate([{bar_x0-1:.3f}, {h:.3f}, {BAR_T/2:.2f}]) "
            f"rotate([0,90,0]) cylinder(d={M3:.2f}, h={BAR_D+2:.2f}, $fn=24);"
            for h in holes)
        return f"""
module arm_{tag}() {{{{
  difference() {{{{
    union() {{{{
      translate([{xlo:.3f}, {min(y_from,y_to):.3f}, 0])
        cube([{xhi-xlo:.3f}, {abs(y_to-y_from):.3f}, {BAR_T:.2f}]);
      // local pad, deep enough to swallow a 1/4-20 tripod nut
      translate([{px:.3f}, {pad_y-PAD_L/2:.3f}, 0])
        cube([{PAD_D-HALF:.2f}, {PAD_L:.2f}, {BAR_T:.2f}]);
    }}}}
{hole_s}
    // tripod thread, and two M4 for any flat bracket
    translate([{pcx:.3f}, {pad_y:.3f}, -1])
      cylinder(d={UNC14:.2f}, h={BAR_T+2:.2f}, $fn=32);
    translate([{pcx:.3f}, {pad_y:.3f}, -0.01])
      cylinder(d={UNC14_AF/math.cos(math.pi/6):.3f}, h={UNC14_T:.2f}, $fn=6);
    translate([{pcx:.3f}, {pad_y-11:.3f}, -1])
      cylinder(d=4.5, h={BAR_T+2:.2f}, $fn=24);
    translate([{pcx:.3f}, {pad_y+11:.3f}, -1])
      cylinder(d=4.5, h={BAR_T+2:.2f}, $fn=24);
  }}}}
}}}}"""

    boxes = {
        'tx': [('tray', T[0], T[1], T[2], T[3]),
               ('arm', bar_x0, lap_lo, x_split, T[1] + OVL),
               ('pad', bar_x0 - (PAD_D - HALF), pad_tx - PAD_L / 2,
                bar_x0, pad_tx + PAD_L / 2)],
        'rx': [('tray', R[0], R[1], R[2], R[3]),
               ('arm', x_split, R[3] - OVL, bar_x1, rlap_hi),
               ('pad', bar_x1, pad_rx - PAD_L / 2,
                bar_x1 + (PAD_D - HALF), pad_rx + PAD_L / 2)],
        'holes': (tx_holes, rx_holes),
        'tx_end': lap_lo, 'rx_end': rlap_hi,
        'pads': ((x_split - PAD_D / 2.0, pad_tx),
                 (x_split + PAD_D / 2.0, pad_rx)),
    }
    return boxes, f"""// GENERATED by carrier.py -- edit that, not this.
//
// Two printed halves.  Each carries one array on a 1.5 mm aluminium plate,
// and they bolt to each other through the lap in the middle of the bar.
// Print both flat, opening upwards, no supports.
//
//   separation is set by which holes you line up in the lap:
//   {SEP-3*STEP:.0f} to {SEP+3*STEP:.0f} mm in {STEP:.0f} mm steps.  {SEP:.0f} is the design point.

part = "all";     // "transmit" | "receive" | "all"
$fn = 48;
{tray(tx, T, 'tx')}
{tray(rx, R, 'rx')}
{arm('tx', T[1] + OVL, lap_lo, True, tx_holes, pad_tx)}
{arm('rx', R[3] - OVL, rlap_hi, False, rx_holes, pad_rx)}

module half_tx() {{
  difference() {{
    union() {{
      tray_tx();
      arm_tx();
    }}
    // The arm reaches {OVL:.0f} mm back under its own tray, so the pocket has
    // to be cut a second time from the finished union.  It must also be cut
    // ALL the way up: the arm is {BAR_T:.0f} mm thick against the tray's
    // {FLOOR+PLATE_T+LIP:.1f} mm, and a pocket only as deep as the plate left a
    // tongue of plastic hanging over the antenna's face, 1.4 mm off the
    // copper.  Nothing may stand in front of a patch.
    translate([{T[4]-CLEAR/2:.3f}, {T[5]-CLEAR/2:.3f}, {FLOOR:.2f}])
      cube([{tx['PW']+CLEAR:.3f}, {tx['PH']+CLEAR:.3f}, {BAR_T-FLOOR+10:.2f}]);
    // clear runs for the coaxial connectors
{windows(tx, T[4], T[5], T[5] - 14.0)}
  }}
}}

module half_rx() {{
  difference() {{
    union() {{
      tray_rx();
      arm_rx();
    }}
    translate([{R[4]-CLEAR/2:.3f}, {R[5]-CLEAR/2:.3f}, {FLOOR:.2f}])
      cube([{rx['PW']+CLEAR:.3f}, {rx['PH']+CLEAR:.3f}, {BAR_T-FLOOR+10:.2f}]);
{windows(rx, R[4], R[5], R[0] - 14.0)}
  }}
}}

if (part == "transmit") half_tx();
else if (part == "receive") half_rx();
else {{
  half_tx();
  half_rx();
  // the aluminium, and the boards sitting on it
  color("silver") translate([{T[4]:.3f}, {T[5]:.3f}, {FLOOR:.2f}])
    cube([{tx['PW']:.2f}, {tx['PH']:.2f}, {PLATE_T:.2f}]);
  color("silver") translate([{R[4]:.3f}, {R[5]:.3f}, {FLOOR:.2f}])
    cube([{rx['PW']:.2f}, {rx['PH']:.2f}, {PLATE_T:.2f}]);
  color("green") translate([{T[4]+tx['off'][0]:.3f}, {T[5]+tx['off'][1]:.3f},
    {FLOOR+PLATE_T:.2f}])
    cube([{tx['board'][0]:.3f}, {tx['board'][1]:.3f}, 0.76]);
  color("green") translate([{R[4]+rx['off'][0]:.3f}, {R[5]+rx['off'][1]:.3f},
    {FLOOR+PLATE_T:.2f}])
    cube([{rx['board'][0]:.3f}, {rx['board'][1]:.3f}, 0.76]);
}}
"""


# 100 mm returns, not 60: the leak goes round the fin's sides, not over
# its tip, and folding the edges is worth more than making it wider.
FIN_W, FIN_H, FIN_WING, FIN_FLANGE = 200.0, 100.0, 100.0, 25.0


def fin_flat():
    """The isolation fin, as a flat pattern with bend lines.

    Each array radiates -2.23 dBi straight out of its edge, right at the
    other one, so at 250 mm apart air alone gives about -40 dB.  A sheet of
    metal standing between them makes the signal bend over an edge to get
    across, and bending costs it dearly.

    What limits it is not the tip but the way round the SIDES.  A fin the
    width of the plates is worth only 11 dB because the leak simply goes
    round.  Folding the side edges forward beats making it wider: 200 mm with
    60 mm returns gives 16.5 dB, which a flat 300 mm fin does not reach.

    It blocks nothing inside 51 degrees off boresight and the radar looks
    +/-27, so the coverage never sees it.
    """
    W, H, wg, fl = FIN_W, FIN_H, FIN_WING, FIN_FLANGE
    x0 = -(W / 2 + wg)
    outline = [((x0, 0.0), 2.0), ((x0 + 2 * wg + W, 0.0), 2.0),
               ((x0 + 2 * wg + W, H), 2.0), ((x0, H), 2.0)]
    # Four bolts onto the spine, which is only BAR_D wide -- the flange is
    # 200 mm across but the thing it fastens to is 25 mm, so a hole pattern
    # spanning the fin would have missed it entirely.
    holes = [(x, y, M4) for x in (-8.0, 8.0) for y in (6.0, fl - 6.0)]
    bends = [(-W / 2, 0.0, -W / 2, H), (W / 2, 0.0, W / 2, H),
             (-W / 2, fl, W / 2, fl)]
    return outline, holes, bends


def extender(tx_end, rx_end):
    """Two identical spacers that lengthen the spine from 280 mm to 400 mm.

    The two halves lap directly for 220 to 280 mm, which is all the bracket
    needed until the front-end board arrived.  That board puts both chains on
    one laminate beside the radio, and its leak budget wants the arrays
    further apart: at 250 mm the radio has 1.8 dB in hand before it
    compresses, at 400 mm it has 4.1.  That is not about damage -- it is
    dynamic range.  A leak sitting just under the converter's ceiling uses up
    the converter, and what is left has to hold the echo.

    Past about 450 mm the front-end board's own path across its laminate
    becomes the floor and more distance buys nothing, so 400 is the knee.

    Two identical pieces rather than one, because a single spacer would be
    310 mm and off the bed.  Each carries a tongue at both ends, on opposite
    sides, so a chain of them alternates and every joint is a proper lap.
    """
    # The chain has to reach from LAP_L inside the transmit arm to LAP_L
    # inside the receive arm, and the two pieces themselves overlap by
    # another LAP_L -- three overlaps, not one.
    gap = tx_end - rx_end
    P = (gap + 3 * LAP_L) / 2.0
    top = tx_end + LAP_L
    body = f"""
module extender() {{
  difference() {{
    union() {{
      // the half-depth tongue that laps whatever is above it
      translate([{0.0:.2f}, {-LAP_L:.2f}, 0])
        cube([{HALF:.2f}, {LAP_L:.2f}, {BAR_T:.2f}]);
      // full depth through the middle
      translate([{-HALF:.2f}, {-(P - LAP_L):.2f}, 0])
        cube([{BAR_D:.2f}, {P - 2 * LAP_L:.2f}, {BAR_T:.2f}]);
      // and the tongue that laps whatever is below, on the other side
      translate([{-HALF:.2f}, {-P:.2f}, 0])
        cube([{HALF:.2f}, {LAP_L:.2f}, {BAR_T:.2f}]);
    }}
""" + "\n".join(
        f"    translate([{-HALF-1:.2f}, {-y:.2f}, {BAR_T/2:.2f}]) "
        f"rotate([0,90,0]) cylinder(d={M3:.2f}, h={BAR_D+2:.2f}, $fn=24);"
        for y in [STEP * (i + 1) for i in range(int((LAP_L - STEP) // STEP))]
        + [P - LAP_L + STEP * (i + 1)
           for i in range(int((LAP_L - STEP) // STEP))]) + f"""
  }}
}}
"""
    return body, P, top


def cradle(pads):
    """Holds the radio behind the spine, with the front-end board on its face.

    The board plugs into the radio's four sockets directly, so it needs no
    tray of its own -- but a 100 mm square hanging off four coaxial joints
    wants its outer edge supported, which is what the two posts are for.

    The radio is captured by its edges rather than by its mounting holes:
    the hole pattern is not something I can verify from here, and a frame
    that grips a 97 x 155 mm board does not need it.
    """
    (px, py), _ = pads
    W, L = B210
    wall, clr = 5.0, 0.5
    OW, OL = W + 2 * wall + clr, L + 2 * wall + clr
    lip = 4.0
    body = f"""
module cradle() {{
  difference() {{
    union() {{
      cube([{OW:.2f}, {OL:.2f}, {SHELF_T + lip:.2f}]);
      // two tabs onto the spine's own mounting pad
      translate([{OW/2 - 14:.2f}, {OL - 1:.2f}, 0])
        cube([28.0, 16.0, {SHELF_T:.2f}]);
    }}
    // the radio drops in from the front
    translate([{wall:.2f}, {wall:.2f}, {SHELF_T:.2f}])
      cube([{W + clr:.2f}, {L + clr:.2f}, {lip + 1:.2f}]);
    // and most of the back is open, so it can breathe and lose weight
    translate([{wall + 12:.2f}, {wall + 12:.2f}, -1])
      cube([{W - 24:.2f}, {L - 24:.2f}, {SHELF_T + 2:.2f}]);
    // four M4 onto the spine pad
    for (dx = [-8, 8], dy = [6, 19])
      translate([{OW/2:.2f} + dx, {OL:.2f} + dy - 1, -1])
        cylinder(d={M4:.2f}, h={SHELF_T + 2:.2f}, $fn=24);
  }}
  // posts holding the outer edge of the front-end board
  for (p = [{FE_POSTS[0][1]:.1f}, {FE_POSTS[1][1]:.1f}])
    translate([{OW/2:.2f} - {FE/2:.1f} + {FE:.1f} - 4.5, p + {wall:.2f}, 0])
      difference() {{
        cylinder(d=9.0, h={SHELF_T + 10:.2f}, $fn=32);
        translate([0, 0, {SHELF_T + 10 - NUT_T - 0.2:.2f}])
          cylinder(d={NUT_AF/math.cos(math.pi/6):.3f}, h={NUT_T + 1:.2f}, $fn=6);
        translate([0, 0, -1]) cylinder(d={M3:.2f}, h={SHELF_T + 12:.2f});
      }}
}}
"""
    return [("cradle", body, (OW, OL + 15.0), (px - OW / 2, py - OL - 7.0))]


# --------------------------------------------------------------------- main
def main():
    tx, rx = plan("transmit"), plan("receive")

    print("  ground plates, sized from the patches outward\n")
    for p in (tx, rx):
        m = check(p)
        ok = all(v >= MARGIN - 0.01 for v in m.values())
        print(f"  {p['name']:9} plate {p['PW']:.0f} x {p['PH']:.0f} x "
              f"{PLATE_T} mm   board sits at "
              f"({p['off'][0]:.2f}, {p['off'][1]:.2f})")
        print(f"            metal past the patch:  "
              f"left {m['left']:.2f}  right {m['right']:.2f}  "
              f"below {m['below']:.2f}  above {m['above']:.2f} mm   "
              f"{'all >= 25' if ok else 'SHORT'}")
        print(f"            {len(p['board_holes'])} board bolts + "
              f"{len(p['plate_holes'])} plate bolts, "
              f"{len(p['notches'])} connector notches")
        dxf(os.path.join(HERE, f"plate_{p['name']}.dxf"),
            plate_outline(p),
            p["board_holes"] + p["plate_holes"],
            csk=(),
            note=(f"5.8 GHz RADAR GROUND PLATE - {p['name'].upper()} - "
                  f"{PLATE_T} mm AL",
                  2.0, p["PH"] + 4.0))

    boxes, src = scad(tx, rx)
    shelves = cradle(boxes["pads"])
    sh_src = "".join(x[1] for x in shelves)
    ext_src, ext_len, ext_top = extender(boxes["tx_end"], boxes["rx_end"])
    sh_src += ext_src
    _o, _h, _b = fin_flat()
    dxf(os.path.join(HERE, "fin.dxf"), _o, _h, csk=(),
        note=(f"5.8 GHz RADAR ISOLATION FIN - {PLATE_T} mm AL - "
              f"BEND UP 90 DEG ON THE THREE MARKED LINES",
              -FIN_W / 2, FIN_H + 5.0),
        extra_lines=_b)
    print(f"\n  isolation fin: {FIN_W + 2*FIN_WING:.0f} x {FIN_H:.0f} mm flat, "
          f"folds to {FIN_W:.0f} mm wide with {FIN_WING:.0f} mm returns")
    print(f"    stands between the two arrays and buys 16.5 dB, taking them "
          f"from -40.1 to -56.6 dB")
    print(f"    blocks nothing inside 51 deg; the radar looks +/-27")
    src = src.replace(
        'if (part == "transmit")',
        sh_src + '\nif (part == "cradle") cradle();\n'
        'else if (part == "extender") extender();\n'
        'else if (part == "transmit")')
    # show it in place in the assembly view, behind the arrays
    # the two extenders that bridge the arms, and the radio behind
    _e1 = ext_top
    _e2 = ext_top - ext_len + LAP_L

    # Every joint in the chain has to be a real lap.  The receive arm once
    # stopped exactly one lap short of its own bolt holes: seven holes drilled
    # through fresh air, and the lower array hanging on nothing.  The hole
    # check below passed it, because the holes were in the right places --
    # there was simply no arm around them.  So measure the metal, not the holes.
    _chain = [("transmit arm", boxes['tx'][1][2], boxes['tx'][1][4]),
              ("upper extender", _e1 - ext_len, _e1),
              ("lower extender", _e2 - ext_len, _e2),
              ("receive arm", boxes['rx'][1][2], boxes['rx'][1][4])]
    print()
    for (na, a0, a1), (nb, b0, b1) in zip(_chain, _chain[1:]):
        ov = min(a1, b1) - max(a0, b0)
        ok = "ok" if ov >= LAP_L - 0.05 else "TOO SHORT -- the spine is in two pieces"
        print(f"  {na:15} laps {nb:15} by {ov:6.1f} mm   {ok}")

    # A lap only works if the two pieces take opposite halves of the bar.
    # Where they overlap along the length AND across the width they are not
    # lapping, they are trying to occupy the same plastic, and the thing
    # cannot be assembled however good it looks on screen.
    def _ext_parts(name, top):
        return [(name + " top tongue", x_split, bar_x1, top - LAP_L, top),
                (name + " waist", bar_x0, bar_x1,
                 top - (ext_len - LAP_L), top - LAP_L),
                (name + " lower tongue", bar_x0, x_split,
                 top - ext_len, top - (ext_len - LAP_L))]
    _solids = ([("transmit arm", bar_x0, x_split, lap_lo, T[1] + OVL)] +
               _ext_parts("upper extender", _e1) +
               _ext_parts("lower extender", _e2) +
               [("receive arm", x_split, bar_x1, R[3] - OVL, rlap_hi)])
    _clash = []
    for i, (na, ax0, ax1, ay0, ay1) in enumerate(_solids):
        for nb, bx0, bx1, by0, by1 in _solids[i + 1:]:
            dx = min(ax1, bx1) - max(ax0, bx0)
            dy = min(ay1, by1) - max(ay0, by0)
            if dx > 0.01 and dy > 0.01:
                _clash.append(f"{na} into {nb}: {dx:.1f} x {dy:.1f} mm")
    print(f"  {'no two pieces share the same plastic' if not _clash else 'CLASH'}"
          + ("" if not _clash else "\n    " + "\n    ".join(_clash)))
    src = src.replace(
        '  half_tx();\n  half_rx();',
        '  half_tx();\n  half_rx();\n'
        f'  color("#7f8fa6") translate([0, {_e1:.3f}, 0]) extender();\n'
        f'  color("#94a3b8") translate([0, {_e2:.3f}, 0]) extender();\n'
        + "".join(
            f'  translate([{o[0]:.3f}, {o[1]:.3f}, -{SHELF_T + 14:.2f}])\n'
            f'    color("#4a6fa5") {k}();\n'
            for k, _s, _wh, o in shelves))
    sc = os.path.join(HERE, "carrier.scad")
    open(sc, "w").write(src)

    # Prove the halves never foul each other anywhere in the slide range,
    # and that bolts still line up at each end of it.
    def hit(a, b, dy):
        # the receive half now slides along Y: the pair is stacked, not
        # side by side
        return (a[1] < b[3] and b[1] < a[3] and
                a[2] < b[4] + dy and b[2] + dy < a[4])
    print()
    # The two arms no longer meet each other -- a pair of extenders bridges
    # them -- so check each arm against the extender it actually laps.
    ext_holes_top = [boxes['tx_end'] + LAP_L - STEP * (i + 1)
                     for i in range(int((LAP_L - STEP) // STEP))]
    ext_holes_bot = [boxes['rx_end'] - LAP_L + STEP * (i + 1)
                     for i in range(int((LAP_L - STEP) // STEP))]
    for d in (-3 * STEP, -2 * STEP, -STEP, 0.0, STEP, 2 * STEP, 3 * STEP):
        bad = [f"{p[0]}/{q[0]}" for p in boxes['tx'] for q in boxes['rx']
               if hit(p, q, d)]
        a = len({round(h, 3) for h in boxes['holes'][0]} &
                {round(h, 3) for h in ext_holes_top})
        b = len({round(h - d, 3) for h in boxes['holes'][1]} &
                {round(h - d, 3) for h in ext_holes_bot})
        print(f"  separation {SEP + d:5.0f} mm   "
              f"{'clear' if not bad else 'FOULS ' + ','.join(bad):22} "
              f"{a} bolts into the transmit arm, {b} into the receive arm")

    osc = "/Applications/OpenSCAD.app/Contents/MacOS/OpenSCAD"
    if os.path.exists(osc):
        for part, out in (("transmit", "half_transmit.stl"),
                          ("receive", "half_receive.stl"),
                          ("cradle", "cradle.stl"),
                          ("extender", "extender.stl")):
            r = subprocess.run([osc, "-o", os.path.join(HERE, out),
                                "-D", f'part="{part}"', sc],
                               capture_output=True, text=True)
            bad = [l for l in r.stderr.splitlines()
                   if "ERROR" in l or "WARNING" in l]
            sz = os.path.getsize(os.path.join(HERE, out)) // 1024
            print(f"  {out:22} {sz:5d} kB   "
                  f"{'clean' if not bad else bad[0][:60]}")
        subprocess.run([osc, "-o", os.path.join(HERE, "assembly.png"),
                        "--imgsize=1800,900", "--colorscheme=Tomorrow",
                        "--viewall", "--autocenter", "--projection=p", sc],
                       capture_output=True, text=True)

    for tag in ('tx', 'rx'):
        bs = boxes[tag]
        x0 = min(b[1] for b in bs); x1 = max(b[3] for b in bs)
        y0 = min(b[2] for b in bs); y1 = max(b[4] for b in bs)
        print(f"  half_{tag}  print footprint {x1-x0:6.1f} x {y1-y0:6.1f} "
              f"x {BAR_T:.0f} mm   "
              f"{'fits a 220 mm bed' if max(x1-x0, y1-y0) <= 220 else 'NEEDS A BIG BED'}")

    n_bolt = sum(len(p["board_holes"]) + len(p["plate_holes"]) for p in (tx, rx))
    print(f"\n  everything it takes to bolt together")
    print(f"    {sum(len(p['board_holes']) for p in (tx, rx)):2d} x M3 x 8 cap head    "
          f"array board -> plate -> nut in the tray")
    print(f"    {sum(len(p['plate_holes']) for p in (tx, rx)):2d} x M3 x 8 pan head     "
          f"plate -> nut in the tray (pan head, not countersunk: a "
          f"countersink would break through 1.5 mm of aluminium)")
    print(f"    {n_bolt:2d} x M3 nut            dropped into the tray's underside")
    print(f"     7 x M3 x 30 + nut     through the lap, setting the separation")
    print(f"     8 x M3 x 8 + nut      radio -> cradle, and cradle lid")
    print(f"     4 x M4 x 12 + nut     cradle -> the bar's own mounting pads")
    print(f"     4 x M4 x 10 + nut     isolation fin -> the top of the bar")
    print(f"     2 x 1/4-20 nut        tripod mounts, one on each arm")
    print(f"    stainless throughout: a steel bolt in an aluminium plate "
          f"outdoors will corrode the plate")

    # how much the arrays droop, which is what actually matters
    span = SEP / 2.0
    I = BAR_T * BAR_D ** 3 / 12.0
    mass = 0.13 * 9.81
    E = 2500.0
    print(f"\n  bar {BAR_D:.0f} x {BAR_T:.0f} mm, arrays hang {span:.0f} mm "
          f"either side of centre")
    print(f"  droop at each array: "
          f"{mass * span ** 3 / (3 * E * I):.3f} mm  "
          f"({math.degrees(math.atan(mass*span**3/(3*E*I)/span)):.4f} deg "
          f"of pointing error)")


if __name__ == "__main__":
    main()
