# Adaptive LRU/FIFO Cache Replacement — gem5 Implementation
## Texas A&M University

### Files
- gem5_files/adaptive_lrufifo_rp.hh   — C++ header (copy to src/mem/cache/replacement_policies/)
- gem5_files/adaptive_lrufifo_rp.cc   — C++ implementation
- gem5_files/AdaptiveLRUFIFORP.py     — Python SimObject
- configs/adaptive_test.py            — gem5 config script
- benchmarks/cache_bench.c            — Benchmark binary (compile with gcc -O2 -static)
- scripts/run_all.sh                  — Run all 16 policy×workload combinations
- scripts/parse_results.py            — Parse gem5 stats.txt and print hit-rate table

### Quick start
```bash
# 1. Copy gem5 files
cp gem5_files/adaptive_lrufifo_rp.* gem5/src/mem/cache/replacement_policies/
cp gem5_files/AdaptiveLRUFIFORP.py  gem5/src/mem/cache/replacement_policies/

# 2. Add to SConscript (two lines)
echo "SimObject('AdaptiveLRUFIFORP.py', sim_objects=['AdaptiveLRUFIFORP'])" >> gem5/src/mem/cache/replacement_policies/SConscript
echo "Source('adaptive_lrufifo_rp.cc')" >> gem5/src/mem/cache/replacement_policies/SConscript

# 3. Build
cd gem5 && scons build/X86/gem5.opt -j$(nproc)

# 4. Compile benchmark
gcc -O2 -static -o benchmarks/cache_bench benchmarks/cache_bench.c

# 5. Run all experiments
./scripts/run_all.sh

# 6. Parse results
python3 scripts/parse_results.py
```
