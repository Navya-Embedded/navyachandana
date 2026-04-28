"""Generate all plots for the IEEE paper"""
import json, os
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
from matplotlib.gridspec import GridSpec

DATA = json.load(open("/home/claude/project/sim_results/all_results.json"))
OUT  = "/home/claude/project/plots"
os.makedirs(OUT, exist_ok=True)

COLORS = {
    "lru":      "#2E5FA3",
    "fifo":     "#2E9E6B",
    "random":   "#C85250",
    "adaptive": "#D4820A",
}
HATCHES = {"lru": "", "fifo": "//", "random": "xx", "adaptive": ".."}
WLS   = ["loop", "stream", "mixed", "stride"]
WL_LABELS = {"loop":"Loop-\nintensive","stream":"Streaming","mixed":"Mixed","stride":"Stride"}

def get(policy, workload, cache_kb=256, assoc=16):
    rows = [r for r in DATA
            if r["policy"]==policy and r["workload"]==workload
            and r["cache_kb"]==cache_kb and r["assoc"]==assoc]
    return rows[0] if rows else None

MAIN_POLS = ["lru","fifo","random","adaptive_lf20_iv500k"]
POL_LABELS = {"lru":"LRU","fifo":"FIFO","random":"Random",
              "adaptive_lf20_iv500k":"Adaptive\n(LF=20, IV=500K)"}

# ── Fig 1: Hit rate bar chart (256KB, 16-way) ─────────────────────────────────
fig, ax = plt.subplots(figsize=(8, 4))
x = np.arange(len(WLS))
w = 0.18
for i, pol in enumerate(MAIN_POLS):
    vals = [get(pol,wl)["hit_rate_pct"] for wl in WLS]
    color = COLORS[pol.split("_")[0]]
    bars = ax.bar(x + (i-1.5)*w, vals, w, label=POL_LABELS[pol],
                  color=color, edgecolor="white", linewidth=0.5,
                  hatch=HATCHES[pol.split("_")[0]])
    for bar, v in zip(bars, vals):
        ax.text(bar.get_x()+bar.get_width()/2, bar.get_height()+0.3,
                f"{v:.1f}", ha="center", va="bottom", fontsize=6.5)

ax.set_xticks(x)
ax.set_xticklabels([WL_LABELS[w] for w in WLS], fontsize=10)
ax.set_ylabel("L2 Hit Rate (%)", fontsize=10)
ax.set_title("L2 Cache Hit Rate by Policy and Workload\n(256 KB, 16-way, default parameters)", fontsize=10)
ax.legend(fontsize=8, ncol=4, loc="upper center", bbox_to_anchor=(0.5,1.17))
ax.set_ylim(0, 105)
ax.yaxis.grid(True, alpha=0.3, linestyle="--")
ax.set_axisbelow(True)
plt.tight_layout()
plt.savefig(f"{OUT}/fig1_hit_rate.png", dpi=150, bbox_inches="tight")
plt.close()
print("Fig 1 done")

# ── Fig 2: MPKI ───────────────────────────────────────────────────────────────
fig, ax = plt.subplots(figsize=(8, 4))
for i, pol in enumerate(MAIN_POLS):
    vals = [get(pol,wl)["mpki"] for wl in WLS]
    color = COLORS[pol.split("_")[0]]
    ax.bar(x + (i-1.5)*w, vals, w, label=POL_LABELS[pol],
           color=color, edgecolor="white", linewidth=0.5,
           hatch=HATCHES[pol.split("_")[0]])

ax.set_xticks(x)
ax.set_xticklabels([WL_LABELS[w] for w in WLS], fontsize=10)
ax.set_ylabel("Misses Per Kilo-Instruction (MPKI)", fontsize=10)
ax.set_title("L2 MPKI by Policy and Workload\n(256 KB, 16-way)", fontsize=10)
ax.legend(fontsize=8, ncol=4, loc="upper right")
ax.yaxis.grid(True, alpha=0.3, linestyle="--")
ax.set_axisbelow(True)
plt.tight_layout()
plt.savefig(f"{OUT}/fig2_mpki.png", dpi=150, bbox_inches="tight")
plt.close()
print("Fig 2 done")

# ── Fig 3: Cache size sensitivity ─────────────────────────────────────────────
fig, axes = plt.subplots(1, 4, figsize=(12, 3.5), sharey=False)
for ax, wl in zip(axes, WLS):
    for pol, label, color in [("lru","LRU",COLORS["lru"]),
                               ("fifo","FIFO",COLORS["fifo"]),
                               ("adaptive_lf20_iv500k","Adaptive",COLORS["adaptive"])]:
        vals = [get(pol, wl, ck)["hit_rate_pct"] for ck in [128,256,512]]
        ax.plot([128,256,512], vals, marker="o", label=label, color=color,
                linewidth=1.8, markersize=5)
    ax.set_title(WL_LABELS[wl].replace("\n"," "), fontsize=9)
    ax.set_xlabel("Cache Size (KB)", fontsize=8)
    ax.set_xticks([128,256,512])
    ax.yaxis.grid(True, alpha=0.3, linestyle="--")
    ax.set_axisbelow(True)

axes[0].set_ylabel("Hit Rate (%)", fontsize=9)
axes[0].legend(fontsize=7.5)
fig.suptitle("Hit Rate vs. Cache Size (16-way associativity)", fontsize=10, y=1.02)
plt.tight_layout()
plt.savefig(f"{OUT}/fig3_cache_size.png", dpi=150, bbox_inches="tight")
plt.close()
print("Fig 3 done")

# ── Fig 4: Associativity sensitivity ──────────────────────────────────────────
fig, axes = plt.subplots(1, 4, figsize=(12, 3.5))
for ax, wl in zip(axes, WLS):
    for pol, label, color in [("lru","LRU",COLORS["lru"]),
                               ("fifo","FIFO",COLORS["fifo"]),
                               ("adaptive_lf20_iv500k","Adaptive",COLORS["adaptive"])]:
        vals = [get(pol, wl, 256, a)["hit_rate_pct"] for a in [8,16,32]]
        ax.plot([8,16,32], vals, marker="s", label=label, color=color,
                linewidth=1.8, markersize=5)
    ax.set_title(WL_LABELS[wl].replace("\n"," "), fontsize=9)
    ax.set_xlabel("Associativity (ways)", fontsize=8)
    ax.set_xticks([8,16,32])
    ax.yaxis.grid(True, alpha=0.3, linestyle="--")
    ax.set_axisbelow(True)

axes[0].set_ylabel("Hit Rate (%)", fontsize=9)
axes[0].legend(fontsize=7.5)
fig.suptitle("Hit Rate vs. Cache Associativity (256 KB)", fontsize=10, y=1.02)
plt.tight_layout()
plt.savefig(f"{OUT}/fig4_assoc.png", dpi=150, bbox_inches="tight")
plt.close()
print("Fig 4 done")

# ── Fig 5: Leader fraction sweep ──────────────────────────────────────────────
fig, axes = plt.subplots(1, 4, figsize=(12, 3.5))
LF_COLORS = {10:"#8B2FC9", 20:"#D4820A", 40:"#1A8C5E"}
for ax, wl in zip(axes, WLS):
    for lf in [10, 20, 40]:
        pol = f"adaptive_lf{lf}_iv500k"
        vals = [get(pol, wl, ck, 16)["hit_rate_pct"] for ck in [128,256,512]]
        ax.plot([128,256,512], vals, marker="D", label=f"LF={lf}",
                color=LF_COLORS[lf], linewidth=1.8, markersize=5)
    # LRU baseline
    vals_lru = [get("lru", wl, ck, 16)["hit_rate_pct"] for ck in [128,256,512]]
    ax.plot([128,256,512], vals_lru, linestyle="--", color="gray",
            label="LRU (baseline)", linewidth=1.2)
    ax.set_title(WL_LABELS[wl].replace("\n"," "), fontsize=9)
    ax.set_xlabel("Cache Size (KB)", fontsize=8)
    ax.set_xticks([128,256,512])
    ax.yaxis.grid(True, alpha=0.3, linestyle="--")
    ax.set_axisbelow(True)

axes[0].set_ylabel("Hit Rate (%)", fontsize=9)
axes[0].legend(fontsize=7.5)
fig.suptitle("Adaptive Policy: Leader Fraction Sensitivity (interval=500K)", fontsize=10, y=1.02)
plt.tight_layout()
plt.savefig(f"{OUT}/fig5_leader_frac.png", dpi=150, bbox_inches="tight")
plt.close()
print("Fig 5 done")

# ── Fig 6: Interval length sweep ──────────────────────────────────────────────
fig, axes = plt.subplots(1, 4, figsize=(12, 3.5))
IV_COLORS = {250:"#E5532A", 500:"#D4820A", 1000:"#2E5FA3"}
for ax, wl in zip(axes, WLS):
    for iv_k, iv_label in [(250,"250K"),(500,"500K"),(1000,"1M")]:
        pol = f"adaptive_lf20_iv{iv_k}k"
        vals = [get(pol, wl, ck, 16)["hit_rate_pct"] for ck in [128,256,512]]
        ax.plot([128,256,512], vals, marker="^", label=f"IV={iv_label}",
                color=IV_COLORS[iv_k], linewidth=1.8, markersize=5)
    vals_lru = [get("lru", wl, ck, 16)["hit_rate_pct"] for ck in [128,256,512]]
    ax.plot([128,256,512], vals_lru, linestyle="--", color="gray",
            label="LRU", linewidth=1.2)
    ax.set_title(WL_LABELS[wl].replace("\n"," "), fontsize=9)
    ax.set_xlabel("Cache Size (KB)", fontsize=8)
    ax.set_xticks([128,256,512])
    ax.yaxis.grid(True, alpha=0.3, linestyle="--")
    ax.set_axisbelow(True)

axes[0].set_ylabel("Hit Rate (%)", fontsize=9)
axes[0].legend(fontsize=7.5)
fig.suptitle("Adaptive Policy: Interval Length Sensitivity (LF=20)", fontsize=10, y=1.02)
plt.tight_layout()
plt.savefig(f"{OUT}/fig6_interval.png", dpi=150, bbox_inches="tight")
plt.close()
print("Fig 6 done")

# ── Fig 7: Cycle reduction heatmap ────────────────────────────────────────────
fig, axes = plt.subplots(1, 4, figsize=(12, 3.5))
COMPARE_POLS = ["fifo","random","adaptive_lf20_iv500k"]
POL_SHORT    = {"fifo":"FIFO","random":"Random","adaptive_lf20_iv500k":"Adaptive"}
CACHE_SIZES2 = [128, 256, 512]
ASSOCS2      = [8, 16, 32]

for ax, wl in zip(axes, WLS):
    matrix = np.zeros((len(ASSOCS2), len(CACHE_SIZES2)))
    for ci, ck in enumerate(CACHE_SIZES2):
        for ai, a in enumerate(ASSOCS2):
            base = get("lru", wl, ck, a)["sim_cycles"]
            adp  = get("adaptive_lf20_iv500k", wl, ck, a)["sim_cycles"]
            matrix[ai, ci] = (base - adp) / base * 100

    im = ax.imshow(matrix, cmap="RdYlGn", vmin=-5, vmax=12, aspect="auto")
    ax.set_xticks(range(3)); ax.set_xticklabels(["128K","256K","512K"], fontsize=8)
    ax.set_yticks(range(3)); ax.set_yticklabels(["8-way","16-way","32-way"], fontsize=8)
    ax.set_title(WL_LABELS[wl].replace("\n"," "), fontsize=9)
    for ci in range(3):
        for ai in range(3):
            ax.text(ci, ai, f"{matrix[ai,ci]:+.1f}%", ha="center", va="center",
                    fontsize=7.5, color="black")

plt.colorbar(im, ax=axes[-1], label="Cycle reduction vs LRU (%)", shrink=0.8)
fig.suptitle("Adaptive Policy: Cycle Reduction vs. LRU Baseline\n(heatmap: assoc × cache size)", fontsize=10, y=1.03)
plt.tight_layout()
plt.savefig(f"{OUT}/fig7_heatmap.png", dpi=150, bbox_inches="tight")
plt.close()
print("Fig 7 done")

# ── Fig 8: Overhead analysis ──────────────────────────────────────────────────
fig, ax = plt.subplots(figsize=(6, 3.5))
lines   = [128*1024//64, 256*1024//64, 512*1024//64]  # lines per cache
labels  = ["128 KB\n(2048 lines)", "256 KB\n(4096 lines)", "512 KB\n(8192 lines)"]
pol_oh  = {"LRU\n(1-bit)":1, "FIFO\n(1-bit)":1, "Adaptive\n(2-bit)":2}
x = np.arange(len(labels))
w2 = 0.25
for i, (pol_label, bits) in enumerate(pol_oh.items()):
    vals = [l * bits / 8 / 1024 for l in lines]  # KB overhead
    ax.bar(x + (i-1)*w2, vals, w2, label=pol_label,
           color=["#2E5FA3","#2E9E6B","#D4820A"][i], edgecolor="white")

ax.set_xticks(x); ax.set_xticklabels(labels, fontsize=9)
ax.set_ylabel("Metadata Overhead (KB)", fontsize=10)
ax.set_title("Per-Line Metadata Overhead by Cache Size", fontsize=10)
ax.legend(fontsize=9)
ax.yaxis.grid(True, alpha=0.3, linestyle="--")
ax.set_axisbelow(True)
plt.tight_layout()
plt.savefig(f"{OUT}/fig8_overhead.png", dpi=150, bbox_inches="tight")
plt.close()
print("Fig 8 done")

print("\nAll 8 figures saved to", OUT)
