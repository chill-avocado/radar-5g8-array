"""Produce the fabrication drawing PDF and the bill of materials."""

import csv
import json
import os
import sys

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import Polygon as MPoly, Circle, Rectangle

HERE = os.path.dirname(os.path.abspath(__file__))
DESIGN = os.path.abspath(os.path.join(HERE, "..", "design"))
VAR = sys.argv[1] if len(sys.argv) > 1 else "transmit"
D = json.load(open(os.path.join(
    DESIGN, {"RO4350B": "board.json"}.get(VAR, f"board_{VAR}.json"))))
S = json.load(open(os.path.join(DESIGN, "synthesis.json")))[VAR]
OUT = os.path.join(HERE, f"radar_5g8_{VAR.lower()}")
BW, BH = D["outline"]
sub = D["stack"]
T = S["tuned"]

NOTES = [
    "FABRICATION NOTES",
    "",
    f"1.  MATERIAL: {sub['name']}, dielectric thickness "
    f"{sub['h_mm']} mm +/-10%.",
    f"    Design permittivity er = {sub['er']} (microstrip design value), "
    f"tan d = {sub['tand']}.",
    "    No substitution without approval: the patch dimensions are tuned to "
    "this permittivity.",
    "2.  LAYERS: 2 copper layers. Layer 1 = antenna and feed network. "
    "Layer 2 = SOLID ground.",
    "    The ground plane must be continuous. Do NOT add thieving, hatching "
    "or venting to layer 2.",
    "3.  COPPER: 1 oz (35 um) finished on both layers.",
    "4.  FINISH: IMMERSION SILVER per IPC-4553.  DO NOT SUBSTITUTE ENIG.",
    "    At 5.8 GHz the current reaches only 0.87 um into the metal, so "
    "ENIG's 3 um
    of nickel would carry all of it at 2.7 times the "
    "resistance of copper --
    4.3 per cent of detection range.  Silver "
    "tarnish is a semiconductor and
    carries no RF current: a blackened "
    "board measures the same as a fresh one.",
    "5.  SOLDER MASK: layer 1 has deliberate openings over the radiating "
    "elements and the",
    "    four end-launch transitions - mask there would detune the patches "
    "and the launches.",
    "    Mask on layer 2 as normal. Green LPI.",
    "6.  SILKSCREEN: white, layer 1 and layer 2 as supplied. Do not print "
    "silkscreen on bare copper.",
    "7.  IMPEDANCE: single-ended 50 ohm +/-10% on the "
    f"{D['w50']:.3f} mm traces, referenced to layer 2.",
    "    Impedance control is requested; adjust trace width if your stack "
    "requires it, and report",
    "    the width used. Do NOT alter the patch or transformer geometry.",
    "8.  ETCH TOLERANCE: +/-0.02 mm on conductor width. Minimum feature on "
    f"this board is {T['wq']:.3f} mm",
    "    (the quarter-wave transformers) - confirm you can hold it before "
    "starting.",
    f"9.  HOLES: {len(D['vias'])} x 0.40 mm plated stitching vias; "
    f"{len(D['mounts'])} x 3.20 mm plated mounting holes.",
    "10. OUTLINE: routed, +/-0.15 mm. Copper reaches the board edge at the "
    "four launch sites by design.",
    "11. NO panelisation tabs across the antenna areas. V-score not "
    "permitted.",
    "12. ELECTRICAL TEST: 100% netlist test.",
    "13. The mask openings in note 5 expose adjacent copper of different nets.",
    "    That is intended on an antenna board and is the only rule waived in",
    "    the DRC report; everything else passes at error AND warning severity.",
]


def draw():
    fig = plt.figure(figsize=(16.54, 11.69))          # A3 landscape
    axb = fig.add_axes([0.04, 0.06, 0.30, 0.88])
    axb.add_patch(Rectangle((0, 0), BW, BH, facecolor="#f4f4f4",
                            edgecolor="k", lw=1.4))
    for p in D["gnd_top"]:
        axb.add_patch(MPoly(p, closed=True, facecolor="#c9c9c9",
                            edgecolor="none"))
    for p in D["top"]:
        axb.add_patch(MPoly(p, closed=True, facecolor="#8a8a8a",
                            edgecolor="k", lw=0.15))
    for x, y, dr, pa in D["vias"]:
        axb.add_patch(Circle((x, y), dr / 2, facecolor="w", edgecolor="k",
                             lw=0.2))
    for x, y, dr, pa in D["mounts"]:
        axb.add_patch(Circle((x, y), dr / 2, facecolor="w", edgecolor="k",
                             lw=0.6))
        axb.plot([x - 3, x + 3], [y, y], "k", lw=0.4)
        axb.plot([x, x], [y - 3, y + 3], "k", lw=0.4)

    def dim(x0, y0, x1, y1, label, off=0):
        axb.annotate("", (x0, y0), (x1, y1),
                     arrowprops=dict(arrowstyle="<->", lw=0.8))
        axb.text((x0 + x1) / 2 + off, (y0 + y1) / 2, label, fontsize=7,
                 ha="center", va="bottom", rotation=0 if y0 == y1 else 90,
                 bbox=dict(fc="white", ec="none", pad=0.5))

    dim(0, -6, BW, -6, f"{BW:.2f}")
    dim(-7, 0, -7, BH, f"{BH:.2f}")
    tx = D["report"]["tx_patches"]
    rx = D["report"]["rx_patches"]
    dim(tx[0][0], tx[0][1], tx[1][0], tx[1][1],
        f"{D['report']['tx_spacing_mm']:.4f}")
    dim(rx[0][0], rx[0][1], rx[1][0], rx[1][1],
        f"{D['report']['rx_spacing_mm']:.4f}")
    for n, x, y, side in D["ports"]:
        axb.plot(x, y, "kv" if side == "bottom" else "k<", ms=5)
        axb.text(x + (0 if side == "bottom" else 3),
                 y + (-4 if side == "bottom" else 2), n, fontsize=7,
                 ha="center")
    axb.set_xlim(-14, BW + 8)
    axb.set_ylim(-12, BH + 8)
    axb.set_aspect("equal")
    axb.axis("off")
    axb.set_title(f"OUTLINE AND DRILL PLAN   {BW} x {BH} mm\n"
                  "(dimensions in mm, origin at lower-left)", fontsize=9)

    axn = fig.add_axes([0.37, 0.06, 0.60, 0.88])
    axn.axis("off")
    axn.text(0, 1.0, f"5.8 GHz 2x2 MIMO FMCW RADAR ARRAY  -  {sub['name']}",
             fontsize=15, weight="bold", va="top", family="monospace")
    y = 0.955
    for line in NOTES:
        w = "bold" if line.isupper() and line else "normal"
        axn.text(0, y, line, fontsize=8.2, va="top", family="monospace",
                 weight=w)
        y -= 0.0182

    y -= 0.012
    axn.text(0, y, "STACKUP", fontsize=9, weight="bold", family="monospace",
             va="top")
    y -= 0.020
    for line in [
        "  +-------------------------------------------+  layer 1  35 um Cu, immersion silver, mask open over antennas",
        f"  |  {sub['name']:<20s} {sub['h_mm']:.3f} mm      |  er {sub['er']}, tan d {sub['tand']}",
        "  +-------------------------------------------+  layer 2  35 um Cu, SOLID ground, masked",
        f"     finished thickness approx {sub['h_mm']+0.07:.2f} mm",
    ]:
        axn.text(0, y, line, fontsize=8.2, va="top", family="monospace")
        y -= 0.0182

    y -= 0.012
    axn.text(0, y, "DRILL TABLE", fontsize=9, weight="bold",
             family="monospace", va="top")
    y -= 0.020
    for line in ["  SIZE      QTY   PLATED   NOTE",
                 f"  0.40 mm   {len(D['vias']):<5d} yes      ground stitching",
                 f"  3.20 mm   {len(D['mounts']):<5d} yes      M3 mounting, bonded to ground"]:
        axn.text(0, y, line, fontsize=8.2, va="top", family="monospace")
        y -= 0.0182

    y -= 0.012
    axn.text(0, y, "ELEMENT POSITIONS  (board origin at lower-left, mm)",
             fontsize=9, weight="bold", family="monospace", va="top")
    y -= 0.020
    axn.text(0, y, "  these are the phase centres to enter into the "
             "beamforming code", fontsize=8.2, va="top", family="monospace")
    y -= 0.0182
    for e in D["elements"]:
        axn.text(0, y, f"  {e['name']:<4s} {e['hand']:<5s} "
                 f"x {e['centre'][0]:8.4f}   y {e['centre'][1]:8.4f}"
                 f"{'   (mirrored)' if e.get('mirrored') else ''}",
                 fontsize=8.2, va="top", family="monospace")
        y -= 0.0182

    y -= 0.012
    axn.text(0, y, "CRITICAL RF DIMENSIONS  (do not modify)", fontsize=9,
             weight="bold", family="monospace", va="top")
    y -= 0.020
    crit = [
        f"  patch                {T['L']+T['dLx']:.4f} x {T['L']+T['dLy']:.4f} mm",
        f"  element pitch        {D['report']['element_pitch_mm']:.4f} mm  "
        f"(half wave in air at 5.800 GHz)",
        f"  50 ohm trace width   {D['w50']:.4f} mm",
        f"  transformer          {T['wq']:.4f} mm wide x {T['lq']:.4f} mm long "
        f"({T['zq_ohm']:.0f} ohm)",
        f"  hybrid arms          {T['arm_series']:.4f} mm (vertical) / "
        f"{T['arm_shunt']:.4f} mm (horizontal)",
        f"  launch ground gap    {D['launch_gap']:.4f} mm each side",
    ]
    for line in crit:
        axn.text(0, y, line, fontsize=8.2, va="top", family="monospace")
        y -= 0.0182

    fig.savefig(os.path.join(OUT, f"FAB_DRAWING_{VAR}.pdf"))
    fig.savefig(os.path.join(OUT, f"FAB_DRAWING_{VAR}.png"), dpi=110)
    print("wrote", os.path.join(OUT, f"FAB_DRAWING_{VAR}.pdf"))


# Verified against the Johnson/Cinch SMA end-launch table, which lists the
# design board thickness per part: -801 is 0.062 in, -831 is 0.042 in and
# -881 is 0.031 in.  Pick by FINISHED thickness (dielectric + 2 oz copper
# + mask), not by the dielectric alone.
FINISHED = sub["h_mm"] + 0.09
CONN = (("Cinch / Johnson", "142-0701-881",
         f"designed for 0.031 in (0.79 mm) board; this board finishes at "
         f"{FINISHED:.3f} mm, so the spring legs close on it. "
         f"Nickel-plated equivalent is 142-0701-886")
        if FINISHED < 0.95 else
        ("Cinch / Johnson", "142-0701-831",
         f"designed for 0.042 in (1.07 mm) board; this board finishes at "
         f"{FINISHED:.3f} mm. Nickel-plated equivalent is 142-0701-836"))

BOM = [
    ["Ref", "Qty", "Description", "Manufacturer", "Part number", "Notes"],
    ["J1-J4 (TX1,TX2,RX1,RX2)", "4",
     "SMA jack, PCB end launch, solder, 50 ohm, DC-18 GHz",
     CONN[0], CONN[1], CONN[2]],
    ["R1-R4", "4", "50.0 ohm +/-1% thin film chip resistor, 0402, to 20 GHz",
     "Vishay", "FC0402E50R0BST1",
     "terminates the hybrid isolated port; a Susumu RR0510P-500-D also fits"],
    ["D1, D2", "0 (DO NOT FIT)",
     "Optional receive limiter, 0402, shunt to ground",
     "-", "see note",
     "Site is empty as shipped and costs 0.09 dB. Fit ONLY a diode whose "
     "total off-state capacitance is 0.10 pF or less: measured in openEMS, "
     "0.1 pF gives -13.8 dB return loss and 0.26 dB loss, 0.2 pF gives "
     "-8.1 dB and 0.82 dB, 0.4 pF shorts the line. An in-line coaxial "
     "limiter is the alternative if you need more margin"],
    ["H1-H6", "6", "M3 mounting hardware", "-", "-",
     "holes are plated and bonded to the ground plane: use metal hardware if "
     "you want the mounting plate grounded, nylon if you do not"],
    ["PCB", "1", f"{sub['name'].replace(' 1.0mm','')}, {sub['h_mm']} mm dielectric, 2 layer, "
     f"1 oz, immersion silver (finished {sub['h_mm'] + 0.09:.3f} mm)",
     "PCBWay / JLCPCB", "-", f"{BW} x {BH} mm, see fabrication drawing"],
]

if __name__ == "__main__":
    draw()
    p = os.path.join(OUT, f"BOM_{VAR}.csv")
    with open(p, "w", newline="") as f:
        csv.writer(f).writerows(BOM)
    print("wrote", p)
