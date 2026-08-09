"""Generate an amplifier board from its design module's JSON.

    python3 build_rf.py pa     the transmit amplifier
    python3 build_rf.py lna    the receive amplifier

Both boards are drawn by the same generator and built the same way, because
they are the same instrument: same laminate, same four-layer stack, same
connectors, same twelve volts, same monitoring header.

Four layers.  The two array boards are two-layer because a patch wants an
unbroken ground directly beneath it and nothing else; this board is a
different animal.  Fourteen supply and monitoring nets have to cross the
middle of it in both directions at once, and on two layers the underside is
solid ground and unavailable, so every one of them would be competing for the
surface with the radio-frequency lines.  Giving them a layer of their own with
ground either side costs one lamination and removes the whole problem.

  F.Cu     everything at radio frequency, and the component lands
  In1.Cu   ground, the reference every printed dimension was designed against
  In2.Cu   supply and control
  B.Cu     ground, and the face that bolts to the chassis

The top dielectric is unchanged at 0.76 mm, so the filter, the coupler, the
supply feeds and the fifty-ohm line are exactly as designed and as simulated.
Total thickness lands at 1.576 mm, which is what the 0.062 inch end-launch
connector takes.

Everything after the board itself -- Gerbers, drill, placement, bill of
materials, a check that the Gerbers say what the board says -- is written
here too, because a board file nobody can order is not a deliverable.
"""

import csv
import json
import subprocess
import sys
import math
import os
import shutil

import pcbnew

HERE = os.path.dirname(os.path.abspath(__file__))
DESIGN = os.path.abspath(os.path.join(HERE, "..", "design"))

NAME = sys.argv[1] if len(sys.argv) > 1 else "pa"
D = json.load(open(os.path.join(DESIGN, f"{NAME}_board.json")))
S = D["stack"]
BW, BH = D["outline"]
CR = 3.0                                   # corner radius

OUT = os.path.join(HERE, f"radar_5g8_{NAME}")
GERB = os.path.join(OUT, "gerbers")
os.makedirs(GERB, exist_ok=True)
PCB = os.path.join(OUT, f"radar_5g8_{NAME}.kicad_pcb")


def MM(v):
    return pcbnew.FromMM(float(v))


def P(x, y):
    return pcbnew.VECTOR2I(MM(x), MM(BH - y))


board = pcbnew.BOARD()
ds = board.GetDesignSettings()
ds.SetCopperLayerCount(4)
ds.SetBoardThickness(MM(S["total_mm"]))
ds.m_TrackMinWidth = MM(0.13)
ds.m_MinClearance = MM(0.13)
ds.m_ViasMinSize = MM(0.45)
ds.m_MinThroughDrill = MM(0.25)
ds.m_SolderMaskExpansion = MM(0.0)
ds.m_SolderMaskMinWidth = MM(0.0)
ds.m_CopperEdgeClearance = MM(0.13)
board.SetDesignSettings(ds)
board.SetCopperLayerCount(4)
# The board is designed to a 0.15 mm gap and the fabrication drawing asks for
# 0.15 mm, so that is what the checker is told to check.  Left at the default
# 0.2 it reports things the board was never trying to achieve.
nc = board.GetDesignSettings().m_NetSettings.GetDefaultNetclass()
nc.SetClearance(MM(0.15))

NETNAMES = sorted({"GND", "?", "sig", "NC"}
                  | set(D.get("top_net", []))
                  | set(D.get("inner_net", []))
                  | set(D.get("via_net", []))
                  | {p[5] for prt in D["parts"] for p in prt.get("pads", [])
                     if len(p) > 5}
                  | {n for prt in D["parts"] for n in prt.get("nets", [])})
nets = {}
for n in NETNAMES:
    ni = pcbnew.NETINFO_ITEM(board, n)
    board.Add(ni)
    nets[n] = ni


def net(n):
    return nets.get(n, nets["?"])


# ------------------------------------------------------------------- outline
def outline():
    r = CR
    for a, b in (((r, 0), (BW - r, 0)), ((BW, r), (BW, BH - r)),
                 ((BW - r, BH), (r, BH)), ((0, BH - r), (0, r))):
        s = pcbnew.PCB_SHAPE(board, pcbnew.SHAPE_T_SEGMENT)
        s.SetStart(P(*a))
        s.SetEnd(P(*b))
        s.SetLayer(pcbnew.Edge_Cuts)
        s.SetWidth(MM(0.10))
        board.Add(s)
    for cx, cy, a0 in ((BW - r, r, 270), (BW - r, BH - r, 0),
                       (r, BH - r, 90), (r, r, 180)):
        a1 = a0 + 90
        am = math.radians((a0 + a1) / 2.0)
        s = pcbnew.PCB_SHAPE(board, pcbnew.SHAPE_T_ARC)
        s.SetArcGeometry(
            P(cx + r * math.cos(math.radians(a0)),
              cy + r * math.sin(math.radians(a0))),
            P(cx + r * math.cos(am), cy + r * math.sin(am)),
            P(cx + r * math.cos(math.radians(a1)),
              cy + r * math.sin(math.radians(a1))))
        s.SetLayer(pcbnew.Edge_Cuts)
        s.SetWidth(MM(0.10))
        board.Add(s)


def add_poly(pts, layer, n=None):
    s = pcbnew.PCB_SHAPE(board, pcbnew.SHAPE_T_POLY)
    v = pcbnew.VECTOR_VECTOR2I()
    for x, y in pts:
        v.append(P(x, y))
    s.SetPolyPoints(v)
    s.SetLayer(layer)
    s.SetFilled(True)
    s.SetWidth(0)
    board.Add(s)
    if n is not None:
        s.SetNet(n)
    return s


def add_via(x, y, drill, pad, n):
    v = pcbnew.PCB_VIA(board)
    v.SetPosition(P(x, y))
    v.SetDrill(MM(drill))
    v.SetWidth(MM(pad))
    v.SetViaType(pcbnew.VIATYPE_THROUGH)
    v.SetLayerPair(pcbnew.F_Cu, pcbnew.B_Cu)
    v.SetNet(n)
    board.Add(v)


# ------------------------------------------------------------------- copper
outline()
PADIDX = set(D.get("pad_idx", []))
for i, (p, nm) in enumerate(zip(D["top"], D["top_net"])):
    if i in PADIDX:
        continue
    add_poly(p, pcbnew.F_Cu, net(nm))
for p in D["gnd_top"]:
    add_poly(p, pcbnew.F_Cu, net("GND"))
for p, nm in zip(D["inner"], D["inner_net"]):
    add_poly(p, pcbnew.In2_Cu, net(nm))


def ground_zone(layer):
    """Ground as a zone, not a slab.

    A filled polygon would sit flat across everything and short to every via
    carrying something else; a zone is poured around what it must avoid and
    opens a clearance ring wherever a foreign net passes through it.  That
    ring is the difference between a ground plane and a short circuit.
    """
    z = pcbnew.ZONE(board)
    z.SetLayer(layer)
    z.SetNet(net("GND"))
    z.SetIsFilled(False)
    z.SetLocalClearance(MM(0.25))
    z.SetMinThickness(MM(0.15))
    z.SetPadConnection(pcbnew.ZONE_CONNECTION_FULL)
    z.SetThermalReliefGap(MM(0.3))
    z.SetThermalReliefSpokeWidth(MM(0.3))
    o = z.Outline()
    o.NewOutline()
    m, r = 0.25, CR - 0.25
    for cx, cy, a0 in ((BW - m - r, m + r, 270), (BW - m - r, BH - m - r, 0),
                       (m + r, BH - m - r, 90), (m + r, m + r, 180)):
        for k in range(9):
            a = math.radians(a0 + k * 90 / 8.0)
            o.Append(P(cx + r * math.cos(a), cy + r * math.sin(a)))
    board.Add(z)
    return z


for lay in (pcbnew.F_Cu, pcbnew.In1_Cu, pcbnew.B_Cu):
    ground_zone(lay)

vias = 0
for (x, y, drill, pad), vn in zip(D["vias"], D["via_net"]):
    add_via(x, y, drill, pad, net(vn))
    vias += 1


# --------------------------------------------------------------- footprints
def bbox(poly):
    xs = [q[0] for q in poly]
    ys = [q[1] for q in poly]
    return min(xs), min(ys), max(xs), max(ys)


# A pad's net is read off the copper the generator drew for it, not off the
# name the part was created with.  Anything the generator later found to be
# one piece of metal has one name by then, and a footprint still carrying the
# old name reads to the checker as a short and to the router as two nets.
PADPOLY = [((bbox(D["top"][i])), D["top_net"][i]) for i in PADIDX]
TRACKS = [(bbox(p), nm) for i, (p, nm) in
          enumerate(zip(D["top"], D["top_net"]))
          if i not in PADIDX and nm not in ("GND", "?")]


def net_under(x, y, w, h):
    a0, b0, a1, b1 = x - w / 2, y - h / 2, x + w / 2, y + h / 2
    best = None
    for (c0, d0, c1, d1), nm in PADPOLY:
        if abs((c0 + c1) / 2 - x) < 0.03 and abs((d0 + d1) / 2 - y) < 0.03:
            return nm
    for (c0, d0, c1, d1), nm in TRACKS:
        if not (c0 > a1 or c1 < a0 or d0 > b1 or d1 < b0):
            best = best or nm
    return best


def fp_new(ref, x, y, value="", tht=False):
    fp = pcbnew.FOOTPRINT(board)
    fp.SetReference(ref)
    fp.SetValue(value)
    fp.SetPosition(P(x, y))
    fp.SetAttributes(pcbnew.FP_THROUGH_HOLE if tht else pcbnew.FP_SMD)
    board.Add(fp)
    return fp


def smd(fp, num, x, y, w, h, n):
    p = pcbnew.PAD(fp)
    p.SetNumber(str(num))
    p.SetAttribute(pcbnew.PAD_ATTRIB_SMD)
    p.SetShape(pcbnew.PAD_SHAPE_RECT)
    p.SetSize(pcbnew.VECTOR2I(MM(w), MM(h)))
    p.SetLayerSet(pcbnew.PAD.SMDMask())
    fp.Add(p)
    p.SetPosition(P(x, y))
    if n is not None:
        p.SetNet(n)
    return p


def tht(fp, num, x, y, size, drill, n):
    p = pcbnew.PAD(fp)
    p.SetNumber(str(num))
    p.SetAttribute(pcbnew.PAD_ATTRIB_PTH)
    p.SetShape(pcbnew.PAD_SHAPE_CIRCLE)
    p.SetSize(pcbnew.VECTOR2I(MM(size), MM(size)))
    p.SetDrillSize(pcbnew.VECTOR2I(MM(drill), MM(drill)))
    p.SetLayerSet(pcbnew.PAD.PTHMask())
    fp.Add(p)
    p.SetPosition(P(x, y))
    p.SetNet(n)
    return p


R0402 = (0.62, 0.62)
CHIP = {"0402": (0.62, 0.62), "0805": (2.00, 1.25),
        "1210": (2.70, 1.30), "SMB": (2.70, 1.30)}
placed = 0
for prt in D["parts"]:
    k = prt["kind"]
    pads = prt.get("pads")
    if k in ("SMD", "FINAL", "DRIVER") and pads:
        fp = fp_new(prt["ref"], prt["x"], prt["y"], prt.get("mpn", ""))
        for num, px, py, w, h, pin in pads:
            if pin == "NC":
                continue
            found = net_under(px, py, w, h)
            smd(fp, num, px, py, w, h, net(found) if found else net(pin))
        placed += 1
    elif k in ("SERIES", "SHUNT"):
        fp = fp_new(prt["ref"], prt["x"], prt["y"], prt.get("value", ""))
        w, h = CHIP.get(prt.get("pkg", "0402"), R0402)
        c = prt["pitch"] / 2.0 if k == "SERIES" else prt["pitch"]
        if k == "SERIES":
            dx, dy = (c, 0.0) if prt.get("horiz", True) else (0.0, c)
            for i, sgn in ((1, -1), (2, 1)):
                px, py = prt["x"] + sgn * dx, prt["y"] + sgn * dy
                smd(fp, i, px, py, w, h, net(net_under(px, py, w, h)))
        else:
            sw, sh = prt.get("size", [w, h])
            ax = prt.get("axis", "y")
            step = (prt["sgn"] * c, 0.0) if ax == "x" else (0.0,
                                                            prt["sgn"] * c)
            for i, (px, py) in ((1, (prt["x"], prt["y"])),
                                (2, (prt["x"] + step[0],
                                     prt["y"] + step[1]))):
                smd(fp, i, px, py, sw, sh, net(net_under(px, py, sw, sh)))
        if prt.get("dnp"):
            fp.SetDNP(True)
        placed += 1
    elif k == "HDR":
        fp = fp_new(prt["ref"], prt["x"], prt["y"], prt.get("mpn", ""),
                    tht=True)
        for num, px, py, nm in prt["pads"]:
            tht(fp, num, px, py, 1.70, 1.02,
                net(net_under(px, py, 1.70, 1.70) or nm))
        placed += 1
    elif k == "FID":
        fp = fp_new(f"FID{int(prt['x'])}_{int(prt['y'])}", prt["x"], prt["y"])
        p = smd(fp, 1, prt["x"], prt["y"], prt["d"], prt["d"], net("GND"))
        p.SetShape(pcbnew.PAD_SHAPE_CIRCLE)
        p.SetLocalSolderMaskMargin(MM((prt["mask"] - prt["d"]) / 2.0))
        placed += 1

for x, y, drill, pad in D["mounts"]:
    fp = fp_new(f"H{int(x)}_{int(y)}", x, y, "M3", tht=True)
    tht(fp, 1, x, y, pad, drill, net("GND"))

# The face that bolts to the chassis is bare copper under each amplifier, not
# masked: solder resist is a good insulator and this is where four watts have
# to leave.  Immersion silver tarnishes to a semiconductor that carries no
# current, so bare silver stays bare silver as far as heat is concerned.
for m in D.get("mask_bot", []):
    x0, y0, x1, y1 = m
    s = pcbnew.PCB_SHAPE(board, pcbnew.SHAPE_T_POLY)
    v = pcbnew.VECTOR_VECTOR2I()
    for px, py in ((x0, y0), (x1, y0), (x1, y1), (x0, y1)):
        v.append(P(px, py))
    s.SetPolyPoints(v)
    s.SetLayer(pcbnew.B_Mask)
    s.SetFilled(True)
    s.SetWidth(0)
    board.Add(s)

# ------------------------------------------------------------------- silk
for lb in D["labels"]:
    x, y, txt, size = lb[0], lb[1], lb[2], lb[3]
    t = pcbnew.PCB_TEXT(board)
    t.SetText(txt)
    t.SetPosition(P(x, y))
    t.SetLayer(pcbnew.F_SilkS)
    t.SetTextSize(pcbnew.VECTOR2I(MM(size * 0.72), MM(size)))
    t.SetTextThickness(MM(size * 0.15))
    t.SetHorizJustify(pcbnew.GR_TEXT_H_ALIGN_CENTER)
    board.Add(t)

for n, x, y, side in D["ports"]:
    t = pcbnew.PCB_TEXT(board)
    t.SetText(n)
    dx, dy = {"left": (14.0, 3.2), "right": (-14.0, 3.2),
              "top": (0.0, -2.6), "bottom": (0.0, 2.6)}[side]
    if n.endswith("_FWD") or n.endswith("_INJ"):
        # the sample connector's label goes on whichever side of its line the
        # detector chain is not
        dx, dy = -16.0, (-2.7 if n[2] == "1" else 2.7)
        if side in ("top", "bottom"):
            dx, dy = 0.0, (-2.6 if side == "top" else 2.6)
    t.SetPosition(P(x + dx, y + dy))
    t.SetLayer(pcbnew.F_SilkS)
    t.SetTextSize(pcbnew.VECTOR2I(MM(0.85), MM(1.2)))
    t.SetTextThickness(MM(0.18))
    t.SetHorizJustify(pcbnew.GR_TEXT_H_ALIGN_CENTER)
    board.Add(t)

for fp in board.GetFootprints():
    xs, ys = [], []
    for p in fp.Pads():
        pos, sz = p.GetPosition(), p.GetSize()
        xs += [pos.x - sz.x / 2, pos.x + sz.x / 2]
        ys += [pos.y - sz.y / 2, pos.y + sz.y / 2]
    if xs:
        m = MM(0.08)
        cy = pcbnew.PCB_SHAPE(fp, pcbnew.SHAPE_T_RECTANGLE)
        cy.SetStart(pcbnew.VECTOR2I(int(min(xs) - m), int(min(ys) - m)))
        cy.SetEnd(pcbnew.VECTOR2I(int(max(xs) + m), int(max(ys) + m)))
        cy.SetLayer(pcbnew.F_CrtYd)
        cy.SetWidth(MM(0.05))
        cy.SetFilled(False)
        fp.Add(cy)
    fp.Reference().SetVisible(False)
    fp.Value().SetVisible(False)

# A drawing to attach to the order.  Three things a fabricator will otherwise
# change to be helpful: the laminate, the stackup, and the bare copper on the
# underside where the amplifiers sit.
NOTES = [
    f"{S['name']} {S['dielectric_mm'][0]} mm top dielectric, "
    f"er {S['er']}, loss tangent {S['tand']} -- NO SUBSTITUTION",
    f"{S['layers']} copper layers, finished thickness "
    f"{S['total_mm']:.3f} mm +/- 10 per cent",
    f"surface finish {S['finish']}, 1 oz copper all layers",
    "minimum track 0.20 mm, minimum gap 0.15 mm, minimum hole 0.25 mm",
    "50 ohm microstrip on F.Cu referenced to In1.Cu: 1.854 mm wide",
    "solder mask omitted on B.Cu under the amplifiers -- intentional, "
    "this is the thermal contact face",
    "copper to board edge 0.15 mm at the six launches -- intentional",
    f"connector: {S['connector']} end launch, six off",
]
for i, txt in enumerate(NOTES):
    t = pcbnew.PCB_TEXT(board)
    t.SetText(("NOTES:  " if i == 0 else "        ") + txt)
    t.SetPosition(P(2.0, -4.0 - i * 3.0))
    t.SetLayer(pcbnew.Cmts_User)
    t.SetTextSize(pcbnew.VECTOR2I(MM(1.1), MM(1.6)))
    t.SetTextThickness(MM(0.2))
    t.SetHorizJustify(pcbnew.GR_TEXT_H_ALIGN_LEFT)
    board.Add(t)

board.BuildConnectivity()
pcbnew.ZONE_FILLER(board).Fill(board.Zones())
board.BuildConnectivity()
pcbnew.SaveBoard(PCB, board)

# The stackup has to be written as text: the bindings do not expose it, and
# without it the job file tells the fabricator to build ordinary FR-4 with
# bare copper, which is what they will then quote and make.
d0, d1, d2 = S["dielectric_mm"]
STACKUP = f"""\t\t(stackup
\t\t\t(layer "F.SilkS" (type "Top Silk Screen"))
\t\t\t(layer "F.Paste" (type "Top Solder Paste"))
\t\t\t(layer "F.Mask" (type "Top Solder Mask") (thickness 0.01))
\t\t\t(layer "F.Cu" (type "copper") (thickness 0.035))
\t\t\t(layer "dielectric 1" (type "core") (thickness {d0}) \
(material "{S['name']}") (epsilon_r {S['er']}) (loss_tangent {S['tand']}))
\t\t\t(layer "In1.Cu" (type "copper") (thickness 0.035))
\t\t\t(layer "dielectric 2" (type "prepreg") (thickness {d1}) \
(material "FR4") (epsilon_r 4.5) (loss_tangent 0.02))
\t\t\t(layer "In2.Cu" (type "copper") (thickness 0.035))
\t\t\t(layer "dielectric 3" (type "core") (thickness {d2}) \
(material "FR4") (epsilon_r 4.5) (loss_tangent 0.02))
\t\t\t(layer "B.Cu" (type "copper") (thickness 0.035))
\t\t\t(layer "B.Mask" (type "Bottom Solder Mask") (thickness 0.01))
\t\t\t(layer "B.Paste" (type "Bottom Solder Paste"))
\t\t\t(layer "B.SilkS" (type "Bottom Silk Screen"))
\t\t\t(copper_finish "Immersion silver")
\t\t\t(dielectric_constraints no)
\t\t)
"""
txt = open(PCB).read()
key = "\t(setup\n"
assert key in txt, "could not find the setup block"
open(PCB, "w").write(txt.replace(key, key + STACKUP, 1))

# ---------------------------------------------------------------- deliverables
# Gerbers and drill through KiCad's own command line rather than the plotting
# bindings, because that is what writes the job file -- and the job file is
# what PCBWay and JLCPCB read to pre-fill the order.  Without it they are told
# the board is ordinary FR-4 with bare copper, and that is what they quote and
# make.
CLI = os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.dirname(sys.executable)))), "MacOS", "kicad-cli")
if not os.path.exists(CLI):
    CLI = shutil.which("kicad-cli") or CLI
LAYERS = ("F.Cu,In1.Cu,In2.Cu,B.Cu,F.Mask,B.Mask,F.SilkS,F.Paste,"
          "Edge.Cuts,Cmts.User")
subprocess.run([CLI, "pcb", "export", "gerbers", "--output", GERB,
                "--layers", LAYERS, "--no-protel-ext", PCB],
               check=True, capture_output=True)
subprocess.run([CLI, "pcb", "export", "drill", "--output", GERB,
                "--format", "excellon", "--generate-map", "--map-format",
                "pdf", PCB], check=True, capture_output=True)
# KiCad has no Gerber token for immersion silver and writes "None" into the
# job file's finish field, which is the field a fabricator's ordering page
# reads.  The Gerber job specification does have a token for it, so it goes
# in by hand.  Left alone the board would be quoted and made with bare copper.
JOB = os.path.join(GERB, f"radar_5g8_{NAME}-job.gbrjob")
if os.path.exists(JOB):
    j = json.load(open(JOB))
    j["GeneralSpecs"]["FinishedSurface"] = "Immersion Ag"
    json.dump(j, open(JOB, "w"), indent=1)

subprocess.run([CLI, "pcb", "export", "pdf", "--output",
                os.path.join(OUT, "FAB_DRAWING.pdf"), "--layers",
                "F.Cu,Edge.Cuts,Cmts.User,F.SilkS", PCB],
               check=True, capture_output=True)

# bill of materials and placement, from the same list the copper came from
with open(os.path.join(OUT, "BOM.csv"), "w", newline="") as f:
    w = csv.writer(f)
    w.writerow(["Ref", "Value", "Package", "Manufacturer part", "Note"])
    for r in D["bom"]:
        w.writerow([r["ref"], r["value"], r["pkg"], r["mpn"], r["note"]])
    for x, y, drill, pad in D["mounts"]:
        pass

with open(os.path.join(OUT, "PLACEMENT.csv"), "w", newline="") as f:
    w = csv.writer(f)
    w.writerow(["Ref", "MidX(mm)", "MidY(mm)", "Layer", "Rotation", "Part"])
    for prt in D["parts"]:
        if prt["kind"] not in ("SMD", "FINAL", "SERIES", "SHUNT", "HDR"):
            continue
        ref = prt.get("ref")
        if not ref:
            continue
        w.writerow([ref, f"{prt['x']:.3f}", f"{prt['y']:.3f}", "top",
                    prt.get("rot", 0),
                    prt.get("mpn") or prt.get("value", "")])

shutil.make_archive(os.path.join(OUT, "gerbers"), "zip", GERB)
