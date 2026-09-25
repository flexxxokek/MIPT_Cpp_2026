#pragma once
// ARC (Adaptive Replacement Cache), Megiddo & Modha, 2003.
//
// Maintains four lists for a cache of capacity c:
//   T1 - resident entries seen exactly once recently ("recency").
//   T2 - resident entries seen more than once recently ("frequency").
//   B1 - ghost list of keys recently evicted from T1 (keys only).
//   B2 - ghost list of keys recently evicted from T2 (keys only).
// |T1|+|T2| <= c, and the adaptation parameter p in [0, c] controls the
// target size of T1, shifting towards recency or frequency depending on
// which ghost list is being hit.
//
// `get` performs a read-only lookup (a hit moves the entry within T1/T2 but
// never touches the ghost lists, since a genuine miss on get() carries no
// value to insert). `put` implements the full ARC algorithm and should be
// called after a real miss to insert the freshly-fetched value, as well as
// for ordinary inserts/updates.

#include <algorithm>
#include <cstddef>
#include <list>
#include <unordered_map>

namespace cache {

template <typename Key, typename Value>
class ARCCache {
public:
    explicit ARCCache(std::size_t capacity) : c_(capacity) {}

    bool get(const Key& key, Value& out) {
        if (auto it = t1_index_.find(key); it != t1_index_.end()) {
            out = values_.at(key);
            t2_.splice(t2_.begin(), t1_, it->second);
            t1_index_.erase(it);
            t2_index_[key] = t2_.begin();
            return true;
        }
        if (auto it = t2_index_.find(key); it != t2_index_.end()) {
            out = values_.at(key);
            t2_.splice(t2_.begin(), t2_, it->second);
            t2_index_[key] = t2_.begin();
            return true;
        }
        return false;
    }

    void put(const Key& key, const Value& value) {
        if (c_ == 0) {
            return;
        }

        // Case I: already resident.
        if (auto it = t1_index_.find(key); it != t1_index_.end()) {
            values_[key] = value;
            t2_.splice(t2_.begin(), t1_, it->second);
            t1_index_.erase(it);
            t2_index_[key] = t2_.begin();
            return;
        }
        if (auto it = t2_index_.find(key); it != t2_index_.end()) {
            values_[key] = value;
            t2_.splice(t2_.begin(), t2_, it->second);
            t2_index_[key] = t2_.begin();
            return;
        }

        // Case II: ghost hit in B1 -> grow target size of T1.
        if (auto it = b1_index_.find(key); it != b1_index_.end()) {
            const std::size_t b1n = b1_.size();
            const std::size_t b2n = b2_.size();
            const std::size_t delta = std::max<std::size_t>(1, b1n ? b2n / b1n : b2n);
            p_ = std::min(c_, p_ + delta);
            replace(/*in_b2=*/false);
            b1_.erase(it->second);
            b1_index_.erase(it);
            promoteToT2(key, value);
            return;
        }

        // Case III: ghost hit in B2 -> grow target size of T2.
        if (auto it = b2_index_.find(key); it != b2_index_.end()) {
            const std::size_t b1n = b1_.size();
            const std::size_t b2n = b2_.size();
            const std::size_t delta = std::max<std::size_t>(1, b2n ? b1n / b2n : b1n);
            p_ = (delta > p_) ? 0 : p_ - delta;
            replace(/*in_b2=*/true);
            b2_.erase(it->second);
            b2_index_.erase(it);
            promoteToT2(key, value);
            return;
        }

        // Case IV: total miss.
        const std::size_t t1n = t1_.size();
        const std::size_t t2n = t2_.size();
        const std::size_t b1n = b1_.size();
        const std::size_t b2n = b2_.size();

        if (t1n + b1n == c_) {
            if (t1n < c_) {
                const Key victim = b1_.back();
                b1_.pop_back();
                b1_index_.erase(victim);
                replace(/*in_b2=*/false);
            } else {
                const Key victim = t1_.back();
                t1_.pop_back();
                t1_index_.erase(victim);
                values_.erase(victim);
            }
        } else if (t1n + b1n < c_ && (t1n + t2n + b1n + b2n) >= c_) {
            if (t1n + t2n + b1n + b2n >= 2 * c_) {
                const Key victim = b2_.back();
                b2_.pop_back();
                b2_index_.erase(victim);
            }
            replace(/*in_b2=*/false);
        }

        values_[key] = value;
        t1_.push_front(key);
        t1_index_[key] = t1_.begin();
    }

    bool contains(const Key& key) const {
        return t1_index_.contains(key) || t2_index_.contains(key);
    }
    std::size_t size() const { return t1_.size() + t2_.size(); }
    std::size_t capacity() const { return c_; }

private:
    using List = std::list<Key>;
    using ListIt = typename List::iterator;

    void promoteToT2(const Key& key, const Value& value) {
        values_[key] = value;
        t2_.push_front(key);
        t2_index_[key] = t2_.begin();
    }

    // REPLACE(x, p) from the ARC paper: evicts one entry from T1 or T2 into
    // the corresponding ghost list. `in_b2` indicates whether the key that
    // triggered this replacement was found in B2 (tie-break rule).
    void replace(bool in_b2) {
        const std::size_t t1n = t1_.size();
        if (t1n >= 1 && (t1n > p_ || (in_b2 && t1n == p_))) {
            const Key victim = t1_.back();
            t1_.pop_back();
            t1_index_.erase(victim);
            values_.erase(victim);
            b1_.push_front(victim);
            b1_index_[victim] = b1_.begin();
        } else if (!t2_.empty()) {
            const Key victim = t2_.back();
            t2_.pop_back();
            t2_index_.erase(victim);
            values_.erase(victim);
            b2_.push_front(victim);
            b2_index_[victim] = b2_.begin();
        }
    }

    std::size_t c_;
    std::size_t p_ = 0;  // Target size of T1, adapted over time.

    List t1_, t2_;
    std::unordered_map<Key, ListIt> t1_index_, t2_index_;

    List b1_, b2_;
    std::unordered_map<Key, ListIt> b1_index_, b2_index_;

    std::unordered_map<Key, Value> values_;
};

}  // namespace cache
