#include "cachesystem/cache_system.h"

#include <gtest/gtest.h>
#include <sstring>

using namespace cache;

TEST(CacheSystem, Builds) {
    std::istringstream strStream {
    "2"
    "2 LRU"
    "3 LRU"
    };
    CacheSystem<int, int> s{strStream};

    s.put(1, 2);
    s.put(2, 2);
}
