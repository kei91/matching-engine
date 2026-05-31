#pragma once

#include "order.hpp"
#include "price_level_array.hpp"

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
    PriceLevelArray m_bids{true};
    PriceLevelArray m_asks{false};

private:

    struct OrderIndex {
        Side side;
        double price;
        std::list<Order>::iterator it;
    };

    std::unordered_map<uint64_t, OrderIndex> m_order_price_index;
};

inline void OrderBook::add(const Order& order) {
    PriceLevelArray& arr = (order.side == Side::Buy) ? m_bids : m_asks;

    auto it = arr.add_order(order);
    arr.update_best(order.price);
    m_order_price_index[order.id] = { order.side, order.price, it };
}

inline void OrderBook::cancel(uint64_t order_id) {
    auto it = m_order_price_index.find(order_id);
    if (it == m_order_price_index.end())
        return;

    auto& [side, price, order_it] = it->second;
    PriceLevelArray& arr = (side == Side::Buy) ? m_bids : m_asks;
    arr.erase_order(price, order_it, order_it->quantity);
    m_order_price_index.erase(it);
}

inline double OrderBook::best_bid() const {
    return m_bids.best_price();
}

inline double OrderBook::best_ask() const {
    return m_asks.best_price();
}

inline std::vector<Trade> OrderBook::match(Order& incoming_order) {
    std::vector<Trade> trades;
    bool is_bid = incoming_order.side == Side::Buy;
    PriceLevelArray& book = is_bid ? m_asks : m_bids;

    while (incoming_order.quantity > 0 && !book.empty()) {
        PriceLevel* pl = book.best_level();
        if (!pl || pl->orders.empty())
            break;

        double level_price = pl->price;
        if (is_bid && incoming_order.price < level_price)
            break;
        if (!is_bid && incoming_order.price > level_price)
            break;

        Order& order = pl->orders.front();
        uint64_t quantity = std::min(incoming_order.quantity, order.quantity);

        incoming_order.quantity -= quantity;
        order.quantity          -= quantity;
        pl->total_quantity      -= quantity;

        trades.push_back({
            is_bid ? incoming_order.id : order.id,
            is_bid ? order.id  : incoming_order.id,
            level_price,
            quantity
        });

        if (order.quantity == 0) {
            m_order_price_index.erase(order.id);
            pl->orders.pop_front();
            if (pl->orders.empty())
                book.advance_best();
        }
    }

    if (incoming_order.quantity > 0)
        add(incoming_order);

    return trades;
}
