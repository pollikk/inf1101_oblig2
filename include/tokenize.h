/**
 * @authors
 * Odin Bjerke <odin.bjerke@uit.no>
 */

#ifndef TOKENIZE_H
#define TOKENIZE_H

#include <stddef.h> // for size_t

#include "defs.h"
#include "list.h"

/* max size of tokens produced by `tokenize_*`, in bytes */
#define TOKEN_SIZE_MAX 1024

/**
 * @brief Powerful and versatile utility to read, filter, delimit and/or convert strings. Designed to be
 * used with the characters manipulation functions available through `<ctype.h>`. Supports
 *
 * See
 * [ctype.h docs](https://www.ibm.com/docs/en/aix/7.3?topic=libraries-list-character-manipulation-subroutines)
 * for a good overview on these functions, or define your own instead.
 *
 * @param str: null-terminated string
 * @param list: pointer to list. Any tokens will be added last to the list, in the order they appear
 * in the file.
 * @param min_token_len: ommit tokens of a length lower than this
 * @param splitfn: pointer to function. Called on each character. Split into token when returned
 * value is non-zero. The split character(s) is not included in the token.
 * @param filterfn [nullable]: If present, the function is called on each character. If the returned
 * value is zero, that character is ommitted from the token.
 * @param transformfn [nullable]: If present, the function is called on each character. The returned
 * character is added to the token in place of the original character. Applied after filter, if
 * present.
 *
 * @returns 0 on success, otherwise a negative error code
 *
 * @note in the event of an error, the given list is returned to its initial state.
 */
int tokenize_string(
    const char *str,
    list_t *list,
    size_t min_token_len,
    int (*splitfn)(int),
    int (*filterfn)(int),
    int (*transformfn)(int)
);

/**
 * @brief Powerful and versatile utility to read, filter, delimit and/or convert file content. Designed to be
 * used with the characters manipulation functions available through `<ctype.h>`.
 *
 * Same as tokenize_string, but for reading a stream (file) at the same time.
 *
 * See
 * [ctype.h docs](https://www.ibm.com/docs/en/aix/7.3?topic=libraries-list-character-manipulation-subroutines)
 * for a good overview on these functions, or define your own instead.
 *
 * @param f: pointer to readable file stream
 * @param list: pointer to list. Any tokens will be added last to the list, in the order they appear
 * in the file.
 * @param min_token_len: ommit tokens of a length lower than this
 * @param splitfn: pointer to function. Called on each character. Split into token when returned
 * value is non-zero. The split character(s) is not included in the token.
 * @param filterfn [nullable]: If present, the function is called on each character. If the returned
 * value is zero, that character is ommitted from the token.
 * @param transformfn [nullable]: If present, the function is called on each character. The returned
 * character is added to the token in place of the original character. Applied after filter, if
 * present.
 *
 * @returns 0 on success, otherwise a negative error code: -1 = critical, -2 = file exits but read failed
 *
 * @note in the event of an error, the given list is returned to its initial state
 */
int tokenize_file(
    FILE *f,
    list_t *list,
    size_t min_token_len,
    int (*splitfn)(int),
    int (*filterfn)(int),
    int (*transformfn)(int)
);

#endif /* TOKENIZE_H */
