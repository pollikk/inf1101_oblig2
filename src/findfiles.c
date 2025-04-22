/**
 * @authors
 * Odin Bjerke <odin.bjerke@uit.no>
 */

#include <string.h>
#include <errno.h>
#include <limits.h>
#include <unistd.h>
#include <dirent.h>

#include <sys/stat.h>

#include "printing.h"
#include "defs.h"
#include "list.h"
#include "set.h"


int find_files(const char *dir_path, list_t *dst, set_t *valid_exts, size_t n_files_max) {
    DIR *dir = opendir(dir_path);
    if (!dir) {
        pr_error("Failed to open directory \"%s\": %s\n", dir_path, strerror(errno));
        return -1;
    }

    struct stat path_stat;
    char full_path[PATH_MAX];
    full_path[0] = '\0';
    int status = 0;

    while (status == 0) {
        /* check if we reached file limit */
        if (n_files_max && (list_length(dst) >= n_files_max)) {
            break;
        }

        /* get the next entry from this directory */
        struct dirent *entry = readdir(dir);
        if (!entry) {
            break; // failed, or no more entries. Either way, break out.
        }

        /* don't follow symlinks */
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        /* Copy directory path */
        size_t dir_len = strlen(dir_path);
        if (dir_len + 2 + strlen(entry->d_name) >= PATH_MAX) {
            pr_warn("Path length exceeded maximum limit. Ignoring entry: %s/%s\n", dir_path, entry->d_name);
            continue;
        }
        /* we already checked length, so no need to do strncpy here */
        strcpy(full_path, dir_path);
        full_path[dir_len] = '/';                       // Add '/' separator
        strcpy(full_path + dir_len + 1, entry->d_name); // Copy file name

        /* might occur if we don't have read access, etc */
        if (stat(full_path, &path_stat) == -1) {
            pr_warn("Failed to access path %s (err: %s). Ignoring.\n", full_path, strerror(errno));
            continue;
        }

        if (S_ISDIR(path_stat.st_mode)) {
            /* for dirs: recursive call */
            status = find_files(full_path, dst, valid_exts, n_files_max);
        } else if (S_ISREG(path_stat.st_mode)) {
            /* for files: check if we should include this file extension */
            if (valid_exts) {
                /* pointer to last occurance of '.' in path, or NULL if not present */
                char *ext = strrchr(full_path, '.');

                /* continue on ext == NULL. Otherw */
                if (!ext || (*(++ext) == '\0') || !set_get(valid_exts, ext)) {
                    continue;
                }
            }

            char *path_cpy = strdup(full_path);
            if (!path_cpy) {
                pr_error("Malloc failed (in strdup)\n");
                return -1;
            }

            /* since we're doing tail recursion, insert first to organize as files appear in the folder */
            status = list_addfirst(dst, path_cpy);
        }
    }

    closedir(dir);

    return status;
}
