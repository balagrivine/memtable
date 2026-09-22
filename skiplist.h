#include <stdint.h>

#define IS_TOMBSTONE 0x01

#define SKIPLIST_ERR_NOT_FOUND -4

/*
 * skiplist_node_t
 * represents a node within the skiplist
 * @param flags determines whether this is a head,tail,tombstone sentinel
 * @param key_size size of the key in this node
 * @param value_size size of the value in this node
 * @param value the value in bytes
 * @param key the key in bytes
 */
typedef struct skiplist_node_t {
    uint8_t flags;
    uint32_t key_size;
    uint32_t value_size;
    uint8_t* value;
    uint8_t* key;
    struct skiplist_node_t* forward[];
} skiplist_node_t;

/*
 * skiplist_t
 * the actual skiplist
 * @param current_level the current maximum active level in the list
 * @param max_level the maximum level to cap new node promotion to higher levels
 * @param probability a flip-coin probability to determine whether a node gets promoted to a higher
 * level
 * @param head the list's head
 * @param comparator_fn the custom comparator used to lexographically compare keys
 */
typedef struct skiplist_t {
    int current_level;
    int max_level;
    float probability;
    skiplist_node_t* head;
    int (*compare_keys)(uint8_t*, uint32_t, uint8_t*, uint32_t);
} skiplist_t;

int skiplist_new(skiplist_t** list, float probability, int max_level,
                 int (*comparator_fn)(uint8_t*, uint32_t, uint8_t*, uint32_t));

skiplist_node_t* skiplist_create_node(skiplist_t* list, uint8_t* key, uint32_t key_size,
                                      uint8_t* value, uint32_t value_size, int level,
                                      uint8_t flags);

int skiplist_get(skiplist_t* list, uint8_t* key, uint32_t key_size, uint8_t** value,
                 uint32_t* value_size);

int skiplist_put(skiplist_t* list, uint8_t* key, uint32_t key_size, uint8_t* value,
                 uint32_t value_size, uint8_t flags);

int skiplist_delete(skiplist_t* list, uint8_t* key, uint32_t value);

skiplist_node_t* skiplist_get_predecesor(skiplist_t* list, uint8_t* key, uint32_t key_size,
                                         skiplist_node_t** update);

int skiplist_destroy(skiplist_t** list);
