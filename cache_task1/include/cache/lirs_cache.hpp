#pragma once
// LIRS (Low Inter-reference Recency Set), Jiang & Zhang, 2002.
//
// Every referenced block is classified as LIR (Low Inter-reference Recency,
// i.e. "hot") or HIR ("cold"). Only a small, bounded number of HIR blocks
// are kept resident at once; the rest of the cache is LIR-resident. This
// gives LIRS strong scan-resistance compared to plain LRU/LFU.
//
// Two structures are maintained:
//   S - a stack recording recency history for LIR blocks, resident HIR
//       blocks, AND non-resident HIR blocks (ghosts). Its bottom entry is
//       always an LIR block (an invariant restored by "stack pruning").
//   Q - a FIFO queue of *resident* HIR blocks only; the front of Q is the
//       eviction candidate.
//
// A HIR block that is re-referenced while its entry is still present in S
// is "proven hot" and is promoted to LIR; this bumps the LIR block that was
// sitting at the bottom of S down to HIR status to keep the LIR population
// bounded.

#include <cstddef>
#include <list>
#include <unordered_map>

namespace cache {

template <typename Key, typename Value>
class LIRSCache {
public:
    // hir_ratio is the target fraction of capacity reserved for resident HIR
    // blocks (the paper suggests ~1%). At least one slot is always reserved
    // for each of LIR/HIR whenever capacity allows it.
    explicit LIRSCache(std::size_t capacity, double hir_ratio = 0.02) : capacity_(capacity) {
        std::size_t hir_cap = static_cast<std::size_t>(capacity * hir_ratio);
        if (capacity >= 2) {
            hir_capacity_ = std::max<std::size_t>(1, hir_cap);
            hir_capacity_ = std::min(hir_capacity_, capacity - 1);
            lir_capacity_ = capacity - hir_capacity_;
        } else {
            // Degenerate tiny cache: everything behaves as a single LIR slot.
            hir_capacity_ = 0;
            lir_capacity_ = capacity;
        }
    }

    bool get(const Key& key, Value& out) {
        auto vit = values_.find(key);
        if (vit == values_.end()) {
            return false;
        }
        out = vit->second;
        touch(key);
        return true;
    }

    void put(const Key& key, const Value& value) {
        if (capacity_ == 0) {
            return;
        }

        if (values_.contains(key)) {
            values_[key] = value;
            touch(key);
            return;
        }

        const bool in_s = s_index_.contains(key);
        if (in_s) {
            // Ghost hit: this block was HIR-non-resident but is being
            // referenced again while its history is still in S -> promote.
            meta_[key] = Meta{Status::kLIR, true};
            values_[key] = value;
            ++lir_count_;
            moveToTopOfS(key);
            if (lir_count_ > lir_capacity_) {
                demoteStackBottom();
            }
            prune();
        } else {
            // Brand new block.
            meta_[key] = Meta{Status::kHIR, true};
            values_[key] = value;
            q_.push_back(key);
            q_index_[key] = std::prev(q_.end());
            moveToTopOfS(key);
        }
        enforceCapacity();
    }

    bool contains(const Key& key) const { return values_.contains(key); }
    std::size_t size() const { return values_.size(); }
    std::size_t capacity() const { return capacity_; }

private:
    enum class Status { kLIR, kHIR };
    struct Meta {
        Status status;
        bool resident;
    };
    using KeyList = std::list<Key>;
    using KeyIt = typename KeyList::iterator;

    void moveToTopOfS(const Key& key) {
        if (auto it = s_index_.find(key); it != s_index_.end()) {
            s_.erase(it->second);
        }
        s_.push_front(key);
        s_index_[key] = s_.begin();
    }

    // Restores the invariant that the bottom of S is an LIR block by
    // dropping trailing HIR entries (resident or ghost) from the bottom.
    void prune() {
        while (!s_.empty()) {
            const Key bottom = s_.back();
            auto mit = meta_.find(bottom);
            if (mit != meta_.end() && mit->second.status == Status::kLIR) {
                break;
            }
            s_.pop_back();
            s_index_.erase(bottom);
            if (mit != meta_.end() && !mit->second.resident) {
                meta_.erase(mit);  // Fully forget non-resident ghosts once pruned.
            }
        }
    }

    // Demotes the LIR block currently at the bottom of S to HIR, making it
    // resident-HIR (it stays in the cache, just loses "hot" status).
    void demoteStackBottom() {
        if (s_.empty()) {
            return;
        }
        const Key victim = s_.back();
        auto mit = meta_.find(victim);
        if (mit != meta_.end() && mit->second.status == Status::kLIR) {
            mit->second.status = Status::kHIR;
            --lir_count_;
            q_.push_back(victim);
            q_index_[victim] = std::prev(q_.end());
        }
    }

    // Existing resident block referenced again.
    void touch(const Key& key) {
        auto mit = meta_.find(key);
        if (mit->second.status == Status::kLIR) {
            const bool was_bottom = !s_.empty() && s_.back() == key;
            moveToTopOfS(key);
            if (was_bottom) {
                prune();
            }
            return;
        }

        // Resident HIR.
        if (s_index_.contains(key)) {
            // Promote to LIR.
            if (auto qit = q_index_.find(key); qit != q_index_.end()) {
                q_.erase(qit->second);
                q_index_.erase(qit);
            }
            mit->second.status = Status::kLIR;
            ++lir_count_;
            moveToTopOfS(key);
            if (lir_count_ > lir_capacity_) {
                demoteStackBottom();
            }
            prune();
        } else {
            // Stays HIR: refresh its position in Q and (re-)enter S.
            if (auto qit = q_index_.find(key); qit != q_index_.end()) {
                q_.erase(qit->second);
            }
            q_.push_back(key);
            q_index_[key] = std::prev(q_.end());
            moveToTopOfS(key);
        }
    }

    void enforceCapacity() {
        while (values_.size() > capacity_) {
            evictFromQ();
        }
    }

    void evictFromQ() {
        if (q_.empty()) {
            // No resident HIR left to evict from; fall back to the LIR
            // stack bottom (can only happen for degenerate tiny caches).
            if (!s_.empty()) {
                const Key victim = s_.back();
                s_.pop_back();
                s_index_.erase(victim);
                values_.erase(victim);
                meta_.erase(victim);
                if (lir_count_ > 0) {
                    --lir_count_;
                }
            }
            return;
        }
        const Key victim = q_.front();
        q_.pop_front();
        q_index_.erase(victim);
        values_.erase(victim);

        auto mit = meta_.find(victim);
        if (mit != meta_.end()) {
            if (s_index_.contains(victim)) {
                mit->second.resident = false;  // Kept as a ghost in S.
            } else {
                meta_.erase(mit);  // Fully forgotten.
            }
        }
    }

    std::size_t capacity_;
    std::size_t lir_capacity_;
    std::size_t hir_capacity_;
    std::size_t lir_count_ = 0;

    KeyList s_;
    std::unordered_map<Key, KeyIt> s_index_;

    KeyList q_;
    std::unordered_map<Key, KeyIt> q_index_;

    std::unordered_map<Key, Meta> meta_;
    std::unordered_map<Key, Value> values_;
};

}  // namespace cache
