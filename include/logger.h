/**
 * @authors
 * Odin Bjerke <odin.bjerke@uit.no>
 *
 * @brief simple clib wrappers to handle logging
 */

#ifndef LOGGER_H
#define LOGGER_H

typedef struct logger logger_t;

/**
 * @brief initialize a logger.
 * @param path: path to file. Will be created if it does not exist, along with the directory. Content will be
 * appended if the file exists.
 * @returns pointer logger on success, otherwise NULL
 */
logger_t *logger_create(const char *path);

/**
 * @brief write to the logger if one is given.
 * @param buf: null terminated string
 * @param logger: pointer to logger
 * @returns 0 on success, otherwise -1
 * @note this will attempt to reopen the associated file if the write fails for any reason. If -1 is returned,
 * this means the reopen failed.
 */
int logger_write_buf(logger_t *logger, const char *buf);

/**
 * @brief Destroy a logger, closing the related stream.
 * @param logger: pointer to logger
 * @note Does nothing if logger is NULL.
 */
void logger_destroy(logger_t *logger);

/**
 * @brief flush the logger, writing any pending
 * @param logger: pointer to logger
 */
void logger_flush(logger_t *logger);

#endif /* LOGGER_H */
