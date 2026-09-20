CC ?= cc

STD      = -std=gnu11
WARNINGS = -Wall -Wextra

# Measurement build: -O2 so the compiler does its job, -g and frame pointers so
# perf can resolve symbols and walk stacks. No sanitizers -- ASan redzones and
# shadow memory inflate both time and page faults, which makes timings useless.
BENCH_OPT = -O2 -g -fno-omit-frame-pointer

# Correctness build. Slow by design.
SAN_OPT = -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined

SKIPLIST = skiplist.c skiplist.h

.PHONY: all bench bench-asan test profile clean

all: bench

bench: bench/bench

bench/bench: bench/bench.c $(SKIPLIST)
	$(CC) $(STD) $(WARNINGS) $(BENCH_OPT) skiplist.c bench/bench.c -o $@

# The skiplist has no destructor yet, so leak reports at exit are expected here.
bench-asan: bench/bench-asan

bench/bench-asan: bench/bench.c $(SKIPLIST)
	$(CC) $(STD) $(WARNINGS) $(SAN_OPT) skiplist.c bench/bench.c -o $@

tests/test: tests/skiplist_tests.c $(SKIPLIST)
	$(CC) $(STD) $(WARNINGS) $(SAN_OPT) skiplist.c tests/skiplist_tests.c -o $@

test: tests/test
	./tests/test

profile: bench/bench
	./bench/profile.sh

clean:
	rm -f bench/bench bench/bench-asan tests/test
	rm -rf bench/profile-out
