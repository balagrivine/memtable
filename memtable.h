#ifndef __MEMTABLE_H__
#define __MEMTABLE_H__

#include "skiplist.h"

/*
 * op_type represents the type of operation
 */
typedef enum op_type {
    put = 0x00,
    delete = 0x01,
} op_type;

typedef struct memtable_t {
    uint32_t size;
    skiplist_t* skiplist;
} memtable_t;

typedef struct memtable_entry_t {
    uint8_t* key;
    uint32_t key_size;
    uint8_t* value;
    uint32_t value_size;
    uint64_t sequence;
} memtable_entry_t;

int memtable_create(memtable_t** memtable, float probability, int max_level);

int memtable_insert(memtable_t* memtable, memtable_entry_t* entry);

int memtable_get(memtable_t* memtable, uint8_t* key, uint32_t key_size, uint64_t sequence,
                 uint8_t** value, uint32_t* value_size);

int memtable_delete(memtable_t* memtable, memtable_entry_t* entry);

#endif /* MEMTABLE_H */
