#ifndef __MEM_CACHE_REPLACEMENT_POLICIES_ADAPTIVE_LRUFIFO_RP_HH__
#define __MEM_CACHE_REPLACEMENT_POLICIES_ADAPTIVE_LRUFIFO_RP_HH__
#include <cstdint>
#include <memory>
#include "mem/cache/replacement_policies/base.hh"
#include "params/AdaptiveLRUFIFORP.hh"
namespace gem5 { namespace replacement_policy {
class AdaptiveLRUFIFO : public Base {
  public:
    struct AdaptiveReplData : ReplacementData {
        uint64_t lruTick, fifoTick;
        uint32_t setIndex;
        AdaptiveReplData() : lruTick(0), fifoTick(0), setIndex(0) {}
    };
    enum Policy : uint8_t { USE_LRU, USE_FIFO };
    mutable Policy   activePolicy;
    mutable uint64_t missLRU, missFIFO, accessCount;
    const uint32_t leaderFraction;
    const uint64_t intervalLen;
    explicit AdaptiveLRUFIFO(const AdaptiveLRUFIFORPParams &p);
    void touch(const std::shared_ptr<ReplacementData>& rd, const PacketPtr pkt) const override;
    void reset(const std::shared_ptr<ReplacementData>& rd, const PacketPtr pkt) const override;
    ReplaceableEntry* getVictim(const ReplacementCandidates& candidates) const override;
    std::shared_ptr<ReplacementData> instantiateEntry() override;
    void setEntrySet(const std::shared_ptr<ReplacementData>& rd, uint32_t setIdx) const;
  private:
    void maybeSwitch() const;
    Policy policyForSet(uint32_t setIdx) const;
};
} }
#endif
