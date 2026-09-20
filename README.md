## Membtable
A skiplist-based memtable implemetation.

The skiplist is reimplemented from William Pugh's work on skiplist reserach from the 1990 paper.

The memtable perfroms insert, search, deletion, immutable conversion and flush operations.

Key's are never deleted / over-written in-place. Instead, deletions insert a sentinel `version` value, ideally a tombstone marker.

### Running tests
```shell
make test
```

### Running benchmarks
The benchmark build is `-O2` with debug info and frame pointers, and no
sanitizers: ASan redzones and shadow memory distort both timing and page-fault
counts badly enough to make the numbers meaningless.

```shell
make bench
./bench/bench            # 10,000,000 puts
./bench/bench 100000     # or pass an op count
```

`make bench-asan` builds the same harness under address and UB sanitizers for
correctness checking. Use it to find bugs, never to measure speed.

### Profiling
```shell
make profile                        # every stage whose tool is installed
./bench/profile.sh record           # or a single stage
./bench/profile.sh cachegrind dhat  # or a subset
```

Stages are `time`, `stat`, `record`, `cachegrind`, `callgrind`, `dhat`; reports
land in `bench/profile-out/`. `PERF_OPS`, `VG_OPS`, `OUT` and `BENCH` override
the defaults.

The valgrind stages simulate a CPU instead of reading one, so they produce
instruction counts and cache statistics on machines with no hardware PMU --
most virtual machines, where `perf stat` reports `<not supported>` for
`cycles` and `instructions`. The script detects this and falls back to perf's
`cpu-clock` software event for sampling.
