"""openEMS: what the receive-line protection site costs, empty and fitted.

The board ships with the limiter site unpopulated, so the first thing to know
is that an empty site is invisible.  The second is what fitting a diode does
to the match, because a shunt limiter's off-state capacitance sits straight
across a 50 ohm line at 5.8 GHz and is not free.

Usage:  python3 protect_site.py <none | C_pF> [tag]
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
cfg = json.load(open(os.path.join(DESIGN, "synthesis.json")))["RO4350B"]
ER, TAND, H = cfg["substrate"]["er"], cfg["substrate"]["tand"], cfg["substrate"]["h_mm"]
WF = cfg["feed"]["w50_mm"]

arg = sys.argv[1] if len(sys.argv) > 1 else "none"
CPF = None if arg == "none" else float(arg)
tag = sys.argv[2] if len(sys.argv) > 2 else arg.replace(".", "p")

# the site exactly as array.py draws it
STUB_W, STUB_L = 0.50, 0.70
PW, PH, GAP = 0.60, 0.65, 0.55
LINE, PAD = 34.0, 10.0
XS = LINE / 2.0                       # site position along the line

ytrace = -WF / 2.0
ysig = ytrace - STUB_L - PH / 2.0
ygnd = ysig - PH - GAP

xmin, xmax = 0.0, LINE
ymin, ymax = ygnd - 6.0, WF / 2.0 + 6.0
zmax = H + 12.0

FDTD = openEMS(NrTS=int(os.environ.get("NRTS", 60000)), EndCriteria=1e-5)
FDTD.SetGaussExcite(F0, FC)
FDTD.SetBoundaryCond(["PML_8", "PML_8", "MUR", "MUR", "PEC", "MUR"])
CSX = ContinuousStructure()
FDTD.SetCSX(CSX)
mesh = CSX.GetGrid()
mesh.SetDeltaUnit(1e-3)

kappa = 2 * np.pi * F0 * 8.8541878128e-12 * ER * TAND
CSX.AddMaterial("sub", epsilon=ER, kappa=kappa).AddBox(
    [xmin, ymin, 0], [xmax, ymax, H], priority=1)

site = CSX.AddMetal("site")
site.AddBox([XS - STUB_W / 2, ysig + PH / 2, H],
            [XS + STUB_W / 2, ytrace, H], priority=10)          # stub
site.AddBox([XS - PW / 2, ysig - PH / 2, H],
            [XS + PW / 2, ysig + PH / 2, H], priority=10)       # signal pad
site.AddBox([XS - PW / 2, ygnd - PH / 2, H],
            [XS + PW / 2, ygnd + PH / 2, H], priority=10)       # ground pad
gp = CSX.AddMetal("sitegnd")
gp.AddBox([XS - 1.35, ygnd - 1.9, H], [XS + 1.35, ygnd + PH / 2, H],
          priority=9)
for dx in (-0.75, 0.75):                                        # ground vias
    gp.AddBox([XS + dx - 0.2, ygnd - 1.45, 0],
              [XS + dx + 0.2, ygnd - 1.05, H], priority=12)

if CPF is not None:
    # a fitted limiter's off-state capacitance, straight across the gap
    CSX.AddLumpedElement("Cd", ny="y", caps=True, C=CPF * 1e-12).AddBox(
        [XS - PW / 2, ygnd + PH / 2, H - 0.001],
        [XS + PW / 2, ysig - PH / 2, H], priority=20)

res = float(os.environ.get('RES', 0.22))
MIN_D = float(os.environ.get('MIND', 0.06))


def clean(v, m=MIN_D):
    out = []
    for x in sorted(float(t) for t in v):
        if not out or x - out[-1] >= m:
            out.append(x)
        else:
            out[-1] = 0.5 * (out[-1] + x)
    return out


xl = list(np.arange(xmin, xmax + 1e-9, 0.30))
for e in (XS - STUB_W / 2, XS + STUB_W / 2, XS - PW / 2, XS + PW / 2,
          XS - 1.35, XS + 1.35):
    xl += [e - res / 3, e + 2 * res / 3]
yl = [ymin, ymax] + list(np.arange(ygnd - 2.0, WF / 2 + 1.0, res))
for e in (-WF / 2, WF / 2, ysig - PH / 2, ysig + PH / 2,
          ygnd - PH / 2, ygnd + PH / 2, ytrace):
    yl += [e - res / 3, e + 2 * res / 3]
mesh.AddLine("x", clean(xl))
mesh.AddLine("y", clean(yl))
mesh.SmoothMeshLines("x", 1.6, 1.35)
mesh.SmoothMeshLines("y", 1.6, 1.35)
mesh.AddLine("z", clean(list(np.linspace(0, H, 4)) + [zmax]))
mesh.SmoothMeshLines("z", 1.6, 1.35)

strip = CSX.AddMetal("strip")
p1 = FDTD.AddMSLPort(1, strip, [xmin, -WF / 2, H], [XS, WF / 2, 0],
                     "x", "z", excite=-1, FeedShift=4.0, MeasPlaneShift=PAD,
                     priority=15)
p2 = FDTD.AddMSLPort(2, strip, [xmax, -WF / 2, H], [XS, WF / 2, 0],
                     "x", "z", excite=0, FeedShift=4.0, MeasPlaneShift=PAD,
                     priority=15)

if __name__ == "__main__":
    nc = [len(mesh.GetLines(d)) for d in range(3)]
    print(f"[{tag}] site {'empty' if CPF is None else f'fitted, {CPF} pF'}   "
          f"mesh {nc[0]}x{nc[1]}x{nc[2]} = {nc[0]*nc[1]*nc[2]/1e6:.2f} Mcells",
          flush=True)
    sp = os.path.join(tempfile.gettempdir(), "oems_ps_" + tag)
    os.makedirs(sp, exist_ok=True)
    FDTD.Run(sp, cleanup=True, verbose=0,
             numThreads=int(os.environ.get("THREADS", 6)))

    f = np.linspace(5.0e9, 6.6e9, 161)
    for p in (p1, p2):
        p.CalcPort(sp, f, ref_impedance=50)
    s11 = p1.uf_ref / p1.uf_inc
    s21 = p2.uf_ref / p1.uf_inc
    k = int(np.argmin(np.abs(f - F0)))
    print(f"   at 5.800 GHz: return loss {20*np.log10(abs(s11[k])):6.2f} dB, "
          f"through loss {20*np.log10(abs(s21[k])):5.2f} dB")
    b = (f >= 5.725e9) & (f <= 5.875e9)
    print(f"   worst in band: return loss "
          f"{20*np.log10(np.abs(s11[b])).max():6.2f} dB, through loss "
          f"{20*np.log10(np.abs(s21[b])).min():5.2f} dB")
    RESULTS = os.path.join(HERE, "results")
    json.dump({"C_pF": CPF, "f": f.tolist(),
               "s11": [s11.real.tolist(), s11.imag.tolist()],
               "s21": [s21.real.tolist(), s21.imag.tolist()]},
              open(os.path.join(RESULTS, f"ps_{tag}.json"), "w"))
