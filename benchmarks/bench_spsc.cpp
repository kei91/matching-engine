#include "spsc_queue.hpp"

#include <benchmark/benchmark.h>
#include <thread>
#include <atomic>
#include <cstdint>

static void BM_SpscThroughput(benchmark::State& state) {
    SPSCQueue<uint64_t, 1024> q;
    std::atomic<bool> stop{false};

    std::thread producer([&] {
        uint64_t v = 0;
        while (!stop.load(std::memory_order_relaxed)) {
            while (!q.push(v)) {
                if (stop.load(std::memory_order_relaxed)) return;
            }
            ++v;
        }
    });

    // consumer thread is a bench thread itself
    uint64_t out = 0;
    for (auto _ : state) {
        while (!q.pop(out)) { /* busy-wait for producer */ }
        benchmark::DoNotOptimize(out);
    }

    stop.store(true, std::memory_order_relaxed);
    producer.join();

    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_SpscThroughput);

BENCHMARK_MAIN();
