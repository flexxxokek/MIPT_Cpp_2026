#include "cache/two_q_cache.hpp"

#include <gtest/gtest.h>

using cache::TwoQCache;

TEST(TwoQCache, MissOnEmptyCache) {
    TwoQCache<int, int> c(4);
    int out;
    EXPECT_FALSE(c.get(1, out));
}

TEST(TwoQCache, BasicPutGet) {
    TwoQCache<int, int> c(4);
    c.put(1, 111);
    int out = -1;
    ASSERT_TRUE(c.get(1, out));
    EXPECT_EQ(out, 111);
}

TEST(TwoQCache, HitInA1inDoesNotDuplicateEntry) {
    TwoQCache<int, int> c(4);
    c.put(1, 1);
    int out;
    c.get(1, out);
    c.get(1, out);
    EXPECT_EQ(c.size(), 1u);
}

TEST(TwoQCache, RespectsCapacityBound) {
    TwoQCache<int, int> c(4);
    for (int i = 0; i < 200; ++i) {
        c.put(i, i);
        EXPECT_LE(c.size(), 4u);
    }
}

TEST(TwoQCache, GhostHitPromotesToMainQueue) {
    // Small A1in so a single insertion pushes older entries out to the
    // ghost list quickly.
    TwoQCache<int, int> c(4, /*kin=*/0.25, /*kout=*/0.5);

    c.put(1, 1);  // Goes to A1in.
    c.put(2, 2);
    c.put(3, 3);
    c.put(4, 4);
    c.put(5, 5);  // Forces key 1 out of A1in into the A1out ghost list.

    int out;
    EXPECT_FALSE(c.get(1, out));  // Ghost: ghost keys carry no value.

    // Re-reference key 1: since its ghost is still around, it should be
    // promoted straight into Am and become resident again.
    c.put(1, 111);
    ASSERT_TRUE(c.get(1, out));
    EXPECT_EQ(out, 111);
}

TEST(TwoQCache, ScanResistanceKeepsHotKeyResident) {
    // A working set of one hot key, interspersed with a long one-time scan.
    // A pure recency-only cache (plain LRU) would evict the hot key during
    // the scan; 2Q should not, because Am (once populated) is protected
    // from a stream of A1in-only cold insertions until they also warm up.
    TwoQCache<int, int> c(8);

    c.put(0, 0);
    int out;
    c.get(0, out);
    c.put(0, 0);  // Re-reference so key 0 can be promoted from a ghost hit
                  // path is unnecessary here; instead force it into Am via
                  // repeated get/put churn below.

    // Prime key 0 into Am by cycling it in and out of the ghost list once.
    for (int i = 1; i <= 3; ++i) c.put(i, i);
    c.put(100, 100);  // Evicts key 0 into ghost (A1in cap ~ 2 for cap=8*0.25).
    c.put(0, 0);       // Ghost hit -> promotes key 0 into Am.

    ASSERT_TRUE(c.get(0, out));

    // Now run a long one-time scan of brand new keys.
    for (int i = 1000; i < 1000 + 50; ++i) {
        c.put(i, i);
    }

    // Key 0 lives in Am and should have survived the scan through A1in.
    EXPECT_TRUE(c.get(0, out));
}

TEST(TwoQCache, UpdatingExistingKeyChangesValueWithoutGrowingSize) {
    TwoQCache<int, int> c(4);
    c.put(1, 1);
    c.put(1, 2);
    EXPECT_EQ(c.size(), 1u);
    int out;
    ASSERT_TRUE(c.get(1, out));
    EXPECT_EQ(out, 2);
}
