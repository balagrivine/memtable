#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../skiplist.h"

/*
 * op_type represents the type of operation
 */
typedef enum op_type {
    put = 0x00,
    delete = 0x01,
} op_type;

static inline void encode_be64(const uint64_t value, uint8_t* out) {
    out[0] = (uint8_t)(value >> 56);
    out[1] = (uint8_t)(value >> 48);
    out[2] = (uint8_t)(value >> 40);
    out[3] = (uint8_t)(value >> 32);
    out[4] = (uint8_t)(value >> 24);
    out[5] = (uint8_t)(value >> 16);
    out[6] = (uint8_t)(value >> 8);
    out[7] = (uint8_t)value;
}

static inline uint64_t decode_be64(const uint8_t* p) {
    return ((uint64_t)p[0] << 56) | ((uint64_t)p[1] << 48) | ((uint64_t)p[2] << 40) |
           ((uint64_t)p[3] << 32) | ((uint64_t)p[4] << 24) | ((uint64_t)p[5] << 16) |
           ((uint64_t)p[6] << 8) | (uint64_t)p[7];
}

static int encode_internal_key(const uint8_t* key, size_t key_size, uint64_t sequence, op_type type,
                               uint8_t* out) {
    if (!key || key_size == 0 || sequence == 0) return -1;

    memcpy(out, key, key_size);

    encode_be64(sequence, out + key_size);

    out[key_size + sizeof(uint64_t)] = type;

    return 0;
}

static int decode_internal_key(uint8_t* internal_key, size_t internal_key_size, uint8_t** key,
                               uint32_t* key_size, op_type* type, uint64_t* sequence) {
    if (!internal_key || internal_key_size == 0 || !key) return -1;

    *key_size = internal_key_size - sizeof(uint64_t) - sizeof(op_type);

    *key = internal_key;

    *sequence = decode_be64(internal_key + *key_size);

    *type = internal_key[*key_size + sizeof(uint64_t)];

    return 0;
}

static inline int generate_random_level(float probability, int max_level) {
    int level = 1;

    while (rand() / (double)RAND_MAX < probability && level < max_level) {
        level++;
    }

    return level;
}

static int skiplist_compare_keys(uint8_t* internal_key_a, uint32_t internal_key_a_size,
                                 uint8_t* internal_key_b, uint32_t internal_key_b_size) {
    if (!internal_key_a || !internal_key_b || internal_key_a_size == 0 || internal_key_b_size == 0)
        return 0;

    uint8_t *user_key_a, *user_key_b;
    uint32_t user_key_a_size, user_key_b_size;
    uint64_t user_key_a_sequence, user_key_b_sequence;
    op_type op_type;

    decode_internal_key(internal_key_a, (size_t)internal_key_a_size, &user_key_a, &user_key_a_size,
                        &op_type, &user_key_a_sequence);

    decode_internal_key(internal_key_b, (size_t)internal_key_b_size, &user_key_b, &user_key_b_size,
                        &op_type, &user_key_b_sequence);

    uint32_t min = user_key_a_size < user_key_b_size ? user_key_a_size : user_key_b_size;

    int cmp = memcmp(user_key_a, user_key_b, min);
    if (cmp != 0) return cmp;

    if (user_key_a_size < user_key_b_size || user_key_a_sequence < user_key_b_sequence) return -1;

    if (user_key_a_size > user_key_b_size || user_key_a_sequence > user_key_b_sequence) return 1;

    return 0;
}

void test_skiplist_new() {
    skiplist_t* list = NULL;
    int max_level = 8;
    float probability = 0.5;

    int rc = skiplist_new(&list, probability, max_level, skiplist_compare_keys);
    assert(rc == 0);
    assert(list->current_level = 1);
    assert(list->max_level = max_level);
    assert(list->probability = probability);
    assert(list->head != NULL);

    skiplist_node_t* head = list->head;

    for (int i = 0; i < max_level; i++) {
        assert(head->forward[i] == NULL);
    }

    list = NULL;
    max_level = 0;
    probability = 1.0;

    assert(skiplist_new(&list, probability, max_level, skiplist_compare_keys) == -1);
}

void test_skiplist_create_node() {
    skiplist_t* list = NULL;
    int max_level = 8;
    float probability = 0.5;
    uint8_t* key = (uint8_t*)"key";
    uint8_t* value = (uint8_t*)"value";
    uint64_t sequence = 1;
    int is_delete = 0;
    int level = generate_random_level(probability, max_level);

    uint64_t internal_key_size = strlen((char*)key) + sizeof(uint64_t) + sizeof(op_type);
    uint8_t internal_key[internal_key_size];

    encode_internal_key(key, strlen((char*)key), sequence, put, internal_key);

    assert(skiplist_new(&list, probability, max_level, skiplist_compare_keys) == 0);

    skiplist_node_t* node = skiplist_create_node(list, internal_key, internal_key_size, value,
                                                 strlen((char*)value), level, is_delete);

    assert(node != NULL);

    int fwd_pointer_count = 0;
    for (int i = 0; i < level; i++) {
        fwd_pointer_count++;
    }

    assert(fwd_pointer_count == level);
    assert(memcmp(node->value, value, node->value_size) == 0);

    uint8_t* decoded_key;
    uint32_t key_size = 0;
    op_type type = 0;
    uint64_t decoded_sequence = 0;
    decode_internal_key(node->key, node->key_size, &decoded_key, &key_size, &type,
                        &decoded_sequence);

    assert(memcmp(key, decoded_key, key_size) == 0);
    assert(decoded_sequence == sequence);
    assert(type == put);

    is_delete = 1;
    node = skiplist_create_node(list, key, strlen((char*)key), value, strlen((char*)value), level,
                                is_delete);

    assert(node->flags & IS_TOMBSTONE);
    assert(node->value == NULL);
    assert(node->value_size == 0);

    assert(skiplist_destroy(&list) == 0);
    assert(list == NULL);
}

void test_skiplist_put() {
    skiplist_t* list = NULL;
    int max_level = 8;
    float probability = 0.5;
    uint8_t* key = (uint8_t*)"key";
    uint8_t* value = (uint8_t*)"value";
    uint8_t flags = 0;
    int sequence = 1;

    assert(skiplist_new(&list, probability, max_level, skiplist_compare_keys) == 0);

    uint64_t internal_key_size = strlen((char*)key) + sizeof(uint64_t) + sizeof(op_type);
    uint8_t internal_key[internal_key_size];

    encode_internal_key(key, strlen((char*)key), sequence, put, internal_key);

    assert(skiplist_put(list, internal_key, internal_key_size, value, strlen((char*)value),
                        flags) == 0);

    uint8_t* ret_value = NULL;
    uint32_t value_size;

    assert(skiplist_get(list, internal_key, internal_key_size, &ret_value, &value_size) == 0);
    assert(memcmp(value, ret_value, value_size) == 0);

    ret_value = NULL;
    assert(skiplist_put(list, internal_key, internal_key_size, value, strlen((char*)value),
                        flags) == 0);

    assert(skiplist_get(list, internal_key, internal_key_size, &ret_value, &value_size) == 0);
    assert(memcmp(value, ret_value, value_size) == 0);

    int total_nodes = 0;
    skiplist_node_t* current = list->head->forward[0];
    while (current != NULL) {
        total_nodes++;
        current = current->forward[0];
    }

    assert(total_nodes == 2);

    assert(skiplist_destroy(&list) == 0);
    assert(list == NULL);
}

void test_skiplist_get() {
    skiplist_t* list = NULL;
    int max_level = 8;
    float probability = 0.5;
    uint8_t* key = (uint8_t*)"key";
    uint8_t* value = (uint8_t*)"value";
    uint8_t flags = 0;
    int sequence = 1;

    assert(skiplist_new(&list, probability, max_level, skiplist_compare_keys) == 0);

    uint64_t internal_key_size = strlen((char*)key) + sizeof(uint64_t) + sizeof(op_type);
    uint8_t internal_key[internal_key_size];

    encode_internal_key(key, strlen((char*)key), sequence, put, internal_key);
    assert(skiplist_put(list, internal_key, internal_key_size, value, strlen((char*)value),
                        flags) == 0);

    uint8_t* ret_value = NULL;
    uint32_t value_size;

    assert(skiplist_get(list, internal_key, internal_key_size, &ret_value, &value_size) == 0);
    assert(memcmp(value, ret_value, value_size) == 0);

    key = (uint8_t*)"non_existent";
    ret_value = NULL;

    internal_key_size = strlen((char*)key) + sizeof(uint64_t) + sizeof(op_type);
    uint8_t internal_key1[internal_key_size];

    encode_internal_key(key, strlen((char*)key), sequence, put, internal_key1);

    assert(skiplist_get(list, internal_key1, internal_key_size, &ret_value, &value_size) ==
           SKIPLIST_ERR_NOT_FOUND);

    assert(skiplist_destroy(&list) == 0);
    assert(list == NULL);
}

int main(void) {
    test_skiplist_new();
    test_skiplist_create_node();
    test_skiplist_put();
    test_skiplist_get();
}
