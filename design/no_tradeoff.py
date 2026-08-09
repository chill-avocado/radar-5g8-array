"""A kilometre at full coverage: only the levers that cost no beam width.

Narrowing the beam is the one lever that buys range by spending sky.  Every
other lever in the radar equation is free of it.  So set the antenna aside
entirely -- single patches, the full measured 103-degree beam, the coverage the
board has today -- and see how far the rest of them reach on their own.
"""

import math
import sys

sys.path.insert(0, __file__.rsplit("/", 1)[0])
from system_budget import (K_BOLTZ, T0, LAM, SIGMA_M2, LOSS_DB, N_TX, N_RX,
                           B210_P1DB_DBM)

G = 6.1                    # single patch, MEASURED, full +/-27 deg coverage


def rng(pt, nf, tcpi, snr_min, n_tx=N_TX, n_rx=N_RX, loss=LOSS_DB):
    mimo = 10 * math.log10(n_tx * n_rx)
    num = (10 ** (pt / 10) / 1e3 * 10 ** ((2 * G + mimo) / 10)
           * LAM ** 2 * SIGMA_M2 * tcpi)
    den = ((4 * math.pi) ** 3 * K_BOLTZ * T0
           * 10 ** ((nf + loss + snr_min) / 10))
    return (num / den) ** 0.25


BASE = dict(pt=10.0, nf=8.0, tcpi=0.10, snr_min=13.0)
print(f"Starting point, everything as it is: "
      f"{rng(**BASE):.0f} m at +/-27 degrees\n")

print("Levers that cost no coverage at all:\n")
steps = [
    ("transmit at 2 W instead of 10 mW", dict(pt=33.0)),
    ("a dedicated 1 dB amplifier, not the switch-and-amp module",
     dict(nf=1.90)),
    ("look for 400 ms instead of 100 ms", dict(tcpi=0.40)),
    ("threshold 11 dB, with track-before-detect", dict(snr_min=11.0)),
    ("tighter cabling and processing, 2 dB of loss not 3", dict(loss=2.0)),
]
cur = dict(BASE)
loss = LOSS_DB
print(f"  {'':52} {'range':>8} {'gain':>8}")
prev = rng(**cur, loss=loss)
print(f"  {'as built, with the radio driving it directly':52} "
      f"{prev:7.0f} m")
for lab, upd in steps:
    if "loss" in upd:
        loss = upd["loss"]
    else:
        cur.update(upd)
    r = rng(**cur, loss=loss)
    print(f"  + {lab:50} {r:7.0f} m {r/prev:7.2f}x")
    prev = r

print(f"\n  All of them together: {prev:.0f} m, "
      f"at the SAME +/-27 degrees the board covers today.\n")

print("And the same again with four channels instead of two, which is what a")
print("second synchronised radio would give.  More channels raise both the")
print("total power radiated and the number of looks combined, and because")
print("each element stays a single patch the coverage does not move:\n")
for n in (2, 3, 4):
    r = rng(pt=33.0, nf=1.90, tcpi=0.40, snr_min=11.0, n_tx=n, n_rx=n,
            loss=2.0)
    print(f"  {n} transmit x {n} receive: {r:6.0f} m   "
          f"(total radiated {10*math.log10(n)+33:.0f} dBm)")

print("\nWhat the transmitter has to be, for a kilometre at full coverage:\n")
for pt in (30, 33, 36):
    r = rng(pt=pt, nf=1.90, tcpi=0.40, snr_min=11.0, loss=2.0)
    need = B210_P1DB_DBM - 3.0 - 13.5 - pt
    w = 10 ** (pt / 10) / 1000
    mark = "  <- clears a kilometre" if r >= 1000 else ""
    print(f"  {pt:+3d} dBm ({w:.1f} W): {r:6.0f} m, "
          f"needs {need:.1f} dB isolation{mark}")

print("\nWhere that isolation comes from, none of it costing coverage:\n")
budget = [("measured today, one board, 92 mm apart", -41.1),
          ("two boards instead of one, 500 mm apart", -14.7),
          ("absorber between them, which is a foam pad", -20.0),
          ("shaped ground edge on each board", -4.0)]
tot = 0.0
for lab, d in budget:
    tot = d if tot == 0.0 else tot + d
    print(f"  {lab:44} {d:+7.1f} dB  -> {tot:7.1f} dB")
print(f"\n  {tot:.1f} dB against the -64.5 dB a 2 W transmitter needs: "
      f"{-64.5 - tot:+.1f} dB to spare.")
