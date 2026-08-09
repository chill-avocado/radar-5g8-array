"""Tracks that can run at any angle, not just along the two axes.

The original stroke() found each corner by assuming one segment was vertical
and the other horizontal, so it could take the x from one and the y from the
other.  That is exact for a board drawn on a grid and useless for anything
diagonal.  This solves the two edges properly, so a track can leave at 45
degrees -- which is what a patch turned corner-up needs, because its feeds
come in perpendicular to edges that are themselves at 45 degrees.

The chamfer on a corner is scaled by how sharp the turn is.  A right angle
gets the full Douville and James mitre; a 45 degree turn is a much gentler
discontinuity and gets proportionally less.
"""

import math


def mitre_cut(w, h):
    """Chamfer leg for an optimum-mitred right-angle bend."""
    m = 52.0 + 65.0 * math.exp(-1.35 * w / h)
    return min(m / 100.0 * w * math.sqrt(2.0), 0.98 * w)


def _isect(a, b):
    """Where two offset edges cross, whatever angle they run at."""
    (x1, y1), (x2, y2) = a
    (x3, y3), (x4, y4) = b
    dx1, dy1 = x2 - x1, y2 - y1
    dx2, dy2 = x4 - x3, y4 - y3
    den = dx1 * dy2 - dy1 * dx2
    if abs(den) < 1e-12:                 # parallel: no corner to build
        return (x2, y2)
    t = ((x3 - x1) * dy2 - (y3 - y1) * dx2) / den
    return (x1 + t * dx1, y1 + t * dy1)


def stroke(points, w, cut90):
    """Polyline of any angles -> ONE polygon, with mitred outer corners.

    cut90 is the chamfer wanted at a square corner; gentler corners get less,
    in proportion to how far the track actually turns.
    """
    def off(p, q, s):
        dx, dy = q[0] - p[0], q[1] - p[1]
        n = math.hypot(dx, dy)
        nx, ny = -dy / n, dx / n
        return ((p[0] + nx * s, p[1] + ny * s), (q[0] + nx * s, q[1] + ny * s))

    half = w / 2.0
    sides = {}
    for sgn, key in ((+1, "L"), (-1, "R")):
        segs = [off(points[i], points[i + 1], sgn * half)
                for i in range(len(points) - 1)]
        chain = [segs[0][0]]
        for i in range(len(segs) - 1):
            p = _isect(segs[i], segs[i + 1])
            ax = points[i + 1][0] - points[i][0]
            ay = points[i + 1][1] - points[i][1]
            bx = points[i + 2][0] - points[i + 1][0]
            by = points[i + 2][1] - points[i + 1][1]
            na, nb = math.hypot(ax, ay), math.hypot(bx, by)
            crossz = ax * by - ay * bx
            outer = (crossz < 0) if sgn > 0 else (crossz > 0)
            if outer:
                # how far the track turns, 0 for straight on, pi for doubling back
                dot = (ax * bx + ay * by) / (na * nb)
                turn = math.acos(max(-1.0, min(1.0, dot)))
                c = cut90 * math.tan(turn / 4.0) / math.tan(math.pi / 4.0 / 2.0)
                c = min(c, 0.49 * na, 0.49 * nb)
                chain.append((p[0] - ax / na * c, p[1] - ay / na * c))
                chain.append((p[0] + bx / nb * c, p[1] + by / nb * c))
            else:
                chain.append(p)
        chain.append(segs[-1][1])
        sides[key] = chain
    return sides["L"] + sides["R"][::-1]


def seg(a, b, w, square_ends=True):
    """A single track of any angle as its own polygon."""
    dx, dy = b[0] - a[0], b[1] - a[1]
    n = math.hypot(dx, dy)
    ux, uy = dx / n, dy / n
    nx, ny = -uy * w / 2.0, ux * w / 2.0
    return [(a[0] + nx, a[1] + ny), (b[0] + nx, b[1] + ny),
            (b[0] - nx, b[1] - ny), (a[0] - nx, a[1] - ny)]


def diamond(cx, cy, side):
    """A square of the given side, stood on its corner."""
    d = side * math.sqrt(2.0) / 2.0
    return [(cx + d, cy), (cx, cy + d), (cx - d, cy), (cx, cy - d)]
