import argparse
import m5
from m5.objects import *
from m5.util import addToPath
addToPath('../')
parser = argparse.ArgumentParser()
parser.add_argument('--cmd',    required=True)
parser.add_argument('--opts',   default='')
parser.add_argument('--policy', default='lru',
                    choices=['lru','fifo','random','adaptive'])
parser.add_argument('--l2size',  default='256kB')
parser.add_argument('--l2assoc', default=16, type=int)
parser.add_argument('--leader_fraction', default=20, type=int)
parser.add_argument('--interval_len',    default=500000, type=int)
args = parser.parse_args()
def make_policy(name):
    if name == 'lru':    return LRURP()
    if name == 'fifo':   return FIFORP()
    if name == 'random': return RandomRP()
    return AdaptiveLRUFIFORP(leader_fraction=args.leader_fraction,
                              interval_len=args.interval_len)
system = System()
system.clk_domain = SrcClockDomain(clock='1GHz', voltage_domain=VoltageDomain())
system.mem_mode   = 'timing'
system.mem_ranges = [AddrRange('512MB')]
system.cpu = TimingSimpleCPU()
system.cpu.icache = Cache(size='32kB',assoc=8,tag_latency=2,data_latency=2,
                          response_latency=2,mshrs=4,tgts_per_mshr=20,
                          replacement_policy=LRURP())
system.cpu.dcache = Cache(size='32kB',assoc=8,tag_latency=2,data_latency=2,
                          response_latency=2,mshrs=4,tgts_per_mshr=20,
                          replacement_policy=LRURP())
system.l2cache = Cache(size=args.l2size,assoc=args.l2assoc,
                       tag_latency=20,data_latency=20,response_latency=20,
                       mshrs=20,tgts_per_mshr=12,clusivity='mostly_incl',
                       replacement_policy=make_policy(args.policy))
system.l2bus  = L2XBar()
system.membus = SystemXBar()
system.cpu.icache.mem_side  = system.l2bus.cpu_side_ports
system.cpu.dcache.mem_side  = system.l2bus.cpu_side_ports
system.l2bus.mem_side_ports = system.l2cache.cpu_side
system.l2cache.mem_side     = system.membus.cpu_side_ports
system.cpu.icache_port = system.cpu.icache.cpu_side
system.cpu.dcache_port = system.cpu.dcache.cpu_side
system.cpu.createInterruptController()
system.cpu.interrupts[0].pio = system.membus.mem_side_ports
system.cpu.interrupts[0].int_requestor = system.membus.cpu_side_ports
system.cpu.interrupts[0].int_responder = system.membus.mem_side_ports
system.system_port = system.membus.cpu_side_ports
system.mem_ctrl = MemCtrl()
system.mem_ctrl.dram = DDR3_1600_8x8()
system.mem_ctrl.dram.range = system.mem_ranges[0]
system.mem_ctrl.port = system.membus.mem_side_ports
process = Process()
process.cmd = [args.cmd] + (args.opts.split() if args.opts else [])
system.cpu.workload = process
system.cpu.createThreads()
root = Root(full_system=False, system=system)
m5.instantiate()
m5.simulate()
