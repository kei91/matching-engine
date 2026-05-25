#include <benchmark/benchmark.h>
#include "order_book.hpp"

void SetupBook(OrderBook& book) {
    double mid = 100.0;
    double tick = 0.01;
    
    uint id = 0;
    uint ts = 0;
    for (int i = 1; i <= 20; i++) {
        for (int j = 0; j < 5; j++) {
            book.add({ ++id, Side::Buy,  mid - i * tick, 100, ++ts });
            book.add({ ++id, Side::Sell, mid + i * tick, 100, ++ts });
        }
    }
}

std::vector<Order> GenerateOrders(int amount) {
    std::vector<Order> orders;

    double mid = 100.0;
    double tick = 0.01;

    uint id = 0;
    uint ts = 0;
    for (int i = 1; i <= amount; i++) {
        orders.push_back({ ++id, Side::Buy,  mid - i * tick, 100, ++ts });
        orders.push_back({ ++id, Side::Sell, mid + i * tick, 100, ++ts });
    }

    return orders;
}

static void BM_AddOrder(benchmark::State& state) {
    OrderBook book;
    
    SetupBook(book);
    
    std::vector<Order> incoming = GenerateOrders(10000);
    
    size_t i = 0;
    for (auto s : state) {
        book.add(incoming[i % incoming.size()]);
        ++i;
    }
}

BENCHMARK(BM_AddOrder)->Iterations(1'000'000);

BENCHMARK_MAIN();