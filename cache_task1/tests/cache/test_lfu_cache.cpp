#include "cache/lfu_cache.hpp"

#include <gtest/gtest.h>

using cache::LFUCache;

TEST(LFUCache, MissOnEmptyCache) {
    LFUCache<int, int> c(2);
    int out = -1;
    EXPECT_FALSE(c.get(1, out));
    EXPECT_EQ(c.size(), 0u);
}

TEST(LFUCache, BasicPutGet) {
    LFUCache<int, int> c(2);
    c.put(1, 100);
    int out = -1;
    ASSERT_TRUE(c.get(1, out));
    EXPECT_EQ(out, 100);
}

TEST(LFUCache, UpdateExistingKeyKeepsSizeStable) {
    LFUCache<int, int> c(2);
    c.put(1, 100);
    c.put(1, 200);
    EXPECT_EQ(c.size(), 1u);
    int out = -1;
    ASSERT_TRUE(c.get(1, out));
    EXPECT_EQ(out, 200);
}

TEST(LFUCache, EvictsLeastFrequentlyUsed) {
    LFUCache<int, int> c(2);
    c.put(1, 1);
    c.put(2, 2);

    int out;
    // Access key 1 twice, key 2 zero times -> key 2 has lowest frequency.
    c.get(1, out);
    c.get(1, out);

    c.put(3, 3);  // Should evict key 2.

    EXPECT_FALSE(c.get(2, out));
    ASSERT_TRUE(c.get(1, out));
    EXPECT_EQ(out, 1);
    ASSERT_TRUE(c.get(3, out));
    EXPECT_EQ(out, 3);
}

TEST(LFUCache, TiesBrokenByLeastRecentlyUsed) {
    LFUCache<int, int> c(2);
    c.put(1, 1);
    c.put(2, 2);
    // Both keys have frequency 1 (from insertion). Touch key 1 to make it
    // more recent within that frequency bucket, leaving key 2 as the LRU
    // entry at frequency 1.
    int out;
    c.get(1, out);
    c.put(1, 1);  // still freq bucket... put on existing key bumps freq too.

    // Re-establish a clean tie: two fresh keys at freq 1.
    LFUCache<int, int> c2(2);
    c2.put(10, 10);
    c2.put(20, 20);
    c2.put(30, 30);  // Evicts key 10 (older / LRU among freq==1 entries).

    EXPECT_FALSE(c2.get(10, out));
    ASSERT_TRUE(c2.get(20, out));
    ASSERT_TRUE(c2.get(30, out));
}

TEST(LFUCache, ZeroCapacityNeverStoresAnything) {
    LFUCache<int, int> c(0);
    c.put(1, 1);
    int out;
    EXPECT_FALSE(c.get(1, out));
    EXPECT_EQ(c.size(), 0u);
}

TEST(LFUCache, RespectsCapacityBound) {
    LFUCache<int, int> c(3);
    for (int i = 0; i < 100; ++i) {
        c.put(i, i);
        EXPECT_LE(c.size(), 3u);
    }
}

TEST(LFUCache, HighFrequencyKeySurvivesManyInsertions) {
    LFUCache<int, int> c(2);
    c.put(1, 1);
    int out;
    for (int i = 0; i < 50; ++i) {
        c.get(1, out);  // Keep bumping key 1's frequency.
    }
    for (int i = 100; i < 150; ++i) {
        c.put(i, i);  // Every one of these should evict the *other* slot.
    }
    ASSERT_TRUE(c.get(1, out));
    EXPECT_EQ(out, 1);
}
