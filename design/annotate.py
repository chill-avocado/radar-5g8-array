"""Draw each delivered board with every feature called out by name."""
import json, os, sys, math
import matplotlib; matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import Polygon as MplPoly, Rectangle, Circle, FancyArrowPatch

HERE = os.path.dirname(os.path.abspath(__file__))
WHICH = sys.argv[1]
B = json.load(open(os.path.join(HERE, f"board_{WHICH}.json")))
BW, BH = B["outline"]
CALL = json.loads(sys.argv[2])          # [[x, y, tx, ty, "text"], ...]
TITLE = sys.argv[3]

fig, ax = plt.subplots(figsize=(13.5, 13.5 * BH / BW * 0.62))
ax.add_patch(Rectangle((0, 0), BW, BH, facecolor="#0d4630", ec="#0a6", lw=1.4,
                       zorder=1))
for p in B["gnd_top"]:
    ax.add_patch(MplPoly(p, closed=True, facecolor="#1b7a4d", ec="none", zorder=2))
for x, y, d, pad in B["vias"]:
    ax.add_patch(Circle((x, y), pad / 2, facecolor="#a8bfb4", ec="none", zorder=3))
for p in B["mask"]:
    ax.add_patch(MplPoly(p, closed=True, facecolor="none", ec="#ffe08a",
                         lw=0.7, ls=(0, (3, 2)), zorder=6))
for p in B["top"]:
    ax.add_patch(MplPoly(p, closed=True, facecolor="#e8b13a", ec="#8a6410",
                         lw=0.3, zorder=4))
for r, x, y, w, h, n in B["pads"]:
    ax.add_patch(Rectangle((x - w / 2, y - h / 2), w, h,
                           facecolor="#d9d2c4", ec="#555", lw=0.4, zorder=5))
for x, y, d, pad in B["mounts"]:
    ax.add_patch(Circle((x, y), pad / 2, facecolor="#c9cdca", ec="#444", zorder=7))
    ax.add_patch(Circle((x, y), d / 2, facecolor="#ffffff", ec="#444", zorder=8))
for e in B["elements"]:
    cx, cy = e["centre"]
    ax.plot(cx, cy, "+", color="#111", ms=13, mew=1.6, zorder=9)
for n, x, y, side in B["ports"]:
    r = ((x - 4.75, -10.0) if side == "bottom" else (-10.0, y - 4.75))
    ax.add_patch(Rectangle(r, 10.0, 10.0, facecolor="#8f948f", ec="#333",
                           lw=0.7, zorder=2))

for x, y, tx, ty, text in CALL:
    ax.add_patch(FancyArrowPatch((tx, ty), (x, y), arrowstyle="-",
                                 color="#111", lw=0.9, zorder=12,
                                 shrinkA=2, shrinkB=1))
    ax.plot(x, y, "o", ms=3.4, color="#111", zorder=13)
    ha = "left" if tx > BW / 2 else "right"
    ax.text(tx + (0.8 if ha == "left" else -0.8), ty, text, fontsize=7.6,
            ha=ha, va="center", zorder=14, color="#111",
            bbox=dict(boxstyle="round,pad=0.32", fc="#fffdf6", ec="#888",
                      lw=0.5, alpha=0.97))

ax.set_xlim(-46, BW + 46); ax.set_ylim(-14, BH + 8)
ax.set_aspect("equal"); ax.axis("off")
ax.set_title(TITLE, fontsize=12, weight="bold")
fig.tight_layout()
RENDERS = os.path.join(HERE, "renders")
os.makedirs(RENDERS, exist_ok=True)
out = os.path.join(RENDERS, f"annotated_{WHICH}.png")
fig.savefig(out, dpi=150, facecolor="white")
print("wrote", out)
