# matching-engine
C++ matching engine for exploring lock-free data structures, cache optimization, and multithreading patterns

## Build

**Requirements:** 
GCC 11+, CMake 3.28+

Google Benchmark:
```bash
sudo apt install libbenchmark-dev
```

**Build all:**
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

**Build asan:**
```bash
cmake -B build_asan -DCMAKE_BUILD_TYPE=Debug
cmake --build build_asan --target bench_asan
```

## Run

**Run tests:**
```bash
cd build && ctest
```
**Run benchmarks:**
```bash
sudo cpupower frequency-set -g performance
taskset -c 0 ./build/bench
```
**Run asan benchmarks:**
```bash
sudo cpupower frequency-set -g performance
taskset -c 0 ./build_asan/bench_asan
```
