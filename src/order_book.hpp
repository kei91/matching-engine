#pragma once

#include "order.hpp"

#include <map>
#include <unordered_map>

class OrderBook {
public:
    void add(const Order& order);
    void cancel(uint64_t order_id);

    double best_bid() const;
    double best_ask() const;

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