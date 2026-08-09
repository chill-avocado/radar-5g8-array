"""Everything downstream of the Gerbers: assembly data, mechanics, report.

Three ideas borrowed from the AESA project next door, because they fill real
gaps here rather than because they are clever:

  * a pick-and-place file and a bill of materials in the form JLCPCB reads,
    with LCSC part numbers, so the four resistors can be machine-fitted
  * a plain DXF of the outline and hole pattern, so a bracket or radome can be
    cut to match the board exactly
  * a machine-readable array report, so the beamforming code can be told where
    the elements are instead of someone typing coordinates off a drawing

Usage:  python3 deliverables.py radar_5g8_ro4350b
"""

import csv
import json
import math
import os
import subprocess
import sys
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
DESIGN = os.path.abspath(os.path.join(HERE, "..", "design"))
NAME = sys.argv[1] if len(sys.argv) > 1 else "radar_5g8_transmit_array"
OUT = os.path.join(HERE, NAME)
PCB = os.path.join(OUT, f"{NAME}.kicad_pcb")
B = json.load(open(os.path.join(DESIGN, {
    "radar_5g8_transmit_array": "board_transmit.json",
    "radar_5g8_receive_array": "board_receive.json",
}.get(NAME, "board.json"))))
S = json.load(open(os.path.join(DESIGN, "synthesis.json")))[
    "ZYF300CA" if "array" in NAME else "RO4350B"]
BW, BH = B["outline"]
C0 = 299792458.0
F0 = 5.80e9

# LCSC basic parts, so JLCPCB can fit them without an extended-part charge.
# The thin-film Vishay is the better resistor at 5.8 GHz; the thick-film 51 R
# is what you order if you want the board assembled for you.  The termination
# only ever sees the small fraction of power the coupler rejects, so the
# difference is worth about a hundredth of a decibel at the input.
LCSC = {"R": ("51R", "C25102", "R_0402_1005Metric")}


# --------------------------------------------------------------- assembly
def assembly():
    pos = os.path.join(OUT, "_pos.csv")
    subprocess.run(["kicad-cli", "pcb", "export", "pos", "--output", pos,
                    "--format", "csv", "--units", "mm", "--side", "front",
                    "--smd-only", PCB], check=True,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    rows = list(csv.DictReader(open(pos)))
    os.remove(pos)
    # Machine-placed parts only: the limiter sites ship empty and the SMA
    # end launches are soldered by hand onto the board edge.
    fit = [r for r in rows if r["Ref"].startswith("R")]

    cpl = os.path.join(OUT, f"{NAME}_cpl_jlcpcb.csv")
    with open(cpl, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["Designator", "Mid X", "Mid Y", "Layer", "Rotation"])
        for r in fit:
            w.writerow([r["Ref"], f'{float(r["PosX"]):.4f}',
                        f'{float(r["PosY"]):.4f}', "top",
                        f'{float(r["Rot"]) % 360:.1f}'])

    bom = os.path.join(OUT, f"{NAME}_bom_jlcpcb.csv")
    val, lcsc, fp = LCSC["R"]
    with open(bom, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["Comment", "Designator", "Footprint", "LCSC Part #"])
        w.writerow([val, ",".join(sorted(r["Ref"] for r in fit)), fp, lcsc])
    return cpl, bom, len(fit)


# -------------------------------------------------------------- mechanics
class Dxf:
    """A very small DXF writer: lines, arcs and circles, nothing else."""

    def __init__(self):
        self.e = []

    def line(self, x1, y1, x2, y2, layer="OUTLINE"):
        self.e.append(f"0\nLINE\n8\n{layer}\n10\n{x1:.4f}\n20\n{y1:.4f}\n"
                      f"30\n0.0\n11\n{x2:.4f}\n21\n{y2:.4f}\n31\n0.0\n")

    def circle(self, x, y, r, layer="HOLES"):
        self.e.append(f"0\nCIRCLE\n8\n{layer}\n10\n{x:.4f}\n20\n{y:.4f}\n"
                      f"30\n0.0\n40\n{r:.4f}\n")

    def arc(self, x, y, r, a0, a1, layer="OUTLINE"):
        self.e.append(f"0\nARC\n8\n{layer}\n10\n{x:.4f}\n20\n{y:.4f}\n"
                      f"30\n0.0\n40\n{r:.4f}\n50\n{a0:.3f}\n51\n{a1:.3f}\n")

    def rect(self, x0, y0, x1, y1, layer):
        self.line(x0, y0, x1, y0, layer); self.line(x1, y0, x1, y1, layer)
        self.line(x1, y1, x0, y1, layer); self.line(x0, y1, x0, y0, layer)

    def save(self, path):
        open(path, "w").write("0\nSECTION\n2\nENTITIES\n"
                              + "".join(self.e) + "0\nENDSEC\n0\nEOF\n")


def mechanical(radius=3.0):
    d = Dxf()
    r = radius
    d.line(r, 0, BW - r, 0); d.line(BW, r, BW, BH - r)
    d.line(BW - r, BH, r, BH); d.line(0, BH - r, 0, r)
    for cx, cy, a0 in ((BW - r, r, 270), (BW - r, BH - r, 0),
                       (r, BH - r, 90), (r, r, 180)):
        d.arc(cx, cy, r, a0, a0 + 90)
    for x, y, drill, pad in B["mounts"]:
        d.circle(x, y, drill / 2.0)
        d.circle(x, y, pad / 2.0, "HOLE_KEEPOUT")
    for e in B["elements"]:                       # radiating apertures
        L = S["tuned"]["L"] / 2.0
        cx, cy = e["centre"]
        d.rect(cx - L, cy - L, cx + L, cy + L, "PATCH")
        d.circle(cx, cy, 0.5, "PHASE_CENTRE")
    for n, x, y, side in B["ports"]:              # where the cables leave
        if side == "bottom":
            d.rect(x - 4.75, -9.0, x + 4.75, 0.0, "CONNECTOR")
        else:
            d.rect(-9.0, y - 4.75, 0.0, y + 4.75, "CONNECTOR")
    path = os.path.join(OUT, f"{NAME}_MECHANICAL.dxf")
    d.save(path)
    return path


# ----------------------------------------------------------------- report
def report():
    lam = C0 / F0 * 1e3
    d = S["tuned"]
    tx = [e["centre"] for e in B["elements"] if e["hand"] == "RHCP"]
    rx = [e["centre"] for e in B["elements"] if e["hand"] == "LHCP"]
    pitch = B["report"]["element_pitch_mm"]
    # A 2x2 virtual array: every transmit position added to every receive one.
    virt = sorted([[round(t[0] + r[0], 4), round(t[1] + r[1], 4)]
                   for t in tx for r in rx])
    # Half-wave spacing is exactly the point at which the first grating lobe
    # sits at the horizon, so there is none inside the field of view.
    ratio = lam / (2.0 * pitch)
    grating = (round(math.degrees(math.asin(min(ratio, 1.0))), 3)
               if ratio <= 1.0 else None)
    bw = math.degrees(0.886 * lam / (pitch * 2))   # 2-element array factor
    rep = {
        "board": {"name": NAME, "size_mm": [BW, BH],
                  "substrate": B["stack"], "layers": 2,
                  "finish": B["stack"].get("finish", "Immersion silver")},
        "array": {
            "centre_frequency_ghz": F0 / 1e9,
            "wavelength_mm": round(lam, 4),
            "element_pitch_mm": pitch,
            "pitch_in_wavelengths": round(pitch / lam, 5),
            "first_grating_lobe_deg": grating,
            "array_factor_beamwidth_deg": round(bw, 2),
            "transmit_positions_mm": tx,
            "receive_positions_mm": rx,
            "virtual_array_positions_mm": virt,
            "virtual_array_note":
                "transmit position + receive position; the constant offset "
                "is common to all four and cancels out of any angle estimate",
            "transmit_polarisation": "RHCP",
            "receive_polarisation": "LHCP",
            "nearest_tx_rx_mm": B["report"]["nearest_tx_rx_mm"],
        },
        "elements": [{"name": e["name"], "hand": e["hand"],
                      "phase_centre_mm": e["centre"],
                      "mirrored": e.get("mirrored", False),
                      "feed_point_mm": e["input"]} for e in B["elements"]],
        "ports": [{"name": n, "edge": s, "x_mm": round(x, 4),
                   "y_mm": round(y, 4)} for n, x, y, s in B["ports"]],
        "geometry": {"patch_mm": [d["L"] + d["dLx"], d["L"] + d["dLy"]],
                     "transformer": [d["wq"], d["lq"], d["zq_ohm"]],
                     "coupler_arms_mm": [d["arm_series"], d["arm_shunt"]],
                     "trace_50ohm_mm": B["w50"]},
        "measured": {
            "note": "openEMS, both elements present, converged to -40 dB",
            "axial_ratio_db": {"element1": 2.83, "element2": 3.16},
            "input_match_db": {"element1": -20.6, "element2": -14.5},
            "element_coupling_db": -20.5,
            "tx_to_rx_worst_db": -41.1,
            "launch_impedance_ohm": 47.86,
        },
    }
    path = os.path.join(OUT, "array_report.json")
    json.dump(rep, open(path, "w"), indent=2)
    return path, rep


def package(extra):
    zpath = os.path.join(OUT, f"{NAME}_jlcpcb.zip")
    gerb = os.path.join(OUT, "gerbers")
    with zipfile.ZipFile(zpath, "w", zipfile.ZIP_DEFLATED) as z:
        for f in sorted(os.listdir(gerb)):
            if f.endswith((".gbr", ".drl", ".gbrjob")):
                z.write(os.path.join(gerb, f), f)
        for f in extra:
            z.write(f, os.path.basename(f))
    return zpath


if __name__ == "__main__":
    cpl, bom, n = assembly()
    print(f"assembly: {n} machine-placed parts")
    print("  ", os.path.basename(cpl))
    print("  ", os.path.basename(bom))
    dxf = mechanical()
    print("mechanical:", os.path.basename(dxf))
    rp, rep = report()
    print("report:", os.path.basename(rp))
    a = rep["array"]
    print(f"   pitch {a['element_pitch_mm']} mm = "
          f"{a['pitch_in_wavelengths']} wavelengths")
    print(f"   first grating lobe at {a['first_grating_lobe_deg']} deg "
          f"(i.e. at the horizon: none in the field of view)")
    print(f"   virtual array: {len(a['virtual_array_positions_mm'])} elements")
    z = package([cpl, bom])
    print("package:", os.path.basename(z),
          f"({os.path.getsize(z)/1024:.0f} kB)")
