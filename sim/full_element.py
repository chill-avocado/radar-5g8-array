"""openEMS: the COMPLETE circularly-polarised element, exactly as drawn.

This is the verification run.  It takes the same polygon list that the KiCad
generator consumes, so the thing simulated here is the thing fabricated: the
patch, both quarter-wave transformers, the branch-line hybrid with its real
junction discontinuities, the length-matched routes, and a lumped 50 ohm
resistor on the isolated port.  One microstrip port at the input.

Outputs: input match, boresight axial ratio, realised gain, and the two
principal-plane patterns.

Usage: python3 full_element.py [tag] [dip_trim_mm] [patch_L_mm]
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
from element2 import Element                                  # noqa: E402

F0, FC = 5.80e9, 0.85e9
VARIANT = os.environ.get("VARIANT", "RO4350B")
cfg = json.load(open(os.path.join(DESIGN, "synthesis.json")))[VARIANT]
ER = cfg["substrate"]["er"]
TAND = cfg["substrate"]["tand"]
H = cfg["substrate"]["h_mm"]
WF = cfg["feed"]["w50_mm"]

tag = sys.argv[1] if len(sys.argv) > 1 else "fe0"
dip_trim = float(sys.argv[2]) if len(sys.argv) > 2 else cfg["tuned"]["dip_trim"]
Lp = float(sys.argv[3]) if len(sys.argv) > 3 else cfg["tuned"]["L"]
dLx = float(sys.argv[4]) if len(sys.argv) > 4 else -0.2145
dLy = float(sys.argv[5]) if len(sys.argv) > 5 else -0.1155

el = Element(cfg, L=Lp, dip_trim=dip_trim, dLx=dLx, dLy=dLy)
polys = el.build()
bb = el.bbox()

FEED = 14.0                       # 50 ohm run from the hybrid to the board edge
PAD = 10.0
# The ground plane must reach at least a half wavelength past the patch.  At
# 4 mm the diffracted field from the nearby ground edge was comparable to the
# patch's own radiation, which collapsed the directivity to 3 dBi and the
# axial ratio to 12 dB -- an artefact of the model, not of the antenna.
# How far the ground plane runs past the patch.  This is what sets the board
# size and therefore most of the cost, so it is measured rather than assumed:
# too little and the energy runs off the edge and diffracts, taking the gain
# and the polarisation with it.
GND_M = float(os.environ.get("GNDM", 25.0))
AIR = 13.0                        # ~lambda0/4 stand-off for the MUR walls

gx0, gx1 = bb[0] - GND_M, bb[2] + GND_M
gy0, gy1 = el.input_pt[1] - 3.0, bb[3] + GND_M
xmin, xmax = gx0 - AIR, gx1 + AIR
ymin, ymax = gy0 - AIR, gy1 + AIR
zmin, zmax = -AIR, H + AIR

# Let the energy criterion decide when to stop.  At 45000 the solver
# warned the run was under three excitation lengths and it was still
# at -30 dB, which is not converged.
NRTS = int(os.environ.get("NRTS", 140000))
FDTD = openEMS(NrTS=NRTS, EndCriteria=1e-4)
FDTD.SetGaussExcite(F0, FC)
# Only the boundary the microstrip runs into needs a PML; a graded PML on all
# six faces made 78 % of the cells absorber and cost an order of magnitude in
# speed.  MUR at a quarter-wave stand-off is ample for the rest.
# Nothing crosses a boundary now, so plain MUR at a quarter-wave stand-off is
# right on all six faces and the near-field box closes cleanly.
FDTD.SetBoundaryCond(["MUR"] * 6)
CSX = ContinuousStructure()
FDTD.SetCSX(CSX)
mesh = CSX.GetGrid()
mesh.SetDeltaUnit(1e-3)

kappa = 2 * np.pi * F0 * 8.8541878128e-12 * ER * TAND
CSX.AddMaterial("sub", epsilon=ER, kappa=kappa).AddBox(
    [gx0, gy0, 0], [gx1, gy1, H], priority=1)
CSX.AddMetal("gnd").AddBox([gx0, gy0, 0], [gx1, gy1, 0], priority=10)

top = CSX.AddMetal("top")
for p in polys:
    top.AddPolygon([[q[0] for q in p], [q[1] for q in p]],
                   norm_dir=2, elevation=H, priority=10)

# isolated port: 50 ohm 0402 from the stub end down to the ground plane
ix, iy = el.iso_pt
top.AddBox([ix - WF / 2, iy - 1.0, H], [ix + WF / 2, iy, H], priority=10)
res = CSX.AddLumpedElement("R50", ny="z", caps=True, R=50.0)
res.AddBox([ix - WF / 2, iy - 1.6, 0], [ix + WF / 2, iy - 1.0, H], priority=20)

# --------------------------------------------------------------------- mesh
res_c, res_air, MIN_D = 0.30, 2.20, 0.075


def clean(v, m=MIN_D):
    out = []
    for x in sorted(float(t) for t in v):
        if not out or x - out[-1] >= m:
            out.append(x)
        else:
            out[-1] = 0.5 * (out[-1] + x)
    return out


ex, ey = set(), set()
for p in polys:
    for q in p:
        ex.add(round(q[0], 4)); ey.add(round(q[1], 4))
# Fine cells only over the copper; the bare ground plane and the air are
# graded out, which keeps the cell count affordable on a big ground plane.
cx0, cx1 = bb[0] - 1.0, bb[2] + 1.0
cy0, cy1 = el.input_pt[1] - 2.5, bb[3] + 1.0
xl = list(np.arange(cx0, cx1 + 1e-9, res_c)) + [xmin, gx0, gx1, xmax]
yl = list(np.arange(cy0, cy1 + 1e-9, res_c)) + [ymin, gy0, gy1, ymax]
for e in ex:
    xl += [e - res_c / 3, e + 2 * res_c / 3]
for e in ey:
    yl += [e - res_c / 3, e + 2 * res_c / 3]
# extra lines across the narrow transformers
xl += list(np.linspace(-el.wq / 2, el.wq / 2, 4))
yl += list(np.linspace(-el.wq / 2, el.wq / 2, 4))
mesh.AddLine("x", clean(xl))
mesh.AddLine("y", clean(yl))
mesh.SmoothMeshLines("x", res_air, 1.35)
mesh.SmoothMeshLines("y", res_air, 1.35)
mesh.AddLine("z", clean([zmin] + list(np.linspace(0, H, 4)) + [zmax]))
mesh.SmoothMeshLines("z", res_air, 1.35)

# A lumped port from the end of the input stub straight down to the ground
# plane.  A microstrip port would have to run out through a domain wall, and
# the near-field box would then be slicing through a live feed line -- which
# is what made the first attempt report negative radiated power.  The accurate
# microstrip S-parameters come from the dedicated two-port runs; what this
# model is for is the pattern and the axial ratio.
px, py = el.input_pt
CSX.AddMetal("feedline").AddBox([px - WF / 2, py - 1.2, H],
                                [px + WF / 2, py, H], priority=10)
port = FDTD.AddLumpedPort(1, 50.0, [px - WF / 2, py - 1.9, 0],
                          [px + WF / 2, py - 1.2, H], "z", excite=1,
                          priority=20)

# Record the near field only at the three frequencies of interest.  Left to
# its default the box dumps the full time-domain field every step, which is
# both enormously slower and gigabytes of disk.
FFREQ = [float(v) for v in os.environ.get(
    "FFREQ", "5.65e9,5.70e9,5.75e9,5.80e9,5.85e9,5.90e9,5.95e9"
).split(",")]
nf2ff = FDTD.CreateNF2FFBox(frequency=FFREQ)

if __name__ == "__main__":
    nc = [len(mesh.GetLines(d)) for d in range(3)]
    print(f"[{tag}] mesh {nc[0]}x{nc[1]}x{nc[2]} = "
          f"{nc[0]*nc[1]*nc[2]/1e6:.2f} Mcells   board {gx1-gx0:.1f} x "
          f"{gy1-gy0:.1f} mm  (L={Lp} dip_trim={dip_trim})", flush=True)
    sp = os.path.join(tempfile.gettempdir(), "oems_fe_" + tag)
    os.makedirs(sp, exist_ok=True)
    FDTD.Run(sp, cleanup=False, verbose=0, numThreads=int(os.environ.get("THREADS", 6)))

    f = np.linspace(5.3e9, 6.3e9, 201)
    port.CalcPort(sp, f, ref_impedance=50)
    s11 = port.uf_ref / port.uf_inc
    k0 = int(np.argmin(np.abs(f - F0)))

    fff = np.array(FFREQ)
    theta = np.arange(-90, 90.5, 1.0)
    out = {"tag": tag, "L": Lp, "dip_trim": dip_trim,
           "f": f.tolist(), "s11": [s11.real.tolist(), s11.imag.tolist()],
           "ff": {}}
    for ff in fff:
        r = nf2ff.CalcNF2FF(sp, ff, theta, [0.0, 90.0],
                            center=[0, 0, H / 2 * 1e-3],
                            outfile=f"nf_{tag}_{ff/1e9:.3f}.h5",
                            read_cached=False, verbose=0)
        Et = np.array(r.E_theta[0])        # shape (n_theta, n_phi)
        Ep = np.array(r.E_phi[0])
        ER = (Et + 1j * Ep) / np.sqrt(2.0)
        EL = (Et - 1j * Ep) / np.sqrt(2.0)
        i0 = int(np.argmin(np.abs(theta)))
        a_, b_ = float(np.abs(ER[i0, 0])), float(np.abs(EL[i0, 0]))
        arr = max((a_ + b_) / max(a_ - b_, 1e-15), 1e-12)
        out["ff"][f"{ff/1e9:.3f}"] = {
            "Dmax_dBi": float(10 * np.log10(r.Dmax[0])),
            "P_rad": float(r.Prad[0]),
            "AR_dB": float(20 * np.log10(abs(arr))),
            "theta": theta.tolist(),
            "dip_trim": dip_trim, "dLx": dLx, "dLy": dLy,
            "ER_E": np.abs(ER[:, 0]).tolist(), "EL_E": np.abs(EL[:, 0]).tolist(),
            "ER_H": np.abs(ER[:, 1]).tolist(), "EL_H": np.abs(EL[:, 1]).tolist(),
        }
        print(f"   {ff/1e9:.3f} GHz : S11 "
              f"{20*np.log10(abs(s11[int(np.argmin(np.abs(f-ff)))])):6.2f} dB   "
              f"Dmax {10*np.log10(r.Dmax[0]):5.2f} dBi   "
              f"axial ratio {20*np.log10(abs(arr)):5.2f} dB", flush=True)
    RESULTS = os.path.join(HERE, "results")
    json.dump(out, open(os.path.join(RESULTS, f"fe_{tag}.json"), "w"))
    b = np.abs(s11) < 10 ** (-10 / 20)
    print(f"   input match at 5.800 GHz : {20*np.log10(abs(s11[k0])):.2f} dB")
    if b.any():
        print(f"   -10 dB band : {f[b].min()/1e9:.3f} - {f[b].max()/1e9:.3f} GHz "
              f"({(f[b].max()-f[b].min())/1e6:.0f} MHz)")
