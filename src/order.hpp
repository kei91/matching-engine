#pragma once

#include <cstdint>
#include <deque>

enum class Side {
	Buy,
	Sell
};

struct Order {
	uint64_t	id;
	Side 		side;
	double		price; // TODO: experiment with tick size
	uint64_t	quantity;
	uint64_t	timestamp;
};

struct PriceLevel {
    double   price;
    uint64_t total_quantity;
    std::deque<Order> orders;
};