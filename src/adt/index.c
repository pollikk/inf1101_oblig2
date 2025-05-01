
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

struct index
{
    // Struktur for den inverterte indexen
    map_t *map;
    size_t amount_of_docs;
    size_t amount_of_terms;
};

typedef struct ast_node
{
    // Struktur for hver node i abstrakt syntax treet
    ast_enums_t type;
    struct ast_node *left;
    struct ast_node *right;
    char *term;
} ast_node_t;

typedef struct read
{
    // struktur for å lese tokens fra index_query for å kunne iterere gjennom
    list_t *tokens;
    list_iter_t *iter;
    char *current;
} parse_t;

// deklarerer funkjsoner som skal brukes
ast_node_t *handle_and(parse_t *parser);
ast_node_t *handle_or(parse_t *parser);
ast_node_t *handle_term(parse_t *parser);

ast_node_t *ast_create_term(char *term)
{
    // Funksjonen er av typen ast_node_t og forventer samme type i retur. Den tar inn argumentet term som er en string.
    // Funksjonen oppretter en ny node og setter typen til TERM og noden sin data blir satt til term fra argumentet
    ast_node_t *node = malloc(sizeof(ast_node_t));
        if(node == NULL){
        pr_error("failed to allocate memory!\n");
    }
    node->type = TERM;
    node->term = term;
    return node;
}

ast_node_t *ast_create(ast_enums_t type, ast_node_t *left, ast_node_t *right)
{
    // Funksjonen er av typen ast_node_t og forventer samme type tilbake i returverdi. Den tar inn tre argumenter; type, left og right.
    //  typen er hvilken type operasjon som blir foretatt. left og right er for pekeren til nodene under i tre strukturen.
    ast_node_t *node = malloc(sizeof(ast_node_t));
    if(node == NULL){
        pr_error("failed to allocate memory!\n");
    }
    node->type = type;
    node->term = NULL;
    node->left = left;
    node->right = right;
    return node;
}

void parser_iterate(parse_t *parser)
{
    // funksjonen er av typen void og forventer ingen returverdi. Den skjekker om parser fra argumentet har nådd slutten av listen.
    //  dersom dette stemmer gjør den parseren sin current verdi om til null. Ellers itererer den et steg fram i listen og setter current
    //  til å være lik det neste i listen.
    if (list_hasnext(parser->iter))
    {
        parser->current = list_next(parser->iter);
    }
    else
    {
        parser->current = NULL;
    }
}

parse_t *parser_create(list_t *tokens)
{
    // funksjonen er av typen parse_t og tar inn et argument som er en peker til en liste. Funksjonen oppretter en ny parse_t struktur
    // og setter inn listen fra argumentet inn i parser sin tokens verdi. Den setter deretter opp iterator for parser og setter current verdien.
    // til slutt returneres den nye opprettede parseren.
    parse_t *parser = malloc(sizeof(parse_t));
    if(parser == NULL){
        pr_error("failed to allocate memory!\n");
    }
    parser->tokens = tokens;
    parser->iter = list_createiter(tokens);
    parser->current = list_next(parser->iter);
    return parser;
}

ast_node_t *handle_not(parse_t *parser)
{
    // funksjonen er av typen ast_node_t og forventer samme returverdi. Den tar inn en parser som argument.
    // funksjonen fungerer ved at det først opprettes en ny node peker for venstre siden av uttrykket.
    //  Deretter skjekkes det om parseren sin current er lik &! ved bruk av strcmp funksjonen. Dersom dette stemmer er strcmp(parser->current,&!) lik 0.
    //  dersom dette stemmer settes det opp en ny node rekursivt med typen NOT som kombinerer høyre og venstre side. Dersom parser sin current
    //  ikke er lik &! vil venstre siden av uttrykket returneres.
    ast_node_t *left_side = handle_and(parser);
    if (parser->current && strcmp(parser->current, "&!") == 0)
    {
        parser_iterate(parser);
        ast_node_t *right_side = handle_not(parser);
        ast_node_t *anode = ast_create(NOT, left_side, right_side);
        return anode;
    }
    return left_side;
}

ast_node_t *handle_and(parse_t *parser)
{
    // Funksjonen fungerer på samme måte som handle_not men ved bruk av AND som enum og && som operator term
    ast_node_t *left = handle_or(parser);
    while (parser->current && strcmp(parser->current, "&&") == 0)
    {
        parser_iterate(parser);
        ast_node_t *right = handle_or(parser);
        left = ast_create(AND, left, right);
    }
    return left;
}

ast_node_t *handle_or(parse_t *parser)
{
    // Funksjonen fungerer på samme måte som handle_not men ved bruk av OR som enum og ||  som operator term
    ast_node_t *left = handle_term(parser);
    while (parser->current && strcmp(parser->current, "||") == 0)
    {
        parser_iterate(parser);
        ast_node_t *right = handle_term(parser);
        left = ast_create(OR, left, right);
    }
    return left;
}

ast_node_t *handle_term(parse_t *parser)
{

    // funksjonen er av typen ast_node_t og forventer samme returverdi. Den tar inn en parse_t som argument.
    // Funksjonen parser et enkelt ord eller et gruppert uttrykk. Dersom parser->current er lik "(", betyr det at en gruppert
    // delspørring starter. Da kalles handle_not() rekursivt for å bygge en AST for hele uttrykket inni parentesene.
    // Etter det itereres parseren videre for å hoppe over ")" og noden returneres. Hvis tokenet ikke er en parantes blir det
    // behandlet som et enkelt søkeord og returneres ved å opprette en ny TERM-node.
    if (parser->current && strcmp(parser->current, "(") == 0)
    {
        parser_iterate(parser);
        ast_node_t *anode = handle_not(parser);
        parser_iterate(parser);
        return anode;
    }
    else if (parser->current)
    {
        char *word = strdup(parser->current);
        parser_iterate(parser);
        ast_node_t *anode = ast_create_term(word);
        return anode;
    }
    return NULL;
}

set_t *evaluate_ast(index_t *index, ast_node_t *node, map_t *score_map)
{
    // funksjonen er av typen set_t og forventer et set i retur. Den tar inn tre argumenter: index, node og score map. Index er den
    // inverterte indexen sendt inn fra index_query. Funksjonene begynner med å sjekke nodetypen for å velge hvilken operasjoner den skal
    //  gjøre. Dersom nodetypen er en TERM vil dette si at det kun er et enkeltord som skal prossesseres. Her vil det da den inverterte indexen
    //  hentes og det vil opprettes et set for å lagre dokumentene som inneholder søkeordet. Dersom ingen dokumenter finnes vil det returneres
    // et tomt set. Dersom det finnes, vil det itereres over dokumenetene og legge hvert dokumentnavn til i settet. Til slutt blir scoren incrementet
    //  og settet blir returnert.
    //  dersom det ikke er et enkelt ord men en operasjon som skal utføres vil det først sjekkes hvilken operasjon det er. Dette blir gjort ved bruk
    //  av set sine funksjoner; set_intersection for AND, set_union for OR og set_difference for dokumenter i venstre side, men ikke høyre.

    if (node->type == TERM)
    {
        entry_t *entry = map_get(index->map, node->term);
        set_t *docs = set_create((cmp_fn)strcmp);

        if (entry == NULL)
        {
            return docs;
        }

        list_t *doc_list = (list_t *)entry->val;
        list_iter_t *iter = list_createiter(doc_list);
        while (list_hasnext(iter))
        {
            query_result_t *doc = list_next(iter);
            set_insert(docs, doc->doc_name);

            if (score_map != NULL)
            {
                entry_t *score_entry = map_get(score_map, doc->doc_name);
                if (score_entry)
                {
                    double *score = (double *)score_entry->val;
                    *score += doc->score;
                }
                else
                {
                    double *new_score = malloc(sizeof(double));
                    *new_score = doc->score;
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
        return result;
    }
    else if (node->type == OR)
    {
        set_t *left = evaluate_ast(index, node->left, score_map);
        set_t *right = evaluate_ast(index, node->right, score_map);
        set_t *result = set_union(left, right);
        return result;
    }
    else if (node->type == NOT)
    {
        set_t *left = evaluate_ast(index, node->left, score_map);
        set_t *right = evaluate_ast(index, node->right, score_map);
        set_t *result = set_difference(left, right);
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
void print_list_of_strings(const char *descr, list_t *tokens)
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
    // funkjsonen er av typen index_t og forventer index_t returverdi. Funkjsonen setter opp verdiene til en tom index og setter hvilke
    //  compare functions den skal ha i sin struktur.
    index_t *index = malloc(sizeof(index_t));
    if (index == NULL)
    {
        pr_error("Failed to allocate memory for index\n");
        return NULL;
    }
    index->map = map_create((cmp_fn)strcmp, (hash64_fn)hash_string_fnv1a64);
    index->amount_of_docs = 0;
    index->amount_of_terms = 0;
    return index;
}

void index_destroy(index_t *index)
{
    // Destroys the index sent in as argument
    index->amount_of_docs = 0;
    index->amount_of_terms = 0;
    map_destroy(index->map, NULL, NULL);
    free(index);
}

int index_document(index_t *index, char *doc_name, list_t *terms)
{
    // funksjonen er av typen int og forventer en integer i retur. Den tar inn tre argumenter index, doc_name og terms. Den fungerer ved å
    //  først opprette en iterator av listen terms. Deretter opprettes en while løkke som itererer over iteratoren. for hver iterasjon
    // hentes et nytt term ut og map_get brukes for å finne ut om termen allerede finnes i indexen. Dersom den gjør det, hentes tilhørende
    // dokumentliste. Hvis termen ikke finnes, opprettes en ny dokumentliste og legges inn i map sammen med termen som nøkkel.
    //  Deretter opprettes en ny iterator for dokumentlisten for å se om dokumentet allerede finnes i listen. Hvis det gjør det, økes scoren.
    //  Hvis det ikke finnes, opprettes en ny query_result_t og legges til i dokumentlisten. Når alle terms er behandlet økes dokumentteller.
    if (index == NULL || doc_name == NULL || terms == NULL)
    {
        perror("Index, doc_name or terms == NULL!\n");
        return -1;
    }

    list_iter_t *iterator = list_createiter(terms);
    while (list_hasnext(iterator))
    {
        char *term = (char *)list_next(iterator);

        entry_t *entry = map_get(index->map, term);
        list_t *doc_list = NULL;
        if (entry != NULL)
        {
            doc_list = (list_t *)entry->val;
        }
        if (doc_list == NULL)
        {
            doc_list = list_create(NULL);
            map_insert(index->map, strdup(term), doc_list);
            index->amount_of_terms++;
        }

        list_iter_t *doc_iter = list_createiter(doc_list);
        query_result_t *found = NULL;
        while (list_hasnext(doc_iter))
        {
            query_result_t *doc = (query_result_t *)list_next(doc_iter);
            if (strcmp(doc->doc_name, doc_name) == 0)
            {
                found = doc;
                break;
            }
        }
        if (found != NULL)
        {
            found->score++;
        }
        else
        {
            query_result_t *new_doc = malloc(sizeof(query_result_t));
            if (new_doc == NULL)
            {
                pr_error("failed to allocate memory!\n");
                return -1;
            }
            new_doc->doc_name = strdup(doc_name);
            new_doc->score = 1;
            list_addlast(doc_list, new_doc);
        }
    }
    index->amount_of_docs++;
    return 0;
}

list_t *index_query(index_t *index, list_t *query_tokens, char *errbuf)
{
    // funksjonen er av typen list_t og forventer samme returverdi. Den tar inn tre argumenter
    // index som er den inverterte indexen, query_tokens som er en liste med tokens fra spørringen, og errbuf som er en buffer for feilmeldinger.
    // først opprettes en parser, basert på token listen og deretter bygges det opp et abstrakt syntax tre ved hjelp av handle_not.
    // dette ASTet evalueres for å finne hvilke dokumenter som matcher, og dette lagres i result_docs.
    // så opprettes en score_map for å kunne rangere resultatene. Deretter itereres det over hvert token i spørringen for å finne hvilke
    // dokumenter som inneholder hvert søkeord, og scorer disse dersom de finnes i result_docs.
    // Til slutt opprettes en liste med query_result_t for hver dokument, hvor scorene hentes fra score_map, og returneres som resultat.
    parse_t *parser = parser_create(query_tokens);
    ast_node_t *ast = handle_not(parser);
    set_t *result_docs = evaluate_ast(index, ast, NULL);
    map_t *score_map = map_create((cmp_fn)strcmp, (hash64_fn)hash_string_fnv1a64);
    list_iter_t *query_iter = list_createiter(query_tokens);

    while (list_hasnext(query_iter))
    {

        char *token = list_next(query_iter);
        entry_t *entry = map_get(index->map, token);
        if (entry == NULL)
        {
            continue;
        }

        list_t *docs = (list_t *)entry->val;

        list_iter_t *doc_iter = list_createiter(docs);
        while (list_hasnext(doc_iter))
        {
            query_result_t *doc = list_next(doc_iter);
            if (set_get(result_docs, doc->doc_name) == NULL)
            {
                continue;
            }

            entry_t *score_entry = map_get(score_map, doc->doc_name);
            if (score_entry != NULL)
            {
                double *score = score_entry->val;
                *score += doc->score;
            }
            else
            {
                double *new_score = malloc(sizeof(double));
                *new_score = doc->score;
                map_insert(score_map, doc->doc_name, new_score);
            }
        }
    }

    list_t *results = list_create(NULL);
    if (results == NULL)
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

    return results;
}
void index_stat(index_t *index, size_t *n_docs, size_t *n_terms)
{
    // funksjonen er av typen void og returnerer derfor ingenting. Den tar inn tre argumenter en peker til index-strukturen,
    // og to pekere til variabler der antall dokumenter og antall unike termer skal lagres.
    // funksjonen begynner med å sjekke om noen av pekerne er NULL. Hvis dette stemmer returneres det direkte uten å gjøre noe mer.
    // hvis alt er gyldig, settes verdiene til n_docs og n_terms ved å hente tallene direkte fra index-strukturen.

    if (index == NULL || n_docs == NULL || n_terms == NULL)
    {
        return;
    }

    *n_docs = index->amount_of_docs;
    *n_terms = index->amount_of_terms;
}