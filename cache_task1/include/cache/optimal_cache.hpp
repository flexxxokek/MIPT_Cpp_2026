#pragma once
// Optimal (Belady's MIN) offline caching algorithm.
//
// Given the *entire* future sequence of key accesses up front, this
// evicts, on every miss, whichever resident key will be reused furthest in
// the future (or never again). This is the provably-optimal offline
// replacement policy: no other algorithm can produce more hits for the same
// capacity and access sequence.
//
// Because it needs to see the future, this is not exposed as an online
// get()/put() cache like the others; instead it is run once over a
// complete sequence and reports the resulting number of hits (and, if
// requested, the hit/miss trace).

#include <cstddef>
#include <deque>
#include <limits>
#include <set>
#include <unordered_map>
#include <vector>

namespace cache {

template <typename Key>
class OptimalCache {
public:
    explicit OptimalCache(std::size_t capacity) : capacity_(capacity) {}

    // Simulates the whole `sequence` against a cache of `capacity_` slots
    // using Belady's MIN algorithm. Returns the number of hits.
    std::size_t run(const std::vector<Key>& sequence) {
        std::vector<bool> trace;
        return runWithTrace(sequence, trace);
    }

    // Same as run(), but also fills `trace` with one bool per access:
    // true = hit, false = miss, in the same order as `sequence`.
    std::size_t runWithTrace(const std::vector<Key>& sequence, std::vector<bool>& trace) {
        trace.assign(sequence.size(), false);
        if (capacity_ == 0) {
            return 0;
        }

        std::unordered_map<Key, std::deque<std::size_t>> future_positions;
        for (std::size_t i = 0; i < sequence.size(); ++i) {
            future_positions[sequence[i]].push_back(i);
        }

        std::set<Key> resident;
        std::size_t hits = 0;

        auto next_use_after = [&](const Key& k) -> std::size_t {
            auto& dq = future_positions[k];
            return dq.empty() ? std::numeric_limits<std::size_t>::max() : dq.front();
        };

        for (std::size_t i = 0; i < sequence.size(); ++i) {
            const Key& key = sequence[i];

            auto& dq = future_positions[key];
            if (!dq.empty() && dq.front() == i) {
                dq.pop_front();  // We are consuming this occurrence right now.
            }

            if (resident.contains(key)) {
                ++hits;
                trace[i] = true;
                continue;
            }

            if (resident.size() >= capacity_) {
                Key victim{};
                std::size_t farthest = 0;
                bool first = true;
                for (const auto& candidate : resident) {
                    const std::size_t nu = next_use_after(candidate);
                    if (first || nu > farthest) {
                        farthest = nu;
                        victim = candidate;
                        first = false;
                    }
                }
                resident.erase(victim);
            }
            resident.insert(key);
        }
        return hits;
    }

    std::size_t capacity() const { return capacity_; }

private:
    std::size_t capacity_;
};

}  // namespace cache
