#pragma once

// =============================================================================
// latency_histogram.hpp - collect many latency samples and report percentiles
// (p50 / p99 / p99.9), not just an average.
//
// HOW IT WORKS (fixed-width linear buckets):
//   Split the latency range [0, max_ns] into equal buckets `bucket_ns` wide.
//   record(ns) just increments the counter for the bucket that ns falls into.
//
//   percentile(p) walks the buckets from low to high, summing counts, until it
//   has covered p% of all samples; the bucket where that happens is the answer.
//
// WHY LINEAR BUCKETS (and the alternative):
//   Linear fixed-width buckets give EXACT percentiles (to within bucket_ns) and
//   are trivial to understand. The downside is range: covering ns..seconds at
//   1 ns resolution would need billions of buckets.
//   The classic alternative is an HDR histogram (logarithmically sized buckets):
//   it covers a huge dynamic range with few buckets, at the cost of approximate
//   values inside each bucket and more complex code. If we ever need ns..seconds
//   we'd switch to that.
// =============================================================================

#include <cstdint>
#include <vector>
#include <limits>
#include <cstddef>

class LatencyHistogram {
public:
    // max_ns - largest latency we bucket precisely; anything above goes to the overflow bucket.
    // bucket_ns - width of one bucket in nanoseconds (resolution).
    explicit LatencyHistogram(uint64_t max_ns = 100'000, uint64_t bucket_ns = 1)
        : m_bucket_ns(bucket_ns),
          m_max_ns(max_ns),
          // +1 extra slot at the end is the overflow bucket.
          m_buckets(static_cast<std::size_t>(max_ns / bucket_ns) + 1, 0) {}

    // Add one latency sample (in nanoseconds).
    void record(uint64_t ns) {
        // Track exact aggregates separately so min/max/mean don't suffer from bucket rounding.
        ++m_count;
        m_sum += ns;
        if (ns < m_min) m_min = ns;
        if (ns > m_max) m_max = ns;

        // Map ns -> bucket index. Samples >= max_ns land in the last (overflow) bucket
        std::size_t idx = (ns >= m_max_ns)
                              ? m_buckets.size() - 1
                              : static_cast<std::size_t>(ns / m_bucket_ns);
        ++m_buckets[idx];
    }

    // Return the p-th percentile in nanoseconds. p is in [0, 100]
    uint64_t percentile(double p) const {
        if (m_count == 0) return 0;

        // Which ranked sample are we looking for (1-based). clamp to [1, count].
        uint64_t rank = static_cast<uint64_t>(p / 100.0 * static_cast<double>(m_count));
        if (rank < 1) rank = 1;
        if (rank > m_count) rank = m_count;

        uint64_t cumulative = 0;
        for (std::size_t i = 0; i < m_buckets.size(); ++i) {
            cumulative += m_buckets[i];
            if (cumulative >= rank) {
                // Overflow bucket
                if (i == m_buckets.size() - 1)
                    return m_max_ns;
                // Report the bucket's representative value
                return static_cast<uint64_t>((i + 1) * m_bucket_ns - 1);
            }
        }
        return m_max_ns; // unreachable when count > 0, but keeps the compiler happy
    }

    uint64_t count() const { return m_count; }
    uint64_t min()   const { return m_count ? m_min : 0; }
    uint64_t max()   const { return m_count ? m_max : 0; }
    double   mean()  const {
        return m_count ? static_cast<double>(m_sum) / static_cast<double>(m_count) : 0.0;
    }

private:
    uint64_t m_bucket_ns;
    uint64_t m_max_ns;
    std::vector<uint64_t> m_buckets;

    uint64_t m_count = 0;
    uint64_t m_sum   = 0;
    uint64_t m_min   = std::numeric_limits<uint64_t>::max();
    uint64_t m_max   = 0;
};
