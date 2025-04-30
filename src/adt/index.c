
#define LOG_LEVEL LOG_LEVEL_DEBUG

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h> 

#include "printing.h"
#include "index.h"
#include "defs.h"
#include "common.h"
#include "list.h"
#include "map.h"
#include "set.h"

typedef struct
{
    char *doc_name;
    size_t count;
} doc_entry_t;

struct index
{
    map_t *map;
    size_t amount_of_docs;
    size_t amount_of_terms;
};

typedef enum
{
    TERM,
    AND,
    OR,
    NOT
} ast_enums_t;

typedef struct anode
{
    ast_enums_t type;
    struct anode *left;
    struct anode *right;
    char *term;
} anode_t;

typedef struct read
{
    list_t *tokens;
    list_iter_t *iter;
    char *current;
} read_t;

static anode_t *handle_query(read_t *reader);
static anode_t *handle_and_term(read_t *reader);
static anode_t *handle_or_term(read_t *reader);
static anode_t *handle_term(read_t *reader);

anode_t *ast_create_term(char *term)
{
    anode_t *node = malloc(sizeof(anode_t));
    node->type = TERM;
    node->term = term;
    node->left = node->right = NULL;
    return node;
}


anode_t *ast_create(ast_enums_t type, anode_t *left, anode_t *right)
{
    anode_t *node = malloc(sizeof(anode_t));
    node->type = type;
    node->term = NULL;
    node->left = left;
    node->right = right;
    return node;
}

void ast_destroy(anode_t *node)
{
    if (!node)
        return;
    if (node->term)
        free(node->term);
    ast_destroy(node->left);
    ast_destroy(node->right);
    free(node);
}

// read_t *reader_create(list_t *tokens);


static void reader_iterate(read_t *reader)
{
    if (list_hasnext(reader->iter))
    {
        reader->current = list_next(reader->iter);
    }
    else
    {
        reader->current = NULL;
    }
}

read_t *reader_create(list_t *tokens)
{
    read_t *reader = malloc(sizeof(read_t));
    reader->tokens = tokens;
    reader->iter = list_createiter(tokens);
    reader->current = NULL;
    // reader_iterate(reader);
    reader->current = list_next(reader->iter);
    return reader;
}

void reader_destroy(read_t *reader)
{
    if (reader)
    {
        list_destroyiter(reader->iter);
        free(reader);
    }
}

static anode_t *handle_query(read_t *reader)
{
    anode_t *left_side = handle_and_term(reader);

    // pr_info("reader current inside query handle= %s", reader->current);
if (reader->current && strcmp(reader->current, "&!") == 0)
{
    // pr_info("Parsing AND NOT\n");
    reader_iterate(reader);
    anode_t *right_side = handle_query(reader);
    return ast_create(NOT, left_side, right_side);
}

    return left_side;
}

static anode_t *handle_and_term(read_t *reader)
{
    anode_t *left = handle_or_term(reader);
    // pr_info("reader current inside and term handle= %s", reader->current);
    while (reader->current && strcmp(reader->current, "&&") == 0)
    {
        reader_iterate(reader);
        anode_t *right = handle_or_term(reader);
        left = ast_create(AND, left, right);
    }
    return left;
}

static anode_t *handle_or_term(read_t *reader)
{
    anode_t *left = handle_term(reader);
    while (reader->current && strcmp(reader->current, "||") == 0)
    {
        reader_iterate(reader);
        anode_t *right = handle_term(reader);
        left = ast_create(OR, left, right);
    }
    return left;
}

static anode_t *handle_term(read_t *reader)
{
    if (reader->current && strcmp(reader->current, "(") == 0)
    {
        reader_iterate(reader); // skip '('
        anode_t *node = handle_query(reader);
        if (!reader->current || strcmp(reader->current, ")") != 0)
        {
            // handle error
            return NULL;
        }
        reader_iterate(reader); // skip ')'
        return node;
    }
    else if (reader->current)
    {
        char *word = strdup(reader->current);
        reader_iterate(reader);
        return ast_create_term(word);
    }
    return NULL;
}

set_t *evaluate_ast(index_t *index, anode_t *node, map_t *score_map)
{
    if (node == NULL)
        return NULL;

    if (node->type == TERM)
    {
        entry_t *entry = map_get(index->map, node->term);
        set_t *docs = set_create((cmp_fn)strcmp);
        if (!entry)
            return docs;

        list_t *doc_list = (list_t *)entry->val;
        list_iter_t *iter = list_createiter(doc_list);
        while (list_hasnext(iter))
        {
            doc_entry_t *doc = list_next(iter);
            set_insert(docs, doc->doc_name);

            if (score_map)
            {
                entry_t *score_entry = map_get(score_map, doc->doc_name);
                if (score_entry)
                {
                    double *score = (double *)score_entry->val;
                    *score += doc->count;
                }
                else
                {
                    double *new_score = malloc(sizeof(double));
                    *new_score = doc->count;
                    map_insert(score_map, strdup(doc->doc_name), new_score);
                }
            }
        }
        list_destroyiter(iter);
        return docs;
    }
    else if (node->type == AND)
    {
        set_t *left = evaluate_ast(index, node->left, score_map);
        set_t *right = evaluate_ast(index, node->right, score_map);
        set_t *result = set_intersection(left, right);
        set_destroy(left, NULL);
        set_destroy(right, NULL);
        return result;
    }
    else if (node->type == OR)
    {
        set_t *left = evaluate_ast(index, node->left, score_map);
        set_t *right = evaluate_ast(index, node->right, score_map);
        set_t *result = set_union(left, right);
        set_destroy(left, NULL);
        set_destroy(right, NULL);
        return result;
    }
    else if (node->type == NOT)
    {
        set_t *left = evaluate_ast(index, node->left, score_map);
        set_t *right = evaluate_ast(index, node->right, score_map);
        set_t *result = set_difference(left, right);
        set_destroy(left, NULL);
        set_destroy(right, NULL);
        return result;
    }

    return NULL;
}

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
    if (!index || !doc_name || !terms)
    {
        return -1;
    }

    if (!index->map)
    {
        printf("ERROR: index->map is NULL\n");
        exit(1);
    }

    list_iter_t *iterator = list_createiter(terms);
    if (iterator == NULL)
    {
        pr_error("Failed to create list iterator\n");
        return -1;
    }

    while (list_hasnext(iterator))
    {
        char *term = (char *)list_next(iterator);
        if (!term)
            continue;

        entry_t *entry = map_get(index->map, term);
        list_t *doc_list = entry ? (list_t *)entry->val : NULL;

        if (!doc_list)
        {
            doc_list = list_create(NULL);
            if (!doc_list)
            {
                pr_error("Failed to create document list\n");
                list_destroyiter(iterator);
                return -1;
            }
            map_insert(index->map, strdup(term), doc_list);
            index->amount_of_terms++;
        }

        list_iter_t *doc_iter = list_createiter(doc_list);
        doc_entry_t *found = NULL;
        while (list_hasnext(doc_iter))
        {
            doc_entry_t *doc = (doc_entry_t *)list_next(doc_iter);
            if (strcmp(doc->doc_name, doc_name) == 0)
            {
                found = doc;
                break;
            }
        }
        list_destroyiter(doc_iter);

        if (found)
        {
            found->count++;
        }
        else
        {
            doc_entry_t *new_doc = malloc(sizeof(doc_entry_t));
            new_doc->doc_name = strdup(doc_name);
            new_doc->count = 1;
            list_addlast(doc_list, new_doc);
        }
    }

    list_destroyiter(iterator);
    index->amount_of_docs++;
    return 0;
}

list_t *index_query(index_t *index, list_t *query_tokens, char *errbuf)
{

    read_t *reader = reader_create(query_tokens);
    if (!reader)
    {
        snprintf(errbuf, LINE_MAX, "Failed to create reader");
        return NULL;
    }

    anode_t *ast = handle_query(reader);
    reader_destroy(reader);

    if (!ast)
    {
        snprintf(errbuf, LINE_MAX, "Invalid query");
        return NULL;
    }

    // Get set of matching documents only
    set_t *result_docs = evaluate_ast(index, ast, NULL);
    ast_destroy(ast);

    if (!result_docs)
    {
        snprintf(errbuf, LINE_MAX, "Failed to evaluate AST");
        return NULL;
    }

    // Prepare to score only query terms
    map_t *score_map = map_create((cmp_fn)strcmp, (hash64_fn)hash_string_fnv1a64);
    list_iter_t *query_iter = list_createiter(query_tokens);

    while (list_hasnext(query_iter))
    {
        char *token = list_next(query_iter);
        // Skip operators and parens
        if (!token || strcmp(token, "&&") == 0 || strcmp(token, "||") == 0 ||
            strcmp(token, "!&") == 0 || strcmp(token, "(") == 0 || strcmp(token, ")") == 0)
        {
            continue;
        }

        entry_t *entry = map_get(index->map, token);
        if (!entry)
            continue;
        list_t *docs = (list_t *)entry->val;

        list_iter_t *doc_iter = list_createiter(docs);
        while (list_hasnext(doc_iter))
        {
            doc_entry_t *doc = list_next(doc_iter);
            if (!set_get(result_docs, doc->doc_name))
                continue; // only score matching docs

            entry_t *score_entry = map_get(score_map, doc->doc_name);
            if (score_entry)
            {
                double *score = (double *)score_entry->val;
                *score += doc->count;
            }
            else
            {
                double *new_score = malloc(sizeof(double));
                *new_score = doc->count;
                map_insert(score_map, strdup(doc->doc_name), new_score);
            }
        }
        list_destroyiter(doc_iter);
    }
    list_destroyiter(query_iter);
    set_destroy(result_docs, NULL);

    // Build final results list
    list_t *results = list_create(NULL);
    if (!results)
    {
        snprintf(errbuf, LINE_MAX, "Failed to create results list");
        return NULL;
    }

    map_iter_t *score_iter = map_createiter(score_map);
    while (map_hasnext(score_iter))
    {
        entry_t *entry = map_next(score_iter);
        if (!entry || !entry->key || !entry->val)
            continue;

        query_result_t *result = malloc(sizeof(query_result_t));
        result->doc_name = strdup((char *)entry->key);
        result->score = *(double *)entry->val;
        list_addlast(results, result);
    }

    map_destroyiter(score_iter);
    map_destroy(score_map, NULL, free);

    return results;
}
void index_stat(index_t *index, size_t *n_docs, size_t *n_terms)
{
    if (!index || !n_docs || !n_terms)
    {
        return;
    }

    *n_docs = index->amount_of_docs;
    *n_terms = index->amount_of_terms;
}