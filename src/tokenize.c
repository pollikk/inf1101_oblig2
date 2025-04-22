/**
 * @authors
 * Odin Bjerke <odin.bjerke@uit.no>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <limits.h>

#include "printing.h"
#include "tokenize.h"
#include "common.h"
#include "list.h"


/* Helper: append token to list if above the length threshold */
static inline int
append_token_dup(list_t *list, char *base, char *head, size_t min_token_len, size_t offset) {
    *head = '\0';

    if (offset >= min_token_len) {
        char *cpy = strdup(base);
        if (cpy == NULL) {
            pr_error("Strdup failed: %s\n", strerror(errno));
            return -1;
        }

        if (list_addlast(list, cpy) < 0) {
            pr_error("list_addlast failed\n");
            free(cpy);
            return -1;
        }
    }

    return 0;
}

int tokenize_string(
    const char *str,
    list_t *list,
    size_t min_token_len,
    int (*delimitfn)(int),
    int (*filterfn)(int),
    int (*transformfn)(int)
) {
    char base[TOKEN_SIZE_MAX];
    base[0] = '\0';

    static const size_t offset_lim = (TOKEN_SIZE_MAX - 2); // must leave room for null terminator + next char
    size_t list_len_before = list_length(list);
    size_t offset = 0; // offset from head to base, and current length of the string we are building

    int status = 0;    // will be set negative on error, otherwise 0
    char *head = base; // next char to write to in token

    for (; status == 0; str++) {
        int c = *str;

        if (c == '\0') {
            /* done, if we have a token built up, add it and break out */
            status = append_token_dup(list, base, head, min_token_len, offset);
            break;
        }

        int is_delimiter = delimitfn(c);

        if (is_delimiter) {
            /* delimiter found. split here. */
            status = append_token_dup(list, base, head, min_token_len, offset);

            /* reset to beginning of buffer, get ready for new token */
            head = base;
            offset = 0;
        }

        if (filterfn == NULL || filterfn(c)) {
            /* Transform character if appliccable, then add it to the buffer */
            *head = transformfn ? transformfn(c) : c;
            offset++;
            head++;

            /* if a delimiter is here, it means it should be included as its own token, since it passed the
             * filter function */
            if (is_delimiter) {
                status = append_token_dup(list, base, head, min_token_len, offset);
                head = base;
                offset = 0;
            } else if (offset >= offset_lim) {
                /* skip token, iterating to next splitter */
                const char *s = str;
                while (*s && !delimitfn(*s)) {
                    s++;
                }
                head = base;
                offset = 0;
            }
        }
    }

    /* either complete the operation, or revert list state on error */
    while ((status < 0) && (list_length(list) > list_len_before)) {
        free(list_poplast(list));
    }

    return status;
}

int tokenize_file(
    FILE *f,
    list_t *list,
    size_t min_token_len,
    int (*delimitfn)(int),
    int (*filterfn)(int),
    int (*transformfn)(int)
) {
    long maybe_file_size = fsize(f);
    if (maybe_file_size < 0) {
        return -1;
    }

    /* cast to unsigned */
    size_t file_size = (size_t) maybe_file_size;

    if (file_size == 0 || file_size < min_token_len) {
        return 0; /* nothing to do */
    }

    /* allocated a temp buffer equal to the size of the file */
    char *content = malloc(file_size + 1);
    if (content == NULL) {
        pr_error("Malloc failed: %s\n", strerror(errno));
        return -1;
    }

    errno = 0;
    /* read the entire file in one gulp */
    size_t read_bytes = fread(content, 1, file_size, f);
    if (errno) {
        pr_warn("Error during read of file: %s\n", strerror(errno));
        free(content);
        return -2;
    }

    content[read_bytes - 1] = '\0';

    /* run tokenize on the buffer */
    int rv = tokenize_string(content, list, min_token_len, delimitfn, filterfn, transformfn);

    /* free the temp buffer. Tokenize will have allocated individual strings per token. */
    free(content);

    return rv;
}
