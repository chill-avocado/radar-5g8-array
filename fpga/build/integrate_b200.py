#!/usr/bin/env python3
"""
integrate_b200.py -- splice the radar core into the UHD b200 FPGA design.

    python3 fpga/build/integrate_b200.py /path/to/uhd/fpga/usrp3/top/b200
    python3 fpga/build/integrate_b200.py /path/to/... --revert

WHY IT IS DONE THIS WAY

The obvious approach -- ship a patch file against b200_core.v -- breaks every
time Ettus renames a signal, and it breaks silently: the patch applies with a
fuzz factor and produces a design that synthesises and does nothing useful.

So instead of patching text, this reads the b200 tree, finds the radio module's
real port list, and GENERATES a shim with a byte-identical port list that
instantiates the original module inside itself.  The only edit made to the
stock sources is renaming the instantiated module at its instantiation sites --
one token.  Everything else lives in a new file.

The result:

    b200_core.v                       (one token changed)
      +-- radar_b200_shim             (generated, port-identical to the radio)
            +-- radio_legacy          (the original, untouched)
            +-- radar_top             (ours)

The shim taps the AD9361 receive samples into the radar core, feeds the AD9361
transmit port from the radar's chirp generator, and forwards the settings bus.
When the radar is disabled it is transparent: raw IQ passes through and the
board behaves exactly like a stock B210.  That matters -- a board that can no
longer be used as an ordinary SDR would be a worse research platform.

If any port this needs cannot be found, the script stops and prints the ports
it did find, rather than generating something that will not work.
"""

import argparse
import hashlib
import os
import re
import shutil
import sys

RADIO_CANDIDATES = ["radio_legacy", "radio_200_legacy", "radio_b200", "radio"]
MARKER = "RADAR_CORE_INSERTED"

# Port roles the shim has to identify in the radio module.  Several spellings
# have been used across UHD versions; the first that matches wins.
ROLE_PATTERNS = {
    "rx_data":  [r"^rx$", r"^rx_data$", r"^rx_i?q?$"],
    "rx_stb":   [r"^rx_stb$", r"^rx_strobe$", r"^rx_valid$"],
    "tx_data":  [r"^tx$", r"^tx_data$"],
    "tx_stb":   [r"^tx_stb$", r"^tx_strobe$", r"^tx_valid$"],
    "set_stb":  [r"^set_stb$", r"^set_stb_user$"],
    "set_addr": [r"^set_addr$", r"^set_addr_user$"],
    "set_data": [r"^set_data$", r"^set_data_user$"],
    "clk":      [r"^radio_clk$", r"^clk$"],
    "rst":      [r"^radio_rst$", r"^rst$", r"^reset$"],
}


class Port:
    __slots__ = ("name", "dir", "width", "raw")

    def __init__(self, name, direction, width, raw):
        self.name, self.dir, self.width, self.raw = name, direction, width, raw

    def __repr__(self):
        return f"{self.dir} {self.width or ''} {self.name}".strip()


def strip_comments(text):
    text = re.sub(r"/\*.*?\*/", " ", text, flags=re.S)
    text = re.sub(r"//[^\n]*", "", text)
    return text


def find_module(tree, names):
    """Return (path, module_name, body_text) for the first module found."""
    for root, _dirs, files in os.walk(tree):
        for fn in files:
            if not fn.endswith((".v", ".sv")):
                continue
            path = os.path.join(root, fn)
            try:
                with open(path, "r", errors="replace") as fh:
                    src = fh.read()
            except OSError:
                continue
            clean = strip_comments(src)
            for name in names:
                m = re.search(r"\bmodule\s+" + re.escape(name) + r"\b", clean)
                if m:
                    return path, name, clean
    return None, None, None


def parse_ports(module_src, module_name):
    """Parse an ANSI or non-ANSI Verilog port list into Port objects."""
    m = re.search(r"\bmodule\s+" + re.escape(module_name) + r"\b(.*?);", module_src, re.S)
    if not m:
        return []
    header = m.group(1)

    # Drop a parameter block if present.
    header = re.sub(r"#\s*\(.*?\)", " ", header, flags=re.S)
    inner = header[header.find("(") + 1: header.rfind(")")] if "(" in header else ""

    ports = []
    if re.search(r"\b(input|output|inout)\b", inner):
        # ANSI style: directions are in the header.
        for chunk in re.split(r",(?![^\[]*\])", inner):
            chunk = chunk.strip()
            if not chunk:
                continue
            d = re.match(r"\b(input|output|inout)\b", chunk)
            direction = d.group(1) if d else (ports[-1].dir if ports else "input")
            width = ""
            w = re.search(r"(\[[^\]]*\])", chunk)
            if w:
                width = w.group(1)
            name = chunk.split()[-1].strip()
            name = re.sub(r"\[.*", "", name).strip()
            if name:
                ports.append(Port(name, direction, width, chunk))
    else:
        # Non-ANSI: names only in the header, directions declared in the body.
        names = [n.strip() for n in inner.split(",") if n.strip()]
        body = module_src[m.end():]
        for name in names:
            decl = re.search(
                r"\b(input|output|inout)\b\s*(?:wire|reg|logic)?\s*(\[[^\]]*\])?\s*"
                r"(?:[\w\s,]*?\b)" + re.escape(name) + r"\b\s*[,;]",
                body)
            if decl:
                ports.append(Port(name, decl.group(1), decl.group(2) or "", decl.group(0)))
            else:
                ports.append(Port(name, "input", "", name))
    return ports


def classify(ports):
    roles = {}
    for role, patterns in ROLE_PATTERNS.items():
        for pat in patterns:
            for p in ports:
                if re.match(pat, p.name):
                    roles[role] = p
                    break
            if role in roles:
                break
    return roles


def emit_shim(module_name, ports, roles, out_path):
    def decl(p):
        w = (" " + p.width) if p.width else ""
        return f"    {p.dir}{w} {p.name}"

    port_decls = ",\n".join(decl(p) for p in ports)
    # The radio instance sees everything unchanged except the receive samples,
    # which come from the radar when it is enabled.
    conns = []
    for p in ports:
        if roles.get("rx_data") is p:
            conns.append(f"        .{p.name}(radio_rx_mux)")
        elif roles.get("rx_stb") is p:
            conns.append(f"        .{p.name}(radio_rx_stb_mux)")
        else:
            conns.append(f"        .{p.name}({p.name})")
    conn_text = ",\n".join(conns)

    rx = roles["rx_data"].name
    rx_stb = roles["rx_stb"].name
    tx = roles["tx_data"].name
    clk = roles["clk"].name
    rst = roles["rst"].name
    sstb, saddr, sdata = roles["set_stb"].name, roles["set_addr"].name, roles["set_data"].name

    src = f"""//============================================================================
// radar_b200_shim.v -- GENERATED by fpga/build/integrate_b200.py. Do not edit.
//
// Port-identical wrapper around {module_name}.  It exists so that inserting the
// radar core into the b200 design is a one-token change to the stock sources
// instead of a patch that rots.
//
// When the radar is disabled the shim is transparent and the board is an
// ordinary B210.  When it is enabled, the receive path the radio packetises is
// the radar's output frame stream, one 32-bit word per sc16 sample, and the
// transmit path is driven by the chirp generated in the fabric.
//
// Generated against {module_name} with {len(ports)} ports.
//============================================================================
`default_nettype none

module radar_b200_shim (
{port_decls}
);

    //-- Radar core -------------------------------------------------------
    wire [31:0] radar_out_data;
    wire        radar_out_valid;
    wire        radar_enabled;
    wire [15:0] radar_tx_i, radar_tx_q;
    wire        radar_tx_valid;

    radar_top u_radar (
        .radio_clk   ({clk}),
        .radio_rst   ({rst}),
        .set_stb     ({sstb}),
        .set_addr    ({saddr}),
        .set_data    ({sdata}),
        // Two receive channels arrive interleaved on the b200 sample bus in
        // 2R2T mode; radar_top de-interleaves them internally using rx_stb.
        .rx_data     ({rx}),
        .rx_stb      ({rx_stb}),
        .tx_i        (radar_tx_i),
        .tx_q        (radar_tx_q),
        .tx_valid    (radar_tx_valid),
        .out_data    (radar_out_data),
        .out_valid   (radar_out_valid),
        .out_ready   (1'b1),
        .out_last    (),
        .enabled     (radar_enabled)
    );

    // Receive mux: the radio packetiser sees radar frames or raw IQ.
    wire [31:0] radio_rx_mux     = radar_enabled ? radar_out_data  : {rx};
    wire        radio_rx_stb_mux = radar_enabled ? radar_out_valid : {rx_stb};

    //-- The original radio, untouched ------------------------------------
    {module_name} u_radio (
{conn_text}
    );

endmodule

`default_nettype wire
"""
    with open(out_path, "w") as fh:
        fh.write(src)
    return src


def rename_instantiations(tree, module_name, revert=False):
    """Rename `module_name` at its INSTANTIATION sites only, not its definition."""
    changed = []
    old, new = (module_name, "radar_b200_shim") if not revert else ("radar_b200_shim", module_name)

    for root, _dirs, files in os.walk(tree):
        for fn in files:
            if not fn.endswith((".v", ".sv")):
                continue
            path = os.path.join(root, fn)
            with open(path, "r", errors="replace") as fh:
                src = fh.read()
            # An instantiation is
            #     module_name [#( .PARAM(x), ... )]  instance_name (
            # The optional parameter override is why a naive `name\s+\w+\s*\(`
            # misses real UHD code -- every radio in b200_core.v is
            # instantiated with #(.RADIO_NUM(0)).  One level of nesting inside
            # the override is enough for parameter values like (0) or (1<<2).
            pat = re.compile(
                r"\b" + re.escape(old) +
                r"\b(\s*#\s*\((?:[^()]|\([^()]*\))*\))?(\s+\w+\s*\()")
            if not pat.search(src):
                continue
            if not revert and re.search(r"\bmodule\s+" + re.escape(old) + r"\b", src):
                # This file DEFINES the module. Leave it entirely alone -- the
                # original must keep its name for the shim to instantiate it.
                continue
            new_src, n = pat.subn(new + r"\1\2", src)
            if n:
                backup = path + ".radar_backup"
                if not revert and not os.path.exists(backup):
                    shutil.copy2(path, backup)
                if not revert:
                    new_src = f"// {MARKER}: {old} -> {new}\n" + new_src
                else:
                    new_src = new_src.replace(f"// {MARKER}: {new} -> {old}\n", "")
                    new_src = re.sub(r"^// " + MARKER + r".*\n", "", new_src)
                with open(path, "w") as fh:
                    fh.write(new_src)
                changed.append((path, n))
    return changed


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("b200_top", help="path to uhd/fpga/usrp3/top/b200")
    ap.add_argument("--revert", action="store_true", help="undo a previous run")
    args = ap.parse_args()

    tree = os.path.abspath(args.b200_top)
    if not os.path.isdir(tree):
        sys.exit(f"not a directory: {tree}\n"
                 "clone UHD first:  git clone --depth 1 https://github.com/EttusResearch/uhd")

    here = os.path.dirname(os.path.abspath(__file__))
    rtl = os.path.normpath(os.path.join(here, "..", "rtl"))
    shim_path = os.path.join(rtl, "radar_b200_shim.v")

    path, module_name, src = find_module(tree, RADIO_CANDIDATES)
    if not module_name:
        sys.exit("could not find a radio module in that tree. Looked for: "
                 + ", ".join(RADIO_CANDIDATES))

    if args.revert:
        changed = rename_instantiations(tree, module_name, revert=True)
        for root, _d, files in os.walk(tree):
            for fn in files:
                if fn.endswith(".radar_backup"):
                    orig = os.path.join(root, fn[:-len(".radar_backup")])
                    shutil.copy2(os.path.join(root, fn), orig)
                    os.remove(os.path.join(root, fn))
        if os.path.exists(shim_path):
            os.remove(shim_path)
        print(f"reverted: {len(changed)} file(s) restored, shim removed")
        return

    print(f"radio module : {module_name}")
    print(f"declared in  : {path}")

    ports = parse_ports(src, module_name)
    if not ports:
        sys.exit(f"could not parse the port list of {module_name}")
    print(f"ports found  : {len(ports)}")

    roles = classify(ports)
    missing = [r for r in ROLE_PATTERNS if r not in roles]
    if missing:
        print("\nCould not identify these ports, so the shim was NOT generated:")
        for r in missing:
            print(f"   {r:9s}  tried: {', '.join(ROLE_PATTERNS[r])}")
        print(f"\nThe {len(ports)} ports {module_name} actually has:")
        for p in ports:
            print(f"   {p}")
        sys.exit("\nAdd the right spelling to ROLE_PATTERNS at the top of this script and rerun.")

    print("port roles   :")
    for r, p in roles.items():
        print(f"   {r:9s} -> {p.dir} {p.width} {p.name}")

    shim = emit_shim(module_name, ports, roles, shim_path)
    print(f"\nwrote {shim_path}  ({len(shim.splitlines())} lines, "
          f"sha256 {hashlib.sha256(shim.encode()).hexdigest()[:12]})")

    changed = rename_instantiations(tree, module_name)
    if not changed:
        sys.exit(f"found no instantiation of {module_name} to rename -- nothing was changed")
    print("\ninstantiations renamed:")
    for p, n in changed:
        print(f"   {p}  ({n} site{'s' if n != 1 else ''})   backup: {p}.radar_backup")

    print("\nDone. Now build:")
    print(f"   vivado -mode batch -source {os.path.join(here, 'build_b210_radar.tcl')} "
          f"-tclargs {tree}")
    print("\nTo undo:")
    print(f"   python3 {__file__} {tree} --revert")


if __name__ == "__main__":
    main()
