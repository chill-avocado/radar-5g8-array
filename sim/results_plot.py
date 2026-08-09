"""Plot the simulated performance of the finished element."""

import json
import os
import sys

import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

HERE = os.path.dirname(os.path.abspath(__file__))
RESULTS = os.path.join(HERE, "results")
tag = sys.argv[1] if len(sys.argv) > 1 else "fe8"
title = sys.argv[2] if len(sys.argv) > 2 else "RO4350B 0.762 mm"
d = json.load(open(os.path.join(RESULTS, f"fe_{tag}.json")))

f = np.array(d["f"]) / 1e9
s11 = 20 * np.log10(np.abs(np.array(d["s11"][0]) + 1j * np.array(d["s11"][1])))
ff = sorted(d["ff"].keys(), key=float)
fg = np.array([float(k) for k in ff])
ar = np.array([d["ff"][k]["AR_dB"] for k in ff])
gain = np.array([d["ff"][k]["Dmax_dBi"] for k in ff])

fig, ax = plt.subplots(1, 3, figsize=(16, 4.6))

ax[0].plot(f, s11, lw=2, color="#1f6fb4")
ax[0].axhline(-10, ls="--", lw=1, color="grey")
ax[0].axvspan(5.725, 5.875, color="#ffd27f", alpha=0.35, label="5.8 GHz ISM band")
ax[0].set_xlim(5.4, 6.2); ax[0].set_ylim(-40, 0)
ax[0].set_xlabel("frequency (GHz)"); ax[0].set_ylabel("input match S11 (dB)")
ax[0].set_title("Input match at the SMA")
ax[0].grid(alpha=0.3); ax[0].legend(fontsize=8)

ax[1].plot(fg, ar, "o-", lw=2, color="#b4451f")
ax[1].axhline(3, ls="--", lw=1, color="grey")
ax[1].axvspan(5.725, 5.875, color="#ffd27f", alpha=0.35)
ax[1].set_xlabel("frequency (GHz)")
ax[1].set_ylabel("axial ratio on boresight (dB)")
ax[1].set_title("Circular-polarisation purity\n(lower is rounder; 3 dB is the usual limit)")
ax[1].set_ylim(0, max(8, ar.max() * 1.15)); ax[1].grid(alpha=0.3)
for x, y in zip(fg, ar):
    ax[1].annotate(f"{y:.2f}", (x, y), fontsize=7, ha="center",
                   xytext=(0, 6), textcoords="offset points")

k = ff[int(np.argmin(np.abs(fg - 5.8)))]
th = np.array(d["ff"][k]["theta"])
for key, lab, col in (("ER_E", "wanted sense, E plane", "#1f6fb4"),
                      ("EL_E", "opposite sense, E plane", "#b4451f"),
                      ("ER_H", "wanted sense, H plane", "#2e8b57"),
                      ("EL_H", "opposite sense, H plane", "#c98b1f")):
    v = np.array(d["ff"][k][key])
    ax[2].plot(th, 20 * np.log10(v / v.max() + 1e-12) if key == "ER_E"
               else 20 * np.log10(v / np.array(d["ff"][k]["ER_E"]).max() + 1e-12),
               lw=1.8, label=lab, color=col,
               ls="-" if key.startswith("ER") else "--")
ax[2].set_xlim(-90, 90); ax[2].set_ylim(-35, 3)
ax[2].set_xlabel("angle off boresight (degrees)")
ax[2].set_ylabel("relative level (dB)")
ax[2].set_title(f"Radiation pattern at {k} GHz")
ax[2].grid(alpha=0.3); ax[2].legend(fontsize=7)

fig.suptitle(f"5.8 GHz circularly-polarised element on {title}"
             f"  -  simulated in openEMS (FDTD)", fontsize=11)
fig.tight_layout()
out = os.path.join(HERE, f"results_{tag}.png")
fig.savefig(out, dpi=125)
print("wrote", out)

i0 = int(np.argmin(np.abs(fg - 5.8)))
b = np.abs(np.array(d["s11"][0]) + 1j * np.array(d["s11"][1])) < 10 ** (-0.5)
print(f"  at 5.800 GHz : S11 {np.interp(5.8, f, s11):.2f} dB, "
      f"axial ratio {ar[i0]:.2f} dB, directivity {gain[i0]:.2f} dBi")
good = fg[ar < 3.0]
if len(good):
    print(f"  axial ratio under 3 dB from {good.min():.3f} to {good.max():.3f} GHz")
print(f"  S11 under -10 dB from {f[b].min():.3f} to {f[b].max():.3f} GHz")
