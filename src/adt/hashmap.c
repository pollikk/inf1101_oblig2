/**
 * @authors
 * Odin Bjerke <odin.bjerke@uit.no>
 *
 * @implements map.h
 * 
 * @brief Hash map with separate chaining.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "printing.h"
#include "defs.h"
#include "common.h"
#include "map.h"


/* how many buckets each map should start with */
#define N_BUCKETS_INITIAL 16

/**
 * Double the number of buckets when a collision occurs at or above this load factor threshold.
 * A lower value leads to fewer collisions, but makes the map take up more memory.
 *
 * Note that the map may only grow after a collision occurs, meaning that if the map is given
 * a perfect hash function is perfect for the keys, there is no space overhead.
 */
#define LF_GROW 0.75


typedef struct mnode mnode_t;
struct mnode {
    entry_t *entry;
    mnode_t *overflow; // points to overflow entry if a collision occurs
};

struct map {
    cmp_fn cmpfn;
    hash64_fn hashfn;
    mnode_t **buckets;
    size_t capacity;
    size_t length;
    size_t rehash_threshold;
};

/**
 * Calculate the length threshold where the map will rehash on a collision
 */
static inline size_t calc_rehash_threshold(size_t capacity) {
    /* lf% of capacity, rounded down */
    double new_thresh = (double) capacity * (double) LF_GROW;

    return (size_t) new_thresh;
}

/* flatten the map, i.e. turning it into a singly linked list */
ATTR_MAYBE_UNUSED
static inline mnode_t *map_flatten(map_t *map) {
    mnode_t *head = NULL;
    mnode_t *tail = NULL;

    /* iterate over all buckets */
    for (size_t i = 0; i < map->capacity; i++) {
        mnode_t *node = map->buckets[i];

        if (node) {
            if (tail) {
                tail->overflow = node;
            } else {
                head = node;
                tail = node;
            }

            /* move tail to the end of the chain */
            while (tail->overflow) {
                tail = tail->overflow;
            }
        }
    }

    return head;
}

/**
 * resize the array of buckets and rehash all entries. There is simply no way to do this that isn't O(n), as
 * we must rehash for all nodes.
 */
static inline int map_resize(map_t *map, size_t new_capacity) {
    mnode_t **new_buckets = calloc(new_capacity, sizeof(mnode_t *));
    if (new_buckets == NULL) {
        return -1;
    }

    size_t n_i = 0;

    /* iterate over all existing buckets */
    for (size_t i_old = 0; i_old < map->capacity; i_old++) {
        mnode_t *node = map->buckets[i_old];

        /* iterate over the node & overflow chain */
        while (node) {
            mnode_t *next = node->overflow; // tmp
            size_t i_new = map->hashfn(node->entry->key) % new_capacity;

            node->overflow = new_buckets[i_new]; // NULL if no chain
            new_buckets[i_new] = node;           // set as new head of chain

            node = next; // increment iter
            n_i += 1;
        }
    }

    assert(n_i == map->length);

    /* free old buckets */
    free(map->buckets);

    // pr_info("{ c: %zu, t: %zu }", map->capacity, map->rehash_threshold);

    map->buckets = new_buckets;
    map->capacity = new_capacity;
    map->rehash_threshold = calc_rehash_threshold(new_capacity);

    // pr_info(" -> { c: %zu, t: %zu }\n", map->capacity, map->rehash_threshold);

    return 0;
}

map_t *map_create(cmp_fn cmpfn, hash64_fn hashfn) {
    map_t *map = malloc(sizeof(map_t));
    if (map == NULL) {
        pr_error("Failed to allocate memory\n");
        return NULL;
    }

    map->buckets = calloc(N_BUCKETS_INITIAL, sizeof(mnode_t *));
    if (map->buckets == NULL) {
        pr_error("Failed to allocate memory\n");
        free(map);
        return NULL;
    }

    map->cmpfn = cmpfn;
    map->hashfn = hashfn;
    map->length = 0;
    map->capacity = N_BUCKETS_INITIAL;
    map->rehash_threshold = calc_rehash_threshold(N_BUCKETS_INITIAL);

    return map;
}

void map_destroy(map_t *map, free_fn key_freefn, free_fn val_freefn) {
    if (!map) {
        return;
    }
    mnode_t *node;

    /* iterate over all buckets */
    for (size_t i = 0; i < map->capacity; i++) {
        node = map->buckets[i];

        /* iterate over the node & overflow chain */
        while (node) {
            mnode_t *next = node->overflow;

            if (key_freefn) {
                key_freefn(node->entry->key);
            }
            if (val_freefn) {
                val_freefn(node->entry->val);
            }

            free(node->entry);
            free(node);

            node = next;
        }
    }

    free(map->buckets);
    free(map);
}

size_t map_length(map_t *map) {
    return map->length;
}

entry_t *map_insert(map_t *map, void *key, void *val) {
    /* Allocate a new entry to be inserted */
    entry_t *entry = malloc(sizeof(entry_t));
    if (!entry) {
        PANIC("Failed to allocate memory\n");
    }

    /* initialize the new entry */
    entry->key = key;
    entry->val = val;

    size_t bucket_i = map->hashfn(key) % map->capacity;
    mnode_t *head = map->buckets[bucket_i];
    mnode_t *curr = head;

    while (curr) {
        if (map->cmpfn(key, curr->entry->key) == 0) {
            /* already present, swap entries and return old entry */
            entry_t *old_entry = curr->entry;
            curr->entry = entry;

            return old_entry;
        }
        curr = curr->overflow;
    }

    /* Key is not present in the map. Allocate a new node as well. */
    mnode_t *new_node = malloc(sizeof(mnode_t));
    if (new_node == NULL) {
        PANIC("Failed to allocate memory\n");
    }

    new_node->entry = entry;
    new_node->overflow = head; // NULL if there was no collission

    map->buckets[bucket_i] = new_node; // set as new head of chain
    map->length++;

    /**
     * If there was a collission and we're above load factor, grow & rehash.
     * It would seem better to to do this before insertion, but we'd have to redo hash/checks etc anyhow, and
     * this streamlines the function logic, likely allowing the compiler to optimize it better
     */
    if (head && (map->length >= map->rehash_threshold)) {
        size_t new_capacity = map->capacity * 2;
        if (map_resize(map, new_capacity) != 0) {
            PANIC("Failed to rehash\n");
        }
    }

    return NULL;
}

entry_t *map_remove(map_t *map, void *key) {
    size_t bucket_i = map->hashfn(key) % map->capacity;
    mnode_t *node = map->buckets[bucket_i];

    mnode_t *prev = NULL;

    while (node && (map->cmpfn(node->entry->key, key) != 0)) {
        prev = node;
        node = node->overflow;
    }

    if (!node) {
        return NULL;
    }

    if (!prev) {
        map->buckets[bucket_i] = node->overflow; // node is first in a bucket
    } else {
        prev->overflow = node->overflow; // fix previous' overflow pointer
    }

    entry_t *entry = node->entry;
    free(node);
    map->length--;

    return entry;
}

entry_t *map_get(map_t *map, void *key) {
    size_t bucket_i = map->hashfn(key) % map->capacity;
    mnode_t *node = map->buckets[bucket_i];

    while (node) {
        if (map->cmpfn(node->entry->key, key) == 0) {
            return node->entry;
        }
        node = node->overflow;
    }

    return NULL;
}


struct map_iter {
    mnode_t **buckets;
    mnode_t *next;
    size_t i_curr_bucket;
    size_t n_remaining;
};

map_iter_t *map_createiter(map_t *map) {
    map_iter_t *iter = malloc(sizeof(map_iter_t));
    if (iter == NULL) {
        pr_error("Failed to allocate memory\n");
        return NULL;
    }

    iter->n_remaining = map->length;
    iter->buckets = map->buckets;
    iter->i_curr_bucket = 0;
    iter->next = map->buckets[0];

    return iter;
}

void map_destroyiter(map_iter_t *iter) {
    free(iter);
}

int map_hasnext(map_iter_t *iter) {
    return (int) iter->n_remaining;
}

entry_t *map_next(map_iter_t *iter) {
    if (iter->n_remaining == 0) {
        return NULL;
    }

    mnode_t *curr = iter->next;

    while (curr == NULL) {
        iter->i_curr_bucket += 1;
        curr = iter->buckets[iter->i_curr_bucket];
    }

    assert(curr);
    assert(curr->entry);

    iter->next = curr->overflow;
    iter->n_remaining -= 1;

    return curr->entry;
}
