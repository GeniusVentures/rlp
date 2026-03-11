#include <discv4/dial_history.hpp>
#include <gtest/gtest.h>

#include <chrono>
#include <thread>

using discv4::DialHistory;

namespace {

/// @brief Build a NodeId filled with a single repeated byte value.
std::array<uint8_t, 64> make_node_id(uint8_t fill)
{
    std::array<uint8_t, 64> id{};
    id.fill(fill);
    return id;
}

} // namespace

// ---------------------------------------------------------------------------
// DialHistoryTest suite — mirrors go-ethereum p2p/util_test.go expHeap tests
// ---------------------------------------------------------------------------

TEST(DialHistoryTest, EmptyContainsNothing)
{
    DialHistory h;
    EXPECT_FALSE(h.contains(make_node_id(0x01)));
    EXPECT_EQ(h.size(), 0u);
}

TEST(DialHistoryTest, AddedNodeIsContained)
{
    DialHistory h;
    const auto id = make_node_id(0x01);
    h.add(id);
    EXPECT_TRUE(h.contains(id));
    EXPECT_EQ(h.size(), 1u);
}

TEST(DialHistoryTest, DifferentNodesAreIndependent)
{
    DialHistory h;
    const auto id1 = make_node_id(0x01);
    const auto id2 = make_node_id(0x02);
    h.add(id1);
    EXPECT_TRUE(h.contains(id1));
    EXPECT_FALSE(h.contains(id2));
}

TEST(DialHistoryTest, ExpiredEntryIsNotContained)
{
    // Use a 50 ms expiry so the test finishes quickly.
    DialHistory h(std::chrono::milliseconds(50));
    const auto id = make_node_id(0x01);
    h.add(id);
    ASSERT_TRUE(h.contains(id));

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Before expire() the entry is still in the map (just past its deadline).
    EXPECT_FALSE(h.contains(id));  // contains() checks the clock directly
}

TEST(DialHistoryTest, ExpireRemovesOnlyStaleEntries)
{
    DialHistory h(std::chrono::milliseconds(50));
    const auto id1 = make_node_id(0x01);
    const auto id2 = make_node_id(0x02);

    h.add(id1);
    std::this_thread::sleep_for(std::chrono::milliseconds(70));
    // id1 is now stale; add id2 with a fresh expiry
    h.add(id2);

    EXPECT_EQ(h.size(), 2u);  // both still in the map before expire()
    h.expire();
    EXPECT_EQ(h.size(), 1u);  // id1 pruned, id2 remains

    EXPECT_FALSE(h.contains(id1));
    EXPECT_TRUE(h.contains(id2));
}

TEST(DialHistoryTest, ExpireOnEmptyIsNoOp)
{
    DialHistory h;
    EXPECT_NO_THROW(h.expire());
    EXPECT_EQ(h.size(), 0u);
}

TEST(DialHistoryTest, ReAddRefreshesExpiry)
{
    DialHistory h(std::chrono::milliseconds(50));
    const auto id = make_node_id(0x01);
    h.add(id);
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    // Re-add before it expires — should refresh the deadline
    h.add(id);
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    // 60 ms since first add, but only 30 ms since second add: still alive
    EXPECT_TRUE(h.contains(id));
}
