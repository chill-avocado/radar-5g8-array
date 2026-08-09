"""Board-drawing machinery shared by the two amplifier boards.

Polygons in millimetres, on a board whose origin is its bottom-left corner.
Everything here is geometry and bookkeeping: lay a track, place a part's land
pattern, drop a via, decide which pieces of copper are really one net, and
stitch the ground.  Nothing here knows what an amplifier is.

The two rules that matter, both learned the hard way on this project:

  * A via's net is recorded, never inferred.  Guessing it from the pad size
    put the reference on the ground net once and the supply on the reference
    net later, and both times it looked like a layout fault rather than a
    bookkeeping one.
  * The via list and its net list are appended together and checked.  Letting
    them drift by one silently shifts every net after it, and the board that
    comes out looks perfect and is wrong.
"""

import math

from geom import rect

R0402 = dict(w=0.62, h=0.62, gap=0.55)
VIA_D, VIA_PAD = 0.40, 0.75
EDGE_KEEP = 0.30


class Board:
    """A two- or four-layer board being drawn.

    Board size and the fifty-ohm width are per-instance, so more than one
    board can use this without editing it.
    """

    CLEAR = 0.15                # the tightest gap the router may leave

    def __init__(self, bw, bh, w50, launch_gap, parts=None):
        self.BW, self.BH, self.W50 = bw, bh, w50
        self.launch_gap = launch_gap
        self.PARTS = parts or {}
        self.top, self.nets = [], []
        self.gnd_top = []
        self.inner, self.inner_net = [], []
        self.vias, self.via_net = [], []
        self.mounts, self.labels, self.ports, self.parts = [], [], [], []
        # Indices of polygons that are component LANDS rather than track.  The
        # KiCad build gets those from the footprints; drawing them there as
        # well would put a second piece of copper under every pad, on whatever
        # net the drawing said, which is how a thermal pad ends up shorted to
        # the trace beside it.
        self.pads = set()
        self.failed = []

    # ------------------------------------------------------------ primitives
    def _via(self, x, y, drill, pad, net="GND"):
        self.vias.append((x, y, drill, pad))
        self.via_net.append(net)
        assert len(self.vias) == len(self.via_net)

    def _add(self, poly, net="?", is_pad=False):
        if is_pad:
            self.pads.add(len(self.top))
        self.top.append(poly)
        self.nets.append(net)

    def line(self, x0, y0, x1, y1, net="?", w=None):
        """A straight run, either horizontal or vertical."""
        w = self.W50 if w is None else w
        if abs(y1 - y0) < 1e-9:
            self._add(rect(min(x0, x1), y0 - w / 2, max(x0, x1), y0 + w / 2),
                      net)
        elif abs(x1 - x0) < 1e-9:
            self._add(rect(x0 - w / 2, min(y0, y1), x0 + w / 2, max(y0, y1)),
                      net)
        else:
            raise ValueError("runs are drawn one axis at a time")

    def corner(self, x, y, net="?", w=None):
        """The square that fills a right-angle turn."""
        w = self.W50 if w is None else w
        self._add(rect(x - w / 2, y - w / 2, x + w / 2, y + w / 2), net)

    def fiducial(self, x, y, d=1.00, mask=2.00):
        self.parts.append(dict(kind="FID", x=x, y=y, d=d, mask=mask))

    # ------------------------------------------------------------ net naming
    def unify_nets(self):
        """Give every piece of copper that touches its neighbours one name.

        A track called TX1_PA running into a pad the datasheet calls RFOUT is
        one electrical node, not two, and calling it two makes the checker
        report a short where the board is simply connected.  This walks the
        copper, groups everything that touches, and names each group once --
        ground first, then whatever the group was called earliest.
        """
        n = len(self.top)
        parent = list(range(n))

        def find(i):
            while parent[i] != i:
                parent[i] = parent[parent[i]]
                i = parent[i]
            return i

        def union(i, j):
            a, b = find(i), find(j)
            if a != b:
                parent[b] = a

        box = []
        for p in self.top:
            xs = [q[0] for q in p]
            ys = [q[1] for q in p]
            box.append((min(xs), min(ys), max(xs), max(ys)))
        order = sorted(range(n), key=lambda i: box[i][0])
        for ii, i in enumerate(order):
            a0, b0, a1, b1 = box[i]
            for j in order[ii + 1:]:
                c0, d0, c1, d1 = box[j]
                if c0 > a1 + 0.01:
                    break
                if not (c0 > a1 + 0.01 or c1 + 0.01 < a0
                        or d0 > b1 + 0.01 or d1 + 0.01 < b0):
                    union(i, j)

        # A merge between two nets that BOTH land on a component pin is not
        # two names for one node -- it is a short, and merging it deletes the
        # evidence before the board is ever written.  A clean check on a board
        # built this way proves nothing.  So refuse, and say where.
        PLACEHOLDER = {"?", "sig", "", None}
        # self.pads is a set of polygon indices here and a list of pad
        # tuples in array.py; take the net name out of whichever it is.
        pin_nets = set()
        for p in self.pads:
            if isinstance(p, int):
                if 0 <= p < len(self.nets):
                    pin_nets.add(self.nets[p])
            elif len(p) > 5:
                pin_nets.add(p[5])
        pin_nets -= PLACEHOLDER
        shorts = {}
        for i in range(n):
            for j in range(i + 1, n):
                if find(i) != find(j):
                    continue
                a, b = self.nets[i], self.nets[j]
                if a == b or a in PLACEHOLDER or b in PLACEHOLDER:
                    continue
                if a in pin_nets and b in pin_nets:
                    xs = [q[0] for q in self.top[i]]
                    ys = [q[1] for q in self.top[i]]
                    shorts.setdefault(tuple(sorted((a, b))),
                                      (round(sum(xs) / len(xs), 2),
                                       round(sum(ys) / len(ys), 2)))
        if shorts:
            msg = "\n".join(
                f"    {a} shorted to {b}  near ({x}, {y}) mm"
                for (a, b), (x, y) in sorted(shorts.items()))
            raise SystemExit(
                f"copper joins {len(shorts)} pair(s) of nets that each reach a "
                f"component pin:\n{msg}\n"
                "  These are shorts, not two names for one node.  Refusing to "
                "write a board that would hide them from the checker.")

        RANK = {"GND": 0}
        groups = {}
        for i in range(n):
            groups.setdefault(find(i), []).append(i)
        for root, members in groups.items():
            names = [self.nets[i] for i in members]
            pick = None
            for nm in names:
                if nm in RANK:
                    pick = nm
                    break
            if pick is None:
                pick = next((nm for nm in names if nm not in ("?", "sig")),
                            names[0])
            for i in members:
                self.nets[i] = pick

        # a via takes the name of whatever it lands in
        for k, ((vx, vy, vd, vp), vn) in enumerate(
                zip(self.vias, self.via_net)):
            if vn == "GND":
                continue
            for i, (a0, b0, a1, b1) in enumerate(box):
                if a0 - 0.02 < vx < a1 + 0.02 and b0 - 0.02 < vy < b1 + 0.02:
                    self.via_net[k] = self.nets[i]
                    break

    # -------------------------------------------------------- what is in the way
    def obstacles(self, skip_net=None):
        out = []
        for p, nt in zip(self.top, self.nets):
            if nt == skip_net:
                continue
            xs = [q[0] for q in p]
            ys = [q[1] for q in p]
            out.append((min(xs), min(ys), max(xs), max(ys), nt))
        for (vx, vy, d, pad), vn in zip(self.vias, self.via_net):
            if vn == skip_net:
                continue
            out.append((vx - pad / 2, vy - pad / 2,
                        vx + pad / 2, vy + pad / 2, vn))
        for x, y, d, pad in self.mounts:
            out.append((x - pad / 2, y - pad / 2,
                        x + pad / 2, y + pad / 2, "MOUNT"))
        # The buried layer counts too.  A through-via is through: it does not
        # care that the run it lands on is invisible from the surface.
        for p, nt in zip(self.inner, self.inner_net):
            if nt == skip_net:
                continue
            xs = [q[0] for q in p]
            ys = [q[1] for q in p]
            out.append((min(xs) - 0.35, min(ys) - 0.35,
                        max(xs) + 0.35, max(ys) + 0.35, nt))
        return out

    def seg_clear(self, x0, y0, x1, y1, w, obs):
        a0, b0 = min(x0, x1) - w / 2, min(y0, y1) - w / 2
        a1, b1 = max(x0, x1) + w / 2, max(y0, y1) + w / 2
        c = self.CLEAR
        for (p0, q0, p1, q1, nt) in obs:
            if nt == "GND":
                continue          # ground pours are meant to be touched
            if not (p0 - c > a1 or p1 + c < a0
                    or q0 - c > b1 or q1 + c < b0):
                return False
        return True

    # ------------------------------------------------------------- connectors
    def sma_launch(self, name, pt, side, w=None):
        """Grounded-coplanar end launch, identical to the array boards'."""
        w = self.W50 if w is None else w
        g, depth, half = self.launch_gap, 4.30, 5.60
        x, y = pt
        if side in ("bottom", "top"):
            y0, yd = ((EDGE_KEEP, depth) if side == "bottom"
                      else (self.BH - depth, self.BH - EDGE_KEEP))
            self.gnd_top.append(rect(x - half, y0, x - w / 2 - g, yd))
            self.gnd_top.append(rect(x + w / 2 + g, y0, x + half, yd))
            for i in range(5):
                yy = ((y0 + 0.9 + i * 1.27) if side == "bottom"
                      else (yd - 0.9 - i * 1.27))
                if not (y0 + 0.5 < yy < yd - 0.5):
                    break
                self._via(x - w / 2 - g - 0.85, yy, VIA_D, VIA_PAD)
                self._via(x + w / 2 + g + 0.85, yy, VIA_D, VIA_PAD)
        else:
            x0, xd = ((EDGE_KEEP, depth) if side == "left"
                      else (self.BW - depth, self.BW - EDGE_KEEP))
            self.gnd_top.append(rect(x0, y - half, xd, y - w / 2 - g))
            self.gnd_top.append(rect(x0, y + w / 2 + g, xd, y + half))
            for i in range(5):
                xx = ((x0 + 0.9 + i * 1.27) if side == "left"
                      else (xd - 0.9 - i * 1.27))
                if not (x0 + 0.5 < xx < xd - 0.5):
                    break
                self._via(xx, y - w / 2 - g - 0.85, VIA_D, VIA_PAD)
                self._via(xx, y + w / 2 + g + 0.85, VIA_D, VIA_PAD)
        # the connector's name is drawn once, by the board generator, from
        # this list -- putting it here as well printed it twice
        self.ports.append((name, x, y, side))

    # ------------------------------------------------------------ bought parts
    def amplifier(self, ref, kind, cx, cy, rot=0, thermal=True):
        """Place a real part: its land pattern, with its pins on their real nets.

        Pin numbering is the industry one -- leadless parts count anticlockwise
        from the corner mark.  Getting that backwards is how a supply pin ends
        up on the radio-frequency track.
        """
        pt = self.PARTS[kind]
        pads = []                        # (number, x, y, w, h, net)

        def turn(dx, dy):
            if rot == 180:
                return -dx, -dy
            if rot == 90:
                return -dy, dx
            if rot == 270:
                return dy, -dx
            return dx, dy

        if pt["pkg"].startswith("QFN"):
            n, p = pt["n_side"], pt["pitch"]
            pw, pl, body, ep = pt["pad_w"], pt["pad_l"], pt["body"], pt["ep"]
            # The land span comes from the datasheet's own recommended pattern
            # where it gives one; the rule of thumb is only a fallback.
            half = pt.get("land_half", body / 2.0 + pl / 2.0 - 0.15)
            span = (n - 1) * p / 2.0
            i = 1
            for k in range(n):                       # down the left
                dx, dy = turn(-half, span - k * p)
                pads.append((i, cx + dx, cy + dy, pl, pw, pt["pins"][i]))
                i += 1
            for k in range(n):                       # across the bottom
                dx, dy = turn(-span + k * p, -half)
                pads.append((i, cx + dx, cy + dy, pw, pl, pt["pins"][i]))
                i += 1
            for k in range(n):                       # up the right
                dx, dy = turn(half, -span + k * p)
                pads.append((i, cx + dx, cy + dy, pl, pw, pt["pins"][i]))
                i += 1
            for k in range(n):                       # back across the top
                dx, dy = turn(span - k * p, half)
                pads.append((i, cx + dx, cy + dy, pw, pl, pt["pins"][i]))
                i += 1
            pads.append((i, cx, cy, ep, ep, pt["pins"][i]))
            ep_w = ep_h = ep
        else:
            raise ValueError(f"{kind}: only leadless parts are placed here")

        for num, px, py, w, h, nt in pads:
            self._add(rect(px - w / 2, py - h / 2, px + w / 2, py + h / 2),
                      nt, True)

        # a thermal pad is only a thermal pad if it reaches the plane
        if ep_w and ep_h and thermal:
            nx = max(1, int(ep_w / 0.65))
            ny = max(1, int(ep_h / 0.65))
            for i in range(nx):
                for j in range(ny):
                    self._via(cx - ep_w / 2 + (i + 0.5) * ep_w / nx,
                              cy - ep_h / 2 + (j + 0.5) * ep_h / ny,
                              0.3, 0.55)

        assert not [v for v in pt["pins"].values() if v.startswith("V")], \
            f"{kind}: supply pins are decoupled by the caller, not here"

        self.parts.append(dict(kind=kind, ref=ref, x=cx, y=cy, rot=rot,
                               mpn=pt["mpn"], pkg=pt["pkg"],
                               pads=[[n_, round(a, 4), round(b, 4),
                                      round(c, 4), round(d, 4), e]
                                     for n_, a, b, c, d, e in pads]))
        return pads

    # ------------------------------------------------------------------ ground
    def pour(self, clear=0.60):
        """Stitch the ground, everywhere there is room for a hole.

        The pour itself is a zone laid down by the CAD, which knows about pads
        and mask and clearance.  All this does is decide where a stitching via
        may be drilled: not through a pad, not through a buried run, not on
        top of another hole.
        """
        keep = []
        for p, nt in zip(self.top, self.nets):
            xs = [q[0] for q in p]
            ys = [q[1] for q in p]
            keep.append((min(xs) - clear, min(ys) - clear,
                         max(xs) + clear, max(ys) + clear))
        for x, y, d, pad in self.mounts:
            keep.append((x - pad / 2 - clear, y - pad / 2 - clear,
                         x + pad / 2 + clear, y + pad / 2 + clear))
        for (vx, vy, vd, vp), vn in zip(self.vias, self.via_net):
            r = vp / 2 + clear
            keep.append((vx - r, vy - r, vx + r, vy + r))
        for p in self.inner:
            xs = [q[0] for q in p]
            ys = [q[1] for q in p]
            keep.append((min(xs) - 1.0, min(ys) - 1.0,
                         max(xs) + 1.0, max(ys) + 1.0))
        for g in list(self.gnd_top):
            xs = [q[0] for q in g]
            ys = [q[1] for q in g]
            keep.append((min(xs), min(ys), max(xs), max(ys)))

        def free(x, y, r=0.0):
            return not any(a - r < x < c + r and b - r < y < d + r
                           for a, b, c, d in keep)

        for i in range(int((self.BW - 2 * 1.5) / 3.0) + 1):
            for j in range(int((self.BH - 2 * 1.5) / 3.0) + 1):
                vx, vy = 3.0 + i * 3.0, 3.0 + j * 3.0
                if vx >= self.BW - 2.0 or vy >= self.BH - 2.0:
                    continue
                if not free(vx, vy, VIA_PAD / 2 + 0.2):
                    continue
                if any(math.hypot(vx - ox, vy - oy) < (VIA_PAD + op) / 2 + 0.35
                       for ox, oy, od, op in self.vias):
                    continue
                self._via(vx, vy, VIA_D, VIA_PAD)
