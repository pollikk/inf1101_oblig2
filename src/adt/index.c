/**
 * @implements index.h
 */

/* set log level for prints in this file */
#define LOG_LEVEL LOG_LEVEL_DEBUG

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h> // for LINE_MAX

#include "printing.h"
#include "index.h"
#include "defs.h"
#include "common.h"
#include "list.h"
#include "map.h"
#include "set.h"

struct index
{
    map_t *map;
    size_t amount_of_docs;
    size_t amount_of_terms;
};
int variable = 0;

/**
 * You may utilize this for lists of query results, or write your own comparison function.
 */
ATTR_MAYBE_UNUSED
int compare_results_by_score(query_result_t *a, query_result_t *b)
{
    if (a->score > b->score)
    {
        return -1;
    }
    if (a->score < b->score)
    {
        return 1;
    }
    return 0;
}

/**
 * @brief debug / helper to print a list of strings with a description.
 * Can safely be removed, but could be useful for debugging/development.
 *
 * Remove this function from your finished program once you are done
 */
ATTR_MAYBE_UNUSED
static void print_list_of_strings(const char *descr, list_t *tokens)
{
    if (LOG_LEVEL <= LOG_LEVEL_INFO)
    {
        return;
    }
    list_iter_t *tokens_iter = list_createiter(tokens);
    if (!tokens_iter)
    {
        /* this is not a critical function, so just print an error and return. */
        pr_error("Failed to create iterator\n");
        return;
    }

    pr_info("\n%s:", descr);
    while (list_hasnext(tokens_iter))
    {
        char *token = (char *)list_next(tokens_iter);
        pr_info("\"%s\", ", token);
    }
    pr_info("\n");

    list_destroyiter(tokens_iter);
}

index_t *index_create()
{
    index_t *index = malloc(sizeof(index_t));
    if (index == NULL)
    {
        pr_error("Failed to allocate memory for index\n");
        return NULL;
    }
    index->map = map_create((cmp_fn)strcmp, (hash64_fn)hash_string_fnv1a64);
    index->amount_of_docs = 0;
    index->amount_of_terms = 0;

    /**
     * TODO: Allocate, initialize and set up nescessary structures
     */

    return index;
}

void index_destroy(index_t *index)
{
    // during development, you can use the following macro to silence "unused variable" errors.
    UNUSED(index);

    /**
     * TODO: Free all memory associated with the index
     */
}

int index_document(index_t *index, char *doc_name, list_t *terms)
{
    /**
     * TODO: Process document, enabling the terms and subsequent document to be found by index_query
     *
     * Note: doc_name and the list of terms is now owned by the index. See the docstring.
     *
     */
    list_iter_t *iterator = list_createiter(terms);
    if (iterator == NULL)
    {
        printf("iterator = Null!");
    }

    while (list_hasnext(iterator))
    {
        char *term = (char *)list_next(iterator);
        list_t *document_list = NULL;
        if (index == NULL || index->map == NULL)
        {
            // document_list = (list_t *)map_get(index->map, term);
            pr_error("index or index->map is NULL!\n");
            return -1;
        }
        if (document_list == NULL)
        {
            document_list = list_create((cmp_fn)strcmp);
            if (document_list == NULL)
            {
                pr_error("Failed to make document list!\n");
                return -1;
            }
            map_insert(index->map, term, document_list);
            index->amount_of_terms++;
            // pr_info("amount of terms = %zu", index->amount_of_terms);
            // pr_info("amount of terms = %zu \n", index->amount_of_terms);
        }
        list_iter_t *document_iterator = list_createiter(document_list);
        int document_exists = 0;
        while (list_hasnext(document_iterator))
        {
            char *exists = (char *)list_next(document_iterator);
            if (strcmp(exists, doc_name) == 0)
            {
                document_exists = 1;
                break;
            }
        }
        if (document_exists == 0)
        {
            list_addlast(document_list, doc_name);
        }
    }

    index->amount_of_docs++;
    // pr_info("amount of docs = %zu \n", index->amount_of_docs);
    return 0;
}

list_t *index_query(index_t *index, list_t *query_tokens, char *errmsg)
{
    print_list_of_strings("query", query_tokens); // remove this if you like

    /**
     * TODO: perform the search, and return:
     * query is invalid => write reasoning to errmsg and return NULL
     * query is valid   => return list with any results (empty if none)
     *
     * Tip: `snprintf(errmsg, LINE_MAX, "...")` is a handy way to write to the error message buffer as you
     * would do with a typical `printf`. `snprintf` does not print anything, rather writing your message to
     * the buffer.
     */

// hvis index eller query_tokens er NULL:
//     skriv feilmelding
//     returner NULL

// lag en tom map: dokumentnavn -> score

// for hvert token i query_tokens:
//     hent listen av dokumenter som inneholder token fra index->map
//     for hvert dokument i listen:
//         hvis dokumentet allerede finnes i score_map:
//             øk score med 1
//         ellers:
//             legg inn dokumentet i score_map med score = 1

// lag en tom resultat-liste

// for hver entry i score_map:
//     lag en query_result_t med dokumentnavn og score
//     legg til i resultatlisten

// sorter resultatlisten på score synkende

// returner resultatlisten


    (void)index;
    (void)errmsg;
    (void)query_tokens;

    return NULL; // TODO: return list of query_result_t objects instead
}

void index_stat(index_t *index, size_t *n_docs, size_t *n_terms)
{
    /**
     * TODO: fix this
     */
    (void)index;
    (void)n_docs;
    (void)n_terms;
    *n_docs = 0;
    *n_terms = 0;
}
