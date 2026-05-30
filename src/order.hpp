#pragma once

#include <cstdint>
#include <list>

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
    std::list<Order> orders;
};

struct Trade {
	uint64_t bid_id;
	uint64_t ask_id;
	double price;
	uint64_t quantity;
};