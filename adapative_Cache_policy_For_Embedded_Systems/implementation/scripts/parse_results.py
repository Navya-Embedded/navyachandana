import re, os
RESULTS="./results"
def read(p,k,d=0):
    try:
        for l in open(p):
            if l.startswith(k): return float(l.split()[1])
    except: pass
    return d
workloads=["loop","stream","mixed","stride"]
print(f"{'Policy':<12}",end="")
for w in workloads: print(f"  {w:>10}",end="")
print()
for pol in ["lru","fifo","random","adaptive"]:
    print(f"{pol:<12}",end="")
    for wl in workloads:
        p=f"{RESULTS}/{pol}_{wl}/stats.txt"
        h=read(p,"system.l2cache.demand_hits::total")
        m=read(p,"system.l2cache.demand_misses::total",1)
        print(f"  {h/(h+m)*100:>9.2f}%",end="")
    print()
