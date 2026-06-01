#include <benchmark/benchmark.h>
#include "order_book.hpp"

static uint64_t g_id = 0;
static uint64_t g_ts = 0;

void SetupBook(OrderBook& book, std::vector<uint64_t>& out_ids) {
    double mid = 100.0;
    double tick = 0.01;

    for (int i = 1; i <= 20; i++) {
        for (int j = 0; j < 5; j++) {
            uint64_t bid_id = ++g_id;
            uint64_t ask_id = ++g_id;
            book.add({ bid_id, Side::Buy,  mid - i * tick, 100, ++g_ts });
            book.add({ ask_id, Side::Sell, mid + i * tick, 100, ++g_ts });
            out_ids.push_back(bid_id);
            out_ids.push_back(ask_id);
        }
    }
}

std::vector<Order> GenerateOrders(int amount) {
    std::vector<Order> orders;
    orders.reserve(amount * 2);

    double mid = 100.0;
    double tick = 0.01;

    for (int i = 0; i < amount; i++) {
        int level = (i % 10) + 1;
        orders.push_back({ ++g_id, Side::Buy,  mid - level * tick, 100, ++g_ts });
        orders.push_back({ ++g_id, Side::Sell, mid + level * tick, 100, ++g_ts });
    }
    return orders;
}

static void BM_AddOrder(benchmark::State& state) {
    OrderBook book;
    std::vector<uint64_t> ids;
    SetupBook(book, ids);

    std::vector<Order> incoming = GenerateOrders(10000);

    size_t i = 0;
    for (auto _ : state) {
        if (i > 0 && i % incoming.size() == 0) { // reset book to manage increasing size
            state.PauseTiming();
            book.reset();
            ids.clear();
            SetupBook(book, ids);
            state.ResumeTiming();
        }
        book.add(incoming[i % incoming.size()]);
        ++i;
    }
}

static void BM_CancelOrders(benchmark::State& state) {
    OrderBook book;
    std::vector<uint64_t> ids;
    SetupBook(book, ids);

    for (auto _ : state) {
        if (ids.empty()) {
            state.PauseTiming();
            book.reset();
            ids.clear();
            SetupBook(book, ids);
            state.ResumeTiming();
        }
        book.cancel(ids.back());
        ids.pop_back();
    }
}

static void BM_MatchOrders(benchmark::State& state) {
    OrderBook book;
    std::vector<uint64_t> ids;
    SetupBook(book, ids);

    for (auto _ : state) {
        // Reset book if it's empty
        if (book.best_bid() == 0.0 || book.best_ask() == 0.0) {
            state.PauseTiming();
            book.reset();
            ids.clear();
            SetupBook(book, ids);
            state.ResumeTiming();
        }

        Order o { ++g_id, Side::Buy, 100.05, 50, ++g_ts };
        book.match(o);
    }
}

static void BM_MixedWorkload(benchmark::State& state) {
    OrderBook book;
    std::vector<uint64_t> ids;
    SetupBook(book, ids);

    std::vector<int> randoms(1000000);
    for (auto& r : randoms) r = rand() % 100; // segmentation faut is I use it in for (auto _ : state)

    size_t i = 0;
    for (auto _ : state) {
        int r = randoms[i % randoms.size()];

        if (r < 60) {
            uint64_t new_id = ++g_id;
            book.add({ new_id, Side::Buy, 99.95, 100, ++g_ts });
            ids.push_back(new_id);
        } else if (r < 85) {
            if (!ids.empty()) {
                book.cancel(ids.back());
                ids.pop_back();
            }
        } else {
            if (book.best_ask() > 0.0) {
                Order o { ++g_id, Side::Buy, 100.05, 50, ++g_ts };
                book.match(o);
            }
        }

        // Reset book if it's almost empty
        if (ids.size() < 10) {
            state.PauseTiming();
            book.reset();
            ids.clear();
            SetupBook(book, ids);
            state.ResumeTiming();
        }
        ++i;
    }
}

BENCHMARK(BM_AddOrder)->Iterations(1000000);
BENCHMARK(BM_CancelOrders)->Iterations(1000000);
BENCHMARK(BM_MatchOrders)->Iterations(1000000);
BENCHMARK(BM_MixedWorkload)->Iterations(1000000);

BENCHMARK_MAIN();