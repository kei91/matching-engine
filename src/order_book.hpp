#pragma once

#include "order.hpp"

#include <map>
#include <unordered_map>
#include <vector>

class OrderBook {
public:
    void add(const Order& order);
    void cancel(uint64_t order_id);

    double best_bid() const;
    double best_ask() const;

    std::vector<Trade> match(Order& incoming_order);

private:
    template<typename mapType, typename matchCheck>
    std::vector<Trade> matchBook(Order& incoming_order, mapType& book, matchCheck canMatch);

private:
    std::map<double, PriceLevel, std::greater<double>> m_bids;
    std::map<double, PriceLevel> m_asks;
    std::unordered_map<uint64_t, std::pair<Side, double>> m_order_price_index;
};

inline void OrderBook::add(const Order& order) {
    PriceLevel& pl = (order.side == Side::Buy) ? m_bids[order.price] : m_asks[order.price];
    pl.price = order.price;
    pl.total_quantity += order.quantity;
    pl.orders.push_back(order);
    m_order_price_index[order.id] = { order.side, order.price};
}

inline void OrderBook::cancel(uint64_t order_id) {
    auto it = m_order_price_index.find(order_id);
    if (it == m_order_price_index.end())
        return;

    uint64_t id = it->first;
    auto [side, price] = it->second;
    PriceLevel& pl = side == Side::Buy ? m_bids[price] : m_asks[price];

    for (std::deque<Order>::iterator it = pl.orders.begin(); it != pl.orders.end(); ++it) {
        if (it->id == id) {
            pl.total_quantity -= it->quantity;
            pl.orders.erase(it);
            break;
        }
    }

    m_order_price_index.erase(it);
}

inline double OrderBook::best_bid() const {
    return m_bids.empty() ? 0.0 : m_bids.begin()->first;
}

inline double OrderBook::best_ask() const {
    return m_asks.empty() ? 0.0 : m_asks.begin()->first;
}

inline std::vector<Trade> OrderBook::match(Order& incoming_order) {
    return incoming_order.side == Side::Buy 
            ? matchBook(incoming_order, m_asks, [](double p, double bp) { return p >= bp;}) 
            : matchBook(incoming_order, m_bids, [](double p, double bp) { return p <= bp;});
}

template<typename mapType, typename matchCheck>
std::vector<Trade> OrderBook::matchBook(Order& incoming_order, mapType& book, matchCheck canMatch) {
    std::vector<Trade> trades;

    while (incoming_order.quantity > 0 &&  !book.empty()) {
        auto& [bp, pl] = *book.begin();
        if (!canMatch(incoming_order.price, bp))
            break;

        Order& order = pl.orders.front();

        uint64_t quantity = std::min(incoming_order.quantity, order.quantity);
        incoming_order.quantity -= quantity;
        order.quantity -= quantity;
        pl.total_quantity -= quantity;

        trades.push_back({
            incoming_order.side == Side::Buy ? incoming_order.id : order.id,
            incoming_order.side == Side::Buy ? order.id : incoming_order.id,
            bp,
            quantity
        });

        if (order.quantity == 0) {
            pl.orders.pop_front();
            if (pl.orders.empty()) {
                book.erase(book.begin());
            }
        }
    }

    if (incoming_order.quantity > 0)
        add(incoming_order);

    return trades;
}
