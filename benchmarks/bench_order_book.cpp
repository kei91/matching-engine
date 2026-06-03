#include <benchmark/benchmark.h>
#include "order_book.hpp"

static uint64_t g_id = 0;
static uint64_t g_ts = 0;

static constexpr double MID             = 100.0;
static constexpr double TICK            = 0.01;
static constexpr int    MAX_DEPTH       = 100;
static constexpr int    REFILL_BATCH    = 256;

void SetupBook(OrderBook& book, std::vector<uint64_t>& out_ids, int levels, int orders_per_level) {
    
    if (levels > MAX_DEPTH)
        throw std::out_of_range("levels exceeds MAX_DEPTH; price would fall outside PriceLevelArray range");

    for (int i = 1; i <= levels; ++i) {
        for (int j = 0; j < orders_per_level; ++j) {
            uint64_t bid_id = ++g_id;
            uint64_t ask_id = ++g_id;
            book.add({ bid_id, Side::Buy,  MID - i * TICK, 100, ++g_ts });
            book.add({ ask_id, Side::Sell, MID + i * TICK, 100, ++g_ts });
            out_ids.push_back(bid_id);
            out_ids.push_back(ask_id);
        }
    }
}

std::vector<Order> GenerateOrders(int amount, int levels) {
    std::vector<Order> orders;
    orders.reserve(amount);

    for (int i = 0; i < amount; ++i) {
        int k = (i % levels) + 1;
        bool buy = (i & 1) == 0;
        double px = buy ? MID - k * TICK : MID + k * TICK;
        orders.push_back({ ++g_id, buy ? Side::Buy : Side::Sell, px, 100, ++g_ts });
    }
    return orders;
}

static void BM_AddOrder(benchmark::State& state) {
    int levels = state.range(0);
    int orders_per_level = state.range(1);

    OrderBook book;
    std::vector<uint64_t> base_ids;
    SetupBook(book, base_ids, levels, orders_per_level);

    std::vector<Order> incoming = GenerateOrders(REFILL_BATCH, levels);
    std::vector<uint64_t> added;
    added.reserve(REFILL_BATCH);

    size_t i = 0;
    for (auto _ : state) {
        if (i == incoming.size()) {
            state.PauseTiming();
            
            for (uint64_t id : added)
                book.cancel(id);

            added.clear();
            i = 0;
            state.ResumeTiming();
        }
        const Order& o = incoming[i];
        book.add(o);
        added.push_back(o.id);
        ++i;
    }
}

static void BM_CancelOrders(benchmark::State& state) {
    int levels = state.range(0);
    int orders_per_level = state.range(1);

    OrderBook book;
    std::vector<uint64_t> ids;
    SetupBook(book, ids, levels, orders_per_level);

    int done = 0;
    for (auto _ : state) {
        if (done == REFILL_BATCH || ids.empty()) {
            state.PauseTiming();
            book = OrderBook();
            ids.clear();
            SetupBook(book, ids, levels, orders_per_level);
            done = 0;
            state.ResumeTiming();
        }
        book.cancel(ids.back());
        ids.pop_back();
        ++done;
    }
}

static void BM_MatchOrders(benchmark::State& state) {
    int levels = state.range(0);
    int orders_per_level = state.range(1);

    OrderBook book;
    std::vector<uint64_t> ids;
    SetupBook(book, ids, levels, orders_per_level);

    int done = 0;
    for (auto _ : state) {
        if (done == REFILL_BATCH || book.best_ask() == 0.0) {
            state.PauseTiming();
            book = OrderBook();
            ids.clear();
            SetupBook(book, ids, levels, orders_per_level);
            done = 0;
            state.ResumeTiming();
        }

        Order o { ++g_id, Side::Buy, 101.55, 100, ++g_ts };
        book.match(o);
        ++done;
    }
}

static void BM_MixedWorkload(benchmark::State& state) {
    int levels = state.range(0);
    int orders_per_level = state.range(1);
    const size_t target = 2 * size_t(levels) * orders_per_level;

    OrderBook book;
    std::vector<uint64_t> ids;
    SetupBook(book, ids, levels, orders_per_level);

    std::vector<int> randoms(1000000);
    for (auto& r : randoms) r = rand() % 100; // segfaults if generated inside the timed loop

    size_t i = 0;
    for (auto _ : state) {
        if (ids.size() > target + REFILL_BATCH || ids.size() < 10) {
            state.PauseTiming();
            book = OrderBook();
            ids.clear();
            SetupBook(book, ids, levels, orders_per_level);
            i = 0;
            state.ResumeTiming();
        }

        int r = randoms[i % randoms.size()];
        if (r < 60) {
            int k = (i % levels) + 1;
            uint64_t new_id = ++g_id;
            book.add({ new_id, Side::Buy, MID - k * TICK, 100, ++g_ts });
            ids.push_back(new_id);
        } else if (r < 85) {
            if (!ids.empty()) {
                book.cancel(ids.back());
                ids.pop_back();
            }
        } else {
            if (book.best_ask() > 0.0) {
                Order o { ++g_id, Side::Buy, 101.55, 100, ++g_ts };
                book.match(o);
            }
        }
        ++i;
    }
}

BENCHMARK(BM_AddOrder)
        ->Args({25, 20})->Args({50, 20})->Args({100, 20})
        ->Args({50,  8})->Args({50, 40})
        ->Iterations(1000000);

BENCHMARK(BM_CancelOrders)
        ->Args({25, 20})->Args({50, 20})->Args({100, 20})
        ->Args({50,  8})->Args({50, 40})
        ->Iterations(1000000);

BENCHMARK(BM_MatchOrders)
        ->Args({25, 20})->Args({50, 20})->Args({100, 20})
        ->Args({50,  8})->Args({50, 40})
        ->Iterations(1000000);

BENCHMARK(BM_MixedWorkload)
        ->Args({25, 20})->Args({50, 20})->Args({100, 20})
        ->Args({50,  8})->Args({50, 40})
        ->Iterations(1000000);

BENCHMARK_MAIN();
