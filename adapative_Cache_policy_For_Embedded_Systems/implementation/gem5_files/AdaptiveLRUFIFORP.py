from m5.params import *
from .ReplacementPolicies import BaseReplacementPolicy
class AdaptiveLRUFIFORP(BaseReplacementPolicy):
    type = 'AdaptiveLRUFIFORP'
    cxx_class = 'gem5::replacement_policy::AdaptiveLRUFIFO'
    cxx_header = 'mem/cache/replacement_policies/adaptive_lrufifo_rp.hh'
    leader_fraction = Param.UInt32(20, "1 in N sets are LRU/FIFO leader sets")
    interval_len    = Param.UInt64(500000, "Accesses between policy comparison intervals")
