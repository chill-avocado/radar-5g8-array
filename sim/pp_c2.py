"""Re-post-process a completed couple2 run: adds the amplitude/phase split."""
import json, os, sys, tempfile, runpy
import numpy as np
tag = sys.argv[1] if len(sys.argv) > 1 else "c3"
sys.argv = ["couple2.py", tag]
g = runpy.run_path("couple2.py", run_name="pp")
ports, nf2ff, H, FFREQ = g["ports"], g["nf2ff"], g["H"], g["FFREQ"]
PITCH = g["PITCH"]
sp = os.path.join(tempfile.gettempdir(), "oems_c2_" + tag)
f = np.linspace(5.4e9, 6.2e9, 161)
for p in ports:
    p.CalcPort(sp, f, ref_impedance=50)
D = g["DRIVE"]; O = 1 - D
s11 = ports[D].uf_ref / ports[D].uf_inc
s21 = ports[O].uf_ref / ports[D].uf_inc
theta = np.arange(-90, 90.5, 1.0)
print(f"{'freq':>8} {'S11':>8} {'S21':>8} {'AR':>7} {'Ey/Ex':>8} {'phase':>9}")
for ff in FFREQ:
    r = nf2ff.CalcNF2FF(sp, ff, theta, [0.0, 90.0],
                        center=[PITCH/2*1e-3, 0, H/2*1e-3],
                        outfile=f"nfc_{tag}_{ff/1e9:.3f}.h5",
                        read_cached=False, verbose=0)
    Et = np.array(r.E_theta[0]); Ep = np.array(r.E_phi[0])
    ER_ = (Et + 1j*Ep)/np.sqrt(2.0); EL_ = (Et - 1j*Ep)/np.sqrt(2.0)
    i0 = int(np.argmin(np.abs(theta)))
    Ex, Ey = Et[i0, 0], Et[i0, 1]
    amp = 20*np.log10(abs(Ey)/abs(Ex))
    dph = (np.degrees(np.angle(Ey)-np.angle(Ex))+180) % 360 - 180
    a, b = abs(ER_[i0,0]), abs(EL_[i0,0])
    ar = 20*np.log10((a+b)/max(a-b,1e-15))
    k = int(np.argmin(np.abs(f-ff)))
    print(f"{ff/1e9:7.3f}G {20*np.log10(abs(s11[k])):7.2f} "
          f"{20*np.log10(abs(s21[k])):7.2f} {ar:6.2f} {amp:+7.2f} {dph:+8.1f}")
