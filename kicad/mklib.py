"""Split the unique footprints out of the saved board into a .pretty library
and register it, so the KiCad sources are self-contained rather than relying
on footprints that exist only inside the board file."""
import json, os, re, sys
OUT = sys.argv[1]
pcb = [f for f in os.listdir(OUT) if f.endswith(".kicad_pcb")][0]
txt = open(os.path.join(OUT, pcb)).read()
LIB = os.path.join(OUT, "radar5g8.pretty")
os.makedirs(LIB, exist_ok=True)
for stale in os.listdir(LIB):          # drop anything from a previous run
    if stale.endswith(".kicad_mod"):
        os.remove(os.path.join(LIB, stale))


def blocks(t, key="\t(footprint "):
    i, out = 0, []
    while True:
        i = t.find(key, i)
        if i < 0:
            return out
        d, j = 0, i + 1
        while True:
            if t[j] == '"':
                j = t.index('"', j + 1)
            elif t[j] == "(":
                d += 1
            elif t[j] == ")":
                d -= 1
                if d == 0:
                    break
            j += 1
        out.append(t[i + 1:j + 1])
        i = j


seen = {}
# Prefer an UNROTATED instance as the library master: KiCad stamps the parent
# footprint's angle onto each pad, so a copy taken from a rotated instance can
# never line up with the unrotated definition.
_all = blocks(txt)
_all.sort(key=lambda b: 0 if re.search(r'\(at [-0-9.]+ [-0-9.]+\)', b) else 1)
for b in _all:
    m = re.search(r'\(footprint\s+"([^"]+)"', b)
    if not m:
        continue
    name = m.group(1).split(":")[-1]
    if name in seen:
        continue
    # A library footprint is stored unrotated.  The board instance may be at
    # 90 degrees, and KiCad stamps that angle onto every pad and field inside
    # it, so strip it here or the library copy will never match the board.
    rot = re.search(r'\(at [-0-9.]+ [-0-9.]+ ([-0-9.]+)\)', b)
    body = re.sub(r'\(at [-0-9. ]+\)\n', '(at 0 0)\n', b, count=1)
    if rot:
        r_ = rot.group(1)
        body = re.sub(r'\(at ([-0-9.]+) ([-0-9.]+) ' + re.escape(r_) + r'\)',
                      r'(at \1 \2)', body)
    body = body.replace(f'(footprint "{m.group(1)}"',
                        f'(footprint "{name}"', 1)
    # A library footprint must not carry the net names or the reference
    # designator of the one instance it was lifted from, or it is unusable
    # for a re-spin.
    body = re.sub(r'\n\s*\(net "[^"]*"\)', '', body)
    body = re.sub(r'(\(property "Reference" )"[^"]*"', r'\1"REF**"', body, count=1)
    # A standalone .kicad_mod needs the version/generator header that a
    # footprint block inside a board file does not carry.  Without it KiCad
    # cannot line the library copy up with the board instance and reports
    # every one of them as out of sync.
    ver = re.search(r'\(version (\d+)\)', txt).group(1)
    body = re.sub(r'(\(footprint "[^"]+"\n)',
                  r'\1\t(version ' + ver + ')\n\t(generator "pcbnew")\n'
                  '\t(generator_version "10.0")\n', body, count=1)
    body = "\n".join(l[1:] if l.startswith("\t") else l
                     for l in body.splitlines())
    open(os.path.join(LIB, name + ".kicad_mod"), "w").write(body + "\n")
    seen[name] = True

tbl = os.path.join(OUT, "fp-lib-table")
open(tbl, "w").write(
    "(fp_lib_table\n  (version 7)\n"
    '  (lib (name "radar5g8")(type "KiCad")'
    '(uri "${KIPRJMOD}/radar5g8.pretty")(options "")(descr '
    '"5.8 GHz radar array footprints"))\n)\n')
print("library:", sorted(seen))

pro = os.path.join(OUT, pcb.replace(".kicad_pcb", ".kicad_pro"))
p = json.load(open(pro))
sev = p.setdefault("board", {}).setdefault("design_settings", {}) \
       .setdefault("rule_severities", {})
# Solder mask is deliberately omitted over the radiating elements and the
# end-launch transitions, so every pad and via inside those windows shares one
# opening. That is the intent, not a defect.
sev["solder_mask_bridge"] = "ignore"
# KiCad ships these five switched off.  Turn them on: this board should pass
# with everything enabled, and the only rule it may waive is the one above.
for k in ("missing_courtyard", "track_not_centered_on_via",
          "tuning_profile_track_geometries", "footprint_filters_mismatch",
          "footprint_type_mismatch"):
    sev[k] = "warning"
json.dump(p, open(pro, "w"), indent=2)
print("set solder_mask_bridge severity to ignore (deliberate mask openings)")
