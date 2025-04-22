/**
 * @authors
 * Odin Bjerke <odin.bjerke@uit.no>
 */

#ifndef FINDFILES_H
#define FINDFILES_H

#include <stddef.h> // for size_t

#include "list.h"
#include "set.h"

/**
 * @brief Recursively find files from the given directory path
 * @param dst: list to add file paths to
 * @param valid_exts: nullable. If present, include only files with a extension in this set
 * @param n_files_max: if > 0, limit to this number of files. the files will stem from the 'last' directories
 * traversed
 * @returns 0 on success, otherwise a negative error code
 */
int find_files(const char *dir_path, list_t *dst, set_t *valid_exts, size_t n_files_max);

#endif /* FINDFILES_H */
