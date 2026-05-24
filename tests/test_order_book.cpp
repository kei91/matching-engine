#include <gtest/gtest.h>
#include "order_book.hpp"

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