"""openEMS: two-port model of the square patch with quarter-wave feeds.

Feed chain per port:   50 ohm microstrip -> quarter-wave transformer -> patch

Measured facts that shaped this topology (all from earlier runs of this file):
  * the transmission-line model's 587 ohm patch edge resistance is wrong; the
    real figure is 311 ohm, so the transformer only has to span 50 -> ~200
  * a deep 3.7 mm inset drags resonance down 11 % AND wrecks the orthogonality
    of the two feeds (S21 -8.6 dB instead of -37.6 dB), which is fatal for
    circular polarisation, so the inset stays shallow and a transformer does
    the impedance work instead

Usage:  python3 patch2p.py <inset_mm> <patch_L_mm> [tag] [wq_mm] [lq_mm]
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
from rfmath import ms_dispersive, RO4350B                    # noqa: E402

F0, FC, UNIT = 5.80e9, 1.20e9, 1e-3
cfg = json.load(open(os.path.join(DESIGN, "synthesis.json")))["RO4350B"]
ER = cfg["substrate"]["er"]
TAND = cfg["substrate"]["tand"]
H = cfg["substrate"]["h_mm"]
WF = cfg["feed"]["w50_mm"]
GAP = 0.40

inset = float(sys.argv[1]) if len(sys.argv) > 1 else 2.70
L = float(sys.argv[2]) if len(sys.argv) > 2 else 13.087
tag = sys.argv[3] if len(sys.argv) > 3 else "run"
WQ = float(sys.argv[4]) if len(sys.argv) > 4 else 0.356
LQ = float(sys.argv[5]) if len(sys.argv) > 5 else 7.841

L2 = L / 2.0
FX = -L2 + inset                 # transformer meets the patch here
TX = FX - LQ                     # transformer input = end of the 50 ohm line
DEEMB, PAD = 2.0, 14.0
AIR, MARGIN = 14.0, 14.0
xmin = ymin = TX - DEEMB - PAD
xmax = ymax = L2 + MARGIN
zmax = H + AIR

FDTD = openEMS(NrTS=60000, EndCriteria=1e-4)
FDTD.SetGaussExcite(F0, FC)
FDTD.SetBoundaryCond(["PML_8", "MUR", "PML_8", "MUR", "PEC", "MUR"])
CSX = ContinuousStructure()
FDTD.SetCSX(CSX)
mesh = CSX.GetGrid()
mesh.SetDeltaUnit(UNIT)

kappa = 2.0 * np.pi * F0 * 8.8541878128e-12 * ER * TAND
CSX.AddMaterial("RO4350B", epsilon=ER, kappa=kappa).AddBox(
    [xmin, ymin, 0], [xmax, ymax, H], priority=1)

# ------------------------------------------------------------------- copper
patch = CSX.AddMetal("patch")
nh = (WQ + 2.0 * GAP) / 2.0
if inset > 1e-6:
    pts = [(-L2, -L2),
           (-nh, -L2), (-nh, FX), (nh, FX), (nh, -L2),
           (L2, -L2), (L2, L2), (-L2, L2),
           (-L2, nh), (FX, nh), (FX, -nh), (-L2, -nh)]
else:
    pts = [(-L2, -L2), (L2, -L2), (L2, L2), (-L2, L2)]
patch.AddPolygon([[p[0] for p in pts], [p[1] for p in pts]],
                 norm_dir=2, elevation=H, priority=10)

trans = CSX.AddMetal("trans")
trans.AddBox([TX, -WQ / 2.0, H], [FX, WQ / 2.0, H], priority=10)   # -x feed
trans.AddBox([-WQ / 2.0, TX, H], [WQ / 2.0, FX, H], priority=10)   # -y feed

# --------------------------------------------------------------------- mesh
res_fine, res_air, res_feed = 0.22, 2.00, 0.30
third, MIN_D = res_fine / 3.0, 0.075


def clean(lines, min_d=MIN_D):
    out = []
    for v in sorted(float(x) for x in lines):
        if not out or v - out[-1] >= min_d:
            out.append(v)
        else:
            out[-1] = 0.5 * (out[-1] + v)
    return out


edges = sorted({-L2, L2, FX, TX, -nh, nh,
                -WF / 2.0, WF / 2.0, -WQ / 2.0, WQ / 2.0})
xl = ([xmax] + list(np.arange(xmin, -L2 + 1e-9, res_feed))
      + list(np.arange(-L2, L2 + 1e-9, res_fine)))
for e in edges:
    xl += [e - third, e + 2 * third]
# ~4 cells across the transformer strip, no finer: the FDTD timestep is set
# by the smallest cell anywhere in the grid.
xl += list(np.linspace(-WQ / 2.0, WQ / 2.0, 4))
yl = list(xl)
mesh.AddLine("x", clean(xl))
mesh.AddLine("y", clean(yl))
mesh.SmoothMeshLines("x", res_air, 1.30)
mesh.SmoothMeshLines("y", res_air, 1.30)
mesh.AddLine("z", clean(list(np.linspace(0, H, 5)) + [zmax]))
mesh.SmoothMeshLines("z", res_air, 1.30)
FDTD.AddEdges2Grid(dirs="xy", properties=patch, metal_edge_res=res_fine / 2)

# ---------------------------------------------------------------- MSL ports
feedline = CSX.AddMetal("feed")
p1 = FDTD.AddMSLPort(1, feedline, [xmin, -WF / 2.0, H], [TX, WF / 2.0, 0],
                     "x", "z", excite=-1, FeedShift=4.0,
                     MeasPlaneShift=PAD, priority=10)
p2 = FDTD.AddMSLPort(2, feedline, [-WF / 2.0, ymin, H], [WF / 2.0, TX, 0],
                     "y", "z", excite=0, FeedShift=4.0,
                     MeasPlaneShift=PAD, priority=10)
port = [p1, p2]

if __name__ == "__main__":
    nc = [len(mesh.GetLines(d)) for d in range(3)]
    print(f"[{tag}] mesh {nc[0]}x{nc[1]}x{nc[2]} = "
          f"{nc[0]*nc[1]*nc[2]/1e6:.2f} Mcells  "
          f"(L={L:.3f} inset={inset:.2f} Wq={WQ:.3f} Lq={LQ:.3f})")
    sim_path = os.path.join(tempfile.gettempdir(), "oems_p2p_" + tag)
    os.makedirs(sim_path, exist_ok=True)
    FDTD.Run(sim_path, cleanup=True, verbose=0,
             numThreads=int(os.environ.get("THREADS", 6)))

    f = np.linspace(5.0e9, 6.6e9, 401)
    for p in port:
        p.CalcPort(sim_path, f, ref_impedance=50)
    S11m = port[0].uf_ref / port[0].uf_inc
    S21m = port[1].uf_ref / port[0].uf_inc
    Zm = port[0].uf_tot / port[0].if_tot

    # de-embed the 2 mm of 50 ohm line back to the transformer input
    ee = np.array([ms_dispersive(WF * 1e-3, RO4350B, ff)[1] for ff in f])
    beta = 2.0 * np.pi * f * np.sqrt(ee) / 299792458.0
    t = np.tan(beta * DEEMB * 1e-3)
    Zin = 50.0 * (Zm - 1j * 50.0 * t) / (50.0 - 1j * Zm * t)
    rot = np.exp(2j * beta * DEEMB * 1e-3)
    S11, S21 = S11m * rot, S21m * rot

    RESULTS = os.path.join(HERE, "results")
    json.dump({"tag": tag, "inset": inset, "L": L, "WQ": WQ, "LQ": LQ,
               "f": f.tolist(),
               "S11": [S11.real.tolist(), S11.imag.tolist()],
               "S21": [S21.real.tolist(), S21.imag.tolist()],
               "Zin": [Zin.real.tolist(), Zin.imag.tolist()]},
              open(os.path.join(RESULTS, f"p2p_{tag}.json"), "w"))

    k = int(np.argmin(np.abs(S11)))
    k0 = int(np.argmin(np.abs(f - F0)))
    b = np.abs(S11) < 10 ** (-10 / 20)
    bw = (f[b].max() - f[b].min()) / 1e6 if b.any() else 0.0
    print(f"   best match  {f[k]/1e9:.4f} GHz  S11 {20*np.log10(abs(S11[k])):6.2f} dB")
    print(f"   at 5.800GHz S11 {20*np.log10(abs(S11[k0])):6.2f} dB   "
          f"Zin {Zin[k0].real:6.1f}{Zin[k0].imag:+6.1f}j   "
          f"S21 {20*np.log10(abs(S21[k0])):6.2f} dB")
    print(f"   -10 dB bandwidth {bw:.0f} MHz")
