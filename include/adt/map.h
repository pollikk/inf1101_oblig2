/**
 * @authors
 * Odin Bjerke <odin.bjerke@uit.no>
 *
 * @note
 * The implementation PANICS on insertion error (failure to allocate memory) in order to avoid complex error
 * checking. This is not feasible for any production-grade implementation, but it's fine for our purposes
 * here.
 */

#ifndef MAP_H
#define MAP_H

#include <stddef.h> // for size_t

#include "defs.h"

/**
 * Type of map. `map_t` is an alias for `struct map`
 */
typedef struct map map_t;

/**
 * This struct is utilized by the map to return key/value pairs from functions. Depending on the context, it
 * may be borrowed from the map (e.g. @`map_get`) or the callers responsibility to free (e.g. @ `map_remove`)
 */
typedef struct entry {
    void *key;
    void *val;
} entry_t;

/**
 * @brief Creates a new, empty map. The map uses the `entr_t` structs to return key/value pairs from
 * functions. Depending on the context, these entries may be borrowed from the map (e.g. @`map_get`) or the
 * callers responsibility to free (e.g. @ `map_remove`).
 *
 * @param cmpfn: function for comparing keys
 * @param hashfn: function for hashing entry keys
 *
 * @note Depending on the underlying implementation, `hashfn` may not be utilized, but should always be
 * provided nonetheless
 *
 * @returns NULL on error, otherwise a pointer to the newly created map
 */
map_t *map_create(cmp_fn cmpfn, hash64_fn hashfn);

/**
 * @brief Destroys the given map. Optional functionality to also destroy values
 * @param map: pointer to a map
 * @param key_freefn: nullable. If present, called on all keys
 * @param val_freefn: nullable. If present, called on all values
 * @note this is safe to call with `map` == NULL, where it simply returns
 */
void map_destroy(map_t *map, free_fn key_freefn, free_fn val_freefn);

/**
 * @brief Get the number of entries contained in a map, colloquially referred to as its length
 * @param map: pointer to a map
 * @returns The number of entries in `map`
 */
size_t map_length(map_t *map);

/**
 * @brief insert a entry into the map. This will replace the entry currently at `key` if such an entry exists.
 *
 * @param map: pointer to map
 * @param key: pointer to a key
 * @param key: pointer to a value
 *
 * @returns NULL if the map did not previously have this key present; otherwise an allocated `entry_t`
 * containing the old, removed key-value pair that was present for this key
 *
 * @note It is the callers responsibility to free a returned entry
 * @note To avoid making the map interface more complex than it already is - this function simply panics on
 * malloc failure
 */
entry_t *map_insert(map_t *map, void *key, void *val);

/**
 * @brief Attempt to remove an entry from the map by its key
 * @param map: pointer to a map
 * @param key: pointer to key
 *
 * @returns NULL if the map did not have this key present, otherwise an `entry_t` containing the
 * removed key-value pair
 *
 * @note It is the callers responsibility to free a returned entry
 */
entry_t *map_remove(map_t *map, void *key);

/**
 * @brief Attempt to get the entry associated with the given key. This function also serves as the de-facto
 * utility to check for the presense of a key in the map
 *
 * @param map: pointer to a map
 * @param key: pointer to key
 *
 * @returns The entry associated with key if is present in the map, otherwise NULL
 *
 * @warning The returned entry is borrowed to the caller by the map. It should never be freed by the caller,
 * or have its key modified in a way that will affect the relevant comparison function.
 */
entry_t *map_get(map_t *map, void *key);

/**
 * Type of map iterator. `map_iter_t` is an alias for `struct map_iter`
 */
typedef struct map_iter map_iter_t;

/**
 * @brief Create an iterator for entries in the given map. Modifying the map during iteration
 * @param map: pointer to map
 * @returns A pointer to the newly allocated iterator, or NULL on failure
 * @note the sequence of items produced by this iterator is unknown, and does not nescessarily correlate to
 * the order in which items were added.
 * @warning removing or inserting items during iteration is undefined behavior
 */
map_iter_t *map_createiter(map_t *map);

/**
 * @brief Destroy a map iterator. Does not free the underlying map
 * @param iter: pointer to iterator
 */
void map_destroyiter(map_iter_t *iter);

/**
 * @brief Check if the given map iterator has reached the end of the underlying map
 * @param iter: pointer to map iterator
 * @returns 0 if iterator is exhausted, otherwise a non-zero integer.
 */
int map_hasnext(map_iter_t *iter);

/**
 * @brief Get the next entry from a map iterator
 * @param iter: pointer to map iterator
 * @returns A pointer to the next entry in the sequence represented by the iterator
 *
 * @warning The returned entry is borrowed to the caller by the map. It should never be freed by the caller,
 * or have its key modified in a way that will affect the relevant comparison function.
 */
entry_t *map_next(map_iter_t *iter);


#endif /* MAP_H */
