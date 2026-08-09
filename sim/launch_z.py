"""openEMS: measure the real impedance of the SMA end-launch section.

The coplanar ground gap at each connector was sized with the closed-form
conductor-backed-CPW model in rfmath.py.  That model is demonstrably wrong:
pushed to an infinite gap the structure becomes an ordinary microstrip and the
model must return the microstrip answer, but it returns 61 ohm where the
microstrip formula returns 50.  So the gap on the board rests on nothing.

This measures it instead.  A line of the launch cross-section is driven at one
end and absorbed into a PML at the other, so there is no reflection coming
back and the impedance seen at the measurement plane IS the characteristic
impedance of that cross-section.  Running it with the coplanar grounds removed
must return 50 ohm, which is the check that the method itself is sound.

Usage:  python3 launch_z.py <gap_mm | none> [tag]
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

F0, FC = 5.80e9, 1.50e9
VARIANT = os.environ.get("VARIANT", "RO4350B")
cfg = json.load(open(os.path.join(DESIGN, "synthesis.json")))[VARIANT]
ER, TAND, H = cfg["substrate"]["er"], cfg["substrate"]["tand"], cfg["substrate"]["h_mm"]
WF = cfg["feed"]["w50_mm"]

arg = sys.argv[1] if len(sys.argv) > 1 else "0.891"
GAP = None if arg == "none" else float(arg)
tag = sys.argv[2] if len(sys.argv) > 2 else (arg.replace(".", "p"))

GW = 5.0              # coplanar ground width either side
LINE = 40.0           # line length; the far end runs into the PML
PAD = 12.0            # measurement plane, this far from the driven end
YM = (GW + 2.0) if GAP is not None else 8.0
xmin, xmax = 0.0, LINE
ymin, ymax = -(WF / 2 + (GAP or 0) + YM), (WF / 2 + (GAP or 0) + YM)
zmax = H + 12.0

FDTD = openEMS(NrTS=int(os.environ.get("NRTS", 60000)), EndCriteria=1e-5)
FDTD.SetGaussExcite(F0, FC)
# The line is absorbed at both ends; zmin is the board's own ground plane.
FDTD.SetBoundaryCond(["PML_8", "PML_8", "MUR", "MUR", "PEC", "MUR"])
CSX = ContinuousStructure()
FDTD.SetCSX(CSX)
mesh = CSX.GetGrid()
mesh.SetDeltaUnit(1e-3)

kappa = 2 * np.pi * F0 * 8.8541878128e-12 * ER * TAND
CSX.AddMaterial("sub", epsilon=ER, kappa=kappa).AddBox(
    [xmin, ymin, 0], [xmax, ymax, H], priority=1)

if GAP is not None:
    g = CSX.AddMetal("cogrnd")
    y0 = WF / 2 + GAP
    g.AddBox([xmin, y0, H], [xmax, y0 + GW, H], priority=10)
    g.AddBox([xmin, -y0 - GW, H], [xmax, -y0, H], priority=10)
    # stitching vias, as the connector maker asks for: 0.4 mm at 1.27 mm pitch
    v = CSX.AddMetal("vias")
    for x in np.arange(1.0, LINE, 1.27):
        for s in (+1, -1):
            yc = s * (y0 + 0.85)
            v.AddBox([x - 0.2, yc - 0.2, 0], [x + 0.2, yc + 0.2, H],
                     priority=12)

res = float(os.environ.get('RES', 0.25))
MIN_D = float(os.environ.get('MIND', 0.075))


def clean(v, m=MIN_D):
    out = []
    for x in sorted(float(t) for t in v):
        if not out or x - out[-1] >= m:
            out.append(x)
        else:
            out[-1] = 0.5 * (out[-1] + x)
    return out


xl = list(np.arange(xmin, xmax + 1e-9, 0.30))
yl = [ymin, ymax] + list(np.arange(-WF, WF + 1e-9, res))
for e in (-WF / 2, WF / 2):
    yl += [e - res / 3, e + 2 * res / 3]
if GAP is not None:
    for e in (WF / 2 + GAP, -WF / 2 - GAP, WF / 2 + GAP + GW, -WF / 2 - GAP - GW):
        yl += [e - res / 3, e + 2 * res / 3]
    yl += list(np.arange(WF / 2, WF / 2 + GAP + 1e-9, max(GAP / 4, 0.15)))
    yl += list(np.arange(-WF / 2 - GAP, -WF / 2 + 1e-9, max(GAP / 4, 0.15)))
mesh.AddLine("x", clean(xl))
mesh.AddLine("y", clean(yl))
mesh.SmoothMeshLines("y", 1.6, 1.35)
mesh.AddLine("z", clean(list(np.linspace(0, H, 4)) + [zmax]))
mesh.SmoothMeshLines("z", 1.6, 1.35)

strip = CSX.AddMetal("strip")
port = FDTD.AddMSLPort(1, strip, [xmin, -WF / 2, H], [xmax, WF / 2, 0],
                       "x", "z", excite=-1, FeedShift=4.0, MeasPlaneShift=PAD,
                       priority=15)

if __name__ == "__main__":
    nc = [len(mesh.GetLines(d)) for d in range(3)]
    print(f"[{tag}] gap={arg} mm  mesh {nc[0]}x{nc[1]}x{nc[2]} = "
          f"{nc[0]*nc[1]*nc[2]/1e6:.2f} Mcells", flush=True)
    sp = os.path.join(tempfile.gettempdir(), "oems_lz_" + tag)
    os.makedirs(sp, exist_ok=True)
    FDTD.Run(sp, cleanup=True, verbose=0, numThreads=int(os.environ.get("THREADS", 6)))

    f = np.linspace(4.5e9, 7.0e9, 251)
    port.CalcPort(sp, f, ref_impedance=50)
    # Far end absorbed, so nothing comes back: what the plane sees is the
    # characteristic impedance of this cross-section.
    Z = port.uf_tot / port.if_tot
    k = int(np.argmin(np.abs(f - F0)))
    z58 = Z[k]
    print(f"   Z0 at 5.800 GHz = {z58.real:.2f} {z58.imag:+.2f}j ohm")
    for ff in (5.725e9, 5.875e9):
        kk = int(np.argmin(np.abs(f - ff)))
        print(f"   Z0 at {ff/1e9:.3f} GHz = {Z[kk].real:.2f} ohm")
    g_ = abs((z58.real - 50.0) / (z58.real + 50.0))
    print(f"   mismatch against 50 ohm: {20*np.log10(max(g_,1e-9)):.1f} dB")
    RESULTS = os.path.join(HERE, "results")
    json.dump({"gap": arg, "f": f.tolist(),
               "Z": [Z.real.tolist(), Z.imag.tolist()]},
              open(os.path.join(RESULTS, f"lz_{tag}.json"), "w"))
