"""openEMS: two-port model of the inset-fed square patch (no hybrid).

Why this run exists
-------------------
The transmission-line model says the patch edge resistance is 587 ohm, which
is exactly the quantity that model is known to overestimate.  The inset depth
that lands the feed on 50 ohm depends on it, so it has to be measured rather
than assumed.  This run also gives the two-port S-matrix at the patch feeds,
which can be combined analytically with an ideal 90 degree hybrid to predict
the circularly-polarised port match before the hybrid is ever meshed.

Usage:  python3 patch2p.py <inset_mm> <patch_L_mm> [tag]
"""

import json
import os
import sys
import tempfile

import numpy as np
from CSXCAD import ContinuousStructure
from openEMS import openEMS
from openEMS.physical_constants import C0

HERE = os.path.dirname(os.path.abspath(__file__))
DESIGN = os.path.abspath(os.path.join(HERE, "..", "design"))
sys.path.insert(0, DESIGN)

F0 = 5.80e9
FC = 1.20e9                      # gaussian half-width -> 4.6 .. 7.0 GHz
UNIT = 1e-3

cfg = json.load(open(os.path.join(DESIGN, "synthesis.json")))["RO4350B"]
ER = cfg["substrate"]["er"]
TAND = cfg["substrate"]["tand"]
H = cfg["substrate"]["h_mm"]
WF = cfg["feed"]["w50_mm"]
GAP = 0.40

inset = float(sys.argv[1]) if len(sys.argv) > 1 else 4.00
L = float(sys.argv[2]) if len(sys.argv) > 2 else cfg["patch"]["L"]
tag = sys.argv[3] if len(sys.argv) > 3 else f"i{inset:.2f}_L{L:.3f}"

L2 = L / 2.0
# The feed run from the notch base to the port's measurement plane is set to
# exactly HALF a guided wavelength. A half-wave line is impedance-transparent,
# so whatever the port measures at 5.8 GHz IS what the patch presents -- no
# de-embedding, which near a quarter-wave point is hopelessly ill-conditioned.
DEEMB = 2.0                      # measurement plane this far short of the patch
PAD = 14.0                       # room for the PML and the excitation plane
FEED = DEEMB + PAD
AIR = 14.0                       # air above the board (~lambda0/4)
MARGIN = 14.0                    # air beyond the patch in +x / +y

xmin, xmax = (-L2 + inset) - FEED, L2 + MARGIN
ymin, ymax = (-L2 + inset) - FEED, L2 + MARGIN
zmax = H + AIR

sim_path = os.path.join(tempfile.gettempdir(), "oems_patch2p_" + tag)

FDTD = openEMS(NrTS=60000, EndCriteria=1e-4)
FDTD.SetGaussExcite(F0, FC)
# xmin / ymin absorb the two microstrip feeds, so they must be PML.
# zmin is PEC: the board's ground plane is far larger than the element, so
# treating it as infinite is the right approximation for match and boresight
# axial ratio, and it removes the whole lower half of the mesh.
FDTD.SetBoundaryCond(["PML_8", "MUR", "PML_8", "MUR", "PEC", "MUR"])

CSX = ContinuousStructure()
FDTD.SetCSX(CSX)
mesh = CSX.GetGrid()
mesh.SetDeltaUnit(UNIT)

# ---------------------------------------------------------------- materials
kappa = 2.0 * np.pi * F0 * 8.8541878128e-12 * ER * TAND
sub = CSX.AddMaterial("RO4350B", epsilon=ER, kappa=kappa)
sub.AddBox([xmin, ymin, 0], [xmax, ymax, H], priority=1)

# ------------------------------------------------------------------- copper
patch = CSX.AddMetal("patch")
nh = (WF + 2.0 * GAP) / 2.0
if inset > 1e-6:
    pts = [
        (-L2, -L2),
        (-nh, -L2), (-nh, -L2 + inset), (nh, -L2 + inset), (nh, -L2),
        (L2, -L2), (L2, L2), (-L2, L2),
        (-L2, nh), (-L2 + inset, nh), (-L2 + inset, -nh), (-L2, -nh),
    ]
else:
    pts = [(-L2, -L2), (L2, -L2), (L2, L2), (-L2, L2)]   # plain edge feed
patch.AddPolygon([[p[0] for p in pts], [p[1] for p in pts]],
                 norm_dir=2, elevation=H, priority=10)

# --------------------------------------------------------------------- mesh
res_fine = 0.22          # around the patch edges, notches and lines
res_air = 2.00
third = res_fine / 3.0
MIN_D = 0.075            # never let two grid lines sit closer than this:
                         # the FDTD timestep is set by the SMALLEST cell, so a
                         # stray micron-wide cell would cost a million steps.


def clean(lines, min_d=MIN_D):
    out = []
    for v in sorted(float(x) for x in lines):
        if not out or v - out[-1] >= min_d:
            out.append(v)
        else:
            out[-1] = 0.5 * (out[-1] + v)      # merge the pair
    return out


# Thirds rule: for a metal edge, put a line 1/3 of a cell outside and 2/3
# inside, which is the standard fix for the field singularity at the edge.
edges = [-L2, L2, -L2 + inset, -nh, nh, -WF / 2.0, WF / 2.0]
edges = sorted(set(edges))
# The feed region must be UNIFORM: the 8-cell PML that absorbs the microstrip
# sits at xmin/ymin, and if the cells there were graded out to res_air the PML
# would be 16 mm deep and would swallow the port's measurement plane.
res_feed = 0.30
xl = ([xmax] + list(np.arange(xmin, -L2 + 1e-9, res_feed))
      + list(np.arange(-L2, L2 + 1e-9, res_fine)))
yl = list(xl)
for e in edges:
    xl += [e - third, e + 2 * third]
    yl += [e - third, e + 2 * third]
mesh.AddLine("x", clean(xl))
mesh.AddLine("y", clean(yl))
mesh.SmoothMeshLines("x", res_air, 1.30)
mesh.SmoothMeshLines("y", res_air, 1.30)

zl = clean(list(np.linspace(0, H, 5)) + [zmax])
mesh.AddLine("z", zl)
mesh.SmoothMeshLines("z", res_air, 1.30)

FDTD.AddEdges2Grid(dirs="xy", properties=patch, metal_edge_res=res_fine / 2)

# ---------------------------------------------------------------- MSL ports
# Built AFTER the mesh, because a microstrip port needs existing grid lines
# along its propagation direction.  Port 1 drives the -x edge (the "x mode"),
# port 2 the -y edge (the "y mode").
feedline = CSX.AddMetal("feed")
p1 = FDTD.AddMSLPort(1, feedline,
                     [xmin, -WF / 2.0, H], [-L2 + inset, WF / 2.0, 0],
                     "x", "z", excite=-1, FeedShift=4.0,
                     MeasPlaneShift=PAD, priority=10)
p2 = FDTD.AddMSLPort(2, feedline,
                     [-WF / 2.0, ymin, H], [WF / 2.0, -L2 + inset, 0],
                     "y", "z", excite=0, FeedShift=4.0,
                     MeasPlaneShift=PAD, priority=10)
port = [p1, p2]

# ---------------------------------------------------------------------- run
if __name__ == "__main__":
    nc = [len(mesh.GetLines(d)) for d in range(3)]
    print(f"[patch2p:{tag}] mesh {nc[0]} x {nc[1]} x {nc[2]} = "
          f"{nc[0]*nc[1]*nc[2]/1e6:.2f} Mcells")
    os.makedirs(sim_path, exist_ok=True)
    CSX.Write2XML(os.path.join(sim_path, "model.xml"))
    FDTD.Run(sim_path, cleanup=False, verbose=1)

    f = np.linspace(5.0e9, 6.6e9, 401)
    for p in port:
        p.CalcPort(sim_path, f, ref_impedance=50)
    S11m = port[0].uf_ref / port[0].uf_inc
    S21m = port[1].uf_ref / port[0].uf_inc
    Zm = port[0].uf_tot / port[0].if_tot

    # De-embed the short run of 50 ohm line between the measurement plane and
    # the patch. Only 2 mm, so tan(beta*d) stays small and this is stable.
    from rfmath import ms_dispersive, RO4350B
    ee = np.array([ms_dispersive(WF * 1e-3, RO4350B, ff)[1] for ff in f])
    beta = 2.0 * np.pi * f * np.sqrt(ee) / 299792458.0        # rad/m
    t = np.tan(beta * DEEMB * 1e-3)
    Z0 = 50.0
    Zin = Z0 * (Zm - 1j * Z0 * t) / (Z0 - 1j * Zm * t)        # move to the load
    rot = np.exp(2j * beta * DEEMB * 1e-3)
    S11, S21 = S11m * rot, S21m * rot

    out = {"tag": tag, "inset_mm": inset, "L_mm": L,
           "f_hz": f.tolist(),
           "S11_re": S11.real.tolist(), "S11_im": S11.imag.tolist(),
           "S21_re": S21.real.tolist(), "S21_im": S21.imag.tolist(),
           "Zin_re": Zin.real.tolist(), "Zin_im": Zin.imag.tolist()}
    res = os.path.join(HERE, f"patch2p_{tag}.json")
    json.dump(out, open(res, "w"))

    # True resonance = where the patch's own reactance crosses zero.
    xi = Zin.imag
    zc = np.where(np.sign(xi[:-1]) != np.sign(xi[1:]))[0]
    zc = [i for i in zc if Zin.real[i] > 15.0]
    k = zc[int(np.argmin(np.abs(f[zc] - F0)))] if zc else int(np.argmin(np.abs(S11)))
    kf0 = int(np.argmin(np.abs(f - F0)))
    out["deembed_mm"] = DEEMB
    print(f"  resonance (X=0) : {f[k]/1e9:.4f} GHz   R = {Zin[k].real:.1f} ohm")
    print(f"  at 5.800 GHz    : S11 = {20*np.log10(abs(S11[kf0])):.2f} dB, "
          f"Zin = {Zin[kf0].real:.1f} {Zin[kf0].imag:+.1f}j, "
          f"S21 = {20*np.log10(abs(S21[kf0])):.2f} dB")
    print(f"  wrote {res}")
