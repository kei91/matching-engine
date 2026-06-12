#pragma once

// =============================================================================
// rdtsc.hpp - reading the Time Stamp Counter (TSC) to measure latency.
//
// WHY THIS FILE EXISTS:
//   To measure how many nanoseconds a SINGLE operation takes (e.g. pushing
//   an order into the queue -> matching it). Ordinary clocks (std::chrono) 
//   cost ~20-50 ns per call by themselves. The CPU instruction `rdtsc` 
//   reads the processor's internal cycle counter almost for free.
//
// WHAT THE TSC IS:
//   The Time Stamp Counter is a 64-bit counter INSIDE the CPU that increments
//   on every cycle since power-on. `rdtsc` ("read TSC") returns its current
//   value. The unit is CYCLES (ticks), NOT nanoseconds. Converting cycles to
//   ns is done by the calibration function below.
//
// IMPORTANT:
//   On older CPUs the TSC ticked at the core's real (variable) frequency - when
//   the CPU lowered its frequency to save power the counter slowed down too, so
//   it could not be used as a clock. On modern CPUs (flags `constant_tsc` and
//   `nonstop_tsc` in `lscpu`) the TSC runs at a CONSTANT rate regardless of the
//   core's actual frequency. Check if the TSC is usable as a clock with:
//       lscpu | grep -o 'constant_tsc\|nonstop_tsc'
// =============================================================================

#include <cstdint>
#include <chrono>      // for calibration cycles -> nanoseconds (via steady_clock)
#include <x86intrin.h> // ready-made intrinsics __rdtsc / __rdtscp / _mm_lfence

namespace rdtsc {

// -----------------------------------------------------------------------------
// NOTE:
//
//   1) The COMPILER may move/drop instructions during optimization.
//      Use `volatile` in inline assembly and compiler barriers.
//
//   2) The CPU executes instructions out-of-order (not in program order) so it
//      doesn't stall. It may run part of the measured code BEFORE the first
//      rdtsc, or read the second rdtsc EARLIER than the code actually finished.
//      Use the barrier instruction `lfence` ("load fence"): it prevents
//      instructions from crossing over it.
//
// Hence the canonical measurement looks like:
//      lfence; t0 = rdtsc; lfence;  <measured code>;  lfence; t1 = rdtsc; lfence;
// -----------------------------------------------------------------------------


// -----------------------------------------------------------------------------
// VARIANT 1 - via inline assembly.
// To know what is happening under the hood.
// Use the intrinsic (variant 2) - as it portable across compilers.
//
//   __asm__         - "an assembly block follows".
//   __volatile__    - prevent compiler instructions reorder
//   "rdtsc"         - the CPU instruction itself.
//   : "=a"(lo)      - the OUTPUT section. `=` = write-only. `a` = register EAX.
//                     Means: "after rdtsc, put EAX into lo".
//     "=d"(hi)      - `d` = register EDX. "put EDX into hi".
//
// NOTE: rdtsc puts its 64-bit result into TWO 32-bit registers in hardware - 
// the high 32 bits in EDX, the low 32 bits in EAX. So we need to reassemble
// them back into a single 64-bit value.
// -----------------------------------------------------------------------------
inline uint64_t read_raw_asm() {
    uint32_t lo, hi;
    __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
    return (static_cast<uint64_t>(hi) << 32) | lo;
    //      ^^^ high bits (EDX)               ^^^ low bits (EAX)
}

// -----------------------------------------------------------------------------
// VARIANT 2 - via an intrinsic (declared in <x86intrin.h>). 
// This is the "raw" variant WITHOUT barriers - when you just need a
// timestamp and accuracy to within tens of cycles doesn't matter.
// -----------------------------------------------------------------------------
inline uint64_t read_raw() {
    return __rdtsc();
}

// -----------------------------------------------------------------------------
// PRECISE timestamp for measuring short code sections.
//
// Here we add the barrier `_mm_lfence()` - prevents CPU to reorder instructions.
// The placement means:
//   _mm_lfence();    // wait until all code BEFORE this point has truly finished
//   t = __rdtsc();   // only now read the counter
//   _mm_lfence();    // don't let code AFTER start executing too early
//
// Note: we could use `__rdtscp()` - a variant of rdtsc that partially
// serializes by itself (waits for preceding instructions to finish). But rdtscp
// does NOT prevent SUBSEQUENT instructions from starting early, so an lfence
// after it is still needed. For simplicity we use lfence+rdtsc+lfence.
// -----------------------------------------------------------------------------
inline uint64_t now() {
    _mm_lfence();
    uint64_t t = __rdtsc();
    _mm_lfence();
    return t;
}

// -----------------------------------------------------------------------------
// CALIBRATION: how many TSC cycles fall into one nanosecond.
//
// rdtsc returns CYCLES, but for reports we need NANOSECONDS. The relation:
//      nanoseconds = cycles / (cycles_per_nanosecond)
//
// How to measure the TSC frequency: sample the TSC and an ordinary clock
// (steady_clock) at the start, wait a fixed interval, sample again - and divide
// the cycle delta by the nanosecond delta.
//
//   tsc cycles over the interval
//   ---------------------------- = cycles per nanosecond
//   ns          over the interval
//
// Call this ONCE at program startup (it blocks for ~`sample_ms` milliseconds),
// store the result and reuse it.
// -----------------------------------------------------------------------------
inline double ticks_per_ns(int sample_ms = 100) {
    using clock = std::chrono::steady_clock;

    const uint64_t tsc0  = now();
    const auto     wall0 = clock::now();

    // Actively wait the requested interval of real time.
    const auto target = wall0 + std::chrono::milliseconds(sample_ms);
    while (clock::now() < target) {
        // busy-wait: just spin so the thread doesn't sleep and the TSC runs evenly.
    }

    const uint64_t tsc1  = now();
    const auto     wall1 = clock::now();

    const uint64_t ticks = tsc1 - tsc0;
    const auto     ns    = std::chrono::duration_cast<std::chrono::nanoseconds>(
                               wall1 - wall0).count();

    return static_cast<double>(ticks) / static_cast<double>(ns);
}

// -----------------------------------------------------------------------------
// Converts a measured interval in cycles to nanoseconds.
// `tpns` is the value obtained once from ticks_per_ns().
//
// Example usage:
//      const double TPNS = rdtsc::ticks_per_ns();   // once at startup
//      ...
//      uint64_t t0 = rdtsc::now();
//      do_work();
//      uint64_t t1 = rdtsc::now();
//      double ns = rdtsc::to_ns(t1 - t0, TPNS);      // latency in ns
// -----------------------------------------------------------------------------
inline double to_ns(uint64_t ticks, double tpns) {
    return static_cast<double>(ticks) / tpns;
}

} // namespace rdtsc
