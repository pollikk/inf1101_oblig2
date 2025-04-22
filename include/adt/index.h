/**
 * @authors
 * Odin Bjerke <odin.bjerke@uit.no>
 */

#ifndef INDEX_H
#define INDEX_H

#include <stddef.h> // for size_t

#include "defs.h"
#include "list.h"

/**
 * Type of index. `index_t` is an alias for `struct index_`
 */
typedef struct index index_t;

/**
 * Type of query_result produced by a index query.
 * Higher score implies the document is more relevant.
 */
typedef struct query_result {
    char *doc_name;
    double score;
} query_result_t;

/**
 * @brief Create a new index
 * @returns a pointer to the newly allocated index, or NULL on failure
 */
index_t *index_create();

/**
 * @brief Destroy the given index, freeing all related resources.
 * @param index: pointer to index
 * @note this is safe to call with `index` == NULL, where it simply returns
 */
void index_destroy(index_t *index);

/**
 * @brief Index a document and its words
 *
 * @param index: pointer to index
 * @param doc_name: distinct reference to a document or file
 * @param words: list of words (terms), exactly as they appear in the document
 * @returns 0 if the operation succeeded, otherwise a negative status code
 *
 * @note The passed doc_name and list of words (including the allocated strings contained within this list) is
 * owned by the callee (index) from this point.
 *
 * The index may utilize these items further, or free them right away. They must be freed by the index at the
 * very latest during index_destroy.
 */
int index_document(index_t *index, char *doc_name, list_t *words);

/**
 * @brief Search the index for documents that match the query
 *
 * @param index: pointer to index
 * @param query_tokens: ordered list of strings representing individual query tokens
 * @param errbuf: Caller-provided buffer to write error messages to (min. buffer size = LINE_MAX)
 *
 * @returns NULL if the query was malformed or otherwise invalid, otherwise a list containing 0..n
 * `query_result_t` structs, sorted by score in descending order. If the return value is NULL, `errbuf` should
 * be set to a string with reasoning. (e.g. "expected term after <some_token>, found operator <other_token>")
 *
 * @note the index may remove strings from the given list of tokens, as long as they are cleaned up (freed) by
 * the index. The list itself should not be destroyed.
 */
list_t *index_query(index_t *index, list_t *query_tokens, char *errbuf);

/**
 * @brief Get the number of unique documents and terms that have been indexed
 * @param n_docs: pointer to size_t - must be set to the number of docs
 * @param n_docs: pointer to size_t - must be set to the number of unique terms
 */
void index_stat(index_t *index, size_t *n_docs, size_t *n_terms);


#endif /* INDEX_H */
