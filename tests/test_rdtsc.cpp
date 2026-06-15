#include "rdtsc.hpp"

#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include <cstdint>

// now() must never go backwards
TEST(Rdtsc, NowIsMonotonic) {
    uint64_t prev = rdtsc::now();
    for (int i = 0; i < 1000; ++i) {
        uint64_t cur = rdtsc::now();
        ASSERT_GE(cur, prev);
        prev = cur;
    }
}

// The hand-written asm and the intrinsic rshoud be very closed
TEST(Rdtsc, AsmAndIntrinsicAgree) {
    uint64_t a = rdtsc::read_raw_asm();
    uint64_t b = rdtsc::read_raw();
    EXPECT_GE(b, a);
    // They are consecutive reads of the same counter; the gap is tiny.
    EXPECT_LT(b - a, 100000u);
}

// Calibration must return a positive value. CPUs run roughly 0.5–10 GHz, i.e. 0.5–10 cycles per nanosecond.
TEST(Rdtsc, TicksPerNsIsPlausible) {
    double tpns = rdtsc::ticks_per_ns(/*sample_ms=*/20);
    EXPECT_GT(tpns, 0.5);
    EXPECT_LT(tpns, 10.0);
}

TEST(Rdtsc, ToNsConversion) {
    EXPECT_DOUBLE_EQ(rdtsc::to_ns(0, 2.0), 0.0);
    EXPECT_DOUBLE_EQ(rdtsc::to_ns(200, 2.0), 100.0); // 200 cycles / 2 per ns = 100 ns
}

// Real sleep with the TSC
TEST(Rdtsc, MeasuresKnownDuration) {
    const double tpns = rdtsc::ticks_per_ns(/*sample_ms=*/20);

    uint64_t t0 = rdtsc::now();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    uint64_t t1 = rdtsc::now();

    double measured_ns = rdtsc::to_ns(t1 - t0, tpns);
    // We asked for 10 ms = 10'000'000 ns; allow a wide window for jitter.
    EXPECT_GT(measured_ns, 5000000.0);
    EXPECT_LT(measured_ns, 50000000.0);
}
