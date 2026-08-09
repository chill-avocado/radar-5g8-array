"""What stacking costs in coverage, and how to buy it back.

Stacking patches is what reaches a kilometre, and it does so by concentrating
the energy into a narrower cone.  That is the same sentence twice: the range
comes FROM the narrowing.  So the question is not whether coverage is lost but
how much, and what it takes to get it back.

Three things are checked here.  Whether the radar still measures in three
dimensions at all (it does, and nothing about it changes).  How wide a cone it
actually sees.  And what combination of stack size and transmit power reaches
a kilometre while keeping the cone as wide as possible.
"""

import math
import sys

import numpy as np

sys.path.insert(0, __file__.rsplit("/", 1)[0])
from system_budget import (K_BOLTZ, T0, LAM, SIGMA_M2, T_CPI_S, SNR_MIN_DB,
                           LOSS_DB, N_TX, N_RX, B210_P1DB_DBM, ISOLATION_DB)

ELEM_BW = 103.0          # MEASURED, one patch, at 5.800 GHz
TH = np.arange(-90, 90.01, 0.05)


def elem_pattern(th):
    """The measured single patch, as a cosine raised to fit 103 degrees."""
    n = math.log(0.5) / math.log(math.cos(math.radians(ELEM_BW / 2)))
    c = np.cos(np.radians(np.clip(th, -89.9, 89.9)))
    return np.clip(c, 0, None) ** n


def stack_pattern(th, n, d=0.5):
    psi = 2 * math.pi * d * np.sin(np.radians(th))
    s = np.sin(n * psi / 2)
    b = n * np.sin(psi / 2)
    af = np.where(np.abs(b) < 1e-9, 1.0, s / np.where(np.abs(b) < 1e-9, 1, b))
    return np.abs(af) * elem_pattern(th)


def bw(p):
    m = p.max()
    inb = TH[p >= m / math.sqrt(2)]
    return inb.max() - inb.min() if len(inb) else 0.0


def two_way(n_tx_along, n_rx_along):
    """Out and back: the transmit pattern times the receive pattern."""
    return bw(stack_pattern(TH, n_tx_along) * stack_pattern(TH, n_rx_along))


def rng(pt, g, nf):
    mimo = 10 * math.log10(N_TX * N_RX)
    num = (10 ** (pt / 10) / 1e3 * 10 ** ((2 * g + mimo) / 10)
           * LAM ** 2 * SIGMA_M2 * T_CPI_S)
    den = ((4 * math.pi) ** 3 * K_BOLTZ * T0
           * 10 ** ((nf + LOSS_DB + SNR_MIN_DB) / 10))
    return (num / den) ** 0.25


def gain(n):
    return 6.1 + 10 * math.log10(n) - 0.15 * (n - 1)


print("Does it still measure in three dimensions?\n")
print("  Range comes from sweeping the frequency: unchanged, about 1 m")
print("  Left-and-right comes from the transmit pair, half a wavelength apart")
print("    across: unchanged, they are still half a wavelength apart across")
print("  Up-and-down comes from the receive pair, half a wavelength apart")
print("    vertically: unchanged, likewise")
print("  The four virtual positions sit at exactly +/-0.25 wavelengths about")
print("    their own centre whatever the stacks do.  Three dimensions intact.\n")
sig = 10 ** (SNR_MIN_DB / 10)
dphi = 1.0 / math.sqrt(2 * sig)
print(f"  Angle accuracy at the detection threshold: "
      f"{math.degrees(dphi / math.pi):.1f} deg, better on stronger targets\n")

print("How wide a cone it sees\n")
print(f"  {'stack':>6} {'left-right':>12} {'up-down':>10} {'cone':>8} "
      f"{'coverage vs now':>17}")
base = None
for n in (1, 2, 3, 4, 6):
    # transmit grows tall, so it narrows up-and-down; receive grows wide, so
    # it narrows left-and-right
    az = two_way(1, n)
    el = two_way(n, 1)
    area = az * el
    if base is None:
        base = area
    print(f"  {n:3d} x   {az:9.1f} deg {el:7.1f} deg {'+/-%.0f' % (min(az,el)/2):>8} "
          f"{base/area:14.1f}x less")

print("\nReaching a kilometre while keeping the cone as wide as possible\n")
print(f"  {'stack':>6} {'transmit':>10} {'amp':>6} {'range':>8} {'cone':>9} "
      f"{'isolation needed':>18}")
best = []
for n in (1, 2, 3, 4):
    for pt, nf, lab in ((30, 3.54, "stock"), (30, 1.90, "better"),
                        (33, 1.90, "better"), (36, 1.90, "better")):
        r = rng(pt, gain(n), nf)
        if r < 1000:
            continue
        az, el = two_way(1, n), two_way(n, 1)
        need = B210_P1DB_DBM - 3.0 - 13.5 - pt
        best.append((min(az, el) / 2, n, pt, lab, r, need))
        print(f"  {n:3d} x   {pt:+7d} dBm {lab:>6} {r:7.0f} m "
              f"{'+/-%.0f' % (min(az,el)/2):>9} {need:14.1f} dB")

best.sort(reverse=True)
w, n, pt, lab, r, need = best[0]
print(f"\n  Widest cone that still reaches a kilometre: {n} patches per")
print(f"  element at {pt:+d} dBm -> {r:.0f} m over +/-{w:.0f} degrees,")
print(f"  needing {need:.1f} dB of isolation.")
