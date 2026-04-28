#!/bin/bash
GEM5=./build/X86/gem5.opt
CFG=configs/adaptive_test.py
BIN=./benchmarks/cache_bench
RESULTS=./results
mkdir -p $RESULTS
for pol in lru fifo random adaptive; do
  for wl in loop stream mixed stride; do
    OUTDIR="$RESULTS/${pol}_${wl}"
    mkdir -p "$OUTDIR"
    echo ">>> $pol / $wl"
    $GEM5 --outdir="$OUTDIR" $CFG --cmd="$BIN" --opts="$wl" --policy="$pol" > "$OUTDIR/stdout.log" 2>&1
  done
done
echo "All runs complete."
