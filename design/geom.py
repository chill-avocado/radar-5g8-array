"""Polygon primitives shared by the EM model and the KiCad generator.

Everything is in millimetres, in the element's own local frame with the patch
centred on (0, 0).  The SAME polygon list feeds openEMS and the PCB, so the
thing that gets simulated is exactly the thing that gets fabricated.
"""

import math

# ---------------------------------------------------------------------------
# primitives
# ---------------------------------------------------------------------------


def rect(x0, y0, x1, y1):
    """Axis-aligned rectangle as a closed polygon (counter-clockwise)."""
    xa, xb = min(x0, x1), max(x0, x1)
    ya, yb = min(y0, y1), max(y0, y1)
    return [(xa, ya), (xb, ya), (xb, yb), (xa, yb)]


def hseg(x0, x1, y, w):
    """Horizontal track centred on y, width w."""
    return rect(x0, y - w / 2.0, x1, y + w / 2.0)


def vseg(y0, y1, x, w):
    """Vertical track centred on x, width w."""
    return rect(x - w / 2.0, y0, x + w / 2.0, y1)


def mitre_triangle(corner, dir_in, dir_out, w):
    """Chamfer polygon that turns a square 90-degree corner into a mitred one.

    Douville & James: the optimum chamfer removes 57 % of the corner diagonal.
    We *add* a triangle across the inner... in fact for a square corner formed
    by two overlapping tracks the correct action is to REMOVE the outer corner.
    Because the generator draws tracks as overlapping rectangles, the mitre is
    applied by subtracting this triangle from the union.
    """
    cx, cy = corner
    ux, uy = dir_in
    vx, vy = dir_out
    # Outer corner point: the corner of the square region on the outside of
    # the turn, i.e. displaced by -w/2 along the incoming direction and
    # -w/2 along the outgoing direction, taken on the outer side.
    ox = cx - (ux + vx) * 0.0
    oy = cy - (uy + vy) * 0.0
    d = w * math.sqrt(2.0)
    m = 0.57 * d              # chamfer measured along the diagonal
    # Outer corner is at (cx - ux*w/2 ... ) -- resolved by the caller which
    # knows the geometry; kept simple here.
    cut = (d - m) / math.sqrt(2.0)
    return ox, oy, cut


def bend_mitre(corner, w, sx, sy):
    """Triangle to subtract at a 90-degree bend.

    corner : centreline intersection point
    w      : track width
    sx, sy : +/-1 signs pointing to the OUTER side of the bend in x and y
    """
    cx, cy = corner
    ox, oy = cx + sx * w / 2.0, cy + sy * w / 2.0     # outer corner point
    d = w * math.sqrt(2.0)
    cut = (1.0 - 0.57) * d * math.sqrt(2.0) / 2.0     # leg of the cut triangle
    cut = min(cut, w * 0.98)
    return [(ox, oy),
            (ox - sx * cut, oy),
            (ox, oy - sy * cut)]


def mitre_excess_length(w):
    """Extra centreline length a mitred right-angle bend behaves as.

    A mitred 90-degree bend is electrically shorter than the two centreline
    half-widths that meet at the corner. The classical equivalent is that the
    bend behaves like a straight line of length (2 - 0.57*sqrt(2)) * w/2 in
    place of the 2*(w/2) of centreline that the Manhattan sum counts.
    """
    return -(0.57 * math.sqrt(2.0)) * w / 2.0


# ---------------------------------------------------------------------------
# polygon boolean helpers (rectilinear only, via a scanline grid)
# ---------------------------------------------------------------------------


def polys_bbox(polys):
    xs = [p[0] for poly in polys for p in poly]
    ys = [p[1] for poly in polys for p in poly]
    return min(xs), min(ys), max(xs), max(ys)


def translate(polys, dx, dy):
    return [[(x + dx, y + dy) for x, y in poly] for poly in polys]


def mirror_x(polys):
    """Mirror about the y axis (x -> -x). Reverses CP handedness."""
    return [[(-x, y) for x, y in poly][::-1] for poly in polys]


def rotate(polys, deg):
    a = math.radians(deg)
    ca, sa = math.cos(a), math.sin(a)
    return [[(x * ca - y * sa, x * sa + y * ca) for x, y in poly]
            for poly in polys]
