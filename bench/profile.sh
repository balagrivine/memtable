#!/usr/bin/env bash
#
# Runs the skiplist benchmark under whichever profilers are installed and drops
# the reports in one directory.
#
#   ./bench/profile.sh                  # every stage that has its tool present
#   ./bench/profile.sh record           # just the perf sampling profile
#   ./bench/profile.sh cachegrind dhat  # any subset
#
# Env knobs: PERF_OPS, VG_OPS, OUT, BENCH.
#
# The valgrind stages simulate a CPU rather than reading one, so they work on a
# box with no PMU -- but they run 50-100x slower, hence the smaller VG_OPS.

set -euo pipefail

cd "$(dirname "$0")/.."

BENCH=${BENCH:-./bench/bench}
OUT=${OUT:-bench/profile-out}
PERF_OPS=${PERF_OPS:-100000}
VG_OPS=${VG_OPS:-100000}

STAGES=("$@")
if [ ${#STAGES[@]} -eq 0 ]; then
    STAGES=(time stat record cachegrind callgrind dhat)
fi

have() { command -v "$1" >/dev/null 2>&1; }

wants() {
    local stage
    for stage in "${STAGES[@]}"; do
        [ "$stage" = "$1" ] && return 0
    done
    return 1
}

note() { printf '\n== %s\n' "$*"; }
skip() { printf '   skipped: %s\n' "$*"; }

if [ ! -x "$BENCH" ]; then
    echo "$BENCH not built. Run: make bench" >&2
    exit 1
fi

mkdir -p "$OUT"

# perf's default event is 'cycles', which needs a hardware PMU. Virtual machines
# usually do not expose one; cpu-clock is an hrtimer-driven software event that
# samples just as well, it just cannot report cycle-accurate costs.
perf_event() {
    if perf stat -e cycles -- true 2>&1 | grep -qi 'not supported'; then
        echo cpu-clock
    else
        echo cycles
    fi
}

check_paranoid() {
    local level
    [ -r /proc/sys/kernel/perf_event_paranoid ] || return 0
    level=$(cat /proc/sys/kernel/perf_event_paranoid)
    if [ "$level" -gt 1 ]; then
        echo "   note: perf_event_paranoid=$level may block sampling."
        echo "         sudo sysctl kernel.perf_event_paranoid=1"
        echo "         (preferred over 'sudo perf record', which leaves root-owned output)"
    fi
}

if wants time; then
    note "resident memory and fault counts ($PERF_OPS ops)"
    if /usr/bin/time -v true >/dev/null 2>&1; then
        /usr/bin/time -v "$BENCH" "$PERF_OPS" 2>&1 | tee "$OUT/time.txt"
    elif /usr/bin/time -l true >/dev/null 2>&1; then
        /usr/bin/time -l "$BENCH" "$PERF_OPS" 2>&1 | tee "$OUT/time.txt"
    else
        skip "no /usr/bin/time supporting -v or -l"
    fi
fi

if wants stat; then
    note "perf stat: what kind of slow is this?"
    if have perf; then
        check_paranoid
        perf stat -e task-clock,context-switches,page-faults,minor-faults,major-faults,\
cycles,instructions,branches,branch-misses,cache-references,cache-misses \
            -- "$BENCH" "$PERF_OPS" 2>&1 | tee "$OUT/perf-stat.txt"
    else
        skip "perf not installed"
    fi
fi

if wants record; then
    note "perf record: which functions burn the time?"
    if have perf; then
        check_paranoid
        event=$(perf_event)
        echo "   sampling event: $event"
        perf record -e "$event" -F 999 -g --call-graph fp \
            -o "$OUT/perf.data" -- "$BENCH" "$PERF_OPS" 2>&1 | tail -2
        perf report -i "$OUT/perf.data" --stdio --no-children --percent-limit 1 \
            > "$OUT/perf-self.txt"
        perf report -i "$OUT/perf.data" --stdio --children --percent-limit 1 \
            > "$OUT/perf-children.txt"
        echo "   top self time:"
        grep -E '^ +[0-9]' "$OUT/perf-self.txt" | head -10 | sed 's/^/     /'
    else
        skip "perf not installed"
    fi
fi

if wants cachegrind; then
    note "cachegrind: simulated cache and branch behaviour ($VG_OPS ops)"
    if have valgrind; then
        valgrind --tool=cachegrind --branch-sim=yes \
            --cachegrind-out-file="$OUT/cachegrind.out" \
            "$BENCH" "$VG_OPS" 2> "$OUT/cachegrind.txt" || true
        if have cg_annotate; then
            cg_annotate "$OUT/cachegrind.out" > "$OUT/cachegrind-annotated.txt"
        fi
        grep -E 'refs|miss rate|mispred' "$OUT/cachegrind.txt" | sed 's/^/   /' || true
    else
        skip "valgrind not installed"
    fi
fi

if wants callgrind; then
    note "callgrind: exact instruction counts per function ($VG_OPS ops)"
    if have valgrind; then
        valgrind --tool=callgrind --callgrind-out-file="$OUT/callgrind.out" \
            "$BENCH" "$VG_OPS" 2> "$OUT/callgrind.txt" || true
        if have callgrind_annotate; then
            callgrind_annotate "$OUT/callgrind.out" > "$OUT/callgrind-annotated.txt"
            echo "   top instruction counts:"
            sed -n '/Ir  *file:function/,/^$/p' "$OUT/callgrind-annotated.txt" \
                | head -12 | sed 's/^/     /'
        fi
    else
        skip "valgrind not installed"
    fi
fi

if wants dhat; then
    note "dhat: allocation sites, block lifetimes, bytes actually touched ($VG_OPS ops)"
    if have valgrind; then
        valgrind --tool=dhat --dhat-out-file="$OUT/dhat.out" \
            "$BENCH" "$VG_OPS" 2> "$OUT/dhat.txt" || true
        grep -E 'Total:|At t-gmax:|At t-end:' "$OUT/dhat.txt" | sed 's/^/   /' || true
        echo "   full detail: load $OUT/dhat.out into https://nnethercote.github.io/dh_view/dh_view.html"
    else
        skip "valgrind not installed"
    fi
fi

note "reports in $OUT/"
ls -1 "$OUT" 2>/dev/null | sed 's/^/   /'
