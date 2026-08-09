#!/bin/sh
cd "$(dirname "$0")"
export THREADS=6
# 1. Does breaking the board buy isolation?  Drive TX2, the worst path.
CUT=66,78 python3 full_board.py cut2 2 > logs/fb_cut2.log 2>&1
# 2. The canceller's two printed couplers.
RES=0.10 MIND=0.035 python3 coupler_dc.py 1.5989 0.9536 7.6683 tap20 > logs/dc_tap20.log 2>&1
RES=0.10 MIND=0.035 python3 coupler_dc.py 1.5385 0.4424 7.7249 inj15 > logs/dc_inj15.log 2>&1
# 3. Back to the axial-ratio sweep that was interrupted.
RES=0.16 MIND=0.055 FFREQ=5.70e9,5.74e9,5.78e9,5.80e9,5.82e9,5.86e9,5.90e9 \
    python3 couple2.py f16a > logs/f16a.log 2>&1
RES=0.16 MIND=0.055 FFREQ=5.70e9,5.74e9,5.78e9,5.80e9,5.82e9,5.86e9,5.90e9 \
    DRIVE=1 python3 couple2.py f16b > logs/f16b.log 2>&1
