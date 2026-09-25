#pragma once
// LFU (Least Frequently Used) cache.
// Classic O(1) implementation using frequency buckets, each bucket being a
// doubly linked list of keys ordered by recency within that frequency
// (so ties on frequency are broken by LRU order).

#include <cstddef>
#include <list>
#include <unordered_map>

namespace cache {

template <typename Key, typename Value>
class LFUCache {
public:
    explicit LFUCache(std::size_t capacity) : capacity_(capacity) {}

    // Returns true and fills `out` if `key` is present, bumping its
    // frequency. Returns false otherwise.
    bool get(const Key& key, Value& out) {
        auto it = items_.find(key);
        if (it == items_.end()) {
            return false;
        }
        out = it->second.value;
        touch(it);
        return true;
    }

    // Inserts or updates `key`. May evict the least-frequently-used entry
    // (ties broken by least-recently-used) if the cache is full.
    void put(const Key& key, const Value& value) {
        if (capacity_ == 0) {
            return;
        }

        auto it = items_.find(key);
        if (it != items_.end()) {
            it->second.value = value;
            touch(it);
            return;
        }

        if (items_.size() >= capacity_) {
            evict();
        }

        constexpr std::size_t kInitialFreq = 1;
        auto& bucket = freq_lists_[kInitialFreq];
        bucket.push_front(key);
        items_.emplace(key, Node{value, kInitialFreq, bucket.begin()});
        min_freq_ = kInitialFreq;
    }

    bool contains(const Key& key) const { return items_.contains(key); }
    std::size_t size() const { return items_.size(); }
    std::size_t capacity() const { return capacity_; }

private:
    struct Node {
        Value value;
        std::size_t freq;
        typename std::list<Key>::iterator list_it;
    };
    using Map = std::unordered_map<Key, Node>;

    void touch(typename Map::iterator it) {
        const std::size_t old_freq = it->second.freq;
        auto& old_bucket = freq_lists_[old_freq];
        old_bucket.erase(it->second.list_it);
        if (old_bucket.empty()) {
            freq_lists_.erase(old_freq);
            if (min_freq_ == old_freq) {
                ++min_freq_;
            }
        }

        const std::size_t new_freq = old_freq + 1;
        auto& new_bucket = freq_lists_[new_freq];
        new_bucket.push_front(it->first);
        it->second.freq = new_freq;
        it->second.list_it = new_bucket.begin();
    }

    void evict() {
        auto bucket_it = freq_lists_.find(min_freq_);
        auto& bucket = bucket_it->second;
        const Key victim = bucket.back();
        bucket.pop_back();
        if (bucket.empty()) {
            freq_lists_.erase(bucket_it);
        }
        items_.erase(victim);
    }

    std::size_t capacity_;
    std::size_t min_freq_ = 0;
    Map items_;
    std::unordered_map<std::size_t, std::list<Key>> freq_lists_;
};

}  // namespace cache
