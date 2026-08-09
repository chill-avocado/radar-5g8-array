"""openEMS: two adjacent elements at the half-wave pitch, on the plate.

The single-element runs told us what one antenna does in isolation.  This one
answers the question the array actually poses: at 25.844 mm pitch there is only
2.8 mm between one element's patch and its neighbour's coupler, so how much
does one leak into the other, and does having a neighbour spoil the circular
polarisation?

Port 1 drives the left element.  Port 2 sits on the right element and is left
terminated in 50 ohm, so the far field this run reports is the EMBEDDED element
pattern -- the pattern an element really has once its neighbour is beside it,
which is the one that matters for beamforming.

Usage: python3 array_sim.py [tag]
"""

import json
import os
import sys
import tempfile

import numpy as np
from CSXCAD import ContinuousStructure
from openEMS import openEMS

HERE = os.path.dirname(os.path.abspath(__file__))
DESIGN = HERE
sys.path.insert(0, DESIGN)
TOPO = os.environ.get("TOPO", "square")
if TOPO == "diamond":
    from element3 import Element                              # noqa: E402
else:
    from element2 import Element                              # noqa: E402

F0, FC = 5.80e9, 0.85e9
VARIANT = os.environ.get("VARIANT", "ZYF300CA")
cfg = json.load(open(os.path.join(DESIGN, "synthesis.json")))[VARIANT]
ER = float(os.environ.get("ERSUB", cfg["substrate"]["er"]))
TAND = cfg["substrate"]["tand"]
H = cfg["substrate"]["h_mm"]
WF = cfg["feed"]["w50_mm"]


# The tuned block can be overridden from the environment, so a sweep never
# has to edit the design file it is measuring.
for _k in ("w_arm_v", "w_arm_h", "arm_series", "arm_shunt", "dip_trim", "L"):
    _v = os.environ.get("T_" + _k.upper())
    if _v is not None:
        cfg["tuned"][_k] = float(_v)
DLX = float(os.environ.get("DLX", cfg["tuned"].get("dLx", 0.0)))
DLY = float(os.environ.get("DLY", cfg["tuned"].get("dLy", 0.0)))

tag = sys.argv[1] if len(sys.argv) > 1 else "c2"
# Which element is driven. The two are NOT in the same environment:
# element 0 has its neighbour's feed network 2.8 mm from its patch,
# while element 1 has only open board on its far side.
DRIVE = int(os.environ.get("DRIVE", 0))
PITCH = float(os.environ.get("PITCH_MM", 0)) or 299792458.0 / F0 / 2.0 * 1e3

# The pair is mirror-symmetric: element 2 is the mirrored variant, so the two
# feed networks face outward and the two patches face each other across clear
# board.  Without that, each element's route-A riser runs 3 mm from the
# neighbouring patch's radiating edge, which wrecks the axial ratio.
MIRROR = os.environ.get("MIRROR", "1") == "1"
el = Element(cfg, dLx=DLX, dLy=DLY)
DTM = os.environ.get("DIP_MIRROR")
# The diamond has no dip and therefore no dip trim -- its two feeds match by
# shape, which is the whole reason for turning the patch.
_kw = {} if TOPO == "diamond" else {
    "dip_trim_mirror": float(DTM) if DTM else None}
e2 = (Element(cfg, mirror=True, dLx=DLX, dLy=DLY, **_kw)
      if MIRROR else Element(cfg, dLx=DLX, dLy=DLY))
base, base2 = el.build(), e2.build()
bb0, bb1 = el.bbox(), e2.bbox()
polys = [list(p) for p in base] + \
        [[(x + PITCH, y) for (x, y) in p] for p in base2]
inputs = [el.input_pt, (e2.input_pt[0] + PITCH, e2.input_pt[1])]
isos = [el.iso_pt, (e2.iso_pt[0] + PITCH, e2.iso_pt[1])]

bb = (bb0[0], min(bb0[1], bb1[1]), bb1[2] + PITCH, max(bb0[3], bb1[3]))
GND_M, AIR = 25.0, 13.0
# 25 mm all round is the real ground: each board bolts flat onto an aluminium
# plate cut to give at least that much past every patch edge.  Set GNDMX /
# GNDMY smaller to see what the bare laminate alone would have done.
GMX = float(os.environ.get("GNDMX", GND_M))
GMY = float(os.environ.get("GNDMY", GND_M))
GMB = float(os.environ.get("GNDMB", 3.0))
gx0, gx1 = bb[0] - GMX, bb[2] + GMX
# The lower edge used to be pinned to the feed point, which on the flat-on
# element hangs 26 mm below the patch and so happened to give a sensible
# ground.  The diamond feeds from the side, level with the patch, so the same
# rule gave it a 33 mm ground instead of the 70 mm the plate actually
# provides -- and would have flattered nothing, it would have wrecked the
# axial ratio.  Measure from the copper, not from the feed.
gy0 = min(el.input_pt[1] - GMB, bb[1] - GMY)
gy1 = bb[3] + GMY
xmin, xmax = gx0 - AIR, gx1 + AIR
ymin, ymax = gy0 - AIR, gy1 + AIR
zmin, zmax = -AIR, H + AIR

# Let the energy criterion decide when to stop.  At 45000 the solver
# warned the run was under three excitation lengths and it was still
# at -30 dB, which is not converged.
NRTS = int(os.environ.get("NRTS", 140000))
FDTD = openEMS(NrTS=NRTS, EndCriteria=1e-4)
FDTD.SetGaussExcite(F0, FC)
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

# 50 ohm termination on each element's isolated port
for n, (ix, iy) in enumerate(isos):
    top.AddBox([ix - WF / 2, iy - 1.0, H], [ix + WF / 2, iy, H], priority=10)
    r = CSX.AddLumpedElement(f"R50_{n}", ny="z", caps=True, R=50.0)
    r.AddBox([ix - WF / 2, iy - 1.6, 0], [ix + WF / 2, iy - 1.0, H],
             priority=20)

# --------------------------------------------------------------------- mesh
# Mesh density is overridable so the result can be shown to be
# converged rather than assumed to be.
res_c = float(os.environ.get('RES', 0.30))
res_air = float(os.environ.get('RESAIR', 2.20))
MIN_D = float(os.environ.get('MIND', 0.075))


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
cx0, cx1 = bb[0] - 1.0, bb[2] + 1.0
cy0, cy1 = el.input_pt[1] - 2.5, bb[3] + 1.0
xl = list(np.arange(cx0, cx1 + 1e-9, res_c)) + [xmin, gx0, gx1, xmax]
yl = list(np.arange(cy0, cy1 + 1e-9, res_c)) + [ymin, gy0, gy1, ymax]
for e in ex:
    xl += [e - res_c / 3, e + 2 * res_c / 3]
for e in ey:
    yl += [e - res_c / 3, e + 2 * res_c / 3]
for off in (0.0, PITCH):
    xl += list(np.linspace(off - el.wq / 2, off + el.wq / 2, 4))
xl += list(np.linspace(PITCH - el.wq / 2, PITCH + el.wq / 2, 4))
yl += list(np.linspace(-el.wq / 2, el.wq / 2, 4))
mesh.AddLine("x", clean(xl))
mesh.AddLine("y", clean(yl))
mesh.SmoothMeshLines("x", res_air, 1.35)
mesh.SmoothMeshLines("y", res_air, 1.35)
mesh.AddLine("z", clean([zmin] + list(np.linspace(0, H, 4)) + [zmax]))
mesh.SmoothMeshLines("z", res_air, 1.35)

ports = []
for n, (px, py) in enumerate(inputs):
    CSX.AddMetal(f"feed{n}").AddBox([px - WF / 2, py - 1.2, H],
                                    [px + WF / 2, py, H], priority=10)
    ports.append(FDTD.AddLumpedPort(
        n + 1, 50.0, [px - WF / 2, py - 1.9, 0], [px + WF / 2, py - 1.2, H],
        "z", excite=(1 if n == DRIVE else 0), priority=20))

FFREQ = [float(v) for v in os.environ.get(
    "FFREQ", "5.725e9,5.800e9,5.875e9").split(",")]
nf2ff = FDTD.CreateNF2FFBox(frequency=FFREQ)

if __name__ == "__main__":
    nc = [len(mesh.GetLines(d)) for d in range(3)]
    print(f"[{tag}] mesh {nc[0]}x{nc[1]}x{nc[2]} = "
          f"{nc[0]*nc[1]*nc[2]/1e6:.2f} Mcells   ground {gx1-gx0:.1f} x "
          f"{gy1-gy0:.1f} mm   pitch {PITCH:.4f} mm", flush=True)
    sp = os.path.join(tempfile.gettempdir(), "oems_c2_" + tag)
    os.makedirs(sp, exist_ok=True)
    FDTD.Run(sp, cleanup=False, verbose=0, numThreads=int(os.environ.get("THREADS", 6)))

    f = np.linspace(5.4e9, 6.2e9, 161)
    for p in ports:
        p.CalcPort(sp, f, ref_impedance=50)
    d, o = DRIVE, 1 - DRIVE
    s11 = ports[d].uf_ref / ports[d].uf_inc
    s21 = ports[o].uf_ref / ports[d].uf_inc

    theta = np.arange(-90, 90.5, 1.0)
    out = {"tag": tag, "pitch_mm": PITCH, "f": f.tolist(),
           "s11": [s11.real.tolist(), s11.imag.tolist()],
           "s21": [s21.real.tolist(), s21.imag.tolist()], "ff": {}}
    print(f"\n{'freq':>9} {'S11':>9} {'S21 coupling':>14} {'AR embedded':>13}"
          f" {'Dmax':>9}")
    for ff in FFREQ:
        r = nf2ff.CalcNF2FF(sp, ff, theta, [0.0, 90.0],
                            center=[PITCH / 2 * 1e-3, 0, H / 2 * 1e-3],
                            outfile=f"nfc_{tag}_{ff/1e9:.3f}.h5",
                            read_cached=False, verbose=0)
        Et = np.array(r.E_theta[0])
        Ep = np.array(r.E_phi[0])
        ER_ = (Et + 1j * Ep) / np.sqrt(2.0)
        EL_ = (Et - 1j * Ep) / np.sqrt(2.0)
        i0 = int(np.argmin(np.abs(theta)))
        a, b = abs(ER_[i0, 0]), abs(EL_[i0, 0])
        ar = 20 * np.log10((a + b) / max(abs(a - b), 1e-15))
        k = int(np.argmin(np.abs(f - ff)))
        # Save the COMPLEX wanted-sense pattern, not just its magnitude: it
        # is the phase across the field of view that biases a direction
        # estimate, and a magnitude-only record cannot show it.
        out["ff"][f"{ff/1e9:.3f}"] = {
            "Dmax_dBi": float(10 * np.log10(r.Dmax[0])),
            "AR_dB": float(ar), "theta": theta.tolist(),
            "ER_E": np.abs(ER_[:, 0]).tolist(),
            "EL_E": np.abs(EL_[:, 0]).tolist(),
            "ER_H": np.abs(ER_[:, 1]).tolist(),
            "EL_H": np.abs(EL_[:, 1]).tolist(),
            "ER_E_ph": np.angle(ER_[:, 0]).tolist(),
            "ER_H_ph": np.angle(ER_[:, 1]).tolist()}
        print(f"{ff/1e9:8.3f}G {20*np.log10(abs(s11[k])):8.2f}dB "
              f"{20*np.log10(abs(s21[k])):13.2f}dB {ar:12.2f}dB "
              f"{10*np.log10(r.Dmax[0]):8.2f}dBi", flush=True)
    json.dump(out, open(os.path.join(HERE, f"c2_{tag}.json"), "w"))
