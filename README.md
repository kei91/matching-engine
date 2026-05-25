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
cmake -B build
cmake --build build
```
## Run

**Run tests:**
```bash
cd build && ctest
```
**Run benchmarks:**
```bash
cd build && ./bench
```
