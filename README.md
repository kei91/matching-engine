# matching-engine
C++ matching engine for exploring lock-free data structures, cache optimization, and multithreading patterns

## Build

### Requirements: 
- GCC 11+, CMake 3.28+
- Google Benchmark v1.9.5 (fetched automatically via FetchContent)

### Build all:
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### Build asan:
```bash
cmake -B build_asan -DCMAKE_BUILD_TYPE=Debug
cmake --build build_asan --target bench_asan
```

## Run

### Run tests:
```bash
cd build && ctest
```
### Run benchmarks:
```bash
sudo cpupower frequency-set -g performance
taskset -c 0 ./build/bench
```
### Run asan benchmarks:
```bash
sudo cpupower frequency-set -g performance
taskset -c 0 ./build_asan/bench_asan
```

## Performance

### Setup
- CPU: Intel Core i5, 4 cores, 8 threads, 4GHz
- L1/L2/L3: 32KB / 256KB / 8MB
- Compiler: GCC 13, -O3 -march=native
- Benchmark: Google Benchmark v1.9.5

### Baseline (std::map + std::deque)
| Benchmark      | Time      |
|----------------|-----------|
| Add            | 71 ns     |
| Cancel         | 60 ns     |
| Match          | 111 ns    |
| MixedWorkload* | 139507 ns |

**MixedWorkload had anomalous results in  due to O(n) scan in cancel() - more details in [BENCHMARKS.md](BENCHMARKS.md)*

### Current Results (std::map + std::list)
| Benchmark      | Time   |
|----------------|--------|
| Add            | 99 ns  |
| Cancel         | 62 ns  |
| Match          | 149 ns |
| MixedWorkload  | 84 ns  |

***More info** in [BENCHMARKS.md](BENCHMARKS.md)*
