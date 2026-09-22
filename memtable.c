#include "memtable.h"

#include <stdlib.h>
#include <string.h>

static inline uint64_t decode_be64(const uint8_t* p) {
    return ((uint64_t)p[0] << 56) | ((uint64_t)p[1] << 48) | ((uint64_t)p[2] << 40) |
           ((uint64_t)p[3] << 32) | ((uint64_t)p[4] << 24) | ((uint64_t)p[5] << 16) |
           ((uint64_t)p[6] << 8) | (uint64_t)p[7];
}

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

int memtable_create(memtable_t** memtable, float probability, int max_level) {
    if (!memtable) return -1;

    memtable_t* mt = malloc(sizeof(*mt));
    if (!mt) return -1;

    mt->skiplist = NULL;
    mt->size = 0;

    int rc = skiplist_new(&mt->skiplist, probability, max_level, skiplist_compare_keys);
    if (rc != 0) {
        free(mt);

        return -1;
    }

    *memtable = mt;
    return 0;
}

int memtable_get(memtable_t* memtable, uint8_t* key, uint32_t key_size, uint64_t sequence,
                 uint8_t** value, uint32_t* value_size) {
    uint64_t internal_key_size = key_size + sizeof(uint64_t) + sizeof(op_type);
    uint8_t internal_key[internal_key_size];

    encode_internal_key(key, strlen((char*)key), sequence, put, internal_key);

    if (skiplist_get(memtable->skiplist, internal_key, internal_key_size, value, value_size) != 0)
        return -1;

    return 0;
}

int memtable_insert(memtable_t* memtable, memtable_entry_t* entry) {
    if (!entry || !memtable) return -1;

    uint64_t internal_key_size = entry->key_size + sizeof(uint64_t) + sizeof(op_type);
    uint8_t internal_key[internal_key_size];

    encode_internal_key(entry->key, entry->key_size, entry->sequence, put, internal_key);

    return skiplist_put(memtable->skiplist, internal_key, internal_key_size, entry->value,
                        entry->value_size, 0);
}

int memtable_delete(memtable_t* memtable, memtable_entry_t* entry) {
    if (!entry || !memtable) return -1;

    uint64_t internal_key_size = entry->key_size + sizeof(uint64_t) + sizeof(op_type);
    uint8_t internal_key[internal_key_size];

    encode_internal_key(entry->key, entry->key_size, entry->sequence, put, internal_key);

    return (skiplist_put(memtable->skiplist, internal_key, internal_key_size, NULL, 0,
                         IS_TOMBSTONE) != 0);
}
