"""Parse and render RS-274X Gerbers to prove the exported files are right.

This deliberately does NOT ask KiCad to draw the board again -- it reads the
files that will actually be sent to the fabricator and rasterises them from
scratch, so a mistake in the export stage cannot hide behind a correct-looking
CAD window.  It also reports the copper area per layer and the drill tally,
which are easy numbers to sanity-check against the design intent.
"""

import glob
import math
import os
import re
import sys

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import Polygon as MPoly, Circle, Rectangle


class Gerber:
    def __init__(self, path):
        self.path = path
        self.scale = 1e-6
        self.apertures = {}
        self.shapes = []          # (kind, data, polarity)
        self.parse()

    # ------------------------------------------------------------------
    def parse(self):
        txt = open(self.path).read()
        fmt = re.search(r"%FSLAX(\d)(\d)Y\d\d\*%", txt)
        if fmt:
            self.scale = 10 ** -int(fmt.group(2))
        if "%MOIN*%" in txt:
            self.scale *= 25.4

        for m in re.finditer(r"%ADD(\d+)([A-Za-z_0-9.]+),?([^*]*)\*%", txt):
            num, shape, params = int(m.group(1)), m.group(2), m.group(3)
            vals = [float(v) for v in params.split("X") if v.strip()] if params else []
            self.apertures[num] = (shape, vals)

        pol, cur, region, pts = "D", None, False, []
        x = y = 0.0
        interp = 1
        for line in txt.splitlines():
            line = line.strip()
            if line.startswith("%LP"):
                pol = line[3]
                continue
            if line.startswith("G36"):
                region, pts = True, []
                continue
            if line.startswith("G37"):
                if len(pts) > 2:
                    self.shapes.append(("poly", pts, pol))
                region = False
                continue
            if re.match(r"^G0?1\*?$", line):
                interp = 1
            if re.match(r"^G0?[23]", line):
                interp = 2
            m = re.match(r"^(?:G0?([123]))?D?(\d+)?\*?$", line)
            ap = re.match(r"^D(\d+)\*$", line)
            if ap and int(ap.group(1)) >= 10:
                cur = int(ap.group(1))
                continue
            coord = re.match(
                r"^(?:G0?([123]))?"
                r"(?:X([+-]?\d+))?(?:Y([+-]?\d+))?"
                r"(?:I([+-]?\d+))?(?:J([+-]?\d+))?"
                r"D0?([123])\*$", line)
            if not coord:
                continue
            g, xs, ys, iv, jv, d = coord.groups()
            if g:
                interp = int(g)
            nx = float(xs) * self.scale if xs else x
            ny = float(ys) * self.scale if ys else y
            if d == "2":                                  # move
                if region and len(pts) > 2:
                    self.shapes.append(("poly", pts, pol))
                pts = [(nx, ny)] if region else []
            elif d == "1":                                # draw
                if region:
                    if interp in (2, 3) and iv is not None:
                        pts += self._arc(x, y, nx, ny,
                                         float(iv) * self.scale,
                                         float(jv or 0) * self.scale, interp)
                    else:
                        pts.append((nx, ny))
                else:
                    self.shapes.append(("line", (x, y, nx, ny, cur), pol))
            elif d == "3":                                # flash
                self.shapes.append(("flash", (nx, ny, cur), pol))
            x, y = nx, ny
        if region and len(pts) > 2:
            self.shapes.append(("poly", pts, pol))

    @staticmethod
    def _arc(x0, y0, x1, y1, i, j, interp, n=24):
        cx, cy = x0 + i, y0 + j
        a0 = math.atan2(y0 - cy, x0 - cx)
        a1 = math.atan2(y1 - cy, x1 - cx)
        r = math.hypot(x0 - cx, y0 - cy)
        if interp == 2:                       # clockwise
            while a1 > a0:
                a1 -= 2 * math.pi
        else:
            while a1 < a0:
                a1 += 2 * math.pi
        return [(cx + r * math.cos(a0 + (a1 - a0) * k / n),
                 cy + r * math.sin(a0 + (a1 - a0) * k / n))
                for k in range(1, n + 1)]

    # ------------------------------------------------------------------
    def area(self):
        tot = 0.0
        for kind, data, pol in self.shapes:
            s = 1.0 if pol == "D" else -1.0
            if kind == "poly":
                a = 0.0
                for k in range(len(data)):
                    x0, y0 = data[k]
                    x1, y1 = data[(k + 1) % len(data)]
                    a += x0 * y1 - x1 * y0
                tot += s * abs(a) / 2.0
            elif kind == "flash":
                sh, v = self.apertures.get(data[2], ("C", [0]))
                tot += s * (math.pi * (v[0] / 2) ** 2 if sh == "C"
                            else v[0] * v[1] if len(v) > 1 else 0.0)
        return tot

    def draw(self, ax, colour):
        for kind, data, pol in self.shapes:
            c = colour if pol == "D" else "#101010"
            if kind == "poly":
                ax.add_patch(MPoly(data, closed=True, facecolor=c,
                                   edgecolor="none"))
            elif kind == "line":
                x0, y0, x1, y1, ap = data
                sh, v = self.apertures.get(ap, ("C", [0.1]))
                ax.plot([x0, x1], [y0, y1], color=c, linewidth=max(v[0], 0.03) * 2.2,
                        solid_capstyle="round")
            elif kind == "flash":
                px, py, apn = data
                sh, v = self.apertures.get(apn, ("C", [0.5]))
                if sh == "C":
                    ax.add_patch(Circle((px, py), v[0] / 2, facecolor=c,
                                        edgecolor="none"))
                elif sh in ("R", "O"):
                    ax.add_patch(Rectangle((px - v[0] / 2, py - v[1] / 2),
                                           v[0], v[1], facecolor=c,
                                           edgecolor="none"))


def drills(path):
    out, cur = {}, None
    for line in open(path):
        m = re.match(r"^T(\d+)C([\d.]+)", line.strip())
        if m:
            out[int(m.group(1))] = [float(m.group(2)), 0]
            continue
        m = re.match(r"^T(\d+)$", line.strip())
        if m:
            cur = int(m.group(1))
            continue
        if line.startswith("X") and cur in out:
            out[cur][1] += 1
    return out


if __name__ == "__main__":
    d = sys.argv[1]
    g = {os.path.basename(p).split("-")[-1][:-4]: Gerber(p)
         for p in sorted(glob.glob(os.path.join(d, "*.gbr")))
         if "drl_map" not in p}
    print(f"{'layer':<16}{'shapes':>8}{'apertures':>11}{'copper mm2':>13}")
    for k, v in g.items():
        print(f"{k:<16}{len(v.shapes):>8}{len(v.apertures):>11}"
              f"{v.area():>13.1f}")
    print()
    for p in sorted(glob.glob(os.path.join(d, "*.drl"))):
        tools = drills(p)
        tot = sum(t[1] for t in tools.values())
        print(f"{os.path.basename(p)}: {tot} holes")
        for t, (dia, n) in sorted(tools.items()):
            print(f"   T{t:02d}  {dia:.3f} mm  x{n}")

    # KiCad plots with y increasing downwards, so the exported coordinates
    # are negative; fit the view to whatever the files actually contain
    # rather than to an assumed board box.
    XS, YS = [], []
    for v in g.values():
        for kind, data, pol in v.shapes:
            pts = (data if kind == "poly" else
                   [(data[0], data[1]), (data[2], data[3])] if kind == "line"
                   else [(data[0], data[1])])
            XS += [p[0] for p in pts]
            YS += [p[1] for p in pts]
    x0, x1, y0, y1 = min(XS), max(XS), min(YS), max(YS)

    order = [("Edge_Cuts", "#ffffff"), ("B_Cu", "#7a4a12"),
             ("F_Cu", "#e0a72a"), ("F_Mask", "#2e7d5b"),
             ("F_Silkscreen", "#f2f2f2")]
    fig, axes = plt.subplots(1, len([o for o in order if o[0] in g]) + 1,
                             figsize=(17, 15))
    for ax, (name, col) in zip(axes, [o for o in order if o[0] in g]):
        ax.add_patch(Rectangle((x0 - 2, y0 - 2), x1 - x0 + 4, y1 - y0 + 4,
                               facecolor="#111111"))
        g[name].draw(ax, col)
        ax.set_title(name, fontsize=9)
    ax = axes[-1]
    ax.add_patch(Rectangle((x0 - 2, y0 - 2), x1 - x0 + 4, y1 - y0 + 4,
                           facecolor="#111111"))
    for name, col in order:
        if name in g and name != "F_Mask":
            g[name].draw(ax, col)
    ax.set_title("composite (as fabricated)", fontsize=9)
    for ax in axes:
        ax.set_xlim(x0 - 3, x1 + 3)
        ax.set_ylim(y0 - 3, y1 + 3)
        ax.set_aspect("equal")
        ax.axis("off")
    fig.tight_layout()
    out = os.path.join(d, "gerber_check.png")
    fig.savefig(out, dpi=110, facecolor="#222222")
    print("\nwrote", out)
