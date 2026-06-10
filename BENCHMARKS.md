## Benchmarks

### Setup
- CPU: Intel Core i5, 4 cores, 8 threads, 4GHz
- L1/L2/L3: 32KB / 256KB / 8MB
- Compiler: GCC 13, -O3 -march=native
- Benchmark: Google Benchmark v1.9.5

### Current Results (std::map + std::list)
| Benchmark      | Time   |
|----------------|--------|
| Add            | 99 ns  |
| Cancel         | 62 ns  |
| Match          | 149 ns |
| MixedWorkload  | 84 ns  |

### Known tradeoffs
- `std::list` - O(1) on cancel but has worse cache locality on match compared to deque
- `std::map` - O(log n) on add - maybe change to flat structure

## Optimizations

### ✅ cancel(): O(n) → O(1)

#### Problem: 
`cancel()` performed a linear scan of `std::deque` inside price level to find order by id. With a large number of orders at a single price level,
this degraded to O(n). Identified through benchmarks: `BM_MixedWorkload` showed 139507 ns versus ~100 ns for isolated operations.

#### Solution:
`m_order_price_index` now stores the iterator directly, `std::deque` has been replaced with `std::list`.

**Results:**
| Benchmark     | Before    | After  |
|---------------|-----------|--------|
| Cancel        | 60 ns     | 62 ns  |
| MixedWorkload | 139507 ns | 84 ns  |
| Add           | 71 ns     | 99 ns  |
| Match         | 111 ns    | 149 ns |

**Tradeoff:** `std::list` has worse cache locality during iteration - Add and Match became slower by ~35%.

### 🔄 std::map → PriceLevelArray + std::pmr

#### Changes:
Replaced `std::map<double, PriceLevel>` with a flat array indexed by price ticks.
Added `std::pmr::unsynchronized_pool_resource` for `std::list` node allocation.

**Results:**
| Benchmark     | Before | After  |
|---------------|--------|--------|
| Cancel        | 62 ns  | 69 ns  |
| MixedWorkload | 84 ns  | 91 ns  |
| Add           | 99 ns  | 90 ns  |
| Match         | 149 ns | 148 ns |


**Conclusion:**
No significant improvement over the original `std::map` baseline. `Add` improved slightly probably due to pool allocation, but `Cancel`/`MixedWorkload` regressed - maybe because `unsynchronized_pool_resource` deallocate overhead outweighs the benefit at this order book depth (20 levels, few orders per level).

Current benchmarks may not reflect realistic load. Next step: parameterize benchmarks by order book depth and orders per level to find the threshold where these optimizations pay off.

### ↩️ Back to std::map + std::list

#### Changes:
Added benchmarks' arguments `levels` and `orders_per_level`, that way:
- the book is kept at a fixed depth during measurement instead of growing from empty.
- restore happens on a fixed cadence, so it doesn't depend on the workload size.

**Results:**
| Benchmark | std::map + std::list | PriceLevelArray + pmr |
|-----------|---------------------|-----------------------|
| Add       | 51 ns               | 61 ns                 |
| Cancel    | 65 ns               | 57 ns                 |
| Match     | 52 ns               | 80 ns                 |
| Mixed     | 90 ns               | 70 ns                 |
**the median of 8 runs at depth 50 levels / 20 orders*

**Conclusion:**
It's a tie - each version wins two of the four:
- `std::map` is faster on `Match` (best level is just `begin()`, no scanning) and slightly faster on `Add`.
- `PriceLevelArray + pmr` is faster on `Cancel` and `Mixed`.

The flat array's main selling point - O(1) instead of O(log n) - doesn't help here, because the price range caps the map at ~256 levels, so `O(log n)` is only ~7 cheap comparisons on hot cache.

> ⚠️ The first single-run numbers showed the array winning by ~2x everywhere. That run was done on battery - the CPU was capped at ~2.4 GHz instead of 4.0 GHz (no turbo), which scaled the map+list numbers up by ~1.6x. Run `utils/bench_preflight.sh` first (checks AC power, turbo, governor, frequency) and always use `--benchmark_repetitions`.

Since performance is a tie, going back to `std::map + std::list`:
- simpler code, stdlib only, no fixed-size array and no custom allocator;
- even faster on the latency-critical `Match` path;
- `pmr::unsynchronized_pool_resource` is not thread-safe, which would get in the way of the upcoming multithreading experiments.

## Multithreading

### ✅ SPSC queue: `alignas(64)` on head/tail (false sharing)

#### Problem:
The lock-free `SPSCQueue` (`src/spsc_queue.hpp`) uses two atomic indices: `read` (written only by the consumer) and `write` (written only by the producer). False sharing is happening if both sit in the same 64-byte cache line.

#### Solution:
`alignas(64)` on each of `read` and `write`, so they land on separate cache lines.

**Results** (`bench_spsc`, producer/consumer pinned to separate physical cores via `taskset -c 0,2`, median of 8 reps):
| Metric        | with `alignas(64)` | without (same line) |
|---------------|--------------------|---------------------|
| Time/op       | 10.1 ns            | 17.3 ns             |
| Throughput    | 99 M items/s       | 58 M items/s        |
| cv            | 8.0 %              | 7.5 %               |

**Conclusion:**
Splitting the two indices onto separate cache lines gives **~40% more throughput** (58 → 99 M items/s). The win only appears with the two threads on *different* cores — on a single core (or hyperthread siblings) there is no inter-core line transfer to eliminate. Measured with `taskset -c 0,2` to keep producer and consumer on distinct physical cores.

### Correctness: ThreadSanitizer

The single-threaded gtest cases (`QueueOrder`, `WrapAround`, ...) verify ring-buffer logic, `tests/test_spsc_concurrent.cpp` runs a producer thread against a consumer thread (1M items, strict FIFO check) under TSan (`dev.sh tsan`).

