"""Run the full synthesis and dump every dimension the layout needs."""

import json
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
from rfmath import (C0, RO4350B, FR4_1MM, ms_dispersive, ms_width_for_z0,
                    ms_guided_wavelength, ms_loss_db_per_m, cbcpw,
                    cbcpw_slot_for_z0, square_patch, patch_bandwidth_estimate,
                    branchline_sparams)

F0 = 5.80e9
BAND = (5.725e9, 5.875e9)      # 5.8 GHz ISM
MM = 1e-3


def banner(t):
    print("\n" + "=" * 74)
    print(t)
    print("=" * 74)


def sanity_checks():
    banner("SANITY CHECKS against published reference values")
    # 50 ohm on 1.6 mm FR-4 (er 4.4) is the classic ~2.9-3.0 mm.
    from rfmath import Substrate
    fr4_16 = Substrate("FR4 1.6", er=4.4, h=1.6e-3, t=35e-6, tand=0.02)
    w, z, ee = ms_width_for_z0(50.0, fr4_16, 1e9)
    print(f"  50 ohm on 1.6 mm FR-4 @1 GHz : w = {w/MM:.3f} mm   (expect ~2.9-3.0)")
    # 50 ohm on 0.762 mm RO4350B is a well-known ~1.65-1.70 mm.
    w, z, ee = ms_width_for_z0(50.0, RO4350B, F0)
    print(f"  50 ohm on RO4350B 0.762 @5.8 : w = {w/MM:.3f} mm   (Rogers MWI ~1.66-1.70)")
    print(f"                                 eps_eff = {ee:.4f}")
    # Static vs dispersive spread
    from rfmath import ms_static
    zs, ees = ms_static(w, RO4350B)
    print(f"  dispersion shift 0 -> 5.8 GHz: eps_eff {ees:.4f} -> {ee:.4f}, "
          f"Z0 {zs:.2f} -> {z:.2f} ohm")


def hybrid_phases():
    banner("BRANCH-LINE HYBRID -- exact 4-port solution")
    S = branchline_sparams(50.0 / math.sqrt(2.0), 50.0, math.pi / 2.0, 50.0)
    names = ["P1 (input)", "P2 (series arm)", "P3 (diagonal)", "P4 (shunt arm)"]
    print("  Drive P1 with ideal quarter-wave arms (Zs=35.36, Zp=50):")
    for i in range(4):
        m = abs(S[i, 0])
        ph = math.degrees(np.angle(S[i, 0]))
        print(f"    S{i+1}1 = {20*math.log10(max(m,1e-12)):7.2f} dB  "
              f"{ph:8.2f} deg   <- {names[i]}")
    ph2 = math.degrees(np.angle(S[1, 0]))
    ph4 = math.degrees(np.angle(S[3, 0]))
    print(f"\n  Phase(P2) - Phase(P4) = {ph2-ph4:+.2f} deg")
    return ph2, ph4


def cp_handedness_note():
    banner("CIRCULAR-POLARISATION HANDEDNESS -- derivation")
    print("""  A square patch fed on two orthogonal edges radiates, at boresight (+z):
        E = x_hat * A_x  +  y_hat * A_y
  Take A_y = A_x * exp(-j90deg)  (the y-edge feed LAGS the x-edge feed):
        E(t) = x_hat*cos(wt) + y_hat*cos(wt-90) = x_hat*cos(wt) + y_hat*sin(wt)
  At t=0 the vector points along +x; a quarter cycle later along +y, so it
  turns x -> y. With the thumb of the RIGHT hand along the direction of
  propagation (+z), the fingers curl x -> y.  Therefore:

        y-edge LAGS x-edge by 90 deg   ==>  RHCP   (transmit array)
        y-edge LEADS x-edge by 90 deg  ==>  LHCP   (receive array)

  On the board the receive element is the transmit element MIRRORED, which
  reverses the handedness exactly while keeping every electrical length
  identical, then rotated 90 deg (rotation of a CP element only adds a
  constant phase, common to both receive channels, so it cancels).""")


def design_for(sub, label):
    banner(f"DESIGN: {label}   [{sub}]")
    lam0 = C0 / F0
    print(f"  Free-space wavelength at 5.800 GHz : {lam0/MM:.3f} mm")
    print(f"  Half-wavelength element spacing    : {lam0/2/MM:.3f} mm")

    d = {}
    d["substrate"] = {"name": sub.name, "er": sub.er, "h_mm": sub.h / MM,
                      "t_um": sub.t * 1e6, "tand": sub.tand}
    d["f0_hz"] = F0
    d["lambda0_mm"] = lam0 / MM
    d["spacing_mm"] = lam0 / 2.0 / MM

    # --- patch -------------------------------------------------------------
    p = square_patch(sub, F0)
    bw = patch_bandwidth_estimate(sub, F0, p["W"], p["L"])
    print(f"\n  -- Square patch --")
    print(f"     side L = W                      : {p['L']/MM:.3f} mm")
    print(f"     eps_eff (wide strip)            : {p['eeff']:.4f}")
    print(f"     fringing extension dL           : {p['dL']/MM:.4f} mm")
    print(f"     slot conductance G1             : {p['G1']*1e3:.4f} mS")
    print(f"     mutual conductance G12          : {p['G12']*1e3:.4f} mS")
    print(f"     edge resistance Rin             : {p['Rin_edge']:.1f} ohm")
    print(f"     est. VSWR<2 bandwidth           : {bw*100:.2f} %  "
          f"({bw*F0/1e6:.0f} MHz)  [ISM band needs 2.59 %]")
    d["patch"] = {k: (v / MM if k in ("L", "W", "dL", "Leff", "lam0") else v)
                  for k, v in p.items()}
    d["patch"]["bw_pct"] = bw * 100

    # --- feed lines --------------------------------------------------------
    w50, z50, ee50 = ms_width_for_z0(50.0, sub, F0)
    zq = math.sqrt(50.0 * p["Rin_edge"])
    wq, zqa, eeq = ms_width_for_z0(zq, sub, F0)
    lam_q = ms_guided_wavelength(wq, sub, F0)
    lam_50 = ms_guided_wavelength(w50, sub, F0)
    print(f"\n  -- Feed lines --")
    print(f"     50 ohm width                    : {w50/MM:.3f} mm  "
          f"(eps_eff {ee50:.3f}, lam_g {lam_50/MM:.3f} mm)")
    print(f"     quarter-wave transformer Z      : {zq:.1f} ohm")
    print(f"     ... width                       : {wq/MM:.3f} mm")
    print(f"     ... length (lam_g/4)            : {lam_q/4/MM:.3f} mm")
    print(f"     50 ohm line loss                : {ms_loss_db_per_m(w50, sub, F0)/1e3*1e3:.3f} dB/m"
          f"  = {ms_loss_db_per_m(w50, sub, F0)*0.03:.3f} dB per 30 mm")
    d["feed"] = {"w50_mm": w50 / MM, "lam_g50_mm": lam_50 / MM,
                 "eeff50": ee50,
                 "zq_ohm": zq, "wq_mm": wq / MM, "lq_mm": lam_q / 4 / MM,
                 "loss_db_per_m": ms_loss_db_per_m(w50, sub, F0)}

    # --- branch-line hybrid ------------------------------------------------
    zs = 50.0 / math.sqrt(2.0)
    ws, zsa, ees = ms_width_for_z0(zs, sub, F0)
    lam_s = ms_guided_wavelength(ws, sub, F0)
    lam_p = ms_guided_wavelength(w50, sub, F0)
    print(f"\n  -- 90 deg branch-line hybrid --")
    print(f"     series arms  Z = {zs:.2f} ohm      : w {ws/MM:.3f} mm, "
          f"l {lam_s/4/MM:.3f} mm")
    print(f"     shunt  arms  Z = 50.00 ohm      : w {w50/MM:.3f} mm, "
          f"l {lam_p/4/MM:.3f} mm")
    # Physical ring dimensions are centre-to-centre of the arms.
    ring_x = lam_s / 4.0
    ring_y = lam_p / 4.0
    print(f"     ring centre-to-centre           : {ring_x/MM:.3f} x {ring_y/MM:.3f} mm")
    print(f"     outer footprint                 : "
          f"{(ring_x + w50)/MM:.3f} x {(ring_y + ws)/MM:.3f} mm")
    d["hybrid"] = {"z_series": zs, "w_series_mm": ws / MM,
                   "l_series_mm": lam_s / 4 / MM,
                   "z_shunt": 50.0, "w_shunt_mm": w50 / MM,
                   "l_shunt_mm": lam_p / 4 / MM,
                   "ring_x_mm": ring_x / MM, "ring_y_mm": ring_y / MM}

    # --- SMA end-launch pad ------------------------------------------------
    # Keep the signal pad at the 50 ohm microstrip width and open the coplanar
    # ground so the launch stays 50 ohm as a conductor-backed CPW.
    s_gap, z_cpw = cbcpw_slot_for_z0(w50, 50.0, sub)
    print(f"\n  -- SMA end-launch (conductor-backed CPW section) --")
    print(f"     signal pad width                : {w50/MM:.3f} mm")
    print(f"     coplanar ground gap for 50 ohm  : {s_gap/MM:.3f} mm  "
          f"(Z = {z_cpw:.2f} ohm)")
    d["launch"] = {"w_mm": w50 / MM, "gap_mm": s_gap / MM}

    # --- band edges --------------------------------------------------------
    print(f"\n  -- Across the 5.725-5.875 GHz band --")
    for f in (BAND[0], F0, BAND[1]):
        z, ee = ms_dispersive(w50, sub, f)
        lg = C0 / (f * math.sqrt(ee))
        elec = 90.0 * (F0 / f) ** 0 * (lam_p / 4.0) / (lg / 4.0)
        print(f"     {f/1e9:.3f} GHz : Z0 {z:.2f} ohm, lam_g {lg/MM:.3f} mm, "
              f"hybrid arm = {elec:.2f} deg")

    return d


def isolation_budget(sep_mm):
    banner("TX-TO-RX ISOLATION BUDGET")
    lam0 = C0 / F0 / MM
    r = sep_mm
    # Free-space path term between two ~6.5 dBic patches, boresight-to-boresight
    # is not the coupling path; co-planar patches see each other through their
    # pattern edge (~ -10 dB relative to peak each).
    fspl = 20 * math.log10(lam0 / (4 * math.pi * r))
    g_edge = 6.5 - 12.0        # gain of a patch toward its own ground plane
    coupling = fspl + 2 * g_edge
    print(f"  Nearest TX-to-RX patch separation  : {r:.1f} mm = {r/lam0:.2f} lambda")
    print(f"  Space-wave path term               : {fspl:.1f} dB")
    print(f"  Two patch edge-gains (~{g_edge:+.1f} dBi each) : {2*g_edge:+.1f} dB")
    print(f"  Predicted space-wave coupling      : {coupling:.1f} dB")
    print(f"  Cross-polar (RHCP TX vs LHCP RX)   : -6 to -10 dB extra in the")
    print(f"                                       near field (not the 30+ dB")
    print(f"                                       you get in the far field)")
    print(f"  Realistic port-to-port isolation   : {coupling-6:.0f} to {coupling-10:.0f} dB")
    print(f"""
  What that means for the radio: with the B210 transmitting +10 dBm, the
  leakage arriving at the receive port is about {10+coupling-8:.0f} dBm. The B210
  receiver saturates near -15 dBm, so there is roughly {abs(10+coupling-8)-15:.0f} dB of headroom.
  The leakage lands at zero beat frequency and is removed by the usual
  high-pass / range-bin-zero rejection in the FMCW processing chain.""")


if __name__ == "__main__":
    sanity_checks()
    hybrid_phases()
    cp_handedness_note()
    out = {}
    out["RO4350B"] = design_for(RO4350B, "PRIMARY - Rogers RO4350B 0.762 mm")
    out["FR4"] = design_for(FR4_1MM, "ALTERNATE - FR-4 1.0 mm")
    isolation_budget(92.08)   # as laid out in array.py
    here = os.path.dirname(os.path.abspath(__file__))
    # MERGE, never replace.  This script only knows how to generate the
    # Rogers and FR-4 entries, but the file also carries the PTFE variant the
    # boards are actually built from, which was synthesised separately.  A
    # plain dump would silently delete it and the next board build would come
    # out on the wrong substrate.
    path = os.path.join(here, "synthesis.json")
    if os.path.exists(path):
        existing = json.load(open(path))
        existing.update(out)
        out = existing
    with open(path, "w") as f:
        json.dump(out, f, indent=2)
    print(f"\nWritten {os.path.join(here, 'synthesis.json')}")
