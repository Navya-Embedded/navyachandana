#include "mem/cache/replacement_policies/adaptive_lrufifo_rp.hh"
#include <cassert>
#include <limits>
#include "base/logging.hh"
#include "sim/cur_tick.hh"
namespace gem5 { namespace replacement_policy {
AdaptiveLRUFIFO::AdaptiveLRUFIFO(const AdaptiveLRUFIFORPParams &p)
    : Base(p), activePolicy(USE_LRU), missLRU(0), missFIFO(0), accessCount(0),
      leaderFraction(p.leader_fraction), intervalLen(p.interval_len) {}
std::shared_ptr<ReplacementData> AdaptiveLRUFIFO::instantiateEntry() {
    return std::make_shared<AdaptiveReplData>(); }
void AdaptiveLRUFIFO::touch(const std::shared_ptr<ReplacementData>& rd, const PacketPtr) const {
    std::static_pointer_cast<AdaptiveReplData>(rd)->lruTick = curTick();
    accessCount++; maybeSwitch(); }
void AdaptiveLRUFIFO::reset(const std::shared_ptr<ReplacementData>& rd, const PacketPtr) const {
    auto data = std::static_pointer_cast<AdaptiveReplData>(rd);
    data->lruTick = data->fifoTick = curTick();
    Policy role = policyForSet(data->setIndex);
    if (role == USE_LRU) missLRU++;
    else if (role == USE_FIFO) missFIFO++;
    accessCount++; maybeSwitch(); }
void AdaptiveLRUFIFO::setEntrySet(const std::shared_ptr<ReplacementData>& rd, uint32_t s) const {
    std::static_pointer_cast<AdaptiveReplData>(rd)->setIndex = s; }
ReplaceableEntry* AdaptiveLRUFIFO::getVictim(const ReplacementCandidates& candidates) const {
    assert(!candidates.empty());
    auto fd = std::static_pointer_cast<AdaptiveReplData>(candidates[0]->replacementData);
    Policy pol = policyForSet(fd->setIndex);
    ReplaceableEntry* victim = nullptr;
    uint64_t oldest = std::numeric_limits<uint64_t>::max();
    for (auto* e : candidates) {
        auto d = std::static_pointer_cast<AdaptiveReplData>(e->replacementData);
        uint64_t score = (pol == USE_LRU) ? d->lruTick : d->fifoTick;
        if (score < oldest) { oldest = score; victim = e; }
    }
    panic_if(!victim, "AdaptiveLRUFIFO: no victim"); return victim; }
void AdaptiveLRUFIFO::maybeSwitch() const {
    if (accessCount % intervalLen != 0) return;
    activePolicy = (missLRU <= missFIFO) ? USE_LRU : USE_FIFO;
    missLRU = missFIFO = 0; }
AdaptiveLRUFIFO::Policy AdaptiveLRUFIFO::policyForSet(uint32_t s) const {
    if (leaderFraction == 0 || s % leaderFraction != 0) return activePolicy;
    return ((s / leaderFraction) % 2 == 0) ? USE_LRU : USE_FIFO; }
} }
