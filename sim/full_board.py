"""openEMS: the whole board, to measure transmit-to-receive isolation.

Everything else has been simulated an element or a pair at a time.  This model
is the entire 76 x 176 mm board read straight out of board.json -- all four
elements, both top ground pours, all 260 stitching vias, the four terminating
resistors and the real ground plane -- so it can answer the one question none
of the smaller models could: how much of the transmitter actually reaches the
receiver across the 92 mm gap.

That number sets whether the receiver stays inside its linear range while the
radar is transmitting, which is the whole reason the two arrays were pushed
apart in the first place.

Usage:  VARIANT=RO4350B python3 full_board.py [tag] [drive_port 1..4]
"""

import json
import os
import sys
import tempfile

import numpy as np
from CSXCAD import ContinuousStructure
from openEMS import openEMS

HERE = os.path.dirname(os.path.abspath(__file__))
DESIGN = os.path.abspath(os.path.join(HERE, "..", "design"))
VARIANT = os.environ.get("VARIANT", "RO4350B")
B = json.load(open(os.path.join(
    DESIGN, "board.json" if VARIANT == "RO4350B" else "board_fr4.json")))
cfg = json.load(open(os.path.join(DESIGN, "synthesis.json")))[VARIANT]

F0 = 5.80e9
FC = float(os.environ.get("FC", 0.60e9))
ER, TAND = cfg["substrate"]["er"], cfg["substrate"]["tand"]
H = cfg["substrate"]["h_mm"]
WF = B["w50"]
BW, BH = B["outline"]

tag = sys.argv[1] if len(sys.argv) > 1 else "fb"
DRIVE = int(sys.argv[2]) if len(sys.argv) > 2 else 1     # 1..4

AIR = 13.0
xmin, xmax = -AIR, BW + AIR
ymin, ymax = -AIR, BH + AIR
zmin, zmax = -AIR, H + AIR

FDTD = openEMS(NrTS=int(os.environ.get("NRTS", 200000)),
               EndCriteria=float(os.environ.get("ENDC", 1e-5)))
FDTD.SetGaussExcite(F0, FC)
FDTD.SetBoundaryCond(["MUR"] * 6)
CSX = ContinuousStructure()
FDTD.SetCSX(CSX)
mesh = CSX.GetGrid()
mesh.SetDeltaUnit(1e-3)

# A slot right through the board -- substrate, ground plane and pour -- across
# the 66 mm band that carries no signal copper.  Coupling between two antennas
# on a grounded slab travels partly as a wave bound to the substrate itself;
# this severs that path and leaves only the route through the air, so the
# difference between CUT and no CUT says which of the two is in charge.
# CUT="y0,y1" in millimetres; unset leaves the board whole.
CUT = os.environ.get("CUT", "")
CY0, CY1 = ([float(v) for v in CUT.split(",")] if CUT else (None, None))


def y_bands(y0, y1):
    """The board's y extent, minus the cut."""
    if CY0 is None or CY1 <= y0 or CY0 >= y1:
        return [(y0, y1)]
    out = []
    if CY0 > y0:
        out.append((y0, min(CY0, y1)))
    if CY1 < y1:
        out.append((max(CY1, y0), y1))
    return out


kappa = 2 * np.pi * F0 * 8.8541878128e-12 * ER * TAND
sub_m = CSX.AddMaterial("sub", epsilon=ER, kappa=kappa)
gnd_m = CSX.AddMetal("gnd")
for a, b in y_bands(0.0, BH):
    sub_m.AddBox([0, a, 0], [BW, b, H], priority=1)
for a, b in y_bands(0.25, BH - 0.25):
    gnd_m.AddBox([0.25, a, 0], [BW - 0.25, b, 0], priority=10)

top = CSX.AddMetal("top")
for p in B["top"] + B["gnd_top"]:
    ys = [q[1] for q in p]
    if CY0 is not None and min(ys) < CY1 and max(ys) > CY0:
        # only the isolation pour reaches into the band, and it is a
        # rectangle, so clipping it is just a matter of shortening it
        xs = [q[0] for q in p]
        assert len(set(round(v, 6) for v in xs)) == 2, "cut crosses a shaped polygon"
        for a, b in y_bands(min(ys), max(ys)):
            if b - a > 0.2:
                top.AddPolygon([[min(xs), max(xs), max(xs), min(xs)],
                                [a, a, b, b]],
                               norm_dir=2, elevation=H, priority=10)
        continue
    top.AddPolygon([[q[0] for q in p], [q[1] for q in p]],
                   norm_dir=2, elevation=H, priority=10)

# every stitching via, as a square post of the same cross-section
vm = CSX.AddMetal("vias")
n_via = 0
for x, y, drill, pad in B["vias"]:
    if CY0 is not None and CY0 <= y <= CY1:
        continue
    r = drill / 2.0
    vm.AddBox([x - r, y - r, 0], [x + r, y + r, H], priority=12)
    n_via += 1

# ---------------------------------------------------------------- terminations
for e in B["elements"]:
    tx, ty = e["term"]
    if e["hand"] == "RHCP":            # transmit: resistor sits below
        box = ([tx - WF / 2, ty - 1.6, 0], [tx + WF / 2, ty - 1.0, H])
        pad = ([tx - WF / 2, ty - 1.0, H], [tx + WF / 2, ty, H])
    else:                              # receive: to the left once transposed
        box = ([tx - 1.6, ty - WF / 2, 0], [tx - 1.0, ty + WF / 2, H])
        pad = ([tx - 1.0, ty - WF / 2, H], [tx, ty + WF / 2, H])
    top.AddBox(pad[0], pad[1], priority=10)
    CSX.AddLumpedElement(f"R_{e['name']}", ny="z", caps=True, R=50.0
                         ).AddBox(box[0], box[1], priority=20)

# --------------------------------------------------------------------- mesh
# 0.45 mm still puts ~28 cells across a patch and ~3 across the
# 0.263 mm transformers; the floor on cell size sets the timestep,
# and on a board this size that is what decides whether the run
# finishes in an hour or in four.
res_c, res_air, MIN_D = 0.45, 2.20, 0.090


def clean(v, m=MIN_D):
    out = []
    for x in sorted(float(t) for t in v):
        if not out or x - out[-1] >= m:
            out.append(x)
        else:
            out[-1] = 0.5 * (out[-1] + x)
    return out


ex, ey = set(), set()
for p in B["top"]:            # signal copper only; the pours need no detail
    for q in p:
        ex.add(round(q[0], 3)); ey.add(round(q[1], 3))
xl = [xmin, 0.0, BW, xmax] + sorted(ex)
yl = [ymin, 0.0, BH, ymax] + sorted(ey)
if CY0 is not None:                      # resolve the slot walls
    for e in (CY0, CY1):
        yl += [e - res_c / 3, e, e + 2 * res_c / 3]
    yl += list(np.arange(CY0, CY1 + 1e-9, max((CY1 - CY0) / 6, 0.45)))
for e in sorted(ex):
    xl += [e - res_c / 3, e + 2 * res_c / 3]
for e in sorted(ey):
    yl += [e - res_c / 3, e + 2 * res_c / 3]
# fill the patches and the open board so nothing is coarser than res_c there
for e in B["elements"]:
    bb = e["bbox"]
    xl += list(np.arange(bb[0], bb[2] + 1e-9, res_c))
    yl += list(np.arange(bb[1], bb[3] + 1e-9, res_c))
for x, y, drill, pad in B["vias"]:
    xl += [x - drill / 2, x + drill / 2]
    yl += [y - drill / 2, y + drill / 2]
mesh.AddLine("x", clean(xl))
mesh.AddLine("y", clean(yl))
mesh.SmoothMeshLines("x", res_air, 1.4)
mesh.SmoothMeshLines("y", res_air, 1.4)
mesh.AddLine("z", clean([zmin] + list(np.linspace(0, H, 4)) + [zmax]))
mesh.SmoothMeshLines("z", res_air, 1.4)

# ---------------------------------------------------------------------- ports
NAMES = ["TX1", "TX2", "RX1", "RX2"]
byname = {e["name"]: e for e in B["elements"]}
ports = []
for n, nm in enumerate(NAMES):
    px, py = byname[nm]["input"]
    if nm.startswith("TX"):                 # feed leaves the bottom edge
        s = [px - WF / 2, 0.15, 0]
        t = [px + WF / 2, 1.05, H]
    else:                                   # feed leaves the left edge
        s = [0.15, py - WF / 2, 0]
        t = [1.05, py + WF / 2, H]
    ports.append(FDTD.AddLumpedPort(n + 1, 50.0, s, t, "z",
                                    excite=(1 if n + 1 == DRIVE else 0),
                                    priority=20))

# Far field on the REAL board.  Every pattern so far came from a cut-down
# model with a 108 x 60 mm ground; this one has the actual 76 x 176 mm plane,
# both pours and all four elements, so it gives the embedded pattern a
# beamformer would actually see.
FFREQ = [float(v) for v in os.environ.get("FFREQ", "5.800e9").split(",")]
nf2ff = FDTD.CreateNF2FFBox(frequency=FFREQ)

if __name__ == "__main__":
    nc = [len(mesh.GetLines(d)) for d in range(3)]
    print(f"[{tag}] driving {NAMES[DRIVE-1]}   mesh {nc[0]}x{nc[1]}x{nc[2]} = "
          f"{nc[0]*nc[1]*nc[2]/1e6:.2f} Mcells   board {BW} x {BH} mm"
          + (f"   slot y {CY0}..{CY1}, {n_via} vias kept" if CY0 else ""),
          flush=True)
    sp = os.path.join(tempfile.gettempdir(), "oems_fb_" + tag)
    os.makedirs(sp, exist_ok=True)
    FDTD.Run(sp, cleanup=True, verbose=0, numThreads=int(os.environ.get("THREADS", 6)))

    f = np.linspace(5.5e9, 6.1e9, 121)
    for p in ports:
        p.CalcPort(sp, f, ref_impedance=50)
    inc = ports[DRIVE - 1].uf_inc
    S = [p.uf_ref / inc for p in ports]
    k = int(np.argmin(np.abs(f - F0)))
    print(f"\n   driven port: {NAMES[DRIVE-1]}")
    for n, nm in enumerate(NAMES):
        lab = "match" if n + 1 == DRIVE else "coupling"
        print(f"   S({nm:>3} <- {NAMES[DRIVE-1]}) = "
              f"{20*np.log10(abs(S[n][k])):7.2f} dB   {lab}")
    out = {"tag": tag, "drive": NAMES[DRIVE - 1], "f": f.tolist(),
           "S": {nm: [S[n].real.tolist(), S[n].imag.tolist()]
                 for n, nm in enumerate(NAMES)}}
    theta = np.arange(-90, 90.5, 1.0)
    cx = sum(e["centre"][0] for e in B["elements"]) / 4.0
    cy = sum(e["centre"][1] for e in B["elements"]) / 4.0
    out["ff"] = {}
    for ff in FFREQ:
        r = nf2ff.CalcNF2FF(sp, ff, theta, [0.0, 90.0],
                            center=[cx * 1e-3, cy * 1e-3, H / 2 * 1e-3],
                            outfile=f"nfb_{tag}_{ff/1e9:.3f}.h5",
                            read_cached=False, verbose=0)
        Et = np.array(r.E_theta[0])
        Ep = np.array(r.E_phi[0])
        ER_ = (Et + 1j * Ep) / np.sqrt(2.0)
        EL_ = (Et - 1j * Ep) / np.sqrt(2.0)
        i0 = int(np.argmin(np.abs(theta)))
        a_, b_ = abs(ER_[i0, 0]), abs(EL_[i0, 0])
        want, other = (a_, b_) if NAMES[DRIVE - 1].startswith("TX") else (b_, a_)
        ar = 20 * np.log10((want + other) / max(want - other, 1e-15))
        out["ff"][f"{ff/1e9:.3f}"] = {
            "Dmax_dBi": float(10 * np.log10(r.Dmax[0])),
            "AR_dB": float(ar), "theta": theta.tolist(),
            "centre_mm": [cx, cy],
            "ER_E": [ER_[:, 0].real.tolist(), ER_[:, 0].imag.tolist()],
            "EL_E": [EL_[:, 0].real.tolist(), EL_[:, 0].imag.tolist()],
            "ER_H": [ER_[:, 1].real.tolist(), ER_[:, 1].imag.tolist()],
            "EL_H": [EL_[:, 1].real.tolist(), EL_[:, 1].imag.tolist()]}
        print(f"   far field {ff/1e9:.3f} GHz: Dmax "
              f"{10*np.log10(r.Dmax[0]):.2f} dBi, wanted-sense axial ratio "
              f"{ar:.2f} dB", flush=True)

    RESULTS = os.path.join(HERE, "results")
    json.dump(out, open(os.path.join(RESULTS, f"fb_{tag}.json"), "w"))
    band = (f >= 5.725e9) & (f <= 5.875e9)
    for n, nm in enumerate(NAMES):
        if n + 1 != DRIVE:
            w = 20 * np.log10(np.abs(S[n][band]))
            print(f"   worst {nm} coupling across the ISM band: {w.max():.2f} dB")
