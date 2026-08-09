#!/bin/sh
#=============================================================================
# run_front.sh -- lint and simulate the 5.8 GHz radar front-end RTL.
#
#   1. lints every module with -Wall and fails immediately on any warning
#      (Verilator treats warnings as errors unless told otherwise, which is
#      exactly what is wanted here)
#   2. verilates the six modules the testbench drives, each into its own
#      model with its own prefix
#   3. builds and runs fpga/sim/tb_front.cpp against all six at once
#
# Exits non-zero if the lint finds anything or if any check in the testbench
# fails.  No arguments.  Override the tool with VERILATOR=... if it moves.
#=============================================================================
set -u

VERILATOR=${VERILATOR:-/usr/local/bin/verilator}
CXX=${CXX:-c++}

HERE=$(cd "$(dirname "$0")" && pwd)
RTL=$HERE/../rtl
SOFT=$HERE/../../soft/include
BUILD=$HERE/../build/front

LINT_MODULES="radar_nco radar_dechirp radar_halfband radar_decim4 radar_window radar_seq radar_regs"
MODELS="radar_nco radar_dechirp radar_decim4 radar_window radar_seq radar_regs"

srcs_for() {
    case "$1" in
        radar_decim4) echo "$RTL/radar_decim4.v $RTL/radar_halfband.v" ;;
        *)            echo "$RTL/$1.v" ;;
    esac
}

if [ ! -x "$VERILATOR" ]; then
    echo "run_front.sh: verilator not found at $VERILATOR" >&2
    exit 2
fi

mkdir -p "$BUILD" || exit 2

echo
echo "================================================================"
echo "  1. LINT   verilator --lint-only -Wall -Wno-DECLFILENAME"
echo "================================================================"
echo "  $("$VERILATOR" --version)"
echo

lint_fail=0
for m in $LINT_MODULES; do
    log=$BUILD/lint_$m.log
    # shellcheck disable=SC2046
    if "$VERILATOR" --lint-only -Wall -Wno-DECLFILENAME -I"$RTL" \
                    --top-module "$m" $(srcs_for "$m") > "$log" 2>&1; then
        printf '    [PASS] %-20s clean\n' "$m"
    else
        printf '    [FAIL] %-20s\n' "$m"
        sed 's/^/           /' "$log"
        lint_fail=1
    fi
done

if [ $lint_fail -ne 0 ]; then
    echo
    echo "  LINT FAILED -- not building."
    exit 1
fi

echo
echo "================================================================"
echo "  2. BUILD"
echo "================================================================"

VROOT=$("$VERILATOR" --getenv VERILATOR_ROOT)
if [ -z "$VROOT" ] || [ ! -d "$VROOT/include" ]; then
    echo "run_front.sh: cannot locate VERILATOR_ROOT" >&2
    exit 2
fi

for m in $MODELS; do
    # shellcheck disable=SC2046
    "$VERILATOR" --cc -I"$RTL" --top-module "$m" --prefix "V$m" \
                 --Mdir "$BUILD/obj_$m" $(srcs_for "$m") || {
        echo "run_front.sh: verilating $m failed" >&2; exit 1; }
    make -s -C "$BUILD/obj_$m" -f "V$m.mk" "V${m}__ALL.a" || {
        echo "run_front.sh: compiling model $m failed" >&2; exit 1; }
    printf '    verilated %s\n' "$m"
done

# The Verilator runtime is shared by all six models, so build it once as its
# own archive rather than letting each model's makefile fold a private copy
# into V<name>__ALL.a -- six copies would collide at link time.
make -s -C "$BUILD/obj_radar_nco" -f Vradar_nco.mk libverilated.a || {
    echo "run_front.sh: building the Verilator runtime failed" >&2; exit 1; }
RTOBJS=$BUILD/obj_radar_nco/libverilated.a
printf '    built the Verilator runtime\n'

VFLAGS="-std=c++17 -O2 -I$VROOT/include -I$VROOT/include/vltstd"
TBFLAGS="$VFLAGS -Wall -Wextra -Wno-unused-parameter -I$SOFT"
for m in $MODELS; do
    TBFLAGS="$TBFLAGS -I$BUILD/obj_$m"
done

# shellcheck disable=SC2086
$CXX $TBFLAGS -c "$HERE/tb_front.cpp" -o "$BUILD/tb_front.o" || {
    echo "run_front.sh: compiling tb_front.cpp failed" >&2; exit 1; }
printf '    compiled tb_front.cpp\n'

LIBS=""
for m in $MODELS; do
    LIBS="$LIBS $BUILD/obj_$m/V${m}__ALL.a"
done

# shellcheck disable=SC2086
$CXX -o "$BUILD/tb_front" "$BUILD/tb_front.o" $RTOBJS $LIBS || {
    echo "run_front.sh: link failed" >&2; exit 1; }
printf '    linked %s\n' "$BUILD/tb_front"

echo
echo "================================================================"
echo "  3. SIMULATE"
echo "================================================================"

"$BUILD/tb_front"
rc=$?

echo "================================================================"
if [ $rc -eq 0 ]; then
    echo "  RESULT: PASS   lint clean, every check passed"
else
    echo "  RESULT: FAIL   see the FAIL lines above"
fi
echo "================================================================"
echo

exit $rc
