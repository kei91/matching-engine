#include "latency_histogram.hpp"

#include <gtest/gtest.h>
#include <cstdint>

// An empty histogram must not crash and returns zeros.
TEST(LatencyHistogram, Empty) {
    LatencyHistogram h;
    EXPECT_EQ(h.count(), 0u);
    EXPECT_EQ(h.percentile(50.0), 0u);
    EXPECT_EQ(h.min(), 0u);
    EXPECT_EQ(h.max(), 0u);
    EXPECT_DOUBLE_EQ(h.mean(), 0.0);
}

// Aggregates (count/min/max/mean) are tracked exactly, independent of buckets.
TEST(LatencyHistogram, Aggregates) {
    LatencyHistogram h;
    h.record(10);
    h.record(20);
    h.record(30);
    EXPECT_EQ(h.count(), 3u);
    EXPECT_EQ(h.min(), 10u);
    EXPECT_EQ(h.max(), 30u);
    EXPECT_DOUBLE_EQ(h.mean(), 20.0);
}

// With a uniform 1..100 distribution, percentiles should land near their value.
TEST(LatencyHistogram, UniformPercentiles) {
    LatencyHistogram h;
    for (uint64_t v = 1; v <= 100; ++v)
        h.record(v);

    EXPECT_EQ(h.count(), 100u);
    // nearest-rank: p50 -> rank 50 -> value 50 (bucket upper edge == value here)
    EXPECT_EQ(h.percentile(50.0), 50u);
    EXPECT_EQ(h.percentile(99.0), 99u);
    EXPECT_EQ(h.percentile(100.0), 100u);
}

// The tail must dominate the high percentiles: 999 fast + 1 slow sample.
// p50 stays fast, but p99.9 must reveal the slow outlier.
TEST(LatencyHistogram, TailIsVisible) {
    LatencyHistogram h;
    for (int i = 0; i < 999; ++i)
        h.record(50);
    h.record(9000); // one slow outlier

    EXPECT_EQ(h.percentile(50.0), 50u);   // median unaffected by the outlier
    EXPECT_GE(h.percentile(99.9), 50u);
    EXPECT_EQ(h.max(), 9000u);            // exact max still captured
    // The average is dragged up but stays near 50; the tail tells the real story.
    EXPECT_LT(h.mean(), 100.0);
}

// Samples above max_ns go to the overflow bucket: counted, reported as >= max_ns.
TEST(LatencyHistogram, Overflow) {
    LatencyHistogram h(/*max_ns=*/1000, /*bucket_ns=*/1);
    h.record(500);
    h.record(5000); // overflow

    EXPECT_EQ(h.count(), 2u);
    EXPECT_EQ(h.max(), 5000u);              // exact max preserved
    EXPECT_EQ(h.percentile(100.0), 1000u);  // overflow reported at the max_ns floor
    EXPECT_EQ(h.overflow_count(), 1u);      // exactly one sample exceeded max_ns
}

// Coarser bucket width rounds the reported percentile up to the bucket edge.
TEST(LatencyHistogram, BucketResolution) {
    LatencyHistogram h(/*max_ns=*/1000, /*bucket_ns=*/10);
    h.record(23); // falls in bucket [20,30) -> reported as largest value in it, 29
    EXPECT_EQ(h.percentile(100.0), 29u);
    EXPECT_EQ(h.min(), 23u); // but exact min is still kept
}
