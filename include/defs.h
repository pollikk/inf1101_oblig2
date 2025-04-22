/**
 * @authors
 * Odin Bjerke <odin.bjerke@uit.no>
 * 
 * @brief Header containing general definitions without any associated C source
 */

#ifndef DEFS_H
#define DEFS_H

#include <stdint.h>

/**
 * @brief Signal to the compiler that a function may be intentionally unused
 */
#define ATTR_MAYBE_UNUSED __attribute__((unused))

/**
 * @brief Signal to the compiler that a switch case may intetionally fall through
 */
#define ATTR_FALLTHROUGH __attribute__((fallthrough))

/**
 * @brief To silence unused variable warning during development
 */
#define UNUSED(x) (void) (x)

/**
 * @brief Cursors to top left + Erase in Display
 * 
 * See [wikipedia ~ ANSI escape code](https://en.wikipedia.org/wiki/ANSI_escape_code#Control_Sequence_Introducer_commands)
 */
#define ANSI_CLEAR_TERM "\033[H\033[J"

/**
 * @brief Type of comparison function
 *
 * @returns
 * - 0 on equality
 *
 * - >0 if a > b
 *
 * - <0 if a < b
 *
 * @note When passing references to comparison functions as parameters,
 * typecast to cmp_fn if said functions' parameters are non-void pointers.
 */
typedef int (*cmp_fn)(const void *, const void *);

/**
 * @brief Type of free (resource deallocation) function
 * @note may be the actual `free` function, or another user-defined function
 */
typedef void (*free_fn)(void *);

/**
 * @brief Type of 64-bit hash function
 */
typedef uint64_t (*hash64_fn)(const void *);


#endif /* DEFS_H */
