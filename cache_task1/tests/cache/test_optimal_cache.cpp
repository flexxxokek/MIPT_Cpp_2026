#include "cache/optimal_cache.hpp"

#include <algorithm>
#include <deque>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <gtest/gtest.h>

using cache::OptimalCache;

TEST(OptimalCache, EmptySequenceHasZeroHits) {
    OptimalCache<int> c(3);
    EXPECT_EQ(c.run({}), 0u);
}

TEST(OptimalCache, ZeroCapacityNeverHits) {
    OptimalCache<int> c(0);
    EXPECT_EQ(c.run({1, 1, 1, 2, 2}), 0u);
}

TEST(OptimalCache, AllUniqueKeysNeverHit) {
    OptimalCache<int> c(3);
    EXPECT_EQ(c.run({1, 2, 3, 4, 5}), 0u);
}

TEST(OptimalCache, CapacityCoversWholeWorkingSetAlwaysHitsAfterFirstUse) {
    OptimalCache<int> c(3);
    // 3 distinct keys, capacity 3: after each key's first appearance, every
    // later occurrence must hit.
    std::vector<int> seq = {1, 2, 3, 1, 2, 3, 1, 2, 3};
    EXPECT_EQ(c.run(seq), 6u);  // First 3 are misses, the rest are hits.
}

TEST(OptimalCache, KnownHandComputedExample) {
    // Sequence: 1 2 3 4 1 2 5 1 2 3 4 5, capacity = 3.
    // Belady's optimal decisions (evict the one used farthest in future):
    //  1(miss,{1}) 2(miss,{1,2}) 3(miss,{1,2,3})
    //  4: must evict; next uses: 1->idx4, 2->idx5, 3->never -> evict 3.
    //     {1,2,4} (miss)
    //  1: hit ({1,2,4})
    //  2: hit ({1,2,4})
    //  5: must evict; next uses: 1->idx7,2->idx8,4->never -> evict 4.
    //     {1,2,5} (miss)
    //  1: hit
    //  2: hit
    //  3: must evict; next uses: 1->never,2->never,5->idx11 -> evict 1 (or 2,
    //     tie; either is "never used again" so either choice is optimal).
    //     (miss) -> resident becomes {2,5,3} or {1,5,3} depending on tie-break
    //  4: miss regardless (not resident, and never used again after this)
    //  5: hit (5 still resident in both tie-break outcomes)
    // Total hits = 1,2 (x2 each) + 5 (last) = 5 hits, 7 misses.
    std::vector<int> seq = {1, 2, 3, 4, 1, 2, 5, 1, 2, 3, 4, 5};
    OptimalCache<int> c(3);
    EXPECT_EQ(c.run(seq), 5u);
}

TEST(OptimalCache, TraceLengthMatchesSequenceAndSumsToHitCount) {
    std::vector<int> seq = {1, 2, 3, 1, 4, 2, 1, 5, 2, 1};
    OptimalCache<int> c(2);
    std::vector<bool> trace;
    std::size_t hits = c.runWithTrace(seq, trace);
    ASSERT_EQ(trace.size(), seq.size());
    std::size_t counted = 0;
    for (bool h : trace) {
        if (h) ++counted;
    }
    EXPECT_EQ(counted, hits);
}

namespace {

// Simple textbook LRU used purely as a reference to check that Belady's
// algorithm never does *worse* than LRU on the same sequence.
std::size_t simulateLRUHits(const std::vector<int>& seq, std::size_t capacity) {
    std::deque<int> order;  // front = MRU, back = LRU.
    std::unordered_set<int> resident;
    std::size_t hits = 0;
    for (int key : seq) {
        if (resident.contains(key)) {
            ++hits;
            order.erase(std::find(order.begin(), order.end(), key));
            order.push_front(key);
            continue;
        }
        if (resident.size() >= capacity) {
            int victim = order.back();
            order.pop_back();
            resident.erase(victim);
        }
        order.push_front(key);
        resident.insert(key);
    }
    return hits;
}

}  // namespace

TEST(OptimalCache, NeverWorseThanLRUOnRandomishSequence) {
    std::vector<int> seq;
    // Deterministic pseudo-random-ish pattern (no <random> dependency needed
    // for reproducibility): a mix of loops and irregular jumps.
    for (int round = 0; round < 30; ++round) {
        seq.push_back(round % 7);
        seq.push_back((round * 3) % 11);
        seq.push_back(round % 4);
        seq.push_back((round * 5 + 2) % 13);
    }

    OptimalCache<int> opt(4);
    std::size_t opt_hits = opt.run(seq);
    std::size_t lru_hits = simulateLRUHits(seq, 4);

    EXPECT_GE(opt_hits, lru_hits);
}
