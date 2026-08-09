"""Post-process a completed full_element run without re-solving."""
import json, os, sys, tempfile, runpy
import numpy as np
tag = sys.argv[1] if len(sys.argv) > 1 else "fe0"
sys.argv = ["full_element.py", tag] + sys.argv[2:]
g = runpy.run_path("full_element.py", run_name="pp")
port, nf2ff, F0, FFREQ, H = g["port"], g["nf2ff"], g["F0"], g["FFREQ"], g["H"]
sp = os.path.join(tempfile.gettempdir(), "oems_fe_" + tag)

f = np.linspace(5.3e9, 6.3e9, 201)
port.CalcPort(sp, f, ref_impedance=50)
s11 = port.uf_ref / port.uf_inc
theta = np.arange(-90, 90.5, 1.0)
g_el = g["el"]
out = {"tag": tag, "f": f.tolist(),
       "dip_trim": g_el.dip_trim, "patch_Lx": g_el.Lx,
       "patch_Ly": g_el.Ly, "variant": g.get("VARIANT", "RO4350B"),
       "s11": [s11.real.tolist(), s11.imag.tolist()], "ff": {}}
print(f"{'freq':>9} {'S11':>8} {'Dmax':>8} {'Prad':>10} {'AR bore':>9}")
for ff in FFREQ:
    r = nf2ff.CalcNF2FF(sp, ff, theta, [0.0, 90.0],
                        center=[0, 0, H / 2 * 1e-3],
                        outfile=f"nf_{tag}_{ff/1e9:.3f}.h5",
                        read_cached=False, verbose=0)
    Et = np.array(r.E_theta[0])          # shape (n_theta, n_phi)
    Ep = np.array(r.E_phi[0])
    ER = (Et + 1j * Ep) / np.sqrt(2.0)   # right-hand component
    EL = (Et - 1j * Ep) / np.sqrt(2.0)   # left-hand component
    i0 = int(np.argmin(np.abs(theta)))
    # At boresight theta_hat at phi=0 is x_hat and at phi=90 is y_hat, so the
    # two columns are the two orthogonal linear components of the radiated
    # field. Their ratio and phase difference say whether a poor axial ratio
    # is an amplitude problem or a phase problem.
    Ex, Ey = Et[i0, 0], Et[i0, 1]
    amp = 20 * np.log10(abs(Ey) / abs(Ex))
    dph = (np.degrees(np.angle(Ey) - np.angle(Ex)) + 180) % 360 - 180
    print(f"          Ey/Ex = {amp:+.2f} dB   phase(Ey)-phase(Ex) = "
          f"{dph:+.1f} deg   (want -90 for RHCP)")
    a, b = abs(ER[i0, 0]), abs(EL[i0, 0])
    ar = 20 * np.log10((a + b) / max(a - b, 1e-15))
    k = int(np.argmin(np.abs(f - ff)))
    out["ff"][f"{ff/1e9:.3f}"] = {
        "Dmax_dBi": float(10 * np.log10(r.Dmax[0])),
        "Prad": float(r.Prad[0]), "AR_dB": float(ar),
        "xpol_dB": float(20 * np.log10(b / a)),
        "theta": theta.tolist(),
        "ER_E": np.abs(ER[:, 0]).tolist(), "EL_E": np.abs(EL[:, 0]).tolist(),
        "ER_H": np.abs(ER[:, 1]).tolist(), "EL_H": np.abs(EL[:, 1]).tolist()}
    print(f"{ff/1e9:8.3f}G {20*np.log10(abs(s11[k])):7.2f}dB "
          f"{10*np.log10(r.Dmax[0]):7.2f}dBi {r.Prad[0]:9.3e} {ar:8.2f}dB")
RESULTS = os.path.join(os.path.dirname(os.path.abspath(__file__)), "results")
json.dump(out, open(os.path.join(RESULTS, f"fe_{tag}.json"), "w"))
b = np.abs(s11) < 10**(-10/20)
if b.any():
    print(f"-10 dB match band: {f[b].min()/1e9:.3f} - {f[b].max()/1e9:.3f} GHz "
          f"({(f[b].max()-f[b].min())/1e6:.0f} MHz)")
