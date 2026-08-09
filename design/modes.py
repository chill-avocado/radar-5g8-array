"""Measure the patch's two resonances directly, instead of hunting for them.

A square patch rings at two frequencies at once, one for each direction across
it.  Circular polarisation needs both directions to answer equally, and they
only do that if both ring at the same frequency.  If they do not, the balance
between them slides as you sweep across the band -- and that slide IS the
slope of axial ratio against frequency.

Every tuning run so far has been a search: change a dimension, simulate the
whole element, look at the axial ratio, guess again.  Half an hour a go, and
three unknowns tangled together.

This measures the two resonances instead.  Take the patches and their two
matching transformers, on the real ground, and leave the coupler and the two
feed routes out entirely.  Put a port on each transformer.  Drive one, then
the other.  Where each one dips is where that direction rings, and the answer
is then arithmetic:

    lengthen a side by 2.43 um  ->  that direction rings 1 MHz lower

so both dimensions follow from two runs.  Whatever phase error is left is the
coupler's own, which is measured separately and is a fixed number.

  python3 modes.py x      drive the x-edge feed
  python3 modes.py y      drive the y-edge feed
  python3 modes.py fit    read both results and print the trim to apply
"""

import json
import os
import sys
import tempfile

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))

F0, FC = 5.80e9, 0.9e9
UM_PER_MHZ = 2.43e-3        # mm of patch side per MHz of resonance shift
TARGET = 5.800e9

VARIANT = os.environ.get("VARIANT", "ZYF300CA")
STUB = 2.0                  # 50 ohm lead-in, so both ports refer to the same
                            # kind of plane as the coupler arm would


def _cfg():
    return json.load(open(os.path.join(HERE, "synthesis.json")))[VARIANT]


def _resonance(f, s11):
    """Frequency of the dip, refined by a parabola through the lowest three."""
    m = np.abs(s11)
    i = int(np.argmin(m))
    if 0 < i < len(m) - 1:
        y0, y1, y2 = m[i - 1], m[i], m[i + 1]
        d = y0 - 2 * y1 + y2
        off = 0.5 * (y0 - y2) / d if abs(d) > 1e-18 else 0.0
        return float(f[i] + off * (f[1] - f[0])), float(20 * np.log10(y1))
    return float(f[i]), float(20 * np.log10(m[i]))


# --------------------------------------------------------------------- model
def build(drive):
    from CSXCAD import ContinuousStructure
    from openEMS import openEMS
    sys.path.insert(0, HERE)
    from element2 import Element
    from geom import rect, hseg, vseg

    cfg = _cfg()
    ER, TAND = cfg["substrate"]["er"], cfg["substrate"]["tand"]
    H, WF = cfg["substrate"]["h_mm"], cfg["feed"]["w50_mm"]
    PITCH = 299792458.0 / F0 / 2.0 * 1e3

    dLx = float(os.environ.get("DLX", cfg["tuned"].get("dLx", 0.0)))
    dLy = float(os.environ.get("DLY", cfg["tuned"].get("dLy", 0.0)))
    el = Element(cfg, dLx=dLx, dLy=dLy)
    Lx2, Ly2, wq, lq = el.Lx2, el.Ly2, el.wq, el.lq

    # one element: patch, two transformers, a 50 ohm lead on each
    xA = -Lx2 - lq                      # transformer A meets the coupler here
    yB = -Ly2 - lq
    def one(sx):
        """sx = +1 as drawn, -1 mirrored, exactly as the board has them."""
        P = [rect(-Lx2, -Ly2, Lx2, Ly2),
             hseg(xA, -Lx2, 0.0, wq),
             vseg(yB, -Ly2, 0.0, wq),
             hseg(xA - STUB, xA + 0.05, 0.0, WF),
             vseg(yB - STUB, yB + 0.05, 0.0, WF)]
        if sx < 0:
            P = [[(-x, y) for (x, y) in p][::-1] for p in P]
        return P

    polys = [list(p) for p in one(+1)] + \
            [[(x + PITCH, y) for (x, y) in p] for p in one(-1)]
    # port feet: (x, y, axis) -- axis is the direction the line runs
    feet = [(xA - STUB, 0.0, "x"), (0.0, yB - STUB, "y"),
            (PITCH - (xA - STUB), 0.0, "x"), (PITCH, yB - STUB, "y")]

    xs = [q[0] for p in polys for q in p]
    ys = [q[1] for p in polys for q in p]
    GM = float(os.environ.get("GNDM", 25.0))
    AIR = 13.0
    # 25 mm is the real ground now: the boards sit on an aluminium plate cut
    # to give at least that much past every patch edge.  It used to be an
    # optimistic modelling assumption; the plate made it true.
    gx0, gx1 = min(xs) - GM, max(xs) + GM
    gy0, gy1 = min(ys) - GM, max(ys) + GM

    FDTD = openEMS(NrTS=int(os.environ.get("NRTS", 120000)), EndCriteria=1e-4)
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

    res_c = float(os.environ.get("RES", 0.32))
    res_air = float(os.environ.get("RESAIR", 2.2))

    def clean(v, m=0.075):
        out = []
        for x in sorted(float(t) for t in v):
            if not out or x - out[-1] >= m:
                out.append(x)
            else:
                out[-1] = 0.5 * (out[-1] + x)
        return out

    xl = list(np.arange(min(xs) - 1, max(xs) + 1, res_c)) + \
        [gx0 - AIR, gx0, gx1, gx1 + AIR]
    yl = list(np.arange(min(ys) - 1, max(ys) + 1, res_c)) + \
        [gy0 - AIR, gy0, gy1, gy1 + AIR]
    for p in polys:
        for q in p:
            xl += [q[0] - res_c / 3, q[0] + 2 * res_c / 3]
            yl += [q[1] - res_c / 3, q[1] + 2 * res_c / 3]
    for off in (0.0, PITCH):
        xl += list(np.linspace(off - wq / 2, off + wq / 2, 4))
    yl += list(np.linspace(-wq / 2, wq / 2, 4))
    mesh.AddLine("x", clean(xl)); mesh.SmoothMeshLines("x", res_air, 1.35)
    mesh.AddLine("y", clean(yl)); mesh.SmoothMeshLines("y", res_air, 1.35)
    mesh.AddLine("z", clean([-AIR] + list(np.linspace(0, H, 4)) + [H + AIR]))
    mesh.SmoothMeshLines("z", res_air, 1.35)

    ports = []
    for n, (px, py, ax) in enumerate(feet):
        if ax == "x":
            sgn = -1.0 if px < PITCH / 2 else 1.0
            box0 = [px + sgn * 1.9, py - WF / 2, 0]
            box1 = [px + sgn * 1.2, py + WF / 2, H]
            CSX.AddMetal(f"f{n}").AddBox(
                [px + sgn * 1.2, py - WF / 2, H], [px, py + WF / 2, H],
                priority=10)
        else:
            box0 = [px - WF / 2, py - 1.9, 0]
            box1 = [px + WF / 2, py - 1.2, H]
            CSX.AddMetal(f"f{n}").AddBox(
                [px - WF / 2, py - 1.2, H], [px + WF / 2, py, H], priority=10)
        lo = [min(a, b) for a, b in zip(box0, box1)]
        hi = [max(a, b) for a, b in zip(box0, box1)]
        ports.append(FDTD.AddLumpedPort(n + 1, 50.0, lo, hi, "z",
                                        excite=(1 if n == drive else 0),
                                        priority=20))
    return FDTD, mesh, ports, (gx1 - gx0, gy1 - gy0)


def run(which):
    drive = {"x": 0, "y": 1}[which]
    FDTD, mesh, ports, gnd = build(drive)
    nc = [len(mesh.GetLines(d)) for d in range(3)]
    print(f"[modes {which}] {nc[0]}x{nc[1]}x{nc[2]} = "
          f"{nc[0]*nc[1]*nc[2]/1e6:.2f} Mcells   ground "
          f"{gnd[0]:.0f} x {gnd[1]:.0f} mm", flush=True)
    sp = os.path.join(tempfile.gettempdir(), "oems_modes_" + which)
    os.makedirs(sp, exist_ok=True)
    FDTD.Run(sp, cleanup=False, verbose=0,
             numThreads=int(os.environ.get("THREADS", 2)))
    f = np.linspace(5.2e9, 6.4e9, 241)
    for p in ports:
        p.CalcPort(sp, f)
    s = [p.uf_ref / ports[drive].uf_inc for p in ports]
    fr, dip = _resonance(f, s[drive])
    out = dict(which=which, f=f.tolist(),
               s=[[np.real(v).tolist(), np.imag(v).tolist()] for v in s],
               f_res=fr, dip_db=dip)
    TUNING = os.path.join(HERE, "tuning")
    os.makedirs(TUNING, exist_ok=True)
    json.dump(out, open(os.path.join(TUNING, f"modes_{which}.json"), "w"))
    print(f"[modes {which}] rings at {fr/1e9:.4f} GHz, dip {dip:.1f} dB")


def fit():
    got = {}
    TUNING = os.path.join(HERE, "tuning")
    for w in ("x", "y"):
        p = os.path.join(TUNING, f"modes_{w}.json")
        if not os.path.exists(p):
            print(f"  modes_{w}.json missing -- run: python3 modes.py {w}")
            return
        got[w] = json.load(open(p))
    cfg = _cfg()
    fx, fy = got["x"]["f_res"], got["y"]["f_res"]
    dLx = (fx - TARGET) / 1e6 * UM_PER_MHZ
    dLy = (fy - TARGET) / 1e6 * UM_PER_MHZ
    print(f"\n  x direction rings at {fx/1e9:.4f} GHz  "
          f"(dip {got['x']['dip_db']:.1f} dB)")
    print(f"  y direction rings at {fy/1e9:.4f} GHz  "
          f"(dip {got['y']['dip_db']:.1f} dB)")
    print(f"  they are {abs(fx-fy)/1e6:.1f} MHz apart, which is what tilts "
          f"the axial ratio across the band\n")
    print(f"  to put both on {TARGET/1e9:.3f} GHz:")
    print(f"     dLx = {dLx:+.4f} mm     dLy = {dLy:+.4f} mm")
    print(f"     patch becomes {cfg['tuned']['L']+dLx:.4f} x "
          f"{cfg['tuned']['L']+dLy:.4f} mm")
    if os.environ.get("APPLY") == "1":
        c = json.load(open(os.path.join(HERE, "synthesis.json")))
        c[VARIANT]["tuned"]["dLx"] = round(dLx, 5)
        c[VARIANT]["tuned"]["dLy"] = round(dLy, 5)
        json.dump(c, open(os.path.join(HERE, "synthesis.json"), "w"), indent=1)
        print("\n  written into synthesis.json")


if __name__ == "__main__":
    a = sys.argv[1] if len(sys.argv) > 1 else "fit"
    if a in ("x", "y"):
        run(a)
    else:
        fit()
