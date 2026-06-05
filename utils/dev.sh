#!/usr/bin/env bash
# Dev helper. Usage:
#   ./utils/dev.sh test      build (Release) and run unit tests
#   ./utils/dev.sh bench     build, stabilize the machine, run benchmarks (perf)
#   ./utils/dev.sh asan      build the asan target and run it (memory / UB checks)
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

    asan)
        # asan is for catching memory/UB bugs, not timing - no preflight/governor/taskset
        cmake -B build_asan -DCMAKE_BUILD_TYPE=Debug
        cmake --build build_asan --target bench_asan
        ./build_asan/bench_asan "$@"
        ;;

    *)
        echo "usage: $0 {test|bench|asan} [extra args...]"
        exit 2
        ;;
esac
