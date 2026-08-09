"""
Transmission-line and patch-antenna synthesis for the 5.8 GHz MIMO radar board.

Everything here is closed-form textbook theory, implemented so that every
number on the board can be traced back to an equation rather than a guess:

  * Hammerstad & Jensen (1980) static microstrip Z0 / eps_eff, with the
    finite-copper-thickness correction.
  * Kirschning & Jansen (1982/84) dispersion, which matters a lot at 5.8 GHz.
  * Conductor-backed coplanar waveguide by conformal mapping (Wen / Ghione)
    for the SMA end-launch pads.
  * Balanis transmission-line model for the rectangular patch, including the
    mutual conductance between the two radiating slots.
"""

import math
from dataclasses import dataclass

C0 = 299792458.0          # m/s, exact
MU0 = 4e-7 * math.pi
ETA0 = 376.730313668


# ----------------------------------------------------------------------------
# Substrate
# ----------------------------------------------------------------------------

@dataclass
class Substrate:
    name: str
    er: float            # relative permittivity used for design
    h: float             # dielectric thickness, m
    t: float             # conductor thickness, m
    tand: float          # loss tangent
    sigma: float = 5.8e7  # copper conductivity, S/m

    def __repr__(self):
        return (f"{self.name}: er={self.er}, h={self.h*1e3:.3f} mm, "
                f"t={self.t*1e6:.0f} um, tand={self.tand}")


# Shengyi S7136H: a hydrocarbon-ceramic laminate made on ordinary FR-4 tooling,
# which is where the saving comes from -- Rogers needs a specialist process.
# Design permittivity 3.61 against RO4350B's 3.66, and a LOWER loss tangent,
# 0.0030 against 0.0037.  Available at 0.76 mm, which is the thickness this
# board is designed on.  (Shengyi datasheet S-USA-S7136H-DS-rv1.3-17.)
# ZYF300CA-P, the PTFE laminate JLCPCB stocks at 0.76 mm.  Chosen over Rogers
# RO4350B on every count that matters here: half the price, a loss tangent of
# 0.0018 against 0.0037, and more bandwidth -- which is this design's weakest
# number.  The only cost is that everything printed grows about ten per cent,
# because a lower permittivity shortens the wave less.
ZYF300CA = Substrate("ZYF300CA-P", er=3.00, h=0.76e-3, t=35e-6, tand=0.0018)
S7136H = Substrate("S7136H", er=3.61, h=0.76e-3, t=35e-6, tand=0.0030)
RO4350B = Substrate("RO4350B", er=3.66, h=0.762e-3, t=35e-6, tand=0.0037)
FR4_1MM = Substrate("FR-4 1.0mm", er=4.35, h=1.000e-3, t=35e-6, tand=0.022)


# ----------------------------------------------------------------------------
# Microstrip: static analysis (Hammerstad & Jensen)
# ----------------------------------------------------------------------------

def _hj_z0_air(u):
    """Characteristic impedance of an air-filled microstrip, H&J 1980."""
    fu = 6.0 + (2.0 * math.pi - 6.0) * math.exp(-((30.666 / u) ** 0.7528))
    return (ETA0 / (2.0 * math.pi)) * math.log(fu / u + math.sqrt(1.0 + (2.0 / u) ** 2))


def _hj_eeff(u, er):
    """Static effective permittivity, H&J 1980."""
    a = (1.0 + (1.0 / 49.0) * math.log((u ** 4 + (u / 52.0) ** 2) / (u ** 4 + 0.432))
         + (1.0 / 18.7) * math.log(1.0 + (u / 18.1) ** 3))
    b = 0.564 * ((er - 0.9) / (er + 3.0)) ** 0.053
    return (er + 1.0) / 2.0 + ((er - 1.0) / 2.0) * (1.0 + 10.0 / u) ** (-a * b)


def _thickness_correction(w, h, t, er):
    """Effective widths accounting for finite strip thickness (H&J)."""
    if t <= 0.0:
        return w, w
    u = w / h
    tn = t / h
    coth = 1.0 / math.tanh(math.sqrt(6.517 * u)) if u > 0 else 1.0
    dw_air = (tn / math.pi) * math.log(1.0 + (4.0 * math.e) / (tn * coth ** 2))
    dw_er = 0.5 * (1.0 + 1.0 / math.cosh(math.sqrt(er - 1.0))) * dw_air
    return (w + dw_air * h), (w + dw_er * h)


def ms_static(w, sub):
    """Static (DC-limit) Z0 and eps_eff of a microstrip of width w."""
    h, er, t = sub.h, sub.er, sub.t
    w_air, w_er = _thickness_correction(w, h, t, er)
    u_air, u_er = w_air / h, w_er / h
    z0_air_1 = _hj_z0_air(u_air)          # air-filled, thickness-corrected
    z0_air_2 = _hj_z0_air(u_er)
    ee = _hj_eeff(u_er, er)
    # H&J thickness correction: eps_eff is scaled by the ratio of the two
    # air-filled impedances, which captures the extra air-side fringing.
    ee_t = ee * (z0_air_2 / z0_air_1) ** 2
    z0 = z0_air_2 / math.sqrt(ee_t)
    return z0, ee_t


# ----------------------------------------------------------------------------
# Microstrip: dispersion (Kirschning & Jansen)
# ----------------------------------------------------------------------------

def ms_dispersive(w, sub, f):
    """Frequency-dependent Z0 and eps_eff (Kirschning & Jansen)."""
    h, er = sub.h, sub.er
    z0_s, ee_s = ms_static(w, sub)
    u = w / h
    fn = f * h * 1e-9 / 1e-3          # GHz*mm  (f in Hz, h in m)

    # --- eps_eff(f), K&J 1982 -------------------------------------------------
    P1 = (0.27488 + (0.6315 + 0.525 / (1.0 + 0.0157 * fn) ** 20) * u
          - 0.065683 * math.exp(-8.7513 * u))
    P2 = 0.33622 * (1.0 - math.exp(-0.03442 * er))
    P3 = 0.0363 * math.exp(-4.6 * u) * (1.0 - math.exp(-((fn / 38.7) ** 4.97)))
    P4 = 1.0 + 2.751 * (1.0 - math.exp(-((er / 15.916) ** 8)))
    Pf = P1 * P2 * ((0.1844 + P3 * P4) * fn) ** 1.5763
    ee_f = er - (er - ee_s) / (1.0 + Pf)

    # --- Z0(f), K&J 1984 ------------------------------------------------------
    R1 = 0.03891 * er ** 1.4
    R2 = 0.267 * u ** 7.0
    R3 = 4.766 * math.exp(-3.228 * u ** 0.641)
    R4 = 0.016 + (0.0514 * er) ** 4.524
    R5 = (fn / 28.843) ** 12.0
    R6 = 22.20 * u ** 1.92
    R7 = 1.206 - 0.3144 * math.exp(-R1) * (1.0 - math.exp(-R2))
    R8 = 1.0 + 1.275 * (1.0 - math.exp(-0.004625 * R3 * er ** 1.674
                                      * (fn / 18.365) ** 2.745))
    R9 = (5.086 * R4 * (R5 / (0.3838 + 0.386 * R4))
          * (math.exp(-R6) / (1.0 + 1.2992 * R5))
          * ((er - 1.0) ** 6 / (1.0 + 10.0 * (er - 1.0) ** 6)))
    R10 = 0.00044 * er ** 2.136 + 0.0184
    R11 = (fn / 19.47) ** 6 / (1.0 + 0.0962 * (fn / 19.47) ** 6)
    R12 = 1.0 / (1.0 + 0.00245 * u ** 2)
    R13 = 0.9408 * ee_f ** R8 - 0.9603
    R14 = (0.9408 - R9) * ee_s ** R8 - 0.9603
    R15 = 0.707 * R10 * (fn / 12.3) ** 1.097
    R16 = 1.0 + 0.0503 * er ** 2 * R11 * (1.0 - math.exp(-((u / 15.0) ** 6)))
    R17 = (R7 * (1.0 - 1.1241 * (R12 / R16)
                 * math.exp(-0.026 * fn ** 1.15656 - R15)))
    z0_f = z0_s * (R13 / R14) ** R17
    return z0_f, ee_f


def ms_width_for_z0(z0_target, sub, f):
    """Synthesise the strip width that gives z0_target at frequency f."""
    lo, hi = 1e-6, 60e-3
    for _ in range(200):
        mid = 0.5 * (lo + hi)
        z, _ = ms_dispersive(mid, sub, f)
        if z > z0_target:
            lo = mid          # narrower strip -> higher Z0, so go wider
        else:
            hi = mid
    w = 0.5 * (lo + hi)
    z, ee = ms_dispersive(w, sub, f)
    return w, z, ee


def ms_guided_wavelength(w, sub, f):
    _, ee = ms_dispersive(w, sub, f)
    return C0 / (f * math.sqrt(ee))


def ms_loss_db_per_m(w, sub, f):
    """Conductor + dielectric loss of a microstrip line."""
    z0, ee = ms_dispersive(w, sub, f)
    # Dielectric loss
    lam0 = C0 / f
    ad = (math.pi * sub.er * (ee - 1.0) * sub.tand
          / (lam0 * math.sqrt(ee) * (sub.er - 1.0)))       # Np/m
    # Conductor loss (Hammerstad / incremental inductance, simple form)
    rs = math.sqrt(math.pi * f * MU0 / sub.sigma)
    ac = rs / (z0 * w)                                      # Np/m
    return (ad + ac) * 8.685889638


# ----------------------------------------------------------------------------
# Conductor-backed CPW (for the SMA end-launch pads)
# ----------------------------------------------------------------------------

def _ellipk_ratio(k):
    """K(k)/K(k') using Hilberg's closed-form approximation (<1e-5 error)."""
    kp = math.sqrt(max(1.0 - k * k, 0.0))
    if k <= 1.0 / math.sqrt(2.0):
        return math.pi / math.log(2.0 * (1.0 + math.sqrt(kp)) / (1.0 - math.sqrt(kp)))
    return math.log(2.0 * (1.0 + math.sqrt(k)) / (1.0 - math.sqrt(k))) / math.pi


def cbcpw(w, s, sub):
    """Conductor-backed CPW: signal width w, slot width s, ground plane at h.

    Returns (Z0, eps_eff). Quasi-static conformal mapping (Ghione 1984).

    LIMITED VALIDITY -- read before using.  As the slot widens the structure
    becomes an ordinary microstrip, so this must converge on ms_dispersive();
    it does not.  Its air-side partial capacitance collapses to zero, eps_eff
    runs to the bulk permittivity, and on RO4350B 0.762 mm with a 1.611 mm
    strip it returns 61 ohm where the true answer is 50.  Trust it only for
    slots comparable with the substrate height, and check anything that
    matters against launch_z.py, which measures the cross-section directly in
    openEMS.  Measured there (see LAUNCH_MEASURED below), the 0.891 mm slot
    used on this board is 47.9 ohm, not the 50.0 this function claims.
    """
    h = sub.h
    a = w / 2.0
    b = w / 2.0 + s
    k1 = a / b
    r1 = _ellipk_ratio(k1)
    k3 = math.tanh(math.pi * a / (2.0 * h)) / math.tanh(math.pi * b / (2.0 * h))
    r3 = _ellipk_ratio(k3)
    q = r3 / (r1 + r3)
    ee = 1.0 + q * (sub.er - 1.0)
    z0 = (60.0 * math.pi / math.sqrt(ee)) / (r1 + r3)
    return z0, ee


# Characteristic impedance of the end-launch cross-section, measured in
# openEMS (sim/launch_z.py) on RO4350B 0.762 mm with a 1.611 mm strip and
# 5 mm coplanar grounds stitched at 1.27 mm.  The method reads 50.89 ohm for a
# plain microstrip whose true value is 50.00, so it carries about +0.9 ohm of
# bias; the trend is what matters.
LAUNCH_MEASURED = {          # slot width mm : Z0 ohm at 5.800 GHz
    None: 50.89,             # no coplanar ground at all (microstrip check)
    0.891: 47.86,
    1.500: 49.01,
    2.500: 49.46,
    4.000: 49.58,
}


def cbcpw_slot_for_z0(w, z0_target, sub):
    """Find the slot width that makes a CBCPW of given strip width hit z0."""
    lo, hi = 20e-6, 8e-3
    for _ in range(200):
        mid = 0.5 * (lo + hi)
        z, _ = cbcpw(w, mid, sub)
        if z < z0_target:
            lo = mid          # wider slot -> higher Z0
        else:
            hi = mid
    s = 0.5 * (lo + hi)
    return s, cbcpw(w, s, sub)[0]


# ----------------------------------------------------------------------------
# Rectangular patch (Balanis transmission-line model)
# ----------------------------------------------------------------------------

def _si(x):
    """Sine integral Si(x) by adaptive Simpson on sinc."""
    n = 2000
    hstep = x / n
    total = 1.0  # sinc(0)
    for i in range(1, n):
        xi = i * hstep
        val = math.sin(xi) / xi
        total += val * (4.0 if i % 2 else 2.0)
    total += math.sin(x) / x
    return total * hstep / 3.0


def patch_slot_conductance(W, lam0):
    """Radiation conductance of one radiating slot (Balanis 14-12)."""
    k0W = 2.0 * math.pi * W / lam0
    integ = (-2.0 + math.cos(k0W) + k0W * _si(k0W) + math.sin(k0W) / k0W)
    return integ / (120.0 * math.pi ** 2)


def patch_mutual_conductance(W, L, lam0, n=400):
    """Mutual conductance between the two radiating slots (Balanis 14-18a)."""
    k0 = 2.0 * math.pi / lam0
    total = 0.0
    for i in range(n + 1):
        th = math.pi * i / n
        st = math.sin(th)
        if st < 1e-9:
            f = 0.0
        else:
            num = math.sin(k0 * W / 2.0 * math.cos(th))
            f = (num / math.cos(th)) ** 2 * _j0(k0 * L * st) * st ** 3
        wgt = 1.0 if i in (0, n) else (4.0 if i % 2 else 2.0)
        total += wgt * f
    total *= (math.pi / n) / 3.0
    return total / (120.0 * math.pi ** 2)


def _j0(x):
    """Bessel J0 via series / asymptotic split."""
    ax = abs(x)
    if ax < 8.0:
        y = x * x
        p1 = (-2957821389.0 + y * (7062834065.0 + y * (-512359803.6
              + y * (10879881.29 + y * (-86327.92757 + y * 228.4622733)))))
        p2 = (40076544269.0 + y * (745249964.8 + y * (7189466.438
              + y * (47447.26470 + y * (226.1030244 + y)))))
        return p1 / p2
    z = 8.0 / ax
    y = z * z
    xx = ax - 0.785398164
    p1 = (1.0 + y * (-0.1098628627e-2 + y * (0.2734510407e-4
          + y * (-0.2073370639e-5 + y * 0.2093887211e-6))))
    p2 = (-0.1562499995e-1 + y * (0.1430488765e-3 + y * (-0.6911147651e-5
          + y * (0.7621095161e-6 + y * (-0.934935152e-7)))))
    return math.sqrt(0.636619772 / ax) * (math.cos(xx) * p1 - z * math.sin(xx) * p2)


def patch_delta_L(W, h, eeff):
    """Fringing extension of one open end (Hammerstad)."""
    return 0.412 * h * ((eeff + 0.3) * (W / h + 0.264)
                        / ((eeff - 0.258) * (W / h + 0.8)))


def square_patch(sub, f):
    """Synthesise a SQUARE patch resonant at f. Returns a dict of results.

    A square patch is used because both orthogonal modes must resonate at the
    same frequency for circular polarisation driven by a 90-degree hybrid.
    """
    lam0 = C0 / f
    L = lam0 / (2.0 * math.sqrt((sub.er + 1.0) / 2.0))   # first guess
    for _ in range(300):
        W = L                                            # square
        _, eeff = ms_dispersive(W, sub, f)               # wide-strip eps_eff
        dL = patch_delta_L(W, sub.h, eeff)
        L_new = C0 / (2.0 * f * math.sqrt(eeff)) - 2.0 * dL
        if abs(L_new - L) < 1e-10:
            L = L_new
            break
        L = 0.5 * (L + L_new)
    W = L
    _, eeff = ms_dispersive(W, sub, f)
    dL = patch_delta_L(W, sub.h, eeff)
    G1 = patch_slot_conductance(W, lam0)
    G12 = patch_mutual_conductance(W, L, lam0)
    Rin_edge = 1.0 / (2.0 * (G1 + G12))                  # odd (TM10) mode
    return {
        "L": L, "W": W, "eeff": eeff, "dL": dL,
        "Leff": L + 2.0 * dL, "G1": G1, "G12": G12,
        "Rin_edge": Rin_edge, "lam0": lam0,
    }


def patch_bandwidth_estimate(sub, f, W, L):
    """Rough VSWR<2 fractional bandwidth (Jackson & Alexopoulos style)."""
    lam0 = C0 / f
    return 3.77 * ((sub.er - 1.0) / sub.er ** 2) * (sub.h / lam0) * (W / L)


# ----------------------------------------------------------------------------
# Branch-line (quadrature) hybrid -- exact 4-port from cascaded TL sections
# ----------------------------------------------------------------------------

def branchline_sparams(z_series, z_shunt, theta, z0=50.0):
    """S-parameters of a branch-line ring built from four TL arms.

    Node/port numbering (physical layout used on this board):

            P4 o----[ series, z_series, theta ]----o P3
               |                                  |
          shunt, z_shunt, theta            shunt, z_shunt, theta
               |                                  |
            P1 o----[ series, z_series, theta ]----o P2

    Solved by nodal analysis with each arm's 2x2 Y-matrix.

    NOTE ON VOCABULARY: "series" here means the pair of arms joining P1-P2 and
    P4-P3, whatever impedance they carry.  element2.py draws the ring rotated,
    so the arms it calls "horizontal" are these ones.  The two files describe
    the same ring; only the naming differs.
    """
    import numpy as np

    # Y-matrix of a lossless transmission-line arm.
    def yarm(zc, th):
        return np.array([[-1j / (zc * math.tan(th)), 1j / (zc * math.sin(th))],
                         [1j / (zc * math.sin(th)), -1j / (zc * math.tan(th))]],
                        dtype=complex)

    Y = np.zeros((4, 4), dtype=complex)
    arms = [(0, 1, z_series), (3, 2, z_series), (0, 3, z_shunt), (1, 2, z_shunt)]
    for a, b, zc in arms:
        y = yarm(zc, theta)
        Y[a, a] += y[0, 0]; Y[a, b] += y[0, 1]
        Y[b, a] += y[1, 0]; Y[b, b] += y[1, 1]
    Y0 = np.eye(4) / z0
    return (Y0 - Y) @ np.linalg.inv(Y0 + Y)


# ---------------------------------------------------------------- coupled lines
# Edge-coupled microstrip, for the directional couplers the leakage canceller
# needs: one to sample the transmit signal, one to inject the correction into
# the receive path.  Even/odd mode impedances from Hammerstad-Jensen with the
# Kirschning-Jansen coupled-line corrections (Hammerstad & Jensen 1980;
# Kirschning & Jansen, MTT-32, 1984).
def coupled_ms(w, s, sub, f=5.8e9):
    """Even and odd mode impedance and effective permittivity of a coupled pair."""
    h, er = sub.h, sub.er
    u, g = w / h, s / h

    # single-line reference
    z0s, ee_s = ms_static(w, sub)

    # even mode: the gap barely loads it, the pair looks like a wider line
    v = u * (20.0 + g ** 2) / (10.0 + g ** 2) + g * math.exp(-g)
    ae = 1.0 + math.log((v ** 4 + (v / 52.0) ** 2) / (v ** 4 + 0.432)) / 49.0 \
         + math.log(1.0 + (v / 18.1) ** 3) / 18.7
    be = 0.564 * ((er - 0.9) / (er + 3.0)) ** 0.053
    fe = (1.0 + 10.0 / v) ** (-ae * be)
    ee_e = (er + 1.0) / 2.0 + (er - 1.0) / 2.0 * fe

    # odd mode: the fields crowd into the gap, so more of them sit in air
    d = 0.593 + 0.694 * math.exp(-0.562 * u)
    aoo = 0.7287 * (ee_s - (er + 1.0) / 2.0) * (1.0 - math.exp(-0.179 * u))
    boo = 0.747 * er / (0.15 + er)
    coo = boo - (boo - 0.207) * math.exp(-0.414 * u)
    doo = 0.593 + 0.694 * math.exp(-0.562 * u)
    ee_o = ((er + 1.0) / 2.0 + aoo - ee_s) * math.exp(-coo * g ** doo) + ee_s

    # mode impedances, via the air-filled line impedances
    z0_air = z0s * math.sqrt(ee_s)
    q1 = 0.8695 * u ** 0.194
    q2 = 1.0 + 0.7519 * g + 0.189 * g ** 2.31
    q3 = 0.1975 + (16.6 + (8.4 / g) ** 6) ** -0.387 \
         + math.log(g ** 10 / (1.0 + (g / 3.4) ** 10)) / 241.0
    q4 = 2.0 * q1 / q2 / (math.exp(-g) * u ** q3 + (2.0 - math.exp(-g)) * u ** -q3)
    ze_air = z0_air / (1.0 - z0_air * q4 / 377.0)

    q5 = 1.794 + 1.14 * math.log(1.0 + 0.638 / (g + 0.517 * g ** 2.43))
    q6 = 0.2305 + math.log(g ** 10 / (1.0 + (g / 5.8) ** 10)) / 281.3 \
         + math.log(1.0 + 0.598 * g ** 1.154) / 5.1
    q7 = (10.0 + 190.0 * g ** 2) / (1.0 + 82.3 * g ** 3)
    q8 = math.exp(-6.5 - 0.95 * math.log(g) - (g / 0.15) ** 5)
    q9 = math.log(q7) * (q8 + 1.0 / 16.5)
    q10 = (q2 * q4 - q5 * math.exp(math.log(u) * q6 * u ** -q9)) / q2
    zo_air = z0_air / (1.0 - z0_air * q10 / 377.0)

    return (ze_air / math.sqrt(ee_e), zo_air / math.sqrt(ee_o), ee_e, ee_o)


def coupler_modes(z0, c_db):
    """Even/odd impedances a coupler of this coupling factor must present."""
    c = 10.0 ** (-abs(c_db) / 20.0)
    return z0 * math.sqrt((1.0 + c) / (1.0 - c)), z0 * math.sqrt((1.0 - c) / (1.0 + c))


def coupler_synth(c_db, sub, z0=50.0, f=5.8e9):
    """Width and gap of an edge-coupled coupler of the wanted coupling."""
    ze_t, zo_t = coupler_modes(z0, c_db)
    best, w0 = None, ms_width_for_z0(z0, sub, f)[0]
    for w in [w0 * k for k in [x / 400.0 for x in range(120, 520)]]:
        lo, hi = 0.03e-3, 6.0e-3
        for _ in range(60):                       # gap that hits the even mode
            s = 0.5 * (lo + hi)
            ze, zo, _, _ = coupled_ms(w, s, sub, f)
            if ze > ze_t:
                lo = s        # too tight: open the gap to weaken the coupling
            else:
                hi = s
        ze, zo, ee_e, ee_o = coupled_ms(w, s, sub, f)
        err = abs(ze - ze_t) / ze_t + abs(zo - zo_t) / zo_t
        if best is None or err < best[0]:
            best = (err, w, s, ze, zo, ee_e, ee_o)
    _, w, s, ze, zo, ee_e, ee_o = best
    ee = ((ee_e + ee_o) / 2.0)
    lam = C0 / f / math.sqrt(ee)
    got = 20.0 * math.log10((ze - zo) / (ze + zo))
    return dict(w_mm=w * 1e3, s_mm=s * 1e3, len_mm=lam / 4.0 * 1e3,
                ze=ze, zo=zo, z0_eff=math.sqrt(ze * zo), coupling_db=got)


# ------------------------------------------------------ single-feed CP patch
# A square patch with two opposite corners clipped resonates at two slightly
# different frequencies along its two diagonals.  Driven between them, on the
# axis halfway round, the two responses come out equal in size and a quarter
# cycle apart, which is circular polarisation from ONE feed and no coupler.
# The clip is sized from the patch's own sharpness: too small and the two
# responses do not separate, too big and they separate too far.
# (Sharma & Gupta, MTT-31 1983; Balanis 4th ed. section 14.2.2.)
def patch_q(sub, L, f=5.8e9):
    """How sharply a patch resonates: radiation, copper and dielectric all in."""
    er_eff = ms_static(L, sub)[1]
    lam0 = C0 / f
    h = sub.h
    # radiation Q of a half-wave patch over a ground plane
    q_rad = (C0 * math.sqrt(er_eff)) / (4.0 * f * h) / 1.0
    # conductor and dielectric, from the usual loss expressions
    q_c = h * math.sqrt(math.pi * f * 4e-7 * math.pi * 5.8e7)
    q_d = 1.0 / sub.tand
    return 1.0 / (1.0 / q_rad + 1.0 / q_c + 1.0 / q_d), q_rad, q_c, q_d


def truncation_for_cp(sub, L, f=5.8e9):
    """Corner clip that puts the two responses a quarter cycle apart."""
    q, q_rad, q_c, q_d = patch_q(sub, L, f)
    # the classic condition: clipped area over total area = 1 / (2 Q)
    ratio = 1.0 / (2.0 * q)
    c = L * math.sqrt(ratio)                      # leg of each clipped corner
    return dict(q=q, q_rad=q_rad, q_c=q_c, q_d=q_d, area_ratio=ratio,
                c_mm=c * 1e3, c_over_L=c / L,
                split_frac=1.0 / (2.0 * q))


# --------------------------------------------------------------------- stripline
# A line buried between two ground planes.  The front-end board needs one to
# carry the reference under the receive chains; it is not microstrip and does
# not take microstrip's width.  (Cohn 1954; Wheeler 1978; IPC-2141A.)
def stripline_z0(w, b, t, er):
    """Impedance of a centred strip of width w in a ground spacing b."""
    m = 6.0 / (3.0 + 2.0 * t / (b - t))
    we = w + (t / math.pi) * (1.0 - 0.5 * math.log(
        (t / b) ** 2 + (1.0 / math.pi / (w / t + 1.1)) ** m))
    return 30.0 * math.pi / math.sqrt(er) * (b - t) / (we + 0.441 * b)


def stripline_loss_db_per_m(w, b, t, er, tand, f):
    """What a buried line costs per metre: the dielectric plus the copper."""
    lam0 = C0 / f
    # dielectric: set only by the material, not the geometry
    a_d = 27.3 * math.sqrt(er) * tand / lam0
    # copper: skin depth over the current-carrying perimeter
    rs = math.sqrt(math.pi * f * 4e-7 * math.pi / 5.8e7)
    z0 = stripline_z0(w, b, t, er)
    a_c = 8.686 * rs / (z0 * (b - t)) * (1.0 + 2.0 * w / (b - t)) / \
        (2.0 * math.pi) * 2.0
    return a_d + a_c


def stripline_width(z0, b, t, er):
    lo, hi = 1e-6, 20e-3
    for _ in range(200):
        mid = 0.5 * (lo + hi)
        if stripline_z0(mid, b, t, er) > z0:
            lo = mid
        else:
            hi = mid
    return 0.5 * (lo + hi)
