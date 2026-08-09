"""The receive amplifier board: two channels, and a radio that cannot be hurt.

The radar's receiver is a USRP B210.  It starts to compress at fifteen
microwatts and it is damaged at a milliwatt, and there are three ways to reach
those levels: our own transmitter leaking across from the other array, a drone
holding a watt-class video transmitter on our own frequency a metre away, and
somebody putting the wrong cable in the wrong socket.  This board sits between
the receive antennas and the radio, makes the radar four tenths more sensitive
than the radio alone, and guarantees that none of those three can get through
to it.

Each chain is:

    connector -> limiter -> low-noise amplifier -> sampling coupler
              -> level pad -> clamp -> connector

The limiter protects the amplifier.  The clamp protects the radio.  The level
pad decides how much gain the radar gets to keep, and the coupler both reports
what the radio is being fed and gives a leakage canceller somewhere to inject.

Everything about the supply, the monitoring and the way the board is drawn is
the same as the transmit amplifier board next door -- same laminate, same
stack, same connectors, same twelve volts, same ten-way monitoring header --
so the pair are one instrument rather than two projects.
"""

import json
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import rfmath as rf                                            # noqa: E402
from geom import rect                                          # noqa: E402
import pa_board as PA                                          # noqa: E402
from pa_board import (SUB, W50, LOSS_MM, F0, F_LO, F_HI, e24, pi_pad,   # noqa
                      _sot23, _sot23_6, _sot223, _chip, R2512, R1210,
                      via_theta, spread_theta, separation_for)

BW, BH = 92.0, 68.0
Y_A, Y_B = 58.0, 10.0                  # the two chains
PULLBACK = 0.15
MOUNT_D, MOUNT_PAD = 3.20, 6.00
LAUNCH_GAP = PA.LAUNCH_GAP
STACK = PA.STACK
W_DC = PA.W_DC


# ---------------------------------------------------------------- real parts
# Every pin of every part is from its datasheet.
#
#   PMA3-83LN+     Mini-Circuits, 0.5-8 GHz gallium arsenide low-noise
#                  amplifier in the SAME 3x3 mm package as the transmit
#                  board's final, matched to fifty ohms inside, supply on the
#                  output pin.  1.5 dB of noise at 5 GHz.
#   SMP1330-085LF  Skyworks silicon limiter diode.  A diode that is invisible
#                  until something large arrives and then turns itself into a
#                  short circuit.  It needs no supply and no control: the
#                  signal itself drives it.
#   SMS7630-079LF  the same detector diode the transmit board uses, here in
#                  pairs as the clamp that decides what the radio may see.
def _interp(f_ghz, lo_f, lo_v, hi_f, hi_v):
    return lo_v + (f_ghz - lo_f) / (hi_f - lo_f) * (hi_v - lo_v)


F_GHZ = F0 / 1e9
LNA = dict(
    mpn="PMA3-83LN+", mfr="Mini-Circuits", pkg="QFN12", body=3.0, pitch=0.51,
    pad_w=0.25, pad_l=0.51, land_half=1.355, ep=1.25, n_side=3,
    # the 5 V column of the datasheet: 5 V is what the transmit board's
    # regulator already makes, and 6 V buys seven hundredths of a decibel
    gain_db=_interp(F_GHZ, 5.0, 20.5, 8.0, 18.7),
    gain_min_db=_interp(F_GHZ, 5.0, 18.7, 8.0, 18.7),
    nf_db=_interp(F_GHZ, 5.0, 1.6, 8.0, 2.2),
    p1db_dbm=_interp(F_GHZ, 5.0, 18.9, 8.0, 17.3),
    oip3_dbm=_interp(F_GHZ, 5.0, 29.7, 8.0, 26.2),
    in_rl_db=_interp(F_GHZ, 5.0, 12.4, 8.0, 6.3),
    out_rl_db=_interp(F_GHZ, 5.0, 18.4, 8.0, 12.6),
    vdd=5.0, idd_ma=60.0, idd_max_ma=94.0,
    pdiss_max_w=0.95, theta_jc=47.0, t_lead_max=105.0, t_die_max=150.0,
    pin_max_dbm=16.0, pin_max_5min_dbm=19.0,
    pins={1: "GND", 2: "RFIN", 3: "GND", 4: "GND", 5: "GND", 6: "GND",
          7: "GND", 8: "RFOUT", 9: "GND", 10: "GND", 11: "GND", 12: "GND",
          13: "GND"},
    note="pins 1 and 3-12 are unused inside the package and are grounded on "
         "the maker's own test board")
LIMITER = dict(
    mpn="SMP1330-085LF", mfr="Skyworks", pkg="QFN 3-pin 2x2",
    c_pf=1.0, rs_ohm=2.0, threshold_dbm=10.0, il_db=0.3,
    atten_at_20_db=8.8, atten_at_30_db=14.0,
    p_cw_w=3.0, theta_jc=40.0,
    pins={1: "A", 2: "K", 3: "A"},
    note="pin 1 and pin 3 are both the anode and sit in the line; pin 2 and "
         "the paddle are the cathode and go to ground")
CLAMP = dict(mpn="SMS7630-079LF", mfr="Skyworks", pkg="SOD-523",
             c_pf=0.14, v_on=0.24,
             note="two of them back to back; whichever way the wave swings, "
                  "one of them is already conducting")


# --------------------------------------------------------------- the chain
L_LAUNCH = 0.05
L_BLOCK = 0.05
L_COUPLER = 0.14                        # a -15 dB tap costs this by maths
L_CLAMP = 0.05
PAD_DEFAULT_DB = 8.0
CABLE_DB = 0.50                         # the jumper from the receive array
B210_NF_DB = 8.0
B210_P1DB_DBM = -15.0
B210_MAX_DBM = 0.0
R_SENSE = 1.00                          # ohm; sixty milliamps is a small
G_SENSE = 50.0                          # current, so the shunt is larger
TRIP_MA = 120.0


def limiter(c_pf=None, f=F0, sub=SUB, z0=50.0, fc_ratio=2.0):
    """A diode that is invisible until it is needed, on a line that lets it be.

    A limiter diode is a capacitor when it is doing nothing, and at 5.8 GHz
    one picofarad is a quarter of the line's own impedance -- put straight
    across a fifty-ohm line it would reflect half the signal before anything
    ever went wrong.  So the line is not fifty ohms where the diode sits.

    Dropping the local impedance raises the frequency at which that
    capacitance starts to matter, in exact proportion: the diode plus a short
    length of thin line either side of it behaves as one section of a low-pass
    filter whose corner is 1/(pi Z C).  At twenty-seven ohms the corner lands
    near twelve gigahertz and 5.8 GHz passes through it almost untouched.  A
    quarter-wave transformer at each end takes the line down to that impedance
    and back up again.

    The diode also needs somewhere for its own rectified current to go, or it
    cannot limit at all.  That is a printed quarter-wave line ending in a via:
    a dead short at direct current, an open circuit at 5.8 GHz.
    """
    c = (LIMITER["c_pf"] if c_pf is None else c_pf) * 1e-12
    z_sec = 1.0 / (math.pi * fc_ratio * f * c)
    z_tr = math.sqrt(z0 * z_sec)
    w_sec = rf.ms_width_for_z0(z_sec, sub, f)[0]
    w_tr = rf.ms_width_for_z0(z_tr, sub, f)[0]
    lam_tr = rf.ms_guided_wavelength(w_tr, sub, f)
    # the series inductance that pairs with the diode, half on each side
    l_half = z_sec ** 2 * c / 2.0
    z_hi = 128.0
    w_hi = rf.ms_width_for_z0(z_hi, sub, f)[0]
    _, ee_hi = rf.ms_dispersive(w_hi, sub, f)
    l_per_m = z_hi * math.sqrt(ee_hi) / 299792458.0
    len_half = l_half / l_per_m
    lam_hi = rf.ms_guided_wavelength(w_hi, sub, f)
    # how hard it clamps: the conducting diode across the section
    att = 20.0 * math.log10(1.0 + z_sec / (2.0 * LIMITER["rs_ohm"]))
    return dict(z_sec=z_sec, z_tr=z_tr, fc_hz=1.0 / (math.pi * z_sec * c),
                w_sec_mm=w_sec * 1e3, w_tr_mm=w_tr * 1e3,
                len_tr_mm=lam_tr / 4.0 * 1e3,
                w_hi_mm=w_hi * 1e3, len_half_mm=len_half * 1e3,
                stub_w_mm=w_hi * 1e3, stub_len_mm=lam_hi / 4.0 * 1e3,
                atten_db=att, c_pf=c * 1e12)


def clamp(c_pf=None, f=F0, sub=SUB, z0=50.0):
    """The last thing between the amplifier and the radio.

    Two small diodes facing opposite ways, from the line to ground.  Below a
    quarter of a volt they are not there; above it, whichever way the wave is
    swinging, one of them is already conducting and the voltage cannot grow.
    A quarter of a volt across fifty ohms is about a milliwatt, so nothing
    beyond a milliwatt leaves this board however the amplifier misbehaves --
    and the level pad after it divides even that.

    Their capacitance is a seventh of the limiter's, so it needs only a
    fraction of a millimetre of thin line either side to disappear.
    """
    c = 2.0 * ((CLAMP["c_pf"] if c_pf is None else c_pf) * 1e-12)
    fc = 1.0 / (math.pi * z0 * c)
    z_hi = 128.0
    w_hi = rf.ms_width_for_z0(z_hi, sub, f)[0]
    _, ee_hi = rf.ms_dispersive(w_hi, sub, f)
    l_per_m = z_hi * math.sqrt(ee_hi) / 299792458.0
    len_half = (z0 ** 2 * c / 2.0) / l_per_m
    p_clamp = (CLAMP["v_on"] / math.sqrt(2.0)) ** 2 / z0
    return dict(c_pf=c * 1e12, fc_hz=fc, w_hi_mm=w_hi * 1e3,
                len_half_mm=len_half * 1e3,
                clamp_dbm=10.0 * math.log10(p_clamp * 1e3))


LIM = limiter()
CLP = clamp()
BIAS = PA.bias_tee()
TAP = rf.coupler_synth(15.0, SUB)      # reports the level, injects a fix
PAD_SHUNT, PAD_SERIES = pi_pad(PAD_DEFAULT_DB)


def chain(pad_db=PAD_DEFAULT_DB, trace_in_mm=34.0, trace_out_mm=30.0):
    """Connector to connector, and what the radio ends up seeing."""
    l_in = (L_LAUNCH + trace_in_mm * LOSS_MM + 2 * L_BLOCK + LIMITER["il_db"])
    l_out = (L_BLOCK + trace_out_mm * LOSS_MM + L_COUPLER + pad_db + L_CLAMP
             + L_LAUNCH)
    gain = LNA["gain_db"] - l_in - l_out
    return dict(pad_db=pad_db, loss_in_db=l_in, loss_out_db=l_out,
                lna_gain_db=LNA["gain_db"], gain_db=gain,
                lna_in_p1db_dbm=LNA["p1db_dbm"] - LNA["gain_db"],
                out_p1db_dbm=LNA["p1db_dbm"] - l_out)


def noise(pad_db=PAD_DEFAULT_DB, cable_db=CABLE_DB):
    """System noise figure, and what it is worth.

    Everything in front of the amplifier costs its own loss twice over -- once
    as loss and once as noise -- which is why the limiter is designed the way
    it is and why the board goes as close to the antennas as the cable allows.
    Everything after the amplifier is divided by the amplifier's gain, which
    is why the level pad can be generous.
    """
    c = chain(pad_db)
    f_pre = 10 ** ((cable_db + L_LAUNCH + 2 * L_BLOCK + LIMITER["il_db"]
                    + 34.0 * LOSS_MM) / 10.0)
    f_lna = 10 ** (LNA["nf_db"] / 10.0)
    g_lna = 10 ** (LNA["gain_db"] / 10.0)
    f_post = 10 ** ((c["loss_out_db"]) / 10.0) * 10 ** (B210_NF_DB / 10.0)
    f = f_pre * (f_lna + (f_post - 1.0) / g_lna)
    f_none = 10 ** ((cable_db + B210_NF_DB) / 10.0)
    return dict(nf_db=10 * math.log10(f), nf_none_db=10 * math.log10(f_none),
                range_ratio=(f_none / f) ** 0.25,
                pre_db=10 * math.log10(f_pre))


def threats(pad_db=PAD_DEFAULT_DB, pt_dbm=None, sep_m=0.624):
    """Every way the radio can be hurt, and what this board does about it.

    The numbers for a drone's own transmitter are the ones already measured
    for the array board: a watt-class video transmitter at these ranges, into
    one of its patch antennas.
    """
    pt = (PA.chain()["out_connector_dbm"] - 0.5) if pt_dbm is None else pt_dbm
    c = chain(pad_db)
    iso = -42.17 - 20 * math.log10(sep_m / 0.0921)
    out = []

    def add(what, at_input):
        after_lim = min(at_input, at_input - max(
            0.0, (at_input - LIMITER["threshold_dbm"])
            * LIM["atten_db"] / 20.0))
        at_lna_out = min(after_lim + LNA["gain_db"], LNA["p1db_dbm"] + 3.0)
        at_radio = min(at_lna_out - c["loss_out_db"],
                       CLP["clamp_dbm"] - pad_db - L_CLAMP - L_LAUNCH)
        out.append(dict(what=what, at_input=at_input, at_lna=after_lim,
                        at_radio=at_radio,
                        damages=at_radio > B210_MAX_DBM,
                        compresses=at_radio > B210_P1DB_DBM,
                        hurts_lna=after_lim > LNA["pin_max_dbm"]))

    add(f"our own transmitter, arrays {sep_m*1000:.0f} mm apart", pt + iso)
    for r, p in ((1.0, 30.0), (0.3, 30.0), (0.3, 33.0)):
        lam = 299792458.0 / F0
        at = p + PA.FINAL["gain_db"] * 0 + 6.1 + 20 * math.log10(
            lam / (4 * math.pi * r))
        add(f"a {10**(p/10)/1000:.0f} W drone at {r:.1f} m", at)
    add("the transmit amplifier plugged in by mistake", pt)
    return out


def thermal(n_under, n_ring, ambient_c=25.0):
    """The amplifier runs on a third of a watt, so this is easy -- but it is
    the same two-stage path the transmit board uses, worked the same way."""
    p = LNA["vdd"] * LNA["idd_ma"] / 1000.0
    h1 = STACK["dielectric_mm"][0]
    h2 = sum(STACK["dielectric_mm"][1:])
    r_top = 1.0 / (1.0 / via_theta(0.30, 25.0, h1, n_under)
                   + 1.0 / (spread_theta(0.70, 2.40, SUB.t * 1e6)
                            + via_theta(0.30, 25.0, h1, n_ring)))
    r_deep = via_theta(0.30, 25.0, h2, n_under + n_ring)
    r_board = r_top + r_deep + 1.2
    t_lead = ambient_c + p * r_board
    return dict(p_w=p, r_board=r_board, t_lead_c=t_lead,
                t_die_c=t_lead + p * LNA["theta_jc"],
                lead_margin_c=LNA["t_lead_max"] - t_lead,
                die_margin_c=LNA["t_die_max"] - (t_lead + p * LNA["theta_jc"]))


# ======================================================================= board
def _smp1330(net_in, net_out):
    """The limiter diode's land, from the datasheet's own footprint drawing.

    Two small anode pads in the line and a long cathode paddle between them
    that goes straight to ground through its own holes.  The anodes are a
    quarter of a millimetre tall, so the line necks down to reach them -- and
    that neck is not a compromise, it is the series inductance the design
    wants either side of the diode.
    """
    return [(1, -0.875, 0.0, 0.55, 0.27, net_in),
            (3, 0.875, 0.0, 0.55, 0.27, net_out),
            (2, 0.0, 0.0, 0.50, 1.70, "GND")]


def _sod523(net_line, net_gnd, swap=False, sgn=1.0):
    """A clamp diode, standing up: the line end nearest the line.

    The pair face opposite ways, and on a two-terminal part that is simply
    which pin number sits at which end.
    """
    a, bnum = (2, 1) if swap else (1, 2)
    return [(a, 0.0, 0.55 * sgn, 0.60, 0.50, net_line),
            (bnum, 0.0, -0.55 * sgn, 0.60, 0.50, net_gnd)]


class LNABoard(PA.PABoard):
    """The transmit board's machinery, on the receive board's own outline.

    Smaller than the transmit board because there is no filter to fit and
    nothing here dissipates more than a third of a watt, but the same
    laminate, the same stack, the same connectors and the same supply, so the
    pair are one instrument.
    """

    def __init__(self):
        PA.pcbgen.Board.__init__(self, BW, BH, W50, LAUNCH_GAP,
                                 parts={"LNA": LNA})
        self.bom = []
        self.mask_bot = []

    def limiter_run(self, ref, x0, y, net_in, net_out, inward, spec=LIM):
        """Down to a low impedance, across the diode, and back up again.

        The two quarter-wave transformers are what let a picofarad sit across
        the line at 5.8 GHz without being noticed.  Between them the line is
        twenty-seven ohms and the diode is one section of a low-pass filter
        whose corner is at twelve gigahertz.
        """
        wt, lt = spec["w_tr_mm"], spec["len_tr_mm"]
        ws, wh, lh = spec["w_sec_mm"], spec["w_hi_mm"], spec["len_half_mm"]
        x = x0
        self._add(rect(x, y - wt / 2, x + lt, y + wt / 2), net_in)
        x += lt
        x_d = x + 1.2 + lh + 0.875
        self._add(rect(x, y - ws / 2, x_d - 0.875 - lh, y + ws / 2), net_in)
        self._add(rect(x_d - 0.875 - lh, y - wh / 2, x_d - 0.875,
                       y + wh / 2), net_in)
        self.smd_part(ref, x_d, y, _smp1330(net_in, net_out),
                      mpn=LIMITER["mpn"], pkg=LIMITER["pkg"],
                      note="invisible until something large arrives")
        for dy in (-0.50, 0.50):
            self._via(x_d, y + dy, 0.30, 0.50, "GND")
        self._add(rect(x_d + 0.875, y - wh / 2, x_d + 0.875 + lh,
                       y + wh / 2), net_out)
        x = x_d + 0.875 + lh
        # The diode's own rectified current has to get back to the line or it
        # cannot limit at all.  That path is a small inductor to ground: ten
        # nanohenries in an 0402 resonates just above this band, so at
        # 5.8 GHz it is the highest impedance it will ever be, and at direct
        # current it is a short.  A printed quarter wave would do the same
        # job and would have to hang eight and a half millimetres straight
        # into the power block below.
        x_ch = x + 2.4
        self._add(rect(x, y - ws / 2, x_ch + 0.31, y + ws / 2), net_out)
        self.shunt_0402(f"L{ref}", x_ch, y - inward * ws / 2, "10n",
                        net_out, -inward, mpn="0402HP-10NXGRW",
                        note="the limiter's return path; the same trick the "
                             "amplifier's own bias choke uses")
        x = x_ch + 1.2
        self._add(rect(x_ch, y - ws / 2, x, y + ws / 2), net_out)
        self._add(rect(x, y - wt / 2, x + lt, y + wt / 2), net_out)
        self.parts.append(dict(kind="LIM", ref=ref, x=x0, y=y,
                               length=x + lt - x0, z_sec=spec["z_sec"],
                               atten_db=spec["atten_db"]))
        return x + lt

    def clamp_site(self, ref, x, y, net, inward, spec=CLP):
        """Two diodes facing opposite ways, and the radio behind them.

        Whichever way the wave swings one of them is already conducting, so
        the voltage across the line cannot grow past a quarter of a volt --
        about a milliwatt.  Their capacitance is small enough that a fifth of
        a millimetre of thin line either side hides it completely.
        """
        wh, lh = spec["w_hi_mm"], spec["len_half_mm"]
        self._add(rect(x - 1.2 - lh, y - wh / 2, x + 1.2 + lh, y + wh / 2),
                  net)
        sgn = -inward
        for k, (dx, swap) in enumerate(((-1.3, False), (1.3, True))):
            xx = x + dx
            yy = y + inward * 1.85
            self.smd_part(f"{ref}{k+1}", xx, yy,
                          _sod523(net, "GND", swap=swap, sgn=sgn),
                          mpn=CLAMP["mpn"], pkg="SOD-523",
                          note="clamps what the radio may see")
            y_line = yy + sgn * 0.55
            self._add(rect(xx - 0.30, min(y, y_line),
                           xx + 0.30, max(y, y_line)), net)
            y_g = yy - sgn * 1.35
            self._via(xx, y_g, 0.25, 0.55, "GND")
            self._add(rect(xx - 0.30, min(y_g, yy - sgn * 0.30),
                           xx + 0.30, max(y_g, yy - sgn * 0.30)), "GND")
        self.parts.append(dict(kind="CLAMP", ref=ref, x=x, y=y,
                               clamp_dbm=spec["clamp_dbm"]))


# ------------------------------------------------------------------- layout
X_C1, X_LIM = 6.4, 9.0
X_C2, X_LNA, X_BT, X_C3 = 34.0, 38.0, 44.0, 48.4
X_TAP, X_PAD, X_CLAMP, X_C4 = 54.0, 66.0, 72.0, 77.0
X_HDR = 91.0
D_TEMP, D_DET = 4.0, 10.0


def channel(b, s, tag, y, inward):
    """One receive chain, laid out left to right along its own centreline."""
    n_in, n_l = f"{tag}_IN", f"{tag}_L"
    n_li, n_lo = f"{tag}_LI", f"{tag}_LO"
    n_t, n_p, n_o = f"{tag}_T", f"{tag}_P", f"{tag}_OUT"
    n_cpl = f"{tag}_CPL"
    rail = f"V5{s}"

    b.sma_launch(f"{tag}_IN", (0.0, y), "left")
    b.line(PULLBACK, y, X_C1 - 0.585, y, n_in)
    b.series_0402(f"C{s}1", X_C1, y, "100p", n_in, n_l,
                  mpn="GRM1555C1H101JA01D",
                  note="blocking capacitor; the limiter must not see any "
                       "direct voltage that arrives on the cable")
    b.line(X_C1 + 0.585, y, X_LIM, y, n_l)
    x_end = b.limiter_run(f"D{s}L", X_LIM, y, n_l, n_li, inward)
    b.line(x_end, y, X_C2 - 0.70, y, n_li)
    b.series_0402(f"C{s}2", X_C2, y, "100p", n_li, n_lo,
                  mpn="GRM1555C1H101JA01D", note="blocking capacitor")

    # ---- the amplifier, in the same package and on the same feed idea as
    # the transmit board's final
    kind = f"LNA_{s}"
    pins = dict(LNA["pins"])
    pins.update({2: n_lo, 8: n_t})
    b.PARTS[kind] = dict(LNA, pins=pins)
    b.amplifier(f"U{s}1", kind, X_LNA, y, thermal=False)
    b._bom(f"U{s}1", LNA["mpn"], "QFN12 3x3", LNA["mpn"],
           "the sensitivity; matched to fifty ohms inside the package")
    b.line(X_C2 + 0.585, y, X_LNA - LNA["land_half"] - 2.4, y, n_lo)
    b.taper(n_lo, X_LNA - LNA["land_half"] - 2.4, X_LNA - LNA["land_half"], y)
    b.taper(n_t, X_LNA + LNA["land_half"] + 2.4, X_LNA + LNA["land_half"], y)
    b.line(X_LNA + LNA["land_half"] + 2.4, y, X_BT, y, n_t)
    _, n_under = b.thermal_field(X_LNA, y, 0.60, pitch=0.60, drill=0.30,
                                 pad=0.50, under=0.65)
    n_ring, _ = b.thermal_field(X_LNA, y, 2.60, pitch=0.62, under=0.0,
                                exclude=0.95)
    y_bt = y + inward * BIAS["len_mm"]
    b.bias_feed(f"L{s}1", (X_BT, y), inward, rail)
    b.line(X_BT, y, X_C3 - 0.585, y, n_t)
    b.series_0402(f"C{s}3", X_C3, y, "100p", n_t, n_cpl + "M",
                  mpn="GRM1555C1H101JA01D", note="blocking capacitor")

    # ---- the coupler: it reports what the radio is being fed, and it is
    # where a leakage canceller would inject its correction
    b.line(X_C3 + 0.585, y, X_TAP, y, n_cpl + "M")
    fwd, inj = b.tap(f"DC{s}", X_TAP, y, -inward, n_cpl + "M", n_cpl,
                     spec=TAP)
    b.line(X_TAP + TAP["len_mm"], y, X_PAD - 3.11, y, n_cpl + "M")
    b.pi_pad_site(s + "P", X_PAD, y, n_cpl + "M", n_p, inward,
                  db=PAD_DEFAULT_DB)
    b.line(X_PAD + 3.11, y, X_CLAMP, y, n_p)
    b.clamp_site(f"D{s}C", X_CLAMP, y, n_p, inward)
    b.line(X_CLAMP, y, X_C4 - 0.585, y, n_p)
    b.series_0402(f"C{s}4", X_C4, y, "100p", n_p, n_o,
                  mpn="GRM1555C1H101JA01D",
                  note="blocking capacitor; nothing direct-current reaches "
                       "the radio from this board")
    b.line(X_C4 + 0.585, y, BW - PULLBACK, y, n_o)
    b.sma_launch(f"{tag}_OUT", (BW, y), "right")

    # the injection connector leaves on the outer edge, straight out from the
    # far end of the sampling line, so it never has to cross the receive line
    edge = BH if inward < 0 else 0.0
    b.sma_launch(f"{tag}_INJ", (inj[0] + 3.6, edge),
                 "top" if inward < 0 else "bottom")
    # A coupler with one end left open is not a coupler, it is a pair of
    # reflections, and the detector on the other end would be reading them.
    # So the injection port is terminated on the board and stays terminated
    # until a canceller is actually connected, at which point this one
    # resistor comes off.
    y_t = edge + inward * 6.0
    b.line(inj[0] + 3.6, y_t, inj[0] + 5.2, y_t, n_cpl, w=0.50)
    b.shunt_0402(f"R{s}J", inj[0] + 5.2, y_t, "51R", n_cpl, +1, axis="x",
                 note="terminates the injection port; REMOVE IT when a "
                      "canceller goes on that connector")
    b.line(inj[0], inj[1], inj[0] + 3.6, inj[1], n_cpl)
    b.corner(inj[0] + 3.6, inj[1], n_cpl)
    b.line(inj[0] + 3.6, inj[1], inj[0] + 3.6, edge - (PULLBACK if inward < 0
                                                       else -PULLBACK), n_cpl)
    return dict(fwd=fwd, inj=inj, n_under=n_under, n_ring=n_ring,
                y_bt=y_bt, rail=rail, nets=(n_in, n_lo, n_t, n_p, n_o))


def detector(b, s, tag, port, inward):
    """What the radio is being fed, as a voltage a computer can read.

    The near end of the sampling line carries a fifteenth of whatever is on
    its way to the radio.  A diode that needs no supply rectifies it, three
    parts smooth it, and the reading says directly how hard the radio is being
    driven -- which is the one number that decides whether the radar is
    listening or being shouted at.
    """
    x, y = port
    n_rf, n_v, n_o = f"{tag}_CPL", f"{tag}_DETV", f"DET{s}"
    y_run = y - inward * 5.5
    b.line(x, y, x, y_run, n_rf, w=0.60)
    b.shunt_0402(f"R{s}T", x, y_run, "51R", n_rf, -inward,
                 note="the sampling line's own load")
    b.line(x - 3.40, y_run, x, y_run, n_rf, w=0.60)
    b.series_0402(f"D{s}1", x - 4.00, y_run, "SMS7630", n_v, n_rf,
                  mpn="SMS7630-079LF", note="zero-bias detector diode")
    xs = x - 4.00 - 0.585
    for k, (ref, val) in enumerate((("C", "100p"), ("R", "10k"),
                                    ("C", "10n"))):
        xk = xs - 1.20 - k * 1.70
        b.line(xk, y_run, xs, y_run, n_v, w=0.60)
        b.shunt_0402(f"{ref}{s}D{k}", xk, y_run, val, n_v, -inward)
    xr = xs - 1.20 - 2 * 1.70
    b.series_0402(f"R{s}O", xr - 1.80, y_run, "1k", n_o, n_v,
                  note="keeps the reading off the radio-frequency side")
    b.line(xr - 1.215, y_run, xr, y_run, n_v, w=0.60)
    return (xr - 2.385, y_run)


def power_block(b, s, yb, y_bt, inward):
    """One channel's supply: a shunt to read the current, then five volts.

    Sixty milliamps is a small current, so the shunt is a whole ohm rather
    than a tenth of one -- sixty millivolts across it, which the amplifier
    beside it turns into three volts.  Most of the twelve-volt drop is taken
    by a resistor before the regulator rather than inside it.
    """
    r12, r5, n_i = f"V12{s}", f"V5{s}", f"V5{s}_I"
    b.smd_part(f"R{s}S", 44.0, yb, _chip(R2512, r12, "V12"), pkg="2512",
               value="1R0", note="reads the amplifier's supply current")
    b.smd_part(f"R{s}V", 38.5, yb, _chip(R2512, n_i, r12), pkg="2512",
               value="82R", note="takes most of the regulator's heat off it")
    b.line(39.5, yb, 42.5, yb, r12, w=0.70)
    b.smd_part(f"U{s}2", 30.0, yb - inward * 3.15, _sot223(("GND", r5, n_i)),
               mpn="AMS1117-5.0", pkg="SOT-223", flip_y=(inward > 0),
               note="five volts, the same regulator the transmit board uses")
    b.line(32.3, yb, 37.0, yb, n_i, w=0.70)
    b.shunt_0402(f"C{s}5", 34.22, yb, "10u", n_i, -inward, pkg="0805")

    y_ina = yb + inward * 3.8
    b.smd_part(f"U{s}3", 44.0, y_ina,
               _sot23_6((f"IMON{s}", "GND", "V12", r12, "GND", r5)),
               mpn="INA181A2IDBVR", pkg="SOT-23-6", flip_y=(inward > 0),
               note="turns the shunt's sixty millivolts into three volts")
    b.line(45.5, yb, 45.5, y_ina - inward * 0.95, "V12", w=0.50)
    b.line(42.5, yb, 42.5, y_ina - inward * 0.95, r12, w=0.50)

    y_tab = yb - inward * 6.30
    b.shunt_0402(f"C{s}6", 24.5, y_tab, "10u", r5, inward, pkg="1210")
    b.shunt_0402(f"C{s}7", 22.0, y_tab, "100n", r5, inward)
    b.line(20.5, y_tab, 31.9, y_tab, r5, w=0.80)
    b.line(21.0, y_ina + inward * 0.95, 21.0, y_tab, r5, w=0.50)
    b.line(21.0, y_ina + inward * 0.95, 42.86, y_ina + inward * 0.95, r5,
           w=0.50)

    # up to the printed feed, and the capacitors that are its short circuit
    y_dec = y_bt + inward * 3.4
    b.shunt_0402(f"C{s}80", X_BT, y_bt + inward * 0.62, "100p", r5, +1,
                 axis="x",
                 note="makes the radio-frequency short the quarter-wave feed "
                      "works against")
    b.line(X_BT, y_bt, X_BT, y_dec, r5, w=0.50)
    b.line(X_BT, y_dec, X_BT + 9.2, y_dec, r5, w=0.50)
    b.line(30.0, y_tab, X_BT, y_tab, r5, w=0.50)
    b.line(X_BT, y_tab, X_BT, y_bt, r5, w=0.50)
    for k, val in enumerate(("1n", "100n"), start=1):
        b.shunt_0402(f"C{s}8{k}", X_BT + 1.7 + (k - 1) * 1.7, y_dec, val, r5,
                     inward)
    b.shunt_0402(f"C{s}83", X_BT + 8.0, y_dec, "10u", r5, inward, pkg="1210")



def build():
    """The whole board: two chains, their supplies, and what watches them."""
    b = LNABoard()
    ch = {"A": channel(b, "A", "RX1", Y_A, -1),
          "B": channel(b, "B", "RX2", Y_B, +1)}
    power_block(b, "A", 41.0, ch["A"]["y_bt"], -1)
    power_block(b, "B", 27.0, ch["B"]["y_bt"], +1)

    # ------------------------------------------------ power and control, right
    hdr = b.header("J5", X_HDR - 8.0, 34.0, 2, 7,
                   ["EN", "GND", "TEMPA", "GND", "IMONA", "GND",
                    "DETA", "GND", "IMONB", "GND", "DETB", "GND",
                    "TEMPB", "GND"])
    b.smd_part("J1", 88.0, 47.0,
               [(1, 0.0, 2.54, 1.70, 1.70, "VIN"),
                (2, 0.0, -2.54, 1.70, 1.70, "GND")],
               pkg="5.08 mm screw terminal", value="2-way",
               note="twelve volts in, a quarter of an amp")
    b.smd_part("F1", 82.0, 49.54, _chip(R1210, "VF", "VIN"), pkg="1210",
               value="1.0 A", mpn="MF-MSMF100", note="resettable fuse")
    b.line(83.15, 49.54, 88.0, 49.54, "VIN", w=0.90)
    b.shunt_0402("D5", 77.5, 49.54, "SMBJ15A", "VF", -1, mpn="SMBJ15A",
                 pkg="SMB", note="clamps whatever the supply lead picks up")
    b.line(57.0, 49.54, 80.85, 49.54, "VF", w=0.90)
    # One transistor blocks a supply connected backwards and is the switch
    # the enable line works.
    b.smd_part("Q3", 71.5, 48.60, _sot23(("V12G", "V12", "VF")),
               mpn="AO3401A", pkg="SOT-23",
               note="blocks a reversed supply and switches the board")
    b.line(72.45, 47.66, 75.0, 47.66, "V12", w=0.70)
    b.line(75.0, 31.0, 75.0, 47.66, "V12", w=0.90)
    b.line(75.0, 44.5, 86.0, 44.5, "V12", w=0.90)
    for k, (val, dx) in enumerate((("22u", 0.0), ("100n", 3.2))):
        b.shunt_0402(f"C2{k}", 81.0 + dx, 44.5, val, "V12", +1,
                     pkg="1210" if val == "22u" else "0402")
    b.line(70.55, 43.0, 70.55, 47.66, "V12G", w=0.35)
    b.line(61.0, 43.0, 70.55, 43.0, "V12G", w=0.35)
    b.line(61.0, 38.0, 75.0, 38.0, "V12", w=0.50)
    for ref, x, val, mpn, note in (
            ("R1", 68.0, "100k", "", "holds the switch on"),
            ("C1", 65.5, "100n", "",
             "brings the switch up over ten milliseconds"),
            ("D6", 63.0, "10V", "BZX84C10",
             "keeps the transistor's gate inside its rating")):
        b.series_0402(ref, x, 40.5, val, "V12", "V12G", horiz=False,
                      mpn=mpn, note=note)
        b.line(x, 41.085, x, 43.0, "V12G", w=0.35)
        b.line(x, 38.0, x, 39.915, "V12", w=0.35)
    b.smd_part("Q4", 59.0, 41.06, _sot23(("EN", "GND", "V12G")),
               mpn="2N7002", pkg="SOT-23",
               note="holds the switch on; the enable line lets go of it")
    b.line(59.0, 42.0, 59.0, 43.0, "V12G", w=0.35)
    b.line(59.0, 43.0, 61.0, 43.0, "V12G", w=0.35)
    # the enable divider sits under the switch, and the fused supply reaches
    # it down the one clear column on this side of the board
    b.line(57.0, 35.085, 57.0, 49.54, "VF", w=0.50)
    b.series_0402("R2", 57.0, 34.5, "100k", "EN", "VF", horiz=False)
    b.line(57.0, 33.915, 61.0, 33.915, "EN", w=0.35)
    b.shunt_0402("R3", 59.8, 33.915, "47k", "EN", -1)
    b.line(58.05, 33.915, 58.05, 40.12, "EN", w=0.35)

    # ------------------------------------------------------------ detectors
    ch["A"]["det"] = detector(b, "A", "RX1", ch["A"]["fwd"], -1)
    ch["B"]["det"] = detector(b, "B", "RX2", ch["B"]["fwd"], +1)

    # ------------------------------------------------ a thermistor per chain
    for s, y, inward in (("A", Y_A, -1), ("B", Y_B, +1)):
        b.shunt_0402(f"RT{s}", 52.0, y + inward * D_TEMP, "10k NTC",
                     f"TEMP{s}", inward, mpn="NCP15XH103F03RC",
                     note="how hot the board is beside the amplifier")

    # -------------------------------------------- everything that is not radio
    for s, y, inward in (("A", Y_A, -1), ("B", Y_B, +1)):
        yb = 41.0 if inward < 0 else 27.0
        for net, src, stub in (
                (f"IMON{s}", (45.14, yb + inward * 4.75), (2.2, 0.0)),
                (f"TEMP{s}", (52.0, y + inward * D_TEMP), (4.4, 0.0)),
                (f"DET{s}", ch[s]["det"], (0.0, 0.0))):
            pin = hdr[net]
            a = (src[0] + stub[0], src[1] + stub[1])
            if stub != (0.0, 0.0):
                b.line(src[0], src[1], a[0], a[1], net, w=0.40)
            b.dc(net, [a, (a[0], pin[1]), pin], skip_last_via=True)
    b.dc("EN", [(58.05, 40.0), (58.05, hdr["EN"][1]), hdr["EN"]],
         skip_last_via=True)

    # twelve volts across to both channels, along the middle
    b.line(49.5, 31.0, 75.0, 31.0, "V12", w=0.90)
    b.line(49.5, 27.0, 49.5, 41.0, "V12", w=0.90)
    b.line(45.5, 41.0, 49.5, 41.0, "V12", w=0.90)
    b.line(45.5, 27.0, 49.5, 27.0, "V12", w=0.90)

    # ----------------------------------------------------------- mechanical
    for x, y in ((5.0, 5.0), (5.0, 63.0), (87.0, 5.0), (87.0, 63.0),
                 (20.0, 52.0), (20.0, 16.0)):
        b.mounts.append((x, y, MOUNT_D, MOUNT_PAD))
    b.unify_nets()
    b.pour()
    for x, y in ((10.0, 26.0), (10.0, 42.0), (88.0, 26.0)):
        b.fiducial(x, y)
    b.mask_bot = [[X_LNA - 3.6, y - 3.6, X_LNA + 3.6, y + 3.6]
                  for y in (Y_A, Y_B)]
    b.labels += [
        (20.0, 37.5, "5.8 GHz RADAR RECEIVE AMPLIFIER", 1.2, "silk"),
        (20.0, 34.5, "ZYF300CA-P 0.76 mm / 4 layer / Ag", 0.9, "silk"),
        (20.0, 31.5, "12 V 0.25 A   J5 = enable + monitors", 0.9, "silk"),
        (10.0, 48.0, "CHANNEL A", 1.1, "silk"),
        (10.0, 20.0, "CHANNEL B", 1.1, "silk"),
    ]
    # the parts that are not soldered on but are needed to build it
    for nm, x, y, side, _dy in b.ports:
        b._bom(nm, "142-0701-801", "SMA end launch", "Cinch 142-0701-801",
               f"{side} edge, 0.062 in board")
    b._bom("H1-H6", "M3 x 6 pan head and washer", "M3", "",
           "the two middle ones sit beside the amplifiers")
    b._bom("TIM", "thermal pad 0.5 mm, 3 W/mK", "10 x 10 mm", "",
           "under each amplifier, between the bare underside and the chassis")
    if b.failed:
        print(f"  {len(b.failed)} connections could not be routed:")
        for f in b.failed[:8]:
            print("     ", f)
    return b, ch


# ====================================================================== report
def report(b, ch):
    c = chain()
    n = noise()
    t = thermal(min(ch["A"]["n_under"], ch["B"]["n_under"]),
                min(ch["A"]["n_ring"], ch["B"]["n_ring"]))
    print("5.8 GHz receive amplifier, two channels\n")
    print(f"  board {BW:g} x {BH:g} mm, {STACK['layers']} layer, "
          f"{STACK['total_mm']:.3f} mm thick, immersion silver")
    print(f"  same laminate, stack, connectors and twelve volts as the "
          f"transmit board\n")

    print("  the chain, one channel, connector to connector")
    print(f"    {'limiter':<28} {-LIMITER['il_db']:+7.2f} dB")
    print(f"    {LNA['mpn']:<28} {LNA['gain_db']:+7.2f} dB   noise "
          f"{LNA['nf_db']:.2f} dB, output limit {LNA['p1db_dbm']:+.1f} dBm")
    print(f"    {'sampling coupler':<28} {-L_COUPLER:+7.2f} dB")
    print(f"    {'level pad, fitted':<28} {-c['pad_db']:+7.2f} dB")
    print(f"    {'line, blocks, clamp, launches':<28} "
          f"{-(c['loss_in_db'] - LIMITER['il_db'] + c['loss_out_db'] - L_COUPLER - c['pad_db']):+7.2f} dB")
    print(f"    {'net':<28} {c['gain_db']:+7.2f} dB\n")
    print(f"    system noise figure          {n['nf_db']:5.2f} dB "
          f"against {n['nf_none_db']:.2f} with the radio alone")
    print(f"    which is worth               {n['range_ratio']:5.2f} times "
          f"the detection range, at the same transmit power\n")

    print("  what can reach the radio, and what stops it")
    print(f"    the radio compresses above {B210_P1DB_DBM:+.0f} dBm and is "
          f"damaged above {B210_MAX_DBM:+.0f} dBm")
    for th in threats():
        verdict = ("DAMAGED" if th["damages"] else
                   "compressed, not harmed" if th["compresses"] else "ok")
        print(f"    {th['what']:<42} {th['at_input']:+6.1f} dBm in, "
              f"{th['at_radio']:+6.1f} at the radio   {verdict}")
    print(f"    the amplifier itself is rated to {LNA['pin_max_dbm']:+.0f} "
          f"dBm and the limiter keeps it there\n")

    print("  the three things that do the protecting")
    print(f"    limiter    {LIM['w_sec_mm']:.2f} mm of {LIM['z_sec']:.0f} ohm "
          f"line between two {LIM['len_tr_mm']:.2f} mm transformers, so a "
          f"picofarad")
    print(f"               of diode has its corner at "
          f"{LIM['fc_hz']/1e9:.1f} GHz instead of "
          f"{1/(math.pi*50*LIM['c_pf']*1e-12)/1e9:.1f}.  It shuts "
          f"{LIM['atten_db']:.0f} dB when it fires")
    print(f"    clamp      two diodes back to back: nothing above "
          f"{CLP['clamp_dbm']:+.1f} dBm leaves the board, and the level pad "
          f"takes off {c['pad_db']:.0f} more")
    print(f"    level pad  fitted at {c['pad_db']:.0f} dB, which is what "
          f"decides how far apart the two arrays have to sit\n")

    print("  the level pad, and what each value asks of the mounting")
    print(f"    {'pad':>5} {'net gain':>9} {'noise':>7} {'range':>7}   "
          f"arrays at least")
    for db in (0.0, 3.0, 6.0, 8.0, 12.0):
        cc, nn = chain(db), noise(db)
        r = separation_for(PA.chain()["out_connector_dbm"] - 0.5,
                           cc["gain_db"], limit_dbm=B210_P1DB_DBM)
        print(f"    {db:4.0f} dB {cc['gain_db']:8.1f} dB "
              f"{nn['nf_db']:6.2f} dB {nn['range_ratio']:6.2f}x   "
              f"{r*1000:5.0f} mm apart")
    print(f"    fitted: {PAD_DEFAULT_DB:.0f} dB\n")

    print("  what the four monitor pins say, per channel")
    print(f"    supply current   {R_SENSE * G_SENSE:.0f} V per amp: "
          f"{R_SENSE * G_SENSE * LNA['idd_ma'] / 1000:.2f} V when it is "
          f"working.  Nothing there means")
    print(f"                     the amplifier is not")
    print(f"    level at the radio  the sampling coupler through a diode: it "
          f"rises the moment")
    print(f"                     something big arrives, whether it is ours "
          f"or somebody else's")
    print(f"    temperature      10 kohm at 25 C beside the amplifier")
    print(f"    enable           held high on the board; pull it to ground "
          f"and both channels stop\n")

    print(f"  heat: {t['p_w']:.2f} W per amplifier, {t['r_board']:.1f} C/W to "
          f"the chassis, ground lead {t['t_lead_c']:.0f} C in a 25 C room "
          f"({t['lead_margin_c']:.0f} C in hand)")
    kinds = {}
    for p in b.parts:
        kinds[p["kind"]] = kinds.get(p["kind"], 0) + 1
    print("\n  parts: " + ", ".join(f"{v} x {k}"
                                    for k, v in sorted(kinds.items())))
    print(f"  {len(b.bom)} fitted, copper polygons {len(b.top)}, "
          f"vias {len(b.vias)}, holes {len(b.mounts)}")
    print("  connectors:")
    for nm, x, y, side, _dy in b.ports:
        print(f"    {nm:10} {side:6} edge at x {x:7.3f}  y {y:7.3f}")


if __name__ == "__main__":
    b, ch = build()
    report(b, ch)
    t = thermal(min(ch["A"]["n_under"], ch["B"]["n_under"]),
                min(ch["A"]["n_ring"], ch["B"]["n_ring"]))
    json.dump({
        "outline": [BW, BH], "layers": 4, "w50": W50,
        "launch_gap": LAUNCH_GAP,
        "stack": dict(name=SUB.name, h_mm=SUB.h * 1e3, er=SUB.er,
                      tand=SUB.tand, finish="immersion silver",
                      copper_oz=1, **STACK),
        "top": b.top, "top_net": b.nets, "pad_idx": sorted(b.pads),
        "inner": b.inner, "inner_net": b.inner_net,
        "gnd_top": b.gnd_top, "vias": b.vias, "via_net": b.via_net,
        "mounts": b.mounts, "ports": b.ports, "parts": b.parts,
        "labels": b.labels, "bom": b.bom, "mask_bot": b.mask_bot,
        "printed": {"bias": BIAS, "tap": TAP, "limiter": LIM, "clamp": CLP,
                    "pad_db": PAD_DEFAULT_DB},
        "chain": chain(), "noise": noise(), "thermal": t,
        "threats": threats(),
        "parts_spec": {"lna": {k: v for k, v in LNA.items() if k != "pins"},
                       "limiter": {k: v for k, v in LIMITER.items()
                                   if k != "pins"},
                       "clamp": CLAMP},
    }, open(os.path.join(HERE, "lna_board.json"), "w"), indent=1)
    print("\nwrote lna_board.json")
