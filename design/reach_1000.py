"""What it actually takes to see a small drone at a kilometre.

Range in a radar goes as the fourth root of everything, which is brutal: to
double the distance you need sixteen times the signal.  So the question is not
"which knob" but "which knobs, and in what combination", and the answer is
decided by one fact -- antenna gain is counted twice, once going out and once
coming back, while transmit power is counted once.
"""

import math
import sys

sys.path.insert(0, __file__.rsplit("/", 1)[0])
from system_budget import (K_BOLTZ, T0, LAM, SIGMA_M2, T_CPI_S, SNR_MIN_DB,
                           LOSS_DB, N_TX, N_RX, ISOLATION_DB, B210_P1DB_DBM)

TARGET_M = 1000.0
NOW = dict(pt=26.0, g=6.1, nf=3.54, tcpi=T_CPI_S)


def rng(pt, g_tx, g_rx, nf, tcpi=T_CPI_S, sigma=SIGMA_M2):
    mimo = 10.0 * math.log10(N_TX * N_RX)
    num = (10 ** (pt / 10.0) / 1e3 * 10 ** ((g_tx + g_rx + mimo) / 10.0)
           * LAM ** 2 * sigma * tcpi)
    den = ((4 * math.pi) ** 3 * K_BOLTZ * T0
           * 10 ** ((nf + LOSS_DB + SNR_MIN_DB) / 10.0))
    return (num / den) ** 0.25


base = rng(NOW["pt"], NOW["g"], NOW["g"], NOW["nf"])
short = 40 * math.log10(TARGET_M / base)
print(f"Where a quarter-watt version lands today:  {base:.0f} m")
print(f"A kilometre needs {short:.1f} dB more signal "
      f"({TARGET_M/base:.2f} times the range, and range goes as the "
      f"fourth root)\n")

print("What each knob is worth on its own:\n")
rows = [
    ("transmit power, +26 -> +36 dBm (4 W)", rng(36, 6.1, 6.1, 3.54), "counted once"),
    ("noise figure, 3.5 -> 1.5 dB", rng(26, 6.1, 6.1, 1.54), "counted once"),
    ("integration, 100 -> 400 ms", rng(26, 6.1, 6.1, 3.54, 0.4), "counted once"),
    ("antenna gain, +6 dB each end", rng(26, 12.1, 12.1, 3.54), "COUNTED TWICE"),
    ("antenna gain, +12 dB each end", rng(26, 18.1, 18.1, 3.54), "COUNTED TWICE"),
]
for lab, r, note in rows:
    print(f"  {lab:38} {r:6.0f} m  ({r/base:4.2f}x)   {note}")

print(f"\nGain is the efficient lever, and it is nearly free: a more directive\n"
      f"antenna also throws less energy sideways, so the transmit-to-receive\n"
      f"leak gets smaller at the same time as the range gets longer.\n")

# --- how to get gain without spoiling the angle measurement
print("Getting that gain without breaking the angle measurement\n")
print("  The transmit pair measures left-and-right, so it must stay half a")
print("  wavelength apart ACROSS.  Nothing stops it growing TALL.")
print("  The receive pair measures up-and-down, so it must stay half a")
print("  wavelength apart VERTICALLY.  Nothing stops it growing WIDE.\n")
print(f"  half a wavelength is {LAM/2*1e3:.2f} mm; one patch is 12.83 mm across,")
print(f"  so a stack fits inside the spacing with "
      f"{LAM/2*1e3 - 12.83:.2f} mm to spare\n")

print(f"  {'patches per element':>20} {'element gain':>13} {'range':>8} "
      f"{'beam across the stack':>22}")
for n in (1, 2, 3, 4, 6, 8):
    # a uniform line of n patches at 0.8 wavelength spacing
    g = 6.1 + 10 * math.log10(n) - (0.15 * (n - 1))     # feed loss as it grows
    r = rng(26, g, g, 3.54)
    bw = math.degrees(0.886 * 1.0 / (n * 0.8)) if n > 1 else 78.0
    mark = "  <- reaches a kilometre" if r >= TARGET_M else ""
    print(f"  {n:12d} patches {g:10.1f} dBi {r:7.0f} m {bw:19.1f} deg{mark}")

print(f"\n  and with the transmit amplifier at +30 dBm (1 W) instead:")
for n in (2, 3, 4):
    g = 6.1 + 10 * math.log10(n) - (0.15 * (n - 1))
    r = rng(30, g, g, 3.54)
    mark = "  <- reaches a kilometre" if r >= TARGET_M else ""
    print(f"  {n:12d} patches {g:10.1f} dBi {r:7.0f} m{mark}")

print(f"\n  and at +30 dBm with a dedicated 1 dB amplifier (system 1.9 dB):")
for n in (2, 3, 4):
    g = 6.1 + 10 * math.log10(n) - (0.15 * (n - 1))
    r = rng(30, g, g, 1.90)
    mark = "  <- reaches a kilometre" if r >= TARGET_M else ""
    print(f"  {n:12d} patches {g:10.1f} dBi {r:7.0f} m{mark}")

print(f"\nIsolation the leak then demands, to keep the radio safe:")
for pt in (26, 30, 33):
    need = B210_P1DB_DBM - 3.0 - 13.5 - pt
    print(f"  {pt:+3d} dBm transmit -> {need:6.1f} dB "
          f"({need - ISOLATION_DB:+.0f} dB beyond the board today)")
