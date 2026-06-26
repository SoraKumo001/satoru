#include <gtest/gtest.h>
#include "utils/lru_cache.h"

using namespace satoru;

// ---- LruCache Tests ----

TEST(LruCacheTest, PutAndGet) {
    LruCache<int, std::string> cache(3);

    cache.put(1, "one");
    cache.put(2, "two");
    cache.put(3, "three");

    EXPECT_EQ(cache.size(), 3u);

    auto* val = cache.get(1);
    ASSERT_NE(val, nullptr);
    EXPECT_EQ(*val, "one");

    val = cache.get(2);
    ASSERT_NE(val, nullptr);
    EXPECT_EQ(*val, "two");

    val = cache.get(3);
    ASSERT_NE(val, nullptr);
    EXPECT_EQ(*val, "three");
}

TEST(LruCacheTest, GetNonExistent) {
    LruCache<int, std::string> cache(3);

    auto* val = cache.get(99);
    EXPECT_EQ(val, nullptr);
}

TEST(LruCacheTest, EvictsLeastRecentlyUsed) {
    LruCache<int, std::string> cache(2);

    cache.put(1, "one");
    cache.put(2, "two");

    // Access key 1 to make it recently used
    cache.get(1);

    // Insert key 3 -> should evict key 2 (LRU)
    cache.put(3, "three");

    EXPECT_EQ(cache.size(), 2u);

    // Key 2 should be evicted
    EXPECT_EQ(cache.get(2), nullptr);

    // Key 1 and 3 should still be present
    auto* val1 = cache.get(1);
    ASSERT_NE(val1, nullptr);
    EXPECT_EQ(*val1, "one");

    auto* val3 = cache.get(3);
    ASSERT_NE(val3, nullptr);
    EXPECT_EQ(*val3, "three");
}

TEST(LruCacheTest, UpdateExistingKey) {
    LruCache<int, std::string> cache(2);

    cache.put(1, "one");
    cache.put(2, "two");

    // Update key 1 -> size should remain 2
    cache.put(1, "ONE");

    EXPECT_EQ(cache.size(), 2u);

    auto* val = cache.get(1);
    ASSERT_NE(val, nullptr);
    EXPECT_EQ(*val, "ONE");
}

TEST(LruCacheTest, Exists) {
    LruCache<int, std::string> cache(2);

    cache.put(1, "one");

    EXPECT_TRUE(cache.exists(1));
    EXPECT_FALSE(cache.exists(2));
}

TEST(LruCacheTest, Clear) {
    LruCache<int, std::string> cache(3);

    cache.put(1, "one");
    cache.put(2, "two");
    cache.put(3, "three");

    EXPECT_EQ(cache.size(), 3u);

    cache.clear();

    EXPECT_EQ(cache.size(), 0u);
    EXPECT_EQ(cache.get(1), nullptr);
    EXPECT_FALSE(cache.exists(1));
}

TEST(LruCacheTest, SingleCapacity) {
    LruCache<int, std::string> cache(1);

    cache.put(1, "one");
    EXPECT_EQ(cache.size(), 1u);

    cache.put(2, "two");
    EXPECT_EQ(cache.size(), 1u);

    EXPECT_EQ(cache.get(1), nullptr);

    auto* val = cache.get(2);
    ASSERT_NE(val, nullptr);
    EXPECT_EQ(*val, "two");
}

TEST(LruCacheTest, EvictionChain) {
    // Verify that multiple insertions properly evict in order
    LruCache<int, int> cache(3);

    cache.put(1, 10);
    cache.put(2, 20);
    cache.put(3, 30);

    // Access 1 and 2, so 3 is the oldest
    cache.get(1);
    cache.get(2);

    cache.put(4, 40);  // evicts 3
    cache.put(5, 50);  // evicts 1 (was accessed before 2, but 2 was accessed after 1)

    EXPECT_EQ(cache.get(3), nullptr);
    EXPECT_EQ(cache.get(1), nullptr);

    auto* v2 = cache.get(2);
    ASSERT_NE(v2, nullptr);
    EXPECT_EQ(*v2, 20);

    auto* v4 = cache.get(4);
    ASSERT_NE(v4, nullptr);
    EXPECT_EQ(*v4, 40);

    auto* v5 = cache.get(5);
    ASSERT_NE(v5, nullptr);
    EXPECT_EQ(*v5, 50);
}
