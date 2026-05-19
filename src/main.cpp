#include "order_book.hpp"
#include <iostream>

int main() {
    OrderBook book;

    book.add({1, Side::Buy,  100.0, 10, 1});
    book.add({2, Side::Sell, 101.0, 5,  2});

    std::cout << "Best bid: " << book.best_bid() << "\n";
    std::cout << "Best ask: " << book.best_ask() << "\n";

    return 0;
}