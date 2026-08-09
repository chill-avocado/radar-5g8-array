"""The two array boards, sized from the elements rather than inherited.

Everything this conversation changed, in one place:

  * two boards, transmit and receive, not one
  * ZYF300CA-P PTFE instead of Rogers RO4350B -- half the price, a loss
    tangent of 0.0018 against 0.0037, and more bandwidth, which is this
    design's weakest number.  Every printed dimension is 9.77 per cent larger
    to suit the lower permittivity; the element SPACING is unchanged, because
    that is half a wavelength in air and the air does not care what the board
    is made of.
  * ground margin 6 mm ON THE BOARD, and the rest supplied by an aluminium
    plate the board bolts onto.  Sweeping GAIN against the margin showed it
    flat, which is true and was the wrong thing to look at: the axial ratio
    is not flat at all.  Measured, 6 mm of laminate gives 7.61 dB and 25 mm
    of metal gives 2.74.  So the board stays small and cheap and the plate
    in mech/ carries the ground out to 25 mm past every patch edge.
  * each board sized to what its own elements need, instead of both inheriting
    the 76 mm width of the board they were once cut from
  * the pair panelised as one piece under 100 x 100 mm, scored to snap apart,
    which is what puts the order inside the fabricator's base price
  * immersion silver, not ENIG.  At 5.8 GHz the current only reaches 0.87 um
    into the metal, so ENIG's 3 um of nickel carries all of it -- 2.7 times
    the resistance of copper, costing 4.3 per cent of range.  Silver's tarnish
    is a semiconductor and carries no current at all: a board gone black
    measures the same as a fresh one.
  * the coplanar launch gap rescaled with everything else, to 0.9699 mm.  The SMA footprint's ground pads follow it -- they were left at the amplifier boards' 0.9781 mm, which is an
    8 um disagreement and two DRC warnings per board

The bracket in mech/ holds the two 250 mm apart, which is where the
transmitter stops deafening the receiver.  200 mm is the floor.  A small gap
is worse than none -- measured, 12 mm came out 1.8 dB WORSE than no gap,
because two fresh board edges radiate more than the path they remove.
"""

import json
import math
import os

import importlib.util

HERE = os.path.dirname(os.path.abspath(__file__))
_spec = importlib.util.spec_from_file_location(
    "radar_array", os.path.join(HERE, "array.py"))
A = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(A)

rect, PITCH = A.rect, A.PITCH
MOUNT_D, MOUNT_PAD = A.MOUNT_D, A.MOUNT_PAD

VARIANT = "ZYF300CA"
CFG = json.load(open(os.path.join(HERE, "synthesis.json")))[VARIANT]
MARGIN = 6.0                     # ground past every patch edge ON THE BOARD.
                                 # The plate the board bolts to takes it the
                                 # rest of the way to 25 mm, which is what the
                                 # polarisation needs.
EDGE = 0.8                       # copper pull-back from the routed edge;
                                 # fabricators ask 0.3, so this is generous

TERM_NET = {"R1": "TX1", "R2": "TX2", "R3": "RX1", "R4": "RX2",
            "D1": "RX1", "D2": "RX2"}


def element_extent():
    """How much room one element needs, measured off the geometry itself."""
    from element2 import Element
    from geom import polys_bbox
    el = Element(CFG)
    bb = polys_bbox(el.build())
    half = CFG["tuned"]["L"] / 2.0
    return dict(below=-bb[1] - half,      # network hanging under the patch
                above=bb[3] - half,
                left=-bb[0] - half,
                right=bb[2] - half,
                feed=-el.input_pt[0] - half,   # where the launch lands
                half=half)


def plan():
    """Where everything goes, derived from the element's real extent.

    The board is not set by the patch and its margin alone: the feed network
    hangs 26 mm below the patch centre and reaches 18 mm to one side, and all
    of that has to be on the board too.  Whichever demand is larger wins.
    """
    e = element_extent()
    h, EC = e["half"], EDGE
    # sideways: the first element's network goes left, the mirrored one's
    # goes right, so the pair is bracketed by network on both flanks
    # The second element is MIRRORED, so its network reaches right by exactly
    # what the first one reaches left.  Using the unmirrored extent for both
    # sides leaves the mirrored network hanging off the board.
    # Three demands on the flank, and the largest wins: the mirrored
    # element's network, the patch's ground margin, and the connector -- whose
    # coplanar ground pads need 5.6 mm either side of where the feed lands.
    reach = max(e["left"], e["right"])
    LAUNCH = 5.6 + 0.3
    left_need = max(reach + h + EC, h + MARGIN, e["feed"] + h + LAUNCH)
    right_need = left_need
    cx1 = left_need
    W = cx1 + PITCH + right_need
    # downward: the feed leaves the network's lowest point and runs to the
    # edge, so the patch centre has to clear that plus a little launch room
    cy = max(e["below"] + h + 5.95, e["feed"] + h + LAUNCH)
    H = cy + h + MARGIN
    return dict(tx=(round(W, 3), round(H, 3), round(cx1, 4), round(cy, 4)),
                rx=(round(H, 3), round(W, 3), round(cy, 4), round(cx1, 4)),
                ext=e)


def _mounts(W, H, occupied):
    """Bolt holes wherever the copper leaves room.

    Corners first, then the middle of each edge.  Two holes along one edge is
    not a mounting -- the board pivots about the line between them -- so the
    search widens until it has at least three that are not collinear.
    """
    r = MOUNT_PAD / 2 + 0.4
    lo, hi_x, hi_y = r + 0.6, W - r - 0.6, H - r - 0.6
    # Search the perimeter rather than nine fixed spots.  Once the patches
    # became a keep-out the fixed grid found only three places, and three
    # bolts is not enough to hold a board that has two connectors being
    # pushed onto it.
    band = 9.0
    cand = []
    x = lo
    while x <= hi_x + 1e-9:
        y = lo
        while y <= hi_y + 1e-9:
            edge = min(x - lo, hi_x - x, y - lo, hi_y - y)
            if edge <= band:
                cand.append((x, y, edge))
            y += 1.0
        x += 1.0
    free = [(x, y, e) for x, y, e in cand
            if not any(a - r < x < c + r and b - r < y < d + r
                       for a, b, c, d in occupied)]
    if not free:
        return []
    # Hug the edge, then spread: take the most edge-hugging point, then
    # repeatedly the free point farthest from everything already chosen.
    free.sort(key=lambda t: (t[2], -(t[0] + t[1])))
    out = [(free[0][0], free[0][1])]
    while len(out) < 6:
        best, bd = None, 0.0
        for x, y, e in free:
            d = min(math.hypot(x - ox, y - oy) for ox, oy in out)
            if d > bd:
                best, bd = (x, y), d
        if best is None or bd < 3 * r:
            break
        out.append(best)
    return [(round(x, 3), round(y, 3), MOUNT_D, MOUNT_PAD) for x, y in out]


def build(kind, grow=(0.0, 0.0, 0.0, 0.0)):
    """grow = (left, right, bottom, top) extra millimetres, so the board can
    be told to contain copper the plan did not foresee."""
    P = plan()
    W, H, p1, p2 = P["tx"] if kind == "TX" else P["rx"]
    gl, gr, gb, gt = grow
    W += gl + gr
    H += gb + gt
    p1 += gl
    p2 += gb
    b = A.Board(CFG, CFG["launch"]["gap_mm"])
    b.BH = H
    w50 = CFG["feed"]["w50_mm"]
    if kind == "TX":
        for i, xc in enumerate((p1, p1 + PITCH)):
            _, ip, iso = b.add_element(f"TX{i+1}", xc, p2, rx=False,
                                       mirror=(i == 1))
            pt = b.feed_to_edge(ip, "bottom", w50, f"TX{i+1}")
            b.sma_launch(f"TX{i+1}", pt, "bottom", w50)
            b.termination(f"R{i+1}", iso, "bottom", w50, f"TX{i+1}")
    else:
        for i, yc in enumerate((p2, p2 + PITCH)):
            _, ip, iso = b.add_element(f"RX{i+1}", p1, yc, rx=True,
                                       mirror=(i == 1))
            pt = b.feed_to_edge(ip, "left", w50, f"RX{i+1}")
            b.sma_launch(f"RX{i+1}", pt, "left", w50)
            b.termination(f"R{i+3}", iso, "left", w50, f"RX{i+1}")
            b.protection_site(f"D{i+1}", A.PROT_X, ip[1], w50)

    # mask open over every radiating face: resist on a patch detunes it
    for e in b.elements:
        x0, y0, x1, y1 = e["bbox"]
        tx, ty = e["term"]
        # Start clear of the little windows over the terminating and
        # protection parts.  At 0.6 mm this opening grazed the protection
        # window by five hundredths of a millimetre, which fuses the two into
        # one opening exposing both the part's ground pad and the patch feed.
        if e["hand"] == "RHCP":
            b.mask.append(rect(x0 - 1.0, ty + 1.05, x1 + 1.0, y1 + 1.0))
        else:
            b.mask.append(rect(tx + 1.05, y0 - 1.0, x1 + 1.0, y1 + 1.0))
    # The launch needs bare copper on the signal line AND on the ground
    # either side of it, because the connector solders to all three.  One big
    # window does that but leaves nothing between them, so a solder bridge is
    # only a whisker away and every checker flags it.  Three windows with a
    # strip of resist left standing in each gap solder exactly the same and
    # cannot bridge.  The gaps are 0.97 mm, so the strips are a comfortable
    # 0.39 mm.
    g, hw = CFG["launch"]["gap_mm"], w50 / 2.0
    sig, gnd = hw + 0.3 * g, hw + 0.7 * g
    for n, x, y, side in b.ports:
        if side == "bottom":
            b.mask.append(rect(x - sig, 0.0, x + sig, 7.0))
            b.mask.append(rect(x - 7.0, 0.0, x - gnd, 7.0))
            b.mask.append(rect(x + gnd, 0.0, x + 7.0, 7.0))
        else:
            # stop at 7 mm, same as the transmit edge, so this window
            # never runs into the element's own opening and makes one
            # big hole exposing the launch grounds and the patch feed
            b.mask.append(rect(0.0, y - sig, 7.0, y + sig))
            b.mask.append(rect(0.0, y - 7.0, 7.0, y - gnd))
            b.mask.append(rect(0.0, y + gnd, 7.0, y + 7.0))

    occupied = []
    for p in b.top + b.gnd_top:
        xs = [q[0] for q in p]; ys = [q[1] for q in p]
        occupied.append((min(xs) - 0.8, min(ys) - 0.8,
                         max(xs) + 0.8, max(ys) + 0.8))
    # A bolt hole near a via is two holes too close together.  The keep-out
    # is both radii plus the fabricator's hole-to-hole rule, not the via's
    # pad alone.
    keep_v = MOUNT_PAD / 2 + 0.75 / 2 + 0.4
    for x, y, d, pad in b.vias:
        occupied.append((x - keep_v, y - keep_v, x + keep_v, y + keep_v))
    # Nothing gets punched through the ground in the band the patches occupy.
    # A hole there sits in the ground current that carries the coupling
    # between the two elements, and the last layout put one 0.4 mm above the
    # line of both patch edges, right between them.
    half = CFG["tuned"]["L"] / 2.0
    keep_p = MOUNT_PAD / 2 + 3.0
    for e in b.elements:
        cx, cy = e["centre"]
        occupied.append((cx - half - keep_p, cy - half - keep_p,
                         cx + half + keep_p, cy + half + keep_p))
    # Two fiducials, both on the connector edge and placed symmetrically
    # about the board's centre line.  They used to sit on opposite corners,
    # which put one of them 9 mm from a radiating patch edge with nothing
    # facing it on the other element -- so the two elements were not the
    # mirror images of each other that the beamforming assumes.  A fiducial
    # trails a 3.9 mm neck below the dot, so it needs 5 mm of board under it.
    # A fiducial's mask window is 2 mm across, wider than the dot, so it can
    # uncover a neighbouring track that the dot itself clears comfortably.
    # Place it against a keep-out grown to the window, not to the copper.
    occ_f = [(a - 1.5, b_ - 1.5, c + 1.5, d + 1.5)
             for a, b_, c, d in occupied]
    mid = (W if kind == "TX" else H) / 2.0
    placed = False
    for stand in (6.0, 7.5, 9.0, 10.5, 12.0):
        for off in [x / 2.0 for x in range(52, 23, -1)]:
            pair = [(mid - off, stand), (mid + off, stand)] if kind == "TX" \
                else [(stand, mid - off), (stand, mid + off)]
            if all(not any(a < fx < c and b_ < fy < d
                           for a, b_, c, d in occ_f) for fx, fy in pair):
                for fx, fy in pair:
                    b.fiducial(fx, fy)
                placed = True
                break
        if placed:
            break


    # Bolts last.  There are hundreds of places a bolt will go and only a
    # handful a fiducial will, so the fiducials choose first and the bolts
    # work around them.  The other way round put both transmit fiducials
    # inside mounting pads, where no camera can see them.
    keep_f = MOUNT_PAD / 2 + 2.0 / 2 + 0.25
    occ_m = list(occupied)
    for p in b.gnd_top[-6:]:
        xs = [q[0] for q in p]; ys = [q[1] for q in p]
        cx, cy = sum(xs) / len(xs), sum(ys) / len(ys)
        occ_m.append((cx - keep_f, cy - keep_f, cx + keep_f, cy + keep_f))
    for x, y, d, pad in _mounts(W, H, occ_m):
        b.mounts.append((x, y, d, pad))
    # Part labels come from the element builder marked for the top face, where
    # this board has almost no room that is not a radiating face.  Move them
    # all under, and drop any that would run off an edge.
    moved = []
    CLAMP_LABELS = True
    for lb in b.labels:
        x, y, txt, size = lb[0], lb[1], lb[2], lb[3]
        if 4.0 < x < W - 4.0 and 4.0 < y < H - 4.0:
            moved.append((x, y, txt, size, "silk_b"))
    b.labels = moved

    nm = "TRANSMIT" if kind == "TX" else "RECEIVE"
    hand = "RHCP  AZIMUTH" if kind == "TX" else "LHCP  ELEVATION"
    y0 = H - 4.0
    # The legend goes on the UNDERSIDE.  That face is one unbroken ground
    # plane with no mask openings on it at all, so nothing there can clash
    # with silkscreen -- while the top is mostly radiating faces that must
    # stay bare.
    # Put the legend where there is actually room: the bolt holes are placed
    # first and one of them may be in the middle, so hunt for the clearest
    # horizontal band rather than assuming the centre is free.
    ALL = [(f"5.8 GHz RADAR - {nm}", 1.6),
           (f"{hand}  d = 25.844 mm", 1.1),
           ("ZYF300CA-P 0.76 mm / 2L / Ag", 1.1),
           ("BOND TO GROUND PLATE", 1.1),
           ("25 mm PAST EVERY PATCH", 1.1),
           ("MOUNT >= 200 mm APART", 1.1),
           ("ABSORBER BETWEEN THE BOARDS", 1.1)]
    blocked = [(y - MOUNT_PAD / 2 - 1.5, y + MOUNT_PAD / 2 + 1.5)
               for x, y, d, pad in b.mounts if abs(x - W / 2) < W * 0.35]

    def fit(n):
        ls = ALL[:n]
        need = sum(l[1] + 1.6 for l in ls)
        yy = need / 2 + 2.0
        while yy < H - need / 2 - 2.0:
            lo, hi = yy - need / 2, yy + need / 2
            if not any(b0 < hi and b1 > lo for b0, b1 in blocked):
                return ls, yy, need
            yy += 0.5
        return None

    # Use as many lines as the clear band will take, dropping the least
    # important first.  A short board simply carries a shorter legend.
    got = None
    for n in (7, 6, 5, 4, 3, 2, 1):
        got = fit(n)
        if got:
            break
    if got:
        ls, yc, need = got
        y = yc + need / 2 - ls[0][1]
        for txt, size in ls:
            b.labels.append((W / 2, y, txt, size, "silk_b"))
            y -= size + 1.6
    return b, W, H


if __name__ == "__main__":
    P = plan()
    e = P["ext"]
    print(f"Element needs {e['below']:.2f} mm below the patch, "
          f"{e['left']:.2f} to the left\n")
    out = {}
    for kind in ("TX", "RX"):
        # Build once, see where the copper actually landed, then grow the
        # outline to contain it.  Cheaper and more reliable than predicting
        # every last ground return by hand.
        grow = [0.0, 0.0, 0.0, 0.0]
        for _ in range(4):
            b, W, H = build(kind, tuple(grow))
            # Only the launch TRACES are allowed to reach their edge.  Every
            # other piece of copper, ground returns included, has to be inside.
            lt = set(b.launch_traces)
            xs, ys = [], []
            for i, p in enumerate(b.top):
                if i in lt:
                    continue
                xs += [q[0] for q in p]; ys += [q[1] for q in p]
            for p in b.gnd_top:
                xs += [q[0] for q in p]; ys += [q[1] for q in p]
            for x, y, d, pad in b.vias:
                xs += [x - pad/2, x + pad/2]; ys += [y - pad/2, y + pad/2]
            need = [max(0.0, EDGE - min(xs)), max(0.0, EDGE - (W - max(xs))),
                    max(0.0, EDGE - min(ys)), max(0.0, EDGE - (H - max(ys)))]
            if all(n < 0.01 for n in need):
                break
            grow = [g + n for g, n in zip(grow, need)]
        b, W, H = build(kind, tuple(grow))
        import drc
        data = {"top": b.top, "gnd_top": b.gnd_top, "vias": b.vias,
                "mounts": b.mounts, "pads": b.pads, "top_net": b.top_net,
                "term_net": TERM_NET}
        viol, close = drc.check(data, W, H, edge_exempt=set(b.launch_traces))
        name = "transmit" if kind == "TX" else "receive"
        print(f"  {name:9} {W:6.2f} x {H:6.2f} mm   {W*H:6.0f} mm2   "
              f"{len(viol)} violations")
        for v in viol[:6]:
            print("      ", v)
        # Measured off THIS board.  It used to be copied out of the old
        # single-board file and patched in three places, so it still quoted a
        # 76 x 176 outline and the Rogers patch and transformer widths.
        from geom import polys_bbox
        from element2 import Element
        cbb = polys_bbox(b.top + b.gnd_top)
        el0 = Element(CFG)
        d = dict(outline=[W, H], top=b.top, top_net=b.top_net,
                 gnd_top=b.gnd_top, vias=b.vias, mounts=b.mounts,
                 mask=b.mask, ports=b.ports, labels=b.labels,
                 elements=b.elements, pads=b.pads, term_net=TERM_NET,
                 launch_gap=CFG["launch"]["gap_mm"], stack=CFG["stack"],
                 w50=CFG["feed"]["w50_mm"],
                 report=dict(
                     board=name,
                     outline_mm=[round(W, 4), round(H, 4)],
                     area_mm2=round(W * H, 1),
                     f0_GHz=CFG["f0_hz"] / 1e9,
                     lambda0_mm=round(CFG["lambda0_mm"], 4),
                     element_pitch_mm=round(PITCH, 4),
                     pitch_lambda=round(PITCH / CFG["lambda0_mm"], 4),
                     patch_Lx_mm=round(el0.Lx, 4),
                     patch_Ly_mm=round(el0.Ly, 4),
                     patch_centres=[[round(v, 4) for v in e["centre"]]
                                    for e in b.elements],
                     hands=[e["hand"] for e in b.elements],
                     transformer_z_ohm=el0.zq,
                     transformer_w_mm=round(el0.wq, 4),
                     w50_mm=round(CFG["feed"]["w50_mm"], 4),
                     lam_g50_mm=round(CFG["feed"]["lam_g50_mm"], 4),
                     ports=[{"name": p[0], "x": round(p[1], 4),
                             "y": round(p[2], 4), "edge": p[3]}
                            for p in b.ports],
                     n_vias=len(b.vias), n_mounts=len(b.mounts),
                     copper_bbox=[round(v, 3) for v in cbb],
                     substrate=CFG["substrate"]["name"],
                     stack=CFG["stack"]))
        json.dump(d, open(os.path.join(HERE, f"board_{name}.json"), "w"))
        out[name] = (W, H)
    tw, th = out["transmit"]; rw, rh = out["receive"]
    # Rotate the receive board back and STACK them: they are the same
    # rectangle in two orientations, so side by side was always the wrong
    # way to fold it and made the docstring's own claim look false.
    pw, ph = max(tw, rh), th + rw + 2.0
    print(f"\n  panelised stacked, 2 mm score: {pw:.2f} x {ph:.2f} mm"
          f"   {'FITS the 100 x 100 base price' if max(pw, ph) <= 100 else 'OVER'}")
    print(f"  total copper area {tw*th + rw*rh:.0f} mm2 against 13224 before "
          f"({100*(1-(tw*th+rw*rh)/13224):.0f} % less)")
