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
XR_C1, XR_LIM = 93.0, 67.4
XR_C2, XR_LNA, XR_BT, XR_C3 = 66.1, 59.5, 52.0, 49.0
XR_TAP, XR_PAD, XR_CLAMP, XR_C4 = 39.5, 34.0, 27.5, 24.0

# Each side's supply row, and the two rules the rows are built on: a supply
# feed's decoupling always runs to the RIGHT of the feed, and a regulator's
# output always leaves on a lane clear of the row rather than through it.
Y_PWR_A, Y_PWR_B = 28.0, 72.0
Y_DEC_OFF = 1.6                         # decoupling row, off a feed's end
Y_DET_OFF = 3.5                         # detector row, off the sampling line
X_V12_IN = 50.0                         # where the buried twelve volts lands
X_V5_BR = 23.5                          # the receive amplifiers' supply branch
# The one clear corridor between the transmit final's ring of holes and the
# receive amplifier's: 2.5 mm wide, and the thermistor and its run share it.
X_TEMP = 55.75

# One regulator per side, feeding that side's transmit driver AND the receive
# amplifier next to it.  A hundred and ninety milliamps is small enough that
# the pair share comfortably, and it saves a third regulator, a third dropper,
# a third current monitor and a square centimetre of board.
TX_PAD_DEFAULT_DB = 6.0
RX_PAD_DEFAULT_DB = 8.0


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
    inward says which way the middle of the board is: the supply feeds and
    everything that is not radio hang that way, and the outer edge is left to
    the signal.
    """
    n_in, n_p1 = f"{tag}_IN", f"{tag}_P"
    n_di, n_d = f"{tag}_DI", f"{tag}_D"
    n_pi, n_f = f"{tag}_PI", f"{tag}_F"
    n_pa, n_cpl = f"{tag}_PA", f"{tag}_CPL"
    r12, r5 = f"V12{s}", f"V5{s}"

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
    # The current-setting pin is number twelve, which is on the package's top
    # row whichever way this chain faces, so its stub always goes up.
    y_iadj = y + FINAL["land_half"] + 1.20
    b.line(X_PA - FINAL["pitch"], y + FINAL["land_half"],
           X_PA - FINAL["pitch"], y_iadj, f"{tag}_IADJ", w=0.30)
    b.smd_part(f"R{s}I", X_PA - FINAL["pitch"], y_iadj + 0.585,
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
    # reading with whatever phase they like.  It is loaded on the board rather
    # than brought out: the supply-current monitor already says the amplifier
    # is making its power, so the one thing worth a detector is what comes
    # BACK, which is how a fallen-off antenna announces itself.
    b.line(fwd[0], fwd[1], fwd[0], fwd[1] + inward * 1.6, n_cpl, w=0.50)
    b.shunt_0402(f"R{s}F", fwd[0], fwd[1] + inward * 1.6, "51R", n_cpl,
                 +1, axis="x",
                 note="loads the forward end of the sampling line")
    return dict(rev=rev, fwd=fwd, n_ring=n_ring, n_under=n_under,
                rails=(r12, r5))


# ------------------------------------------------------------- receive chain
def rx_channel(b, s, tag, y, y_j, inward, rail):
    """One receive chain: antenna in on the right, radio out on the left.

    Limiter, low-noise amplifier, sampling coupler, level pad, clamp.  It is
    drawn right to left because that is the way its signal travels, which
    keeps every connector on the edge it belongs to and means the receive line
    never has to double back past itself.  The amplifier is turned end for end
    so that its input faces the antenna.
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
    x_end = b.limiter_run(f"D{s}L", XR_LIM, y, n_li, n_l, inward)
    b.line(x_end, y, XR_C1 - 0.585, y, n_l)
    b.series_0402(f"C{s}2", XR_C2, y, "100p", n_lo, n_li,
                  mpn="GRM1555C1H101JA01D", note="blocking capacitor")
    b.line(XR_C2 + 0.585, y, XR_LIM, y, n_li)

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

    b.line(far[0], far[1], far[0], far[1] + inward * 1.6, n_cpl, w=0.50)
    b.shunt_0402(f"R{s}F", far[0], far[1] + inward * 1.6, "51R", n_cpl,
                 -1, axis="x", note="loads the far end of the sampling line")
    return dict(near=near, far=far, n_under=n_under, n_ring=n_ring,
                nets=(n_in, n_lo, n_t, n_p, n_o))


# ------------------------------------------------------------------ detectors
def detector(b, s, tag, port, inward):
    """Radio-frequency power in, a slow voltage a computer can read out.

    A diode that needs no supply of its own rectifies the sample, three parts
    smooth it, and a series resistor keeps the reading off the
    radio-frequency side.  Five parts and no rail.  It is laid out back
    towards the middle of the board so that it never has to reach past a
    connector to get where it is going.
    """
    x, y = port
    n_rf, n_v, n_o = f"{tag}_CPL", f"{tag}_DETV", f"DET{s}"
    y_run = y + inward * Y_DET_OFF
    b.line(x, y, x, y_run, n_rf, w=0.60)
    b.shunt_0402(f"R{s}T", x, y_run, "51R", n_rf, +1, axis="x",
                 note="the sampling line's own load")
    b.line(x - 3.40, y_run, x, y_run, n_rf, w=0.60)
    b.series_0402(f"D{s}1", x - 4.00, y_run, "SMS7630", n_v, n_rf,
                  mpn="SMS7630-079LF", note="zero-bias detector diode")
    xs = x - 4.585
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


# -------------------------------------------------------------- the supplies
def tx_power_block(b, s, yb, y_bt, inward):
    """One side's supply, in a single row, with nothing crossing anything.

    Twelve volts arrives from the buried layer at the right-hand end, passes
    the shunt that measures the current, and the node beyond it climbs to the
    final amplifier and carries on left into the dropper and the regulator
    that make five volts.  Every part sits on one line, so the only things
    that have to leave the row are the regulator's output and the amplifier's
    twelve volts -- and both leave on lanes clear of the row rather than
    through it, which is what makes a row like this route at all.
    """
    r12, r5, n_i = f"V12{s}", f"V5{s}", f"V5{s}_I"
    up = inward                       # away from this side's transmit chain
    y_dec = y_bt + up * Y_DEC_OFF

    # ---- the row itself, right to left along the current's own path
    b.smd_part(f"R{s}S", 44.0, yb, _chip(R2512, r12, "V12"), pkg="2512",
               value="0R1", note="reads this side's whole supply current")
    b.smd_part(f"R{s}V", 37.0, yb, _chip(R2512, n_i, r12), pkg="2512",
               value="27R", note="takes most of the regulator's heat off it")
    b.line(39.3, yb, 41.7, yb, r12, w=0.90)
    b.line(46.3, yb, X_V12_IN, yb, "V12", w=0.90)
    # its pins land on the row and its metal tab -- which is the output --
    # faces the chain, where the five volts is wanted
    b.smd_part(f"U{s}3", 30.5, yb - up * 3.15, _sot223(("GND", r5, n_i)),
               mpn="AMS1117-5.0", pkg="SOT-223", flip_y=(up > 0),
               note="five volts for this side's driver and receive amplifier")
    b.line(33.55, yb, 34.7, yb, n_i, w=0.90)
    y_tab = yb - up * 6.30
    b.shunt_0402(f"C{s}5", 34.5, y_tab, "10u", r5, up, pkg="0805",
                 note="the regulator's own output capacitor; 16 V part")
    b.line(32.4, y_tab, 34.5, y_tab, r5, w=0.60)

    # ---- the current monitor, directly across the row from its own shunt so
    # that the two wires measuring sixty millivolts are as short as they get
    y_ina = yb - up * 4.5
    b.smd_part(f"U{s}4", 44.0, y_ina,
               _sot23_6((f"IMON{s}", "GND", "V12", r12, "GND", r5)),
               mpn="INA181A2IDBVR", pkg="SOT-23-6", flip_y=(up < 0),
               note="turns the shunt's sixty millivolts into three volts")
    b.line(42.86, y_ina + up * 0.95, 42.86, yb, r12, w=0.45)
    b.line(45.14, y_ina + up * 0.95, 45.14, yb, "V12", w=0.45)
    # five volts to the monitor, on a lane clear of the row
    y_lane5 = yb - up * 5.5
    b.line(34.5, y_lane5, 42.86, y_lane5, r5, w=0.45)
    b.line(42.86, y_lane5, 42.86, y_ina - up * 0.95, r5, w=0.45)
    b.line(34.5, y_lane5, 34.5, y_tab, r5, w=0.45)

    # ---- twelve volts up to the final amplifier's printed feed, on a lane
    # clear of the row the other way
    y_lane12 = yb + up * 3.5
    b.line(40.5, yb, 40.5, y_lane12, r12, w=0.70)
    b.line(40.5, y_lane12, X_BT2, y_lane12, r12, w=0.70)
    b.line(X_BT2, y_dec, X_BT2, y_lane12, r12, w=0.70)

    # ---- the decoupling that the printed feeds work against.  The first
    # capacitor on each feed IS the short circuit that turns a quarter wave
    # into an open circuit at the amplifier's pin, so it sits on the end of
    # the feed with its own hole to ground; the slower ones follow along.
    # Both rows run to the RIGHT of their feed, into board that is otherwise
    # empty, which is what keeps them clear of the supply row.
    b.shunt_0402(f"C{s}80", X_BT2, y_bt + up * 0.62, "100p", r12, +1,
                 axis="x",
                 note="makes the radio-frequency short the quarter-wave feed "
                      "works against; keep its hole to ground short")
    b.line(X_BT2, y_dec, X_BT2 + 9.2, y_dec, r12, w=0.70)
    for k, val in enumerate(("1n", "100n"), start=1):
        b.shunt_0402(f"C{s}8{k}", X_BT2 + 1.7 + (k - 1) * 1.7, y_dec, val,
                     r12, up)
    b.shunt_0402(f"C{s}83", X_BT2 + 8.0, y_dec, "10u", r12, up, pkg="1210",
                 note="25 V part on the twelve-volt rail")
    b.shunt_0402(f"C{s}90", X_BT1, y_bt + up * 0.62, "100p", r5, -1,
                 axis="x", note="the same job on the driver's feed")
    b.line(X_BT1, y_dec, X_BT1 + 9.4, y_dec, r5, w=0.50)
    for k, val in enumerate(("1n", "100n"), start=1):
        b.shunt_0402(f"C{s}9{k}", X_BT1 + 1.7 + (k - 1) * 1.7, y_dec, val, r5,
                     up)
    b.shunt_0402(f"C{s}93", X_BT1 + 8.0, y_dec, "10u", r5, up,
                 pkg="1210")
    # the regulator's output down to the feed row, and out to the left where
    # the receive amplifier's own branch picks it up
    b.line(36.0, y_dec, X_BT1, y_dec, r5, w=0.50)
    b.line(36.0, y_dec, 36.0, y_tab, r5, w=0.50)
    b.line(X_V5_BR, y_tab, 32.4, y_tab, r5, w=0.50)
    return dict(v5_at=(X_V5_BR, y_tab), imon=(45.14, y_ina - up * 0.95))


def rx_feed(b, s, y, y_bt, inward, src, src_rail):
    """Five volts across to one receive amplifier, and its decoupling.

    It comes from the transmit side's regulator, so it has to get past that
    side's whole supply row and then past the receive line itself.  It walks
    round the end of the row on the surface, and ducks under the receive line
    on a short stretch of the underside -- which costs nothing, because the
    line's own reference is the plane immediately below it, not that one.  A
    ferrite bead where it comes back up keeps the transmitter's supply noise
    out of the receiver.
    """
    rail = f"V5R{s}"
    y_dec = y + inward * (BIAS["len_mm"] + Y_DEC_OFF)
    x0 = XR_BT - 12.0
    b.line(X_V5_BR, min(src[1], y - inward * 2.0),
           X_V5_BR, max(src[1], y - inward * 2.0), src_rail, w=0.50)
    b.hop(src_rail, (X_V5_BR, y - inward * 2.0), (X_V5_BR, y + inward * 2.0),
          w=0.80)
    b.line(X_V5_BR, y + inward * 2.0, X_V5_BR, y_dec - inward * 2.2,
           src_rail, w=0.50)
    b.corner(X_V5_BR, y_dec - inward * 2.2, src_rail, w=0.50)
    b.line(X_V5_BR, y_dec - inward * 2.2, x0 - 2.4, y_dec - inward * 2.2,
           src_rail, w=0.50)
    b.corner(x0 - 2.4, y_dec - inward * 2.2, src_rail, w=0.50)
    b.line(x0 - 2.4, y_dec - inward * 2.2, x0 - 2.4, y_dec, src_rail, w=0.50)
    b.series_0402(f"FB{s}", x0 - 1.2, y_dec, "600R", src_rail, rail, kind="L",
                  mpn="BLM15HD601SN1",
                  note="keeps the transmitter's supply noise out of the "
                       "receive amplifier")
    b.shunt_0402(f"C{s}80", XR_BT, y_bt + inward * 0.62, "100p", rail, +1,
                 axis="x",
                 note="makes the radio-frequency short the quarter-wave feed "
                      "works against")
    b.line(x0 - 0.615, y_dec, XR_BT, y_dec, rail, w=0.50)
    b.line(XR_BT, y_bt, XR_BT, y_dec, rail, w=0.50)
    for k, val in enumerate(("1n", "100n"), start=1):
        b.shunt_0402(f"C{s}8{k}", x0 + 1.4 + (k - 1) * 1.7, y_dec, val,
                     rail, -inward)


def input_block(b):
    """Where the twelve volts comes in, and everything that guards it.

    In the corner furthest from every antenna: a screw terminal, a fuse that
    resets itself, a clamp for whatever the supply lead picks up, and one
    transistor doing two jobs -- blocking a supply connected backwards, and
    being the switch the enable line works.  Beyond it the board's own twelve
    volts leaves on the buried layer and crosses under everything.

    The corner is laid out in bands: supply in at the bottom, the fuse and the
    clamp above it, the blocking transistor above that, the twelve-volt rail
    and its bulk capacitors above that, and the gate network at the top.  Only
    the enable line has to get from the bottom of the corner to the top past
    all of it, and it does that underneath.
    """
    b.smd_part("J1", 9.0, 4.6,
               [(1, -2.54, 0.0, 1.70, 1.70, "VIN"),
                (2, 2.54, 0.0, 1.70, 1.70, "GND")],
               pkg="5.08 mm screw terminal", value="2-way",
               note="twelve volts in, one and a quarter amps")
    b.smd_part("F1", 5.0, 9.5, _chip(R1210, "VIN", "VF", horiz=False),
               pkg="1210", value="1.5 A", mpn="MF-MSMF150",
               note="resettable fuse")
    b.line(6.46, 4.6, 6.46, 7.0, "VIN", w=0.90)
    b.line(5.0, 7.0, 6.46, 7.0, "VIN", w=0.90)
    b.line(5.0, 7.0, 5.0, 8.35, "VIN", w=0.90)
    b.line(5.0, 10.65, 5.0, 12.0, "VF", w=0.90)
    b.shunt_0402("D5", 9.0, 12.0, "SMBJ15A", "VF", +1, mpn="SMBJ15A",
                 pkg="SMB", axis="x",
                 note="clamps whatever the supply lead picks up")
    b.line(5.0, 12.0, 9.0, 12.0, "VF", w=0.90)
    # Drain to the incoming side and source to the board, so the transistor's
    # own body diode faces the way that blocks a reversed supply.
    b.smd_part("Q3", 6.0, 15.0, _sot23(("V12G", "V12", "VF")),
               pkg="SOT-23", mpn="AO3401A", flip_y=True,
               note="blocks a reversed supply and switches the board")
    b.line(6.0, 12.0, 6.0, 14.06, "VF", w=0.70)
    b.line(6.95, 15.94, 6.95, 19.0, "V12", w=0.70)
    b.line(6.95, 19.0, 16.0, 19.0, "V12", w=0.90)
    for val, pkg, x in (("22u", "1210", 9.0), ("22u", "1210", 12.0),
                        ("100n", "0402", 14.5)):
        b.shunt_0402(f"C2{x:.0f}", x, 19.0, val, "V12", -1, pkg=pkg)
    # the gate network: one rail for the gate, one for the supply, three parts
    # bridging between them, and the transistor the host lets go of
    b.line(4.8, 15.94, 4.8, 24.5, "V12G", w=0.35)
    b.line(4.8, 15.94, 5.05, 15.94, "V12G", w=0.35)
    b.line(4.8, 24.5, 15.5, 24.5, "V12G", w=0.35)
    for ref, x, val, mpn, note in (
            ("R1", 10.5, "100k", "", "holds the switch on"),
            ("C1", 13.0, "100n", "",
             "brings the switch up over ten milliseconds"),
            ("D6", 15.5, "10V", "BZX84C10",
             "keeps the transistor's gate inside its rating")):
        b.series_0402(ref, x, 21.75, val, "V12", "V12G", horiz=False,
                      mpn=mpn, note=note)
        b.line(x, 19.0, x, 21.165, "V12", w=0.35)
        b.line(x, 22.335, x, 24.5, "V12G", w=0.35)
    b.smd_part("Q4", 7.5, 21.75, _sot23(("EN", "GND", "V12G")),
               pkg="SOT-23", mpn="2N7002",
               note="holds the switch on; the enable line lets go of it")
    b.line(7.5, 22.69, 7.5, 24.5, "V12G", w=0.35)
    b.smd_part("R2", 2.6, 13.0, _chip((0.62, 0.62, 0.585), "VF", "EN",
                                      horiz=False),
               pkg="0402", value="100k",
               note="pulls the enable line up if nothing drives it")
    b.line(2.6, 12.0, 2.6, 12.415, "VF", w=0.40)
    b.line(2.6, 12.0, 5.0, 12.0, "VF", w=0.40)
    b.shunt_0402("R3", 2.6, 16.0, "47k", "EN", -1, axis="x")
    b.line(2.6, 13.585, 2.6, 16.0, "EN", w=0.35)
    # the enable line is the one thing that has to get from the bottom of this
    # corner to the top, so it goes underneath rather than round
    b.dc("EN", [(2.6, 16.0), (2.6, 20.81), (6.55, 20.81)], w=0.40)
    return (6.55, 20.81)


# ======================================================================= build
def build():
    """The whole front end: four chains, two supplies, one power feed."""
    b = FEBoard()
    for ys in SLOT_Y:
        b.finger_slot(ys)

    ch = {}
    ch["A"] = tx_channel(b, "A", "TX1", Y_TX1, Y_J[0], +1)
    ch["B"] = tx_channel(b, "B", "TX2", Y_TX2, Y_J[3], -1)
    ch["C"] = rx_channel(b, "C", "RX1", Y_RX1, Y_J[1], +1, "V5RC")
    ch["D"] = rx_channel(b, "D", "RX2", Y_RX2, Y_J[2], -1, "V5RD")

    pa = tx_power_block(b, "A", Y_PWR_A, Y_TX1 + BIAS["len_mm"], +1)
    pb = tx_power_block(b, "B", Y_PWR_B, Y_TX2 - BIAS["len_mm"], -1)
    rx_feed(b, "C", Y_RX1, Y_RX1 + BIAS["len_mm"], +1, pa["v5_at"], "V5A")
    rx_feed(b, "D", Y_RX2, Y_RX2 - BIAS["len_mm"], -1, pb["v5_at"], "V5B")
    en_src = input_block(b)

    # ------------------------------------------------------------ detectors
    ch["A"]["det"] = detector(b, "A", "TX1", ch["A"]["rev"], +1)
    ch["B"]["det"] = detector(b, "B", "TX2", ch["B"]["rev"], -1)
    ch["C"]["det"] = detector(b, "C", "RX1", ch["C"]["near"], +1)
    ch["D"]["det"] = detector(b, "D", "RX2", ch["D"]["near"], -1)

    # ------------------------------- a thermistor beside each transmit final
    for s, y, inward in (("A", Y_TX1, +1), ("B", Y_TX2, -1)):
        b.shunt_0402(f"RT{s}", X_TEMP, y + inward * 3.6, "10k NTC",
                     f"TEMP{s}", -inward, mpn="NCP15XH103F03RC",
                     note="how hot the board is beside the amplifier")

    # ----------------------------------------------------- monitoring header
    # In the middle of the board, where every one of the nine things it
    # carries is a short buried run away.  Put in a corner it would need a
    # ten-lane corridor squeezed past four connectors to reach it.
    hdr = b.header("J5", 70.0, 50.0, 8, 2,
                   ["GND", "DETD", "IMONB", "TEMPB", "DETB", "GND",
                    "GND", "GND",
                    "GND", "EN", "DETC", "IMONA", "TEMPA", "DETA",
                    "GND", "GND"])

    # ------------------------------- everything that is not radio, buried
    # Each run leaves the surface where it starts, crosses on the layer set
    # aside for it under a ground plane, and comes back where it is wanted.
    # The lanes are dealt out so that a run's height falls as its source moves
    # right, and its header pin moves right as its source does: with those two
    # rules together, nine signals share one layer without a single crossing.
    # Down under the near transmit chain to its supply row, then along the
    # underside to the far one.  The underside is a ground plane and stays
    # one except for that stretch; giving it up costs nothing, because what
    # runs above it is referenced to the plane immediately below the surface.
    b.dc("V12", [(16.0, 19.0), (16.0, 14.0), (X_V12_IN, 14.0),
                 (X_V12_IN, Y_PWR_A)], w=0.90, n_via=2)
    b.line(X_V12_IN, Y_PWR_A, X_V12_IN, Y_PWR_A + 2.2, "V12", w=0.90)
    b.hop("V12", (X_V12_IN, Y_PWR_A + 2.2), (X_V12_IN, Y_PWR_B - 2.2), w=1.20)
    b.line(X_V12_IN, Y_PWR_B - 2.2, X_V12_IN, Y_PWR_B, "V12", w=0.90)

    # A lane's height falls as its source moves right, and its header pin
    # moves right as its source does.  Those two rules together are what let
    # nine signals share one layer without a single crossing.  The enable line
    # is the exception: it starts behind the four finger slots, so it comes
    # out along the board first and only then turns for the header.
    lanes = [("EN", en_src, 45.0, 17.0), ("DETC", ch["C"]["det"], 44.4, None),
             ("IMONA", pa["imon"], 43.8, None),
             ("TEMPA", (X_TEMP, 10.6), 43.2, None),
             ("DETA", ch["A"]["det"], 42.6, 79.0),
             ("DETD", ch["D"]["det"], 55.0, None),
             ("IMONB", pb["imon"], 55.6, None),
             ("TEMPB", (X_TEMP, 89.4), 56.2, None),
             ("DETB", ch["B"]["det"], 56.8, 79.0)]
    for net, src, lane, turn in lanes:
        pin = hdr[net]
        pts = [src]
        if turn is not None:
            pts += [(src[0], 30.0 if lane < 50 else 70.0),
                    (turn, 30.0 if lane < 50 else 70.0)]
            pts.append((turn, lane))
        else:
            pts.append((src[0], lane))
        pts += [(pin[0], lane), pin]
        b.dc(net, pts, w=0.40, skip_last_via=True)

    # ----------------------------------------------------------- mechanical
    # Four of the eight bolts sit as close to the four amplifiers as the
    # copper allows, because that is where the heat has to leave the board.
    for x, y in ((X_PA, Y_TX1 + 8.8), (X_PA, Y_TX2 - 8.8),
                 (XR_LNA + 4.0, 43.0), (XR_LNA + 4.0, 57.0),
                 (95.5, 21.0), (95.5, 79.0), (10.0, 90.0), (30.0, 50.0)):
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
    for x, y in ((22.0, 97.0), (78.0, 97.0), (60.0, 3.0)):
        b.fiducial(x, y)
    b.labels += [
        (24.0, 97.0, "5.8 GHz RADAR FRONT END", 1.3, "silk"),
        (24.0, 94.5, "TX 0.75 W x2   RX 20 dB x2   12 V 1.25 A", 0.85,
         "silk"),
        (66.0, 44.0, "J5  enable + monitors", 0.85, "silk"),
        (20.0, 12.5, "TX1  0.75 W", 1.0, "silk"),
        (20.0, 88.0, "TX2  0.75 W", 1.0, "silk"),
        (20.0, 40.5, "RX1", 1.0, "silk"),
        (20.0, 59.0, "RX2", 1.0, "silk"),
        (2.3, 50.0, "USRP B210", 0.9, "silk"),
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


def leak_budget(pad_db=RX_PAD_DEFAULT_DB, sep_m=0.624):
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
    print(f"    drive {c['drive_for_full_dbm']:+.1f} dBm -> out "
          f"{c['out_connector_dbm']:+.2f} dBm "
          f"({c['out_connector_w']:.3f} W), gain {c['gain_db']:.2f} dB")
    print(f"    level pad {TX_PAD_DEFAULT_DB:.0f} dB fitted, so the radio "
          f"cannot drive it past its rating")
    print(f"    filter measured {MEASURED['filter']['loss_5800']:.2f} dB in "
          f"band, {MEASURED['filter']['rej_2f0']:.1f} dB at the second "
          f"harmonic")
    print(f"    final dissipates {d['final_w']:.2f} W, die "
          f"{t['t_die_c']:.0f} C, {t['die_margin_c']:.0f} C in hand\n")

    print("  receive, each channel")
    print(f"    gain {rx['gain_db']:+.2f} dB net, noise figure "
          f"{n['nf_db']:.2f} dB, {n['range_ratio']:.2f}x the range the radio "
          f"has on its own")
    print(f"    worst thing that can arrive holds the amplifier at "
          f"{worst_lna:+.1f} dBm against its "
          f"{LNA['pin_max_dbm']:+.0f} dBm rating")
    print(f"    and holds the radio at {worst_radio:+.1f} dBm against "
          f"damage at {B210_MAX_DBM:+.0f} dBm")
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
        name="radar_5g8_frontend", outline=[BW, BH], layers=4,
        slots=b.slots, w50=W50, launch_gap=LAUNCH_GAP,
        stack=dict(name=SUB.name, h_mm=SUB.h * 1e3, er=SUB.er, tand=SUB.tand,
                   finish="immersion silver", copper_oz=1, **STACK),
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
