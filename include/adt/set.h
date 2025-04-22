/**
 * @authors
 * Odin Bjerke <odin.bjerke@uit.no>
 *
 * @note
 * The implementation PANICS on insertion error (failure to allocate memory) in order to avoid complex error
 * checking. This is not feasible for any production-grade implementation, but it's fine for our purposes
 * here.
 */

#ifndef SET_H
#define SET_H

#include <stddef.h> // for size_t

#include "defs.h"

/**
 * Type of set. `set_t` is an alias for `struct set`
 */
typedef struct set set_t;

/**
 * @brief Creates a new, empty set
 * @param cmpfn: function for comparing elements
 * @returns A pointer to the newly created set, or NULL on failure
 */
set_t *set_create(cmp_fn cmpfn);

/**
 * @brief Destroys the given set. Optional functionality to also destroy values
 * @param set: pointer to a set
 * @param elem_freefn: nullable. If present, the function is called on each element of the set.
 * @note this is safe to call with `set` == NULL, where it simply returns
 */
void set_destroy(set_t *set, free_fn elem_freefn);

/**
 * @brief Get the number of elements contained in a set, colloquially referred to as its length
 * @param set: pointer to a set
 * @returns The number of elements in `set`
 */
size_t set_length(set_t *set);

/**
 * @brief Add an element to the given set
 * @param set: pointer to a set
 * @returns NULL if the set did not already contain this elem. Otherwise, a pointer to the old, removed entry.
 */
void *set_insert(set_t *set, void *elem);

/**
 * @brief Get an element from the given set
 *
 * @param set: pointer to a set
 * @param elem: pointer to a an element to search for, using the sets comparison function to check for
 * equality
 *
 * @returns Returns a pointer to the found element if it is contained in the given set, otherwise NULL
 *
 * @warning the returned value is borrowed to the caller by the set. Modifying it in a way that affects
 * comparison function on it may lead to undefined behavior.
 */
void *set_get(set_t *set, void *elem);

/**
 * @brief Set union operation
 *
 * @param a: pointer to a set. The comparison function of this set is utilized, and passed on to `c`
 * @param b: ponter to another set
 * @returns A newly created set `c`, containing elements that are in EITHER `a` or `b`. Returns NULL on
 * failure
 *
 * @warning the returned set `c` contains shallow copies of elements from both `a`, `b` and should be treated
 * with a significant degree of care. Modifying values of `c` in a way that affects comparison function on
 * said elements may lead to undefined behavior.
 */
set_t *set_union(set_t *a, set_t *b);

/**
 * @brief Set intersection operation
 *
 * @param a: pointer to a set. The comparison function of this set is utilized, and passed on to `c`
 * @param b: ponter to another set
 * @returns A newly created set `c`, containing elements that are in BOTH `a` and `b`. Returns NULL on
 * failure
 *
 * @warning the returned set `c` contains shallow copies of elements from `a`, `b` and should be treated
 * with a significant degree of care. Modifying values of `c` in a way that affects comparison function on
 * said elements may lead to undefined behavior.
 */
set_t *set_intersection(set_t *a, set_t *b);

/**
 * @brief Set difference operation.
 * @param a: pointer to a set. The comparison function of this set is utilized, and passed on to `c`
 * @param b: ponter to another set
 * @returns A newly created set `c`, containing elements all elements from `a` that are NOT IN `b`. Returns
 * NULL on failure
 *
 * @warning the returned set `c` contains shallow copies of elements from `a`, and should be treated with a
 * significant degree of care. Modifying values of `c` in a way that affects comparison function on
 * said elements may lead to undefined behavior.
 */
set_t *set_difference(set_t *a, set_t *b);

/**
 * Type of set iterator. `set_iter_t` is an alias for `struct set_iter`
 */
typedef struct set_iter set_iter_t;

/**
 * @brief Create an iterator for the given set
 * @param set: pointer to set
 * @returns A pointer to the newly allocated iterator, or NULL on failure
 *
 * @warning it is imperative you destroy this iterator before altering the set
 */
set_iter_t *set_createiter(set_t *set);

/**
 * @brief Destroy a set iterator. Does not free the underlying set or any items within it
 * @param iter: pointer to iterator
 */
void set_destroyiter(set_iter_t *iter);

/**
 * @brief Check if the given set iterator has reached the end of the underlying set
 * @param iter: pointer to iterator
 * @returns 0 if iterator is exhausted, otherwise a non-zero integer.
 */
int set_hasnext(set_iter_t *iter);

/**
 * @brief Get the next item in the sequence represented by the iterator
 * @param iter: pointer to iterator
 * @returns A pointer to the next item
 *
 * @warning the returned value is borrowed to the caller by the set. Modifying it in a way that affects
 * comparison function on it may lead to undefined behavior.
 */
void *set_next(set_iter_t *iter);

#endif
