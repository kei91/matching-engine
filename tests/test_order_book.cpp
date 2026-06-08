#include <gtest/gtest.h>
#include "order_book.hpp"
#include "spsc_queue.hpp"

TEST(OrderBookTest, AddBuyOrder) {
    OrderBook book;
    book.add({1, Side::Buy, 100.0, 10, 1});
    EXPECT_EQ(book.best_bid(), 100.0);
}

TEST(OrderBookTest, AddSellOrder) {
    OrderBook book;
    book.add({1, Side::Sell, 101.0, 5, 1});
    EXPECT_EQ(book.best_ask(), 101.0);
}

TEST(OrderBookTest, FullMatch) {
    OrderBook book;
    book.add({1, Side::Sell, 100.0, 10, 1});
    
    Order buy = {2, Side::Buy, 100.0, 10, 2};
    auto trades = book.match(buy);

    ASSERT_EQ(trades.size(), 1);
    EXPECT_EQ(trades[0].quantity, 10);
    EXPECT_EQ(book.best_ask(), 0.0); 
}

TEST(OrderBookTest, NoMatch) {
    OrderBook book;
    book.add({1, Side::Sell, 101.0, 10, 1});
    
    Order buy = {2, Side::Buy, 100.0, 10, 2};
    auto trades = book.match(buy);

    EXPECT_TRUE(trades.empty());
    EXPECT_EQ(book.best_bid(), 100.0);
    EXPECT_EQ(book.best_ask(), 101.0); 
}

TEST(OrderBookTest, PartialFill_IncomingLarger) {
    OrderBook book;
    book.add({1, Side::Sell, 100.0, 10, 1});

    Order buy = {2, Side::Buy, 100.0, 16, 2};
    auto trades = book.match(buy);

    ASSERT_EQ(trades.size(), 1);
    EXPECT_EQ(trades[0].quantity, 10);
    EXPECT_EQ(book.best_bid(), 100.0);
    EXPECT_EQ(book.best_ask(), 0.0); 
}

TEST(OrderBookTest, PartialFill_MakerLarger) {
    OrderBook book;
    book.add({1, Side::Sell, 100.0, 10, 1});

    Order buy = {2, Side::Buy, 100.0, 6, 2};
    auto trades = book.match(buy);

    ASSERT_EQ(trades.size(), 1);
    EXPECT_EQ(trades[0].quantity, 6);
    EXPECT_EQ(book.best_bid(), 0.0);
    EXPECT_EQ(book.best_ask(), 100.0); 
}

TEST(OrderBookTest, MultiLevelMatch) {
    OrderBook book;
    book.add({1, Side::Sell, 100.0, 5, 1});
    book.add({2, Side::Sell, 101.0, 5, 2});

    Order buy = {3, Side::Buy, 101.0, 10, 3};
    auto trades = book.match(buy);

    ASSERT_EQ(trades.size(), 2);
    EXPECT_EQ(trades[0].quantity, 5);
    EXPECT_EQ(trades[1].quantity, 5);
    EXPECT_EQ(book.best_ask(), 0.0); 
}

TEST(OrderBookTest, PriceTimePriority) {
    OrderBook book;
    book.add({1, Side::Sell, 100.0, 5, 1});
    book.add({2, Side::Sell, 100.0, 5, 2});

    Order buy = {3, Side::Buy, 100.0, 5, 3};
    auto trades = book.match(buy);

    ASSERT_EQ(trades.size(), 1);
    EXPECT_EQ(trades[0].quantity, 5);
    EXPECT_EQ(trades[0].ask_id, 1); 
}

TEST(OrderBookTest, IndexCleanedOnPartialLevelFill) {
    OrderBook book;
    book.add({1, Side::Sell, 100.0, 100, 0});
    book.add({2, Side::Sell, 100.0, 100, 1});

    Order buy{3, Side::Buy, 100.0, 100, 2};
    book.match(buy); 

    book.cancel(1);
    EXPECT_EQ(book.best_ask(), 100.0);
}

TEST (SPSCQueueTest, QueueOrder) {
    SPSCQueue<int, 8> q;
    for (int i = 0; i < 5; ++i)
        EXPECT_TRUE(q.push(i));

    int out = 0;
    for (int i = 0; i < 5; ++i) {
        EXPECT_TRUE(q.pop(out));
        EXPECT_EQ(out, i);
    }
}

TEST (SPSCQueueTest, EmptyQueue) {
    SPSCQueue<int, 8> q;

    int out;
    EXPECT_FALSE(q.pop(out));
}

TEST (SPSCQueueTest, PushFull) {
    SPSCQueue<int, 4> q;
    EXPECT_TRUE(q.push(1));
    EXPECT_TRUE(q.push(2));
    EXPECT_TRUE(q.push(3));
    EXPECT_FALSE(q.push(4));
}

TEST (SPSCQueueTest, WrapAround) {
    SPSCQueue<int, 4> q;
    int out, expected = 0, next = 0;

    for (int round = 0; round < 100; ++round) {
        while (q.push(next))
            ++next;

        while (q.pop(out)) {
            EXPECT_EQ(out, expected);
            ++expected;
        }
    }

    EXPECT_EQ(expected, next); 
}

TEST(SPSCQueueTest, Interleaved) {
    SPSCQueue<int, 4> q;
    int out, expected = 0, next = 0;
    for (int i = 0; i < 100; ++i) {
        if (q.push(next)) ++next;
        if (q.push(next)) ++next;
        
        EXPECT_TRUE(q.pop(out));
        EXPECT_EQ(out, expected++);
    }
    while (q.pop(out))
        ASSERT_EQ(out, expected++);

    EXPECT_EQ(expected, next);
}