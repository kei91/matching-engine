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