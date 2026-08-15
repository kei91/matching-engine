#include "latency_histogram.hpp"

#include <gtest/gtest.h>
#include <algorithm> // std::count - print() tests
#include <cstddef>
#include <cstdint>
#include <string>

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

TEST(LatencyHistogram, SaturatedFalse) {
    LatencyHistogram h(/*max_ns=*/1000, /*bucket_ns=*/1);
    h.record(500);

    EXPECT_FALSE(h.saturated(50.0)); 
}

TEST(LatencyHistogram, SaturatedTrue) {
    LatencyHistogram h(/*max_ns=*/1000, /*bucket_ns=*/1);
    h.record(5000);

    EXPECT_TRUE(h.saturated(100.0));    
}

TEST(LatencyHistogram, SaturatedMix) {
    LatencyHistogram h(/*max_ns=*/1000, /*bucket_ns=*/1);
    for (int i = 0; i < 999; ++i) h.record(500); 
    h.record(5000);                                // 0.1% in overflow

    EXPECT_FALSE(h.saturated(50.0));
    EXPECT_TRUE(h.saturated(100.0));
}

// --- print() -----------------------------------------------------------------
// print() exists to show the SHAPE of a distribution, which percentiles cannot:
// a bimodal distribution reads as an ordinary set of percentiles. These tests
// capture stdout and assert on the rendered chart.

namespace {

std::string render(const LatencyHistogram& h, const char* title = "t") {
    testing::internal::CaptureStdout();
    h.print(title);
    return testing::internal::GetCapturedStdout();
}

// Count how many rows actually carry a bar (i.e. had samples).
std::size_t bar_rows(const std::string& s) {
    std::size_t n = 0;
    for (std::size_t p = s.find('#'); p != std::string::npos; p = s.find('#', p)) {
        ++n;
        p = s.find('\n', p);
        if (p == std::string::npos) break;
    }
    return n;
}

} // namespace

// An empty histogram must not divide by zero or print a bogus chart.
TEST(LatencyHistogramPrint, EmptyIsSafe) {
    LatencyHistogram h(1000, 1);
    const std::string out = render(h);
    EXPECT_NE(out.find("(empty)"), std::string::npos);
    EXPECT_EQ(out.find('#'), std::string::npos); // no bars at all
}

// A single sample renders exactly one bar, scaled to full width.
TEST(LatencyHistogramPrint, SingleSample) {
    LatencyHistogram h(1000, 1);
    h.record(42);
    const std::string out = render(h);
    EXPECT_EQ(bar_rows(out), 1u);
    EXPECT_NE(out.find("100.000%"), std::string::npos);
}

// The title and the sample count both appear in the header.
TEST(LatencyHistogramPrint, HeaderCarriesTitleAndCount) {
    LatencyHistogram h(1000, 1);
    for (int i = 0; i < 7; ++i) h.record(100);
    const std::string out = render(h, "my title");
    EXPECT_NE(out.find("my title"), std::string::npos);
    EXPECT_NE(out.find("7 samples"), std::string::npos);
}

// A row with samples must never render as blank: a 0.001% tail is exactly what
// we are looking for, so it gets a minimum-width bar rather than rounding away.
TEST(LatencyHistogramPrint, ThinTailStillDrawn) {
    LatencyHistogram h(10'000'000, 1);
    for (int i = 0; i < 100000; ++i) h.record(200); // dominant hump
    h.record(500000);                               // one lone outlier, 0.001%
    const std::string out = render(h);
    EXPECT_GE(bar_rows(out), 2u);                   // hump + outlier both drawn
}

// Long empty stretches collapse, so two humps decades apart stay on one screen.
TEST(LatencyHistogramPrint, EmptyRunsCollapse) {
    LatencyHistogram h(10'000'000, 1);
    for (int i = 0; i < 1000; ++i) h.record(200);      // hump 1
    for (int i = 0; i < 1000; ++i) h.record(100000);   // hump 2, three decades up
    const std::string out = render(h);
    EXPECT_NE(out.find("empty rows"), std::string::npos);
    // The chart stays compact despite spanning ns..100us.
    EXPECT_LT(std::count(out.begin(), out.end(), '\n'), 20u);
}

// The leading empty decades (below the smallest sample) carry no information
// and must not be printed as a gap marker.
TEST(LatencyHistogramPrint, NoLeadingGap) {
    LatencyHistogram h(10'000'000, 1);
    for (int i = 0; i < 100; ++i) h.record(50000); // nothing below 50us
    const std::string out = render(h);
    const std::size_t first_nl = out.find('\n');
    const std::string second_line = out.substr(first_nl + 1, out.find('\n', first_nl + 1) - first_nl);
    EXPECT_EQ(second_line.find("empty rows"), std::string::npos);
}

// Overflow samples get their own labelled row.
TEST(LatencyHistogramPrint, OverflowRowIsLabelled) {
    LatencyHistogram h(1000, 1);
    for (int i = 0; i < 10; ++i) h.record(500);
    h.record(999999); // overflow
    const std::string out = render(h);
    EXPECT_NE(out.find("(overflow)"), std::string::npos);
}

// The real motivation: a bimodal distribution is invisible in the percentiles
// but obvious in the chart.
TEST(LatencyHistogramPrint, BimodalIsVisible) {
    LatencyHistogram h(10'000'000, 1);
    for (int i = 0; i < 6000; ++i) h.record(200);      // 60% fast
    for (int i = 0; i < 4000; ++i) h.record(120000);   // 40% slow
    const std::string out = render(h);

    // Two separated bars, each carrying a large share of the samples.
    EXPECT_NE(out.find("empty rows"), std::string::npos); // a real gap between them
    EXPECT_NE(out.find("60.000%"), std::string::npos);
    EXPECT_NE(out.find("40.000%"), std::string::npos);

    // ...while p50 lands in the fast hump and reports nothing about the slow one.
    EXPECT_LT(h.percentile(50.0), 1000u);
}

// Regression: samples sitting exactly at max() must be drawn. The row walk used
// to stop at `lo <= m_max`, which ended one row early and silently dropped the
// whole top hump when every sample there shared the same value.
TEST(LatencyHistogramPrint, MaxValueRowIsDrawn) {
    LatencyHistogram h(10'000'000, 1);
    for (int i = 0; i < 1000; ++i) h.record(200);      // hump 1
    for (int i = 0; i < 1000; ++i) h.record(100000);   // hump 2, all at the exact max
    const std::string out = render(h);

    EXPECT_EQ(bar_rows(out), 2u);                      // both humps drawn
    EXPECT_NE(out.find("100.0us"), std::string::npos); // the top row is labelled
    // Each hump is half the samples; neither may be missing or double-counted.
    EXPECT_EQ(std::count(out.begin(), out.end(), '#') > 0, true);
    EXPECT_NE(out.find("50.000%"), std::string::npos);
}
