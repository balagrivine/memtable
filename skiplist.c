#include "skiplist.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static inline int generate_random_level(float probability, int max_level) {
    int level = 1;

    while (rand() / (double)RAND_MAX < probability && level < max_level) {
        level++;
    }

    return level;
}

int skiplist_new(skiplist_t** list, float probability, int max_level,
                 int (*comparator)(uint8_t*, uint32_t, uint8_t*, uint32_t)) {
    if (!list || probability <= 0.0f || probability >= 1.0f || max_level == 0 || !comparator) {
        return -1;
    }

    skiplist_t* new_list = (skiplist_t*)malloc(sizeof(skiplist_t));
    if (!new_list) return -1;

    size_t node_size = sizeof(skiplist_node_t) + (sizeof(skiplist_node_t*) * max_level);

    skiplist_node_t* head = (skiplist_node_t*)malloc(node_size);
    if (!head) {
        free(new_list);
        return -1;
    }

    for (int i = 0; i < max_level; i++) {
        head->forward[i] = NULL;
    }

    new_list->head = head;
    new_list->current_level = 1;
    new_list->probability = probability;
    new_list->max_level = max_level;
    new_list->compare_keys = comparator;

    *list = new_list;

    return 0;
}

skiplist_node_t* skiplist_create_node(skiplist_t* list, uint8_t* key, uint32_t key_size,
                                      uint8_t* value, uint32_t value_size, int level,
                                      uint8_t flags) {
    if (!list || !key || key_size == 0 || !value || value_size == 0 || level == 0) {
        return NULL;
    }

    size_t node_size = sizeof(skiplist_node_t) + (level * sizeof(skiplist_node_t*));

    skiplist_node_t* node = malloc(node_size);
    if (!node) return NULL;

    for (int i = 0; i < level; i++) {
        node->forward[i] = NULL;
    }

    uint8_t* key_buffer = (uint8_t*)malloc((size_t)key_size);
    if (!key_buffer) {
        free(node);
        return NULL;
    }

    memcpy(key_buffer, key, key_size);
    node->key = key;
    node->key_size = key_size;
    node->flags = flags;

    if (flags & IS_TOMBSTONE) {
        node->value = NULL;
        node->value_size = 0;
    } else {
        uint8_t* value_buffer = (uint8_t*)malloc((size_t)value_size);
        if (!value_buffer) {
            free(node);
            free(key_buffer);

            return NULL;
        }

        memcpy(value_buffer, value, value_size);

        node->value = value;
        node->value_size = value_size;
    }

    return node;
}

skiplist_node_t* skiplist_get_predecesor(skiplist_t* list, uint8_t* key, uint32_t key_size,
                                         skiplist_node_t** update) {
    if (!list || !key || key_size == 0) {
        return NULL;
    }

    skiplist_node_t* current = list->head;
    for (int i = list->current_level - 1; i >= 0; i--) {
        while (current->forward[i] &&
               list->compare_keys(current->forward[i]->key, current->forward[i]->key_size, key,
                                  key_size) < 0) {
            current = current->forward[i];
        }

        if (update) update[i] = current;
    }

    return current;
}

int skiplist_get(skiplist_t* list, uint8_t* key, uint32_t key_size, uint8_t** value,
                 uint32_t* value_size) {
    if (!list || !key || key_size == 0 || !value) {
        return -1;
    }

    skiplist_node_t* pred = skiplist_get_predecesor(list, key, key_size, NULL);
    if (!pred) {
        *value = NULL;
        return -1;
    }

    skiplist_node_t* target = pred->forward[0];
    if (!target || target->flags & IS_TOMBSTONE) {
        return SKIPLIST_ERR_NOT_FOUND;
    }

    if (memcmp(target->key, key, key_size) == 0) {
        uint8_t* value_bufer = (uint8_t*)malloc(target->value_size);
        if (!value_bufer) return -1;

        memcpy(value_bufer, target->value, target->value_size);
        *value = value_bufer;
        *value_size = target->value_size;
    }

    return 0;
}

int skiplist_put(skiplist_t* list, uint8_t* key, uint32_t key_size, uint8_t* value,
                 uint32_t value_size, uint8_t flags) {
    if (!list || !key || key_size == 0 || !value || value_size == 0) {
        return -1;
    }

    skiplist_node_t* update[list->max_level];

    skiplist_node_t* pred = skiplist_get_predecesor(list, key, key_size, update);
    if (!pred) return -1;

    int node_level = generate_random_level(list->probability, list->max_level);

    if (node_level > list->current_level) {
        for (int i = list->current_level; i < node_level; i++) {
            update[i] = list->head;
        }

        list->current_level = node_level;
    }

    skiplist_node_t* new_node =
            skiplist_create_node(list, key, key_size, value, value_size, node_level, flags);
    if (!new_node) return -1;

    for (int i = 0; i < list->current_level; i++) {
        new_node->forward[i] = update[i]->forward[i];
        update[i]->forward[i] = new_node;
    }

    return 0;
}
