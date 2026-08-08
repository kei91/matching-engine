// End-to-end latency benchmark: producer thread -> SPSC queue -> matching thread.
//
// This captures the WHOLE path:
//   push + time spent waiting in the queue + pop + match.
//
// HOW THE TIMESTAMP CROSSES THREADS:
//   The producer stamps order.timestamp = rdtsc::now() just before pushing.
//   The matching thread, after match(), reads rdtsc::now() again and the delta
//   is the end-to-end latency. (Order.timestamp is unused by the matching logic
//   - FIFO priority comes from std::list insertion order - so we reuse it.)
//
//   IMPORTANT: the start stamp is taken on one core and the end stamp on another.
//   Subtracting them is only valid because on a modern single-socket CPU the TSC
//   is SYNCHRONIZED across cores (invariant TSC + the kernel keeps them in step;
//   see the `tsc_adjust` flag in `lscpu`). On multi-socket / old hardware this
//   would be wrong.
//
// Run pinned to two SEPARATE physical cores, e.g. `taskset -c 0,2` (see
// `dev.sh bench-pipeline`), so producer and consumer don't share a core.

#include "order_book.hpp"
#include "spsc_queue.hpp"
#include "rdtsc.hpp"
#include "latency_histogram.hpp"

#include <cstdint>
#include <cstdio>
#include <thread>
#include <x86intrin.h> // _mm_pause in the spin loop

namespace {

constexpr double   MID      = 100.0;
constexpr uint64_t WARMUP   = 100000;
constexpr uint64_t MEASURED = 1000000;
constexpr uint64_t TOTAL    = WARMUP + MEASURED;

// Utilisation levels to sweep, as a fraction of measured capacity. The knee of
// the curve sits close to 100%, so the grid is deliberately dense near the top.
constexpr double LOADS[]  = { 0.10, 0.30, 0.50, 0.70, 0.85, 0.95 };

// End-to-end latencies span ns..ms once the queue backs up, so the histogram
// ceiling is 10 ms here (bench_latency uses 10 us for a single add()).
constexpr uint64_t HIST_MAX_NS   = 10000000;
constexpr uint64_t HIST_BUCKET_NS = 1;

// Alternating buy/sell at the same price: each order crosses the resting one on
// the opposite side, so the match path runs for real and the book stays small.
// This keeps the measurement in steady state - no unbounded std::list growth,
// which was the caveat on the bench_latency numbers.
Order make_order(uint64_t i) {
    return Order{ /*id=*/i + 1,
                  (i & 1) == 0 ? Side::Buy : Side::Sell,
                  MID,
                  /*quantity=*/100,
                  /*timestamp=*/0 }; // stamped at push time below
}

// Busy-wait until the TSC reaches `deadline`. We spin rather than sleep because
// sleeping costs tens of microseconds and would dominate the interval we're
// trying to space orders by.
inline void spin_until(uint64_t deadline) {
    while (rdtsc::read_raw() < deadline)
        _mm_pause(); // hint to the CPU that this is a spin loop
}

// --- step 1: what is the consumer's real service time? -----------------------
// Saturate the queue and time the consumer's WHOLE loop, then divide by the
// order count. It must be the whole loop, not just book.match():
//
//   match()          ~ 91 ns   the actual work
//   pop()            ~ 36 ns   acquire load + release store = inter-core traffic
//   2x rdtsc::now()  ~ 27 ns   the measurement instrumentation itself
//                    -------
//                    ~154 ns   <- this is what caps the pipeline
//
// Timing match() alone reports ~91 ns and overstates capacity by ~70%, which
// makes the load sweep below ask for arrival rates the consumer cannot serve -
// every level above ~60% would silently be a saturation run, and they all
// report the same "queue is full" latency. Capacity has to be measured at the
// slowest stage of the pipeline, instrumentation included.
double measure_service_ns(double tpns) {
    SPSCQueue<Order, 1024> queue;

    std::thread producer([&] {
        for (uint64_t i = 0; i < TOTAL; ++i) {
            Order o = make_order(i);
            o.timestamp = rdtsc::now();          // same work as the real run
            while (!queue.push(o)) { /* spin: queue full */ }
        }
    });

    uint64_t begin = 0, end = 0;
    uint64_t checksum = 0;    // print checksum to be sure compliler don´t skip / optimize this part

    std::thread consumer([&] {
        OrderBook book;
        Order o;
        for (uint64_t i = 0; i < WARMUP; ++i) {   // warm up, untimed
            while (!queue.pop(o)) {}
            book.match(o);
            checksum += rdtsc::now() - o.timestamp;
        }
        begin = rdtsc::read_raw();
        for (uint64_t i = WARMUP; i < TOTAL; ++i) {
            while (!queue.pop(o)) { /* spin: queue empty */ }
            book.match(o);
            checksum += rdtsc::now() - o.timestamp;
        }
        end = rdtsc::read_raw();
    });

    producer.join();
    consumer.join();

    const double service_ns = rdtsc::to_ns(end - begin, tpns) / static_cast<double>(MEASURED);
    std::printf("consumer service time (saturated, whole loop): %.1f ns/order\n", service_ns);
    std::printf("=> capacity ~ %.2f M orders/s  [checksum %lu]\n\n",
                1000.0 / service_ns, static_cast<unsigned long>(checksum));
    return service_ns;
}

// --- step 2: end-to-end latency at a fixed offered load ----------------------
// `interval_ticks` is the TSC spacing between consecutive pushes; it sets the
// arrival rate. Everything else matches the saturated version.
LatencyHistogram run_at_load(double tpns, double load, double service_ns) {
    // Target arrival rate = load x capacity, so the gap between orders is service_ns / load.
    const double   interval_ns    = service_ns / load;
    const uint64_t interval_ticks = static_cast<uint64_t>(interval_ns * tpns);

    SPSCQueue<Order, 1024> queue;

    // Sanity check on the pacer: if the producer cannot keep up with the
    // schedule it is silently running flat out, and the run is a saturation run
    // mislabelled as a load level. We compare the achieved rate against target.
    uint64_t prod_begin = 0, prod_end = 0;

    std::thread producer([&] {
        prod_begin = rdtsc::read_raw();
        uint64_t next = prod_begin;
        for (uint64_t i = 0; i < TOTAL; ++i) {
            // Pace the arrivals: don't emit order i before its scheduled slot.
            next += interval_ticks;
            spin_until(next);

            Order o = make_order(i);
            o.timestamp = rdtsc::now();          // ingress stamp
            while (!queue.push(o)) { /* spin: consumer is behind */ }
        }
        prod_end = rdtsc::read_raw();
    });

    LatencyHistogram hist(HIST_MAX_NS, HIST_BUCKET_NS);
    std::thread consumer([&] {
        OrderBook book;                           // lives entirely in this thread
        Order o;
        for (uint64_t i = 0; i < TOTAL; ++i) {
            while (!queue.pop(o)) { /* spin: queue empty, producer will fill */ }

            book.match(o);                        // process through the order book

            const uint64_t done = rdtsc::now();
            if (i >= WARMUP)
                hist.record(static_cast<uint64_t>(rdtsc::to_ns(done - o.timestamp, tpns)));
        }
    });

    producer.join();
    consumer.join();

    // Achieved arrival rate: TOTAL orders over the producer's wall time.
    const double achieved_mps =
        static_cast<double>(TOTAL) / rdtsc::to_ns(prod_end - prod_begin, tpns) * 1000.0;

    std::printf("%4.0f%% | %6.2f | %6.2f | %6lu | %7lu | %7lu | %8lu | %9.1f\n",
                load * 100.0,
                1000.0 / interval_ns,                                 // target M orders/s
                achieved_mps,                                         // actually delivered
                static_cast<unsigned long>(hist.percentile(50.0)),
                static_cast<unsigned long>(hist.percentile(99.0)),
                static_cast<unsigned long>(hist.percentile(99.9)),
                static_cast<unsigned long>(hist.max()),
                hist.mean());
}

} // namespace

int main() {
    const double tpns = rdtsc::ticks_per_ns();
    std::printf("calibration: %.3f cycles/ns (~%.2f GHz)\n\n", tpns, tpns);

    const double service_ns = measure_service_ns(tpns);

    std::printf("push->matched latency vs offered load (%lu measured orders each, ns):\n",
                static_cast<unsigned long>(MEASURED));
    std::printf("     |   M ord/s     |                latency (ns)               \n");
    std::printf("load | target|  got  |    p50 |     p99 |   p99.9 |      max |      mean\n");
    std::printf("-----+-------+-------+--------+---------+---------+----------+----------\n");

    for (double load : LOADS)
        run_at_load(tpns, load, service_ns);

    return 0;
}
