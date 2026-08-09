#!/usr/bin/env bash
#=============================================================================
# run_fft.sh -- lint, build and verify fpga/rtl/radar_fft.v
#
#   1. lint radar_fft.v and radar_bitrev.v with -Wall, at every transform size
#      the design is used at and with both output orderings
#   2. verilate the FFT four times, N = 1024 / 512 / 256 / 128
#   3. build fpga/sim/tb_fft.cpp against all four models
#   4. run it
#
# Exits non-zero if the lint finds anything or if any test fails.
#=============================================================================
set -u
set -o pipefail

HERE="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
RTL="$HERE/../rtl"
BUILD="$HERE/../build/fft"
VERILATOR="${VERILATOR:-/usr/local/bin/verilator}"
JOBS="${JOBS:-4}"

SIZES=("1024 10" "512 9" "256 8" "128 7")

if [ ! -x "$VERILATOR" ]; then
    VERILATOR="$(command -v verilator || true)"
fi
if [ -z "$VERILATOR" ] || [ ! -x "$VERILATOR" ]; then
    echo "FAIL  verilator not found (set VERILATOR=/path/to/verilator)"
    exit 1
fi

echo "======================================================================"
echo " radar_fft verification"
echo " verilator : $($VERILATOR --version)"
echo " rtl       : $RTL"
echo "======================================================================"

#-----------------------------------------------------------------------------
# 1. Lint
#-----------------------------------------------------------------------------
echo
echo "--- lint (-Wall -Wno-DECLFILENAME) ---"
LINT_FAIL=0

lint() {
    local label="$1"; shift
    if "$VERILATOR" --lint-only -Wall -Wno-DECLFILENAME "+incdir+$RTL" "$@" \
            "$RTL/radar_fft.v" "$RTL/radar_bitrev.v" >/tmp/radar_fft_lint.$$ 2>&1; then
        echo "PASS  lint  $label"
    else
        echo "FAIL  lint  $label"
        cat /tmp/radar_fft_lint.$$
        LINT_FAIL=1
    fi
    rm -f /tmp/radar_fft_lint.$$
}

for s in "${SIZES[@]}"; do
    set -- $s
    lint "radar_fft N=$1 NLOG2=$2 natural output" \
         --top-module radar_fft -GN="$1" -GNLOG2="$2" -GNATURAL_OUT=1
done
set -- 1024 10
lint "radar_fft N=1024 bit-reversed output" \
     --top-module radar_fft -GN=1024 -GNLOG2=10 -GNATURAL_OUT=0

if "$VERILATOR" --lint-only -Wall -Wno-DECLFILENAME --top-module radar_bitrev \
        "$RTL/radar_bitrev.v" >/tmp/radar_br_lint.$$ 2>&1; then
    echo "PASS  lint  radar_bitrev standalone"
else
    echo "FAIL  lint  radar_bitrev standalone"
    cat /tmp/radar_br_lint.$$
    LINT_FAIL=1
fi
rm -f /tmp/radar_br_lint.$$

if [ "$LINT_FAIL" -ne 0 ]; then
    echo
    echo "LINT FAILED"
    exit 1
fi

#-----------------------------------------------------------------------------
# 2. Verilate one model per transform size
#-----------------------------------------------------------------------------
echo
echo "--- build ---"
rm -rf "$BUILD"
mkdir -p "$BUILD"

INCS=()
LIBS=()
for s in "${SIZES[@]}"; do
    set -- $s
    n="$1"; l="$2"
    if ! "$VERILATOR" --cc -O3 --top-module radar_fft \
            -Mdir "$BUILD/obj_$n" --prefix "Vfft$n" \
            -GN="$n" -GNLOG2="$l" -GNATURAL_OUT=1 \
            "+incdir+$RTL" "$RTL/radar_fft.v" "$RTL/radar_bitrev.v" \
            --build -j "$JOBS" >"$BUILD/verilate_$n.log" 2>&1; then
        echo "FAIL  verilate N=$n"
        tail -30 "$BUILD/verilate_$n.log"
        exit 1
    fi
    echo "PASS  verilate N=$n"
    INCS+=("-I$BUILD/obj_$n")
    LIBS+=("$BUILD/obj_$n/libVfft$n.a")
done

# and once more with the reorder buffer switched off, so the bit-reversed
# output ordering is exercised too
if ! "$VERILATOR" --cc -O3 --top-module radar_fft \
        -Mdir "$BUILD/obj_rev1024" --prefix "Vfftrev1024" \
        -GN=1024 -GNLOG2=10 -GNATURAL_OUT=0 \
        "+incdir+$RTL" "$RTL/radar_fft.v" "$RTL/radar_bitrev.v" \
        --build -j "$JOBS" >"$BUILD/verilate_rev1024.log" 2>&1; then
    echo "FAIL  verilate N=1024 NATURAL_OUT=0"
    tail -30 "$BUILD/verilate_rev1024.log"
    exit 1
fi
echo "PASS  verilate N=1024 NATURAL_OUT=0"
INCS+=("-I$BUILD/obj_rev1024")
LIBS+=("$BUILD/obj_rev1024/libVfftrev1024.a")

VROOT="$($VERILATOR --getenv VERILATOR_ROOT)"
if ! c++ -std=gnu++17 -O2 -Wall -Wno-sign-compare \
        "${INCS[@]}" -I"$VROOT/include" -I"$VROOT/include/vltstd" \
        "$HERE/tb_fft.cpp" "${LIBS[@]}" "$BUILD/obj_1024/libverilated.a" \
        -o "$BUILD/tb_fft" >"$BUILD/link.log" 2>&1; then
    echo "FAIL  compile tb_fft.cpp"
    cat "$BUILD/link.log"
    exit 1
fi
echo "PASS  compile tb_fft.cpp"

#-----------------------------------------------------------------------------
# 3. Run
#-----------------------------------------------------------------------------
echo
echo "--- simulate ---"
"$BUILD/tb_fft"
RC=$?

echo
if [ "$RC" -eq 0 ]; then
    echo "======================================================================"
    echo " radar_fft: PASS"
    echo "======================================================================"
else
    echo "======================================================================"
    echo " radar_fft: FAIL"
    echo "======================================================================"
fi
exit "$RC"
