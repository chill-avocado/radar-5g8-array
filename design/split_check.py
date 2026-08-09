"""Does moving the receive array away spoil the angle measurement?

The claim the split architecture rests on: in a MIMO radar the direction of a
target is read from the differences between the virtual elements, and a virtual
element sits at a transmit position plus a receive position.  Move every
receive element by the same amount and every virtual element moves by that same
amount, so the differences -- and therefore the angles -- do not change.

That is an argument.  This is a measurement: form the actual beam pattern and
estimate the angle of a simulated target, with the arrays together and with
them pulled apart, and compare.
"""

import math

import numpy as np

C0 = 299792458.0
F0 = 5.80e9
LAM = C0 / F0
K = 2 * math.pi / LAM
PITCH = LAM / 2

# as built: transmit pair spaced in x, receive pair spaced in y
TX = np.array([[0.0, 0.0], [PITCH, 0.0]])
RX0 = np.array([[0.0, 0.0], [0.0, PITCH]])


def steer(tx, rx, az, el):
    """Phase each transmit-receive pair sees from a direction."""
    u = np.array([math.sin(az) * math.cos(el), math.sin(el)])
    return np.exp(1j * K * ((tx @ u)[:, None] + (rx @ u)[None, :])).ravel()


def estimate(tx, rx, az_t, el_t, snr_db=20.0, seed=0):
    """Beamform a noisy snapshot and read back the direction."""
    rng = np.random.default_rng(seed)
    sig = steer(tx, rx, az_t, el_t)
    n = (rng.normal(size=sig.size) + 1j * rng.normal(size=sig.size))
    sig = sig + n * 10 ** (-snr_db / 20.0) / math.sqrt(2)
    grid = np.radians(np.arange(-60, 60.05, 0.05))
    best, ba, be = -1, 0, 0
    for el in grid[::20]:
        w = np.array([steer(tx, rx, a, el) for a in grid])
        p = np.abs(w.conj() @ sig) ** 2
        i = int(np.argmax(p))
        if p[i] > best:
            best, ba, be = p[i], grid[i], el
    return math.degrees(ba), math.degrees(be)


print("Virtual element positions, in wavelengths\n")
for lab, off in (("arrays together (one board)", 0.0),
                 ("receive array 100 mm away", 0.100),
                 ("receive array 500 mm away", 0.500)):
    rx = RX0 + np.array([0.0, off])
    virt = np.array([t + r for t in TX for r in rx]) / LAM
    rel = virt - virt.mean(axis=0)
    print(f"  {lab:28} centres {np.round(virt[:, 1], 3)} ...")
    print(f"  {'':28} about their own centre "
          f"{np.round(rel, 4).tolist()}")

print("\nAngle read back from a target at azimuth +17.0, elevation -11.0 deg\n")
print(f"  {'':28} {'azimuth':>9} {'elevation':>10} {'error':>9}")
for lab, off in (("arrays together (one board)", 0.0),
                 ("receive array 12 mm away", 0.012),
                 ("receive array 100 mm away", 0.100),
                 ("receive array 500 mm away", 0.500),
                 ("receive array 2 m away", 2.000)):
    rx = RX0 + np.array([0.0, off])
    a, e = estimate(TX, rx, math.radians(17.0), math.radians(-11.0))
    err = math.hypot(a - 17.0, e + 11.0)
    print(f"  {lab:28} {a:8.2f}  {e:9.2f}  {err:8.3f} deg")

print("\nWhere the far field starts, so the two arrays act as one aperture:")
for off in (0.012, 0.100, 0.500, 2.000):
    d = off + PITCH
    print(f"  arrays {off*1000:6.0f} mm apart -> aperture {d*1000:6.1f} mm, "
          f"far field beyond {2*d*d/LAM:6.2f} m")
