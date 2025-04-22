#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <limits.h>

#include "common.h"
#include "printing.h"
#include "defs.h"
#include "logger.h"

struct logger {
    FILE *f;
    char *path;
    int status;
};

/* Opens the logfile at logfile_pathbuf. Returns 0 on success, -1 on failure. */
static int logfile_open_from_pathbuf(logger_t *logger) {
    /* create dir if it does not exist */
    if (mkdir_if_needed(logger->path) < 0) {
        return -1;
    }

    if (logger->f) {
        fclose(logger->f);
        logger->f = NULL;
    }

    logger->f = fopen(logger->path, "a+");
    if (!logger->f) {
        pr_error("Failed to open logfile at %s: %s\n", logger->path, strerror(errno));
        return -1;
    }

    return 0;
}

/* Writes to the logfile, attempting to reopen if the initial write fails */
static int write_with_retry(logger_t *logger, const char *buf) {
    int status = fputs(buf, logger->f);
    if (status >= 0) {
        return 0;
    }

    pr_error("Failed to write to logfile: %s. Attempting to reopen.\n", strerror(errno));

    if (logfile_open_from_pathbuf(logger) == 0) {
        status = fputs(buf, logger->f);
        if (status >= 0) {
            return 0;
        }
    }

    pr_error("Failed to write to logfile after reopening: %s\n", strerror(errno));
    return -1;
}

void logger_destroy(logger_t *logger) {
    if (!logger) {
        return;
    }
    if (logger->f) {
        fclose(logger->f);
    }
    free(logger->path);
    free(logger);
}

logger_t *logger_create(const char *path) {
    logger_t *logger = malloc(sizeof(logger_t));
    if (!logger) {
        pr_error("Malloc failed: %s\n", strerror(errno));
        return NULL;
    }

    if (!path || *path == '\0') {
        pr_error("Invalid path: \"%s\"\n", path);
        free(logger);
        return NULL;
    }

    size_t path_len = strlen(path);
    if (path_len >= (PATH_MAX - 1)) {
        pr_error("Path to logfile \"%s\" is too long\n", path);
        free(logger);
        return NULL;
    }

    logger->status = 0;
    logger->path = strdup(path);

    if (logfile_open_from_pathbuf(logger) < 0) {
        free(logger->path);
        free(logger);
        return NULL;
    }

    return logger;
}

void logger_flush(logger_t *logger) {
    fflush(logger->f);
}

int logger_write_buf(logger_t *logger, const char *buf) {
    return write_with_retry(logger, buf);
}
