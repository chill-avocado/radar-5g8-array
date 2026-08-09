"""One board, both directions, bolted straight onto the radio.

The radar has four radio connectors in a row on the front of the USRP B210 --
transmit, receive, receive, transmit -- and until now it had two separate
amplifier boards hanging off them on four cables.  This is both of those
boards on one piece of laminate, with four plugs on its radio edge that mate
directly with those four sockets.  No cables between the board and the radio,
half the connectors, one power feed, one enclosure.

The whole question is whether the transmitters can share laminate with the
receivers.  Each transmit chain ends at three quarters of a watt, and the
receiver beside it starts to compress at fifteen microwatts -- a ratio of
fifty thousand million to one, on the same piece of board.  openEMS was asked
directly (sim/fe_isolation.py): two fifty-ohm lines running the length of the
board with a stitched ground wall between them couple at -58.9 dB at 23 mm
apart.  The chains here sit 29 mm apart and only their last third carries full
power, so what crosses the board is smaller than what already crosses the air
between the two antenna arrays.  The board adds about a decibel to a leak the
radar was built to live with, and that decibel is bought back by moving the
arrays seven centimetres further apart.

The layout falls straight out of the connector order:

    radio edge          the board            antenna edge
    ------------------------------------------------------
    TRX1  --.                                       .-- TX1 out
             `--- transmit chain A ----------------'
    RX1   ------ receive chain C  <---------------------- RX1 in
    RX2   ------ receive chain D  <---------------------- RX2 in
    TRX2  --.                                       .-- TX2 out
             `--- transmit chain B ----------------'

Transmit fans out to the two outer edges, receive stays in the middle, and the
three gaps between them carry the power and the monitoring.  Nothing crosses
anything on the surface; everything that is not radio crosses on a buried
layer under a ground plane.

The radio edge is cut into four fingers, one per connector, so that four rigid
plugs in a row can meet four sockets in a row without any of them being
levered.  That is the one mechanical thing a board like this has to get right.
"""

import json
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import pcbgen                                                # noqa: E402
import pa_board as PA                                        # noqa: E402
import lna_board as LN                                       # noqa: E402
from geom import rect                                        # noqa: E402
from pa_board import (SUB, W50, LOSS_MM, F0, F_LO, F_HI, LAM_G,      # noqa
                      e24, pi_pad, _sot89, _sot23, _sot223, _chip,
                      _sot23_6, R2512, R1210, FINAL, DRIVER, BIAS, LPF,
                      TAP, STACK, LAUNCH_GAP, W_DC, MEASURED,
                      via_theta, spread_theta, separation_for)
from lna_board import (LNA, LIMITER, CLAMP, LIM, CLP, _smp1330,       # noqa
                       _sod523, B210_P1DB_DBM, B210_MAX_DBM, B210_NF_DB,
                       CABLE_DB)


# ===================================================================== outline
BW, BH = 100.0, 100.0
PULLBACK = 0.15
MOUNT_D, MOUNT_PAD = 3.20, 6.00

# The radio's four sockets, and therefore our four plugs.  Fifteen millimetre
# pitch, measured off the B210's front panel.
PITCH = 15.0
Y_J = (27.5, 42.5, 57.5, 72.5)          # TRX1, RX1, RX2, TRX2, bottom to top

# Where each chain actually runs.  Transmit splays to the outer edges, receive
# stays central, and that is what buys the isolation.
Y_TX1, Y_RX1, Y_RX2, Y_TX2 = 7.0, 36.0, 64.0, 93.0
X_JOG = 18.0                            # where a chain steps to its own line

# Four fingers on the radio edge.  Each connector then sits on its own tongue
# of board and can take up a couple of tenths of a millimetre of pitch error
# without any of the four being strained.  Without these, four rigid coaxial
# joints in a line have to be perfect all at once, and they never are.
SLOT_Y = (35.0, 50.0, 65.0)
SLOT_DEPTH, SLOT_W = 15.0, 1.60


# ================================================================== the chains
# Transmit: the same seven stages the separate transmit board carried, at the
# same spacings, shifted right to clear the finger slots and the step down to
# the outer line.
X_PAD, X_C1 = 22.0, 27.9
X_DRV, X_BT1, X_C2 = 34.0, 39.9, 45.5
X_PA, X_BT2, X_C3 = 52.0, 58.3, 62.5
X_LPF, X_TAP_GAP = 65.5, 3.2

# Receive, laid out right to left because its signal travels that way: in from
# the antenna at the far edge, out to the radio at the near one.
XR_C1, XR_LIM = 93.0, 74.0
XR_C2, XR_LNA, XR_BT, XR_C3 = 72.7, 66.0, 58.0, 55.0
XR_TAP, XR_PAD, XR_CLAMP, XR_C4 = 45.0, 39.0, 32.0, 27.0

# Both transmit channels keep their own five volts, because a single regulator
# feeding both drivers would need a dropper resistor hot enough to matter.
# Both receive channels share one, because a hundred and twenty milliamps is
# small enough that sharing costs nothing and saves a regulator, a monitor, a
# dropper and about a square centimetre of board.
Y_PWR_A, Y_PWR_B = 28.0, 72.0
Y_RXPWR = 50.0
RX_PAD_DEFAULT_DB = 8.0
TX_PAD_DEFAULT_DB = 6.0


class FEBoard(LN.LNABoard):
    """Both boards' machinery on one outline.

    The transmit board's class already knew how to place a part in the line,
    print a filter, print a coupler and print a supply feed; the receive
    board's added the limiter and the clamp.  This inherits the pair of them
    and changes nothing but the size of the board they are drawn on.
    """

    def __init__(self):
        pcbgen.Board.__init__(self, BW, BH, W50, LAUNCH_GAP,
                              parts={"FINAL": FINAL, "DRIVER": DRIVER,
                                     "LNA": LNA})
        self.bom = []
        self.bot, self.bot_net = [], []
        self.mask_bot = []
        self.slots = []

    def finger_slot(self, y, depth=SLOT_DEPTH, w=SLOT_W):
        self.slots.append(dict(y=y, depth=depth, width=w))

    def jog(self, net, x, y0, y1, w=None):
        """Step a chain from its connector's height to its own centreline."""
        if abs(y1 - y0) < 1e-9:
            return
        self.corner(x, y0, net, w=w)
        self.line(x, y0, x, y1, net, w=w)
        self.corner(x, y1, net, w=w)


# ------------------------------------------------------------ transmit chain
def tx_channel(b, s, tag, y, y_j, inward):
    """One transmit chain: radio in on the left, antenna out on the right.

    Level pad, driver, watt-class final, harmonic filter, sampling coupler.
    inward says which way the middle of the board is; the supply feeds and
    everything that is not radio hang that way, and the outer edge is left to
    the signal.
    """
    n_in, n_p1 = f"{tag}_IN", f"{tag}_P"
    n_di, n_d = f"{tag}_DI", f"{tag}_D"
    n_pi, n_f = f"{tag}_PI", f"{tag}_F"
    n_pa, n_cpl = f"{tag}_PA", f"{tag}_CPL"
    r12, r5 = f"V12{s}", f"V5{s}"

    # in from the radio, then down (or up) to this chain's own line
    b.sma_launch(f"{tag}_RADIO", (0.0, y_j), "left")
    b.line(PULLBACK, y_j, X_JOG, y_j, n_in)
    b.jog(n_in, X_JOG, y_j, y)
    b.line(X_JOG, y, X_PAD - 3.11, y, n_in)
    b.pi_pad_site(s + "P", X_PAD, y, n_in, n_p1, inward, db=TX_PAD_DEFAULT_DB)
    b.line(X_PAD + 3.11, y, X_C1 - 0.585, y, n_p1)
    b.series_0402(f"C{s}1", X_C1, y, "1n", n_p1, n_di,
                  mpn="GRM1555C1H102JA01D", note="blocking capacitor")

    # ---- driver: leads on the centreline, tab and its holes facing the edge
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

    # ---- the final.  Heat leaves in two parallel routes: a close block of
    # holes straight under the package's own metal pad, and a wider ring that
    # catches what the surface copper carries sideways.
    kind = f"FINAL_{s}"
    pins = dict(FINAL["pins"])
    pins.update({2: n_pi, 8: n_pa, 12: f"{tag}_IADJ"})
    b.PARTS[kind] = dict(FINAL, pins=pins)
    b.amplifier(f"U{s}2", kind, X_PA, y, thermal=False)
    b._bom(f"U{s}2", FINAL["mpn"], "QFN12 3x3", FINAL["mpn"],
           "the watt; matched to fifty ohms inside the package")
    y_iadj = y - inward * (FINAL["land_half"] + 1.20)
    b.line(X_PA - FINAL["pitch"], y - inward * FINAL["land_half"],
           X_PA - FINAL["pitch"], y_iadj, f"{tag}_IADJ", w=0.30)
    b.smd_part(f"R{s}I", X_PA - FINAL["pitch"], y_iadj - inward * 0.60,
               _chip((0.62, 0.62, 0.585), f"{tag}_IADJ", "GND", horiz=False),
               pkg="0402", value="DNP", dnp=True)
    b.line(X_C2 + 0.585, y, X_PA - FINAL["land_half"] - 2.4, y, n_pi)
    b.taper(n_pi, X_PA - FINAL["land_half"] - 2.4, X_PA - FINAL["land_half"],
            y)
    b.taper(n_pa, X_PA + FINAL["land_half"] + 2.4, X_PA + FINAL["land_half"],
            y)
    b.line(X_PA + FINAL["land_half"] + 2.4, y, X_BT2, y, n_pa)
    _, n_under = b.thermal_field(X_PA, y, 0.60, pitch=0.60, drill=0.30,
                                 pad=0.50, under=0.65)
    n_ring, _ = b.thermal_field(X_PA, y, 2.60, pitch=0.62, under=0.0,
                                exclude=0.95)
    b.bias_feed(f"L{s}2", (X_BT2, y), inward, r12)
    b.line(X_BT2, y, X_C3 - 0.585, y, n_pa)
    b.series_0402(f"C{s}3", X_C3, y, "1n", n_pa, n_f,
                  mpn="GRM1555C1H102JA01D", note="blocking capacitor")

    # ---- filter, coupler, out to the antenna
    b.line(X_C3 + 0.585, y, X_LPF, y, n_f)
    x_end = b.filter_run(f"FL{s}", X_LPF, y, n_f)
    b.line(x_end, y, x_end + X_TAP_GAP, y, n_f)
    fwd, rev = b.tap(f"DC{s}", x_end + X_TAP_GAP, y, inward, n_f, n_cpl)
    b.line(fwd[0] + TAP["len_mm"], y, BW - PULLBACK, y, n_f)
    b.sma_launch(f"{tag}_ANT", (BW, y), "right")

    # The forward end of the sampling line has to be loaded or the coupler is
    # not a coupler, it is a pair of reflections adding themselves to the
    # reflected-power reading with whatever phase they like.  It is loaded
    # here rather than brought out to a connector: the supply-current monitor
    # already says the amplifier is making its power, so the one thing worth
    # a detector is what is coming BACK.
    b.line(fwd[0], fwd[1], fwd[0], fwd[1] + inward * 1.8, n_cpl, w=0.50)
    b.shunt_0402(f"R{s}F", fwd[0], fwd[1] + inward * 1.8, "51R", n_cpl,
                 inward, note="loads the forward end of the sampling line")
    return dict(rev=rev, fwd=fwd, n_ring=n_ring, n_under=n_under,
                rails=(r12, r5))


# ------------------------------------------------------------- receive chain
def rx_channel(b, s, tag, y, y_j, inward, rail):
    """One receive chain: antenna in on the right, radio out on the left.

    Limiter, low-noise amplifier, sampling coupler, level pad, clamp.  It is
    drawn right to left because that is the way its signal travels, which
    keeps every connector on the edge it belongs to and means the receive
    line never has to double back past itself.
    """
    n_in, n_l = f"{tag}_IN", f"{tag}_L"
    n_li, n_lo = f"{tag}_LI", f"{tag}_LO"
    n_t, n_p, n_o = f"{tag}_T", f"{tag}_P", f"{tag}_OUT"
    n_cpl = f"{tag}_CPL"

    b.sma_launch(f"{tag}_ANT", (BW, y), "right")
    b.line(XR_C1 + 0.585, y, BW - PULLBACK, y, n_in)
    b.series_0402(f"C{s}1", XR_C1, y, "100p", n_l, n_in,
                  mpn="GRM1555C1H101JA01D",
                  note="blocking capacitor; the limiter must never see a "
                       "direct voltage that arrives on the cable")

    # ---- the limiter, drawn left to right, so its input end is its right one
    x_end = b.limiter_run(f"D{s}L", XR_LIM, y, n_li, n_l, inward)
    b.line(x_end, y, XR_C1 - 0.585, y, n_l)
    b.series_0402(f"C{s}2", XR_C2, y, "100p", n_lo, n_li,
                  mpn="GRM1555C1H101JA01D", note="blocking capacitor")
    b.line(XR_C2 + 0.585, y, XR_LIM, y, n_li)

    # ---- the amplifier, turned end for end so its input faces the antenna
    kind = f"LNA_{s}"
    pins = dict(LNA["pins"])
    pins.update({2: n_lo, 8: n_t})
    b.PARTS[kind] = dict(LNA, pins=pins)
    b.amplifier(f"U{s}1", kind, XR_LNA, y, rot=180, thermal=False)
    b._bom(f"U{s}1", LNA["mpn"], "QFN12 3x3", LNA["mpn"],
           "the sensitivity; matched to fifty ohms inside the package")
    b.line(XR_LNA + LNA["land_half"] + 2.4, y, XR_C2 - 0.585, y, n_lo)
    b.taper(n_lo, XR_LNA + LNA["land_half"] + 2.4,
            XR_LNA + LNA["land_half"], y)
    b.taper(n_t, XR_LNA - LNA["land_half"] - 2.4, XR_LNA - LNA["land_half"],
            y)
    _, n_under = b.thermal_field(XR_LNA, y, 0.60, pitch=0.60, drill=0.30,
                                 pad=0.50, under=0.65)
    n_ring, _ = b.thermal_field(XR_LNA, y, 2.60, pitch=0.62, under=0.0,
                                exclude=0.95)
    b.line(XR_BT, y, XR_LNA - LNA["land_half"] - 2.4, y, n_t)
    b.bias_feed(f"L{s}1", (XR_BT, y), inward, rail)
    b.line(XR_C3 + 0.585, y, XR_BT, y, n_t)
    b.series_0402(f"C{s}3", XR_C3, y, "100p", n_cpl + "M", n_t,
                  mpn="GRM1555C1H101JA01D", note="blocking capacitor")

    # ---- coupler, level pad, clamp, out to the radio
    b.line(XR_TAP + TAP["len_mm"], y, XR_C3 - 0.585, y, n_cpl + "M")
    far, near = b.tap(f"DC{s}", XR_TAP, y, inward, n_cpl + "M", n_cpl)
    b.line(XR_PAD + 3.11, y, XR_TAP, y, n_cpl + "M")
    b.pi_pad_site(s + "P", XR_PAD, y, n_p, n_cpl + "M", inward,
                  db=RX_PAD_DEFAULT_DB)
    b.line(XR_CLAMP, y, XR_PAD - 3.11, y, n_p)
    b.clamp_site(f"D{s}C", XR_CLAMP, y, n_p, inward)
    b.line(XR_C4 + 0.585, y, XR_CLAMP, y, n_p)
    b.series_0402(f"C{s}4", XR_C4, y, "100p", n_o, n_p,
                  mpn="GRM1555C1H101JA01D",
                  note="blocking capacitor; nothing direct-current reaches "
                       "the radio from this board")
    b.line(X_JOG, y, XR_C4 - 0.585, y, n_o)
    b.jog(n_o, X_JOG, y, y_j)
    b.line(PULLBACK, y_j, X_JOG, y_j, n_o)
    b.sma_launch(f"{tag}_RADIO", (0.0, y_j), "left")

    # the far end of the sampling line, loaded on the board
    b.line(far[0], far[1], far[0], far[1] + inward * 1.8, n_cpl, w=0.50)
    b.shunt_0402(f"R{s}F", far[0], far[1] + inward * 1.8, "51R", n_cpl,
                 inward, note="loads the far end of the sampling line")
    return dict(near=near, far=far, n_under=n_under, n_ring=n_ring,
                nets=(n_in, n_lo, n_t, n_p, n_o))


# ------------------------------------------------------------------ detectors
def detector(b, s, tag, port, inward, sgn=1.0):
    """Radio-frequency power in, a slow voltage a computer can read out.

    A diode that needs no supply of its own rectifies the sample, three parts
    smooth it, and a series resistor keeps the reading off the
    radio-frequency side.  Five parts and no rail.
    """
    x, y = port
    n_rf, n_v, n_o = f"{tag}_CPL", f"{tag}_DETV", f"DET{s}"
    y_run = y + inward * 2.2
    b.line(x, y, x, y_run, n_rf, w=0.60)
    b.shunt_0402(f"R{s}T", x, y_run, "51R", n_rf, inward, axis="x",
                 note="the sampling line's own load")
    b.line(min(x, x + sgn * 3.40), y_run, max(x, x + sgn * 3.40), y_run,
           n_rf, w=0.60)
    xd = x + sgn * 4.00
    b.series_0402(f"D{s}1", xd, y_run, "SMS7630",
                  *((n_rf, n_v) if sgn > 0 else (n_v, n_rf)),
                  mpn="SMS7630-079LF", note="zero-bias detector diode")
    xs = xd + sgn * 0.585
    for k, (ref, val) in enumerate((("C", "100p"), ("R", "10k"),
                                    ("C", "10n"))):
        xk = xs + sgn * (1.20 + k * 1.70)
        b.line(min(xs, xk), y_run, max(xs, xk), y_run, n_v, w=0.60)
        b.shunt_0402(f"{ref}{s}D{k}", xk, y_run, val, n_v, inward)
    xr = xs + sgn * (1.20 + 2 * 1.70)
    b.series_0402(f"R{s}O", xr + sgn * 1.80, y_run, "1k",
                  *((n_v, n_o) if sgn > 0 else (n_o, n_v)),
                  note="keeps the reading off the radio-frequency side")
    b.line(min(xr, xr + sgn * 1.215), y_run, max(xr, xr + sgn * 1.215),
           y_run, n_v, w=0.60)
    return (xr + sgn * 2.385, y_run)


# -------------------------------------------------------------- the supplies
def tx_power_block(b, s, yb, x_bt1, x_bt2, y_bt, inward):
    """One transmit channel's supply, in a single row.

    Twelve volts arrives, passes the shunt that measures the final's current,
    and the node beyond it both climbs to the amplifier and carries on into
    the dropper and the regulator that make five volts for the driver.  It is
    one row rather than a block because the gap it has to live in is
    twenty-nine millimetres tall and the amplifier's supply feed already owns
    the first third of that.
    """
    r12, r5, n_i = f"V12{s}", f"V5{s}", f"V5{s}_I"
    # left to right: regulator, dropper, shunt, current amplifier
    b.smd_part(f"U{s}3", 26.0, yb + inward * 3.15, _sot223(("GND", r5, n_i)),
               mpn="AMS1117-5.0", pkg="SOT-223", flip_y=(inward < 0),
               note="five volts for the driver")
    b.smd_part(f"R{s}V", 34.5, yb, _chip(R2512, n_i, r12), pkg="2512",
               value="27R", note="takes most of the regulator's heat off it")
    b.smd_part(f"R{s}S", 42.0, yb, _chip(R2512, r12, "V12"), pkg="2512",
               value="0R1", note="reads the final amplifier's supply current")
    b.line(35.5, yb, 40.5, yb, r12, w=0.70)
    b.line(28.3, yb, 33.0, yb, n_i, w=0.70)
    b.shunt_0402(f"C{s}5", 30.65, yb, "10u", n_i, inward, pkg="0805",
                 note="16 V part; ten microfarads does not come in an 0402")
    # the current amplifier sits beside its own shunt, so the two wires that
    # measure forty millivolts are as short as a layout allows
    b.smd_part(f"U{s}4", 48.5, yb,
               _sot23_6((f"IMON{s}", "GND", "V12", r12, "GND", r5)),
               mpn="INA181A2IDBVR", pkg="SOT-23-6", flip_y=(inward < 0),
               note="turns the shunt's forty millivolts into two volts")
    b.line(43.5, yb, 47.55, yb, r12, w=0.50)
    b.line(43.5, yb - inward * 0.95, 43.5, yb, r12, w=0.50)
    b.line(49.45, yb + inward * 0.95, 51.5, yb + inward * 0.95, "V12", w=0.50)
    b.line(51.5, yb - inward * 1.2, 51.5, yb + inward * 0.95, "V12", w=0.50)
    b.line(43.0, yb - inward * 0.95, 43.0, yb, "V12", w=0.50)
    b.line(43.0, yb - inward * 0.95, 51.5, yb - inward * 0.95, "V12", w=0.50)
    # five volts also feeds the current amplifier, round the outside
    b.line(24.0, yb - inward * 1.9, 47.55, yb - inward * 1.9, r5, w=0.50)
    b.line(24.0, yb - inward * 1.9, 24.0, yb + inward * 3.15, r5, w=0.50)
    b.line(24.0, yb + inward * 3.15, 24.7, yb + inward * 3.15, r5, w=0.50)
    b.line(47.55, yb - inward * 1.9, 47.55, yb - inward * 0.95, r5, w=0.50)

    # ---- the decoupling that the printed feeds work against.  The first
    # capacitor on each feed IS the short circuit that turns a quarter wave
    # into an open circuit at the amplifier's pin, so it sits on the end of
    # the feed with its own hole to ground; the slower ones follow along.
    y_dec = y_bt + inward * 2.6
    b.shunt_0402(f"C{s}80", x_bt2, y_bt + inward * 0.62, "100p", r12, +1,
                 axis="x",
                 note="makes the radio-frequency short the quarter-wave feed "
                      "works against; keep its hole to ground short")
    b.line(x_bt2, y_bt, x_bt2, y_dec, r12, w=0.70)
    b.line(x_bt2, y_dec, x_bt2 + 9.2, y_dec, r12, w=0.70)
    for k, val in enumerate(("1n", "100n"), start=1):
        b.shunt_0402(f"C{s}8{k}", x_bt2 + 1.7 + (k - 1) * 1.7, y_dec, val,
                     r12, -inward)
    b.shunt_0402(f"C{s}83", x_bt2 + 8.0, y_dec, "10u", r12, -inward,
                 pkg="1210", note="25 V part on the twelve-volt rail")
    b.shunt_0402(f"C{s}90", x_bt1, y_bt + inward * 0.62, "100p", r5, -1,
                 axis="x", note="the same job on the driver's feed")
    b.line(x_bt1 - 12.0, y_dec, x_bt1, y_dec, r5, w=0.50)
    b.line(x_bt1, y_bt, x_bt1, y_dec, r5, w=0.50)
    for k, val in enumerate(("1n", "100n"), start=1):
        b.shunt_0402(f"C{s}9{k}", x_bt1 - 1.7 - (k - 1) * 1.7, y_dec, val, r5,
                     -inward)
    b.shunt_0402(f"C{s}93", x_bt1 - 11.4, y_dec, "10u", r5, -inward,
                 pkg="1210")
    # up from the row to the two printed feeds
    b.line(x_bt2 + 9.2, y_dec, x_bt2 + 9.2, yb - inward * 1.9, r12, w=0.70)
    b.line(x_bt2 + 9.2, yb - inward * 1.9, 51.5, yb - inward * 1.9, r12,
           w=0.70)
    b.line(x_bt1 - 12.0, y_dec, x_bt1 - 12.0, yb + inward * 5.4, r5, w=0.50)
    b.line(x_bt1 - 12.0, yb + inward * 5.4, 21.0, yb + inward * 5.4, r5,
           w=0.50)
    b.line(21.0, yb - inward * 1.9, 21.0, yb + inward * 5.4, r5, w=0.50)
    b.line(21.0, yb - inward * 1.9, 24.0, yb - inward * 1.9, r5, w=0.50)


def rx_power_block(b, yb):
    """Five volts for both receive amplifiers, from one regulator.

    Sixty milliamps each is small enough that one regulator, one dropper and
    one current monitor do for the pair, and a receiver that stops working is
    identified by its own level detector going quiet rather than by its
    current.  That saves four parts, a square centimetre of board, and the
    heat of a second regulator.
    """
    r5, n_i = "V5R", "V5R_I"
    b.smd_part("URV", 26.0, yb - 3.15, _sot223(("GND", r5, n_i)),
               mpn="AMS1117-5.0", pkg="SOT-223", flip_y=True,
               note="five volts for both receive amplifiers")
    for k, x in enumerate((34.5, 34.5)):
        b.smd_part(f"RRV{k+1}", x, yb + (2.6 if k else -2.6),
                   _chip(R2512, n_i, "V12R"), pkg="2512", value="82R",
                   note="two in parallel so neither has to take a whole watt")
    b.smd_part("RRS", 42.0, yb, _chip(R2512, "V12R", "V12"), pkg="2512",
               value="1R0", note="reads both amplifiers' supply current")
    for dy in (-2.6, 2.6):
        b.line(35.5, yb + dy, 36.6, yb + dy, "V12R", w=0.70)
        b.line(32.4, yb + dy, 33.5, yb + dy, n_i, w=0.70)
    b.line(36.6, yb - 2.6, 36.6, yb + 2.6, "V12R", w=0.70)
    b.line(32.4, yb - 2.6, 32.4, yb + 2.6, n_i, w=0.70)
    b.line(36.6, yb, 40.5, yb, "V12R", w=0.70)
    b.line(28.3, yb, 32.4, yb, n_i, w=0.70)
    b.shunt_0402("CR5", 30.35, yb, "10u", n_i, -1, pkg="0805")
    b.smd_part("URI", 48.5, yb, _sot23_6(("IMONR", "GND", "V12", "V12R",
                                          "GND", r5)),
               mpn="INA181A2IDBVR", pkg="SOT-23-6",
               note="turns the shunt's sixty millivolts into three volts")
    b.line(43.5, yb, 47.55, yb, "V12R", w=0.50)
    b.line(43.5, yb, 43.5, yb + 0.95, "V12R", w=0.50)
    b.line(43.0, yb - 0.95, 43.0, yb, "V12", w=0.50)
    b.line(43.0, yb - 0.95, 51.5, yb - 0.95, "V12", w=0.50)
    b.line(49.45, yb + 0.95, 51.5, yb + 0.95, "V12", w=0.50)
    b.line(51.5, yb - 0.95, 51.5, yb + 0.95, "V12", w=0.50)
    # five volts back to the current amplifier and out to the two feeds
    b.line(24.0, yb + 5.6, 47.55, yb + 5.6, r5, w=0.50)
    b.line(47.55, yb + 0.95, 47.55, yb + 5.6, r5, w=0.50)
    b.line(24.0, yb - 3.15, 24.0, yb + 5.6, r5, w=0.50)
    b.line(24.0, yb - 3.15, 24.7, yb - 3.15, r5, w=0.50)


def rx_feed(b, s, y, y_bt, inward):
    """The decoupling on one receive amplifier's printed supply feed."""
    r5 = "V5R"
    y_dec = y + inward * 11.5
    b.shunt_0402(f"C{s}80", XR_BT, y_bt + inward * 0.62, "100p", r5, +1,
                 axis="x",
                 note="makes the radio-frequency short the quarter-wave feed "
                      "works against")
    b.line(XR_BT, y_bt, XR_BT, y_dec, r5, w=0.50)
    b.line(XR_BT - 10.4, y_dec, XR_BT, y_dec, r5, w=0.50)
    for k, val in enumerate(("1n", "100n"), start=1):
        b.shunt_0402(f"C{s}8{k}", XR_BT - 1.7 - (k - 1) * 1.7, y_dec, val,
                     r5, -inward)
    b.shunt_0402(f"C{s}83", XR_BT - 9.4, y_dec, "10u", r5, -inward,
                 pkg="1210")
    return (XR_BT - 10.4, y_dec)


def input_block(b):
    """Where the twelve volts comes in, and everything that guards it.

    In the corner furthest from the radio: a screw terminal, a fuse that
    resets itself, a clamp for whatever the supply lead picks up, and one
    transistor doing two jobs -- blocking a supply connected backwards, and
    being the switch that the enable line works.  Beyond it the board's own
    twelve volts leaves on the buried layer and crosses under everything.
    """
    b.smd_part("J1", 9.0, 5.6,
               [(1, -2.54, 0.0, 1.70, 1.70, "VIN"),
                (2, 2.54, 0.0, 1.70, 1.70, "GND")],
               pkg="5.08 mm screw terminal", value="2-way",
               note="twelve volts in, one and a quarter amps")
    b.smd_part("F1", 7.2, 11.0, _chip(R1210, "VIN", "VF"), pkg="1210",
               value="1.5 A", mpn="MF-MSMF150", note="resettable fuse")
    b.line(6.46, 5.6, 6.46, 11.0, "VIN", w=0.90)
    b.shunt_0402("D5", 13.0, 11.0, "SMBJ15A", "VF", +1, mpn="SMBJ15A",
                 pkg="SMB", axis="x",
                 note="clamps whatever the supply lead picks up")
    b.line(8.35, 11.0, 13.0, 11.0, "VF", w=0.90)
    # drain to the incoming side and source to the board, so the transistor's
    # own internal diode faces the way that blocks a reversed supply
    b.smd_part("Q3", 7.0, 15.0, _sot23(("V12G", "V12", "VF")),
               pkg="SOT-23", mpn="AO3401A",
               note="blocks a reversed supply and switches the board")
    b.line(7.95, 11.0, 7.95, 14.06, "VF", w=0.70)
    b.line(6.05, 15.94, 6.05, 18.5, "V12", w=0.70)
    b.line(6.05, 18.5, 16.0, 18.5, "V12", w=0.90)
    for k, (val, pkg, x) in enumerate((("22u", "1210", 9.5),
                                       ("22u", "1210", 13.0),
                                       ("100n", "0402", 15.8))):
        b.shunt_0402(f"C2{k}", x, 18.5, val, "V12", +1, pkg=pkg)
    # the gate network: one rail for the gate, one for the supply, three
    # parts bridging between them, and the transistor the host lets go of
    b.line(7.95, 15.94, 7.95, 22.6, "V12G", w=0.35)
    b.line(7.95, 22.6, 15.0, 22.6, "V12G", w=0.35)
    for ref, x, val, mpn, note in (
            ("R1", 9.6, "100k", "", "holds the switch on"),
            ("C1", 12.0, "100n", "",
             "brings the switch up over ten milliseconds"),
            ("D6", 14.4, "10V", "BZX84C10",
             "keeps the transistor's gate inside its rating")):
        b.series_0402(ref, x, 21.0, val, "V12", "V12G", horiz=False,
                      mpn=mpn, note=note)
        b.line(x, 21.585, x, 22.6, "V12G", w=0.35)
        b.line(x, 19.67, x, 20.415, "V12", w=0.35)
    b.smd_part("Q4", 15.0, 24.2, _sot23(("EN", "GND", "V12G")),
               pkg="SOT-23", mpn="2N7002",
               note="holds the switch on; the enable line lets go of it")
    b.series_0402("R2", 5.0, 23.5, "100k", "VF", "EN", horiz=False)
    b.shunt_0402("R3", 5.0, 25.6, "47k", "EN", +1, axis="x")
    b.line(5.0, 11.0, 5.0, 22.915, "VF", w=0.50)
    b.line(5.0, 24.085, 5.0, 25.6, "EN", w=0.35)
    b.line(5.0, 25.6, 13.1, 25.6, "EN", w=0.35)
    b.line(13.1, 25.14, 13.1, 25.6, "EN", w=0.35)
    b.line(15.94, 23.25, 16.6, 23.25, "V12G", w=0.35)
    b.line(16.6, 22.6, 16.6, 23.25, "V12G", w=0.35)


# ======================================================================= build
def build():
    """The whole front end: four chains, three supplies, one power feed."""
    b = FEBoard()
    for ys in SLOT_Y:
        b.finger_slot(ys)

    ch = {}
    ch["A"] = tx_channel(b, "A", "TX1", Y_TX1, Y_J[0], +1)
    ch["B"] = tx_channel(b, "B", "TX2", Y_TX2, Y_J[3], -1)
    ch["C"] = rx_channel(b, "C", "RX1", Y_RX1, Y_J[1], +1, "V5R")
    ch["D"] = rx_channel(b, "D", "RX2", Y_RX2, Y_J[2], -1, "V5R")

    # ---------------------------------------------------------- the supplies
    tx_power_block(b, "A", Y_PWR_A, X_BT1, X_BT2, Y_TX1 + BIAS["len_mm"], +1)
    tx_power_block(b, "B", Y_PWR_B, X_BT1, X_BT2, Y_TX2 - BIAS["len_mm"], -1)
    rx_power_block(b, Y_RXPWR)
    fc = rx_feed(b, "C", Y_RX1, Y_RX1 + BIAS["len_mm"], +1)
    fd = rx_feed(b, "D", Y_RX2, Y_RX2 - BIAS["len_mm"], -1)
    b.line(fc[0], fc[1], fd[0], fd[1], "V5R", w=0.50)
    b.line(47.55, Y_RXPWR + 5.6, fc[0], Y_RXPWR + 5.6, "V5R", w=0.50)
    b.line(fc[0], fc[1], fc[0], fd[1], "V5R", w=0.50)

    input_block(b)

    # ------------------------------------------------------------ detectors
    ch["A"]["det"] = detector(b, "A", "TX1", ch["A"]["rev"], +1, sgn=+1)
    ch["B"]["det"] = detector(b, "B", "TX2", ch["B"]["rev"], -1, sgn=+1)
    ch["C"]["det"] = detector(b, "C", "RX1", ch["C"]["near"], +1, sgn=-1)
    ch["D"]["det"] = detector(b, "D", "RX2", ch["D"]["near"], -1, sgn=-1)

    # ------------------------------- a thermistor beside each transmit final
    for s, y, inward in (("A", Y_TX1, +1), ("B", Y_TX2, -1)):
        b.shunt_0402(f"RT{s}", X_PA + 4.4, y + inward * 3.6, "10k NTC",
                     f"TEMP{s}", inward, mpn="NCP15XH103F03RC",
                     note="how hot the board is beside the amplifier")

    # ----------------------------------------------------- monitoring header
    # Top-left corner, the furthest point on the board from every antenna
    # cable, so a ribbon of slow signals never runs alongside a live one.
    hdr = b.header("J5", 9.0, 87.5, 2, 8,
                   ["GND", "EN", "IMONA", "GND", "TEMPA", "DETA",
                    "GND", "IMONR", "DETC", "GND", "DETD", "IMONB",
                    "GND", "TEMPB", "DETB", "GND"])

    # ------------------------------- everything that is not radio, buried
    # Each run leaves the surface where it starts, crosses the board on the
    # layer set aside for it under a ground plane, and comes back where it is
    # wanted.  That is what the four layers are for, and it is what lets ten
    # monitoring signals and three supplies share a board with four chains
    # without one of them ever having to cross another on the surface.
    b.dc("V12", [(16.0, 18.5), (16.0, Y_PWR_A), (51.5, Y_PWR_A)],
         w=0.90, n_via=2, skip_last_via=False)
    b.dc("V12", [(51.5, Y_PWR_A + 1.2), (51.5, Y_RXPWR - 0.95)],
         w=0.90, n_via=2)
    b.dc("V12", [(51.5, Y_RXPWR + 0.95), (51.5, Y_PWR_B - 1.2)],
         w=0.90, n_via=2)
    b.dc("EN", [(16.6, 22.6), (16.6, 60.0), (2.9, 60.0), (2.9, hdr["EN"][1]),
                hdr["EN"]], w=0.45, skip_last_via=True)

    for net, src in ((f"IMONA", (50.45, Y_PWR_A + 1.9)),
                     ("TEMPA", (X_PA + 4.4, Y_TX1 + 3.6)),
                     ("DETA", ch["A"]["det"]),
                     ("IMONR", (50.45, Y_RXPWR + 1.9)),
                     ("DETC", ch["C"]["det"]),
                     ("DETD", ch["D"]["det"]),
                     (f"IMONB", (50.45, Y_PWR_B - 1.9)),
                     ("TEMPB", (X_PA + 4.4, Y_TX2 - 3.6)),
                     ("DETB", ch["B"]["det"])):
        pin = hdr[net]
        b.dc(net, [src, (src[0], BH - 4.0), (pin[0], BH - 4.0), pin],
             w=0.40, skip_last_via=True)

    # ----------------------------------------------------------- mechanical
    # Four of the eight bolts sit as close to the four amplifiers as the
    # copper allows, because that is where the heat has to leave the board.
    for x, y in ((X_PA, Y_TX1 + 8.6), (X_PA, Y_TX2 - 8.6),
                 (XR_LNA + 6.0, Y_RX1 + 8.0), (XR_LNA + 6.0, Y_RX2 - 8.0),
                 (95.5, 21.0), (95.5, 79.0), (4.5, 32.0), (4.5, 68.0)):
        b.mounts.append((x, y, MOUNT_D, MOUNT_PAD))

    b.unify_nets()
    b.pour()
    # the underside is left bare under each amplifier, so its heat meets the
    # chassis through metal rather than through solder resist
    for y in (Y_TX1, Y_TX2):
        b.mask_bot.append([X_PA - 3.6, y - 3.6, X_PA + 3.6, y + 3.6])
        b.mask_bot.append([X_DRV - 2.6, y - 3.4, X_DRV + 2.6, y + 3.4])
    for y in (Y_RX1, Y_RX2):
        b.mask_bot.append([XR_LNA - 3.6, y - 3.6, XR_LNA + 3.6, y + 3.6])
    for x, y in ((22.0, 97.0), (BW - 4.0, 50.0), (60.0, 3.0)):
        b.fiducial(x, y)
    b.labels += [
        (56.0, 49.5, "5.8 GHz RADAR FRONT END", 1.3, "silk"),
        (56.0, 46.0, "TX 0.75 W x2   RX 20 dB x2", 0.9, "silk"),
        (56.0, 43.0, "ZYF300CA-P 0.76 mm / 4 layer / Ag", 0.9, "silk"),
        (56.0, 54.0, "12 V 1.25 A   J5 = enable + monitors", 0.9, "silk"),
        (20.0, 12.5, "TX1  0.75 W", 1.0, "silk"),
        (20.0, 88.0, "TX2  0.75 W", 1.0, "silk"),
        (20.0, 32.0, "RX1", 1.0, "silk"),
        (20.0, 67.5, "RX2", 1.0, "silk"),
        (2.2, 50.0, "USRP B210", 1.0, "silk"),
    ]
    for nm, x, y, side in b.ports:
        if side == "left":
            b._bom(nm, "142-0711-841", "SMA plug end launch",
                   "Cinch 142-0711-841",
                   "mates straight onto the radio; 0.062 in board")
        else:
            b._bom(nm, "142-0701-801", "SMA jack end launch",
                   "Cinch 142-0701-801", "antenna cable; 0.062 in board")
    b._bom("H1-H8", "M3 x 6 pan head and washer", "M3", "",
           "four of the eight sit beside the amplifiers and carry their heat")
    b._bom("TIM", "thermal pad 0.5 mm, 3 W/mK", "10 x 10 mm", "",
           "under each amplifier, between the bare underside and the chassis")
    if b.failed:
        print(f"  {len(b.failed)} connections could not be routed:")
        for f in b.failed[:8]:
            print("     ", f)
    return b, ch


# ====================================================================== report
def isolation(sep_mm=None, run_mm=35.0):
    """What crosses the laminate from a transmit chain to a receive one.

    Measured by openEMS on this exact stack: two fifty-ohm lines with a
    stitched ground wall between them, 23 mm apart, running alongside for
    80 mm, couple at -58.88 dB.  Two things make the real board better than
    that number.  The chains here are further apart, and only the last third
    of a transmit chain -- filter, coupler, output line -- carries full power;
    before the final amplifier the line is twenty decibels down.

    Coupling between parallel lines falls with distance and with how far they
    run alongside each other, and both are counted here.
    """
    sep = (Y_RX1 - Y_TX1) if sep_mm is None else sep_mm
    ref_sep, ref_run, ref_db = 23.0, 80.0, -58.88
    # measured falls off close to inverse-square once the wall dominates
    d_sep = -20.0 * math.log10(sep / ref_sep)
    d_run = 10.0 * math.log10(run_mm / ref_run)
    return dict(sep_mm=sep, run_mm=run_mm, ref_db=ref_db,
                iso_db=ref_db + d_sep + d_run,
                from_sep_db=d_sep, from_run_db=d_run)


def leak_budget(pad_db=RX_PAD_DEFAULT_DB, sep_m=0.375):
    """Everything that reaches the radio while the transmitter is running.

    Two routes: through the air between the two antenna arrays, and across
    the laminate.  Both land at zero range and the radar's own processing
    removes them; what matters is only that the radio is not driven into
    compression, which happens at fifteen microwatts.
    """
    c = PA.chain(TX_PAD_DEFAULT_DB)
    pt = c["out_connector_dbm"]
    rx = LN.chain(pad_db)
    g = rx["gain_db"]
    # measured on the whole undivided array board: -42.17 dB at 92.1 mm
    # between the transmit and receive blocks, thinning as the square of the
    # distance once they are pulled apart
    air_iso = -42.17 - 20.0 * math.log10(sep_m / 0.0921)
    air_in = pt + air_iso
    board = isolation()
    board_in = pt + board["iso_db"]
    tot_in = 10.0 * math.log10(10 ** (air_in / 10.0) + 10 ** (board_in / 10.0))
    return dict(pt_dbm=pt, gain_db=g, sep_m=sep_m,
                air_iso_db=air_iso, air_in_dbm=air_in, air_radio=air_in + g,
                board_iso_db=board["iso_db"], board_in_dbm=board_in,
                board_radio=board_in + g,
                total_in_dbm=tot_in, total_radio_dbm=tot_in + g,
                margin_db=B210_P1DB_DBM - (tot_in + g),
                added_by_board_db=(tot_in + g) - (air_in + g))


def separation_for_combined(pad_db=RX_PAD_DEFAULT_DB, limit_dbm=-18.0):
    """How far apart the two arrays have to sit now the board is shared."""
    lo, hi = 0.05, 5.0
    for _ in range(60):
        mid = math.sqrt(lo * hi)
        if leak_budget(pad_db, mid)["total_radio_dbm"] > limit_dbm:
            lo = mid
        else:
            hi = mid
    return hi


def report(b, ch):
    c = PA.chain(TX_PAD_DEFAULT_DB)
    rx = LN.chain(RX_PAD_DEFAULT_DB)
    n = LN.noise(RX_PAD_DEFAULT_DB)
    d = PA.dissipation("p1db")
    t = PA.thermal(min(ch["A"]["n_under"], ch["B"]["n_under"]),
                   min(ch["A"]["n_ring"], ch["B"]["n_ring"]))
    tl = LN.thermal(ch["C"]["n_under"], ch["C"]["n_ring"])
    iso = isolation()
    lk = leak_budget()
    th = LN.threats(RX_PAD_DEFAULT_DB)
    worst_lna = max(t["at_lna"] for t in th)
    worst_radio = max(t["at_radio"] for t in th)

    print("5.8 GHz radar front end: two transmit chains and two receive "
          "chains on one board\n")
    print(f"  board {BW:g} x {BH:g} mm, {STACK['layers']} layer, "
          f"{STACK['total_mm']:.3f} mm, {SUB.name}")
    print(f"  mounts straight on the radio: four plugs at {PITCH:g} mm "
          f"pitch, on four separate fingers")
    print(f"  {len(b.ports)} connectors, {len(b.bom)} lines on the parts "
          f"list, {len(b.vias)} holes\n")

    print("  transmit, each channel")
    print(f"    in {c['pin_dbm']:+.1f} dBm -> out {c['pout_dbm']:+.2f} dBm "
          f"({c['pout_w']:.3f} W), gain {c['gain_db']:.2f} dB")
    print(f"    level pad {TX_PAD_DEFAULT_DB:.0f} dB fitted, so the radio "
          f"cannot drive it past its rating")
    print(f"    filter measured {MEASURED['filter']['loss_5800']:.2f} dB in "
          f"band, {MEASURED['filter']['rej_2f0']:.1f} dB at the second "
          f"harmonic")
    print(f"    final runs at {d['final_w']:.2f} W dissipated, die "
          f"{t['t_die_c']:.0f} C, {t['die_margin_c']:.0f} C in hand\n")

    print("  receive, each channel")
    print(f"    gain {rx['gain_db']:+.2f} dB net, noise figure "
          f"{n['nf_db']:.2f} dB, {n['range_ratio']:.2f}x the range the radio "
          f"has on its own")
    print(f"    limiter holds the amplifier at "
          f"{LN.threats(RX_PAD_DEFAULT_DB)['worst_lna_dbm']:+.1f} dBm "
          f"against its {LNA['pin_max_dbm']:+.0f} dBm rating")
    print(f"    clamp holds the radio at "
          f"{LN.threats(RX_PAD_DEFAULT_DB)['worst_radio_dbm']:+.1f} dBm "
          f"against damage at {B210_MAX_DBM:+.0f} dBm")
    print(f"    amplifier die {tl['t_die_c']:.0f} C, "
          f"{tl['die_margin_c']:.0f} C in hand\n")

    print("  transmit and receive on one board")
    print(f"    chains {iso['sep_mm']:.0f} mm apart, full power over the "
          f"last {iso['run_mm']:.0f} mm")
    print(f"    measured -58.88 dB at 23 mm over 80 mm, so "
          f"{iso['iso_db']:.1f} dB here")
    print(f"    across the laminate: {lk['board_radio']:+.1f} dBm at the "
          f"radio")
    print(f"    through the air at {lk['sep_m']*1000:.0f} mm between arrays: "
          f"{lk['air_radio']:+.1f} dBm")
    print(f"    together {lk['total_radio_dbm']:+.1f} dBm, against "
          f"{B210_P1DB_DBM:+.0f} dBm where the radio starts to compress "
          f"({lk['margin_db']:.1f} dB in hand)")
    print(f"    sharing the board costs {lk['added_by_board_db']:.2f} dB, "
          f"bought back by moving the arrays to "
          f"{separation_for_combined()*1000:.0f} mm\n")

    print("  what one board saves against two")
    a1 = PA.BW * PA.BH + LN.BW * LN.BH
    print(f"    laminate {BW*BH:.0f} mm2 against {a1:.0f} mm2 "
          f"({100*(1-BW*BH/a1):.0f} per cent less)")
    print(f"    {len(b.ports)} connectors against 12, and four cables "
          f"between the radio and the board become none")
    print(f"    one power feed, one enable line, one monitoring header, "
          f"one enclosure")


# ======================================================================== main
def emit(b):
    return dict(
        name="radar_5g8_frontend", outline=[BW, BH],
        slots=b.slots, stack=STACK, w50=W50, launch_gap=LAUNCH_GAP,
        top=b.top, top_net=b.nets, pad_idx=sorted(b.pads),
        gnd_top=b.gnd_top, inner=b.inner, inner_net=b.inner_net,
        bot=b.bot, bot_net=b.bot_net, mask_bot=b.mask_bot,
        vias=[list(v) for v in b.vias], via_net=b.via_net,
        mounts=[list(m) for m in b.mounts], ports=[list(p) for p in b.ports],
        parts=b.parts, labels=b.labels, bom=b.bom,
        printed=dict(lpf=LPF, tap=TAP, bias=BIAS, limiter=LIM, clamp=CLP))


if __name__ == "__main__":
    board, chans = build()
    report(board, chans)
    out = os.path.join(HERE, "fe_board.json")
    json.dump(emit(board), open(out, "w"))
    print(f"\n  wrote {os.path.relpath(out, os.path.dirname(HERE))}")
