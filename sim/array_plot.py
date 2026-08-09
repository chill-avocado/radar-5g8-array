import json, os, numpy as np, matplotlib
matplotlib.use("Agg"); import matplotlib.pyplot as plt
HERE = os.path.dirname(os.path.abspath(__file__))
RESULTS = os.path.join(HERE, "results")
D = {t: json.load(open(os.path.join(RESULTS, f"c2_{t}.json"))) for t in ("b1", "b2")}
LAB = {"b1": "element 1  (TX1 / RX1)", "b2": "element 2  (TX2 / RX2)"}
COL = {"b1": "#1f6fb4", "b2": "#b4451f"}
fig, ax = plt.subplots(1, 3, figsize=(16, 4.6))
for t, d in D.items():
    f = np.array(d["f"]) / 1e9
    s11 = 20*np.log10(np.abs(np.array(d["s11"][0]) + 1j*np.array(d["s11"][1])))
    s21 = 20*np.log10(np.abs(np.array(d["s21"][0]) + 1j*np.array(d["s21"][1])))
    ff = sorted(d["ff"], key=float); fg = np.array([float(k) for k in ff])
    ar = np.array([d["ff"][k]["AR_dB"] for k in ff])
    ax[0].plot(f, s11, lw=2, color=COL[t], label=LAB[t])
    ax[1].plot(fg, ar, "o-", lw=2, color=COL[t], label=LAB[t])
    ax[2].plot(f, s21, lw=2, color=COL[t], label=LAB[t])
for a, ttl, yl in ((ax[0], "Input match with the neighbour present", "S11 (dB)"),
                   (ax[1], "Circular-polarisation purity in the array", "axial ratio (dB)"),
                   (ax[2], "Leakage into the neighbouring element", "S21 (dB)")):
    a.axvspan(5.725, 5.875, color="#ffd27f", alpha=0.3)
    a.set_xlabel("frequency (GHz)"); a.set_ylabel(yl); a.set_title(ttl, fontsize=10)
    a.grid(alpha=0.3); a.legend(fontsize=8); a.set_xlim(5.6, 6.0)
ax[0].axhline(-10, ls="--", lw=1, c="grey"); ax[0].set_ylim(-35, 0)
ax[1].axhline(3, ls="--", lw=1, c="grey"); ax[1].set_ylim(0, 8)
ax[2].set_ylim(-30, -10)
fig.suptitle("5.8 GHz array, both elements at the 25.844 mm half-wave pitch "
             "- openEMS, converged to -40 dB", fontsize=11)
fig.tight_layout(); fig.savefig("results_array.png", dpi=125)
for t, d in D.items():
    ff = sorted(d["ff"], key=float); fg = np.array([float(k) for k in ff])
    ar = np.array([d["ff"][k]["AR_dB"] for k in ff])
    u = fg[ar < 3.0]
    print(f"{LAB[t]}: AR at 5.800 = {ar[list(fg).index(5.8)]:.2f} dB; "
          f"under 3 dB over {u.min():.3f}-{u.max():.3f} GHz" if len(u) else "")
print("wrote results_array.png")
