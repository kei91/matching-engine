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
#include <cmath>    // std::pow  - log-spaced rows in print()
#include <cstdio>   // std::printf / std::snprintf - print()
#include <string>   // std::string - building the bars in print()

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

    // How many samples landed in the overflow bucket, i.e. were >= max_ns.
    // Useful to quantify the tail: "N outliers out of count".
    uint64_t overflow_count() const {
        return m_buckets.empty() ? 0 : m_buckets.back();
    }
    uint64_t min()   const { return m_count ? m_min : 0; }
    uint64_t max()   const { return m_count ? m_max : 0; }
    double   mean()  const {
        return m_count ? static_cast<double>(m_sum) / static_cast<double>(m_count) : 0.0;
    }

    // -------------------------------------------------------------------------
    // Print the distribution as an ASCII bar chart.
    // The recording buckets are linear (1 ns), but latency spans ns..ms, so the
    // rows here are LOGARITHMIC (each row covers 10^(k/rows_per_decade)).
    // -------------------------------------------------------------------------
    void print(const char* title = "latency distribution",
               int rows_per_decade = 5,
               int width = 48) const {
        std::printf("%s (%llu samples)\n", title,
                    static_cast<unsigned long long>(m_count));
        if (m_count == 0) {
            std::printf("  (empty)\n");
            return;
        }

        // Collect log-spaced rows: [lo, hi) in ns, plus the sample count in it.
        struct Row { uint64_t lo, hi, n; };
        std::vector<Row> rows;
        uint64_t peak = 0;

        // Walk rows until the one CONTAINING m_max has been emitted - `lo <= m_max`
        // would stop one row early and drop every sample equal to the maximum.
        const double step = std::pow(10.0, 1.0 / rows_per_decade);
        const std::size_t last_linear = m_buckets.size() - 1;   // excl. overflow slot
        double lo = 1.0;
        while (static_cast<uint64_t>(lo) <= m_max) {
            const double hi = lo * step;
            const uint64_t lo_ns = static_cast<uint64_t>(lo);
            uint64_t hi_ns = static_cast<uint64_t>(hi);
            if (hi_ns <= lo_ns) { lo = hi; continue; }  // degenerate row at the low end

            // Sum the linear buckets falling inside [lo_ns, hi_ns).
            uint64_t n = 0;
            const std::size_t b0 = static_cast<std::size_t>(lo_ns / m_bucket_ns);
            std::size_t b1 = static_cast<std::size_t>(hi_ns / m_bucket_ns);
            if (b1 > last_linear) b1 = last_linear;
            for (std::size_t b = b0; b < b1; ++b)
                n += m_buckets[b];

            rows.push_back({lo_ns, hi_ns, n});
            if (n > peak) peak = n;
            lo = hi;
        }

        const uint64_t over = overflow_count();
        if (over > peak) peak = over;
        if (peak == 0) peak = 1;

        // Collapse runs of empty rows into a single "..." line. A bimodal
        // distribution puts its humps decades apart, and printing the silence
        // between them verbatim pushes them off one screen - which defeats the
        // point of drawing the shape at all.
        std::size_t gap = 0;
        bool started = false;                      // suppress the leading gap: the
        for (const Row& r : rows) {                // empty decades below m_min are
            if (r.n == 0) { ++gap; continue; }     // not information

            if (gap > 0 && started) {
                if (gap <= 2) {
                    // A one- or two-row gap is narrower than its own "..." line.
                    std::printf("  %8s |%-*s|\n", "", width, "");
                    if (gap == 2) std::printf("  %8s |%-*s|\n", "", width, "");
                } else {
                    std::printf("  %8s     ... %llu empty rows ...\n",
                                "", static_cast<unsigned long long>(gap));
                }
            }
            gap = 0;
            started = true;

            const int bar = static_cast<int>(
                static_cast<double>(r.n) / static_cast<double>(peak) * width);
            std::printf("  %8s |%-*s| %7.3f%%  %llu\n",
                        fmt_ns(r.lo), width,
                        std::string(bar == 0 ? 1 : bar, '#').c_str(),
                        100.0 * static_cast<double>(r.n) / static_cast<double>(m_count),
                        static_cast<unsigned long long>(r.n));
        }
        if (over > 0) {
            const int bar = static_cast<int>(
                static_cast<double>(over) / static_cast<double>(peak) * width);
            std::printf("  %7s+ |%-*s| %7.3f%%  %llu  (overflow)\n",
                        fmt_ns(m_max_ns), width,
                        std::string(bar == 0 ? 1 : bar, '#').c_str(),
                        100.0 * static_cast<double>(over) / static_cast<double>(m_count),
                        static_cast<unsigned long long>(over));
        }
    }

private:
    // Render a ns value compactly (12345 -> "12.3us") into a rotating buffer, so
    // several calls can appear in one printf argument list.
    static const char* fmt_ns(uint64_t ns) {
        static thread_local char bufs[4][16];
        static thread_local int  idx = 0;
        char* b = bufs[idx++ & 3];
        if (ns < 1000)             std::snprintf(b, 16, "%lluns", static_cast<unsigned long long>(ns));
        else if (ns < 1000000)     std::snprintf(b, 16, "%.1fus", static_cast<double>(ns) / 1e3);
        else if (ns < 1000000000)  std::snprintf(b, 16, "%.1fms", static_cast<double>(ns) / 1e6);
        else                       std::snprintf(b, 16, "%.2fs",  static_cast<double>(ns) / 1e9);
        return b;
    }


    uint64_t m_bucket_ns;
    uint64_t m_max_ns;
    std::vector<uint64_t> m_buckets;

    uint64_t m_count = 0;
    uint64_t m_sum   = 0;
    uint64_t m_min   = std::numeric_limits<uint64_t>::max();
    uint64_t m_max   = 0;
};
