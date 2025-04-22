/**
 * @authors
 * Odin Bjerke <odin.bjerke@uit.no>
 */

/* set log level for prints in this file */
#define LOG_LEVEL LOG_LEVEL_DEBUG

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <limits.h>
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/ioctl.h>

#include "printing.h"
#include "findfiles.h"
#include "defs.h"
#include "common.h"
#include "tokenize.h"
#include "list.h"
#include "index.h"
#include "set.h"
#include "logger.h"


/* SETTING: limit the maximum number of results printed for queries. 0=unlimited. */
#define MAX_RESULT_TABLE_ROWS 20

/* SETTING: Update 'Processing document # n / N' output every 'x' files. 0=disable */
#define PRINT_PROGRESS_INTERVAL 100

#define CLI_COMMAND_EXIT      ".exit"
#define CLI_COMMAND_CLEAR     ".clear"
#define CLI_COMMAND_AUTOCLEAR ".autoclear"
#define CLI_COMMAND_INFO      ".info"
#define CLI_COMMAND_STAT      ".stat"

/* these are pointers instead of definitions as we want to refer other pointers to them */
static const char *type_arg = "--type";
static const char *limit_arg = "--limit";
static const char *stderr_arg = "--stderr";
static const char *outfile_arg = "--outfile";
static const char *help_arg = "--help";

/* will be set to a logger if the optional --outfile argument is present */
static logger_t *result_logger = NULL;

/* write to the result logger, if it exists */
static void log_result(const char *buf) {
    if (result_logger) {
        if (logger_write_buf(result_logger, buf) != 0) {
            /* failed to write, abort logging */
            logger_destroy(result_logger);
            result_logger = NULL;
        }
    }
}

/* write to stdout and the result logger, if it exists */
static void output_result(const char *buf) {
    fputs(buf, stdout);
    log_result(buf);
}

/* print input prefix, and input if given */
static void cli_pr_input(const char *input) {
    printf("%s>>>%s ", ANSI_COLOR_PUR_B, ANSI_COLOR_RESET);
    if (input) {
        printf("%s\n", input);
    }
}

/* write a error message to stdout */
#define cli_pr_error(prefix, fmt, ...) \
    printf(ANSI_COLOR_RED_B prefix ANSI_COLOR_RESET ": " fmt, ##__VA_ARGS__)


static void print_command_list() {
    static const int col_w = 12;
    printf("%sAvailable commands%s\n", ANSI_COLOR_YEL_B, ANSI_COLOR_RESET);
    printf("%-*s - %s\n", col_w, CLI_COMMAND_EXIT, "Exit the application");
    printf("%-*s - %s\n", col_w, CLI_COMMAND_CLEAR, "Clear the terminal once");
    printf("%-*s - %s\n", col_w, CLI_COMMAND_AUTOCLEAR, "Toggle clearing the terminal on each new query");
    printf("%-*s - %s\n", col_w, CLI_COMMAND_STAT, "Print the number indexed documents and unique terms");
    printf("%-*s - %s\n", col_w, CLI_COMMAND_INFO, "Print this message");
    printf("Note: Clearing the terminal only works in ANSI/POSIX terminal emulators\n");
}

static void print_arg_usage(int col_w, const char *arg, const char *val, const char *descr) {
    int whitespace_w = (int) col_w - (strlen(arg) + strlen(val));
    fprintf(stderr, "%s %s %*s - %s\n", arg, val, whitespace_w, "", descr);
}

static void print_usage(char **argv) {
    static const int col_w = 22;
    fprintf(stderr, "\nUsage: \"%s <data-dir> [...optional args>]\"\n", basename(argv[0]));
    fprintf(stderr, "Required Arguments:\n");
    fprintf(stderr, "%-*s - %s\n", col_w + 2, "<data-dir>", "Path to directory of files to index");
    fprintf(stderr, "Optional Arguments:\n");
    print_arg_usage(col_w, type_arg, "<1...n>", "Filter included data files by extension");
    print_arg_usage(col_w, limit_arg, "<n>", "Limit number of included data files");
    print_arg_usage(col_w, outfile_arg, "<fpath>", "Log succesful queries / results to a file");
    print_arg_usage(col_w, stderr_arg, "<fpath | tty>", "Redirect stderr to file or terminal");
}

/**
 * @brief query-specific character input control
 * @returns 1 if character is an operator or part of one
 */
int is_operator_part(int c) {
    switch (c) {
        /* return 1 for all of these, otherwise let `isalnum` decide */
        case '(':
            ATTR_FALLTHROUGH;
        case ')':
            ATTR_FALLTHROUGH;
        case '|':
            ATTR_FALLTHROUGH;
        case '&':
            ATTR_FALLTHROUGH;
        case '!':
            return 1;
        default:
            /* break here as gcc sometimes throws a no-return warning. clang does not. */
            break;
    }
    return 0;
}

/**
 * @brief query-specific character input control
 * @returns 1 if character should be included in a query, otherise 0
 */
int is_valid_query_char(int c) {
    if (is_operator_part(c)) {
        return 1;
    }
    /* not a special char, filter normally as ascii alphanumeric */
    return is_ascii_alnum(c);
}

static void process_query_results(list_t *results, const char *input, long double t_secs) {
    char result_buf[LINE_MAX];
    size_t n_results = list_length(results);
    int n_decimals = (t_secs > 1.0E-3) ? 4 : 6; // 6 decimals if less than 1ms, otherwise 4

    if (result_logger) {
        /* write the query itself to the logfile */
        snprintf(result_buf, LINE_MAX, "\n>>> %s\n", input);
        log_result(result_buf);
    }

    snprintf(
        result_buf,
        LINE_MAX,
        "=== Found %zu result%s in %.*Lfs ===\n",
        n_results,
        (n_results == 1) ? "" : "s",
        n_decimals,
        t_secs
    );
    output_result(result_buf);

    if (!results) {
        return;
    }

    snprintf(result_buf, LINE_MAX, "%-10s %s\n", "Score", "Document");
    output_result(result_buf);

    size_t n_printed = 0;

    while (list_length(results)) {
        query_result_t *res = list_popfirst(results);

        /* verify some properties of the result object */
        assert(res != NULL);
        assert(res->doc_name != NULL);
        assertf(res->doc_name[0] != '\0', "result doc_name cannot be an empty string\n");

        snprintf(result_buf, LINE_MAX, "%-10.3f %s\n", res->score, res->doc_name);
        output_result(result_buf);

        n_printed += 1;
        free(res); // free the result we just popped

        if (MAX_RESULT_TABLE_ROWS) {
            if (n_printed >= MAX_RESULT_TABLE_ROWS && list_length(results)) {
                snprintf(result_buf, LINE_MAX, " ... and %zu more\n", list_length(results));
                output_result(result_buf);
                break;
            }
        }
    }

    if (result_logger) {
        logger_flush(result_logger);
    }
}

static list_t *tokenize_query(char *query) {
    list_t *tokens = list_create((cmp_fn) strcmp);
    if (!tokens) {
        cli_pr_error("Query error", "Likely out of memory\n");
        return NULL;
    }

    /**
     * 1. Parse the query into a list of tokens
     * This will reduce phrases such as "o-k" to "ok", which is completely fine for searching purposes.
     * In fact, google tends to ignore most special characters, although in more sophisticated manner.
     */
    int status = tokenize_string(query, tokens, 1, is_space_or_par, is_valid_query_char, tolower);
    if (status < 0) {
        cli_pr_error("Query error", "Failed to tokenize query\n");
        list_destroy(tokens, free);
        return NULL;
    }

    return tokens;
}

/* execute a query with the index and print results (if any) or error message */
static void execute_query(index_t *idx, list_t *tokens, const char *input) {
    pr_debug("input = \"%s\"\n", input);

    struct timeval t_start, t_end;
    char errmsg_buf[LINE_MAX];
    memset(errmsg_buf, 0, LINE_MAX);

    /* run the query, timing the time it takes */
    gettimeofday(&t_start, NULL);
    list_t *results = index_query(idx, tokens, errmsg_buf);
    gettimeofday(&t_end, NULL);

    long double t_secs = (long double) (t_end.tv_sec - t_start.tv_sec);    // difference in seconds
    t_secs += (long double) (t_end.tv_usec - t_start.tv_usec) / 1000000.0; // convert µs part to secs & add

    if (results) {
        process_query_results(results, input, t_secs);

        /* destroy the list of results and any result_t objects in it */
        list_destroy(results, free);
    } else if (*errmsg_buf) {
        cli_pr_error("Invalid query", "%s\n", errmsg_buf);
    } else {
        cli_pr_error("Index error", "Index returned no results or error message\n");
    }
}


/**
 * @brief Run the interpreter
 * @param idx: pointer to index
 * @param piped_input: nullable. If passed, will run these instead of processing from stdin.
 */
static int run_interpreter(index_t *idx, list_t *piped_input) {
    pr_debug("Starting interpreter\n");
    printf("\n");

    if (!piped_input) {
        printf("Exit with the \"%s\" command.\n", CLI_COMMAND_EXIT);
        printf("Enter \"%s\" for a list of all available commands.\n", CLI_COMMAND_INFO);
    }

    char input[LINE_MAX];
    int auto_clear = -1;

    while (1) {
        memset(input, 0, LINE_MAX);

        if (piped_input) {
            if (!list_length(piped_input)) {
                pr_info("Executed all piped queries\n");
                return 0;
            }
            char *piped_line = list_popfirst(piped_input);
            memcpy(input, piped_line, strlen(piped_line) + 1); // copy the piped input
            free(piped_line);                                  // free the original

            cli_pr_input(input); // simulate actual input
        } else {
            cli_pr_input(NULL);

            if (fgets(input, LINE_MAX, stdin) == NULL) {
                pr_error("Failed to read from stdin: %s\n", strerror(errno));
                cli_pr_error("Critical error", "Failed to read from stdin: %s\n", strerror(errno));
                return -1;
            }
        }

        trim(input); // remove any leading and trailing whitespace, as well as newline
        if (*input == '\0') {
            continue; // ignore empty query
        }

        /* check if command, handle if so */
        if (*input == '.') {
            if (strcmp(input, CLI_COMMAND_EXIT) == 0) {
                return 0;
            } else if (strcmp(input, CLI_COMMAND_CLEAR) == 0) {
                printf(ANSI_CLEAR_TERM);
            } else if (strcmp(input, CLI_COMMAND_AUTOCLEAR) == 0) {
                auto_clear *= -1;
                printf("autoclear toggled %s\n", (auto_clear == 1) ? "on" : "off");
            } else if (strcmp(input, CLI_COMMAND_STAT) == 0) {
                size_t n_docs, n_terms;
                index_stat(idx, &n_docs, &n_terms);
                printf("Index consists of %zu documents and %zu unique terms\n", n_docs, n_terms);
            } else if (strcmp(input, CLI_COMMAND_INFO) == 0) {
                print_command_list();
            } else {
                cli_pr_error("Unrecognized command", "\"%s\"\n", input);
                printf("Enter \"%s\" for a list of all available commands.\n", CLI_COMMAND_INFO);
            }

            continue;
        }

        /* Clear window now if set to do so, before any potential query-related errors */
        if (auto_clear == 1) {
            printf(ANSI_CLEAR_TERM);
            cli_pr_input(input);
        }

        /* transform the input into a list of tokens */
        list_t *tokens = tokenize_query(input);
        if (!tokens) {
            return -1;
        }

        if (list_length(tokens)) {
            execute_query(idx, tokens, input);
        } else {
            printf("Found no usable characters in the query\n");
        }

        list_destroy(tokens, free);
    }
}

/**
 * Process an individual file, reading it anc converting to tokens (words)
 */
static list_t *read_file_terms(char *fpath) {
    FILE *infile = fopen(fpath, "r");
    if (infile == NULL) {
        pr_error("Failed to open %s: %s\n", fpath, strerror(errno));
        return NULL;
    }

    list_t *terms = list_create((cmp_fn) strcmp);
    if (terms == NULL) {
        pr_error("Failed to create list (likely out of memory)\n");
        fclose(infile);
        return NULL;
    }

    /**
     * tokenize file:
     * - tokens must be min. 1 char
     * - split at whitespace,
     * - include only alphanumeric ascii chars,
     * - convert to lowercase
     */
    int status = tokenize_file(infile, terms, 1, isspace, is_ascii_alnum, tolower);
    fclose(infile);

    if (status < 0) {
        pr_error("Failed to tokenize file '%s'\n", fpath);
        list_destroy(terms, free);
        return NULL;
    }

    return terms;
}

/**
 * @param fpaths: list of 1..n paths
 * @returns the created index if succesful, otherwise NULL
 */
static index_t *build_index(list_t *fpaths) {
    pr_debug("Building index\n");

    index_t *idx = index_create();
    if (idx == NULL) {
        pr_error("Failed to create index\n");
        return NULL;
    }

    const size_t files_total = list_length(fpaths);
    size_t i = 0;

    while (list_length(fpaths)) {
        i++;
        if (PRINT_PROGRESS_INTERVAL && (i % PRINT_PROGRESS_INTERVAL == 0 || i == 1 || i == files_total)) {
            printf("\rProcessing document # %zu / %zu", i, files_total);
            fflush(stdout);
        }

        char *path = list_popfirst(fpaths);
        assert(path);

        list_t *terms = read_file_terms(path);

        if (terms == NULL) {
            pr_error("\nFailed to process document.. Ignoring this path and continuing.");
            free(path);
        } else {
            /**
             * Process document with the index.
             * index owns 'path' and 'terms' from this point, regardless of status
             */
            int status = index_document(idx, path, terms);

            if (status != 0) {
                PANIC("\nindex_document failed!\n");
            }
        }
    }

    /* send a newline as the progress print uses carriage return printing */
    if (PRINT_PROGRESS_INTERVAL) {
        printf("\n");
    }

    return idx;
}

/* helper for process_args */
static int insert_valid_ext(char *arg, set_t *valid_exts) {
    if (!is_ascii_alpha_string(arg)) {
        pr_error("Invalid file extension \"%s\"\n", arg);
        return -1;
    }

    /* copy the argument */
    char *arg_cpy = strdup(arg);
    if (!arg_cpy) {
        pr_error("Malloc failed (in strdup): %s\n", strerror(errno));
        return -1;
    }

    /* insert into set of extensions */
    char *old = set_insert(valid_exts, arg_cpy);

    if (old) {
        pr_warn("Extension \"%s\" is specified multiple times. Ignoring the duplicate\n", old);
        free(old);
    } else {
        pr_debug("Including \".%s\" files\n", arg_cpy);
    }

    return 0;
}

/**
 * @brief Parse arguments and populate the `fpaths` list.
 * @param fpaths: list to populate with data file paths
 *
 * @note This function is long and ugly. However, it gets the job done and provides feedback on
 * malformed/misused arguments. Not sure how to split it up, as it would just result in passing a very large
 * number of parameters around, which i doubt will improve readability much.
 */
static int process_args(int argc, char **argv, list_t *fpaths) {
    /* scan for help argument first */
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], help_arg) == 0) {
            return -10;
        }
    }

    // first argument: directory of data files
    if (argc < 2) {
        pr_error("Missing required positional argument: <data-dir>\n");
        return -1;
    }
    char *dir_path = argv[1];

    /* if there is a trailing right slash, remove it */
    char *dir_path_end = &dir_path[strlen(dir_path) - 1];
    if (*dir_path_end == '/') {
        *dir_path_end = '\0'; // strings in argv are indeed modifiable
    }

    /* verify that dir_path exists and is a directory */
    if (!dir_exists(dir_path)) {
        pr_error("<data-dir>: The directory \"%s\" does not exist\n", dir_path);
        return -1;
    }

    const char *parsing = NULL; // argument currently being parsed
    int parsed_values = 0;      // values parsed for the argument we're parsing

    size_t max_n_files = 0;                         // limit to n files, 0 => no limit
    set_t *valid_exts = NULL;                       // temporary set of file extensions to include
    set_t *completed = set_create((cmp_fn) strcmp); // arguments that are already parsed

    if (!completed) {
        return -1; // failed to create set
    }

    /* for the optional arguments, default to error (see the end label) */
    int status = -1;

    /* parse optional arguments/values one by one */
    for (int i = 2; i < argc; i++) {
        char *arg = argv[i];

        /* this will capture anything starting with --, so we detect arguments missing values */
        if (strncmp(arg, "--", sizeof("--") - 1) == 0) {
            if (parsing && !parsed_values) {
                pr_error("Expected value assignment after %s, found %s\n", parsing, arg);
                goto end;
            }

            /* determine which arg was entered */
            if (!strcmp(arg, outfile_arg)) {
                parsing = outfile_arg;
            } else if (!strcmp(arg, stderr_arg)) {
                parsing = stderr_arg;
            } else if (!strcmp(arg, type_arg)) {
                /* create a set to hold the extension arguments that follow */
                valid_exts = set_create((cmp_fn) strcmp);
                if (valid_exts == NULL) {
                    goto end;
                }
                parsing = type_arg;
            } else if (!strcmp(arg, limit_arg)) {
                parsing = limit_arg;
            } else {
                pr_error("Unrecognized argument: \"%s\"\n", arg);
                goto end;
            }

            /* add to set of already found commands */
            if (set_insert(completed, (void *) parsing)) {
                pr_error("Duplicate argument: \"%s\"\n", arg);
                goto end;
            }

            parsed_values = 0; // parsed_values 0 of the current argument

            /* continue to the following value */
            continue;
        }

        if (parsing == type_arg) {
            if (insert_valid_ext(arg, valid_exts) < 0) {
                goto end;
            }
            /* may have more values to parse */
            parsed_values += 1;
            continue;
        }

        /* these arguments only have 1 value */
        if (parsing == outfile_arg) {
            result_logger = logger_create(arg);

            if (result_logger == NULL) {
                pr_error("Failed to create result_logger\n");
                goto end;
            }
        } else if (parsing == stderr_arg) {
            if (redirect_stderr(arg) < 0) {
                goto end;
            }
            pr_debug("Redirected stderr\n");
        } else if (parsing == limit_arg) {
            if (!is_digit_string(arg)) {
                pr_error("Expected integer value following %s, found \"%s\"\n", limit_arg, arg);
                goto end;
            }
            max_n_files = strtoul(arg, NULL, 10);
        } else {
            pr_error("Unrecognized or misplaced argument: \"%s\"\n", arg);
            goto end;
        }

        parsing = NULL;
        parsed_values = 1;
    }

    if (parsing && !parsed_values) {
        pr_error("Expected value assignment after \"%s\"\n", parsing);
        pr_debug("See the README for further info on usage\n");
        goto end;
    }

    /* find the files at dir_path */
    if (find_files(dir_path, fpaths, valid_exts, max_n_files) < 0) {
        pr_error("<data-dir>: Failed to find files at \"%s\"\n", dir_path);
        goto end;
    }

    /* verify that we found at least one path to a file */
    if (list_length(fpaths) == 0) {
        pr_error("<data-dir>: Found no valid files to index at \"%s\"\n", dir_path);
        goto end;
    }

    /* being here means that everything went OK */
    pr_debug("Discovered %zu files in directory \"%s\"\n", list_length(fpaths), dir_path);
    status = 0;

    /* continue to cleanup */

end:

    /* clean up any temporary data structures and return */
    set_destroy(valid_exts, free);
    set_destroy(completed, NULL);

    return status;
}

/**
 * @brief create a list of each line from the piped input.
 * @returns NULL on error; otherwise a list of the input, line for line. Newline characters are removed.
 */
static list_t *read_piped_lines() {
    int bytes_ready = 0;

    /* determine size of piped input */
    if (ioctl(STDIN_FILENO, FIONREAD, &bytes_ready) == -1) {
        pr_error("ioctl failed: %s\n", strerror(errno));
        return NULL;
    }

    if (bytes_ready == 0) {
        pr_error("Expected input from pipe\n");
        return NULL;
    }

    /* temp. buffer to hold the piped input */
    char *content_buf = malloc((size_t) bytes_ready + 1);
    if (!content_buf) {
        pr_error("Malloc failed: %s\n", strerror(errno));
        return NULL;
    }

    /* read all bytes to the buffer */
    if (read(STDIN_FILENO, content_buf, bytes_ready) != bytes_ready) {
        pr_error("Failed to read from stdin: %s\n", strerror(errno));
        free(content_buf);
        return NULL;
    }

    content_buf[bytes_ready] = '\0';
    trim(content_buf);

    /* list to hold each line */
    list_t *piped = list_create((cmp_fn) strcmp);
    if (!piped) {
        free(content_buf);
        return NULL;
    }

    /**
     * Tokenize input (as lines):
     * - minimum length 1
     * - split on newlines
     * - include only printed characters
     */
    int status = tokenize_string(content_buf, piped, 1, is_newline, isprint, NULL);
    free(content_buf); // free the temp buf

    if (status != 0) {
        pr_error("Failed to tokenize stdin\n");
        list_destroy(piped, free);
        return NULL;
    }

    /* verify that there is some input after tokenizing */
    if (list_length(piped) == 0) {
        pr_error("Found no valid characters in input\n");
        list_destroy(piped, free);
        return NULL;
    }

    return piped;
}

int main(int argc, char **argv) {
    int exit_code = EXIT_FAILURE; // default to failure
    list_t *piped_input = NULL;

    /* If the descriptor of stdin is not a terminal, get the piped input. */
    if (isatty(STDIN_FILENO) == 0) {
        piped_input = read_piped_lines();

        if (piped_input == NULL) {
            return EXIT_FAILURE;
        }
    }

    /* named 'idx' as 'index' collides with a function from <string.h> */
    index_t *idx = NULL;

    /* create a list to populate with paths */
    list_t *fpaths = list_create((cmp_fn) strcmp);

    int arg_status = process_args(argc, argv, fpaths);

    if (fpaths != NULL && arg_status == 0) {
        idx = build_index(fpaths);

        /* hand over control to the interpreter */
        if (idx && run_interpreter(idx, piped_input) == 0) {
            exit_code = EXIT_SUCCESS; // interpreter exited OK
        }
        /* continue to cleanup */
    }

    if (idx) {
        pr_debug("Destroying index\n");
        index_destroy(idx);
    } else {
        /* if there is an index, this list will have been destroyed by it */
        list_destroy(fpaths, free);
    }

    list_destroy(piped_input, free); // empty list if interpreting went ok
    logger_destroy(result_logger);

    if (arg_status == -10) {
        print_usage(argv);
        exit_code = EXIT_SUCCESS; // help argument is not a failure
    } else if (arg_status < 0) {
        fprintf(stderr, "Run \"%s %s\" to print arguments and usage\n", argv[0], help_arg);
    } else {
        pr_debug("Exiting, status: %s\n", (exit_code == EXIT_SUCCESS) ? "OK" : "ERROR");
    }

    return exit_code;
}
