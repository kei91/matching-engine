#!/usr/bin/env python3
"""Run a benchmark N times and report per-row medians plus run-to-run spread.

WHY THIS EXISTS:
    A single benchmark run is one sample. Some numbers it prints are stable to
    three digits, others move by 6x between identical runs - and you cannot tell
    which is which from one run. Worse, near the knee of a load curve the median
    itself is bimodal: repeated runs land on one of two values (see the "knee is
    a bistable zone" section in BENCHMARKS.md), and quoting a single run there is
    quoting a coin flip.

    So: repeat, take the median, and print the spread next to it. The spread
    column is the point - it tells you how many digits of each number are real.

    Repetition lives here rather than in the C++ so the benchmarks stay about
    measuring one thing, and so any bench with a table-shaped output can use it.

USAGE:
    ./utils/bench_reps.py                       # 5 reps of bench-pipeline
    ./utils/bench_reps.py -n 3                  # 3 reps
    ./utils/bench_reps.py -c bench-latency      # a different dev.sh command
    ./utils/bench_reps.py -n 7 -c bench-pipeline

The machine must be stable: each rep goes through dev.sh, which runs the
governor + preflight gate. If preflight refuses, this aborts rather than
averaging in a throttled run.
"""

import argparse
import os
import re
import statistics as st
import subprocess
import sys

# Rows of the sweep table, e.g.
#   "  10% |   0.83 |   0.83 |    219 |    1881 |   16396 |   236679 |     356.9"
# Leading label is the load level; the rest are numeric columns.
ROW = re.compile(r"^\s*(\d+)%\s*\|(.+)$")
NUM = re.compile(r"^[\d.]+$")

# Scalars reported once per run, above the table.
SERVICE = re.compile(r"service time.*?:\s*([\d.]+)\s*ns/order")
CAPACITY = re.compile(r"capacity\s*~\s*([\d.]+)\s*M orders/s")


def parse_run(text):
    """-> (rows: {load: [floats]}, service: float|None, capacity: float|None)"""
    rows = {}
    for line in text.splitlines():
        m = ROW.match(line)
        if not m:
            continue
        cells = [c.strip() for c in m.group(2).split("|")]
        if all(NUM.match(c) for c in cells if c):
            rows[int(m.group(1))] = [float(c) for c in cells if c]

    s = SERVICE.search(text)
    c = CAPACITY.search(text)
    return rows, (float(s.group(1)) if s else None), (float(c.group(1)) if c else None)


def spread(xs):
    """max/min - how much a number moves between identical runs."""
    lo, hi = min(xs), max(xs)
    return hi / lo if lo > 0 else float("inf")


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("-n", "--reps", type=int, default=5, help="number of runs (default 5)")
    ap.add_argument("-c", "--cmd", default="bench-pipeline",
                    help="dev.sh subcommand to run (default bench-pipeline)")
    ap.add_argument("--timeout", type=int, default=1800,
                    help="per-run timeout in seconds (default 1800)")
    args = ap.parse_args()

    os.chdir(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))

    all_rows, service, capacity = {}, [], []
    header = None

    for rep in range(1, args.reps + 1):
        print(f"--- rep {rep}/{args.reps} ---", flush=True)
        try:
            out = subprocess.run(["./utils/dev.sh", args.cmd],
                                 capture_output=True, text=True,
                                 timeout=args.timeout).stdout
        except subprocess.TimeoutExpired:
            sys.exit(f"rep {rep} exceeded {args.timeout}s - aborting")

        if "Environment not stable" in out:
            sys.exit("preflight refused the run - machine not stable. "
                     "Fix it (governor/battery) and retry; do not average a throttled run.")

        rows, s, c = parse_run(out)
        if not rows:
            sys.exit(f"rep {rep}: no table rows parsed - did the output format change?\n\n{out}")

        # Keep the bench's own column header so the summary is self-describing.
        if header is None:
            for line in out.splitlines():
                if "p50" in line and "|" in line:
                    header = line
                    break

        for load, vals in rows.items():
            all_rows.setdefault(load, []).append(vals)
        if s is not None:
            service.append(s)
        if c is not None:
            capacity.append(c)

    ncols = min(len(v) for vs in all_rows.values() for v in vs)

    print(f"\n\n=== medians over {args.reps} reps ({args.cmd}) ===")
    if service:
        print(f"service time: {st.median(service):.1f} ns/order  (spread {spread(service):.2f}x)")
    if capacity:
        print(f"capacity:     {st.median(capacity):.2f} M orders/s  (spread {spread(capacity):.2f}x)")
    print()
    if header:
        print(header)

    for load in sorted(all_rows):
        cols = list(zip(*[v[:ncols] for v in all_rows[load]]))
        meds = [st.median(c) for c in cols]
        # Columns are: target M/s, achieved M/s, p50, p99, p99.9, max, mean.
        # Rates want 2 decimals, latencies are integers, the last (mean) gets 1.
        cells = []
        for i, m in enumerate(meds):
            if i < 2:
                cells.append(f"{m:6.2f}")
            elif i == len(meds) - 1:
                cells.append(f"{m:9.1f}")
            else:
                cells.append(f"{m:7.0f}")
        print(f"{load:4d}% | " + " | ".join(cells))

    # Reuse the bench's own column names for the spread table, so it reads the
    # same way as the medians above instead of col1..colN.
    names = []
    if header:
        names = [c.strip() for c in header.split("|")[1:]]
    if len(names) != ncols:
        names = [f"col{i + 1}" for i in range(ncols)]

    print(f"\n=== run-to-run spread (max/min over {args.reps} reps) ===")
    print("A number is only worth quoting to the precision its spread supports.")
    print("load | " + " | ".join(f"{n:>7}" for n in names))

    suspicious = []
    for load in sorted(all_rows):
        cols = list(zip(*[v[:ncols] for v in all_rows[load]]))
        sp = [spread(c) for c in cols]
        print(f"{load:4d}% | " + " | ".join(f"{s:6.2f}x" for s in sp))
        for name, s in zip(names, sp):
            # A median column that swings by more than ~5x is not noise on top of
            # a stable value - the samples are landing in two different regimes,
            # and the median between them describes neither.
            if s > 5.0 and name.startswith("p"):
                suspicious.append((load, name, s))

    if suspicious:
        print("\n!! unstable columns - the median above is NOT a usable summary:")
        for load, name, s in suspicious:
            print(f"     {load}% {name}: swings {s:.1f}x between runs")
        print("   Either the runs straddle a regime boundary (see the knee section in")
        print("   BENCHMARKS.md) or the machine was noisy. Inspect the per-run output")
        print("   and the distribution shape before quoting anything from that row.")


if __name__ == "__main__":
    main()
