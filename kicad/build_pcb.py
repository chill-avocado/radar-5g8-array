"""Generate the KiCad PCB from board.json using KiCad's own pcbnew bindings.

Run with KiCad's bundled interpreter:
  /Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/\
Versions/3.9/bin/python3.9 build_pcb.py

Coordinates: the design frame has y pointing up with the origin at the bottom
left corner of the board; KiCad's PCB frame has y pointing down.  Mapping
(x, y) -> (x, BH - y) therefore reproduces the same physical board as seen
from the component side -- it is a change of convention, not a mirror, which
matters here because mirroring would reverse the circular polarisation.
"""

import json
import math
import os
import sys

import pcbnew

HERE = os.path.dirname(os.path.abspath(__file__))
DESIGN = os.path.abspath(os.path.join(HERE, "..", "design"))
VARIANT = sys.argv[1] if len(sys.argv) > 1 else "RO4350B"
# "RO4350B" is the original single board; "transmit" and "receive" are the
# two halves it was split into once the arrays moved onto separate boards.
SRC = os.path.join(DESIGN, {"RO4350B": "board.json"}.get(
    VARIANT, f"board_{VARIANT.lower()}.json"))

D = json.load(open(SRC))
BW, BH = D["outline"]
STACK = D.get("stack", {"h_mm": 0.762, "name": "Rogers RO4350B"})

OUT = os.path.join(HERE, "radar_5g8_" + VARIANT.lower()
                   if VARIANT == "RO4350B" else
                   "radar_5g8_" + VARIANT.lower() + "_array")
os.makedirs(OUT, exist_ok=True)
PCB = os.path.join(OUT, f"radar_5g8_{VARIANT.lower()}.kicad_pcb")


def MM(v):
    return pcbnew.FromMM(float(v))


def P(x, y):
    """Design (x, y) in mm -> KiCad VECTOR2I."""
    return pcbnew.VECTOR2I(MM(x), MM(BH - y))


board = pcbnew.BOARD()

# ------------------------------------------------------------------ stackup
ds = board.GetDesignSettings()
ds.SetCopperLayerCount(2)
ds.SetBoardThickness(MM(STACK["h_mm"] + 0.09))
ds.m_TrackMinWidth = MM(0.13)
ds.m_MinClearance = MM(0.13)
ds.m_ViasMinSize = MM(0.60)
ds.m_MinThroughDrill = MM(0.30)
ds.m_SolderMaskExpansion = MM(0.05)
# End-launch traces run to the routed edge by design.
ds.m_CopperEdgeClearance = MM(0.13)
board.SetDesignSettings(ds)

nets = {}
for n in ("GND", "TX1", "TX2", "RX1", "RX2"):
    ni = pcbnew.NETINFO_ITEM(board, n)
    board.Add(ni)
    nets[n] = ni

# Which element each polygon belongs to: array.py appends element copper in
# order, so tag by bounding box against the element records.
ELEM = D["report"]["ports"]


def net_for_point(x, y):
    best, bd = "GND", 1e9
    for e in D["elements"]:
        bb = e["bbox"]
        dx = max(bb[0] - x, 0, x - bb[2])
        dy = max(bb[1] - y, 0, y - bb[3])
        d = (dx * dx + dy * dy) ** 0.5
        if d < bd:
            bd, best = d, e["name"]
    # Anything further than a few mm from every element is ground.
    return best if bd < 3.0 else "GND"


# --------------------------------------------------------------- board edge
# Rounded corners: kinder in a bracket, and a square corner on a 0.76 mm
# laminate is the first thing to chip.
CR = 0.8                        # small boards with edge connectors: a big
                                # radius eats into the launch ground pads
for a, b in (((CR, 0), (BW - CR, 0)), ((BW, CR), (BW, BH - CR)),
             ((BW - CR, BH), (CR, BH)), ((0, BH - CR), (0, CR))):
    s = pcbnew.PCB_SHAPE(board, pcbnew.SHAPE_T_SEGMENT)
    s.SetStart(P(*a))
    s.SetEnd(P(*b))
    s.SetLayer(pcbnew.Edge_Cuts)
    s.SetWidth(MM(0.10))
    board.Add(s)
_k = CR / math.sqrt(2.0)
for (sx, sy), (mx, my), (ex, ey) in (
        ((0, CR), (CR - _k, CR - _k), (CR, 0)),
        ((BW - CR, 0), (BW - CR + _k, CR - _k), (BW, CR)),
        ((BW, BH - CR), (BW - CR + _k, BH - CR + _k), (BW - CR, BH)),
        ((CR, BH), (CR - _k, BH - CR + _k), (0, BH - CR))):
    s = pcbnew.PCB_SHAPE(board, pcbnew.SHAPE_T_ARC)
    # three points, so the sweep direction cannot be ambiguous once the
    # design frame's y-flip is applied
    s.SetArcGeometry(P(sx, sy), P(mx, my), P(ex, ey))
    s.SetLayer(pcbnew.Edge_Cuts)
    s.SetWidth(MM(0.10))
    board.Add(s)


def add_poly(pts, layer, net=None):
    s = pcbnew.PCB_SHAPE(board, pcbnew.SHAPE_T_POLY)
    v = pcbnew.VECTOR_VECTOR2I()
    for x, y in pts:
        v.append(P(x, y))
    s.SetPolyPoints(v)
    s.SetLayer(layer)
    s.SetFilled(True)
    s.SetWidth(0)
    board.Add(s)
    if net is not None:
        s.SetNet(net)
    return s


# ------------------------------------------------------------- bottom ground
# One solid pour, pulled back 0.25 mm from the routed edge.  A continuous
# ground plane directly under every patch and every feed line is what makes
# the microstrip impedances real; it is never broken anywhere on this board.
m = 0.25
rg = CR - m                       # the pour has to follow the rounded edge
_gp = []
for cx, cy, a0 in ((BW - CR, CR, -90.0), (BW - CR, BH - CR, 0.0),
                   (CR, BH - CR, 90.0), (CR, CR, 180.0)):
    for i in range(13):
        t = math.radians(a0 + 90.0 * i / 12.0)
        _gp.append((cx + rg * math.cos(t), cy + rg * math.sin(t)))
add_poly(_gp, pcbnew.B_Cu, nets["GND"])

# -------------------------------------------------------------- top copper
# Fuse everything on one net into one shape before it goes down.
#
# The design draws each element as a couple of dozen overlapping rectangles
# -- patch, transformers, ring arms, the two routes -- which is the natural
# way to describe it but leaves KiCad holding two dozen separate objects that
# merely happen to touch.  Every mask opening over a radiating face then
# looks like an opening across two dozen unrelated things, and the checker
# says so, 33 times on one board.  Fusing them first is also simply a better
# Gerber: one outline per net instead of a pile of overlapping edges.
# A true union, holes and all.  The coupler is a RING, so its merged shape
# has a hole in the middle; keeping only outer outlines would fill it in and
# turn the hybrid into a solid block of copper.  The area check below is what
# catches that if it ever happens again.
def _merge(polys):
    acc = pcbnew.SHAPE_POLY_SET()
    for pts in polys:
        one = pcbnew.SHAPE_POLY_SET()
        c = pcbnew.SHAPE_LINE_CHAIN()
        for x, y in pts:
            c.Append(P(x, y))
        c.SetClosed(True)
        one.AddOutline(c)
        acc.BooleanAdd(one)
    acc.Simplify()
    return acc


def _area(polys):
    a = 0.0
    for p in polys:
        s = 0.0
        for i in range(len(p)):
            x1, y1 = p[i]
            x2, y2 = p[(i + 1) % len(p)]
            s += x1 * y2 - x2 * y1
        a += abs(s) / 2.0
    return a


_by_net = {}
for poly in D["top"]:
    cx = sum(p[0] for p in poly) / len(poly)
    cy = sum(p[1] for p in poly) / len(poly)
    _by_net.setdefault(net_for_point(cx, cy), []).append(poly)
_by_net.setdefault("GND", []).extend(D["gnd_top"])

_merged = 0
for _name, _polys in _by_net.items():
    ps = _merge(_polys)
    got = ps.Area() / 1e12                      # nm^2 -> mm^2
    loose = _area(_polys)
    if got > loose + 0.05:
        raise SystemExit(
            f"merge on net {_name} GAINED copper: {got:.3f} mm2 against at "
            f"most {loose:.3f} loose -- a hole has been filled in")
    # One shape per connected PIECE, not one shape for the whole net.  A
    # single shape holding a dozen islands is not what a polygon means, and
    # KiCad stops seeing the pads inside it as connected to anything.
    for i in range(ps.OutlineCount()):
        one = pcbnew.SHAPE_POLY_SET()
        one.AddOutline(ps.Outline(i))
        for j in range(ps.HoleCount(i)):
            one.AddHole(ps.Hole(i, j), 0)
        s = pcbnew.PCB_SHAPE(board, pcbnew.SHAPE_T_POLY)
        s.SetPolyShape(one)
        s.SetLayer(pcbnew.F_Cu)
        s.SetFilled(True)
        s.SetWidth(0)
        board.Add(s)
        s.SetNet(nets[_name])
        _merged += 1

# ---------------------------------------------------- back face, bare metal
# The board bolts face-down onto an aluminium ground plate, and the bond that
# matters is the whole back face pressing against it -- far better than any
# bolt.  With the back fully resisted it was pressing MASK against the plate
# instead, which is neither the electrical joint the design assumes nor a flat
# one.  Open it, keeping a 1 mm border so the mask still seals the routed edge.
_bm = 1.0
_bp = []
_rb = CR - _bm
for cx, cy, a0 in ((BW - CR, CR, -90.0), (BW - CR, BH - CR, 0.0),
                   (CR, BH - CR, 90.0), (CR, CR, 180.0)):
    for i in range(13):
        t = math.radians(a0 + 90.0 * i / 12.0)
        _bp.append((cx + _rb * math.cos(t), cy + _rb * math.sin(t)))

# ------------------------------------------------------- solder mask windows
# The windows over the radiating faces are deliberately wide open -- resist
# on a patch detunes it.  But a bolt pad sitting inside one of them is bare
# metal on a different net in the same opening, which is a bridge waiting for
# a washer.  So the resist is put back as a ring around every bolt.
_mask = pcbnew.SHAPE_POLY_SET()
for poly in D.get("mask", []):
    _c = pcbnew.SHAPE_LINE_CHAIN()
    for x, y in poly:
        _c.Append(P(x, y))
    _c.SetClosed(True)
    _one = pcbnew.SHAPE_POLY_SET()
    _one.AddOutline(_c)
    _mask.BooleanAdd(_one)
# Same for component pads.  A hand-drawn window is for bare copper, never
# for a part: the parts carry their own openings with resist in between, and
# a big window swallowing one exposes its two nets in a single hole.
for _pd in D.get("pads", []):
    _px, _py, _pw, _ph = _pd[1], _pd[2], _pd[3], _pd[4]
    _keep = pcbnew.SHAPE_POLY_SET()
    _keep.NewOutline()
    for _cx, _cy in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
        _keep.Append(P(_px + _cx * (_pw / 2 + 0.35),
                       _py + _cy * (_ph / 2 + 0.35)))
    _mask.BooleanSubtract(_keep)
for _mx, _my, _md, _mp in D.get("mounts", []):
    _ring = pcbnew.SHAPE_POLY_SET()
    _ring.NewOutline()
    _r = _mp / 2.0 + 0.35
    for _k in range(48):
        _t = 2 * math.pi * _k / 48
        _ring.Append(P(_mx + _r * math.cos(_t), _my + _r * math.sin(_t)))
    _mask.BooleanSubtract(_ring)
_mask.Simplify()
for _i in range(_mask.OutlineCount()):
    _p = pcbnew.SHAPE_POLY_SET()
    _p.AddOutline(_mask.Outline(_i))
    for _j in range(_mask.HoleCount(_i)):
        _p.AddHole(_mask.Hole(_i, _j), 0)
    _s = pcbnew.PCB_SHAPE(board, pcbnew.SHAPE_T_POLY)
    _s.SetPolyShape(_p)
    _s.SetLayer(pcbnew.F_Mask)
    _s.SetFilled(True)
    _s.SetWidth(0)
    board.Add(_s)

# -------------------------------------------------------------------- vias
for x, y, drill, pad in D["vias"]:
    v = pcbnew.PCB_VIA(board)
    v.SetPosition(P(x, y))
    v.SetDrill(MM(drill))
    v.SetWidth(MM(pad))
    v.SetViaType(pcbnew.VIATYPE_THROUGH)
    v.SetLayerPair(pcbnew.F_Cu, pcbnew.B_Cu)
    v.SetNetCode(nets["GND"].GetNetCode())
    board.Add(v)

# ---------------------------------------------------------- footprints: SMA
def sma_footprint(ref, x, y, side, w, gap):
    fp = pcbnew.FOOTPRINT(board)
    fp.SetReference(ref)
    fp.SetValue("SMA-EDGE-50R")
    edge = "BottomEdge" if side == "bottom" else "LeftEdge"
    fp.SetFPID(pcbnew.LIB_ID("radar5g8", f"SMA_EndLaunch_50R_{edge}"))
    fp.SetPosition(P(x, y))
    fp.SetLayer(pcbnew.F_Cu)
    board.Add(fp)

    def mkpad(name, dx, dy, pw, ph, net):
        p = pcbnew.PAD(fp)
        p.SetNumber(name)
        p.SetAttribute(pcbnew.PAD_ATTRIB_SMD)
        p.SetShape(pcbnew.PAD_SHAPE_RECT)
        p.SetSize(pcbnew.VECTOR2I(MM(pw), MM(ph)))
        p.SetLayerSet(pcbnew.PAD.SMDMask())
        p.SetPosition(P(x + dx, y + dy))
        p.SetNetCode(net.GetNetCode())
        fp.Add(p)

    sig = nets[ref]
    if side == "bottom":
        mkpad("1", 0.0, 1.30, w, 2.30, sig)
        mkpad("2", -(w / 2 + gap + 1.5), 1.60, 2.60, 2.90, nets["GND"])
        mkpad("3", +(w / 2 + gap + 1.5), 1.60, 2.60, 2.90, nets["GND"])
    else:
        mkpad("1", 1.30, 0.0, 2.30, w, sig)
        mkpad("2", 1.60, -(w / 2 + gap + 1.5), 2.90, 2.60, nets["GND"])
        mkpad("3", 1.60, +(w / 2 + gap + 1.5), 2.90, 2.60, nets["GND"])
    # No SetOrientationDegrees: the pads above are already placed in absolute
    # board coordinates for this edge, so rotating the footprint as well would
    # transform them twice and drop the ground pads onto the signal trace.
    return fp


w50 = D["w50"]
gap = D["launch_gap"]
for name, x, y, side in D["ports"]:
    sma_footprint(name, x, y, side, w50, gap)

# --------------------------------------------------- footprints: 0402 loads
def res_footprint(ref, pads):
    """One 0402 land pattern, rotated to suit, rather than two mirrored ones.

    Pads are given RELATIVE to the footprint (SetPos0) and the rotation then
    places them, which is what a pick-and-place file needs: a single library
    part with an angle, not two differently-drawn parts both claiming zero
    degrees.
    """
    (n0, x0, y0, w0, h0, net0), (n1, x1, y1, w1, h1, net1) = pads
    cx_, cy_ = (x0 + x1) / 2.0, (y0 + y1) / 2.0
    horiz = abs(x1 - x0) > abs(y1 - y0)

    fp = pcbnew.FOOTPRINT(board)
    fp.SetReference(ref)
    fp.SetValue("DNF - limiter site" if ref.startswith("D") else "50R")
    # Name it after the land it actually is.  It was hard-coded "0402" while
    # the land grew to 0805, which is the sort of label that gets a 0402 part
    # ordered for a site that has to dissipate a watt.
    _sz = "0805" if max(w0, h0) > 1.0 else "0402"
    base = (f"D_{_sz}_Limiter" if ref.startswith("D")
            else f"R_{_sz}_1W_50R" if _sz == "0805" else "R_0402_50R")
    fp.SetFPID(pcbnew.LIB_ID("radar5g8", base))
    fp.SetPosition(P(cx_, cy_))
    fp.SetLayer(pcbnew.F_Cu)
    if ref.startswith("D"):
        fp.SetDNP(True)              # limiter sites ship empty
    fp.SetAttributes(pcbnew.FP_SMD)  # so it reaches the pick-and-place file
    board.Add(fp)

    sep = MM(abs((x1 - x0) if horiz else (y1 - y0)) / 2.0)
    for i, (nm, xx, yy, pw, ph, nn) in enumerate(pads):
        p = pcbnew.PAD(fp)
        p.SetNumber(str(i + 1))
        p.SetAttribute(pcbnew.PAD_ATTRIB_SMD)
        p.SetShape(pcbnew.PAD_SHAPE_RECT)
        # The land size comes from the design, not from a constant.  This
        # was pinned at 0.62 mm, so growing the terminating resistor to an
        # 0805 that can take the reflected watt never reached the board.
        p.SetSize(pcbnew.VECTOR2I(MM(pw), MM(ph)))
        p.SetLayerSet(pcbnew.PAD.SMDMask())
        # unrotated: pads sit either side of the origin along x
        lead = (xx > cx_) if horiz else (yy > cy_)
        p.SetFPRelativePosition(pcbnew.VECTOR2I(sep if lead else -sep, 0))
        net = nets["GND"] if nn == "GND" else nets[D["term_net"][ref]]
        p.SetNetCode(net.GetNetCode())
        fp.Add(p)
    # KiCad rotates the pads about the origin from their Pos0 when the
    # orientation is applied, so this must come after they are added.
    fp.SetOrientationDegrees(0.0 if horiz else 90.0)
    return fp


byref = {}
for ref, x, y, pw, ph, nn in D["pads"]:
    byref.setdefault(ref[:-1], []).append((ref, x, y, pw, ph, nn))
for ref, pads in sorted(byref.items()):
    res_footprint(ref, pads)

# ---------------------------------------------------------- mounting holes
for i, (x, y, drill, pad) in enumerate(D["mounts"]):
    fp = pcbnew.FOOTPRINT(board)
    fp.SetReference(f"H{i+1}")
    fp.SetValue("M3")
    fp.SetFPID(pcbnew.LIB_ID("radar5g8", "MountingHole_M3"))
    fp.SetPosition(P(x, y))
    board.Add(fp)
    p = pcbnew.PAD(fp)
    p.SetNumber("1")
    p.SetAttribute(pcbnew.PAD_ATTRIB_PTH)
    p.SetShape(pcbnew.PAD_SHAPE_CIRCLE)
    p.SetSize(pcbnew.VECTOR2I(MM(pad), MM(pad)))
    p.SetDrillSize(pcbnew.VECTOR2I(MM(drill), MM(drill)))
    p.SetLayerSet(pcbnew.PAD.PTHMask())
    p.SetPosition(P(x, y))
    p.SetNetCode(nets["GND"].GetNetCode())
    fp.Add(p)

# ----------------------------------------------------------------- silkscreen
for x, y, text, size, layer in D["labels"]:
    t = pcbnew.PCB_TEXT(board)
    t.SetText(text)
    t.SetPosition(P(x, y))
    if layer == "silk_b":
        t.SetLayer(pcbnew.B_SilkS)
        t.SetMirrored(True)
    else:
        t.SetLayer(pcbnew.F_SilkS)
    t.SetTextSize(pcbnew.VECTOR2I(MM(size * 0.72), MM(size)))
    t.SetTextThickness(MM(size * 0.15))
    t.SetHorizJustify(pcbnew.GR_TEXT_H_ALIGN_CENTER)
    board.Add(t)

# Every footprint needs a courtyard, or KiCad cannot check that two parts do
# not physically collide.  Draw one round each footprint's own pads.
for fp in board.GetFootprints():
    xs, ys = [], []
    for p in fp.Pads():
        pos, sz = p.GetPosition(), p.GetSize()
        xs += [pos.x - sz.x / 2, pos.x + sz.x / 2]
        ys += [pos.y - sz.y / 2, pos.y + sz.y / 2]
    if not xs:
        continue
    m = MM(0.25)
    cy = pcbnew.PCB_SHAPE(fp, pcbnew.SHAPE_T_RECTANGLE)
    cy.SetStart(pcbnew.VECTOR2I(int(min(xs) - m), int(min(ys) - m)))
    cy.SetEnd(pcbnew.VECTOR2I(int(max(xs) + m), int(max(ys) + m)))
    cy.SetLayer(pcbnew.F_CrtYd)
    cy.SetWidth(MM(0.05))
    cy.SetFilled(False)
    fp.Add(cy)

# Hide the auto-placed reference/value fields: this board carries its own
# silkscreen legend, and KiCad's default fields land on bare copper and over
# the board edge.
for fp in board.GetFootprints():
    fp.Reference().SetVisible(False)
    fp.Value().SetVisible(False)

pcbnew.SaveBoard(PCB, board)

# Write the physical stackup into the board file.  Without it KiCad puts its
# defaults into the Gerber job file -- and both PCBWay and JLCPCB read that
# file to pre-fill the order, so it would have asked for FR-4 with no plating:
# the wrong laminate and bare copper on an unmasked antenna board.
FIN = STACK.get("finish", "ENIG")
STACKUP = f"""\t\t(stackup
\t\t\t(layer "F.SilkS" (type "Top Silk Screen"))
\t\t\t(layer "F.Paste" (type "Top Solder Paste"))
\t\t\t(layer "F.Mask" (type "Top Solder Mask") (thickness 0.01))
\t\t\t(layer "F.Cu" (type "copper") (thickness 0.035))
\t\t\t(layer "dielectric 1" (type "core") (thickness {STACK['h_mm']}) \
(material "{STACK['name']}") (epsilon_r {STACK['er']}) \
(loss_tangent {STACK['tand']}))
\t\t\t(layer "B.Cu" (type "copper") (thickness 0.035))
\t\t\t(layer "B.Mask" (type "Bottom Solder Mask") (thickness 0.01))
\t\t\t(layer "B.Paste" (type "Bottom Solder Paste"))
\t\t\t(layer "B.SilkS" (type "Bottom Silk Screen"))
\t\t\t(copper_finish "{FIN}")
\t\t\t(dielectric_constraints no)
\t\t)
"""
txt = open(PCB).read()
key = "\t(setup\n"
assert key in txt, "could not find the setup block"
txt = txt.replace(key, key + STACKUP, 1)
open(PCB, "w").write(txt)
print("stackup written:", STACK["name"], STACK["h_mm"], "mm,", FIN)
print("wrote", PCB)
# Write the footprint library from CANONICAL, unrotated copies of what the
# board contains, using KiCad's own writer.  Hand-extracting the s-expression
# did not work: every termination instance on the transmit board sits at 90
# degrees, so the extracted part carried that rotation baked into its pads and
# matched nothing.  Un-rotating a clone gives the part the board is actually
# an instance of.
_lib = os.path.join(OUT, "radar5g8.pretty")
if os.path.isdir(_lib):
    for _f in os.listdir(_lib):
        os.remove(os.path.join(_lib, _f))
else:
    os.makedirs(_lib, exist_ok=True)
_io = pcbnew.PCB_IO_MGR.FindPlugin(pcbnew.PCB_IO_MGR.KICAD_SEXP)
_seen = set()
for _fp in board.GetFootprints():
    _nm = str(_fp.GetFPID().GetLibItemName())
    if _nm in _seen:
        continue
    _seen.add(_nm)
    _ang, _pos = _fp.GetOrientationDegrees(), _fp.GetPosition()
    _nets = [(_p, _p.GetNetCode()) for _p in _fp.Pads()]
    _fp.SetOrientationDegrees(0)
    _fp.SetPosition(pcbnew.VECTOR2I(0, 0))
    for _p, _n in _nets:
        _p.SetNetCode(0)
    _io.FootprintSave(_lib, _fp)
    _fp.SetPosition(_pos)                       # and put it straight back
    _fp.SetOrientationDegrees(_ang)
    for _p, _n in _nets:
        _p.SetNetCode(_n)
print(f"  footprint library: {len(_seen)} canonical parts written from the board")

# Open the back in clean bands either side of the legend, rather than
# cutting round every character.  Subtracting each glyph's bounding box left
# nothing at all: eight generous boxes spread across the face union to cover
# it.  Ink behind a solid ground plane is RF-harmless -- the only thing that
# matters here is metal meeting the plate, so give it whole bands.
_lab = [l for l in D.get("labels", []) if len(l) > 4 and l[4] == "silk_b"]
if _lab:
    _ly = [l[1] for l in _lab]
    _lo, _hi = min(_ly) - 2.2, max(_ly) + 2.6
else:
    _lo = _hi = None
_open = []
_bm = 1.0
for _y0, _y1 in ([(_bm, _lo), (_hi, BH - _bm)] if _lo is not None
                 else [(_bm, BH - _bm)]):
    if _y1 - _y0 < 3.0:
        continue
    _open.append([(_bm, _y0), (BW - _bm, _y0), (BW - _bm, _y1), (_bm, _y1)])
_bare = 0.0
for _p in _open:
    add_poly(_p, pcbnew.B_Mask)
    _bare += abs(_p[1][0] - _p[0][0]) * abs(_p[2][1] - _p[1][1])
print(f"  back face: {len(_open)} opening(s), {_bare:.0f} mm2 bare copper "
      f"({100*_bare/(BW*BH):.0f}% of the board) clamping to the plate")

print(f"  outline {BW} x {BH} mm, {len(D['top'])} signal polys, "
      f"{len(D['gnd_top'])} ground polys, {len(D['vias'])} vias, "
      f"{len(D['mounts'])} mounting holes")
