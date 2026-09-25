#include "cache/lirs_cache.hpp"

#include <gtest/gtest.h>

using cache::LIRSCache;

TEST(LIRSCache, MissOnEmptyCache) {
    LIRSCache<int, int> c(8);
    int out;
    EXPECT_FALSE(c.get(1, out));
}

TEST(LIRSCache, BasicPutGet) {
    LIRSCache<int, int> c(8);
    c.put(1, 111);
    int out = -1;
    ASSERT_TRUE(c.get(1, out));
    EXPECT_EQ(out, 111);
}

TEST(LIRSCache, UpdateExistingKeyKeepsSizeStable) {
    LIRSCache<int, int> c(8);
    c.put(1, 1);
    c.put(1, 2);
    EXPECT_EQ(c.size(), 1u);
    int out;
    ASSERT_TRUE(c.get(1, out));
    EXPECT_EQ(out, 2);
}

TEST(LIRSCache, RespectsCapacityBound) {
    LIRSCache<int, int> c(10);
    for (int i = 0; i < 500; ++i) {
        c.put(i, i);
        EXPECT_LE(c.size(), 10u);
    }
}

TEST(LIRSCache, RepeatedKeyEventuallyBecomesResidentAndSticky) {
    LIRSCache<int, int> c(8);
    int out;

    // Reference key 1 repeatedly so it gets promoted to LIR.
    for (int i = 0; i < 5; ++i) {
        if (!c.get(1, out)) {
            c.put(1, 1);
        }
    }
    ASSERT_TRUE(c.get(1, out));

    // Flood with brand-new keys.
    for (int i = 100; i < 200; ++i) {
        c.put(i, i);
    }

    // A hot (LIR) key should survive a flood of one-time references much
    // better than a cold one would.
    EXPECT_TRUE(c.get(1, out));
}

TEST(LIRSCache, ScanResistanceBeatsPlainRecencyOnly) {
    // Build a small hot working set, then run a long one-time scan much
    // larger than the cache. LIRS should retain the hot set because only a
    // small HIR quota is churned by the scan; a size-matched pure-LRU cache
    // would evict everything.
    //
    // Each hot key is referenced twice back-to-back, giving it a low
    // inter-reference recency (IRR) so it gets promoted to LIR -- this is
    // exactly the pattern LIRS is designed to recognize as "hot". (A plain
    // round-robin single pass over the working set, by contrast, gives every
    // key an equally large IRR and is a known corner case where only one
    // element gets promoted per cycle -- that is expected LIRS behavior,
    // not something this test is trying to exercise.)
    constexpr std::size_t kCapacity = 20;
    constexpr int kHotSetSize = 5;
    LIRSCache<int, int> lirs(kCapacity, /*hir_ratio=*/0.2);

    int out;
    for (int round = 0; round < 3; ++round) {
        for (int i = 0; i < kHotSetSize; ++i) {
            if (!lirs.get(i, out)) lirs.put(i, i);
            if (!lirs.get(i, out)) lirs.put(i, i);  // Immediate re-reference.
        }
    }

    // Long one-time scan, much bigger than the cache.
    for (int i = 1000; i < 1000 + 500; ++i) {
        lirs.put(i, i);
    }

    int hits = 0;
    for (int i = 0; i < kHotSetSize; ++i) {
        if (lirs.get(i, out)) ++hits;
    }
    // The hot set should have survived the scan (LIRS's key property).
    EXPECT_EQ(hits, kHotSetSize);
}

TEST(LIRSCache, ContainsReflectsResidency) {
    LIRSCache<int, int> c(4);
    EXPECT_FALSE(c.contains(1));
    c.put(1, 1);
    EXPECT_TRUE(c.contains(1));
}
