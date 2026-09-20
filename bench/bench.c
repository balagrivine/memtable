#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../skiplist.h"

#define DEFAULT_PUT_OPS 100000
#define KEY_SIZE 10
#define VALUE_SIZE 20

/*
 * Fixed seeds keep runs comparable. A benchmark you cannot diff against the
 * previous run cannot tell you whether a change helped.
 */
#define RNG_SEED 0x2545f4914f6cdd1dULL
#define LIBC_SEED 1

static const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
#define CHARSET_SIZE (sizeof(charset) - 1)

skiplist_t* list;

static uint64_t rng_state = RNG_SEED;

static inline uint64_t xorshift64(void) {
    uint64_t x = rng_state;

    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;

    return rng_state = x;
}

static double now_seconds(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);

    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

/*
 * Keys and values live in two flat buffers generated before the timed region,
 * so the profile attributes samples to skiplist_put rather than to the harness
 * PRNG. Sizes are passed explicitly, so no NUL terminator is needed.
 */
static int generate_workload(long ops, uint8_t** keys, uint8_t** values) {
    uint8_t* key_buffer = (uint8_t*)malloc((size_t)ops * KEY_SIZE);
    if (!key_buffer) return -1;

    uint8_t* value_buffer = (uint8_t*)malloc((size_t)ops * VALUE_SIZE);
    if (!value_buffer) {
        free(key_buffer);
        return -1;
    }

    for (size_t i = 0; i < (size_t)ops * KEY_SIZE; i++) {
        key_buffer[i] = (uint8_t)charset[xorshift64() % CHARSET_SIZE];
    }

    for (size_t i = 0; i < (size_t)ops * VALUE_SIZE; i++) {
        value_buffer[i] = (uint8_t)charset[xorshift64() % CHARSET_SIZE];
    }

    *keys = key_buffer;
    *values = value_buffer;

    return 0;
}

static int bench_skiplist_put(long ops, uint8_t* keys, uint8_t* values) {
    int rc = 0;

    double start = now_seconds();

    for (long i = 0; i < ops; i++) {
        rc = skiplist_put(list, keys + i * KEY_SIZE, KEY_SIZE, values + i * VALUE_SIZE, VALUE_SIZE,
                          0);
        if (rc != 0) break;
    }

    double elapsed = now_seconds() - start;

    if (rc != 0) {
        fprintf(stderr, "skiplist_put failed with return code %d\n", rc);
        return rc;
    }

    printf("put     ops=%ld  elapsed=%.3fs  %.1f ns/op  %.0f ops/sec\n", ops, elapsed,
           elapsed * 1e9 / (double)ops, (double)ops / elapsed);

    return 0;
}

int bench_comparator(uint8_t* key_a, uint32_t key_a_size, uint8_t* key_b, uint32_t key_b_size) {
    uint32_t min = key_a_size < key_b_size ? key_a_size : key_b_size;

    int cmp = memcmp(key_a, key_b, min);
    if (cmp != 0) return cmp;

    if (key_a_size < key_b_size) return -1;

    if (key_a_size > key_b_size) return 1;

    return 0;
}

int main(int argc, char** argv) {
    long ops = DEFAULT_PUT_OPS;

    if (argc > 1) {
        ops = strtol(argv[1], NULL, 10);
        if (ops <= 0) {
            fprintf(stderr, "usage: %s [num_ops]\n", argv[0]);
            return 1;
        }
    }

    /* skiplist.c still draws its node levels from rand(), so seed it too. */
    srand(LIBC_SEED);

    float probability = 0.5;
    int max_level = 16;

    if (skiplist_new(&list, probability, max_level, bench_comparator) != 0) {
        fprintf(stderr, "skiplist_new failed\n");
        return 1;
    }

    uint8_t *keys = NULL, *values = NULL;
    if (generate_workload(ops, &keys, &values) != 0) {
        fprintf(stderr, "failed to allocate workload for %ld ops\n", ops);
        return 1;
    }

    int rc = bench_skiplist_put(ops, keys, values);

    free(keys);
    free(values);

    return rc == 0 ? 0 : 1;
}
