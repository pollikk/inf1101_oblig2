/**
 * @authors
 * Odin Bjerke <odin.bjerke@uit.no>
 *
 * @brief Defines printf-like and assert macros.
 *
 * @details
 * The compile-time definition of `LOG_LEVEL` controls what prints are included in the compiled program.
 * This definition may either be global (using `-D NDEBUG` when compiling) or set on a per-file basis with a
 * default fallback defined in this file.
 *
 * - if `PRINTING_NCOLOR` is defined, colors are removed.
 * - if `PRINTING_NMETA` is defined, meta information such as file/line is removed.
 * - if `NDEBUG` is defined, asserts and non-error prints will do nothing, and their invocations are optimized
 * away by the compiler.
 *
 * ---
 *
 * ## March 2025
 * - Removed __func__ meta from debug
 * - Changed meta info such as [error] to appear first in output prefix, where present
 * - Added proper docstrings for the macros
 * - Changed colors from affecting entire text to rather just the label (e.g. "error ")
 * - Added prefix label for all print methods other than info
 * - Removed color from info print, embracing it as a toggle-able fprintf wrapper
 * - Added bold ANSI colors, and
 */

#ifndef PRINTING_H
#  define PRINTING_H

#  include <stdio.h>
#  include <stdlib.h>

enum {
    LOG_LEVEL_PANIC = 1, /* minimum log level. only prints on PANIC. */
    LOG_LEVEL_ERROR = 2, /* enable pr_error */
    LOG_LEVEL_WARN = 3,  /* enable pr_warn (+ pr_error) */
    LOG_LEVEL_INFO = 4,  /* enable pr_info (+ pr_error, pr_warn) */
    LOG_LEVEL_DEBUG = 5  /* enable pr_debug (+ pr_error, pr_warn, pr_info) */
};

/*************/
/** OPTIONS **/
/*************/

/* uncomment to remove all colors */
// #define PRINTING_NCOLOR

/* uncomment to remove all meta info (e.g. file/line) */
// #define PRINTING_NMETA

#  ifndef LOG_LEVEL
/* default log level */
#    define LOG_LEVEL LOG_LEVEL_DEBUG
#  endif

/* target for pr_info. Can be set to stdio, etc. */
#  define INFO_STREAM stderr

/********************/
/** END OF OPTIONS **/
/********************/

/**** ansi colors ****/

/* to reset any applied colors, otherwise the output is colored forever in terminals */
#  define ANSI_COLOR_RESET "\033[0m"

/* regular colors */

#  define ANSI_COLOR_BLA "\033[0;30m"
#  define ANSI_COLOR_RED "\033[0;31m"
#  define ANSI_COLOR_GRE "\033[0;32m"
#  define ANSI_COLOR_YEL "\033[0;33m"
#  define ANSI_COLOR_BLU "\033[0;34m"
#  define ANSI_COLOR_PUR "\033[0;35m"
#  define ANSI_COLOR_CYA "\033[0;36m"
#  define ANSI_COLOR_WHI "\033[0;37m"

/* bold colors */

#  define ANSI_COLOR_BLA_B "\033[1;30m"
#  define ANSI_COLOR_RED_B "\033[1;31m"
#  define ANSI_COLOR_GRE_B "\033[1;32m"
#  define ANSI_COLOR_YEL_B "\033[1;33m"
#  define ANSI_COLOR_BLU_B "\033[1;34m"
#  define ANSI_COLOR_PUR_B "\033[1;35m"
#  define ANSI_COLOR_CYA_B "\033[1;36m"
#  define ANSI_COLOR_WHI_B "\033[1;37m"

/* device whether to include colors */
#  ifdef PRINTING_NCOLOR
#    define COLOR_PR_RESET
#    define COLOR_PR_ERROR
#    define COLOR_PR_WARN
#    define COLOR_PR_DEBUG
#  else
#    define COLOR_PR_RESET ANSI_COLOR_RESET
#    define COLOR_PR_ERROR ANSI_COLOR_RED_B
#    define COLOR_PR_WARN  ANSI_COLOR_PUR_B
#    define COLOR_PR_DEBUG ANSI_COLOR_YEL_B
#  endif /* PRINTING_NCOLOR */

/* device whether to include meta information */
#  ifdef PRINTING_NMETA
#    define META_FILE_LINE_FMT  "%s%s"
#    define META_FILE_LINE_ARGS "", ""
#    define META_FUNC_FMT       "%s"
#    define META_FUNC_ARGS      ""
#    define COLOR_META
#  else
#    define META_FILE_LINE_FMT  "%s:%d: "
#    define META_FILE_LINE_ARGS __FILE__, __LINE__
#    define META_FUNC_FMT       "<%s>: "
#    define META_FUNC_ARGS      __func__
#    ifdef PRINTING_NCOLOR
#      define COLOR_META
#    else
#      define COLOR_META ANSI_COLOR_WHI_B
#    endif /* PRINTING_NCOLOR */
#  endif   /* PRINTING_NMETA */

/**
 * [PRINTING_H INTERNAL MACRO]
 * wrap fprintf, only print if log level is sufficient
 */
#  define do_print_if_lvl(lvl, f, fmt, ...)     \
        do {                                    \
            if (lvl <= LOG_LEVEL) {             \
                fprintf(f, fmt, ##__VA_ARGS__); \
            } else {                            \
                ((void) 0);                     \
            }                                   \
        } while (0)

/**
 * @brief Convenience macro to print error message before aborting (forcefully quitting) the program.
 *
 * Prefix format: `__FILE__:__LINE__: <__func__>: "PANIC " ...`
 *
 * @note Never omitted, regardless of `LOG_LEVEL` setting.
 */
#  define PANIC(fmt, ...)                                                                                \
        do {                                                                                             \
            do_print_if_lvl(                                                                             \
                LOG_LEVEL_PANIC,                                                                         \
                stderr,                                                                                  \
                COLOR_META META_FILE_LINE_FMT META_FUNC_FMT COLOR_PR_ERROR "PANIC: " COLOR_PR_RESET fmt, \
                META_FILE_LINE_ARGS,                                                                     \
                META_FUNC_ARGS,                                                                          \
                ##__VA_ARGS__                                                                            \
            );                                                                                           \
            abort();                                                                                     \
        } while (0)

#  ifndef NDEBUG

/**
 * @brief printf-like utility macro intended for error messages.
 *
 * Prefix format: `__FILE__:__LINE__: <__func__>: "error: " ...`
 *
 * @note omitted only if `LOG_LEVEL == LOG_LEVEL_PANIC`. Prefix is simplified if `NDEBUG` is defined, as
 * file/line meta info is typically wonky when compiled with optimizations - as `NDEBUG` implies is the case.
 */
#    define pr_error(fmt, ...)                                                                         \
          do_print_if_lvl(                                                                             \
              LOG_LEVEL_ERROR,                                                                         \
              stderr,                                                                                  \
              COLOR_META META_FILE_LINE_FMT META_FUNC_FMT COLOR_PR_ERROR "error: " COLOR_PR_RESET fmt, \
              META_FILE_LINE_ARGS,                                                                     \
              META_FUNC_ARGS,                                                                          \
              ##__VA_ARGS__                                                                            \
          )

/**
 * @brief printf-like utility macro intended for warnings.
 *
 * Prefix format: `__FILE__:__LINE__: "warning: " ...`
 *
 * @note omitted if `LOG_LEVEL < LOG_LEVEL_WARN` or `NDEBUG` is defined.
 */
#    define pr_warn(fmt, ...)                                                             \
          do_print_if_lvl(                                                                \
              LOG_LEVEL_WARN,                                                             \
              stderr,                                                                     \
              COLOR_META META_FILE_LINE_FMT COLOR_PR_WARN "warning: " COLOR_PR_RESET fmt, \
              META_FILE_LINE_ARGS,                                                        \
              ##__VA_ARGS__                                                               \
          )

/**
 * @brief printf-like utility macro intended for general information.
 *
 * No prefix, safe for multi-line/partial prints. Essentially just a printf wrapper with the added ability to
 * toggle, either globally or per-file.
 *
 * @note omitted if `LOG_LEVEL < LOG_LEVEL_INFO` or `NDEBUG` is defined.
 */
#    define pr_info(fmt, ...) do_print_if_lvl(LOG_LEVEL_INFO, INFO_STREAM, fmt, ##__VA_ARGS__)

/**
 * @brief printf-like utility macro intended for debug prints.
 *
 * Prefix format: `__FILE__:__LINE__: <__func__>: "debug: " ...`
 *
 * @note omitted if `LOG_LEVEL != LOG_LEVEL_DEBUG` or `NDEBUG` is defined.
 */
#    define pr_debug(fmt, ...)                                                                         \
          do_print_if_lvl(                                                                             \
              LOG_LEVEL_DEBUG,                                                                         \
              stderr,                                                                                  \
              COLOR_META META_FILE_LINE_FMT META_FUNC_FMT COLOR_PR_DEBUG "debug: " COLOR_PR_RESET fmt, \
              META_FILE_LINE_ARGS,                                                                     \
              META_FUNC_ARGS,                                                                          \
              ##__VA_ARGS__                                                                            \
          )

#  else /* NDEBUG */

#    define pr_error(fmt, ...) do_print_if_lvl(LOG_LEVEL_ERROR, stderr, "[error] " fmt, ##__VA_ARGS__)
#    define pr_warn(fmt, ...)  ((void) 0)
#    define pr_debug(fmt, ...) ((void) 0)
#    define pr_info(fmt, ...)  ((void) 0)

#  endif /* !NDEBUG */

#endif /* PRINTING_H */


/* --- intentionally outside of include guard for per-include control --- */

#ifdef assert
#  undef assert
#endif /* assert */

#ifdef assertf
#  undef assertf
#endif /* assertf */

#ifndef NDEBUG

/**
 * @brief Drop-in replacement for the assertion macro provided by `<assert.h>`.
 *
 * Does nothing unless `assertion` evaluates to false, in which case program execution is aborted and
 * and the literal assertion is printed. (e.g. `assertion '<not-true>' failed\n`)
 *
 * @note omitted if `NDEBUG` is defined.
 */
#  define assert(assertion)                                                                      \
        do {                                                                                     \
            if (assertion) {                                                                     \
                ((void) 0);                                                                      \
            } else {                                                                             \
                PANIC(ANSI_COLOR_WHI "assertion `%s` failed" ANSI_COLOR_RESET "\n", #assertion); \
                abort();                                                                         \
            }                                                                                    \
        } while (0)

/**
 * @brief Like `assert`, but with a printf-like extension that is executed on assertion failure. Typically
 * used to print the values of the variables.
 *
 * Does nothing unless `assertion` evaluates to false, in which case program execution is aborted and
 * and the literal assertion is printed along with the provided message. (e.g. `assertion '<not-true>' failed:
 * <msg>`). Does not include a newline.
 *
 * @note omitted if `NDEBUG` is defined.
 */
#  define assertf(assertion, fmt, ...)                                             \
        do {                                                                       \
            if (assertion) {                                                       \
                ((void) 0);                                                        \
            } else {                                                               \
                PANIC(                                                             \
                    ANSI_COLOR_WHI "assertion `%s` failed: " ANSI_COLOR_RESET fmt, \
                    #assertion,                                                    \
                    ##__VA_ARGS__                                                  \
                );                                                                 \
                abort();                                                           \
            }                                                                      \
        } while (0)

#else

#  define assert(assertion)            ((void) 0)
#  define assertf(assertion, fmt, ...) ((void) 0)

#endif /* NDEBUG */
