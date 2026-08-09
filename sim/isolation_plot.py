import json, os, numpy as np, matplotlib
matplotlib.use("Agg"); import matplotlib.pyplot as plt
HERE = os.path.dirname(os.path.abspath(__file__))
RESULTS = os.path.join(HERE, "results")
fig, ax = plt.subplots(1, 2, figsize=(12, 4.6))
for i, (tag, drv) in enumerate((("tx1", "TX1"), ("tx2", "TX2"))):
    d = json.load(open(os.path.join(RESULTS, f"fb_{tag}.json"))); f = np.array(d["f"]) / 1e9
    for nm, c in (("TX1", "#888888"), ("TX2", "#888888"),
                  ("RX1", "#1f6fb4"), ("RX2", "#b4451f")):
        if nm == drv:
            continue
        S = np.array(d["S"][nm][0]) + 1j * np.array(d["S"][nm][1])
        ax[i].plot(f, 20*np.log10(np.abs(S)), lw=2, color=c,
                   ls="--" if nm.startswith("TX") else "-",
                   label=f"into {nm}")
    ax[i].axvspan(5.725, 5.875, color="#ffd27f", alpha=0.35)
    ax[i].set_title(f"transmitting on {drv}", fontsize=10)
    ax[i].set_xlabel("frequency (GHz)"); ax[i].set_ylabel("coupling (dB)")
    ax[i].set_xlim(5.5, 6.1); ax[i].set_ylim(-70, -15)
    ax[i].grid(alpha=0.3); ax[i].legend(fontsize=8)
fig.suptitle("Measured isolation on the complete 76 x 176 mm board "
             "- all four elements, both pours, 260 vias (openEMS, -50 dB)",
             fontsize=11)
fig.tight_layout(); fig.savefig(os.path.join(HERE, "results_isolation.png"), dpi=125)
print("wrote results_isolation.png")
w = []
for tag in ("tx1", "tx2"):
    d = json.load(open(os.path.join(RESULTS, f"fb_{tag}.json"))); f = np.array(d["f"])
    b = (f >= 5.725e9) & (f <= 5.875e9)
    S1 = np.array(d["S"]["RX1"][0]) + 1j*np.array(d["S"]["RX1"][1])
    S2 = np.array(d["S"]["RX2"][0]) + 1j*np.array(d["S"]["RX2"][1])
    w.append((20*np.log10(np.sqrt(np.abs(S1)**2 + np.abs(S2)**2))[b]).max())
print(f"worst total transmit-to-receive leakage in band: {max(w):.1f} dB")
print(f"with +10 dBm transmitted that is {10+max(w):.1f} dBm at the receiver")
