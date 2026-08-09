"""Techniques not yet used, and what each is actually worth.

The one that matters most is a change of where the leak is dealt with.  Every
decision so far has treated the transmit leak as a level the radio must
survive.  It does not have to reach the radio at all.

A frequency-swept radar works by comparing what comes back with what is being
sent.  Do that comparison in hardware, before the radio, and range turns into
a simple tone: something 1000 m away produces a tone of about a megahertz,
something at zero range produces no tone at all.  The leak IS the zero-range
return.  A filter that blocks everything below a few kilohertz removes it
completely while passing every real target beyond about ten metres.

That lifts the ceiling that has decided the transmit power all along.
"""

import math
import sys

sys.path.insert(0, __file__.rsplit("/", 1)[0])
from system_budget import (K_BOLTZ, T0, LAM, SIGMA_M2, N_TX, N_RX,
                           B210_P1DB_DBM, ISOLATION_DB)

C0 = 299792458.0
G = 6.1                     # single patch, measured, full coverage
LNA_GAIN, LNA_NF = 15.0, 1.30
LNA_IN_P1DB = -8.0
MIXER_IN_P1DB = 5.0         # a passive mixer swallows far more than an ADC


def rng(pt, nf=2.31, t=0.40, snr=11.0, loss=2.0, n_tx=N_TX, n_rx=N_RX):
    mimo = 10 * math.log10(n_tx * n_rx)
    num = (10 ** (pt / 10) / 1e3 * 10 ** ((2 * G + mimo) / 10)
           * LAM ** 2 * SIGMA_M2 * t)
    den = ((4 * math.pi) ** 3 * K_BOLTZ * T0
           * 10 ** ((nf + loss + snr) / 10))
    return (num / den) ** 0.25


BASE = rng(30.0)
print(f"Where the design stands: {BASE:.0f} m at full coverage\n")
print("=" * 68)
print("1.  Compare in hardware, so the leak never reaches the radio")
print("=" * 68)
B, T = 150e6, 1.0e-3
print(f"\n  A {B/1e6:.0f} MHz sweep in {T*1e3:.0f} ms turns range into a tone:\n")
for r in (0, 10, 100, 1000, 2000):
    print(f"    {r:5.0f} m -> {2*r*B/(C0*T)/1e3:8.1f} kHz")
print(f"\n  Block everything under 20 kHz and the leak is gone, along with")
print(f"  every echo closer than {20e3*C0*T/(2*B):.0f} m, which is not a target.\n")
print(f"  What then sets the transmit power is the amplifier at the front,")
print(f"  not the radio behind it:\n")
print(f"  {'transmit':>10} {'leak at the amp':>17} {'after it':>11} "
      f"{'range':>8}   verdict")
for pt in (30, 33, 36, 40, 43):
    at_lna = pt - 79.8
    at_mix = at_lna + LNA_GAIN
    ok = at_lna < LNA_IN_P1DB and at_mix < MIXER_IN_P1DB
    print(f"  {pt:+7d} dBm {at_lna:14.1f} dBm {at_mix:8.1f} dBm "
          f"{rng(pt):7.0f} m   {'fine' if ok else 'too much'}")
print(f"\n  The couplers already drawn on the front-end board are exactly what")
print(f"  feeds those comparisons -- they were drawn for a canceller that is")
print(f"  no longer needed, and they do this job instead.")

print("\n" + "=" * 68)
print("2.  Look for longer, with the target's motion taken out")
print("=" * 68)
print(f"\n  A drone at 20 m/s crosses {20*0.4:.0f} m in 400 ms, which is "
      f"{20*0.4/1.0:.0f} range cells,")
print(f"  so simply adding up the returns smears them.  Undo the motion first")
print(f"  and the look can be far longer.\n")
print(f"  {'look time':>11} {'cells crossed':>15} {'range':>8}")
for t in (0.4, 0.8, 1.6, 3.2):
    print(f"  {t*1000:8.0f} ms {20*t/1.0:12.0f} {rng(40.0, t=t):7.0f} m")

print("\n" + "=" * 68)
print("3.  Change frequency between looks, so a bad angle does not persist")
print("=" * 68)
print(f"\n  A drone is a jumble of scatterers, so its echo rises and falls by")
print(f"  many decibels as the aspect changes.  Waiting for a good moment is")
print(f"  what forces a high threshold.  Sweeping a different part of the band")
print(f"  each look decorrelates that, and the threshold can come down.\n")
print(f"  {'threshold':>11} {'range':>8}")
for s in (13.0, 11.0, 9.0, 8.0):
    print(f"  {s:8.1f} dB {rng(40.0, t=1.6, snr=s):7.0f} m")

print("\n" + "=" * 68)
print("4.  All of it together")
print("=" * 68)
r = rng(40.0, t=1.6, snr=8.0)
print(f"\n  10 W, 1.6 second look, frequency-diverse threshold:")
print(f"    {r:.0f} m at the full +/-27 degrees, "
      f"{r/BASE:.1f} times the current design")
print(f"    and {r/1000:.1f} times your floor.\n")
print(f"  With a second synchronised radio on top: "
      f"{rng(40.0, t=1.6, snr=8.0, n_tx=4, n_rx=4):.0f} m")
