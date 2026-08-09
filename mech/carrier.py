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
SEP = 250.0            # between array centres
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
BOARDS = {
    "pa":  dict(size=(100.0, 84.0),
                holes=[(5, 5), (5, 79), (36.5, 22), (36.5, 62),
                       (95, 5), (95, 79)]),
    "lna": dict(size=(92.0, 68.0),
                holes=[(5, 5), (5, 63), (36.5, 18), (36.5, 50),
                       (87, 5), (87, 63)]),
}


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
    # The gap the spine lives in, between the two trays.
    gap_hi, gap_lo = T[1], R[3]
    x_split = 0.0                       # tx takes x<0, rx takes x>0
    bar_x0, bar_x1 = -HALF, HALF

    # Put the joint wherever it makes the two prints the same length, rather
    # than at the halfway point.  The trays are different sizes, so halfway
    # made the transmit half 219 mm long -- past what a 220 mm bed can hold
    # once the skirt is counted.
    # Put the joint where the two prints come out the same length.
    mid = (gap_hi + gap_lo) / 2.0
    n_hole = int((LAP_L - STEP) // STEP)
    lap_hi, lap_lo = mid + LAP_L / 2.0, mid - LAP_L / 2.0
    tx_holes = [lap_lo + STEP * (i + 1) for i in range(n_hole)]
    rx_holes = [lap_hi - STEP * (i + 1) for i in range(n_hole)]
    pad_tx = lap_hi + 25.0
    pad_rx = lap_lo - 25.0
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
               ('arm', x_split, R[3] - OVL, bar_x1, lap_hi),
               ('pad', bar_x1, pad_rx - PAD_L / 2,
                bar_x1 + (PAD_D - HALF), pad_rx + PAD_L / 2)],
        'holes': (tx_holes, rx_holes),
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
{arm('rx', R[3] - OVL, lap_hi, False, rx_holes, pad_rx)}

module half_tx() {{
  difference() {{
    union() {{
      tray_tx();
      arm_tx();
    }}
    // the arm runs up into the tray, so cut the plate pocket again here or
    // it fills the corner of it
    translate([{T[4]-CLEAR/2:.3f}, {T[5]-CLEAR/2:.3f}, {FLOOR:.2f}])
      cube([{tx['PW']+CLEAR:.3f}, {tx['PH']+CLEAR:.3f}, {PLATE_T+LIP+1:.2f}]);
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
      cube([{rx['PW']+CLEAR:.3f}, {rx['PH']+CLEAR:.3f}, {PLATE_T+LIP+1:.2f}]);
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


def shelf(pads):
    """A flat tray behind the bar for the amplifier and the receive board.

    Both boards already carry six M3 holes and had nowhere to go.  They bolt
    to this; it bolts to the four M4 points already on the bar's mounting
    pads.  It sits BEHIND the plane of the arrays, so the ground plates
    shield it and it takes nothing out of the aperture.

    The receive board goes on the receive side on purpose.  Its whole benefit
    is being the first thing the signal meets, so every millimetre of cable
    in front of it is noise figure given away.
    """
    (txx, txy), (rxx, rxy) = pads
    pa, lna = BOARDS["pa"], BOARDS["lna"]
    gap, marg = 10.0, 6.0
    W = marg + pa["size"][0] + gap + lna["size"][0] + marg
    H = marg + max(pa["size"][1], lna["size"][1]) + marg
    # put each board under its own side of the bracket
    ox_pa = marg
    ox_lna = marg + pa["size"][0] + gap
    # line the shelf up so its M4 holes fall on the bracket's pads
    sx = txx - (ox_pa + pa["size"][0] / 2.0)
    sy = min(txy, rxy) - 14.0 - H

    def nut(x, y):
        return (f"    translate([{x:.3f}, {y:.3f}, -1]) "
                f"cylinder(d={NUT_AF/math.cos(math.pi/6):.3f}, "
                f"h={NUT_T+1:.2f}, $fn=6);\n"
                f"    translate([{x:.3f}, {y:.3f}, -1]) "
                f"cylinder(d={M3:.2f}, h={SHELF_T+2:.2f});")

    body = []
    for tag, b, ox in (("pa", pa, ox_pa), ("lna", lna, ox_lna)):
        oy = marg
        for hx, hy in b["holes"]:
            body.append(nut(ox + hx, oy + hy))
    # four clearance holes onto the bracket's own M4 pattern
    tabs, tab_h = [], 14.0
    for px, py in ((txx, txy), (rxx, rxy)):
        for dx in (-11.0, 11.0):
            tabs.append((px - sx + dx, py - sy))
    tab_s = "\n".join(
        f"    translate([{x:.3f}, {y:.3f}, -1]) "
        f"cylinder(d={M4:.2f}, h={SHELF_T+2:.2f}, $fn=24);" for x, y in tabs)
    tab_body = "\n".join(
        f"      translate([{x-14:.3f}, {H - 1:.3f}, 0]) "
        f"cube([28.0, {y - H + 8:.3f}, {SHELF_T:.2f}]);"
        for x, y in tabs[::2])
    return f"""
// ---------------------------------------------------------------- shelf
// Bolts to the four M4 points on the bar and carries both electronics
// boards behind the arrays.  Print flat.  {W:.0f} x {H:.0f} mm.
module shelf() {{
  difference() {{
    union() {{
      cube([{W:.2f}, {H:.2f}, {SHELF_T:.2f}]);
{tab_body}
    }}
{chr(10).join(body)}
{tab_s}
    // lighten it: it only has to be flat and hold nine bolts
    for (i = [0 : 3]) for (j = [0 : 1])
      translate([{marg + 18:.2f} + i * 46, {marg + 20:.2f} + j * 34, -1])
        cube([26, 22, {SHELF_T+2:.2f}]);
  }}
}}
""", (W, H), (sx, sy)


FIN_W, FIN_H, FIN_WING, FIN_FLANGE = 200.0, 100.0, 60.0, 25.0


def fin_flat():
    """The isolation fin, as a flat pattern with bend lines.

    Each array radiates -2.23 dBi straight out of its edge, right at the
    other one, so at 250 mm apart air alone gives about -40 dB.  A sheet of
    metal standing between them makes the signal bend over an edge to get
    across, and bending costs it dearly.

    What actually limits it is not the tip but the way round the SIDES.  A
    fin the width of the plates is worth only 11 dB because the leak simply
    goes round.  Folding the side edges forward beats making it wider:
    200 mm with 60 mm returns gives 16.5 dB, which a flat 300 mm fin does not
    reach.  Total -56.6 dB.

    It only blocks past 51 degrees off boresight, and the radar looks +/-27,
    so the coverage never sees it.
    """
    W, H, wg, fl = FIN_W, FIN_H, FIN_WING, FIN_FLANGE
    x0 = -(W / 2 + wg)
    outline = [((x0, 0.0), 2.0), ((x0 + 2 * wg + W, 0.0), 2.0),
               ((x0 + 2 * wg + W, H), 2.0), ((x0, H), 2.0)]
    # a flange along the bottom of the centre panel, bent back to bolt down
    outline = [((x0, -fl if False else 0.0), 2.0)] + outline[1:]
    holes = [(x, fl / 2.0, M4) for x in (-W / 4, 0.0, W / 4)]
    bends = [(-W / 2, 0.0, -W / 2, H), (W / 2, 0.0, W / 2, H),
             (-W / 2, fl, W / 2, fl)]
    return outline, holes, bends


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
    sh_src, sh_wh, sh_off = shelf(boxes["pads"])
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
    src = src.replace('if (part == "transmit")',
                      sh_src + '\nif (part == "shelf") shelf();\n'
                      'else if (part == "transmit")')
    # show it in place in the assembly view, behind the arrays
    src = src.replace('  half_tx();\n  half_rx();',
                      '  half_tx();\n  half_rx();\n'
                      f'  translate([{sh_off[0]:.3f}, {sh_off[1]:.3f}, -{SHELF_T:.2f}])\n'
                      '    color("#4a6fa5") shelf();')
    sc = os.path.join(HERE, "carrier.scad")
    open(sc, "w").write(src)

    # Prove the halves never foul each other anywhere in the slide range,
    # and that bolts still line up at each end of it.
    def hit(a, b, dx):
        return (a[1] < b[3] + dx and b[1] + dx < a[3] and
                a[2] < b[4] and b[2] < a[4])
    print()
    for d in (-3 * STEP, -2 * STEP, -STEP, 0.0, STEP, 2 * STEP, 3 * STEP):
        bad = [f"{p[0]}/{q[0]}" for p in boxes['tx'] for q in boxes['rx']
               if hit(p, q, d)]
        common = len({round(h, 3) for h in boxes['holes'][0]} &
                     {round(h + d, 3) for h in boxes['holes'][1]})
        print(f"  separation {SEP + d:5.0f} mm   "
              f"{'clear' if not bad else 'FOULS ' + ','.join(bad):22} "
              f"{common} bolts line up")

    osc = "/Applications/OpenSCAD.app/Contents/MacOS/OpenSCAD"
    if os.path.exists(osc):
        for part, out in (("transmit", "half_transmit.stl"),
                          ("receive", "half_receive.stl"),
                          ("shelf", "shelf.stl")):
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
    print(f"    {sum(len(p['plate_holes']) for p in (tx, rx)):2d} x M3 x 8 pan head     plate -> nut in the tray"
          f"plate -> nut in the tray (pan head: a countersink would break through 1.5 mm plate)")
    print(f"    {n_bolt:2d} x M3 nut            dropped into the tray's underside")
    print(f"     7 x M3 x 30 + nut     through the lap, setting the separation")
    print(f"    12 x M3 x 8 + nut      amplifier and receive board -> shelf")
    print(f"     4 x M4 x 12 + nut     shelf -> the bar's own mounting pads")
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
