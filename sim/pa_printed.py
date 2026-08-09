"""openEMS: measure the three things this amplifier board prints in copper.

The filter, the sampling coupler and the supply feed were all synthesised from
closed-form theory, and on this project closed-form theory has been wrong
often enough to be worth checking every time -- the patch edge resistance was
out by a factor of two, the coupler ring behaved ten per cent short, and the
connector launch model returned 61 ohm where the truth was 50.

So each printed part is driven on the real copper, taken from the same
pa_board.json the Gerbers come from.

  lpf    the five-section low-pass.  Does it pass 5.8 GHz without loss and
         stop the harmonics the amplifier makes at 11.6 and 17.4 GHz?
  tap    the coupled pair.  How much does it lift, how much does it cost the
         transmission, and can it tell a forward wave from a reflected one?
  bias   the quarter-wave feed and its stub.  Is the amplifier's output pin
         really looking at an open circuit, and what does the feed cost?

Usage:  python3 pa_printed.py <lpf|tap|bias> [tag]
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
PR = D["printed"]
S = D["stack"]
ER, TAND, H = S["er"], S["tand"], S["h_mm"]
WF = D["w50"]

MODE = sys.argv[1] if len(sys.argv) > 1 else "lpf"
TAG = sys.argv[2] if len(sys.argv) > 2 else MODE

F0 = 5.80e9
FMAX = {"lpf": 20.0e9, "tap": 9.0e9, "bias": 9.0e9}[MODE]
FC = FMAX / 2.0
LEAD = 8.0                     # fifty-ohm line either side of the structure
MARGIN = 6.0                   # ground and substrate past the widest copper

kappa = 2 * np.pi * F0 * 8.8541878128e-12 * ER * TAND


def clean(v, m=0.05):
    """One mesh line where two nearly coincide: a stray micron-wide cell sets
    the timestep for the whole run."""
    out = []
    for x in sorted(float(t) for t in v):
        if not out or x - out[-1] >= m:
            out.append(x)
        else:
            out[-1] = 0.5 * (out[-1] + x)
    return out


# --------------------------------------------------------------- geometry
# Each structure is laid out here in its own local frame, as a list of
# (x0, y0, x1, y1) rectangles plus where the ports go.  The numbers come
# straight out of the design, never re-derived.
def geom_lpf():
    x, secs = LEAD, PR["lpf"]["sections"]
    boxes = [(0.0, -WF / 2, LEAD, WF / 2)]
    for s in secs:
        boxes.append((x, -s["w_mm"] / 2, x + s["len_mm"], s["w_mm"] / 2))
        x += s["len_mm"]
    boxes.append((x, -WF / 2, x + LEAD, WF / 2))
    return boxes, x + LEAD, max(s["w_mm"] for s in secs), []


def geom_tap():
    """The coupled pair, with its two sampled ports stepped well clear.

    On the board those ports turn away from the transmission line within a
    millimetre or two.  Left running alongside it they would keep coupling,
    and the measurement would report more coupling than the structure has.
    """
    t = PR["tap"]
    w, g, L = t["w_mm"], t["s_mm"], t["len_mm"]
    total = LEAD + L + LEAD
    yc = w + g
    ylead = yc + 6.0
    return ([(0.0, -w / 2, total, w / 2),
             (LEAD, yc - w / 2, LEAD + L, yc + w / 2),
             (LEAD - w / 2, yc, LEAD + w / 2, ylead + w / 2),
             (LEAD + L - w / 2, yc, LEAD + L + w / 2, ylead + w / 2),
             (0.0, ylead - w / 2, LEAD + w / 2, ylead + w / 2),
             (LEAD + L - w / 2, ylead - w / 2, total, ylead + w / 2)],
            total, ylead + w, [("c1", 0.0, ylead), ("c2", total, ylead)])


def geom_bias():
    """The feed exactly as the board draws it, ending on its landing pad.

    The pad is grounded through a via here, which is the limit the decoupling
    capacitors approach: it says what the quarter-wave line itself can do,
    with the capacitors' own few ohms added on top afterwards.  Drawing the
    feed as a plain quarter wave, without the half-width at each junction,
    put the whole thing at 7.55 GHz -- which is what the first run measured.
    """
    b = PR["bias"]
    wl, ll = b["w_mm"], b["len_mm"]
    total = LEAD + LEAD
    y_end = -ll
    boxes = [(0.0, -WF / 2, total, WF / 2),
             (LEAD - wl / 2, y_end, LEAD + wl / 2, 0.0),
             (LEAD - 0.31, y_end - 0.62, LEAD + 0.31, y_end)]
    return boxes, total, ll + 1.0, []


BOXES, LX, YSPAN, EXTRA = {"lpf": geom_lpf, "tap": geom_tap,
                           "bias": geom_bias}[MODE]()

ymin = min(b[1] for b in BOXES) - MARGIN
ymax = max(b[3] for b in BOXES) + MARGIN
zmax = H + 12.0

FDTD = openEMS(NrTS=int(os.environ.get("NRTS", 160000)), EndCriteria=1e-4)
FDTD.SetGaussExcite((F0 + FMAX / 2) / 2.0, (FMAX - 0.5e9) / 2.0)
# The line leaves through the x faces, so those absorb; everywhere else is a
# simple absorbing wall a quarter wave back, which is far cheaper than a
# matched layer and enough where nothing is meant to reach it.
FDTD.SetBoundaryCond(["PML_8", "PML_8", "MUR", "MUR", "PEC", "MUR"])
CSX = ContinuousStructure()
FDTD.SetCSX(CSX)
mesh = CSX.GetGrid()
mesh.SetDeltaUnit(1e-3)

CSX.AddMaterial("sub", epsilon=ER, kappa=kappa).AddBox(
    [0, ymin, 0], [LX, ymax, H], priority=1)

metal = CSX.AddMetal("cu")
for (x0, y0, x1, y1) in BOXES:
    metal.AddBox([x0, y0, H], [x1, y1, H], priority=10)
if MODE == "bias":
    # the landing pad's ground via, which is what the decoupling gives it
    y_end = -PR["bias"]["len_mm"]
    metal.AddBox([LEAD - 0.15, y_end - 0.46, 0],
                 [LEAD + 0.15, y_end - 0.16, H], priority=12)

# ------------------------------------------------------------------ mesh
RES = float(os.environ.get("RES", 0.30))
FINE = float(os.environ.get("FINE", 0.10))
xl = [0.0, LX] + list(np.arange(0.0, LX + 1e-9, RES))
yl = [ymin, ymax] + list(np.arange(-max(abs(ymin), 2.0), ymax + 1e-9, RES))
for (x0, y0, x1, y1) in BOXES:
    for e in (x0, x1):
        xl += [e - FINE / 3, e + 2 * FINE / 3]
    for e in (y0, y1):
        yl += [e - FINE / 3, e + 2 * FINE / 3]
        yl += list(np.arange(y0, y1 + 1e-9, max((y1 - y0) / 6.0, FINE)))
mesh.AddLine("x", clean(xl, FINE * 0.6))
mesh.AddLine("y", clean(yl, FINE * 0.6))
mesh.SmoothMeshLines("x", RES, 1.35)
mesh.SmoothMeshLines("y", RES, 1.35)
mesh.AddLine("z", clean(list(np.linspace(0, H, 5)) + [zmax], 0.05))
mesh.SmoothMeshLines("z", 1.2, 1.35)

PORTS = []
PORTS.append(FDTD.AddMSLPort(1, metal, [0, -WF / 2, H], [LEAD * 0.8, WF / 2, 0],
                             "x", "z", excite=-1, FeedShift=2.0,
                             MeasPlaneShift=LEAD * 0.55, priority=15))
PORTS.append(FDTD.AddMSLPort(2, metal, [LX, -WF / 2, H],
                             [LX - LEAD * 0.8, WF / 2, 0], "x", "z",
                             MeasPlaneShift=LEAD * 0.55, priority=15))
# The two sampled ports leave straight out of the side, into an absorbing
# boundary.  Turned along the board instead they would keep coupling to the
# transmission line all the way to the measurement plane, and the answer would
# be about the leads rather than about the coupler.
for i, (nm, px, ylead) in enumerate(EXTRA):
    w = PR["tap"]["w_mm"]
    sgn = 1.0 if px < LX / 2 else -1.0
    PORTS.append(FDTD.AddMSLPort(
        3 + i, metal, [px, ylead - w / 2, H],
        [px + sgn * LEAD * 0.8, ylead + w / 2, 0], "x", "z",
        MeasPlaneShift=LEAD * 0.55, priority=15))


if __name__ == "__main__":
    nc = [len(mesh.GetLines(d)) for d in range(3)]
    print(f"[{TAG}] {MODE}  mesh {nc[0]}x{nc[1]}x{nc[2]} = "
          f"{nc[0]*nc[1]*nc[2]/1e6:.2f} Mcells, {len(PORTS)} ports", flush=True)
    sp = os.path.join(tempfile.gettempdir(), "oems_pa_" + TAG)
    os.makedirs(sp, exist_ok=True)
    FDTD.Run(sp, cleanup=True, verbose=0,
             numThreads=int(os.environ.get("THREADS", 6)))

    f = np.linspace(1.0e9, FMAX, 601)
    for p in PORTS:
        p.CalcPort(sp, f, ref_impedance=50)
    a1 = PORTS[0].uf_inc
    s = {}
    for k, p in enumerate(PORTS):
        s[k + 1] = p.uf_ref / a1

    def db(v):
        return 20.0 * np.log10(np.maximum(np.abs(v), 1e-12))

    def at(v, fx):
        return float(np.interp(fx, f, db(v)))

    out = {"mode": MODE, "f": f.tolist(),
           "s": {str(k): [np.real(v).tolist(), np.imag(v).tolist()]
                 for k, v in s.items()}}

    if MODE == "lpf":
        print(f"   through at 5.725 GHz  {at(s[2], 5.725e9):6.2f} dB")
        print(f"   through at 5.800 GHz  {at(s[2], 5.800e9):6.2f} dB")
        print(f"   through at 5.875 GHz  {at(s[2], 5.875e9):6.2f} dB")
        print(f"   match   at 5.800 GHz  {at(s[1], 5.800e9):6.2f} dB")
        for h in (2, 3):
            print(f"   rejection at {h*5.8:.1f} GHz  "
                  f"{at(s[2], h*5.8e9):6.2f} dB")
        worst = max(at(s[2], ff) for ff in (5.725e9, 5.8e9, 5.875e9))
        print(f"   worst in band {worst:.2f} dB, "
              f"design said {-PR['lpf']['ripple_db']:.2f} plus copper loss")
    elif MODE == "tap":
        print(f"   coupling  at 5.800 GHz  {at(s[3], 5.8e9):6.2f} dB "
              f"(design {PR['tap']['coupling_db']:.2f})")
        print(f"   isolated  at 5.800 GHz  {at(s[4], 5.8e9):6.2f} dB")
        print(f"   directivity             "
              f"{at(s[3], 5.8e9) - at(s[4], 5.8e9):6.2f} dB")
        print(f"   through   at 5.800 GHz  {at(s[2], 5.8e9):6.2f} dB")
        print(f"   match     at 5.800 GHz  {at(s[1], 5.8e9):6.2f} dB")
    else:
        print(f"   through at 5.725 GHz  {at(s[2], 5.725e9):6.2f} dB")
        print(f"   through at 5.800 GHz  {at(s[2], 5.800e9):6.2f} dB")
        print(f"   through at 5.875 GHz  {at(s[2], 5.875e9):6.2f} dB")
        print(f"   match   at 5.800 GHz  {at(s[1], 5.800e9):6.2f} dB")
        print(f"   design said the feed costs "
              f"{PR['bias']['insertion_db']:.3f} dB")

    RESULTS = os.path.join(HERE, "results")
    json.dump(out, open(os.path.join(RESULTS, f"pa_{TAG}.json"), "w"))
    print(f"   wrote pa_{TAG}.json")
