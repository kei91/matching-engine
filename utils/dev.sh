#!/usr/bin/env bash
# Dev helper. Usage:
#   ./utils/dev.sh test      build (Release) and run unit tests
#   ./utils/dev.sh bench     build, stabilize the machine, run benchmarks (perf)
#   ./utils/dev.sh bench-latency  build, stabilize, run the rdtsc latency bench (p50/p99/p99.9)
#   ./utils/dev.sh asan      build the asan target and run it (memory / UB checks)
#   ./utils/dev.sh tsan      build the concurrent SPSC test under ThreadSanitizer (data races)
#
# Extra args after the command go to the underlying tool, e.g.:
#   ./utils/dev.sh bench --benchmark_filter=BM_MatchOrders
#   ./utils/dev.sh test  -R MatchTest
set -euo pipefail

cd "$(dirname "$0")/.."   # repo root

cmd="${1:-}"
shift || true

case "$cmd" in
    test)
        cmake -B build -DCMAKE_BUILD_TYPE=Release
        cmake --build build
        ctest --test-dir build --output-on-failure "$@"
        ;;

    bench)
        cmake -B build -DCMAKE_BUILD_TYPE=Release
        cmake --build build

        # performance governor needs sudo + cpupower; preflight re-checks it below
        sudo cpupower frequency-set -g performance || echo "WARN: couldn't set governor (cpupower/sudo missing)"

        # only the perf benchmark needs a stable machine - abort if it isn't
        if ! ./utils/bench_preflight.sh; then
            echo "Environment not stable for benchmarking. Aborting."
            exit 1
        fi

        # default to repeated runs unless the caller passes their own flags
        if [ "$#" -eq 0 ]; then
            set -- --benchmark_repetitions=8 --benchmark_report_aggregates_only=true
        fi
        taskset -c 0 ./build/bench "$@"
        ;;

    bench-spsc)
        cmake -B build -DCMAKE_BUILD_TYPE=Release
        cmake --build build --target bench_spsc

        sudo cpupower frequency-set -g performance || echo "WARN: couldn't set governor"
        if ! ./utils/bench_preflight.sh; then
            echo "Environment not stable for benchmarking. Aborting."
            exit 1
        fi

        if [ "$#" -eq 0 ]; then
            set -- --benchmark_repetitions=8 --benchmark_report_aggregates_only=true
        fi
        # -c 0,2 keeps producer and consumer on SEPARATE physical cores
        # (NOT 0,1 - those are usually hyperthread siblings sharing L1/L2).
        taskset -c 0,2 ./build/bench_spsc "$@"
        ;;    

    bench-latency)
        cmake -B build -DCMAKE_BUILD_TYPE=Release
        cmake --build build --target bench_latency

        sudo cpupower frequency-set -g performance || echo "WARN: couldn't set governor"
        if ! ./utils/bench_preflight.sh; then
            echo "Environment not stable for benchmarking. Aborting."
            exit 1
        fi

        # pinned to one core; the bench prints its own p50/p99/p99.9 distribution
        taskset -c 0 ./build/bench_latency "$@"
        ;;

    asan)
        # asan is for catching memory/UB bugs, not timing - no preflight/governor/taskset
        cmake -B build_asan -DCMAKE_BUILD_TYPE=Debug
        cmake --build build_asan --target bench_asan tests_asan
        ./build_asan/tests_asan "$@"
        ./build_asan/bench_asan "$@"
        ;;

    tsan)
        cmake -B build_tsan -DCMAKE_BUILD_TYPE=Debug
        cmake --build build_tsan --target tests_tsan
        # setarch -R disables ASLR (Address Space Layout Randomization): 
        # TSan aborts with "unexpected memory mapping" on recent kernels.
        setarch -R ./build_tsan/tests_tsan "$@"
        ;;

    *)
        echo "usage: $0 {test|bench|bench-spsc|bench-latency|asan|tsan} [extra args...]"
        exit 2
        ;;
esac
