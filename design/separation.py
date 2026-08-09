"""How far apart the two arrays need to be, from two measurements.

Coupling between two antennas on a grounded board arrives by two routes.  One
goes through the air and thins out as the square of the distance, the way any
radiated signal does.  The other is bound to the board itself, spreading
sideways through the substrate like a ripple on a pond, and thins out only as
the first power of the distance -- so at short range it dominates, and it is
the reason a compact board isolates so much worse than free space suggests.

Two openEMS runs separate them: one with the board whole, one with a slot cut
right through it.  The slot removes the bound route and leaves the air route
alone, so the difference between the two measurements is the bound route's
share.  With the two apart, the air route can be extrapolated honestly to any
separation, which is what decides how far the receive array has to sit.
"""

import math

C0 = 299792458.0
F0 = 5.80e9
LAM = C0 / F0

R_NOW = 0.0921          # centre-to-centre now: transmit y=31 to receive y=122.16


# MEASURED, and it refutes the idea the model was built on.  Cutting the board
# did not remove coupling, it added it: the two new edges radiate more than the
# path through the substrate was carrying.  So the substrate route was never
# the dominant one at this spacing, and a small gap is worse than none.
#
#   worst in band, one board  -42.17 dB
#   worst in band, 12 mm gap  -40.33 dB   <- 1.8 dB WORSE
#
# What survives is the part that was never in doubt: pull the two arrays
# properly apart and the free-space path thins as the square of the distance.
# The baseline for that is the whole board, not the cut one.
MEAS_WHOLE = -42.17
MEAS_CUT12 = -40.33


def split(total_db, air_only_db):
    """Share of the coupling that travels through the board, not the air."""
    p_tot = 10 ** (total_db / 10.0)
    p_air = 10 ** (air_only_db / 10.0)
    p_sub = max(p_tot - p_air, 1e-30)
    return (10 * math.log10(p_sub), 100.0 * p_sub / p_tot)


def predict(air_only_db, r_new, r_ref=R_NOW):
    """The air route at a new separation: it thins as the square of distance."""
    return air_only_db - 20.0 * math.log10(r_new / r_ref)


def table(total_db, air_only_db, need_db):
    sub_db, pct = split(total_db, air_only_db)
    print(f"  board whole:      {total_db:6.2f} dB")
    print(f"  board cut:        {air_only_db:6.2f} dB   <- the air route alone")
    print(f"  through the board:{sub_db:6.2f} dB   ({pct:.0f} % of the coupling)")
    print(f"  cutting the board is worth "
          f"{air_only_db - total_db:+.1f} dB on its own\n")
    print(f"  {'separation':>12} {'isolation':>11} {'far field beyond':>18}   verdict")
    for r in (0.012 + R_NOW, 0.100, 0.200, 0.300, 0.500, 0.750):
        iso = predict(air_only_db, r)
        d = r + LAM / 2
        ff = 2 * d * d / LAM
        ok = "enough for the full watt" if iso <= need_db else \
             f"{need_db - iso:+.0f} dB short"
        if ff > 30.0:
            ok += "; far field too distant"
        print(f"  {r*1000:9.0f} mm {iso:9.1f} dB {ff:15.1f} m   {ok}")


if __name__ == "__main__":
    import sys
    if len(sys.argv) > 2:
        table(float(sys.argv[1]), float(sys.argv[2]),
              float(sys.argv[3]) if len(sys.argv) > 3 else -57.0)
    else:
        print("waiting on the cut measurement")
