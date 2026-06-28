// Latency benchmark for OrderBook::add, using the rdtsc timer + LatencyHistogram.
//
// Google Benchmark reports average throughput over a batch, so this bench
// measures EACH individual operation with rdtsc and feeds it into a histogram,
// so we can get - p50 / p99 / p99.9, not just the mean.
//
// First test for add() only for simplicity.

#include "order_book.hpp"
#include "rdtsc.hpp"
#include "latency_histogram.hpp"

#include <cstdint>
#include <cstdio>

namespace {

constexpr double   MID         = 100.0;
constexpr double   TICK        = 0.01;
constexpr int      LEVELS      = 20;
constexpr uint64_t WARMUP      = 100000;
constexpr uint64_t MEASURED    = 2000000;

Order make_order(uint64_t i) {
    const bool buy = (i & 1) == 0;
    const int  k = static_cast<int>(i % LEVELS) + 1;
    const double px = buy ? MID - k * TICK : MID + k * TICK;
    return Order{ i + 1,
                  buy ? Side::Buy : Side::Sell,
                  px,
                  100,
                  i };
}

} // namespace

int main() {
    // Calibrate cycles->ns once at startup.
    const double tpns = rdtsc::ticks_per_ns();
    std::printf("calibration: %.3f cycles/ns (~%.2f GHz)\n\n", tpns, tpns);

    OrderBook book;
    LatencyHistogram hist( 10000, 1);

    uint64_t id = 0;

    // Warm caches and branch predictors.
    for (uint64_t i = 0; i < WARMUP; ++i)
        book.add(make_order(id++));

    for (uint64_t i = 0; i < MEASURED; ++i) {
        const Order o = make_order(id++);

        const uint64_t t0 = rdtsc::now();
        book.add(o);
        const uint64_t t1 = rdtsc::now();

        hist.record(static_cast<uint64_t>(rdtsc::to_ns(t1 - t0, tpns)));
    }

    std::printf("OrderBook::add latency over %lu ops (ns):\n", static_cast<unsigned long>(hist.count()));
    std::printf("  min    %lu\n",   static_cast<unsigned long>(hist.min()));
    std::printf("  p50    %lu\n",   static_cast<unsigned long>(hist.percentile(50.0)));
    std::printf("  p99    %lu\n",   static_cast<unsigned long>(hist.percentile(99.0)));
    std::printf("  p99.9  %lu\n",   static_cast<unsigned long>(hist.percentile(99.9)));
    std::printf("  max    %lu\n",   static_cast<unsigned long>(hist.max()));
    std::printf("  mean   %.1f\n",  hist.mean());
    
    const uint64_t outliers = hist.overflow_count();
    std::printf("  outliers (>10us)  %lu  (%.4f%% of ops)\n",
                static_cast<unsigned long>(outliers),
                100.0 * static_cast<double>(outliers) / static_cast<double>(hist.count()));

    return 0;
}
