#!/usr/bin/env bash
# Checks the machine is in a stable state for benchmarking.
# Exits non-zero if something would make results noisy/inconsistent,
# so you can gate the run:  ./scripts/bench_preflight.sh && taskset -c 0 ./build/bench
set -u

ok=0

# 1. AC power? On my machine on battery the CPU drops out of turbo (4.0 -> ~2.4 GHz).
ac=$(cat /sys/class/power_supply/AC/online 2>/dev/null || echo 1)
if [ "$ac" != "1" ]; then
    echo "WARN: running on battery - no turbo, frequency will be capped. Plug in the charger."
    ok=1
fi

# 2. Turbo enabled?
no_turbo=$(cat /sys/devices/system/cpu/intel_pstate/no_turbo 2>/dev/null || echo 0)
if [ "$no_turbo" != "0" ]; then
    echo "WARN: turbo is disabled (intel_pstate/no_turbo=$no_turbo)."
    ok=1
fi

# 3. Governor = performance?
gov=$(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor 2>/dev/null || echo unknown)
if [ "$gov" != "performance" ]; then
    echo "WARN: governor is '$gov', not 'performance'. Run: sudo cpupower frequency-set -g performance"
    ok=1
fi

# 4. Current frequency close to max? (within 10%)
cur=$(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq 2>/dev/null || echo 0)
max=$(cat /sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq 2>/dev/null || echo 0)
if [ "$max" -gt 0 ] && [ "$cur" -lt $(( max * 9 / 10 )) ]; then
    echo "WARN: cpu0 at $(( cur / 1000 )) MHz vs max $(( max / 1000 )) MHz - frequency is throttled."
    ok=1
fi

if [ "$ok" -eq 0 ]; then
    echo "OK: AC power, turbo on, performance governor, $(( cur / 1000 ))/$(( max / 1000 )) MHz."
else
    echo "--> benchmark results may be unreliable in this state."
fi
exit $ok
