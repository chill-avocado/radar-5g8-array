#!/bin/sh
cd "$(dirname "$0")"
export THREADS=6
for g in 6 9 12 16 20 25; do
  GNDM=$g python3 full_element.py g$g > logs/gs_$g.log 2>&1
done
