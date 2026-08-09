"""Generate the KiCad schematic.

Every symbol is defined inside the file, so the sheet opens correctly on any
machine without chasing library paths.

The schematic is the electrical story of the board: each channel is an SMA
end launch feeding a 90 degree branch-line hybrid, whose two quadrature
outputs drive the two orthogonal edges of one square patch through quarter-
wave transformers, with the hybrid's isolated port terminated in 50 ohm.
"""

import json
import os
import sys
import uuid as _uuid

HERE = os.path.dirname(os.path.abspath(__file__))
DESIGN = os.path.abspath(os.path.join(HERE, "..", "design"))
VARIANT = sys.argv[1] if len(sys.argv) > 1 else "RO4350B"
D = json.load(open(os.path.join(
    DESIGN, "board.json" if VARIANT == "RO4350B" else "board_fr4.json")))
S = json.load(open(os.path.join(DESIGN, "synthesis.json")))[VARIANT]
OUT = os.path.join(HERE, f"radar_5g8_{VARIANT.lower()}")
os.makedirs(OUT, exist_ok=True)
SCH = os.path.join(OUT, f"radar_5g8_{VARIANT.lower()}.kicad_sch")

U = lambda: str(_uuid.uuid4())
FONT = "(effects (font (size 1.27 1.27)))"
HIDE = "(effects (font (size 1.27 1.27)) (hide yes))"


def pin(kind, shape, x, y, ang, length, name, num):
    return (f'      (pin {kind} {shape} (at {x} {y} {ang}) (length {length})\n'
            f'        (name "{name}" (effects (font (size 1.0 1.0))))\n'
            f'        (number "{num}" (effects (font (size 1.0 1.0)))))\n')


def libsym(name, ref, body, pins, val="", desc=""):
    s = (f'  (symbol "{name}" (pin_names (offset 0.254)) (exclude_from_sim no)'
         f' (in_bom yes) (on_board yes)\n'
         f'    (property "Reference" "{ref}" (at 0 6.35 0) {FONT})\n'
         f'    (property "Value" "{val}" (at 0 -6.35 0) {FONT})\n'
         f'    (property "Footprint" "" (at 0 0 0) {HIDE})\n'
         f'    (property "Datasheet" "" (at 0 0 0) {HIDE})\n'
         f'    (property "Description" "{desc}" (at 0 0 0) {HIDE})\n'
         f'    (symbol "{name.split(":")[-1]}_0_1"\n{body}    )\n'
         f'    (symbol "{name.split(":")[-1]}_1_1"\n{pins}    )\n'
         f'  )\n')
    return s


def rect(x0, y0, x1, y1, fill="none"):
    return (f'      (rectangle (start {x0} {y0}) (end {x1} {y1})\n'
            f'        (stroke (width 0.254) (type default))'
            f' (fill (type {fill})))\n')


def poly(pts, w=0.254, fill="none"):
    p = " ".join(f"(xy {a} {b})" for a, b in pts)
    return (f'      (polyline (pts {p})\n'
            f'        (stroke (width {w}) (type default))'
            f' (fill (type {fill})))\n')


def circ(x, y, r, fill="none"):
    return (f'      (circle (center {x} {y}) (radius {r})\n'
            f'        (stroke (width 0.254) (type default))'
            f' (fill (type {fill})))\n')


# ------------------------------------------------------------ symbol library
LIB = []
LIB.append(libsym(
    "Radar:Hybrid90", "HY",
    rect(-10.16, 7.62, 10.16, -7.62, "background")
    + poly([(-10.16, 5.08), (10.16, -5.08)])
    + poly([(-10.16, -5.08), (10.16, 5.08)]),
    pin("passive", "line", -15.24, 5.08, 0, 5.08, "IN", "1")
    + pin("passive", "line", -15.24, -5.08, 0, 5.08, "ISO", "2")
    + pin("passive", "line", 15.24, 5.08, 180, 5.08, "0deg", "3")
    + pin("passive", "line", 15.24, -5.08, 180, 5.08, "-90deg", "4"),
    "BRANCH-LINE 3dB 90deg",
    "Branch-line quadrature hybrid etched in microstrip"))

LIB.append(libsym(
    "Radar:PatchCP", "AE",
    poly([(0, -7.62), (0, -2.54)])
    + rect(-5.08, 5.08, 5.08, -2.54, "none")
    + poly([(-5.08, -2.54), (0, -2.54)])
    + poly([(0, -2.54), (0, 5.08)]),
    pin("passive", "line", -10.16, -2.54, 0, 5.08, "X", "1")
    + pin("passive", "line", 0, -10.16, 90, 2.54, "Y", "2"),
    "SQUARE PATCH", "Dual-fed square microstrip patch"))

LIB.append(libsym(
    "Radar:SMA", "J",
    circ(0, 0, 2.54) + circ(0, 0, 0.762, "outline")
    + poly([(2.54, 0), (5.08, 0)]) + poly([(0, -2.54), (0, -5.08)]),
    pin("passive", "line", 7.62, 0, 180, 2.54, "SIG", "1")
    + pin("passive", "line", 0, -7.62, 90, 2.54, "SHIELD", "2"),
    "SMA-EDGE-50R", "SMA jack, end launch, 50 ohm"))

LIB.append(libsym(
    "Radar:R", "R",
    rect(-1.016, 2.54, 1.016, -2.54),
    pin("passive", "line", 0, 5.08, 270, 2.54, "~", "1")
    + pin("passive", "line", 0, -5.08, 90, 2.54, "~", "2"),
    "50R", "Thin-film chip resistor"))

LIB.append(libsym(
    "Radar:MountingHole", "H",
    circ(0, 0, 1.6) + circ(0, 0, 2.8),
    pin("passive", "line", 0, -5.08, 90, 2.54, "GND", "1"),
    "M3", "M3 plated mounting hole, bonded to the ground plane"))

LIB.append(
    '  (symbol "power:GND" (power) (pin_numbers (hide yes))'
    ' (pin_names (offset 0) (hide yes)) (exclude_from_sim no)'
    ' (in_bom no) (on_board yes)\n'
    f'    (property "Reference" "#PWR" (at 0 -6.35 0) {HIDE})\n'
    f'    (property "Value" "GND" (at 0 -3.81 0) {FONT})\n'
    f'    (property "Footprint" "" (at 0 0 0) {HIDE})\n'
    f'    (property "Datasheet" "" (at 0 0 0) {HIDE})\n'
    '    (symbol "GND_0_1"\n'
    + poly([(0, 0), (0, -1.27), (1.27, -1.27), (0, -2.54),
            (-1.27, -1.27), (0, -1.27)])
    + '    )\n    (symbol "GND_1_1"\n'
    + pin("power_in", "line", 0, 0, 270, 0, "GND", "1")
    + '    )\n  )\n')

# ------------------------------------------------------------------ instances
BODY, WIRES, LABELS = [], [], []


def place(lib, ref, val, x, y, fp="", extra=()):
    u = U()
    props = (f'    (property "Reference" "{ref}" (at {x} {y-8.9} 0) {FONT})\n'
             f'    (property "Value" "{val}" (at {x} {y+8.9} 0) {FONT})\n'
             f'    (property "Footprint" "{fp}" (at {x} {y} 0) {HIDE})\n'
             f'    (property "Datasheet" "" (at {x} {y} 0) {HIDE})\n')
    for k, v in extra:
        props += f'    (property "{k}" "{v}" (at {x} {y} 0) {HIDE})\n'
    npins = {"Radar:Hybrid90": 4, "Radar:PatchCP": 2, "Radar:SMA": 2,
             "Radar:R": 2, "power:GND": 1, "Radar:MountingHole": 1}[lib]
    pins = "".join(f'    (pin "{i+1}" (uuid "{U()}"))\n' for i in range(npins))
    BODY.append(
        f'  (symbol (lib_id "{lib}") (at {x} {y} 0) (unit 1)\n'
        f'    (exclude_from_sim no) (in_bom yes) (on_board yes) (dnp no)\n'
        f'    (uuid "{u}")\n{props}{pins}'
        f'    (instances (project "radar_5g8"\n'
        f'      (path "/{ROOT}" (reference "{ref}") (unit 1)))))\n')


def wire(x0, y0, x1, y1):
    WIRES.append(f'  (wire (pts (xy {x0} {y0}) (xy {x1} {y1}))\n'
                 f'    (stroke (width 0) (type default)) (uuid "{U()}"))\n')


def text(x, y, s, size=1.27):
    LABELS.append(f'  (text "{s}" (exclude_from_sim no) (at {x} {y} 0)\n'
                  f'    (effects (font (size {size} {size})) (justify left))'
                  f' (uuid "{U()}"))\n')


ROOT = U()
CH = [("TX1", "R1", "RHCP"), ("TX2", "R2", "RHCP"),
      ("RX1", "R3", "LHCP"), ("RX2", "R4", "LHCP")]

# Symbol bodies are drawn in a y-up frame but the sheet is y-down, so a pin
# defined at relative (px, py) lands at sheet (X + px, Y - py).
def PIN(ox, oy, px, py):
    return (ox + px, oy - py)


for i, (port, rref, hand) in enumerate(CH):
    ox, oy = 45.0, 62.0 + i * 52.0
    hx = ox + 42.0
    ax = ox + 92.0
    rx, ry = ox + 22.0, oy + 17.0

    place("Radar:SMA", port, "SMA-EDGE-50R", ox, oy,
          "radar5g8:SMA_EndLaunch_50R")
    place("power:GND", f"#PWR{i*2+1:02d}", "GND", *PIN(ox, oy, 0, -7.62))
    place("Radar:Hybrid90", f"HY{i+1}", "BRANCH-LINE 3dB 90deg", hx, oy)
    place("Radar:R", rref, "50R 0402 thin film", rx, ry,
          "radar5g8:R_0402_50R_ThinFilm")
    place("power:GND", f"#PWR{i*2+2:02d}", "GND", *PIN(rx, ry, 0, -5.08))
    place("Radar:PatchCP", f"AE{i+1}", f"SQUARE PATCH {hand}", ax, oy)

    sig = PIN(ox, oy, 7.62, 0)
    hin = PIN(hx, oy, -15.24, 5.08)
    hiso = PIN(hx, oy, -15.24, -5.08)
    h0 = PIN(hx, oy, 15.24, 5.08)
    h90 = PIN(hx, oy, 15.24, -5.08)
    px_ = PIN(ax, oy, -10.16, -2.54)
    py_ = PIN(ax, oy, 0, -10.16)
    r1 = PIN(rx, ry, 0, 5.08)

    wire(*sig, ox + 15.0, oy)                       # SMA -> hybrid input
    wire(ox + 15.0, oy, ox + 15.0, hin[1])
    wire(ox + 15.0, hin[1], *hin)
    wire(*hiso, rx, hiso[1])                        # isolated port -> 50 ohm
    wire(rx, hiso[1], *r1)
    # Transmit takes 0 deg to the x edge and -90 deg to the y edge, which
    # makes the y mode lag and the wave turn right-handed.  The receive
    # element is that element reflected, so the two outputs swap and the wave
    # turns the other way.  Drawing both the same would be a lie.
    to_x, to_y = (h0, h90) if hand == "RHCP" else (h90, h0)
    wire(*to_x, hx + 27.0, to_x[1])                 # -> patch x edge
    wire(hx + 27.0, to_x[1], hx + 27.0, px_[1])
    wire(hx + 27.0, px_[1], *px_)
    wire(*to_y, ax, to_y[1])                        # -> patch y edge
    wire(ax, to_y[1], *py_)

    text(ox - 6.0, oy - 16.0, f"{port}   {hand}", 1.8)
    if hand == "LHCP":
        text(ox - 6.0, oy - 12.6,
             "element is the transmit element reflected: opposite hand", 1.0)
    if port in ("TX2", "RX2"):
        text(ox - 6.0, oy - 9.6,
             "mirrored layout, fed from the opposite hybrid corner", 1.0)
    zq = D["report"]["transformer_z_ohm"]
    text(hx + 3.0, h0[1] - 2.0, f"lambda_g/4  {zq:.0f} ohm", 1.0)
    text(hx + 3.0, h90[1] + 3.2, f"lambda_g/4  {zq:.0f} ohm", 1.0)

for i in range(6):
    mx = 210.0 + i * 16.0
    place("Radar:MountingHole", f"H{i+1}", "M3", mx, 250.0,
          "radar5g8:MountingHole_M3")
    place("power:GND", f"#PWR{20+i:02d}", "GND", *PIN(mx, 250.0, 0, -5.08))

text(20, 16, "5.8 GHz 2x2 MIMO FMCW RADAR ARRAY", 2.5)
text(20, 22, f"Substrate: {S['substrate']['name']}  "
             f"h={S['substrate']['h_mm']} mm  er={S['substrate']['er']}  "
             f"tand={S['substrate']['tand']}   2 layer, ENIG", 1.4)
text(20, 27, f"Patch {D['report']['patch_Lx_mm']} x "
             f"{D['report']['patch_Ly_mm']} mm; element pitch "
             f"{D['report']['element_pitch_mm']} mm (half wave at 5.800 GHz)",
     1.4)
text(20, 32, "TX1/TX2 right-hand circular, spaced in azimuth.  "
             "RX1/RX2 left-hand circular, spaced in elevation.", 1.4)
text(20, 37, "Opposite sense on receive rejects the direct transmit leakage "
             "while accepting the odd-bounce target return.", 1.4)
text(206, 240, "Mounting holes: M3 plated, bonded to the ground plane.", 1.2)

out = ("(kicad_sch\n  (version 20250114)\n  (generator \"eeschema\")\n"
       "  (generator_version \"10.0\")\n"
       f"  (uuid \"{ROOT}\")\n  (paper \"A3\")\n"
       "  (title_block\n"
       "    (title \"5.8 GHz 2x2 MIMO FMCW Radar Array\")\n"
       "    (rev \"A\")\n"
       f"    (comment 1 \"{S['substrate']['name']} {S['substrate']['h_mm']} mm,"
       " 2 layer, ENIG\")\n"
       "    (comment 2 \"TX: RHCP azimuth pair   RX: LHCP elevation pair\")\n"
       "  )\n"
       "  (lib_symbols\n" + "".join(LIB) + "  )\n"
       + "".join(WIRES) + "".join(LABELS) + "".join(BODY)
       + f'  (sheet_instances (path "/" (page "1")))\n)\n')
open(SCH, "w").write(out)
print("wrote", SCH)
