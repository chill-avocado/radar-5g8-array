"""Geometric clearance checks on the assembled board.

Runs before anything is written to KiCad, because a short between the signal
copper and a ground pour is far cheaper to find here than in a Gerber viewer.
Checks: signal-to-ground, signal-to-via, signal-to-mounting-hole, and every
piece of copper against the routed board edge.
"""

import math


def _pt_seg(p, a, b):
    """Distance from point p to segment ab."""
    ax, ay = a
    vx, vy = b[0] - ax, b[1] - ay
    L = vx * vx + vy * vy
    if L < 1e-18:
        return math.hypot(p[0] - ax, p[1] - ay)
    t = max(0.0, min(1.0, ((p[0] - ax) * vx + (p[1] - ay) * vy) / L))
    return math.hypot(p[0] - ax - t * vx, p[1] - ay - t * vy)


def _cross(o, a, b):
    return (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0])


def _seg_dist(p, q, r, s):
    """Shortest distance between segments pq and rs.

    For two 2-D segments that do not cross, the minimum is always reached at
    an endpoint of one of them, so four point-to-segment tests are exact.
    """
    d1, d2 = _cross(r, s, p), _cross(r, s, q)
    d3, d4 = _cross(p, q, r), _cross(p, q, s)
    if ((d1 > 0) != (d2 > 0)) and ((d3 > 0) != (d4 > 0)):
        return 0.0
    return min(_pt_seg(p, r, s), _pt_seg(q, r, s),
               _pt_seg(r, p, q), _pt_seg(s, p, q))


def _point_in(poly, pt):
    x, y = pt
    inside = False
    n = len(poly)
    for i in range(n):
        x1, y1 = poly[i]
        x2, y2 = poly[(i + 1) % n]
        if (y1 > y) != (y2 > y):
            xin = x1 + (y - y1) * (x2 - x1) / (y2 - y1)
            if x < xin:
                inside = not inside
    return inside


def poly_dist(a, b):
    """Distance between two polygons; 0.0 if they overlap."""
    for p in a:
        if _point_in(b, p):
            return 0.0
    for p in b:
        if _point_in(a, p):
            return 0.0
    best = float("inf")
    for i in range(len(a)):
        for j in range(len(b)):
            best = min(best, _seg_dist(a[i], a[(i + 1) % len(a)],
                                       b[j], b[(j + 1) % len(b)]))
            if best == 0.0:
                return 0.0
    return best


def bbox(p):
    xs = [q[0] for q in p]
    ys = [q[1] for q in p]
    return min(xs), min(ys), max(xs), max(ys)


def _far(a, b, lim):
    ax0, ay0, ax1, ay1 = bbox(a)
    bx0, by0, bx1, by1 = bbox(b)
    return (ax0 - bx1 > lim or bx0 - ax1 > lim or
            ay0 - by1 > lim or by0 - ay1 > lim)


def circle_poly(x, y, r, n=16):
    return [(x + r * math.cos(2 * math.pi * i / n),
             y + r * math.sin(2 * math.pi * i / n)) for i in range(n)]


def check(board, bw, bh, min_clear=0.20, edge_clear=0.25,
          report_below=1.2, edge_exempt=()):
    """Return (violations, closest_pairs)."""
    sig = list(board["top"])
    # Net of each signal polygon, so an element's own overlapping shapes are
    # not reported as shorts against themselves.
    signet = list(board.get("top_net", ["?"] * len(sig)))
    gnd = list(board["gnd_top"])
    for ref, x, y, w, h, net in board["pads"]:
        poly = [(x - w / 2, y - h / 2), (x + w / 2, y - h / 2),
                (x + w / 2, y + h / 2), (x - w / 2, y + h / 2)]
        if net == "GND":
            gnd.append(poly)
        else:
            sig.append(poly)
            signet.append(board.get("term_net", {}).get(ref[:-1], "?"))
    obstacles = [("via", circle_poly(x, y, pa / 2)) for x, y, d, pa in board["vias"]]
    obstacles += [("mount", circle_poly(x, y, pa / 2))
                  for x, y, d, pa in board["mounts"]]

    viol, close = [], []
    for i, s in enumerate(sig):
        for j, g in enumerate(gnd):
            if _far(s, g, report_below):
                continue
            d = poly_dist(s, g)
            if d < min_clear:
                viol.append(("signal-to-ground", i, j, round(d, 4)))
            elif d < report_below:
                close.append(("signal-to-ground", i, j, round(d, 4)))
        for kind, o in obstacles:
            if _far(s, o, report_below):
                continue
            d = poly_dist(s, o)
            if d < min_clear:
                viol.append((f"signal-to-{kind}", i, -1, round(d, 4)))
            elif d < report_below:
                close.append((f"signal-to-{kind}", i, -1, round(d, 4)))

    for i in range(len(sig)):
        for j in range(i + 1, len(sig)):
            if signet[i] == signet[j] or _far(sig[i], sig[j], report_below):
                continue
            dd = poly_dist(sig[i], sig[j])
            if dd < min_clear:
                viol.append(("signal-to-signal", i, j, round(dd, 4)))
            elif dd < report_below:
                close.append(("signal-to-signal", i, j, round(dd, 4)))

    for name, polys in (("signal", sig), ("ground", gnd)):
        for i, p in enumerate(polys):
            if name == "signal" and i in edge_exempt:
                continue          # end-launch traces deliberately reach the edge
            x0, y0, x1, y1 = bbox(p)
            m = min(x0, y0, bw - x1, bh - y1)
            if m < edge_clear:
                viol.append((f"{name}-to-board-edge", i, -1, round(m, 4)))
    return viol, sorted(close, key=lambda t: t[3])
