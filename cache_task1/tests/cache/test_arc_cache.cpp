#include "cache/arc_cache.hpp"

#include <gtest/gtest.h>

using cache::ARCCache;

TEST(ARCCache, MissOnEmptyCache) {
    ARCCache<int, int> c(4);
    int out;
    EXPECT_FALSE(c.get(1, out));
}

TEST(ARCCache, BasicPutGet) {
    ARCCache<int, int> c(4);
    c.put(1, 111);
    int out = -1;
    ASSERT_TRUE(c.get(1, out));
    EXPECT_EQ(out, 111);
}

TEST(ARCCache, UpdateExistingKeyKeepsSizeStable) {
    ARCCache<int, int> c(4);
    c.put(1, 1);
    c.put(1, 2);
    EXPECT_EQ(c.size(), 1u);
    int out;
    ASSERT_TRUE(c.get(1, out));
    EXPECT_EQ(out, 2);
}

TEST(ARCCache, RespectsCapacityBound) {
    ARCCache<int, int> c(5);
    for (int i = 0; i < 500; ++i) {
        c.put(i, i);
        EXPECT_LE(c.size(), 5u);
    }
}

TEST(ARCCache, RepeatedAccessPromotesToT2AndSurvivesChurn) {
    ARCCache<int, int> c(4);
    c.put(1, 1);
    int out;
    ASSERT_TRUE(c.get(1, out));  // Hit: moves key 1 from T1 to T2.
    c.put(1, 1);                 // Access again to reinforce.

    // Push enough distinct new keys through T1 to cause repeated evictions.
    for (int i = 100; i < 120; ++i) {
        c.put(i, i);
    }

    // Key 1 sits in T2 (frequency list) and should survive a stream of
    // once-only T1 insertions, unlike a plain LRU cache of the same size.
    EXPECT_TRUE(c.get(1, out));
}

TEST(ARCCache, GhostHitInB1AdaptsTowardsRecency) {
    ARCCache<int, int> c(2);
    c.put(1, 1);
    c.put(2, 2);
    c.put(3, 3);  // Evicts key 1 (T1 LRU) into B1.

    int out;
    EXPECT_FALSE(c.get(1, out));

    // Re-inserting key 1 should hit the B1 ghost, adapt p upward, and bring
    // key 1 back as a resident (now in T2).
    c.put(1, 111);
    ASSERT_TRUE(c.get(1, out));
    EXPECT_EQ(out, 111);
    EXPECT_LE(c.size(), 2u);
}

TEST(ARCCache, OutperformsPlainRecencyOnLoopingScanPattern) {
    // Classic ARC benchmark shape: a small hot loop plus a one-off scan,
    // repeated. A frequency-blind LRU cache thrashes on this; ARC should
    // retain the loop's working set thanks to T2/B2.
    constexpr int kLoopSize = 4;
    constexpr int kCapacity = 6;
    ARCCache<int, int> c(kCapacity);

    auto access_loop = [&]() {
        for (int i = 0; i < kLoopSize; ++i) {
            int out;
            if (!c.get(i, out)) {
                c.put(i, i);
            }
        }
    };

    // Warm up: touch the loop keys enough times that they become T2/B2
    // "hot" entries.
    for (int r = 0; r < 3; ++r) access_loop();

    // Interleave the hot loop with one-off scan keys.
    int scan_key = 1000;
    for (int round = 0; round < 20; ++round) {
        access_loop();
        int out;
        if (!c.get(scan_key, out)) {
            c.put(scan_key, scan_key);
        }
        ++scan_key;
    }

    // After the interleaved scan, the small hot loop should still mostly
    // hit.
    int hits = 0;
    for (int i = 0; i < kLoopSize; ++i) {
        int out;
        if (c.get(i, out)) ++hits;
    }
    EXPECT_GE(hits, kLoopSize / 2);
}
