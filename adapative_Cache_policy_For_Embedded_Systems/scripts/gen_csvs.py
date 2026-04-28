import json, csv, os

DATA = json.load(open("/home/claude/project/sim_results/all_results.json"))
OUT  = "/home/claude/project/sim_results"

# 1. Full results CSV
with open(f"{OUT}/full_results.csv", "w", newline="") as f:
    w = csv.DictWriter(f, fieldnames=DATA[0].keys())
    w.writeheader()
    w.writerows(DATA)
print(f"full_results.csv: {len(DATA)} rows")

# 2. Summary table: 256KB 16-way main configs
main = [r for r in DATA if r["cache_kb"]==256 and r["assoc"]==16
        and r["policy"] in ("lru","fifo","random","adaptive_lf20_iv500k")]
with open(f"{OUT}/summary_256kb_16way.csv", "w", newline="") as f:
    w = csv.DictWriter(f, fieldnames=main[0].keys())
    w.writeheader()
    w.writerows(main)
print(f"summary_256kb_16way.csv: {len(main)} rows")

# 3. Adaptive parameter sweep
adp = [r for r in DATA if r["policy_base"]=="adaptive"
       and r["cache_kb"]==256 and r["assoc"]==16]
with open(f"{OUT}/adaptive_param_sweep.csv", "w", newline="") as f:
    w = csv.DictWriter(f, fieldnames=adp[0].keys())
    w.writeheader()
    w.writerows(adp)
print(f"adaptive_param_sweep.csv: {len(adp)} rows")

# 4. Speedup vs LRU
rows = []
for cache_kb in [128,256,512]:
    for assoc in [8,16,32]:
        for wl in ["loop","stream","mixed","stride"]:
            lru = next((r for r in DATA if r["policy"]=="lru"
                        and r["workload"]==wl and r["cache_kb"]==cache_kb
                        and r["assoc"]==assoc), None)
            if not lru: continue
            for pol in ["fifo","random","adaptive_lf20_iv500k"]:
                row = next((r for r in DATA if r["policy"]==pol
                            and r["workload"]==wl and r["cache_kb"]==cache_kb
                            and r["assoc"]==assoc), None)
                if not row: continue
                base_c = lru["sim_cycles"]
                rows.append({
                    "policy": pol, "workload": wl,
                    "cache_kb": cache_kb, "assoc": assoc,
                    "lru_hit_rate": lru["hit_rate_pct"],
                    "pol_hit_rate": row["hit_rate_pct"],
                    "hr_delta_pct": round(row["hit_rate_pct"]-lru["hit_rate_pct"],2),
                    "lru_cycles_M": round(lru["sim_cycles"]/1e6,2),
                    "pol_cycles_M": round(row["sim_cycles"]/1e6,2),
                    "cycle_reduction_pct": round((base_c-row["sim_cycles"])/base_c*100,2),
                    "lru_mpki": lru["mpki"],
                    "pol_mpki": row["mpki"],
                    "mpki_reduction": round(lru["mpki"]-row["mpki"],2),
                })

with open(f"{OUT}/speedup_vs_lru.csv", "w", newline="") as f:
    w = csv.DictWriter(f, fieldnames=rows[0].keys())
    w.writeheader()
    w.writerows(rows)
print(f"speedup_vs_lru.csv: {len(rows)} rows")
print("Done.")
