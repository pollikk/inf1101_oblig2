/**
 * @authors
 * Odin Bjerke <odin.bjerke@uit.no>
 *
 * @brief commonly used functions that don't really fit in anywhere else, and have minimal internal
 * dependancies
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <limits.h>

#include "printing.h"
#include "common.h"

/* -- comparison functions -- */

int compare_integers(const int *a, const int *b) {
    return (*a) - (*b);
}

int compare_characters(const char *a, const char *b) {
    return (int) *a - (int) *b;
}

int compare_pointers(const void *a, const void *b) {
    uintptr_t pa = (uintptr_t) a;
    uintptr_t pb = (uintptr_t) b;

    if (pa > pb) {
        return 1;
    }
    if (pa < pb) {
        return -1;
    }
    return 0;
}

/* -- hash functions -- */

uint64_t hash_string_fnv1a64(const void *str) {
    /* note that these values are NOT chosen randomly. Modifying them will break the function. */
    static const uint64_t FNV_offset_basis = 0xcbf29ce484222325;
    static const uint64_t FNV_prime = 0x100000001b3;

    uint64_t hash = FNV_offset_basis;
    const uint8_t *p = (const uint8_t *) str;

    while (*p) {
        /* FNV-1a hash differs from the FNV-1 hash only by the
         * order in which the multiply and XOR is performed */
        hash ^= (uint64_t) *p;
        hash *= FNV_prime;
        p++;
    }

    return hash;
}

/* -- character control -- */

int is_newline(int c) {
    return (c == '\n') ? 1 : 0;
}

int is_ascii_alnum(int c) {
    if (isascii(c) && isalnum(c)) {
        return 1;
    }
    return 0;
}

int is_space_or_par(int c) {
    switch (c) {
        case '(':
            return 1;
        case ')':
            return 1;
        default:
            return isspace(c);
    }
}

/* -- string control -- */

int is_digit_string(const char *str) {
    for (const char *c = str; *c != '\0'; c++) {
        if (!isdigit(*c)) {
            return 0;
        }
    }
    return 1;
}

int is_ascii_alpha_string(const char *str) {
    /* loop over string, get its length and verify that extension consists only of ascii chars */
    for (const char *c = str; *c != '\0'; c++) {
        if (!isascii(*c) || !isalpha(*c)) {
            return 0;
        }
    }
    return 1;
}

/* -- misc -- */

char *basename(const char *fpathlike) {
    char *s = strrchr(fpathlike, '/');

    if (s && ++s) {
        return s;
    }
    return (char *) fpathlike;
}

char *trim(char *str) {
    /* move past any leading whitespace */
    char *start = str;
    while (isspace(*start)) {
        start++;
    }

    /* edge case: empty string, or only spaces */
    if (*start == '\0') {
        *str = '\0';
        return str;
    }

    /* move to end of string */
    char *end = start;
    while (*end != '\0') {
        end++;
    }

    /**
     * This is safe as string is not empty, so there must be something before the null terminator.
     * Move backwards until the first non-space char, up to start, clearing any trailing whitespace
     */
    do {
        *end = '\0';
        end--;
    } while (isspace(*end));

    /* if there was leading whitespace, shift the entire string in-place */
    if (start != str) {
        for (char *s = str; *s != '\0'; s++, start++) {
            *s = *start;
        }
    }

    /* return the modified, trimmed string */
    return str;
}

int dir_exists(const char *path) {
    struct stat st;

    if (stat(path, &st) != 0) {
        /* path does not exist or stat failed. Either way, not a dir as far as we're concerned */
        return 0;
    }

    /* check if the path is a dir */
    return S_ISDIR(st.st_mode);
}

int mkdir_if_needed(const char *path) {
    /* extract directory from path */
    char *last_slash = strrchr(path, '/');
    char dir[PATH_MAX];

    if (!last_slash) {
        /* assume current directory */
        return 0;
    }

    size_t dir_len = last_slash - path;
    strncpy(dir, path, dir_len); // copy the directory part
    dir[dir_len] = '\0';

    if (!dir_exists(dir)) {
        /* 0755 permissions seems typical */
        if (mkdir(dir, 0755) == -1 && errno != EEXIST) {
            pr_error("Failed to create directory \"%s\": %s\n", dir, strerror(errno));
            return -1;
        }
    }

    return 0;
}

int redirect_stderr(const char *path) {
    int dst_fd;

    /* determine if terminal or file */
    if (strncmp(path, "/dev/", sizeof("/dev/") - 1) == 0) {
        /* all unix terminals begin with '/dev/' */
        dst_fd = open(path, O_WRONLY);
    } else {
        if (mkdir_if_needed(path) < 0) {
            return -1;
        }
        /* open the file in write mode, create it if it doesnt exist, and set permissions */
        dst_fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    }

    if (dst_fd < 0) {
        pr_error("Failed to access path \"%s\": %s\n", path, strerror(errno));
        return -1;
    }

    /* If succesful, stderr will now point to `dst_fd` */
    int fd2 = dup2(dst_fd, STDERR_FILENO);

    if (fd2 < 0) {
        pr_error("Failed to redirect stderr to \"%s\": %s\n", path, strerror(errno));
    }

    /* close the original fd. It's either duped and accessible through STDERR_FILENO, or the duplication
     * failed. Either way clean it up. */
    close(dst_fd);

    return fd2; // -1, or STDERR_FILENO
}

long fsize(FILE *f) {
    /* C with proper error handling is painful :-( */

    if (!f) {
        pr_error("Invalid file pointer\n");
        return -1; // Return -1 to indicate an error
    }

    long current_pos = ftell(f);
    if (current_pos == -1) {
        pr_error("ftell failed: %s\n", strerror(errno));
        return -1;
    }

    if (fseek(f, 0, SEEK_END) != 0) {
        pr_error("fseek failed @ SEEK_END: %s\n", strerror(errno));
        return -1;
    }

    long file_size = ftell(f);
    if (file_size == -1) {
        pr_error("ftell failed: %s\n", strerror(errno));
        return -1;
    }

    /* restore the file pointer to the original position */
    if (fseek(f, current_pos, SEEK_SET) != 0) {
        pr_error("fseek failed @ SEEK_SET: %s\n", strerror(errno));
        return -1;
    }

    // return the size of the file
    return file_size;
}
