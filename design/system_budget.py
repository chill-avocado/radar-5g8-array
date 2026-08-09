"""What limits this radar, and what going active would actually buy.

Two calculations decide the whole architecture of an active version:

  1. Detection range, which says how much an amplifier is worth.
  2. Transmit leakage into the receiver, which says whether the amplifier can
     be fitted at all.  This board measures -41 dB of transmit-to-receive
     isolation.  Raise the transmit power and that number, not the noise
     figure, becomes the thing that decides the design.
"""

import math

C0 = 299792458.0
K_BOLTZ = 1.380649e-23
T0 = 290.0

F0 = 5.80e9
LAM = C0 / F0
G_ELEMENT_DBI = 6.1          # measured, embedded, converged mesh
N_TX, N_RX = 2, 2
ISOLATION_DB = -41.1         # measured on the whole board
B210_NF_DB = 8.0             # typical at the top of the B210's range
B210_MAX_DBM = 0.0           # absolute maximum at the receive port
B210_P1DB_DBM = -15.0        # 1 dB compression
SIGMA_M2 = 0.01              # small quadcopter at 5.8 GHz
T_CPI_S = 0.10               # coherent processing interval
SNR_MIN_DB = 13.0
LOSS_DB = 3.0                # cable, connector, mismatch, processing


def dbm(w):
    return 10.0 * math.log10(w * 1e3)


def w(dbm_):
    return 10.0 ** (dbm_ / 10.0) / 1e3


def detection_range(pt_dbm, nf_db, g_tx_dbi, g_rx_dbi):
    """Range at which a target of SIGMA_M2 reaches SNR_MIN after processing."""
    # MIMO coherent gain: every transmitter is heard by every receiver
    mimo_gain_db = 10.0 * math.log10(N_TX * N_RX)
    num = (w(pt_dbm) * 10 ** ((g_tx_dbi + g_rx_dbi + mimo_gain_db) / 10.0)
           * LAM ** 2 * SIGMA_M2 * T_CPI_S)
    den = ((4.0 * math.pi) ** 3 * K_BOLTZ * T0
           * 10 ** ((nf_db + LOSS_DB + SNR_MIN_DB) / 10.0))
    return (num / den) ** 0.25


def leakage(pt_dbm, lna_gain_db=0.0):
    at_port = pt_dbm + ISOLATION_DB
    return at_port, at_port + lna_gain_db


def line(label, pt, nf, lna_g=0.0):
    r = detection_range(pt, nf, G_ELEMENT_DBI, G_ELEMENT_DBI)
    port, radio = leakage(pt, lna_g)
    flag = ("DAMAGES the radio" if radio > B210_MAX_DBM else
            "compresses the radio" if radio > B210_P1DB_DBM else "ok")
    print(f"  {label:<34} {pt:+5.0f} dBm  NF {nf:4.1f} dB   "
          f"range {r:6.0f} m   leakage into radio {radio:+6.1f} dBm  {flag}")
    return r


if __name__ == "__main__":
    print(f"5.8 GHz, {SIGMA_M2} m2 target, {T_CPI_S*1000:.0f} ms coherent "
          f"integration, {SNR_MIN_DB:.0f} dB detection threshold")
    print(f"measured: element gain {G_ELEMENT_DBI} dBi, "
          f"transmit-to-receive isolation {ISOLATION_DB} dB\n")

    base = line("passive board, B210 alone", 10, B210_NF_DB)
    print()
    r_lna = line("+ receive amplifier only", 10, 2.0, 20.0)
    print()
    for p in (20, 25, 30, 33):
        line(f"+ transmit amplifier at {p:+d} dBm", p, 2.0, 20.0)

    print(f"\n  range multiplier from a receive amplifier alone: "
          f"{r_lna/base:.2f}x")
    print("\nwhat isolation each transmit power needs, to keep the radio")
    print("below its 1 dB compression point through a 20 dB amplifier:")
    for p in (10, 20, 25, 30, 33):
        need = B210_P1DB_DBM - 20.0 - p
        print(f"   {p:+3d} dBm transmit -> needs {need:6.1f} dB isolation "
              f"({need - ISOLATION_DB:+.1f} dB more than this board has)")


# --------------------------------------------------------------------------
# What is actually buildable, given the isolation this board has
# --------------------------------------------------------------------------

LNA_NF_DB = 1.0
LNA_IN_P1DB_DBM = -10.0          # a typical 5 GHz low-noise amplifier


def cascade_nf(lna_nf_db, lna_gain_db, back_nf_db):
    f1 = 10 ** (lna_nf_db / 10.0)
    g1 = 10 ** (lna_gain_db / 10.0)
    f2 = 10 ** (back_nf_db / 10.0)
    return 10.0 * math.log10(f1 + (f2 - 1.0) / g1)


def survey():
    print("\nreceive amplifier gain, at +10 dBm transmit and the measured "
          "-41.1 dB isolation:")
    print(f"  {'gain':>5} {'system NF':>10} {'range':>8} {'into LNA':>10} "
          f"{'into radio':>11}   verdict")
    best = None
    for g in (0, 6, 9, 12, 15, 18, 21, 24):
        nf = cascade_nf(LNA_NF_DB, g, B210_NF_DB) if g else B210_NF_DB
        r = detection_range(10, nf, G_ELEMENT_DBI, G_ELEMENT_DBI)
        at_lna, at_radio = leakage(10, g)
        ok = at_lna < LNA_IN_P1DB_DBM and at_radio < B210_P1DB_DBM
        if ok and (best is None or r > best[1]):
            best = (g, r, nf)
        print(f"  {g:4d} dB {nf:9.2f} dB {r:7.0f} m {at_lna:9.1f} dBm "
              f"{at_radio:10.1f} dBm   {'ok' if ok else 'saturates'}")
    if best:
        g, r, nf = best
        base = detection_range(10, B210_NF_DB, G_ELEMENT_DBI, G_ELEMENT_DBI)
        print(f"\n  best that fits inside the isolation this board already "
              f"has: {g} dB of gain")
        print(f"  system noise figure {nf:.2f} dB, range {r:.0f} m "
              f"({r/base:.2f}x the passive board), no cancellation needed")


def cancellation():
    print("\nwith transmit leakage cancellation, which is what a continuous-"
          "wave radar\nnormally uses: a sample of the transmit signal is "
          "adjusted in amplitude and\nphase and injected into the receive "
          "path in antiphase.")
    base = detection_range(10, B210_NF_DB, G_ELEMENT_DBI, G_ELEMENT_DBI)
    print(f"  {'cancellation':>13} {'effective isolation':>21} "
          f"{'safe transmit power':>21} {'range':>8}")
    for c in (0, 15, 25, 35):
        iso = ISOLATION_DB - c
        pt = B210_P1DB_DBM - 12.0 - iso        # 12 dB amplifier
        pt = min(pt, 33.0)
        r = detection_range(pt, cascade_nf(LNA_NF_DB, 12.0, B210_NF_DB),
                            G_ELEMENT_DBI, G_ELEMENT_DBI)
        print(f"  {c:10d} dB {iso:18.1f} dB {pt:18.1f} dBm {r:7.0f} m"
              f"   ({r/base:.1f}x)")


survey()
cancellation()
