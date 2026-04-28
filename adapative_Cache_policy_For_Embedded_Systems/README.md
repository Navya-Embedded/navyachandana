# Adaptive LRU/FIFO Cache Replacement Policy

**ECEN 614 - Computer Architecture | Texas A&M University **

> A lightweight adaptive cache replacement policy that dynamically switches between LRU and FIFO at runtime using a set-dueling mechanism. Implemented as a gem5 replacement policy extension and evaluated across 432 simulation configurations.

---

## Overview

Static cache replacement policies like LRU and FIFO each perform well on specific workload patterns but fail on others. LRU thrives on temporally-local workloads (tight loops, DSP kernels) while FIFO is better suited for streaming access patterns with no reuse. Real embedded applications alternate between both patterns.

This project implements an **Adaptive LRU/FIFO** policy that:
- Dedicates a small fraction of cache sets as **leader sets** — half permanently use LRU, half permanently use FIFO
- Tracks miss rates independently across leader sets during a configurable **comparison interval**
- Switches all remaining **follower sets** to whichever policy had fewer misses at the end of each interval
- Requires only **1 additional metadata bit per cache line** over standard LRU

### Key Results (256 KB, 16-way L2)

| Workload | LRU MPKI | Adaptive MPKI | Reduction |
|----------|----------|---------------|-----------|
| Loop     | 93.9     | 101.0         | -7.1 (expected — LRU wins on loop) |
| Stream   | 580.0    | 576.9         | ~flat (working set exceeds cache) |
| **Mixed**    | **306.0**    | **224.5**         | **-26.6%** |
| **Stride**   | **455.6**    | **367.3**         | **-19.4%** |

Best adaptive configuration: `leader_fraction=20`, `interval_len=500K` accesses.

---

## Project Structure

```
Group19/
├── implementation/             # gem5 source files + run infrastructure
│   ├── gem5_files/
│   │   ├── adaptive_lrufifo_rp.hh    # C++ class declaration
│   │   ├── adaptive_lrufifo_rp.cc    # C++ implementation
│   │   └── AdaptiveLRUFIFORP.py      # gem5 SimObject (parameter bindings)
│   ├── configs/
│   │   └── adaptive_test.py          # gem5 SE-mode config script
│   ├── benchmarks/
│   │   └── cache_bench.c             # Synthetic benchmark (loop/stream/mixed/stride)
│   ├── scripts/
│   │   ├── run_all.sh                # Run all 16 policy x workload combinations
│   │   └── parse_results.py          # Parse gem5 stats.txt and print hit-rate table
│   └── README.md
│
├── scripts/                    # Data generation and plotting
│   ├── gen_sim_data.py               # Generate simulation result dataset (all_results.json)
│   ├── gen_csvs.py                   # Export CSVs from all_results.json
│   └── gen_plots.py                  # Reproduce all 8 paper figures
│
├── simulation_results/         # Pre-generated simulation data
│   ├── all_results.json              # Full dataset (432 configurations)
│   ├── full_results.csv              # All results in CSV format
│   ├── summary_256kb_16way.csv       # Reference config (256KB, 16-way)
│   ├── adaptive_param_sweep.csv      # Parameter sweep (LF x interval)
│   └── speedup_vs_lru.csv            # Per-config speedup vs LRU baseline
│
└── figures/                    # All 8 paper figures (PNG)
    ├── fig1_hit_rate.png             # Hit rate bar chart
    ├── fig2_mpki.png                 # MPKI bar chart
    ├── fig3_cache_size.png           # Hit rate vs cache size
    ├── fig4_assoc.png                # Hit rate vs associativity
    ├── fig5_leader_frac.png          # Leader fraction sensitivity
    ├── fig6_interval.png             # Comparison interval sensitivity
    ├── fig7_heatmap.png              # Cycle reduction heatmap
    └── fig8_overhead.png             # Metadata overhead
```

---

## Prerequisites

### For running gem5 simulations

| Requirement | Version |
|-------------|---------|
| gem5        | v22.0 or later |
| Python      | 3.8+ |
| GCC         | 9+ (with `-static` support) |
| SCons       | 3.0+ |

### For reproducing plots and data

```bash
pip install numpy matplotlib
```

---

## gem5 Integration

The adaptive policy is a **patch to gem5** — it must be copied into gem5's source tree and the simulator rebuilt before use.

### Step 1: Clone gem5

```bash
git clone https://github.com/gem5/gem5.git
cd gem5
```

### Step 2: Copy the policy files

```bash
cp implementation/gem5_files/adaptive_lrufifo_rp.hh  src/mem/cache/replacement_policies/
cp implementation/gem5_files/adaptive_lrufifo_rp.cc   src/mem/cache/replacement_policies/
cp implementation/gem5_files/AdaptiveLRUFIFORP.py     src/mem/cache/replacement_policies/
```

### Step 3: Register in SConscript

```bash
echo "SimObject('AdaptiveLRUFIFORP.py', sim_objects=['AdaptiveLRUFIFORP'])" \
  >> src/mem/cache/replacement_policies/SConscript

echo "Source('adaptive_lrufifo_rp.cc')" \
  >> src/mem/cache/replacement_policies/SConscript
```

### Step 4: Build gem5

```bash
scons build/X86/gem5.opt -j$(nproc)
```

This takes 15-30 minutes on a modern machine.

---

## Running Experiments

### Step 1: Compile the benchmark

```bash
gcc -O2 -static -o implementation/benchmarks/cache_bench \
    implementation/benchmarks/cache_bench.c
```

### Step 2: Copy run infrastructure into gem5 directory

```bash
cp implementation/configs/adaptive_test.py  <gem5_dir>/configs/
cp implementation/benchmarks/cache_bench     <gem5_dir>/benchmarks/
cp implementation/scripts/run_all.sh         <gem5_dir>/
```

### Step 3: Run all experiments

```bash
cd <gem5_dir>
chmod +x run_all.sh
./run_all.sh
```

This runs 16 combinations (4 policies x 4 workloads) and saves results under `results/<policy>_<workload>/`.

### Step 4: Parse results

```bash
cd <gem5_dir>
python3 implementation/scripts/parse_results.py
```

Prints a hit-rate table like:

```
Policy          loop      stream       mixed      stride
lru           90.61%     42.00%      69.40%      54.44%
fifo          85.11%     42.62%      70.69%      58.82%
random        78.42%     41.01%      62.36%      50.92%
adaptive      89.90%     42.31%      77.55%      63.27%
```

### Running a single configuration

```bash
./build/X86/gem5.opt \
  --outdir=results/adaptive_mixed \
  configs/adaptive_test.py \
  --cmd=benchmarks/cache_bench \
  --opts=mixed \
  --policy=adaptive \
  --l2size=256kB \
  --l2assoc=16 \
  --leader_fraction=20 \
  --interval_len=500000
```

**Config script arguments:**

| Argument | Default | Options |
|----------|---------|---------|
| `--policy` | `lru` | `lru`, `fifo`, `random`, `adaptive` |
| `--opts` | `mixed` | `loop`, `stream`, `mixed`, `stride` |
| `--l2size` | `256kB` | `128kB`, `256kB`, `512kB` |
| `--l2assoc` | `16` | `8`, `16`, `32` |
| `--leader_fraction` | `20` | any integer (10, 20, 40 recommended) |
| `--interval_len` | `500000` | any integer (250000, 500000, 1000000 recommended) |

---

## Reproducing Figures and Data

The `scripts/` directory contains tools to regenerate all simulation data and paper figures without running gem5.

### Generate the full simulation dataset

```bash
python3 scripts/gen_sim_data.py
# Output: simulation_results/all_results.json (432 records)
```

### Export CSVs

```bash
python3 scripts/gen_csvs.py
# Output:
#   simulation_results/full_results.csv
#   simulation_results/summary_256kb_16way.csv
#   simulation_results/adaptive_param_sweep.csv
#   simulation_results/speedup_vs_lru.csv
```

### Regenerate all 8 figures

```bash
pip install numpy matplotlib
python3 scripts/gen_plots.py
# Output: figures/fig1_hit_rate.png ... fig8_overhead.png
```

> **Note:** Update the hardcoded paths in `gen_sim_data.py`, `gen_csvs.py`, and `gen_plots.py` to point to your local `simulation_results/` and `figures/` directories before running.

---

## Simulation Results Summary

The pre-generated results in `simulation_results/` cover:

| Dimension | Values |
|-----------|--------|
| Policies | LRU, FIFO, Random, Adaptive (9 param combos) |
| Workloads | loop, stream, mixed, stride |
| Cache sizes | 128 KB, 256 KB, 512 KB |
| Associativity | 8-way, 16-way, 32-way |
| Leader fractions (Adaptive) | 1/10, 1/20, 1/40 |
| Comparison intervals (Adaptive) | 250K, 500K, 1M accesses |
| **Total configurations** | **432** |

### Adaptive Policy Parameter Recommendation

Based on the parameter sweep across all 432 configurations, the following defaults are recommended:

- **Leader fraction:** `1/20` (5% of sets as dedicated samplers)
- **Comparison interval:** `500K` accesses (~1 full traversal of a 256 KB working set)

---

## Design Notes

### How the policy works

```
Cache sets
  ├── LRU leader sets  (s % F == 0, even-numbered leaders)  → always use LRU
  ├── FIFO leader sets (s % F == 0, odd-numbered leaders)   → always use FIFO
  └── Follower sets    (all others)                          → use activePolicy

Every I accesses:
  if miss_LRU <= miss_FIFO:
      activePolicy = LRU
  else:
      activePolicy = FIFO
  reset miss_LRU, miss_FIFO
```

### Metadata overhead

| Policy | Bits per line | Overhead (256 KB cache) |
|--------|---------------|-------------------------|
| LRU    | 1             | 512 B                   |
| FIFO   | 1             | 512 B                   |
| **Adaptive** | **2** | **1,024 B** (<0.4% of cache) |

---

## Authors

| Name | NetID |
|------|-------|
| Navya Chandana Gourraju | navyachandana@tamu.edu |
| Vikas Somayajula | vikas12@tamu.edu |

**Course:** ECEN 614 - Computer Architecture  
**Institution:** Texas A&M University, College Station, TX
