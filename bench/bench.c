#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "../skiplist.h"

#define SKIPLIST_PUT_OPS 10000000

skiplist_t* list;

static void generate_random_key_value(uint8_t* key, uint8_t* value, int key_size, int value_size) {
    const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    int charset_size = sizeof(charset) - 1;

    for (int i = 0; i < key_size; i++) {
        int k = rand() % charset_size;
        key[i] = charset[k];
    }

    for (int i = 0; i < value_size; i++) {
        int k = rand() % charset_size;
        value[i] = charset[k];
    }

    key[key_size] = '\0';
    value[value_size] = '\0';
}

void bench_skiplist_put() {
    assert(list != NULL);

    int key_size = 10, value_size = 20, rc = 0;
    uint8_t key[key_size + 1], value[value_size + 1];

    generate_random_key_value(key, value, key_size, value_size);

    for (int i = 0; i < SKIPLIST_PUT_OPS; i++) {
        rc = skiplist_put(list, key, key_size, value, value_size, 0);
        if (rc != 0) break;
    }

    printf("Completed skiplist benchmark operations with return code: %d\n", rc);
}

int bench_comparator(uint8_t* key_a, uint32_t key_a_size, uint8_t* key_b, uint32_t key_b_size) {
    uint32_t min = key_a_size < key_b_size ? key_a_size : key_b_size;

    int cmp = memcmp(key_a, key_b, min);
    if (cmp != 0) return cmp;

    if (key_a_size < key_b_size) return -1;

    if (key_a_size > key_b_size) return 1;

    return 0;
}

int main(void) {
    srand(time(NULL));

    float probability = 0.5;
    int max_level = 16;

    assert(skiplist_new(&list, probability, max_level, bench_comparator) == 0);

    bench_skiplist_put();
}
