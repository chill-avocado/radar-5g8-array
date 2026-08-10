#!/usr/bin/env bash
#=============================================================================
# run_back.sh -- lint, build and run the radar back-end testbench
#
#   1. verilator --lint-only -Wall on every module; any warning is an error
#   2. verilate each module into its own C++ model
#   3. build and run fpga/sim/tb_back.cpp against all four
#
# Exits non-zero if any step or any test fails.
#=============================================================================
set -u -o pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
RTL="$HERE/../rtl"
BUILD="$HERE/build"
VERILATOR="${VERILATOR:-/usr/local/bin/verilator}"

MODULES=(radar_corner_turn radar_power radar_cfar2d radar_pack)
PREFIX=(Vct Vpw Vcf Vpk)

fail() { echo "FAIL: $*" >&2; exit 1; }

command -v "$VERILATOR" >/dev/null 2>&1 || fail "verilator not found at $VERILATOR"

echo "== verilator $("$VERILATOR" --version) =="
echo

#-----------------------------------------------------------------------------
# 1. Lint
#-----------------------------------------------------------------------------
lint_bad=0
for m in "${MODULES[@]}"; do
    if "$VERILATOR" --lint-only -Wall -Wno-DECLFILENAME "$RTL/$m.v" 2>&1 | tee "/tmp/lint_$m.log" | grep -q '%\(Error\|Warning\)'; then
        echo "LINT FAIL   $m.v"
        cat "/tmp/lint_$m.log"
        lint_bad=1
    else
        echo "LINT PASS   $m.v"
    fi
done
echo
[ "$lint_bad" -eq 0 ] || fail "lint reported problems"

#-----------------------------------------------------------------------------
# 2. Verilate
#-----------------------------------------------------------------------------
rm -rf "$BUILD"
mkdir -p "$BUILD"

for i in "${!MODULES[@]}"; do
    m="${MODULES[$i]}"
    p="${PREFIX[$i]}"
    "$VERILATOR" --cc --Mdir "$BUILD/obj_$p" --prefix "$p" \
        -O2 -Wno-fatal --x-assign 0 --x-initial 0 \
        --top-module "$m" "$RTL/$m.v" \
        || fail "verilate $m"
    make -s -C "$BUILD/obj_$p" -f "$p.mk" >/dev/null || fail "build $m model"
    echo "BUILT       $m -> $p"
done
echo

#-----------------------------------------------------------------------------
# 3. Compile and link the testbench
#-----------------------------------------------------------------------------
VROOT="$("$VERILATOR" --getenv VERILATOR_ROOT)"
VINC="$VROOT/include"

INCS=(-I"$VINC" -I"$VINC/vltstd")
LIBS=()
for p in "${PREFIX[@]}"; do
    INCS+=(-I"$BUILD/obj_$p")
    LIBS+=("$BUILD/obj_$p/$p__ALL.a")
done

CXX="${CXX:-c++}"
"$CXX" -std=c++17 -O2 -Wall -Wno-unused-parameter \
    "${INCS[@]}" \
    "$HERE/tb_back.cpp" \
    "$VINC/verilated.cpp" \
    "$VINC/verilated_threads.cpp" \
    "${LIBS[@]}" \
    -o "$BUILD/tb_back" || fail "compile tb_back.cpp"

echo "BUILT       tb_back"
echo

#-----------------------------------------------------------------------------
# 4. Run
#-----------------------------------------------------------------------------
"$BUILD/tb_back"
rc=$?
echo
if [ $rc -ne 0 ]; then
    echo "RESULT: FAIL"
else
    echo "RESULT: PASS"
fi
exit $rc
