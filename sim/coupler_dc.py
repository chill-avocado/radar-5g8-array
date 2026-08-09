"""openEMS: measure a printed edge-coupled directional coupler.

The leakage canceller stands on two of these: a loose tap that samples the
transmit signal without disturbing it, and a looser injection point that puts
the correction into the receive path without adding noise.  Closed-form
coupled-line theory was 10 per cent out on this board's branch-line hybrid, so
the gaps it produces are treated as a starting guess and measured here.

Four ports, in the usual coupler order:
    1 in  --== coupled section ==--  2 through
    4 iso --==                ==--  3 coupled

What matters: coupling on port 3 (the design target), directivity, which is
how much better port 3 is than port 4 and therefore how cleanly the sample
represents the forward wave, and through loss on port 2.

Usage:  python3 coupler_dc.py <w_mm> <s_mm> <len_mm> [tag]
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

F0, FC = 5.80e9, 1.80e9
cfg = json.load(open(os.path.join(DESIGN, "synthesis.json")))["RO4350B"]
ER, TAND = cfg["substrate"]["er"], cfg["substrate"]["tand"]
H = cfg["substrate"]["h_mm"]
W50 = cfg["feed"]["w50_mm"]

W = float(sys.argv[1]); S = float(sys.argv[2]); LC = float(sys.argv[3])
tag = sys.argv[4] if len(sys.argv) > 4 else "dc"

FEED = 14.0                    # straight 50 ohm run into each of the 4 ports
PAD = 8.0                      # measurement plane, back from the driven end
SEP = W / 2 + S / 2            # centre of each strip, either side of y = 0

xmin, xmax = -FEED - LC / 2, FEED + LC / 2
ymin, ymax = -(SEP + W / 2 + 9.0), (SEP + W / 2 + 9.0)
zmax = H + 11.0

FDTD = openEMS(NrTS=int(os.environ.get("NRTS", 160000)), EndCriteria=1e-4)
FDTD.SetGaussExcite(F0, FC)
# All four line ends run out through a PML; the sides and lid are far enough
# away for MUR.  zmin is the board's own ground plane.
FDTD.SetBoundaryCond(["PML_8", "PML_8", "MUR", "MUR", "PEC", "MUR"])
CSX = ContinuousStructure()
FDTD.SetCSX(CSX)
mesh = CSX.GetGrid()
mesh.SetDeltaUnit(1e-3)

kappa = 2 * np.pi * F0 * 8.8541878128e-12 * ER * TAND
CSX.AddMaterial("sub", epsilon=ER, kappa=kappa).AddBox(
    [xmin, ymin, 0], [xmax, ymax, H], priority=1)

metal = CSX.AddMetal("cu")
# the coupled section itself
for sgn in (+1, -1):
    yc = sgn * SEP
    metal.AddBox([-LC / 2, yc - W / 2, H], [LC / 2, yc + W / 2, H], priority=10)

res = float(os.environ.get("RES", 0.10))
MIN_D = float(os.environ.get("MIND", 0.035))


def clean(v, m=MIN_D):
    out = []
    for x in sorted(float(t) for t in v):
        if not out or x - out[-1] >= m:
            out.append(x)
        else:
            out[-1] = 0.5 * (out[-1] + x)
    return out


# the gap is the whole point, so it gets resolved properly
gap_lines = list(np.linspace(-S / 2, S / 2, max(5, int(S / res) + 1)))
yl = [ymin, ymax] + gap_lines
for e in (SEP - W / 2, SEP + W / 2, -SEP - W / 2, -SEP + W / 2):
    yl += [e - res / 3, e + 2 * res / 3, e]
yl += list(np.arange(-(SEP + W / 2) - 3.0, SEP + W / 2 + 3.0, 0.35))
xl = [xmin, xmax] + list(np.arange(-LC / 2 - 2.0, LC / 2 + 2.0, 0.30))
for e in (-LC / 2, LC / 2):
    xl += [e - res / 3, e + 2 * res / 3]
mesh.AddLine("x", clean(xl))
mesh.AddLine("y", clean(yl))
mesh.SmoothMeshLines("x", 1.7, 1.4)
mesh.SmoothMeshLines("y", 1.7, 1.4)
mesh.AddLine("z", clean(list(np.linspace(0, H, 4)) + [zmax], 0.05))
mesh.SmoothMeshLines("z", 1.7, 1.4)

# ports, numbered the way coupler datasheets do
ports = []
spec = [(1, xmin, +SEP, "x", -1), (2, xmax, +SEP, "x", 0),
        (3, xmax, -SEP, "x", 0), (4, xmin, -SEP, "x", 0)]
for n, xe, yc, d, exc in spec:
    xin = -LC / 2 if xe < 0 else LC / 2
    ports.append(FDTD.AddMSLPort(
        n, metal, [xe, yc - W / 2, H], [xin, yc + W / 2, 0], d, "z",
        excite=exc, FeedShift=4.0, MeasPlaneShift=PAD, priority=15))

if __name__ == "__main__":
    nc = [len(mesh.GetLines(i)) for i in range(3)]
    print(f"[{tag}] w={W} s={S} len={LC} mm   mesh {nc[0]}x{nc[1]}x{nc[2]} = "
          f"{nc[0]*nc[1]*nc[2]/1e6:.2f} Mcells", flush=True)
    sp = os.path.join(tempfile.gettempdir(), "oems_dc_" + tag)
    os.makedirs(sp, exist_ok=True)
    FDTD.Run(sp, cleanup=True, verbose=0,
             numThreads=int(os.environ.get("THREADS", 6)))

    f = np.linspace(4.6e9, 7.0e9, 241)
    for p in ports:
        p.CalcPort(sp, f, ref_impedance=50)
    inc = ports[0].uf_inc
    s = {n: ports[n - 1].uf_ref / inc for n in (1, 2, 3, 4)}
    db = {n: 20 * np.log10(np.abs(s[n]) + 1e-15) for n in s}
    k = int(np.argmin(np.abs(f - F0)))
    print(f"   at 5.800 GHz: coupling {db[3][k]:6.2f} dB   through "
          f"{db[2][k]:5.2f} dB   isolation {db[4][k]:6.2f} dB   "
          f"return {db[1][k]:6.2f} dB")
    print(f"   directivity {db[3][k]-db[4][k]:.2f} dB   "
          f"phase of the sample {np.degrees(np.angle(s[3][k])):+7.1f} deg")
    b = (f >= 5.725e9) & (f <= 5.875e9)
    print(f"   across the band: coupling {db[3][b].min():.2f} to "
          f"{db[3][b].max():.2f} dB, return worse than "
          f"{db[1][b].max():.2f} dB")
    RESULTS = os.path.join(HERE, "results")
    json.dump({"w": W, "s": S, "len": LC, "f": f.tolist(),
               **{f"s{n}1": [s[n].real.tolist(), s[n].imag.tolist()]
                  for n in (1, 2, 3, 4)}},
              open(os.path.join(RESULTS, f"dc_{tag}.json"), "w"))
