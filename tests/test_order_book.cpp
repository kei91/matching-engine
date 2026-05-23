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
