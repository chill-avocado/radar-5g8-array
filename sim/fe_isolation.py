"""openEMS: how far apart two chains have to be on one board.

Putting the transmit amplifiers and the receive amplifiers on the same piece
of laminate only works if what leaks across it is smaller than what already
leaks between the two antennas.  That is the whole question for a combined
front end, and it is a geometry question: two fifty-ohm lines running side by
side for the length of the board, with a wall of ground vias between them.

Measured here on the real stack -- 0.76 mm of PTFE over a ground plane -- as
a function of how far apart the lines sit, with and without the wall.

Usage:  python3 fe_isolation.py <separation_mm> [wall|nowall] [tag]
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
sys.path.insert(0, DESIGN)

D = json.load(open(os.path.join(DESIGN, "pa_board.json")))
S = D["stack"]
ER, TAND, H = S["er"], S["tand"], S["h_mm"]
WF = D["w50"]

SEP = float(sys.argv[1]) if len(sys.argv) > 1 else 23.0
WALL = (sys.argv[2] if len(sys.argv) > 2 else "wall") == "wall"
TAG = sys.argv[3] if len(sys.argv) > 3 else f"{SEP:.0f}{'w' if WALL else 'n'}"

F0, FMAX = 5.80e9, 8.0e9
RUN = 80.0                     # how far the two lines run alongside
MARGIN = 8.0
kappa = 2 * np.pi * F0 * 8.8541878128e-12 * ER * TAND


def clean(v, m=0.08):
    out = []
    for x in sorted(float(t) for t in v):
        if not out or x - out[-1] >= m:
            out.append(x)
        else:
            out[-1] = 0.5 * (out[-1] + x)
    return out


y_a, y_b = 0.0, SEP
ymin, ymax = y_a - MARGIN, y_b + MARGIN
zmax = H + 14.0

FDTD = openEMS(NrTS=int(os.environ.get("NRTS", 90000)), EndCriteria=1e-5)
FDTD.SetGaussExcite((F0 + FMAX / 2) / 2.0, (FMAX - 3.0e9) / 2.0)
# The lines run out through the x faces; everything else is an absorbing wall
# far enough back that what radiates sideways is counted, not reflected.
FDTD.SetBoundaryCond(["PML_8", "PML_8", "MUR", "MUR", "PEC", "MUR"])
CSX = ContinuousStructure()
FDTD.SetCSX(CSX)
mesh = CSX.GetGrid()
mesh.SetDeltaUnit(1e-3)

CSX.AddMaterial("sub", epsilon=ER, kappa=kappa).AddBox(
    [0, ymin, 0], [RUN, ymax, H], priority=1)
metal = CSX.AddMetal("cu")
for yc in (y_a, y_b):
    metal.AddBox([0, yc - WF / 2, H], [RUN, yc + WF / 2, H], priority=10)

# the ground pour between and around them, and its stitching
if WALL:
    g = CSX.AddMetal("pour")
    g.AddBox([0, y_a + WF / 2 + 1.0, H], [RUN, y_b - WF / 2 - 1.0, H],
             priority=8)
    v = CSX.AddMetal("wallvia")
    for x in np.arange(1.5, RUN, 3.0):
        for yy in (SEP / 2 - 1.5, SEP / 2, SEP / 2 + 1.5):
            v.AddBox([x - 0.2, yy - 0.2, 0], [x + 0.2, yy + 0.2, H],
                     priority=12)

RES = float(os.environ.get("RES", 0.45))
FINE = 0.12
xl = list(np.arange(0.0, RUN + 1e-9, RES))
yl = [ymin, ymax] + list(np.arange(ymin, ymax + 1e-9, RES))
for yc in (y_a, y_b):
    for e in (yc - WF / 2, yc + WF / 2):
        yl += [e - FINE / 3, e + 2 * FINE / 3]
    yl += list(np.arange(yc - WF / 2, yc + WF / 2 + 1e-9, WF / 5))
mesh.AddLine("x", clean(xl, RES * 0.6))
mesh.AddLine("y", clean(yl, FINE * 0.6))
mesh.SmoothMeshLines("y", RES, 1.4)
mesh.AddLine("z", clean(list(np.linspace(0, H, 4)) + [zmax], 0.06))
mesh.SmoothMeshLines("z", 1.4, 1.4)

PORTS = [
    FDTD.AddMSLPort(1, metal, [0, y_a - WF / 2, H], [12.0, y_a + WF / 2, 0],
                    "x", "z", excite=-1, FeedShift=3.0, MeasPlaneShift=8.0,
                    priority=15),
    FDTD.AddMSLPort(2, metal, [RUN, y_a - WF / 2, H],
                    [RUN - 12.0, y_a + WF / 2, 0], "x", "z",
                    MeasPlaneShift=8.0, priority=15),
    FDTD.AddMSLPort(3, metal, [0, y_b - WF / 2, H], [12.0, y_b + WF / 2, 0],
                    "x", "z", MeasPlaneShift=8.0, priority=15),
    FDTD.AddMSLPort(4, metal, [RUN, y_b - WF / 2, H],
                    [RUN - 12.0, y_b + WF / 2, 0], "x", "z",
                    MeasPlaneShift=8.0, priority=15),
]

if __name__ == "__main__":
    nc = [len(mesh.GetLines(d)) for d in range(3)]
    print(f"[{TAG}] {SEP:.0f} mm apart, {'wall' if WALL else 'no wall'}, "
          f"mesh {nc[0]}x{nc[1]}x{nc[2]} = {nc[0]*nc[1]*nc[2]/1e6:.2f} Mcells",
          flush=True)
    sp = os.path.join(tempfile.gettempdir(), "oems_fe_" + TAG)
    os.makedirs(sp, exist_ok=True)
    FDTD.Run(sp, cleanup=True, verbose=0,
             numThreads=int(os.environ.get("THREADS", 6)))

    f = np.linspace(4.5e9, 7.0e9, 251)
    for p in PORTS:
        p.CalcPort(sp, f, ref_impedance=50)
    a1 = PORTS[0].uf_inc
    db = lambda v: 20.0 * np.log10(np.maximum(np.abs(v), 1e-14))
    s21 = PORTS[1].uf_ref / a1
    s31 = PORTS[2].uf_ref / a1
    s41 = PORTS[3].uf_ref / a1
    k = int(np.argmin(np.abs(f - F0)))
    print(f"   through the driven line     {db(s21)[k]:7.2f} dB")
    print(f"   into the near end of the other {db(s31)[k]:7.2f} dB")
    print(f"   into the far end of the other  {db(s41)[k]:7.2f} dB")
    worst = max(db(s31)[k], db(s41)[k])
    print(f"   worst crosstalk             {worst:7.2f} dB over {RUN:.0f} mm")
    RESULTS = os.path.join(HERE, "results")
    json.dump({"sep_mm": SEP, "wall": WALL, "f": f.tolist(),
               "s31": db(s31).tolist(), "s41": db(s41).tolist(),
               "worst_db": float(worst)},
              open(os.path.join(RESULTS, f"fe_iso_{TAG}.json"), "w"))
    print(f"   wrote fe_iso_{TAG}.json")
