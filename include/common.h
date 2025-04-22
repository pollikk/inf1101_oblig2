/**
 * @authors
 * Odin Bjerke <odin.bjerke@uit.no>
 */

#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>

#include "defs.h"

/**
 * @brief Compare two integers.
 * @param a,b: pointer to integer
 * @returns see `cmp_fn`
 */
int compare_integers(const int *a, const int *b);

/**
 * @brief Compare two characters. If strings are passed, only the first character is compared
 * @param a,b: pointer to character
 * @returns see `cmp_fn`
 */
int compare_characters(const char *a, const char *b);

/**
 * @brief Compare two pointers (by memory address)
 * @param a,b: pointer
 * @returns see `cmp_fn`
 */
int compare_pointers(const void *a, const void *b);

/**
 * @brief Fowler-Noll-Vo (FNV-1a) hash algorithm for strings, 64-bit variant
 * @param str: null-terminated string
 * @returns The 64 bit hash of `str`
 * @note see [this](https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function#FNV-1a_hash)
 * page for further information on the algorithm.
 */
uint64_t hash_string_fnv1a64(const void *str);

/**
 * @param c: character-type integer
 * @returns a positive integer if character is a newline, otherwise 0
 */
int is_newline(int c);

/**
 * @param c: character-type integer
 * @returns a positive integer if character is an ascii alphanumeric value, otherwise 0
 */
int is_ascii_alnum(int c);

/**
 * @param c: character-type integer
 * @returns 1 if character is whitespace or a parenthesis, otherwise 0
 */
int is_space_or_par(int c);

/**
 * @param str: null-terminated string
 * @returns a positive integer if string consits only of digits, otherwise 0
 */
int is_digit_string(const char *str);

/**
 * @param str: null-terminated string
 * @returns a positive integer if string consits only of alphabetic ascii characters
 */
int is_ascii_alpha_string(const char *str);

/**
 * @returns 1 if the path points to an existing directory, otherwise 0
 */
int dir_exists(const char *path);

/**
 * @brief create directory to 'path' if it does not exist
 * @param path: path to file or directory
 * @note maximum depth 1, does not recur
 */
int mkdir_if_needed(const char *path);

/**
 * @brief get the size of a file. Restores the streams position.
 * @param f: pointer to file/stream
 * @returns -1 on failure, otherwise the total size of the file (bytes)
 */
long fsize(FILE *f);

/**
 * @param fpathlike: filepath-like string. Does not need to exist on disk.
 * @returns a pointer to final component of the path (e.g. "mypath/myfile.png" -> "myfile.png")
 */
char *basename(const char *fpathlike);

/**
 * @brief trim leading and trailing whitespace from given string.
 * The string is modified in-place.
 * @param str: null terminated string
 * @returns a pointer to the trimmed string. (in fact, str now points to the trimmed string, regardless of
 * whether the returned pointer is kept or not)
 */
char *trim(char *str);

/**
 * @brief Redirects stderr to a file or different terminal (inferred from '/dev/' or not)
 * If given a path to a file, it will be created if it does not exist. Any existing content will be erased.
 * @param path: path to a file or terminal
 * @returns STDERR_FILENO on success, otherwise a negative error code
 */
int redirect_stderr(const char *path);


#endif /* COMMON_H */
