#pragma once

#include "order.hpp"
#include <array>
#include <cmath>
#include <stdexcept>

#include <memory_resource>
#include <string>

// covers price range from 99.00 to 101.56
static constexpr int32_t BASE_TICK = 9900;
static constexpr int32_t CAPACITY  = 256;

static constexpr double TICK_SIZE   = 0.01;
static constexpr double TICK_FACTOR = 1.0 / TICK_SIZE;

inline int32_t price_to_tick(double price) noexcept {
    return static_cast<int32_t>(std::round(price * TICK_FACTOR));
}
 
inline double tick_to_price(int32_t tick) noexcept {
    return tick * TICK_SIZE;
}

class PriceLevelArray {
public:
    PriceLevelArray(std::pmr::memory_resource* mem, bool best_is_high) 
        : m_mem(mem) 
        , m_best_is_high (best_is_high)
    { 
        reset();
    }

    void reset() {
        for (auto& pl : m_levels) {
            pl.orders = std::pmr::list<Order>{m_mem};
            pl.price = 0.0;
            pl.total_quantity = 0;
        }
        m_best_id = -1;
        m_size = 0;
    }
 
    std::list<Order>::iterator add_order(const Order& order) {
        int32_t id = tick_index(price_to_tick(order.price));
        PriceLevel& pl = m_levels[id];

        if (pl.orders.empty())
            ++m_size;

        pl.price = order.price;
        pl.total_quantity += order.quantity;
        pl.orders.push_back(order);
        return std::prev(pl.orders.end());
    }
 
    void update_best(double price) noexcept {
        int32_t id = tick_index(price_to_tick(price));
        if (m_best_id == -1 || is_better(id, m_best_id))
            m_best_id = id;
    }
 
    void erase_order(double price, std::list<Order>::iterator it, uint64_t quantity) {
        int32_t id = tick_index(price_to_tick(price));
        PriceLevel& pl = m_levels[id];
        pl.total_quantity -= quantity;
        pl.orders.erase(it);
        if (pl.orders.empty()) {
            --m_size;
            if (id == m_best_id)
                m_best_id = find_best_from(m_best_is_high ? id - 1 : id + 1);
        }
    }
 
    PriceLevel* best_level() noexcept {
        return m_best_id == -1 ? nullptr : &m_levels[m_best_id];
    }
 
    double best_price() const noexcept {
        return m_best_id == -1 ? 0.0 : tick_to_price(BASE_TICK + m_best_id);
    }
 
    void advance_best() noexcept {
        if (m_best_id != -1)
            m_best_id = find_best_from(m_best_is_high ? m_best_id - 1 : m_best_id + 1);
    }
 
    bool empty() const noexcept { return m_size == 0; }
 
private:
    int32_t tick_index(int32_t tick) const {
        int32_t id = tick - BASE_TICK;
        if (id < 0 || id >= CAPACITY) {
            std::string str = "price out of PriceLevelArray range: " + std::to_string(id);
            throw std::out_of_range(str);
        }    
        return id;
    }
 
    bool is_better(int32_t compare, int32_t current) const noexcept {
        return m_best_is_high ? compare > current : compare < current;
    }

    int32_t find_best_from(int32_t from) const noexcept {
        if (m_best_is_high) {
            for (int32_t i = from; i >= 0; --i)
                if (!m_levels[i].orders.empty()) return i;
        } else {
            for (int32_t i = from; i < CAPACITY; ++i)
                if (!m_levels[i].orders.empty()) return i;
        }
        return -1;
    }

private:
    std::pmr::memory_resource* m_mem;

    bool m_best_is_high;
    int32_t m_best_id;
    int32_t m_size;
    
    std::array<PriceLevel, CAPACITY> m_levels;
};