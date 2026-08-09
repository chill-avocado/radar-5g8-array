"""openEMS: the 90 degree branch-line hybrid on its own.

Ideal transmission-line theory says the two outputs are exactly 3.01 dB down
and exactly 90 degrees apart.  Real rings are not ideal: each corner is a
microstrip T-junction whose reference plane sits inside the corner, so the
arms behave slightly long.  This run measures the amplitude and phase balance
so the arm lengths can be trimmed before the ring is committed to copper.

Port layout matches how the ring is wired on the board:
    BL (port 1) leaves downward  - input from the SMA
    BR (port 2) leaves downward  - isolated, 50 ohm 0402 on the board
    TL (port 3) leaves upward    - route A, to the x-mode patch edge
    TR (port 4) leaves rightward - route B, to the y-mode patch edge

Usage: python3 hybrid.py [trim_series_mm] [trim_shunt_mm] [tag]
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

F0, FC = 5.80e9, 1.20e9
VARIANT = os.environ.get("VARIANT", "RO4350B")
cfg = json.load(open(os.path.join(DESIGN, "synthesis.json")))[VARIANT]
ER = cfg["substrate"]["er"]
TAND = cfg["substrate"]["tand"]
H = cfg["substrate"]["h_mm"]
WF = cfg["feed"]["w50_mm"]          # 50 ohm port-stub width
WS = cfg["hybrid"]["w_series_mm"]
# Arm widths may be overridden to build an intentionally unbalanced coupler.
WH = float(os.environ.get("ARM_WH", WF))     # horizontal arms
WV = float(os.environ.get("ARM_WV", WS))     # vertical arms

ts = float(sys.argv[1]) if len(sys.argv) > 1 else 0.0
tsh = float(sys.argv[2]) if len(sys.argv) > 2 else 0.0
tag = sys.argv[3] if len(sys.argv) > 3 else "h0"

A = float(os.environ.get("ARM_A", cfg["hybrid"]["l_series_mm"])) + ts
B = float(os.environ.get("ARM_B", cfg["hybrid"]["l_shunt_mm"])) + tsh
xl, xr, yb, yt = 0.0, B, 0.0, A

LINE, PAD, DEEMB = 16.0, 12.0, 2.0
xmin, xmax = xl - LINE, xr + LINE
ymin, ymax = yb - LINE, yt + LINE
AIRZ = 12.0

FDTD = openEMS(NrTS=40000, EndCriteria=1e-4)
FDTD.SetGaussExcite(F0, FC)
FDTD.SetBoundaryCond(["PML_8", "PML_8", "PML_8", "PML_8", "PEC", "MUR"])
CSX = ContinuousStructure()
FDTD.SetCSX(CSX)
mesh = CSX.GetGrid()
mesh.SetDeltaUnit(1e-3)

kappa = 2 * np.pi * F0 * 8.8541878128e-12 * ER * TAND
CSX.AddMaterial("sub", epsilon=ER, kappa=kappa).AddBox(
    [xmin, ymin, 0], [xmax, ymax, H], priority=1)

ring = CSX.AddMetal("ring")
ring.AddBox([xl - WV / 2, yb - WH / 2, H], [xr + WV / 2, yb + WH / 2, H], priority=10)
ring.AddBox([xl - WV / 2, yt - WH / 2, H], [xr + WV / 2, yt + WH / 2, H], priority=10)
ring.AddBox([xl - WV / 2, yb, H], [xl + WV / 2, yt, H], priority=10)
ring.AddBox([xr - WV / 2, yb, H], [xr + WV / 2, yt, H], priority=10)

res, MIN_D = 0.28, 0.075


def clean(v, m=MIN_D):
    out = []
    for x in sorted(float(t) for t in v):
        if not out or x - out[-1] >= m:
            out.append(x)
        else:
            out[-1] = 0.5 * (out[-1] + x)
    return out


xl_ = list(np.arange(xmin, xmax + 1e-9, res))
yl_ = list(np.arange(ymin, ymax + 1e-9, res))
for e in (xl, xr, xl - WV / 2, xl + WV / 2, xr - WV / 2, xr + WV / 2,
          -WF / 2, WF / 2):
    xl_ += [e - res / 3, e + 2 * res / 3]
for e in (yb, yt, yb - WH / 2, yb + WH / 2, yt - WH / 2, yt + WH / 2):
    yl_ += [e - res / 3, e + 2 * res / 3]
mesh.AddLine("x", clean(xl_))
mesh.AddLine("y", clean(yl_))
mesh.AddLine("z", clean(list(np.linspace(0, H, 5)) + [H + AIRZ]))
mesh.SmoothMeshLines("z", 2.0, 1.3)

feed = CSX.AddMetal("feed")
p1 = FDTD.AddMSLPort(1, feed, [xl - WF / 2, ymin, H], [xl + WF / 2, yb, 0],
                     "y", "z", excite=-1, FeedShift=4.0, MeasPlaneShift=PAD,
                     priority=10)
p2 = FDTD.AddMSLPort(2, feed, [xr - WF / 2, ymin, H], [xr + WF / 2, yb, 0],
                     "y", "z", excite=0, FeedShift=4.0, MeasPlaneShift=PAD,
                     priority=10)
p3 = FDTD.AddMSLPort(3, feed, [xl - WF / 2, ymax, H], [xl + WF / 2, yt, 0],
                     "y", "z", excite=0, FeedShift=4.0, MeasPlaneShift=PAD,
                     priority=10)
p4 = FDTD.AddMSLPort(4, feed, [xmax, yt - WF / 2, H], [xr, yt + WF / 2, 0],
                     "x", "z", excite=0, FeedShift=4.0, MeasPlaneShift=PAD,
                     priority=10)
ports = [p1, p2, p3, p4]

if __name__ == "__main__":
    nc = [len(mesh.GetLines(d)) for d in range(3)]
    print(f"[hyb:{tag}] mesh {nc[0]}x{nc[1]}x{nc[2]} = "
          f"{nc[0]*nc[1]*nc[2]/1e6:.2f} Mcells  (A={A:.4f} B={B:.4f})")
    sp = os.path.join(tempfile.gettempdir(), "oems_hyb_" + tag)
    os.makedirs(sp, exist_ok=True)
    FDTD.Run(sp, cleanup=True, verbose=0,
             numThreads=int(os.environ.get("THREADS", 6)))

    f = np.linspace(5.2e9, 6.4e9, 241)
    for p in ports:
        p.CalcPort(sp, f, ref_impedance=50)
    from rfmath import ms_dispersive, RO4350B, FR4_1MM
    SUB = RO4350B if VARIANT == "RO4350B" else FR4_1MM
    ee = np.array([ms_dispersive(WF * 1e-3, SUB, ff)[1] for ff in f])
    beta = 2 * np.pi * f * np.sqrt(ee) / 299792458.0
    # every port line is measured PAD from its far end; de-embed to the ring
    d = (LINE - PAD) * 1e-3
    rot = np.exp(2j * beta * d)
    inc = ports[0].uf_inc * np.exp(-1j * beta * d)
    S = [ports[0].uf_ref * rot / ports[0].uf_inc]
    for p in ports[1:]:
        S.append(p.uf_ref * np.exp(1j * beta * d) / inc)
    S = np.array(S)

    k = int(np.argmin(np.abs(f - F0)))
    nm = ["S11 BL in ", "S21 BR iso", "S31 TL out", "S41 TR out"]
    for i in range(4):
        print(f"   {nm[i]}: {20*np.log10(abs(S[i, k])):7.2f} dB  "
              f"{np.degrees(np.angle(S[i, k])):8.2f} deg")
    amp = 20 * np.log10(abs(S[2, k]) / abs(S[3, k]))
    ph = np.degrees(np.angle(S[2, k]) - np.angle(S[3, k]))
    ph = (ph + 180) % 360 - 180
    print(f"   amplitude imbalance TL-TR : {amp:+.3f} dB")
    print(f"   phase difference    TL-TR : {ph:+.2f} deg  (want +90)")
    ar = abs(1 / np.tan(np.radians((abs(ph)) / 2)))
    r = 10 ** (amp / 20)
    print(f"   -> axial ratio from the coupler alone: "
          f"{20*np.log10(max(r*ar, 1/(r*ar))):.2f} dB")
    RESULTS = os.path.join(HERE, "results")
    json.dump({"f": f.tolist(),
               "S": [[S[i].real.tolist(), S[i].imag.tolist()] for i in range(4)],
               "A": A, "B": B},
              open(os.path.join(RESULTS, f"hyb_{tag}.json"), "w"))
