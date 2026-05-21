#include "order_book.hpp"
#include <iostream>

int main() {
    OrderBook book;

    book.add({1, Side::Buy,  100.0, 10, 1});
    book.add({2, Side::Sell, 101.0, 5,  2});

    std::cout << "Best bid: " << book.best_bid() << "\n";
    std::cout << "Best ask: " << book.best_ask() << "\n";

    Order o({3, Side::Buy,  102.0, 1, 3});
    std::vector<Trade> trs = book.match(o);
    for (auto& tr : trs ) {
        std::cout << "Trade bid: " << tr.bid_id << "\n";
        std::cout << "Trade ask: " << tr.ask_id << "\n";
        std::cout << "Trade price: " << tr.price << "\n";
        std::cout << "Trade quantity: " << tr.quantity << "\n";
    }

    return 0;
}