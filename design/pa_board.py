"""The transmit power amplifier board: two channels, three quarters of a watt each.

The array boards are finished and measured.  What limits the radar now is how
loudly it can shout: the radio on its own puts about ten milliwatts into each
transmit antenna, and detection range grows only as the fourth root of that.
This board sits between the radio and the two transmit antennas and multiplies
each channel by about seven hundred, which is four and a half times the range.

Two identical chains, laid out as mirror images about the middle of the board,
with everything that carries power in the band between them.  Each chain is:

    connector -> level pad -> driver -> final amplifier -> harmonic filter
              -> sampling coupler -> connector

Four things on it are printed rather than bought, because copper is free, a
printed part cannot be out of stock, it has no self-resonance to be surprised
by, and it carries as much current as you like:

  * the feed that gets supply voltage onto the amplifier's own output pin
    without letting the signal escape up the supply wire,
  * the filter that removes the harmonics a hard-driven amplifier makes,
  * the coupler that samples both what is going out and what is coming back,
  * the shunt legs of the level pad, which also drain static off the connector.

Everything electrical is derived here and written to pa_board.json, which the
KiCad generator turns into a board.  The same numbers feed the openEMS model
in sim/pa_printed.py, so what is simulated is what gets made.
"""

import json
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import rfmath as rf                                            # noqa: E402
from geom import rect                                          # noqa: E402
import pcbgen                                                  # noqa: E402

C0 = 299792458.0
F0 = 5.80e9
F_LO, F_HI = 5.725e9, 5.875e9          # the band the licence covers

# Same laminate, thickness, copper and finish as the two array boards, so the
# whole radar is one material and one set of constants.  A PTFE board is a
# poor conductor of heat, which is why the amplifiers sit over fields of holes
# and the board bolts to metal directly underneath them.
SUB = rf.ZYF300CA
W50 = rf.ms_width_for_z0(50.0, SUB, F0)[0] * 1e3
LAM_G = rf.ms_guided_wavelength(W50 * 1e-3, SUB, F0) * 1e3
LOSS_MM = rf.ms_loss_db_per_m(W50 * 1e-3, SUB, F0) / 1000.0
LAUNCH_GAP = 0.9781                    # the array boards' launch, unchanged

# Four layers, not two.  Fourteen supply and monitoring nets have to cross
# the middle of this board in both directions at once, and on two layers the
# underside is solid ground and unavailable, so every one of them would have
# to share the surface with the others and with the radio-frequency lines.
# Giving them a layer of their own with ground either side of it costs one
# more lamination and removes the whole problem.  The top dielectric is
# unchanged, so every printed dimension on the surface is exactly as designed.
#
#   top      everything at radio frequency
#   inner 1  ground, the reference for all of it
#   inner 2  supply and control
#   bottom   ground, and the face that bolts to the chassis
STACK = dict(layers=4, dielectric_mm=[0.76, 0.338, 0.338],
             total_mm=0.76 + 0.338 + 0.338 + 4 * 0.035,
             connector="Cinch 142-0701-801 (0.062 in)")
W_DC = 0.45                            # a buried run; 0.9 mm for the supplies

BW, BH = 100.0, 84.0
Y_A, Y_B = 74.0, 10.0                  # the two chains
PULLBACK = 0.15
MOUNT_D, MOUNT_PAD = 3.20, 6.00


# ---------------------------------------------------------------- real parts
# Every pin of every part is from its datasheet.  A board whose nets were
# guessed passes every check and then fails on the bench.
#
#   PMA3-73-1W+  Mini-Circuits, 300-7500 MHz gallium arsenide power amplifier,
#                matched to fifty ohms inside the package so there is no
#                matching network to get wrong.  Supply arrives on the output
#                pin, which is why the printed feed exists.  Sold for radar,
#                and its own noise figure is 2.3 dB, which matters because a
#                transmitter's noise lands in its own receiver.
#   PHA-1H+      Mini-Circuits, 0.05-6 GHz driver in a SOT-89 with a metal
#                tab.  It runs twelve decibels below its own limit here, so it
#                adds gain and almost no distortion.
#
# Interpolated to 5.800 GHz from the datasheet tables, which quote 4.0 and
# 6.0 GHz.  Straight-line between the two, which is what the swept graphs do.
def _interp(f_ghz, lo_f, lo_v, hi_f, hi_v):
    return lo_v + (f_ghz - lo_f) / (hi_f - lo_f) * (hi_v - lo_v)


F_GHZ = F0 / 1e9
FINAL = dict(
    mpn="PMA3-73-1W+", mfr="Mini-Circuits", pkg="QFN12", body=3.0, pitch=0.51,
    pad_w=0.25, pad_l=0.51, land_half=1.355, ep=1.25, n_side=3,
    gain_db=_interp(F_GHZ, 4.0, 20.1, 6.0, 19.3),
    gain_min_db=_interp(F_GHZ, 4.0, 18.2, 6.0, 17.5),
    p1db_dbm=_interp(F_GHZ, 4.0, 31.7, 6.0, 29.0),
    psat_dbm=_interp(F_GHZ, 4.0, 33.5, 6.0, 32.2),
    nf_db=_interp(F_GHZ, 4.0, 2.5, 6.0, 2.3),
    h2_dbc=_interp(F_GHZ, 4.0, -28.0, 6.0, -44.0),
    h3_dbc=_interp(F_GHZ, 4.0, -53.0, 6.0, -52.0),
    vdd=12.0, idq_ma=190.0, id_p1db_ma=400.0, id_sat_ma=500.0,
    pdiss_max_w=5.5, theta_jc=12.9, t_lead_max=85.0, t_die_max=175.0,
    pin_max_dbm=24.0, price_gbp=31.0,
    pins={1: "GND", 2: "RFIN", 3: "GND", 4: "GND", 5: "GND", 6: "GND",
          7: "GND", 8: "RFOUT", 9: "GND", 10: "GND", 11: "GND", 12: "IADJ",
          13: "GND"},
    note="pins 4-6 and 10-11 are unused inside the package and are grounded "
         "on the maker's own test board, which is also the best thing to do "
         "with them thermally")
DRIVER = dict(
    mpn="PHA-1H+", mfr="Mini-Circuits", pkg="SOT89",
    gain_db=_interp(F_GHZ, 4.0, 10.9, 6.0, 9.6),
    gain_min_db=_interp(F_GHZ, 4.0, 9.8, 6.0, 9.0),
    p1db_dbm=_interp(F_GHZ, 4.0, 22.5, 6.0, 22.0),
    nf_db=_interp(F_GHZ, 4.0, 2.6, 6.0, 3.4),
    vdd=5.0, id_ma=132.0, id_max_ma=165.0, pdiss_max_w=1.0, theta_jc=36.1,
    pin_max_dbm=24.0, price_gbp=8.4,
    pins={1: "RFIN", 2: "GND", 3: "RFOUT", 4: "GND"})


# --------------------------------------------------------------- the chain
# Losses between the parts, each one a real piece of the board rather than a
# lump allowance: the connector transition as measured on the array boards,
# the line at 0.0032 dB per millimetre, and the printed parts as designed.
L_LAUNCH = 0.05
L_BLOCK = 0.05                          # a 1000 pF 0402 at 5.8 GHz
L_FILTER = 0.17                         # MEASURED on the copper, sim/pa_printed.py
L_BIAS = 0.07                           # each supply feed, capacitor-limited
L_COUPLER = 0.03                        # MEASURED; the maths said 0.043
PAD_DEFAULT_DB = 6.0
TRACE_IN_MM, TRACE_OUT_MM = 28.0, 34.0
PREDROP_R = 27.0                        # shares the regulator's heat
LDO_V = 5.0
R_SENSE = 0.10                          # ohm, in each final's supply
G_SENSE = 50.0                          # the current amplifier's gain, V/V
TRIP_MA = 600.0                         # tell the host to shut a channel here


def pi_pad(db, z0=50.0):
    """The three resistors that set how hard the radio drives the board.

    Also why the input needs nothing else to drain static: the two shunt legs
    are a direct path to ground at direct current.
    """
    k = 10.0 ** (abs(db) / 20.0)
    return z0 * (k + 1.0) / (k - 1.0), z0 * (k * k - 1.0) / (2.0 * k)


def e24(x):
    """Nearest value in the ordinary five per cent resistor series."""
    base = [1.0, 1.1, 1.2, 1.3, 1.5, 1.6, 1.8, 2.0, 2.2, 2.4, 2.7, 3.0,
            3.3, 3.6, 3.9, 4.3, 4.7, 5.1, 5.6, 6.2, 6.8, 7.5, 8.2, 9.1, 10.0]
    d = 10.0 ** math.floor(math.log10(x))
    return min((b * d for b in base), key=lambda v: abs(v - x))


def chain(pad_db=PAD_DEFAULT_DB):
    """Connector to connector, worked backwards from the one fixed number.

    The final amplifier wants a particular level at its input and nothing else
    on the board is allowed to argue with it, so everything else is derived
    from that.
    """
    l_in = L_LAUNCH + TRACE_IN_MM * LOSS_MM + 2 * L_BLOCK + pad_db
    l_mid = L_BLOCK + 6.0 * LOSS_MM + L_BIAS
    l_out = (L_BLOCK + TRACE_OUT_MM * LOSS_MM + L_BIAS + L_FILTER + L_COUPLER
             + L_LAUNCH)

    drive_final = FINAL["p1db_dbm"] - FINAL["gain_db"]
    out_board = FINAL["p1db_dbm"] - l_out
    drive_board = drive_final + l_mid - DRIVER["gain_db"] + l_in
    worst = ((FINAL["gain_min_db"] - FINAL["gain_db"])
             + (DRIVER["gain_min_db"] - DRIVER["gain_db"]))
    return dict(
        pad_db=pad_db, loss_in_db=l_in, loss_mid_db=l_mid, loss_out_db=l_out,
        gain_db=DRIVER["gain_db"] + FINAL["gain_db"] - l_in - l_mid - l_out,
        drive_for_full_dbm=drive_board,
        drive_for_full_worst_dbm=drive_board - worst,
        driver_out_dbm=drive_final + l_mid,
        driver_backoff_db=DRIVER["p1db_dbm"] - (drive_final + l_mid),
        final_in_dbm=drive_final,
        out_connector_dbm=out_board,
        out_connector_w=10 ** (out_board / 10.0) / 1e3,
        sat_connector_dbm=FINAL["psat_dbm"] - l_out,
        pair_dbm=out_board + 10 * math.log10(2.0))


def dissipation(driven="p1db"):
    """Heat one channel makes, and it makes it without a break.

    A frequency-modulated radar transmits continuously, so there is no duty
    cycle to hide behind: whatever is dissipated is dissipated all the time.
    """
    idd = {"idle": FINAL["idq_ma"], "p1db": FINAL["id_p1db_ma"],
           "sat": FINAL["id_sat_ma"]}[driven] / 1000.0
    pout = {"idle": 0.0, "p1db": 10 ** (FINAL["p1db_dbm"] / 10.0) / 1e3,
            "sat": 10 ** (FINAL["psat_dbm"] / 10.0) / 1e3}[driven]
    pin = {"idle": 0.0,
           "p1db": 10 ** ((FINAL["p1db_dbm"] - FINAL["gain_db"]) / 10.0) / 1e3,
           "sat": 10 ** ((FINAL["psat_dbm"] - FINAL["gain_db"] + 3.0) / 10.0)
           / 1e3}[driven]
    p_final = FINAL["vdd"] * idd + pin - pout
    i_drv = DRIVER["id_ma"] / 1000.0
    return dict(final_w=p_final, final_i_a=idd,
                driver_w=DRIVER["vdd"] * i_drv,
                predrop_w=PREDROP_R * i_drv * i_drv,
                ldo_w=(FINAL["vdd"] - PREDROP_R * i_drv - LDO_V) * i_drv,
                pae_pct=100.0 * (pout - pin) / (FINAL["vdd"] * idd),
                headroom_w=FINAL["pdiss_max_w"] - p_final,
                channel_w=FINAL["vdd"] * idd + FINAL["vdd"] * i_drv)


def via_theta(drill_mm, plate_um, h_mm, n):
    """How well a field of plated holes carries heat to the other side."""
    r_out = (drill_mm / 2.0 + plate_um / 1000.0) * 1e-3
    r_in = drill_mm / 2.0 * 1e-3
    return (h_mm * 1e-3) / (400.0 * math.pi * (r_out ** 2 - r_in ** 2)) \
        / max(n, 1)


def spread_theta(r_in_mm, r_out_mm, t_um, k=400.0):
    """Sideways through a copper foil, from a small square out to the holes."""
    return math.log(r_out_mm / r_in_mm) / (2.0 * math.pi * k * t_um * 1e-6)


def thermal(n_under, n_ring, ambient_c=25.0, driven="p1db"):
    """Where the final amplifier's die ends up, path by path.

    The maker specifies the part from its ground lead outwards, so everything
    below that lead belongs to this board, and on a four-layer board it leaves
    in two stages.

    First, from the package's own metal pad down to the ground plane just
    under the surface: partly straight down the holes directly beneath it,
    partly sideways through the surface foil into the ring of holes around it.
    Two routes in parallel, not one after the other -- treating them as a
    chain is what makes a field of holes look useless when it is not.

    Then from that plane down to the face that bolts to the chassis.  By this
    point the heat has spread across the plane, so every hole in the field
    carries some of it, and that stage is nearly free.
    """
    p = dissipation(driven)["final_w"]
    h1 = STACK["dielectric_mm"][0]
    h2 = sum(STACK["dielectric_mm"][1:])
    r_under = via_theta(0.30, 25.0, h1, n_under)
    r_ring = (spread_theta(0.70, 2.40, SUB.t * 1e6)
              + via_theta(0.30, 25.0, h1, n_ring))
    r_top = 1.0 / (1.0 / r_under + 1.0 / r_ring)
    r_deep = via_theta(0.30, 25.0, h2, n_under + n_ring)
    r_iface = 1.2                     # bare copper under the part, 0.3 mm pad
    r_board = r_top + r_deep + r_iface
    t_lead = ambient_c + p * r_board
    t_die = t_lead + p * FINAL["theta_jc"]
    return dict(p_w=p, n_under=n_under, n_ring=n_ring, r_under=r_under,
                r_ring=r_ring, r_top=r_top, r_deep=r_deep, r_iface=r_iface,
                r_board=r_board, t_lead_c=t_lead, t_die_c=t_die,
                lead_margin_c=FINAL["t_lead_max"] - t_lead,
                die_margin_c=FINAL["t_die_max"] - t_die,
                max_ambient_c=FINAL["t_lead_max"] - p * r_board)


# ------------------------------------------------------------ printed parts
def bias_tee(z_line=128.0, f=F0, sub=SUB, w_main=None, l_cap_nh=0.15):
    """How to put twelve volts on a pin that is also the signal output.

    A quarter-wave line turns whatever is at one end into its opposite at the
    other: a short becomes an open.  So a short circuit is made at the far end
    and a quarter wave of thin line turns it into an open circuit right at the
    amplifier's pin.  The signal sees nothing; the supply walks in.

    Thin line, because thin is a high impedance and a high-impedance quarter
    wave turns a good short into a better one.  0.20 mm is thin enough to be
    worth it and wide enough for any fabricator, and it carries half an amp
    with eight millivolts across it.

    The short at the far end is made by capacitors, not by a printed stub.
    Both were built and measured.  A printed quarter-wave open stub is a
    beautiful idea and it cost 0.39 dB: on resonance it carries a large
    standing-wave current and radiates it, and the measurement showed eight
    per cent of the power going nowhere.  A pair of capacitors with their own
    ground vias is a short of a few ohms and has no aperture to radiate from,
    and the same quarter wave then costs a hundredth of a decibel.

    A thin feed meeting a wide line does not begin where it is drawn to
    begin -- the current spreads into the wider conductor and the electrical
    reference sits about half a width inside it.  Measured: drawn at a plain
    quarter wave the feed behaved as though it belonged at 7.55 GHz.  The
    drawn length therefore carries that half-width at each end.
    """
    w = rf.ms_width_for_z0(z_line, sub, f)[0]
    lam = rf.ms_guided_wavelength(w, sub, f)
    w_ref = (W50 * 1e-3) if w_main is None else w_main
    dl_junction = w_ref / 2.0 + 0.62e-3 / 2.0
    z_short = 2.0 * math.pi * f * l_cap_nh * 1e-9
    z_seen = z_line ** 2 / z_short
    return dict(w_mm=w * 1e3, len_mm=(lam / 4.0 + dl_junction) * 1e3,
                len_ideal_mm=lam / 4.0 * 1e3, junction_mm=dl_junction * 1e3,
                z0=z_line, l_cap_nh=l_cap_nh,
                z_short_ohm=z_short, z_seen_ohm=z_seen,
                insertion_db=20 * math.log10(1.0 + 25.0 / z_seen),
                r_dc_ohm=1.72e-8 * (lam / 4.0 + dl_junction) / (w * sub.t))


CHEB_0P1 = {5: [1.1468, 1.3712, 1.9750, 1.3712, 1.1468],
            7: [1.1812, 1.4228, 2.0967, 1.5733, 2.0967, 1.4228, 1.1812]}

# MEASURED, and it is why this filter is not the one the textbook draws.
# Built exactly as synthesised at a 7.0 GHz corner, the copper came out with
# its corner at 6.32 GHz -- nine per cent low -- and 5.8 GHz was already on
# the skirt: 0.9 dB of loss and only 7.6 dB of return loss, which would have
# thrown away a fifth of the transmitter.  The passband ripple below 5 GHz
# was textbook, so the synthesis is right and only the frequency is wrong:
# the wide sections behave longer than their drawn length because each step
# adds fringing capacitance the model does not carry.  So the design corner
# is raised by the measured factor, and the order raised from five to seven
# to win back the rejection that moving the corner up would have cost.
LPF_REALISED = 0.903


def cheb_atten(f, fc, n=5, ripple_db=0.1):
    """How far down a Chebyshev filter is, above its corner."""
    if f <= fc:
        return 0.0
    eps2 = 10.0 ** (ripple_db / 10.0) - 1.0
    t = math.cosh(n * math.acosh(f / fc))
    return 10.0 * math.log10(1.0 + eps2 * t * t)


def lowpass(fc_want_hz=7.90e9, order=7, z_hi=128.0, z_lo=24.0, sub=SUB):
    """A printed filter that passes 5.8 GHz and stops what the amplifier adds.

    Driven near its limit an amplifier makes copies of the signal at twice and
    three times the frequency.  They are not wanted, they are not licensed and
    the antenna radiates some of them.  Removing them costs nothing but a strip
    of copper: alternating wide and narrow sections behave as the capacitors
    and inductors of a five-pole filter, wide being capacitive and narrow
    inductive.

    Cornered at 7.0 GHz it is still flat at the top of the band and already a
    hundredfold down at twice the frequency.
    """
    fc_hz = fc_want_hz / LPF_REALISED
    wc = 2.0 * math.pi * fc_hz
    out = []
    for i, g in enumerate(CHEB_0P1[order]):
        z_t = z_lo if i % 2 == 0 else z_hi
        w = rf.ms_width_for_z0(z_t, sub, fc_hz)[0]
        lam = rf.ms_guided_wavelength(w, sub, fc_hz)
        z = rf.ms_dispersive(w, sub, fc_hz)[0]
        if i % 2 == 0:
            val = g / (50.0 * wc)                       # farads
            bl = val * wc * z
        else:
            val = g * 50.0 / wc                         # henries
            bl = val * wc / z
        out.append(dict(kind="C" if i % 2 == 0 else "L", w_mm=w * 1e3,
                        len_mm=bl / (2 * math.pi) * lam * 1e3, value=val,
                        z0=z, theta_deg=math.degrees(bl)))
    return dict(fc_hz=fc_hz, fc_realised_hz=fc_want_hz, order=order,
                sections=out, ripple_db=0.1,
                length_mm=sum(s["len_mm"] for s in out),
                reject={"2f0": cheb_atten(2 * F0, fc_want_hz, order),
                        "3f0": cheb_atten(3 * F0, fc_want_hz, order),
                        "f_hi": cheb_atten(F_HI, fc_want_hz, order)})


TAP = rf.coupler_synth(20.0, SUB)      # samples forward and reflected power
BIAS = bias_tee()
LPF = lowpass()
PAD_SHUNT, PAD_SERIES = pi_pad(PAD_DEFAULT_DB)


# What openEMS measured on the copper as drawn (sim/pa_printed.py).  Each of
# these replaced a closed-form number that was wrong, and the differences are
# recorded next to them.
MEASURED = {
    "filter": dict(loss_5725=0.14, loss_5800=0.16, loss_5875=0.17,
                   match_db=-15.2, rej_2f0=-37.8, rej_3f0=-60.2,
                   note="first cut, five sections at a 7.0 GHz corner, came "
                        "out with its corner at 6.32 GHz and cost 0.86 dB in "
                        "band; the corner is now raised by that measured "
                        "factor and the order taken to seven"),
    "tap": dict(coupling_db=-20.75, isolated_db=-29.25, directivity_db=8.49,
                through_db=-0.03, match_db=-40.1,
                note="the coupling is within a decibel of the synthesis; the "
                     "directivity is what microstrip gives and it is what "
                     "decides how sharply the reflected-power reading moves"),
    "bias": dict(loss_db=0.02, match_db=-37.8,
                 note="with a printed quarter-wave open stub for its short "
                      "this cost 0.39 dB and lost eight per cent of the "
                      "power to radiation; terminated on capacitors instead "
                      "it costs nothing measurable"),
}


def separation_for(pt_dbm, lna_gain_db, limit_dbm=-15.0,
                   iso_ref_db=-42.17, r_ref_m=0.0921):
    """How far apart the two array boards have to sit at this transmit power.

    Measured on the whole undivided board: -42.17 dB at 92.1 mm between the
    transmit and receive blocks.  Pull them apart and the path through the air
    thins as the square of the distance, so the separation follows directly
    from how much leakage the receiver can take.
    """
    need = limit_dbm - lna_gain_db - pt_dbm
    return r_ref_m * 10 ** ((iso_ref_db - need) / 20.0)


# ======================================================================= board
class PABoard(pcbgen.Board):
    """The same geometry machinery the front-end board uses, on this board's size.

    Everything it already knows how to do -- lay a track, find a clear spot
    for a via, pour ground, work out which pieces of copper are really one net
    -- applies unchanged.  What it does not have is a part sitting IN the line
    rather than shunt to ground, which every blocking capacitor on a signal
    path is, so that is added here.
    """

    def __init__(self):
        super().__init__(BW, BH, W50, LAUNCH_GAP,
                         parts={"FINAL": FINAL, "DRIVER": DRIVER})
        self.bom = []
        self.bot, self.bot_net = [], []

    def dc(self, net, pts, w=W_DC, n_via=1, skip_last_via=False):
        """A supply or monitoring run, on the layer set aside for them.

        It leaves the surface at one end and comes back at the other, and in
        between it is under a ground plane, so it can cross anything without
        having to be told about it.  That is the whole reason this board has
        four layers instead of two.
        """
        for (x0, y0), (x1, y1) in zip(pts, pts[1:]):
            if abs(y1 - y0) < 1e-9:
                self.inner.append(rect(min(x0, x1), y0 - w / 2,
                                       max(x0, x1), y0 + w / 2))
            else:
                self.inner.append(rect(x0 - w / 2, min(y0, y1),
                                       x0 + w / 2, max(y0, y1)))
            self.inner_net.append(net)
        ends = [pts[0]] if skip_last_via else [pts[0], pts[-1]]
        for px, py in ends:
            for i in range(n_via):
                d = (i - (n_via - 1) / 2.0) * 0.80
                self._add(rect(px + d - 0.30, py - 0.30,
                               px + d + 0.30, py + 0.30), net)
                self._via(px + d, py, 0.30, 0.60, net)
        self.parts.append(dict(kind="DCRUN", ref=net,
                               pts=[[round(a, 3), round(b, 3)]
                                    for a, b in pts]))

    def hop(self, net, a, b, w=0.80, n_via=2):
        """Take a run under another one, on the far side of the board.

        The underside is a ground plane and stays one, except here: a short
        length of it is given over to the supply so that the three signals
        going the other way can keep the surface to themselves.  It is done in
        the middle of the board where there is no radio-frequency line above
        to lose its reference, and the plane closes up either side of it.
        """
        (x0, y0), (x1, y1) = a, b
        if abs(y1 - y0) < 1e-9:
            self.bot.append(rect(min(x0, x1), y0 - w / 2,
                                 max(x0, x1), y0 + w / 2))
        else:
            self.bot.append(rect(x0 - w / 2, min(y0, y1),
                                 x0 + w / 2, max(y0, y1)))
        self.bot_net.append(net)
        for k, (px, py) in enumerate((a, b)):
            for i in range(n_via):
                d = (i - (n_via - 1) / 2.0) * 0.85
                vx = px + (d if abs(y1 - y0) > 1e-9 else 0.0)
                vy = py + (0.0 if abs(y1 - y0) > 1e-9 else d)
                self._add(rect(vx - 0.35, vy - 0.35, vx + 0.35, vy + 0.35),
                          net)
                self._via(vx, vy, 0.35, 0.65, net)
        self.parts.append(dict(kind="HOP", ref=net, x=x0, y=y0, x2=x1, y2=y1))

    # --------------------------------------------------------------- parts
    def _bom(self, ref, value, pkg, mpn="", note=""):
        self.bom.append(dict(ref=ref, value=value, pkg=pkg, mpn=mpn,
                             note=note))

    def series_0402(self, ref, x, y, value, net_a, net_b, horiz=True,
                    kind="C", mpn="", note=""):
        """A blocking capacitor or a pad resistor sitting in the line itself.

        Both ends are live: the track arrives on one pad and leaves the other,
        so neither may be tied to ground the way a decoupling part is.
        """
        p = pcbgen.R0402
        c = p["gap"] / 2 + p["w"] / 2
        if horiz:
            a = rect(x - c - p["w"] / 2, y - p["h"] / 2,
                     x - c + p["w"] / 2, y + p["h"] / 2)
            b = rect(x + c - p["w"] / 2, y - p["h"] / 2,
                     x + c + p["w"] / 2, y + p["h"] / 2)
            ends = ((x - c, y), (x + c, y))
        else:
            a = rect(x - p["w"] / 2, y - c - p["h"] / 2,
                     x + p["w"] / 2, y - c + p["h"] / 2)
            b = rect(x - p["w"] / 2, y + c - p["h"] / 2,
                     x + p["w"] / 2, y + c + p["h"] / 2)
            ends = ((x, y - c), (x, y + c))
        self._add(a, net_a, True)
        self._add(b, net_b, True)
        self.parts.append(dict(kind="SERIES", ref=ref, x=x, y=y, value=value,
                               horiz=horiz, mpn=mpn,
                               nets=[net_a, net_b], pitch=round(2 * c, 4)))
        self._bom(ref, value, "0402", mpn, note)
        return ends

    def shunt_0402(self, ref, x, y, value, net, sgn, kind="R", mpn="",
                   note="", pkg="0402", axis="y"):
        """A part from the line to ground, its live pad on the line itself.

        Which way the ground end steps matters: along a horizontal line it
        steps in y, but on a VERTICAL line it has to step in x, or the ground
        pad lands on the very line it is decoupling and shorts the supply to
        ground -- silently, because the checker then sees one net and is happy.
        """
        w, h, c = {"0402": (0.62, 0.62, 1.17),
                   "0805": (2.00, 1.25, 1.95),
                   "1210": (2.70, 1.30, 2.30),
                   "SMB": (2.70, 1.30, 2.30)}[pkg]
        if axis == "x":
            w, h = h, w
        self._add(rect(x - w / 2, y - h / 2, x + w / 2, y + h / 2), net, True)
        xg = x + (sgn * c if axis == "x" else 0.0)
        yg = y + (0.0 if axis == "x" else sgn * c)
        self._add(rect(xg - w / 2, yg - h / 2, xg + w / 2, yg + h / 2),
                  "GND", True)
        for d in ((0.0,) if pkg == "0402" else (-0.85, 0.85)):
            self._via(xg + (0.0 if axis == "x" else d),
                      yg + (d if axis == "x" else 0.0), 0.25, 0.55, "GND")
        self.parts.append(dict(kind="SHUNT", ref=ref, x=x, y=y, value=value,
                               sgn=sgn, mpn=mpn, pitch=round(c, 4), pkg=pkg,
                               size=[w, h], axis=axis, nets=[net, "GND"]))
        self._bom(ref, value, pkg, mpn, note)
        return (x, yg)

    def pi_pad_site(self, ref, x, y, net_in, net_out, sgn, db=PAD_DEFAULT_DB):
        """Series resistor with a shunt leg either side: sets the drive level.

        Fitted rather than adjustable, on purpose.  The radio in front of this
        board already carries a calibrated attenuator with a quarter of a
        decibel of resolution, so a second one here would be a duplicate with
        a control bus attached.  What the radio cannot do is stop somebody
        turning it up too far, and that is what these three resistors are for.
        """
        sh, se = pi_pad(db)
        p, c = pcbgen.R0402, pcbgen.R0402["gap"] / 2 + pcbgen.R0402["w"] / 2
        self.shunt_0402(f"R{ref}1", x - 1.90, y, f"{e24(sh):g}R", net_in, sgn)
        self.series_0402(f"R{ref}2", x, y, f"{e24(se):g}R", net_in, net_out,
                         kind="R")
        self.shunt_0402(f"R{ref}3", x + 1.90, y, f"{e24(sh):g}R", net_out,
                        sgn)
        # The line necks to the pads' own width through the site.  A full
        # fifty-ohm line is 1.85 mm wide and the shunt legs stand 1.17 mm off
        # its centre, so left at full width the copper would swallow them.
        self._add(rect(x - 1.90 - 0.31, y - 0.31, x - c, y + 0.31), net_in)
        self._add(rect(x + c, y - 0.31, x + 1.90 + 0.31, y + 0.31), net_out)
        # the line comes back to full width only once it is clear of the
        # shunt legs, which stand 1.17 mm off the centre
        for dx, nt in ((-2.21, net_in), (2.21, net_out)):
            self.taper(nt, x + dx + (2.2 if dx > 0 else -2.2), x + dx, y,
                       w_pin=0.62, steps=3)
        self.parts.append(dict(kind="PADSET", ref=ref, x=x, y=y, db=db,
                               shunt=e24(sh), series=e24(se)))

    def smd_part(self, ref, x, y, pads, mpn="", pkg="", value="", dnp=False,
                 flip_y=False, note=""):
        """Any bought part, from a pad list of (number, dx, dy, w, h, net)."""
        out = []
        for num, dx, dy, w, h, nt in pads:
            if flip_y:
                dy = -dy
            self._add(rect(x + dx - w / 2, y + dy - h / 2,
                           x + dx + w / 2, y + dy + h / 2), nt, True)
            out.append((num, x + dx, y + dy, w, h, nt))
        self.parts.append(dict(kind="SMD", ref=ref, x=x, y=y, mpn=mpn,
                               pkg=pkg, value=value, dnp=dnp,
                               pads=[[n, round(a, 4), round(b, 4),
                                      round(cc, 4), round(d, 4), e]
                                     for n, a, b, cc, d, e in out]))
        if not dnp:
            self._bom(ref, value or mpn, pkg, mpn, note)
        return out

    def header(self, ref, x, y, cols, rows, names, pitch=2.54):
        """A pin header, drawn as plated holes on a grid."""
        pads = []
        n = 1
        for r in range(rows):
            for c in range(cols):
                px = x + (c - (cols - 1) / 2.0) * pitch
                py = y + ((rows - 1) / 2.0 - r) * pitch
                nt = names[n - 1]
                self._add(rect(px - 0.85, py - 0.85, px + 0.85, py + 0.85),
                          nt, True)
                pads.append((n, px, py, nt))
                n += 1
        self.parts.append(dict(kind="HDR", ref=ref, x=x, y=y, pitch=pitch,
                               pads=[[a, round(b, 3), round(c, 3), d]
                                     for a, b, c, d in pads],
                               mpn=f"{cols}x{rows} 2.54 mm header"))
        self._bom(ref, f"{cols}x{rows} header", "2.54 mm", "", "")
        return {nm: (px, py) for _, px, py, nm in pads}

    # ---------------------------------------------------------- printed bits
    def taper(self, net, x_wide, x_pin, y, w_pin=0.30, steps=5):
        """Bring a fifty-ohm line down to something a QFN pin can take.

        A 1.85 mm track is three and a half times the package's pin pitch, so
        run into the pin at full width it lands on the two earthed pins either
        side.  It narrows over two millimetres instead -- short enough at
        5.8 GHz that the impedance bump is worth a fraction of a decibel, and
        drawn as steps because everything else on this board is rectangles.
        """
        d = (x_pin - x_wide) / steps
        for k in range(steps):
            f = (k + 1.0) / steps
            w = self.W50 + (w_pin - self.W50) * f
            x0, x1 = sorted((x_wide + k * d, x_wide + (k + 1) * d))
            self._add(rect(x0, y - w / 2, x1, y + w / 2), net)
        return x_pin

    def bias_feed(self, ref, pt, sgn, net_rail, side=-1, spec=BIAS):
        """Supply onto an amplifier's output pin, without the signal escaping.

        A quarter wave of thin line away from the pin, ending on a pad that
        the decoupling capacitors make into a radio-frequency short.  The line
        turns that short into an open circuit right where the signal is.
        Beyond it the supply is ordinary direct current.
        """
        x, y = pt
        wl, ll = spec["w_mm"], spec["len_mm"]
        y_end = y + sgn * ll
        self._add(rect(x - wl / 2, min(y, y_end), x + wl / 2, max(y, y_end)),
                  net_rail)
        self._add(rect(x - 0.31, min(y_end, y_end + sgn * 0.62),
                       x + 0.31, max(y_end, y_end + sgn * 0.62)), net_rail)
        self.parts.append(dict(kind="BIAS", ref=ref, x=x, y=y_end, w=wl,
                               length=ll, side=side))
        return (x, y_end)

    def filter_run(self, ref, x0, y, net, spec=LPF):
        """The printed low-pass, drawn section by section from the synthesis."""
        x = x0
        for s in spec["sections"]:
            self._add(rect(x, y - s["w_mm"] / 2, x + s["len_mm"],
                           y + s["w_mm"] / 2), net)
            x += s["len_mm"]
        self.parts.append(dict(kind="LPF", ref=ref, x=x0, y=y, length=x - x0,
                               sections=[[s["kind"], round(s["w_mm"], 4),
                                          round(s["len_mm"], 4)]
                                         for s in spec["sections"]]))
        return x

    def tap(self, ref, x0, y, sgn, net_thru, net_cpl, spec=TAP):
        """One coupled pair that samples both directions at once.

        Two lines side by side lift a hundredth of what goes past.  Which end
        it comes out of depends on which way it was travelling: what is going
        out appears at the far end of the sampling line, what is coming back
        appears at the near end.  So one piece of copper gives both a clean
        sample of the transmission -- for a power meter, or for the canceller
        if one is ever fitted -- and a warning that the antenna has fallen off.
        """
        w, s, L = spec["w_mm"], spec["s_mm"], spec["len_mm"]
        y_c = y + sgn * (w + s)
        self._add(rect(x0, y - w / 2, x0 + L, y + w / 2), net_thru)
        self._add(rect(x0, y_c - w / 2, x0 + L, y_c + w / 2), net_cpl)
        self.parts.append(dict(kind="TAP", ref=ref, x=x0 + L / 2, y=y,
                               coupling_db=spec["coupling_db"], w=w, gap=s,
                               length=L))
        # Which end is which is not a matter of taste.  A pair of coupled
        # lines sends what is going one way out of one end and what is coming
        # back out of the other, and the forward sample appears at the end
        # NEAREST THE INPUT, not the output.  Getting that round the wrong way
        # would have put the power meter on the reflection and the fault
        # detector on the transmission.
        return (x0, y_c), (x0 + L, y_c)          # forward end, reflected end

    def thermal_field(self, cx, cy, half, pitch=0.62, drill=0.30, pad=0.52,
                      under=0.70, exclude=0.0):
        """Holes under an amplifier, so its heat has somewhere to go.

        A gallium arsenide amplifier making three quarters of a watt of signal
        throws away five times that as heat, and the laminate under it is
        plastic.  What is left is the copper in these holes, so there are as
        many of them as will fit without touching a live pad -- a block right
        under the package's own metal pad, and a ring around it that the
        surface copper feeds sideways.
        """
        hx, hy = (half, half) if isinstance(half, (int, float)) else half
        obs = [o for o in self.obstacles() if o[4] != "GND"]
        n, n_under = 0, 0
        kx, ky = int(hx / pitch) + 1, int(hy / pitch) + 1
        for i in range(-kx, kx + 1):
            for j in range(-ky, ky + 1):
                vx, vy = cx + i * pitch, cy + j * pitch
                if abs(vx - cx) > hx + 1e-9 or abs(vy - cy) > hy + 1e-9:
                    continue
                if max(abs(vx - cx), abs(vy - cy)) < exclude:
                    continue
                if any(math.hypot(vx - ox, vy - oy) < (pad + op) / 2 + 0.10
                       for ox, oy, _, op in self.vias):
                    continue
                if not self.seg_clear(vx, vy, vx, vy, pad + 0.10, obs):
                    continue
                self._via(vx, vy, drill, pad, "GND")
                n += 1
                if abs(vx - cx) <= under and abs(vy - cy) <= under:
                    n_under += 1
        self.parts.append(dict(kind="THERMAL", x=cx, y=cy, n=n,
                               n_under=n_under, drill=drill))
        return n, n_under


# ------------------------------------------------------------- land patterns
def _sot89(net_in, net_out):
    """The driver's land, from the standard SOT-89 outline turned on its side.

    The three leads run along the top and the metal tab hangs below, so the
    signal goes in at the left lead and out at the right one and travels the
    way the rest of the board does.  The tab is the part's ground and its only
    route for heat, so it sits over its own field of holes.
    """
    return [(1, -1.50, 1.95, 0.90, 1.30, net_in),
            (2, 0.00, 1.8625, 0.90, 1.475, "GND"),
            (3, 1.50, 1.95, 0.90, 1.30, net_out),
            (4, 0.00, -0.4375, 1.733, 3.125, "GND")]


def _sot23(nets):
    return [(1, -0.95, -0.9375, 0.60, 1.475, nets[0]),
            (2, 0.95, -0.9375, 0.60, 1.475, nets[1]),
            (3, 0.00, 0.9375, 0.60, 1.475, nets[2])]


def _sot23_5(nets):
    return [(1, -0.95, -1.1375, 0.60, 1.325, nets[0]),
            (2, 0.00, -1.1375, 0.60, 1.325, nets[1]),
            (3, 0.95, -1.1375, 0.60, 1.325, nets[2]),
            (4, 0.95, 1.1375, 0.60, 1.325, nets[3]),
            (5, -0.95, 1.1375, 0.60, 1.325, nets[4])]


def _sot223(nets):
    return [(1, -2.30, -3.15, 1.50, 2.00, nets[0]),
            (2, 0.00, -3.15, 1.50, 2.00, nets[1]),
            (3, 2.30, -3.15, 1.50, 2.00, nets[2]),
            (2, 0.00, 3.15, 3.80, 2.00, nets[1])]


def _soic14(nets):
    out = [(i + 1, -2.70, 3.81 - i * 1.27, 1.95, 0.60, nets[i])
           for i in range(7)]
    return out + [(i + 8, 2.70, -3.81 + i * 1.27, 1.95, 0.60, nets[i + 7])
                  for i in range(7)]


def _chip(size, net_a, net_b, horiz=True):
    w, h, c = size
    if horiz:
        return [(1, -c, 0.0, w, h, net_a), (2, c, 0.0, w, h, net_b)]
    return [(1, 0.0, -c, h, w, net_a), (2, 0.0, c, h, w, net_b)]


R2512 = (1.60, 3.35, 1.50)
R1210 = (1.30, 2.70, 1.15)
SOD523 = (0.70, 0.80, 0.60)
TERM2 = 5.08




def _sot23_6(nets):
    """Six pins on a SOT-23, counted down the left and up the right.

    Written out flipped, because the amplifier that reads the current wants
    its positive input on the supply side of the shunt and its negative input
    on the load side, and on this board the supply arrives from the right.
    """
    return [(1, 1.1375, -0.95, 1.325, 0.60, nets[0]),
            (2, 1.1375, 0.00, 1.325, 0.60, nets[1]),
            (3, 1.1375, 0.95, 1.325, 0.60, nets[2]),
            (4, -1.1375, 0.95, 1.325, 0.60, nets[3]),
            (5, -1.1375, 0.00, 1.325, 0.60, nets[4]),
            (6, -1.1375, -0.95, 1.325, 0.60, nets[5])]


# ------------------------------------------------------------------- layout
# Everything below is placed by hand on a chosen grid rather than left to a
# search, because on two layers the underside is solid ground and unavailable
# for routing: every net lives on the surface, and the only way two of them
# never cross is to decide in advance which lane each one runs in.
X_PAD, X_C1 = 10.5, 16.4
X_DRV, X_BT1, X_C2 = 22.5, 28.4, 34.0
X_PA, X_BT2, X_C3 = 40.5, 46.8, 51.0
X_LPF, X_TAP_GAP = 54.0, 3.2
X_HDR = 91.0

# Lanes in the middle band, measured inward from each chain.  A lane's order
# follows where its signal comes from: what starts nearest the chain runs
# nearest the chain, so nothing has to climb over anything else.
D_TEMP, D_DET, D_V12 = 4.0, 10.0, 16.0
X_DROP_TEMP, X_DROP_DET = 88.0, 84.5


def channel(b, s, tag, y, inward):
    """One transmit chain, laid out left to right along its own centreline.

    inward is which way the middle of the board is: the supply feeds hang that
    way, into the band where all the power electronics live, and the signal
    keeps the outer edge to itself.
    """
    n_in, n_p1 = f"{tag}_IN", f"{tag}_P"
    n_di, n_d = f"{tag}_DI", f"{tag}_D"
    n_pi, n_f = f"{tag}_PI", f"{tag}_F"
    n_pa, n_cpl = f"{tag}_PA", f"{tag}_CPL"    # both coupler ends
    r12, r5 = f"V12{s}", f"V5{s}"

    b.sma_launch(f"{tag}_IN", (0.0, y), "left")
    b.line(PULLBACK, y, X_PAD - 3.11, y, n_in)
    b.pi_pad_site(s + "P", X_PAD, y, n_in, n_p1, inward)
    b.line(X_PAD + 3.11, y, X_C1 - 0.585, y, n_p1)
    b.series_0402(f"C{s}1", X_C1, y, "1n", n_p1, n_di,
                  mpn="GRM1555C1H102JA01D", note="blocking capacitor")

    # ---- driver: leads on the centreline, tab and its holes facing outward
    up = inward < 0
    y_drv = y + (1.95 if up else -1.95)
    y_tab = y_drv + (0.4375 if up else -0.4375)
    b.line(X_C1 + 0.585, y, X_DRV - 1.50, y, n_di)
    b.smd_part(f"U{s}1", X_DRV, y_drv, _sot89(n_di, n_d), mpn=DRIVER["mpn"],
               pkg="SOT-89", flip_y=up, note="0.05-6 GHz driver")
    b.thermal_field(X_DRV, y_tab, (0.55, 1.20), pitch=0.55, drill=0.30,
                    pad=0.50, under=1.30)
    b.line(X_DRV + 1.50, y, X_BT1, y, n_d)
    b.bias_feed(f"L{s}1", (X_BT1, y), inward, r5)
    b.line(X_BT1, y, X_C2 - 0.585, y, n_d)
    b.series_0402(f"C{s}2", X_C2, y, "1n", n_d, n_pi,
                  mpn="GRM1555C1H102JA01D", note="blocking capacitor")

    # ---- final.  Two fields of holes, not one: a close block of nine
    # directly under the package's own metal pad takes heat straight down, and
    # a wider ring takes what the surface copper carries sideways.  The land's
    # single automatic hole is turned off so it does not crowd the block out.
    kind = f"FINAL_{s}"
    pins = dict(FINAL["pins"])
    pins.update({2: n_pi, 8: n_pa, 12: f"{tag}_IADJ"})
    b.PARTS[kind] = dict(FINAL, pins=pins)
    b.amplifier(f"U{s}2", kind, X_PA, y, thermal=False)
    b._bom(f"U{s}2", FINAL["mpn"], "QFN12 3x3", FINAL["mpn"],
           "the watt; matched to fifty ohms inside the package")
    # the current-setting pin, left open, which is the state the part was
    # characterised in, with two empty pads in case it ever wants trimming
    y_iadj = y + FINAL["land_half"] + 1.20
    b.line(X_PA - FINAL["pitch"], y + FINAL["land_half"],
           X_PA - FINAL["pitch"], y_iadj, f"{tag}_IADJ", w=0.30)
    b.smd_part(f"R{s}I", X_PA - FINAL["pitch"], y_iadj + 0.60,
               _chip((0.62, 0.62, 0.585), f"{tag}_IADJ", "GND", horiz=False),
               pkg="0402", value="DNP", dnp=True)
    b.line(X_C2 + 0.585, y, X_PA - FINAL["land_half"] - 2.4, y, n_pi)
    b.taper(n_pi, X_PA - FINAL["land_half"] - 2.4, X_PA - FINAL["land_half"], y)
    b.taper(n_pa, X_PA + FINAL["land_half"] + 2.4, X_PA + FINAL["land_half"], y)
    b.line(X_PA + FINAL["land_half"] + 2.4, y, X_BT2, y, n_pa)
    _, n_under = b.thermal_field(X_PA, y, 0.60, pitch=0.60, drill=0.30,
                                 pad=0.50, under=0.65)
    n_ring, _ = b.thermal_field(X_PA, y, 2.60, pitch=0.62, under=0.0,
                                exclude=0.95)
    b.bias_feed(f"L{s}2", (X_BT2, y), inward, r12)
    b.line(X_BT2, y, X_C3 - 0.585, y, n_pa)
    b.series_0402(f"C{s}3", X_C3, y, "1n", n_pa, n_f,
                  mpn="GRM1555C1H102JA01D", note="blocking capacitor")

    # ---- filter, tap, out.  The sampling line runs on the inward side, so
    # what it feeds can reach the middle of the board without having to climb
    # over the transmit line itself.
    b.line(X_C3 + 0.585, y, X_LPF, y, n_f)
    x_end = b.filter_run(f"FL{s}", X_LPF, y, n_f)
    b.line(x_end, y, x_end + X_TAP_GAP, y, n_f)
    fwd, rev = b.tap(f"DC{s}", x_end + X_TAP_GAP, y, inward, n_f, n_cpl)
    b.line(fwd[0] + TAP["len_mm"], y, BW - PULLBACK, y, n_f)
    b.sma_launch(f"{tag}_OUT", (BW, y), "right")

    # the forward sample goes out to its own connector, round the outside of
    # the detector so the two never meet
    y_fwd = y + inward * 8.0
    b.sma_launch(f"{tag}_FWD", (BW, y_fwd), "right")
    # A coupler with one end left open is not a coupler, it is a pair of
    # reflections: the wave runs to the open connector, comes back, and adds
    # itself to the detector's reading with whatever phase it likes.  So the
    # sample port is terminated on the board and stays terminated until
    # somebody actually connects something, at which point this one resistor
    # comes off.
    x_t = BW - 7.0
    b.line(x_t, y_fwd, x_t, y_fwd + inward * 1.6, n_cpl, w=0.50)
    b.shunt_0402(f"R{s}F", x_t, y_fwd + inward * 1.6, "51R", n_cpl, inward,
                 note="terminates the sample port; REMOVE IT when a cable "
                      "goes on that connector")
    b.line(fwd[0], fwd[1], fwd[0], y_fwd, n_cpl)
    b.corner(fwd[0], y_fwd, n_cpl)
    b.line(fwd[0], y_fwd, BW - PULLBACK, y_fwd, n_cpl)
    return dict(rev=rev, fwd=fwd, n_ring=n_ring, n_under=n_under,
                rails=(r12, r5))


def detector(b, s, tag, port, inward):
    """Turns reflected power into a voltage a computer can read.

    The near end of the sampling line carries whatever is coming back from the
    antenna.  A diode that needs no supply of its own rectifies it, three
    parts smooth it, and out comes a slow voltage saying how much of the
    transmission is being returned.  With the antenna connected it sits near
    nothing; take the cable off and it jumps by more than ten decibels, which
    is the difference a watching computer is asked to notice.
    """
    x, y = port
    n_rf, n_v, n_o = f"{tag}_CPL", f"{tag}_DETV", f"DET{s}"
    y_run = y + inward * 2.0
    b.line(x, y, x, y_run, n_rf, w=0.60)
    b.shunt_0402(f"R{s}T", x, y_run, "51R", n_rf, inward,
                 note="the sampling line's own load; without it the coupler "
                      "is a pair of reflections")
    b.line(x, y_run, x + 3.40, y_run, n_rf, w=0.60)
    b.series_0402(f"D{s}1", x + 4.00, y_run, "SMS7630", n_rf, n_v,
                  mpn="SMS7630-079LF", note="zero-bias detector diode")
    xs = x + 4.00 + 0.585
    for k, (ref, val) in enumerate((("C", "100p"), ("R", "10k"),
                                    ("C", "10n"))):
        xk = xs + 1.20 + k * 1.70
        b.line(xs, y_run, xk, y_run, n_v, w=0.60)
        b.shunt_0402(f"{ref}{s}D{k}", xk, y_run, val, n_v, inward)
    xr = xs + 1.20 + 2 * 1.70
    b.series_0402(f"R{s}O", xr + 1.80, y_run, "1k", n_v, n_o,
                  note="keeps the reading off the radio-frequency side")
    b.line(xr, y_run, xr + 1.215, y_run, n_v, w=0.60)
    b.line(xr + 2.385, y_run, xr + 3.60, y_run, n_o, w=0.40)
    return (xr + 3.60, y_run)


def power_block(b, s, yb, y_bt, inward):
    """One channel's supply: a current shunt, twelve volts, and five volts.

    Strictly left to right, so the two rails never have to cross on a board
    whose underside is solid ground and therefore not available.  Reading from
    the right: twelve volts arrives, passes the shunt that measures it, and
    the node beyond both climbs to the amplifier and carries on left into the
    dropper and the regulator that make five volts for the driver.
    """
    r12, r5, n_i = f"V12{s}", f"V5{s}", f"V5{s}_I"
    b.smd_part(f"R{s}S", 44.0, yb, _chip(R2512, r12, "V12"), pkg="2512",
               value="0R1", note="reads the final amplifier's supply current")
    b.smd_part(f"R{s}V", 38.5, yb, _chip(R2512, n_i, r12), pkg="2512",
               value="27R", note="takes most of the regulator's heat off it")
    b.line(39.5, yb, 42.5, yb, r12, w=0.70)
    b.smd_part(f"U{s}3", 30.0, yb - inward * 3.15, _sot223(("GND", r5, n_i)),
               mpn="AMS1117-5.0", pkg="SOT-223", flip_y=(inward > 0),
               note="five volts for the driver")
    b.line(32.3, yb, 37.0, yb, n_i, w=0.70)
    b.shunt_0402(f"C{s}5", 34.22, yb, "10u", n_i, -inward, pkg="0805",
                 note="16 V part; ten microfarads does not come in an 0402")

    # the amplifier that reads the shunt, right under it, so the two wires
    # that measure a forty-millivolt difference are as short as they can be
    y_ina = yb + inward * 3.8
    b.smd_part(f"U{s}4", 44.0, y_ina,
               _sot23_6((f"IMON{s}", "GND", "V12", r12, "GND", r5)),
               mpn="INA181A2IDBVR", pkg="SOT-23-6", flip_y=(inward > 0),
               note="turns the shunt's forty millivolts into two volts")
    b.line(45.5, yb, 45.5, y_ina - inward * 0.95, "V12", w=0.50)
    b.line(42.5, yb, 42.5, y_ina - inward * 0.95, r12, w=0.50)

    y_tab = yb - inward * 6.30
    b.shunt_0402(f"C{s}6", 25.0, y_tab, "10u", r5, inward, pkg="1210")
    b.shunt_0402(f"C{s}7", 22.5, y_tab, "100n", r5, inward)
    b.line(20.0, y_tab, 31.9, y_tab, r5, w=0.80)
    # five volts also feeds the current amplifier, and gets there round the
    # outside of the regulator rather than across its own pins
    b.line(20.0, y_ina + inward * 0.95, 20.0, y_tab, r5, w=0.50)
    b.line(20.0, y_ina + inward * 0.95, 42.86, y_ina + inward * 0.95, r5,
           w=0.50)

    # up to the printed feeds
    b.line(30.0, y_tab, 30.0, y_bt - inward * 2.0, r5, w=0.50)
    b.corner(30.0, y_bt - inward * 2.0, r5, w=0.50)
    b.line(30.0, y_bt - inward * 2.0, X_BT1 + 2.0, y_bt - inward * 2.0, r5,
           w=0.50)
    b.corner(X_BT1 + 2.0, y_bt - inward * 2.0, r5, w=0.50)
    b.line(X_BT1 + 2.0, y_bt - inward * 2.0, X_BT1 + 2.0, y_bt, r5, w=0.50)
    b.line(X_BT1, y_bt, X_BT1 + 2.0, y_bt, r5, w=0.50)
    y_up = yb - inward * 7.0
    b.line(42.5, yb, 42.5, y_up, r12, w=0.70)
    b.corner(42.5, y_up, r12, w=0.70)
    b.line(42.5, y_up, X_BT2, y_up, r12, w=0.70)
    b.corner(X_BT2, y_up, r12, w=0.70)
    b.line(X_BT2, y_up, X_BT2, y_bt, r12, w=0.70)

    # The decoupling IS the short circuit the quarter-wave feed works
    # against, so the first capacitor sits on the end of the feed itself with
    # its own ground via, and the slower ones follow along the rail.
    y_dec = y_bt + inward * 3.4
    b.shunt_0402(f"C{s}80", X_BT2, y_bt + inward * 0.62, "100p", r12, +1,
                 axis="x",
                 note="makes the radio-frequency short the quarter-wave feed "
                      "works against; keep its ground via short")
    b.line(X_BT2, y_bt, X_BT2, y_dec, r12, w=0.70)
    b.line(X_BT2, y_dec, X_BT2 + 9.2, y_dec, r12, w=0.70)
    for k, val in enumerate(("1n", "100n"), start=1):
        b.shunt_0402(f"C{s}8{k}", X_BT2 + 1.7 + (k - 1) * 1.7, y_dec, val,
                     r12, inward)
    b.shunt_0402(f"C{s}83", X_BT2 + 8.0, y_dec, "10u", r12, inward,
                 pkg="1210", note="25 V part on the twelve-volt rail")
    b.shunt_0402(f"C{s}90", X_BT1, y_bt + inward * 0.62, "100p", r5, -1,
                 axis="x", note="the same job on the driver's feed")
    b.line(X_BT1 - 12.0, y_dec, X_BT1, y_dec, r5, w=0.50)
    b.line(X_BT1, y_bt, X_BT1, y_dec, r5, w=0.50)
    for k, val in enumerate(("1n", "100n"), start=1):
        b.shunt_0402(f"C{s}9{k}", X_BT1 - 1.7 - (k - 1) * 1.7, y_dec, val, r5,
                     inward)
    b.shunt_0402(f"C{s}93", X_BT1 - 11.4, y_dec, "10u", r5, inward,
                 pkg="1210")


def build():
    """The whole board: two chains, their supplies, and what watches them."""
    b = PABoard()
    ch = {"A": channel(b, "A", "TX1", Y_A, -1),
          "B": channel(b, "B", "TX2", Y_B, +1)}
    y_bt = {"A": Y_A - BIAS["len_mm"], "B": Y_B + BIAS["len_mm"]}
    power_block(b, "A", 51.0, y_bt["A"], -1)
    power_block(b, "B", 33.0, y_bt["B"], +1)

    # ------------------------------------------------ power and control, right
    # Supply and control enter on the same edge, well away from all six radio
    # connectors, so nothing carrying direct current has to cross a chain.
    hdr = b.header("J5", X_HDR, 42.0, 2, 7,
                   ["EN", "GND", "GND", "DETA", "TEMPA", "GND",
                    "IMONA", "GND", "IMONB", "GND", "TEMPB", "GND",
                    "GND", "DETB"])
    b.smd_part("J1", 94.0, 57.0,
               [(1, 0.0, 2.54, 1.70, 1.70, "VIN"),
                (2, 0.0, -2.54, 1.70, 1.70, "GND")],
               pkg="5.08 mm screw terminal", value="2-way",
               note="twelve volts in, one and a quarter amps")
    b.smd_part("F1", 89.5, 59.54, _chip(R1210, "VF", "VIN"), pkg="1210",
               value="1.5 A", mpn="MF-MSMF150", note="resettable fuse")
    b.line(90.65, 59.54, 94.0, 59.54, "VIN", w=0.90)
    b.shunt_0402("D5", 85.5, 59.54, "SMBJ15A", "VF", +1, mpn="SMBJ15A",
                 pkg="SMB", note="clamps whatever the supply lead picks up")
    b.line(79.5, 59.54, 88.35, 59.54, "VF", w=0.90)
    # One transistor does two jobs: it blocks a supply connected backwards,
    # and it is the switch the enable line works.  Drain to the incoming side
    # and source to the board, so its own internal diode faces the right way.
    b.smd_part("Q3", 79.5, 58.60, _sot23(("V12G", "V12", "VF")),
               mpn="AO3401A", pkg="SOT-23",
               note="blocks a reversed supply and switches the board")
    b.line(80.45, 57.66, 82.0, 57.66, "V12", w=0.70)
    b.line(82.0, 41.0, 82.0, 57.66, "V12", w=0.90)
    b.line(82.0, 55.0, 90.0, 55.0, "V12", w=0.90)
    for k, (val, dx) in enumerate((("22u", 0.0), ("22u", 3.2),
                                   ("100n", 5.8))):
        b.shunt_0402(f"C2{k}", 84.0 + dx, 55.0, val, "V12", +1,
                     pkg="1210" if val == "22u" else "0402")
    # the gate network, in the clear space below the switch: one rail for the
    # gate, one for the supply, and three parts bridging between them
    b.line(78.55, 53.0, 78.55, 57.66, "V12G", w=0.35)
    b.line(69.0, 53.0, 78.55, 53.0, "V12G", w=0.35)
    b.line(69.0, 48.0, 82.0, 48.0, "V12", w=0.50)
    for ref, x, val, mpn, note in (
            ("R1", 76.0, "100k", "", "holds the switch on"),
            ("C1", 73.5, "100n", "",
             "brings the switch up over ten milliseconds"),
            ("D6", 71.0, "10V", "BZX84C10",
             "keeps the transistor's gate inside its rating")):
        b.series_0402(ref, x, 50.5, val, "V12", "V12G", horiz=False,
                      mpn=mpn, note=note)
        b.line(x, 51.085, x, 53.0, "V12G", w=0.35)
        b.line(x, 48.0, x, 49.915, "V12", w=0.35)
    b.smd_part("Q4", 67.0, 51.06, _sot23(("EN", "GND", "V12G")),
               mpn="2N7002", pkg="SOT-23",
               note="holds the switch on; the enable line lets go of it")
    b.line(67.0, 52.0, 67.0, 53.0, "V12G", w=0.35)
    b.line(67.0, 53.0, 69.0, 53.0, "V12G", w=0.35)
    b.series_0402("R2", 61.0, 50.12, "100k", "VF", "EN")
    b.shunt_0402("R3", 63.5, 50.12, "47k", "EN", -1)
    b.line(61.585, 50.12, 66.05, 50.12, "EN", w=0.35)
    b.line(60.415, 50.12, 59.0, 50.12, "VF", w=0.50)
    b.line(59.0, 50.12, 59.0, 61.0, "VF", w=0.50)
    b.line(59.0, 61.0, 79.5, 61.0, "VF", w=0.50)
    b.line(79.5, 59.54, 79.5, 61.0, "VF", w=0.50)

    # ------------------------------------------------------------ detectors
    ch["A"]["det"] = detector(b, "A", "TX1", ch["A"]["rev"], -1)
    ch["B"]["det"] = detector(b, "B", "TX2", ch["B"]["rev"], +1)
    ch["A"]["det_net"], ch["B"]["det_net"] = "DETA", "DETB"

    # ------------------------------------------------ a thermistor per chain
    # Two wires and nothing else: whatever reads it supplies its own pull-up,
    # so no supply rail has to be brought up here past everything else.
    for s, y, inward in (("A", Y_A, -1), ("B", Y_B, +1)):
        b.shunt_0402(f"RT{s}", 52.0, y + inward * D_TEMP, "10k NTC",
                     f"TEMP{s}", inward, mpn="NCP15XH103F03RC",
                     note="how hot the board is beside the amplifier")

    # -------------------------------------------- everything that is not radio
    # Each of these leaves the surface where it starts, drops straight to the
    # row it is going to, and crosses the board on the buried layer under a
    # ground plane.  The rows are dealt out so that a run always meets the
    # header above every run that started to its left, which is what lets
    # seven signals share one layer without a single crossing.
    for s, y, inward in (("A", Y_A, -1), ("B", Y_B, +1)):
        yb = 51.0 if inward < 0 else 33.0
        for net, src, stub in (
                (f"IMON{s}", (45.14, yb + inward * 4.75), (0.0, inward * 2.2)),
                (f"TEMP{s}", (52.0, y + inward * D_TEMP), (4.4, 0.0)),
                (ch[s]["det_net"], ch[s]["det"], (0.0, 0.0))):
            pin = hdr[net]
            a = (src[0] + stub[0], src[1] + stub[1])
            if stub != (0.0, 0.0):
                b.line(src[0], src[1], a[0], a[1], net, w=0.40)
            b.dc(net, [a, (a[0], pin[1]), pin], skip_last_via=True)
    b.line(63.5, 50.12, 65.0, 50.12, "EN", w=0.35)
    b.dc("EN", [(65.0, 50.12), (65.0, hdr["EN"][1]), hdr["EN"]],
         skip_last_via=True)

    # twelve volts across to both channels, on the surface, along the middle
    # where the buried layer has left it nothing to argue with
    b.line(47.0, 41.0, 82.0, 41.0, "V12", w=0.90)
    b.line(47.0, 33.0, 47.0, 51.0, "V12", w=0.90)
    b.line(45.5, 51.0, 47.0, 51.0, "V12", w=0.90)
    b.line(45.5, 33.0, 47.0, 33.0, "V12", w=0.90)

    # ----------------------------------------------------------- mechanical
    # Two of the six bolts sit as close to the final amplifiers as the copper
    # allows, because that is where the heat has to leave the board.
    for x, y in ((5.0, 5.0), (5.0, 79.0), (95.0, 5.0), (95.0, 79.0),
                 (36.5, 62.0), (36.5, 22.0)):
        b.mounts.append((x, y, MOUNT_D, MOUNT_PAD))
    b.unify_nets()
    b.pour()
    # the underside is left bare under each amplifier, so its heat meets the
    # chassis through metal rather than through solder resist
    b.mask_bot = []
    for y in (Y_A, Y_B):
        b.mask_bot.append([X_PA - 3.6, y - 3.6, X_PA + 3.6, y + 3.6])
        b.mask_bot.append([X_DRV - 2.6, y - 3.4, X_DRV + 2.6, y + 3.4])
    for x, y in ((4.0, 42.0), (BW - 4.0, 42.0), (50.0, 81.0)):
        b.fiducial(x, y)
    b.labels += [
        (16.0, 47.5, "5.8 GHz RADAR TRANSMIT AMPLIFIER", 1.2, "silk"),
        (16.0, 44.5, "ZYF300CA-P 0.76 mm / 4 layer / Ag", 0.9, "silk"),
        (16.0, 41.5, "12 V 1.25 A   J5 = enable + monitors", 0.9, "silk"),
        (10.0, 56.0, "CHANNEL A   0.75 W", 1.1, "silk"),
        (10.0, 28.0, "CHANNEL B   0.75 W", 1.1, "silk"),
    ]
    # the parts that are not soldered to the board but are needed to build it
    for nm, x, y, side, _dy in b.ports:
        b._bom(nm, "142-0701-801", "SMA end launch", "Cinch 142-0701-801",
               f"{side} edge, 0.062 in board")
    b._bom("H1-H6", "M3 x 6 pan head and washer", "M3", "",
           "two of the six sit beside the amplifiers and carry their heat")
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
    d = dissipation("p1db")
    t = thermal(min(ch["A"]["n_under"], ch["B"]["n_under"]),
                min(ch["A"]["n_ring"], ch["B"]["n_ring"]))
    print("5.8 GHz transmit power amplifier, two channels\n")
    print(f"  board {BW:g} x {BH:g} mm, {STACK['layers']} layer, "
          f"{STACK['total_mm']:.3f} mm thick, immersion silver")
    print(f"  {SUB.name} {SUB.h*1e3:.2f} mm on top, so the printed parts see "
          f"the same reference as the arrays do")
    print(f"  50 ohm line {W50:.4f} mm wide, losing "
          f"{LOSS_MM*1000:.2f} dB per metre at {F0/1e9:.3f} GHz\n")

    print("  the chain, one channel, connector to connector")
    print(f"    {'level pad, fitted':<30} {-c['pad_db']:+7.2f} dB")
    print(f"    {DRIVER['mpn'] + ' driver':<30} {DRIVER['gain_db']:+7.2f} dB"
          f"   its own limit is {DRIVER['p1db_dbm']:+.1f} dBm and it runs "
          f"{c['driver_backoff_db']:.1f} dB below it")
    print(f"    {FINAL['mpn'] + ' final':<30} {FINAL['gain_db']:+7.2f} dB"
          f"   its own limit is {FINAL['p1db_dbm']:+.1f} dBm and it runs "
          f"there")
    print(f"    {'line, blocks, filter, tap':<30} "
          f"{-(c['loss_in_db'] - c['pad_db'] + c['loss_mid_db'] + c['loss_out_db']):+7.2f} dB")
    print(f"    {'net':<30} {c['gain_db']:+7.2f} dB\n")
    print(f"    drive needed for full output   {c['drive_for_full_dbm']:+6.2f}"
          f" dBm  ({c['drive_for_full_worst_dbm']:+.2f} on worst-case parts)")
    print(f"    output at the connector        "
          f"{c['out_connector_dbm']:+6.2f} dBm  "
          f"= {c['out_connector_w']*1000:.0f} mW")
    print(f"    if driven into saturation      "
          f"{c['sat_connector_dbm']:+6.2f} dBm")
    print(f"    the two channels together      {c['pair_dbm']:+6.2f} dBm  "
          f"= {2*c['out_connector_w']:.2f} W\n")

    print("  heat, and none of it stops: a chirping radar transmits without a break")
    print(f"    final amplifier                {d['final_w']:5.2f} W   "
          f"({d['pae_pct']:.0f} per cent of its supply becomes signal)")
    print(f"    driver                         {d['driver_w']:5.2f} W")
    print(f"    dropper and regulator          "
          f"{d['predrop_w'] + d['ldo_w']:5.2f} W")
    print(f"    the final is rated for         {FINAL['pdiss_max_w']:5.2f} W  "
          f" ({d['headroom_w']:.2f} W in hand)")
    print(f"    the whole board draws          "
          f"{2*d['channel_w']:5.2f} W from twelve volts, "
          f"{2*(d['final_i_a'] + DRIVER['id_ma']/1000):.2f} A\n")
    print(f"    {t['n_under']} holes under the package itself and "
          f"{t['n_ring']} in the ring around it")
    print(f"      straight down                {t['r_under']:5.1f} C/W")
    print(f"      sideways then down           {t['r_ring']:5.1f} C/W")
    print(f"      the two in parallel          {t['r_top']:5.1f} C/W")
    print(f"      on down to the chassis face  {t['r_deep']:5.1f} C/W")
    print(f"      through the compound         {t['r_iface']:5.1f} C/W")
    print(f"    ground lead reaches {t['t_lead_c']:.0f} C in a 25 C room, "
          f"{t['lead_margin_c']:.0f} C below its limit;")
    print(f"    the die reaches {t['t_die_c']:.0f} C, "
          f"{t['die_margin_c']:.0f} C below its.  Good to a "
          f"{t['max_ambient_c']:.0f} C room.\n")

    print("  what is printed instead of bought")
    print(f"    supply feed      {BIAS['w_mm']:.3f} mm of "
          f"{BIAS['z0']:.0f} ohm line, {BIAS['len_mm']:.3f} mm long, onto the "
          f"decoupling")
    print(f"                     a quarter wave is {BIAS['len_ideal_mm']:.3f}"
          f" mm; the extra {BIAS['junction_mm']:.3f} mm is the two junctions,"
          f" measured")
    print(f"                     the capacitors are a "
          f"{BIAS['z_short_ohm']:.1f} ohm short, which the line turns into "
          f"{BIAS['z_seen_ohm']/1000:.1f} kohm")
    print(f"                     across the signal, costing "
          f"{BIAS['insertion_db']:.3f} dB")
    print(f"                     {BIAS['r_dc_ohm']*1000:.0f} milliohm at "
          f"direct current, {BIAS['r_dc_ohm']*0.4*1000:.0f} mV at "
          f"four hundred milliamps")
    print(f"    harmonic filter  {LPF['order']} sections, "
          f"{LPF['length_mm']:.2f} mm long, drawn for a "
          f"{LPF['fc_hz']/1e9:.2f} GHz corner so the copper lands on "
          f"{LPF['fc_realised_hz']/1e9:.2f}")
    for i, s in enumerate(LPF["sections"]):
        print(f"                     {'wide' if s['kind'] == 'C' else 'thin'}"
              f"  {s['w_mm']:6.3f} x {s['len_mm']:6.3f} mm  "
              f"{s['z0']:6.1f} ohm  {s['theta_deg']:5.1f} deg")
    print(f"                     flat to {F_HI/1e9:.3f} GHz, "
          f"{LPF['reject']['2f0']:.0f} dB down at {2*F0/1e9:.1f} GHz "
          f"and {LPF['reject']['3f0']:.0f} dB at {3*F0/1e9:.1f} GHz")
    print(f"                     the amplifier's second harmonic is "
          f"{FINAL['h2_dbc']:.0f} dBc, so it leaves the board at "
          f"{FINAL['h2_dbc'] - LPF['reject']['2f0']:.0f} dBc")
    print(f"    sampling tap     {TAP['coupling_db']:.2f} dB, width "
          f"{TAP['w_mm']:.3f} mm, gap {TAP['s_mm']:.3f} mm, length "
          f"{TAP['len_mm']:.3f} mm")
    print(f"                     forward sample "
          f"{c['out_connector_dbm'] + TAP['coupling_db']:+.1f} dBm at its "
          f"own connector, costing {-10*math.log10(1-10**(TAP['coupling_db']/10)):.3f} dB")
    print(f"    level pad        {PAD_DEFAULT_DB:.0f} dB: "
          f"{e24(PAD_SHUNT):g} / {e24(PAD_SERIES):g} / {e24(PAD_SHUNT):g} ohm,"
          f" and a path to ground for static\n")

    print("  measured on the copper, not calculated (sim/pa_printed.py)")
    m = MEASURED
    print(f"    filter        {m['filter']['loss_5800']:.2f} dB through at "
          f"5.800 GHz, match {m['filter']['match_db']:.1f} dB,")
    print(f"                  {-m['filter']['rej_2f0']:.0f} dB down at "
          f"11.6 GHz and {-m['filter']['rej_3f0']:.0f} dB at 17.4 GHz")
    print(f"    sampling tap  {m['tap']['coupling_db']:.2f} dB coupled, "
          f"{-m['tap']['through_db']:.2f} dB through, "
          f"{m['tap']['directivity_db']:.1f} dB of directivity")
    print(f"    supply feed   {m['bias']['loss_db']:.2f} dB through, match "
          f"{m['bias']['match_db']:.1f} dB\n")
    print("  the level pad decides what the radio has to supply")
    for db in (0.0, 3.0, 6.0, 9.0):
        cc = chain(db)
        print(f"    {db:.0f} dB fitted   radio must give "
              f"{cc['drive_for_full_dbm']:+5.2f} dBm, and at the radio's own "
              f"maximum the")
        print(f"                  amplifier is driven "
              f"{9.0 - cc['drive_for_full_dbm']:+5.1f} dB past its "
              f"compression point")
    print(f"    fitted: {PAD_DEFAULT_DB:.0f} dB\n")
    print("  what the four monitor pins say, per channel")
    print(f"    supply current   {R_SENSE * G_SENSE:.1f} V per amp: "
          f"{R_SENSE * G_SENSE * FINAL['idq_ma'] / 1000:.2f} V idling, "
          f"{R_SENSE * G_SENSE * FINAL['id_p1db_ma'] / 1000:.2f} V at full "
          f"output.  Above "
          f"{R_SENSE * G_SENSE * TRIP_MA / 1000:.1f} V something is wrong")
    print(f"    reflected power  a diode across the sampling line's far end; "
          f"near nothing with the")
    print(f"                     antenna on, and it jumps when the cable "
          f"comes off")
    print(f"    temperature      10 kohm at 25 C beside the amplifier, "
          f"falling as it warms;")
    print(f"                     the reading device supplies its own pull-up")
    print(f"    enable           held high on the board.  Pull it to ground "
          f"and everything")
    print(f"                     stops, over about ten milliseconds\n")
    print("  what this asks of the rest of the radar")
    for g, what in ((0.0, "radio straight onto the receive array"),
                    (15.0, "with a 15 dB amplifier in front of it")):
        r = separation_for(c["out_connector_dbm"], g)
        print(f"    {what:<38} {r*1000:.0f} mm between the two arrays")
    print("    (measured -42.17 dB at 92.1 mm on the undivided board; the path")
    print("     through the air thins as the square of the distance)")

    kinds = {}
    for p in b.parts:
        kinds[p["kind"]] = kinds.get(p["kind"], 0) + 1
    print("\n  parts: " + ", ".join(f"{v} x {k}"
                                    for k, v in sorted(kinds.items())))
    print(f"  {len(b.bom)} fitted, copper polygons {len(b.top)}, "
          f"vias {len(b.vias)}, holes {len(b.mounts)}")
    print("  connectors:")
    for n, x, y, s, _dy in b.ports:
        print(f"    {n:10} {s:6} edge at x {x:7.3f}  y {y:7.3f}")


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
        "printed": {"bias": BIAS, "lpf": LPF, "tap": TAP,
                    "pad_db": PAD_DEFAULT_DB},
        "chain": chain(), "dissipation": dissipation("p1db"), "thermal": t,
        "parts_spec": {"final": {k: v for k, v in FINAL.items()
                                 if k != "pins"},
                       "driver": {k: v for k, v in DRIVER.items()
                                  if k != "pins"}},
    }, open(os.path.join(HERE, "pa_board.json"), "w"), indent=1)
    print("\nwrote pa_board.json")
