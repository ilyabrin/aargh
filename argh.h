/*
 * argh.h - v1.9.0 - Single-header command-line argument parser for C
 *
 * The API follows Semantic Versioning: no breaking changes before v2.0.
 *
 * Options write straight into your variables. No heap allocations, no global
 * state, and option tables can be `static const` (read-only memory).
 *
 * USAGE
 *   In exactly one .c file:
 *       #define ARGH_IMPLEMENTATION
 *       #include "argh.h"
 *   Everywhere else just #include "argh.h".
 *   Or #define ARGH_STATIC before including: everything in this one file,
 *   all functions static.
 *
 * EXAMPLE
 *   int main(int argc, char **argv) {
 *       bool verbose = false;
 *       int jobs = 4;
 *
 *       argh_parser p;
 *       argh_init(&p, "mytool", "Does useful things");
 *       argh_flag(&p, 'v', "verbose", &verbose, "Verbose output");
 *       argh_int(&p, 'j', "jobs", &jobs, "Parallel jobs");
 *
 *       if (!argh_parse(&p, argc, argv))
 *           return argh_exit_code(&p);  // --help, --version or an error
 *
 *       // use verbose and jobs
 *   }
 *
 * CONFIGURATION (define before including)
 *   ARGH_BUILDER_CAP  options that argh_flag()/argh_int()/... can add (32)
 *   ARGH_MAX_OPTS     options on the active command path, all tables (64)
 *   ARGH_MAX_TABLES   tables per parser, the builder counts as one (8)
 *   ARGH_MAX_DEPTH    levels of nested commands (4)
 *   ARGH_HELP_WIDTH   column where help text wraps, 0 for none (80)
 *   ARGH_NO_SUGGEST   no "did you mean" suggestions in error messages
 *   ARGH_NO_COMMANDS  no commands: smaller code for programs without them
 *   ARGH_NO_STDIO     no <stdio.h>: output goes only to argh_set_writer()
 *   ARGH_NO_FLOAT     no argh_double: no strtod and no floating point
 *   ARGH_STATIC       all functions static, implementation included
 *
 * LICENSE: MIT (see end of file)
 */

#ifndef ARGH_H_INCLUDED
#define ARGH_H_INCLUDED

#include <stdbool.h>
#include <stddef.h>

/* The version of this file, to check at compile time:
 *     #if ARGH_VERSION_MAJOR < 1
 *     #error "needs argh.h 1.0 or later"
 *     #endif */
#define ARGH_VERSION_MAJOR 1
#define ARGH_VERSION_MINOR 9
#define ARGH_VERSION_PATCH 0
#define ARGH_VERSION "1.9.0"

/* ARGH_STATIC: every function is static and the implementation is included,
 * for a program in one file or a library that embeds its own copy of argh.h
 * without clashing with another one. */
#ifdef ARGH_STATIC
#ifndef ARGH_IMPLEMENTATION
#define ARGH_IMPLEMENTATION
#endif
#if defined(__GNUC__) || defined(__clang__)
#define ARGH__DEF static __attribute__((unused))
#else
#define ARGH__DEF static
#endif
#else
#define ARGH__DEF extern
#endif

#ifndef ARGH_BUILDER_CAP
#define ARGH_BUILDER_CAP 32
#endif

#ifndef ARGH_MAX_OPTS
#define ARGH_MAX_OPTS 64
#endif

#ifndef ARGH_MAX_TABLES
#define ARGH_MAX_TABLES 8
#endif

#ifndef ARGH_MAX_DEPTH
#define ARGH_MAX_DEPTH 4
#endif

/* Help text wraps at this column; 0 turns wrapping off and leaves its code
 * out. Only output depends on it, so files may use different values. */
#ifndef ARGH_HELP_WIDTH
#define ARGH_HELP_WIDTH 80
#endif

/* The settings above change the size of argh_parser, so every file that
 * includes argh.h must use the same ones. argh_init is renamed after them:
 * files with different settings fail to link, with a name such as
 * argh_init_settings_b32_o64_t8_d4_cmd in the error, instead of corrupting
 * memory at run time. Define the settings as plain numbers. */
#ifdef ARGH_NO_COMMANDS
#define ARGH__CMD_TAG nocmd
#else
#define ARGH__CMD_TAG cmd
#endif
#define ARGH__INIT_NAME_(b, o, t, d, c) argh_init_settings_b##b##_o##o##_t##t##_d##d##_##c
#define ARGH__INIT_NAME(b, o, t, d, c) ARGH__INIT_NAME_(b, o, t, d, c)
#define argh_init ARGH__INIT_NAME(ARGH_BUILDER_CAP, ARGH_MAX_OPTS, ARGH_MAX_TABLES, ARGH_MAX_DEPTH, ARGH__CMD_TAG)

#ifdef __cplusplus
extern "C"
{
#endif

    /* ============================================================================
     * Types
     * ============================================================================ */

    /* Internal: what an option entry is, stored in argh_opt.kind by the
     * macros and builder calls. Not part of the API, values may change. */
    enum argh__kind
    {
        ARGH__K_END = 0, /* table terminator */
        ARGH__K_FLAG,    /* bool:          -v, --verbose, --no-verbose */
        ARGH__K_COUNT,   /* int:           -vvv */
        ARGH__K_INT,     /* int:           -j 4, --jobs=4 */
        ARGH__K_LONG,    /* long:          same as int */
        ARGH__K_UINT,    /* unsigned:      -n 4, no minus sign */
        ARGH__K_SIZE,    /* size_t:        same as unsigned */
        ARGH__K_DOUBLE,  /* double:        --ratio 0.5 */
        ARGH__K_STRING,  /* const char *:  -o file */
        ARGH__K_ENUM,    /* int (index):   --mode fast */
        ARGH__K_LIST,    /* argh_values:   -I a -I b */
        ARGH__K_CUSTOM,  /* your type:     --size 10M, see argh_type */
        /* Options end here: everything from FLAG to CUSTOM is one */
        ARGH__K_POS,     /* const char *:  positional argument */
        ARGH__K_REST,    /* argh_values:   all remaining positionals */
        ARGH__K_GROUP,   /* help section heading */
        ARGH__K_EXAMPLE, /* a command line for help, long_name holds it */
        ARGH__K_ENV,     /* environment variable for target's option, long_name holds it */
        ARGH__K_IMPLICIT, /* value of target's option when given bare, long_name holds it */
        ARGH__K_RANGE     /* bounds of target's integer option: extra and metavar hold them */
    };

    /* Per-option flags. Stored in argh_opt.flags, combine with |. */
    enum argh_opt_flag
    {
        ARGH_REQUIRED = 1 << 0,  /* must be given */
        ARGH_OPTIONAL = 1 << 1,  /* positional may be omitted */
        ARGH_HIDDEN = 1 << 2,    /* not shown in help */
        ARGH_NEGATABLE = 1 << 3, /* flag also accepts --no-<name> */
        ARGH_ONCE = 1 << 4       /* giving it twice is an error */
    };

    /* Parser-wide flags for argh_set_flags(). */
    enum argh_parser_flag
    {
        ARGH_POSIX = 1 << 0,       /* stop parsing options at the first positional */
        ARGH_NO_AUTO_HELP = 1 << 1 /* no built-in -h/--help and -V/--version */
    };

    /* Values are stable: new codes are only ever added at the end. */
    typedef enum argh_err
    {
        ARGH_E_NONE = 0,
        ARGH_E_UNKNOWN_OPTION,      /* --verbos */
        ARGH_E_MISSING_VALUE,       /* --jobs at the end of the line */
        ARGH_E_INVALID_VALUE,       /* --jobs abc, --mode slow */
        ARGH_E_OUT_OF_RANGE,        /* --jobs 99999999999 */
        ARGH_E_UNEXPECTED_VALUE,    /* --count=3 on a counter */
        ARGH_E_SHORT_EQUALS,        /* -o=file (use -o file) */
        ARGH_E_UNEXPECTED_ARGUMENT, /* a positional nobody asked for */
        ARGH_E_MISSING_REQUIRED,    /* required option or positional absent */
        ARGH_E_REPEATED,            /* ARGH_ONCE option given twice */
        ARGH_E_TOO_MANY_VALUES,     /* list buffer full */
        ARGH_E_CONFIG,              /* mistake in the option definitions */
        ARGH_E_UNKNOWN_COMMAND,     /* tool remtoe */
        ARGH_E_MISSING_COMMAND,     /* tool (when a command is required) */
        ARGH_E_CONFLICT,            /* --json --yaml with ARGH_AT_MOST_ONE */
        ARGH_E_ONE_REQUIRED,        /* none of an ARGH_EXACTLY_ONE / ARGH_AT_LEAST_ONE set */
        ARGH_E_REQUIRES,            /* --tls-key without --tls-cert (ARGH_REQUIRES) */
        ARGH_E_CUSTOM               /* argh_fail() from a validator */
    } argh_err;

    /* One option, positional or help heading. Build it with the ARGH_* macros
     * or the argh_* builder functions rather than by hand. */
    typedef struct argh_opt
    {
        char short_name;       /* 'v' for -v, 0 for none */
        const char *long_name; /* "verbose" for --verbose, positional name */
        unsigned char kind;    /* enum argh__kind */
        unsigned char flags;   /* enum argh_opt_flag */
        void *target;          /* variable the value is written to */
        const void *extra;     /* ARGH__K_ENUM: NULL-terminated choices,
                                  ARGH__K_CUSTOM: const argh_type *,
                                  ARGH__K_RANGE: the lower bound, cast */
        const char *help;      /* help text, group title for ARGH__K_GROUP */
        const char *metavar;   /* value name in help, NULL for a default */
    } argh_opt;

    /* A list of strings: repeated options (ARGH__K_LIST) or the remaining
     * positionals (ARGH__K_REST). Strings point into argv. */
    typedef struct argh_values
    {
        const char **items;
        int count;
        int capacity; /* ARGH__K_LIST only: size of items */
    } argh_values;

    /* A value type of your own, for ARGH_CUSTOM / argh_custom. Define it once
     * as a constant and reuse it for any number of options:
     *
     *     static const argh_type size_type = {"<size>", parse_size, format_size};
     */
    typedef struct argh_type
    {
        /* Name of the value in help, such as "<size>". NULL for "<value>". */
        const char *metavar;

        /* Converts text and stores it in *target. Returns NULL on success, or
         * a short reason on failure, which ends up in the error message:
         * "invalid value '10Q' for '--size': <reason>". */
        const char *(*parse)(const char *text, void *target);

        /* Optional: writes the current value of *target as text, so help can
         * show the default. Return false to show none. NULL for no default. */
        bool (*format)(const void *target, char *buf, size_t size);
    } argh_type;

    /* Initializer for a list backed by a fixed array:
     *     const char *buf[8];
     *     argh_values includes = ARGH_VALUES(buf); */
#define ARGH_VALUES(array) {(array), 0, (int)(sizeof(array) / sizeof((array)[0]))}

    /* Variables per rule */
#define ARGH_RULE_MAX 4

    /* A constraint between options, referring to their variables. Build tables
     * of them with ARGH_AT_MOST_ONE, ARGH_EXACTLY_ONE, ARGH_AT_LEAST_ONE,
     * ARGH_REQUIRES and ARGH_RULES_END. */
    typedef struct argh_rule
    {
        int kind;
        const void *targets[ARGH_RULE_MAX]; /* unused slots are NULL */
    } argh_rule;

    /* Internal: stored in argh_rule.kind by the rule macros */
    enum argh__rule_kind
    {
        ARGH__R_END = 0,
        ARGH__R_AT_MOST_ONE,  /* no two of them together */
        ARGH__R_EXACTLY_ONE,  /* one of them, and only one */
        ARGH__R_AT_LEAST_ONE, /* one of them or more */
        ARGH__R_REQUIRES      /* the first needs all the others */
    };

#define ARGH_AT_MOST_ONE(...) {ARGH__R_AT_MOST_ONE, {__VA_ARGS__}}
#define ARGH_EXACTLY_ONE(...) {ARGH__R_EXACTLY_ONE, {__VA_ARGS__}}
#define ARGH_AT_LEAST_ONE(...) {ARGH__R_AT_LEAST_ONE, {__VA_ARGS__}}
#define ARGH_REQUIRES(target, ...) {ARGH__R_REQUIRES, {target, __VA_ARGS__}}
#define ARGH_RULES_END {ARGH__R_END, {NULL, NULL, NULL, NULL}}

    typedef struct argh_error
    {
        argh_err code;
        int argv_index;         /* argv position of the problem, -1 if none */
        const argh_opt *opt;    /* option involved, if any */
        const char *value;      /* offending text, if any */
        const char *detail;     /* extra context: ARGH_E_CONFIG, custom types,
                                   the message of argh_fail() */
        const char *suggestion; /* closest valid name for an unknown option
                                   or command (without dashes), or NULL */
        const argh_rule *rule;  /* the rule that failed, for rule errors */
        char short_name;        /* offending short option, if any */
    } argh_error;

    struct argh_parser;

    /* A command: `tool <name> ...`. Build tables of them with ARGH_CMD,
     * ARGH_CMD_GROUP and ARGH_CMD_END. */
    typedef struct argh_cmd
    {
        const char *name;
        const char *help;
        const argh_opt *opts;         /* options and positionals, NULL if none */
        const struct argh_cmd *subs;  /* subcommands, NULL for a leaf */
        int (*run)(struct argh_parser *p, void *user); /* handler, NULL if none */
        unsigned flags;               /* ARGH_POSIX: options end at its first positional */
    } argh_cmd;

    /* Output sink for help, version and error messages. */
    typedef void (*argh_write_fn)(void *ctx, bool to_stderr, const char *text, size_t len);

    /* Checks what rules can't express. Return true if the arguments are fine,
     * or return argh_fail(p, "message"). */
    typedef bool (*argh_validate_fn)(struct argh_parser *p, void *ctx);

    /* Lives on the stack. Fields are internal: use the functions below.
     * Pointers first and small fields last, to keep padding out. */
    typedef struct argh_parser
    {
        const char *argh__name;
        const char *argh__about;
        const char *argh__version;
        const argh_opt *argh__tables[ARGH_MAX_TABLES];
        argh_write_fn argh__write;
        void *argh__write_ctx;
        const argh_rule *argh__rules;
        /* Set by argh_rules(): calling the rule checker through a pointer
         * lets the linker drop it from programs that don't use rules */
        int (*argh__rule_check)(struct argh_parser *p);
        argh_validate_fn argh__validator;
        void *argh__validator_ctx;
#ifndef ARGH_NO_COMMANDS
        const argh_cmd *argh__commands;
        const argh_cmd *argh__path[ARGH_MAX_DEPTH];
#endif
        argh_error argh__error;
        argh_opt argh__builder[ARGH_BUILDER_CAP + 1];

        unsigned short argh__builder_count;
        unsigned char argh__table_count;
        unsigned char argh__flags;
        unsigned char argh__status;
        unsigned char argh__setup_problem; /* ARGH__P_*, found before parsing */
#ifndef ARGH_NO_COMMANDS
        unsigned char argh__depth;
#endif
        unsigned char argh__seen[(ARGH_MAX_OPTS + 7) / 8];
    } argh_parser;

    /* ============================================================================
     * Setup
     * ============================================================================ */

    /* name: program name for help and errors, NULL to take it from argv[0].
     * about: one-line description for help, can be NULL. */
    ARGH__DEF void argh_init(argh_parser *p, const char *name, const char *about);

    /* Enables -V/--version, printing "<name> <version>". */
    ARGH__DEF void argh_version(argh_parser *p, const char *version);

    /* ARGH_POSIX, ARGH_NO_AUTO_HELP, combined with |. Replaces the flags set
     * before, so pass them all in one call. */
    ARGH__DEF void argh_set_flags(argh_parser *p, unsigned flags);

    /* Redirects all output. The default writes to stdout and stderr, or
     * discards it with ARGH_NO_STDIO. */
    ARGH__DEF void argh_set_writer(argh_parser *p, argh_write_fn write, void *ctx);

    /* Adds an option table ending with ARGH_END. Tables and builder calls can
     * be mixed; options appear in help in the order they were added. */
    ARGH__DEF void argh_table(argh_parser *p, const argh_opt *table);

#ifndef ARGH_NO_COMMANDS
    /* Sets the top-level commands, a table ending with ARGH_CMD_END. Options
     * added with argh_table() and builder calls become global options that
     * work before and after the command name. */
    ARGH__DEF void argh_commands(argh_parser *p, const argh_cmd *commands);
#endif

    /* ============================================================================
     * Builder: add options one call at a time
     *
     * Each call returns the new option, or NULL if ARGH_BUILDER_CAP is exceeded
     * (argh_parse then fails with ARGH_E_CONFIG). Modifiers accept NULL.
     * ============================================================================ */

    ARGH__DEF argh_opt *argh_flag(argh_parser *p, char short_name, const char *long_name, bool *target, const char *help);
    ARGH__DEF argh_opt *argh_count(argh_parser *p, char short_name, const char *long_name, int *target, const char *help);
    ARGH__DEF argh_opt *argh_int(argh_parser *p, char short_name, const char *long_name, int *target, const char *help);
    ARGH__DEF argh_opt *argh_long(argh_parser *p, char short_name, const char *long_name, long *target, const char *help);
    ARGH__DEF argh_opt *argh_uint(argh_parser *p, char short_name, const char *long_name, unsigned *target, const char *help);
    ARGH__DEF argh_opt *argh_size(argh_parser *p, char short_name, const char *long_name, size_t *target, const char *help);
#ifndef ARGH_NO_FLOAT
    ARGH__DEF argh_opt *argh_double(argh_parser *p, char short_name, const char *long_name, double *target, const char *help);
#endif
    ARGH__DEF argh_opt *argh_string(argh_parser *p, char short_name, const char *long_name, const char **target, const char *help);
    ARGH__DEF argh_opt *argh_enum(argh_parser *p, char short_name, const char *long_name, int *target,
                        const char *const *choices, const char *help);
    ARGH__DEF argh_opt *argh_list(argh_parser *p, char short_name, const char *long_name, argh_values *target, const char *help);
    ARGH__DEF argh_opt *argh_pos(argh_parser *p, const char *name, const char **target, const char *help);
    ARGH__DEF argh_opt *argh_rest(argh_parser *p, const char *name, argh_values *target, const char *help);
    ARGH__DEF argh_opt *argh_custom(argh_parser *p, char short_name, const char *long_name, void *target,
                          const argh_type *type, const char *help);
    ARGH__DEF argh_opt *argh_group(argh_parser *p, const char *title);
    /* A usage example for help, starting with the program name. Without
     * NDEBUG, argh_parse checks that it parses with the current options. */
    ARGH__DEF argh_opt *argh_example(argh_parser *p, const char *command, const char *help);
    /* The option bound to target takes its value from environment variable
     * name when the command line doesn't give one: command line, then
     * environment, then the default. */
    ARGH__DEF argh_opt *argh_env(argh_parser *p, void *target, const char *name);
    ARGH__DEF argh_opt *argh_implicit(argh_parser *p, void *target, const char *value);
    ARGH__DEF argh_opt *argh_range(argh_parser *p, void *target, long lo, long hi);

    ARGH__DEF argh_opt *argh_required(argh_opt *opt);
    ARGH__DEF argh_opt *argh_optional(argh_opt *opt);
    ARGH__DEF argh_opt *argh_hidden(argh_opt *opt);
    ARGH__DEF argh_opt *argh_negatable(argh_opt *opt);
    ARGH__DEF argh_opt *argh_once(argh_opt *opt);
    ARGH__DEF argh_opt *argh_metavar(argh_opt *opt, const char *metavar);

    /* Sets constraints between options, a table ending with ARGH_RULES_END:
     *
     *     static const argh_rule rules[] = {
     *         ARGH_AT_MOST_ONE(&json, &yaml),
     *         ARGH_REQUIRES(&tls_key, &tls_cert),
     *         ARGH_RULES_END
     *     };
     *
     * A rule applies when all its variables belong to options that are active
     * (the program's own options and those of the selected command). */
    ARGH__DEF void argh_rules(argh_parser *p, const argh_rule *rules);

    /* Runs fn after all other checks passed, for anything rules can't say */
    ARGH__DEF void argh_set_validator(argh_parser *p, argh_validate_fn fn, void *ctx);

    /* For validators: records an error with this message and returns false.
     * The message must outlive the parser (a string literal is ideal). */
    ARGH__DEF bool argh_fail(argh_parser *p, const char *message);

    /* ============================================================================
     * Parsing and results
     * ============================================================================ */

    /* Returns true when the program should continue. Returns false after
     * printing help, the version, or an error; then return argh_exit_code().
     * argv is reordered in place: positionals end up first, in order. */
    ARGH__DEF bool argh_parse(argh_parser *p, int argc, char **argv);

    /* 0 after --help/--version or success, 2 after a usage error. */
    ARGH__DEF int argh_exit_code(const argh_parser *p);

    /* True if the option bound to target appeared on the command line. */
    ARGH__DEF bool argh_given(const argh_parser *p, const void *target);

#ifndef ARGH_NO_COMMANDS
    /* The command that was selected (the deepest one), NULL without commands. */
    ARGH__DEF const argh_cmd *argh_command(const argh_parser *p);

    /* Calls the selected command's handler and returns its result, or 0 if the
     * command has no handler. */
    ARGH__DEF int argh_run(argh_parser *p, void *user);
#endif

    /* The error from the last argh_parse(), code ARGH_E_NONE if none. */
    ARGH__DEF const argh_error *argh_last_error(const argh_parser *p);

    /* Formats the last error as one line without a trailing newline.
     * Returns the full length, like snprintf; output is truncated to fit. */
    ARGH__DEF size_t argh_format_error(const argh_parser *p, char *buf, size_t size);

    ARGH__DEF void argh_print_help(const argh_parser *p);

    /* ============================================================================
     * Table macros
     *
     *   static const argh_opt opts[] = {
     *       ARGH_FLAG('v', "verbose", &verbose, "Verbose output"),
     *       ARGH_STRING('o', "output", &out, "Output file", ARGH_REQUIRED),
     *       ARGH_END
     *   };
     *
     * After the help text come two optional arguments: flags, and the name of
     * the value in help (metavar), which needs flags before it (0 for none):
     *
     *       ARGH_STRING(0, "tls-key", &key, "Client key", 0, "<file>"),
     *
     * The compiler warns if the variable has the wrong type for the option.
     * ============================================================================ */

/* The traditional MSVC preprocessor passes __VA_ARGS__ on as one token.
 * An extra expansion pass splits it into separate arguments. */
#define ARGH__EXPAND(x) x
#define ARGH__FIRST(...) ARGH__EXPAND(ARGH__FIRST_(__VA_ARGS__, ~))
#define ARGH__FIRST_(a, ...) a
#define ARGH__SECOND(...) ARGH__EXPAND(ARGH__SECOND_(__VA_ARGS__, 0, ~))
#define ARGH__SECOND_(a, b, ...) b
#define ARGH__THIRD(...) ARGH__EXPAND(ARGH__THIRD_(__VA_ARGS__, NULL, NULL, ~))
#define ARGH__THIRD_(a, b, c, ...) c
#define ARGH__THIRD_OR_0(...) ARGH__EXPAND(ARGH__THIRD_(__VA_ARGS__, 0, 0, ~))

/* Constant-expression type check: both ?: branches must be compatible */
#define ARGH__TARGET(type, ptr) ((void *)(1 ? (ptr) : (type *)0))

#define ARGH__OPT(s, l, kind, type, target, extra, ...)                                    \
    {                                                                                      \
        (char)(s), (l), (unsigned char)(kind), (unsigned char)(ARGH__SECOND(__VA_ARGS__)), \
            ARGH__TARGET(type, target), (const void *)(extra), ARGH__FIRST(__VA_ARGS__),   \
            ARGH__THIRD(__VA_ARGS__)                                                       \
    }

#define ARGH_FLAG(s, l, target, ...) ARGH__OPT(s, l, ARGH__K_FLAG, bool, target, NULL, __VA_ARGS__)
#define ARGH_COUNT(s, l, target, ...) ARGH__OPT(s, l, ARGH__K_COUNT, int, target, NULL, __VA_ARGS__)
#define ARGH_INT(s, l, target, ...) ARGH__OPT(s, l, ARGH__K_INT, int, target, NULL, __VA_ARGS__)
#define ARGH_LONG(s, l, target, ...) ARGH__OPT(s, l, ARGH__K_LONG, long, target, NULL, __VA_ARGS__)
#define ARGH_UINT(s, l, target, ...) ARGH__OPT(s, l, ARGH__K_UINT, unsigned, target, NULL, __VA_ARGS__)
#define ARGH_SIZE(s, l, target, ...) ARGH__OPT(s, l, ARGH__K_SIZE, size_t, target, NULL, __VA_ARGS__)
#ifndef ARGH_NO_FLOAT
#define ARGH_DOUBLE(s, l, target, ...) ARGH__OPT(s, l, ARGH__K_DOUBLE, double, target, NULL, __VA_ARGS__)
#endif
#define ARGH_STRING(s, l, target, ...) ARGH__OPT(s, l, ARGH__K_STRING, const char *, target, NULL, __VA_ARGS__)
#define ARGH_ENUM(s, l, target, choices, ...) \
    ARGH__OPT(s, l, ARGH__K_ENUM, int, target, choices, __VA_ARGS__)
#define ARGH_LIST(s, l, target, ...) ARGH__OPT(s, l, ARGH__K_LIST, argh_values, target, NULL, __VA_ARGS__)
#define ARGH_POS(name, target, ...) ARGH__OPT(0, name, ARGH__K_POS, const char *, target, NULL, __VA_ARGS__)
#define ARGH_REST(name, target, ...) ARGH__OPT(0, name, ARGH__K_REST, argh_values, target, NULL, __VA_ARGS__)
/* No type check here: the argh_type decides what target points to */
#define ARGH_CUSTOM(s, l, target, type, ...)                                                        \
    {                                                                                               \
        (char)(s), (l), (unsigned char)ARGH__K_CUSTOM, (unsigned char)(ARGH__SECOND(__VA_ARGS__)),   \
            (void *)(target), (const void *)(type), ARGH__FIRST(__VA_ARGS__),                       \
            ARGH__THIRD(__VA_ARGS__)                                                                \
    }
#define ARGH_GROUP(title) {0, NULL, (unsigned char)ARGH__K_GROUP, 0, NULL, NULL, (title), NULL}
/* A usage example shown in help; checked to parse in builds without NDEBUG */
#define ARGH_EXAMPLE(command, help) {0, (command), (unsigned char)ARGH__K_EXAMPLE, 0, NULL, NULL, (help), NULL}
/* The option bound to target takes its value from this environment variable
 * when the command line doesn't give one */
#define ARGH_ENV(target, name) {0, (name), (unsigned char)ARGH__K_ENV, 0, (void *)(target), NULL, NULL, NULL}
/* Right after an option: makes its value optional. --name alone (or -n)
 * stands for --name=value; another value then needs '=': --name=other. */
#define ARGH_IMPLICIT(target, value) {0, (value), (unsigned char)ARGH__K_IMPLICIT, 0, (void *)(target), NULL, NULL, NULL}
/* Right after an integer option: values outside lo..hi (inclusive) are out
 * of range. The bounds ride in pointer fields, so they need no storage. */
#define ARGH_RANGE(target, lo, hi) {0, NULL, (unsigned char)ARGH__K_RANGE, 0, (void *)(target), (const void *)(ptrdiff_t)(lo), NULL, (const char *)(ptrdiff_t)(hi)}
#define ARGH_END {0, NULL, (unsigned char)ARGH__K_END, 0, NULL, NULL, NULL, NULL}

    /* ============================================================================
     * Command macros
     *
     *   static const argh_cmd commands[] = {
     *       ARGH_CMD("build", "Build the project", build_opts, run_build),
     *       ARGH_CMD("clean", "Remove build output", NULL),
     *       ARGH_CMD_GROUP("remote", "Manage remotes", remote_commands),
     *       ARGH_CMD_END
     *   };
     *
     * ARGH_CMD(name, help, options[, handler[, flags]]): the handler is optional.
     * Flags: ARGH_POSIX ends options at the command's first positional, so
     * `tool exec prog --its-flag` passes --its-flag on without `--`.
     * ARGH_CMD_GROUP(name, help, subcommands): a command that only holds
     * other commands.
     * ============================================================================ */

#ifndef ARGH_NO_COMMANDS
#define ARGH_CMD(name, help, ...) \
    {(name), (help), ARGH__FIRST(__VA_ARGS__), NULL, ARGH__SECOND(__VA_ARGS__), ARGH__THIRD_OR_0(__VA_ARGS__)}
#define ARGH_CMD_GROUP(name, help, subs) {(name), (help), NULL, (subs), NULL, 0}
#define ARGH_CMD_END {NULL, NULL, NULL, NULL, NULL, 0}
#endif

#ifdef __cplusplus
}
#endif

#endif /* ARGH_H_INCLUDED */

/* ============================================================================
 * Implementation
 * ============================================================================ */

#ifdef ARGH_IMPLEMENTATION
#ifndef ARGH_IMPLEMENTATION_DONE
#define ARGH_IMPLEMENTATION_DONE

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#ifndef ARGH_NO_FLOAT
#include <math.h>
#endif
#include <stdint.h>
#ifndef ARGH_NO_STDIO
#include <stdio.h>
#endif
#include <stdlib.h>

/* Reads an environment variable for ARGH_ENV; NULL when it isn't set.
 * Define it before the implementation to read settings from elsewhere or to
 * fake the environment in tests. Firmware (ARGH_NO_STDIO) has no environment
 * by default, which keeps getenv out of the image. */
#ifndef ARGH_GETENV
#ifdef ARGH_NO_STDIO
/* No environment: ARGH_ENV entries are accepted and do nothing */
#define ARGH__NO_ENV
#elif defined(_MSC_VER)
/* MSVC deprecates getenv (C4996), which /WX turns into an error. The value
 * is only read, never kept past argh_parse, so getenv is safe here. */
#pragma warning(push)
#pragma warning(disable : 4996)
static const char *argh__getenv(const char *name)
{
    return getenv(name);
}
#pragma warning(pop)
#define ARGH_GETENV(name) argh__getenv(name)
#else
#define ARGH_GETENV(name) getenv(name)
#endif
#endif
#include <string.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /* Problems found by setup calls, reported by argh_parse */
    enum
    {
        ARGH__P_NONE = 0,
        ARGH__P_TABLES,
        ARGH__P_BUILDER
    };

    enum
    {
        ARGH__S_READY = 0,
        ARGH__S_OK,
        ARGH__S_HELP,
        ARGH__S_VERSION,
        ARGH__S_ERROR,
        ARGH__S_CHECKING /* parsing an example: check everything, write nothing */
    };

/* Values reach your variables except while an example is checked. Release
 * builds never check examples, so the test disappears there. */
#ifndef NDEBUG
#define ARGH__WRITING(p) ((p)->argh__status != ARGH__S_CHECKING)
#else
#define ARGH__WRITING(p) 1
#endif

/* Widest left column in help before descriptions move to the next line */
#define ARGH__HELP_COLUMN_MAX 30

/* Iterates all entries on the active path: the parser's tables, then the
 * options of each selected command. idx is a stable index for the "seen"
 * bitset. The _T form takes the slot counter name so loops can be nested. */
#define ARGH__EACH_T(p, t, o, idx)                                                     \
    for (int t = 0, idx = 0; t < argh__slot_count(p); t++)                             \
        for (const argh_opt *o = argh__slot(p, t); o->kind != ARGH__K_END; o++, idx++)
#define ARGH__EACH(p, o, idx) ARGH__EACH_T(p, argh__t, o, idx)

    static const argh_opt argh__empty_table[1] = {ARGH_END};

/* Command state. With ARGH_NO_COMMANDS these are constants, and the compiler
 * drops every branch that deals with commands. */
#ifdef ARGH_NO_COMMANDS
#define ARGH__DEPTH(p) 0
#define ARGH__LEAF(p) ((const argh_cmd *)NULL)
#else
#define ARGH__DEPTH(p) ((p)->argh__depth)
#define ARGH__LEAF(p) ((p)->argh__depth ? (p)->argh__path[(p)->argh__depth - 1] : (const argh_cmd *)NULL)
#endif

    /* Slots: tables first, then one per command on the path */
    static int argh__slot_count(const argh_parser *p)
    {
        return p->argh__table_count + ARGH__DEPTH(p);
    }

    static const argh_opt *argh__slot(const argh_parser *p, int slot)
    {
        const argh_opt *t = NULL;
        if (slot < p->argh__table_count)
            return p->argh__tables[slot];
#ifndef ARGH_NO_COMMANDS
        t = p->argh__path[slot - p->argh__table_count]->opts;
#endif
        return t ? t : argh__empty_table;
    }

    /* Subcommands expected at the current level, NULL if none */
    static const argh_cmd *argh__level_commands(const argh_parser *p)
    {
#ifdef ARGH_NO_COMMANDS
        (void)p;
        return NULL;
#else
        return p->argh__depth ? p->argh__path[p->argh__depth - 1]->subs : p->argh__commands;
#endif
    }

#ifndef ARGH_NO_COMMANDS
    static const argh_cmd *argh__find_command(const argh_cmd *cmds, const char *name)
    {
        for (; cmds && cmds->name; cmds++)
            if (strcmp(cmds->name, name) == 0)
                return cmds;
        return NULL;
    }
#endif

    /* ------------------------------------------------------------------------
     * Output helpers
     * ------------------------------------------------------------------------ */

    static void argh__stdio_write(void *ctx, bool to_stderr, const char *text, size_t len)
    {
        (void)ctx;
#ifdef ARGH_NO_STDIO
        (void)to_stderr;
        (void)text;
        (void)len;
#else
        fwrite(text, 1, len, to_stderr ? stderr : stdout);
#endif
    }

    static void argh__outn(const argh_parser *p, bool err, const char *s, size_t n)
    {
        if (s && n)
            p->argh__write(p->argh__write_ctx, err, s, n);
    }

    static void argh__out(const argh_parser *p, bool err, const char *s)
    {
        if (s)
            argh__outn(p, err, s, strlen(s));
    }

    static void argh__spaces(const argh_parser *p, int n)
    {
        static const char spaces[] = "                                ";
        while (n > 0)
        {
            int k = n < 32 ? n : 32;
            argh__outn(p, 0, spaces, (size_t)k);
            n -= k;
        }
    }

    /* Bounded string builder: tracks the full length like snprintf */
    typedef struct
    {
        char *buf;
        size_t size;
        size_t len;
    } argh__sb;

    static void argh__sb_putn(argh__sb *b, const char *s, size_t n)
    {
        size_t i;
        for (i = 0; i < n && s[i]; i++)
        {
            if (b->len + 1 < b->size)
                b->buf[b->len] = s[i];
            b->len++;
        }
        if (b->size)
            b->buf[b->len < b->size ? b->len : b->size - 1] = '\0';
    }

    static void argh__sb_put(argh__sb *b, const char *s)
    {
        if (s)
            argh__sb_putn(b, s, strlen(s));
    }

    static void argh__sb_char(argh__sb *b, char c)
    {
        argh__sb_putn(b, &c, 1);
    }

    /* Wide enough for unsigned long and size_t, with no 64-bit math where
     * both are 32 bits (most firmware) */
#if SIZE_MAX > ULONG_MAX
    typedef size_t argh__uint;
#else
    typedef unsigned long argh__uint;
#endif

    /* Decimal text of u; buf needs 24 bytes. Avoids pulling in printf. */
    static char *argh__fmt_uint(char *buf, argh__uint u)
    {
        char *s = buf + 23;
        *s = '\0';
        do
            *--s = (char)('0' + u % 10);
        while (u /= 10);
        return s;
    }

    static const char *argh__fmt_long(char *buf, long v)
    {
        /* Negate as unsigned so LONG_MIN works */
        char *s = argh__fmt_uint(buf, v < 0 ? 0UL - (unsigned long)v : (unsigned long)v);
        if (v < 0)
            *--s = '-';
        return s;
    }

#if defined(ARGH_NO_STDIO) && !defined(ARGH_NO_FLOAT)
    /* A short form of %g for help defaults: up to 6 decimals, trailing zeros
     * dropped. Values it cannot show plainly give NULL (no default shown). */
    static const char *argh__fmt_double(char *buf, double v)
    {
        char *s = buf;
        double a = v < 0 ? -v : v;
        unsigned long ip;
        unsigned long frac;
        int i;

        if (!isfinite(v) || a >= 1e9 || (a != 0 && a < 1e-6))
            return NULL;
        ip = (unsigned long)a;
        frac = (unsigned long)((a - (double)ip) * 1e6 + 0.5);
        if (frac >= 1000000UL)
        {
            ip++;
            frac = 0;
        }
        if (v < 0 && (ip || frac))
            *s++ = '-';
        {
            char digits[24];
            const char *d = argh__fmt_long(digits, (long)ip);
            size_t n = strlen(d);
            memcpy(s, d, n);
            s += n;
        }
        if (frac)
        {
            *s++ = '.';
            for (i = 5; i >= 0; i--)
            {
                s[i] = (char)('0' + frac % 10);
                frac /= 10;
            }
            for (i = 6; s[i - 1] == '0'; i--)
                ;
            s += i;
        }
        *s = '\0';
        return buf;
    }
#endif

    /* ------------------------------------------------------------------------
     * Option lookup
     * ------------------------------------------------------------------------ */

    static bool argh__is_option_kind(int kind)
    {
        return kind >= ARGH__K_FLAG && kind <= ARGH__K_CUSTOM;
    }

    /* For option kinds only: every one but flags and counters takes a value */
    static bool argh__takes_value(const argh_opt *o)
    {
        return o->kind != ARGH__K_FLAG && o->kind != ARGH__K_COUNT;
    }

    /* ARGH_IMPLICIT and ARGH_RANGE come right after their option, in any
     * order. Every option has an entry after it (at least ARGH_END), so
     * finding one costs a read or two, not a search. */
    static const argh_opt *argh__after(const argh_opt *o, int kind)
    {
        const argh_opt *e;
        /* They are the last kinds: one compare settles the usual case */
        for (e = o + 1; e->kind >= ARGH__K_IMPLICIT && e->target == o->target; e++)
            if (e->kind == kind)
                return e;
        return NULL;
    }

    static long argh__range_lo(const argh_opt *r)
    {
        return (long)(ptrdiff_t)r->extra;
    }

    static long argh__range_hi(const argh_opt *r)
    {
        return (long)(ptrdiff_t)r->metavar;
    }

    static const argh_opt *argh__find_long(const argh_parser *p, const char *name, size_t len, int *index)
    {
        ARGH__EACH(p, o, i)
        {
            /* Compared in place: names are short, and most differ in the
             * first character, so a call to strncmp costs more than it saves */
            const char *l = o->long_name;
            size_t k = 0;
            if (!l || l[0] != name[0] || !argh__is_option_kind(o->kind))
                continue;
            while (k < len && l[k] == name[k])
                k++;
            if (k == len && l[len] == '\0')
            {
                *index = i;
                return o;
            }
        }
        return NULL;
    }

    static const argh_opt *argh__find_short(const argh_parser *p, char c, int *index)
    {
        ARGH__EACH(p, o, i)
        {
            if (argh__is_option_kind(o->kind) && o->short_name == c)
            {
                *index = i;
                return o;
            }
        }
        return NULL;
    }

    /* Indices past ARGH_MAX_OPTS are ignored here and reported as a
     * configuration error after the scan */
    static bool argh__seen(const argh_parser *p, int index)
    {
        if (index >= ARGH_MAX_OPTS)
            return false;
        return (p->argh__seen[index >> 3] >> (index & 7)) & 1u;
    }

    static void argh__mark(argh_parser *p, int index)
    {
        if (index < ARGH_MAX_OPTS)
            p->argh__seen[index >> 3] = (unsigned char)(p->argh__seen[index >> 3] | (1u << (index & 7)));
    }

    /* ------------------------------------------------------------------------
     * Value conversion: strict, whole string, no surrounding spaces
     * ------------------------------------------------------------------------ */

    /* Decimal or 0x hex with an optional sign. Octal is never used. */
    static bool argh__int_syntax(const char *s, int *base)
    {
        if (*s == '+' || *s == '-')
            s++;
        if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
        {
            s += 2;
            *base = 16;
            if (!*s)
                return false;
            for (; *s; s++)
                if (!isxdigit((unsigned char)*s))
                    return false;
            return true;
        }
        *base = 10;
        if (!*s)
            return false;
        for (; *s; s++)
            if (!isdigit((unsigned char)*s))
                return false;
        return true;
    }

    /* No minus sign: strtoul would wrap "-1" around to the maximum. The digit
     * loop works for any width and keeps strtol out of firmware images. */
    static argh_err argh__parse_uint(const char *s, argh__uint hi, argh__uint *out)
    {
        int base;
        argh__uint v = 0;
        if (*s == '-' || !argh__int_syntax(s, &base))
            return ARGH_E_INVALID_VALUE;
        s += (*s == '+') + (base == 16 ? 2 : 0);
        for (; *s; s++)
        {
            unsigned d = isdigit((unsigned char)*s) ? (unsigned)(*s - '0')
                                                    : (unsigned)(tolower((unsigned char)*s) - 'a' + 10);
            if (d > hi || v > (hi - d) / (unsigned)base)
                return ARGH_E_OUT_OF_RANGE;
            v = v * (unsigned)base + d;
        }
        *out = v;
        return ARGH_E_NONE;
    }

    /* The magnitude goes through argh__parse_uint, bounded by -lo or hi */
    static argh_err argh__parse_long(const char *s, long lo, long hi, long *out)
    {
        bool neg = *s == '-';
        argh__uint u;
        argh_err e;
        if (neg && s[1] == '+')
            return ARGH_E_INVALID_VALUE;
        e = argh__parse_uint(s + neg, neg ? 0UL - (unsigned long)lo : (unsigned long)hi, &u);
        if (e == ARGH_E_NONE)
            /* Built from u - 1 so that LONG_MIN doesn't overflow */
            *out = neg ? (u ? -(long)(u - 1) - 1 : 0) : (long)u;
        return e;
    }

#ifndef ARGH_NO_FLOAT
    static argh_err argh__parse_double(const char *s, double *out)
    {
        char *end;
        double v;
        /* Rejects leading spaces, "inf" and "nan", which strtod accepts */
        if (!(*s == '+' || *s == '-' || *s == '.' || isdigit((unsigned char)*s)))
            return ARGH_E_INVALID_VALUE;
        v = strtod(s, &end);
        if (end == s || *end != '\0')
            return ARGH_E_INVALID_VALUE;
        if (!isfinite(v))
            return ARGH_E_OUT_OF_RANGE;
        *out = v;
        return ARGH_E_NONE;
    }
#endif

    static bool argh__ieq(const char *a, const char *b)
    {
        for (; *a && *b; a++, b++)
            if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
                return false;
        return *a == *b;
    }

    static argh_err argh__parse_bool(const char *s, bool *out)
    {
        if (argh__ieq(s, "true") || argh__ieq(s, "yes") || argh__ieq(s, "on") || strcmp(s, "1") == 0)
            *out = true;
        else if (argh__ieq(s, "false") || argh__ieq(s, "no") || argh__ieq(s, "off") || strcmp(s, "0") == 0)
            *out = false;
        else
            return ARGH_E_INVALID_VALUE;
        return ARGH_E_NONE;
    }

    /* Converts and stores one value. v is NULL for flags and counters. */
    /* reason: set to the argh_type's explanation when a custom parse fails */
    /* write is false while an example is checked: values are converted and
     * checked, but nothing reaches the variables. A list's capacity and a
     * custom type's parse function need the variable, so they are skipped. */
    static argh_err argh__store(const argh_opt *o, const char *v, bool negated, const char **reason, bool write)
    {
        const argh_opt *r;
        long l;
        argh__uint u;
#ifndef ARGH_NO_FLOAT
        double d;
#endif
        bool b;
        argh_err e;

        switch (o->kind)
        {
        case ARGH__K_FLAG:
            if (!v)
                b = !negated;
            else if ((e = argh__parse_bool(v, &b)) != ARGH_E_NONE)
                return e;
            if (write)
                *(bool *)o->target = b;
            return ARGH_E_NONE;
        case ARGH__K_COUNT:
            if (write && *(int *)o->target < INT_MAX)
                (*(int *)o->target)++;
            return ARGH_E_NONE;
        case ARGH__K_INT:
        case ARGH__K_LONG:
            if (o->kind == ARGH__K_INT)
                e = argh__parse_long(v, INT_MIN, INT_MAX, &l);
            else
                e = argh__parse_long(v, LONG_MIN, LONG_MAX, &l);
            if (e != ARGH_E_NONE)
                return e;
            if ((r = argh__after(o, ARGH__K_RANGE)) != NULL && (l < argh__range_lo(r) || l > argh__range_hi(r)))
                return ARGH_E_OUT_OF_RANGE;
            if (write && o->kind == ARGH__K_INT)
                *(int *)o->target = (int)l;
            else if (write)
                *(long *)o->target = l;
            return ARGH_E_NONE;
        case ARGH__K_UINT:
        case ARGH__K_SIZE:
            if ((e = argh__parse_uint(v, o->kind == ARGH__K_UINT ? UINT_MAX : SIZE_MAX, &u)) != ARGH_E_NONE)
                return e;
            /* Bounds of an unsigned option are never negative (checked) */
            if ((r = argh__after(o, ARGH__K_RANGE)) != NULL &&
                (u < (argh__uint)argh__range_lo(r) || u > (argh__uint)argh__range_hi(r)))
                return ARGH_E_OUT_OF_RANGE;
            if (write && o->kind == ARGH__K_UINT)
                *(unsigned *)o->target = (unsigned)u;
            else if (write)
                *(size_t *)o->target = (size_t)u;
            return ARGH_E_NONE;
#ifndef ARGH_NO_FLOAT
        case ARGH__K_DOUBLE:
            if ((e = argh__parse_double(v, &d)) != ARGH_E_NONE)
                return e;
            if (write)
                *(double *)o->target = d;
            return ARGH_E_NONE;
#endif
        case ARGH__K_STRING:
            if (write)
                *(const char **)o->target = v;
            return ARGH_E_NONE;
        case ARGH__K_ENUM:
        {
            const char *const *choices = (const char *const *)o->extra;
            int i;
            for (i = 0; choices && choices[i]; i++)
            {
                if (strcmp(choices[i], v) == 0)
                {
                    if (write)
                        *(int *)o->target = i;
                    return ARGH_E_NONE;
                }
            }
            return ARGH_E_INVALID_VALUE;
        }
        case ARGH__K_LIST:
        {
            argh_values *list = (argh_values *)o->target;
            if (!write)
                return ARGH_E_NONE;
            if (list->count >= list->capacity)
                return ARGH_E_TOO_MANY_VALUES;
            list->items[list->count++] = v;
            return ARGH_E_NONE;
        }
        case ARGH__K_CUSTOM:
            if (!write)
                return ARGH_E_NONE;
            *reason = ((const argh_type *)o->extra)->parse(v, o->target);
            return *reason ? ARGH_E_INVALID_VALUE : ARGH_E_NONE;
        default:
            return ARGH_E_NONE;
        }
    }

    /* ------------------------------------------------------------------------
     * Parsing
     * ------------------------------------------------------------------------ */

    static int argh__fail(argh_parser *p, argh_err code, int index, const argh_opt *o,
                          const char *value, char short_name)
    {
        argh_error *e = &p->argh__error;
        e->code = code;
        e->argv_index = index;
        e->opt = o;
        e->value = value;
        e->short_name = short_name;
        return ARGH__S_ERROR;
    }

    static bool argh__auto_help(const argh_parser *p)
    {
        return !(p->argh__flags & ARGH_NO_AUTO_HELP);
    }

    static int argh__config_error(argh_parser *p, const char *detail, const argh_opt *o)
    {
        p->argh__error.detail = detail;
        return argh__fail(p, ARGH_E_CONFIG, -1, o, NULL, 0);
    }

    /* Option bound to target on the active path, and its "seen" index */
    static const argh_opt *argh__find_target(const argh_parser *p, const void *target, int *index);

    /* The ARGH_IMPLICIT value of a value option, or NULL. The entry follows
     * its option directly, so this costs one look on every parse. Every
     * option has an entry after it: at least ARGH_END. */
    static const char *argh__implicit(const argh_opt *o)
    {
        const argh_opt *e = argh__takes_value(o) ? argh__after(o, ARGH__K_IMPLICIT) : NULL;
        return e ? e->long_name : NULL;
    }

    static int argh__apply(argh_parser *p, const argh_opt *o, int index, const char *v,
                           bool negated, int argi, char short_name)
    {
        argh_err e;
        const char *reason = NULL;
        if ((o->flags & ARGH_ONCE) && argh__seen(p, index))
            return argh__fail(p, ARGH_E_REPEATED, argi, o, NULL, short_name);
        e = argh__store(o, v, negated, &reason, ARGH__WRITING(p));
        if (e != ARGH_E_NONE)
        {
            p->argh__error.detail = reason;
            return argh__fail(p, e, argi, o, v, short_name);
        }
        argh__mark(p, index);
        return ARGH__S_OK;
    }

    /* Moves argv[i] to argv[*w], shifting the options in between right.
     * Keeps argv a permutation, with positionals in their original order. */
    static void argh__move_positional(char **argv, int *w, int i)
    {
        char *arg = argv[i];
        int k;
        for (k = i; k > *w; k--)
            argv[k] = argv[k - 1];
        argv[*w] = arg;
        (*w)++;
    }

    static bool argh__next_is_value(int i, int argc, char **argv)
    {
        return i + 1 < argc && argv[i + 1] && strcmp(argv[i + 1], "--") != 0;
    }

#ifndef ARGH_NO_COMMANDS
    /* `tool help remote add`: selects the named commands, then asks for help */
    static int argh__help_command(argh_parser *p, int argc, char **argv, int i)
    {
        for (; i < argc && argv[i]; i++)
        {
            const argh_cmd *level = argh__level_commands(p);
            const argh_cmd *c = argh__find_command(level, argv[i]);
            if (!level)
                return argh__fail(p, ARGH_E_UNEXPECTED_ARGUMENT, i, NULL, argv[i], 0);
            if (!c)
                return argh__fail(p, ARGH_E_UNKNOWN_COMMAND, i, NULL, argv[i], 0);
            if (p->argh__depth >= ARGH_MAX_DEPTH)
                return argh__config_error(p, "commands nested deeper than ARGH_MAX_DEPTH", NULL);
            p->argh__path[p->argh__depth] = c;
            p->argh__depth++;
        }
        return ARGH__S_HELP;
    }
#endif

    /* One pass over argv. With apply == false nothing is written: the pass
     * only finds out whether help or version was requested, so that help can
     * show the defaults before any option changes them. */
    static int argh__scan(argh_parser *p, int argc, char **argv, bool apply, int *positional_count)
    {
        int w = 1;
        bool only_positionals = false;
        int i;

#ifndef ARGH_NO_COMMANDS
        p->argh__depth = 0;
#endif
        for (i = 1; i < argc && argv[i]; i++)
        {
            char *arg = argv[i];
            int rc = ARGH__S_OK;

            if (only_positionals || arg[0] != '-' || arg[1] == '\0')
            {
#ifndef ARGH_NO_COMMANDS
                const argh_cmd *level = only_positionals ? NULL : argh__level_commands(p);
                if (level)
                {
                    /* At a level with subcommands, a positional names one */
                    const argh_cmd *c = argh__find_command(level, arg);
                    if (!c && argh__auto_help(p) && strcmp(arg, "help") == 0)
                        return argh__help_command(p, argc, argv, i + 1);
                    if (!c)
                        rc = argh__fail(p, ARGH_E_UNKNOWN_COMMAND, i, NULL, arg, 0);
                    else if (p->argh__depth >= ARGH_MAX_DEPTH)
                        rc = argh__config_error(p, "commands nested deeper than ARGH_MAX_DEPTH", NULL);
                    else
                    {
                        p->argh__path[p->argh__depth] = c;
                        p->argh__depth++;
                    }
                    if (rc != ARGH__S_OK && apply)
                        return rc;
                    continue;
                }
#endif
                if (apply)
                    argh__move_positional(argv, &w, i);
                if (p->argh__flags & ARGH_POSIX)
                    only_positionals = true;
#ifndef ARGH_NO_COMMANDS
                else if (p->argh__depth && (p->argh__path[p->argh__depth - 1]->flags & ARGH_POSIX))
                    only_positionals = true;
#endif
                continue;
            }

            if (arg[1] == '-' && arg[2] == '\0')
            {
                only_positionals = true;
                continue;
            }

            if (arg[1] == '-')
            {
                /* --name, --name=value, --no-name */
                const char *name = arg + 2;
                const char *eq = strchr(name, '=');
                size_t len = eq ? (size_t)(eq - name) : strlen(name);
                const argh_opt *o;
                const char *v = NULL;
                bool negated = false;
                int index = 0;

                o = argh__find_long(p, name, len, &index);
                if (argh__auto_help(p))
                {
                    if (len == 4 && strncmp(name, "help", 4) == 0)
                        return ARGH__S_HELP;
                    /* A command may have its own --version, like
                     * `cargo install --version 1.0`; it wins */
                    if (!o && p->argh__version && len == 7 && strncmp(name, "version", 7) == 0)
                        return ARGH__S_VERSION;
                }
                if (!o && len > 3 && strncmp(name, "no-", 3) == 0)
                {
                    o = argh__find_long(p, name + 3, len - 3, &index);
                    if (o && o->kind == ARGH__K_FLAG && (o->flags & ARGH_NEGATABLE))
                        negated = true;
                    else
                        o = NULL;
                }
                if (!o)
                    rc = argh__fail(p, ARGH_E_UNKNOWN_OPTION, i, NULL, arg, 0);
                else if (argh__takes_value(o))
                {
                    if (eq)
                        v = eq + 1;
                    else if ((v = argh__implicit(o)) != NULL)
                        ; /* --color alone; the next argument is not its value */
                    else if (argh__next_is_value(i, argc, argv))
                        v = argv[++i];
                    else
                        rc = argh__fail(p, ARGH_E_MISSING_VALUE, i, o, NULL, 0);
                }
                else if (eq && (o->kind != ARGH__K_FLAG || negated))
                    rc = argh__fail(p, ARGH_E_UNEXPECTED_VALUE, i, o, eq + 1, 0);
                else if (eq)
                    v = eq + 1;

                if (rc == ARGH__S_OK && apply)
                    rc = argh__apply(p, o, index, v, negated, i, 0);
            }
            else
            {
                /* -v, -abc, -ofile, -o file */
                const char *c;
                int argi = i;
                for (c = arg + 1; *c && rc == ARGH__S_OK; c++)
                {
                    const argh_opt *o;
                    int index = 0;

                    if (*c == '=')
                    {
                        rc = argh__fail(p, ARGH_E_SHORT_EQUALS, argi, NULL, arg, c[-1]);
                        break;
                    }
                    o = argh__find_short(p, *c, &index);
                    if (argh__auto_help(p))
                    {
                        if (*c == 'h')
                            return ARGH__S_HELP;
                        if (!o && *c == 'V' && p->argh__version)
                            return ARGH__S_VERSION;
                    }

                    if (!o)
                    {
                        rc = argh__fail(p, ARGH_E_UNKNOWN_OPTION, argi, NULL, arg, *c);
                        break;
                    }
                    if (argh__takes_value(o) && !argh__implicit(o))
                    {
                        const char *v = NULL;
                        if (c[1] == '=')
                            rc = argh__fail(p, ARGH_E_SHORT_EQUALS, argi, o, arg, *c);
                        else if (c[1])
                            v = c + 1;
                        else if (argh__next_is_value(i, argc, argv))
                            v = argv[++i];
                        else
                            rc = argh__fail(p, ARGH_E_MISSING_VALUE, argi, o, NULL, *c);
                        if (rc == ARGH__S_OK && apply)
                            rc = argh__apply(p, o, index, v, false, argi, *c);
                        break; /* the rest of the cluster was the value */
                    }
                    /* A flag, a counter, or a value option used bare like -c */
                    if (apply)
                        rc = argh__apply(p, o, index, argh__implicit(o), false, argi, *c);
                }
            }

            /* The dry pass ignores errors: it only looks for help/version */
            if (rc != ARGH__S_OK && apply)
                return rc;
        }

        *positional_count = w - 1;
        return ARGH__S_OK;
    }

    /* Hands positionals to ARGH__K_POS / ARGH__K_REST, checks required ones */
    static int argh__assign_positionals(argh_parser *p, char **argv, int count)
    {
        int used = 0;
        ARGH__EACH(p, o, index)
        {
            if (o->kind == ARGH__K_POS)
            {
                if (used < count)
                {
                    if (ARGH__WRITING(p))
                        *(const char **)o->target = argv[1 + used];
                    used++;
                    argh__mark(p, index);
                }
                else if (!(o->flags & ARGH_OPTIONAL))
                {
                    return argh__fail(p, ARGH_E_MISSING_REQUIRED, -1, o, NULL, 0);
                }
            }
            else if (o->kind == ARGH__K_REST)
            {
                int n = count - used;
                if (ARGH__WRITING(p))
                {
                    argh_values *rest = (argh_values *)o->target;
                    rest->items = (const char **)(void *)(argv + 1 + used);
                    rest->count = n;
                    rest->capacity = n;
                }
                used = count;
                if (n > 0)
                    argh__mark(p, index);
                else if (o->flags & ARGH_REQUIRED)
                    return argh__fail(p, ARGH_E_MISSING_REQUIRED, -1, o, NULL, 0);
            }
        }
        if (used < count)
            return argh__fail(p, ARGH_E_UNEXPECTED_ARGUMENT, 1 + used, NULL, argv[1 + used], 0);
        return ARGH__S_OK;
    }

#ifndef NDEBUG
    static bool argh__table_has_target(const argh_opt *o, const void *target)
    {
        for (; o && o->kind != ARGH__K_END; o++)
            if (o->target == target && o->kind < ARGH__K_GROUP) /* options and positionals */
                return true;
        return false;
    }

#ifndef ARGH_NO_COMMANDS
    static bool argh__tree_has_target(const argh_cmd *cmds, const void *target)
    {
        for (; cmds && cmds->name; cmds++)
            if (argh__table_has_target(cmds->opts, target) || argh__tree_has_target(cmds->subs, target))
                return true;
        return false;
    }
#endif

    /* Is target bound to any option at all, selected command or not? */
    static bool argh__target_exists(const argh_parser *p, const void *target)
    {
        int t;
        for (t = 0; t < p->argh__table_count; t++)
            if (argh__table_has_target(p->argh__tables[t], target))
                return true;
#ifndef ARGH_NO_COMMANDS
        return argh__tree_has_target(p->argh__commands, target);
#else
        return false;
#endif
    }
#endif

    /* Evaluates the rule table. Rules with a variable outside the active path
     * are skipped: they belong to a command that wasn't selected. */
    static int argh__check_rules(argh_parser *p)
    {
        const argh_rule *r;
        for (r = p->argh__rules; r && r->kind != ARGH__R_END; r++)
        {
            int given = 0, count = 0, i, index;
            bool active = true;
            const argh_opt *first_given = NULL, *missing = NULL;

            for (i = 0; i < ARGH_RULE_MAX && r->targets[i]; i++)
            {
                const argh_opt *o = argh__find_target(p, r->targets[i], &index);
                count++;
                if (!o)
                {
#ifndef NDEBUG
                    if (!argh__target_exists(p, r->targets[i]))
                        return argh__config_error(p, "a rule refers to a variable that no option is bound to", NULL);
#endif
                    active = false;
                    break;
                }
                if (argh__seen(p, index))
                {
                    given++;
                    if (!first_given)
                        first_given = o;
                }
                else if (i > 0 && !missing)
                {
                    missing = o;
                }
            }
            if (!active)
                continue;
            if (count < 2)
                return argh__config_error(p, "a rule needs at least two variables", NULL);

            p->argh__error.rule = r;
            switch (r->kind)
            {
            case ARGH__R_AT_MOST_ONE:
                if (given > 1)
                    return argh__fail(p, ARGH_E_CONFLICT, -1, first_given, NULL, 0);
                break;
            case ARGH__R_EXACTLY_ONE:
                if (given > 1)
                    return argh__fail(p, ARGH_E_CONFLICT, -1, first_given, NULL, 0);
                if (given == 0)
                    return argh__fail(p, ARGH_E_ONE_REQUIRED, -1, NULL, NULL, 0);
                break;
            case ARGH__R_AT_LEAST_ONE:
                if (given == 0)
                    return argh__fail(p, ARGH_E_ONE_REQUIRED, -1, NULL, NULL, 0);
                break;
            case ARGH__R_REQUIRES:
            {
                int head_index;
                argh__find_target(p, r->targets[0], &head_index);
                if (argh__seen(p, head_index) && missing)
                    return argh__fail(p, ARGH_E_REQUIRES, -1, missing, NULL, 0);
                break;
            }
            default:
                return argh__config_error(p, "unknown rule kind", NULL);
            }
            p->argh__error.rule = NULL;
        }
        return ARGH__S_OK;
    }

    static int argh__run_validator(argh_parser *p)
    {
        if (!p->argh__validator || p->argh__validator(p, p->argh__validator_ctx))
            return ARGH__S_OK;
        /* A validator that returned false without argh_fail() */
        if (p->argh__error.code != ARGH_E_CUSTOM)
            return argh__fail(p, ARGH_E_CUSTOM, -1, NULL, NULL, 0);
        return ARGH__S_ERROR;
    }

    /* Options the command line left out take their ARGH_ENV variable, if set.
     * The value goes through the same checks as on the command line; an
     * error has argv_index -1, which is how messages tell the source. A
     * counter takes a number, a list one value. */
    static int argh__apply_env(argh_parser *p)
    {
#ifdef ARGH__NO_ENV
        (void)p;
#else
        ARGH__EACH(p, e, unused)
        {
            const argh_opt *o;
            const char *v;
            int index = 0;
            (void)unused;
            if (e->kind != ARGH__K_ENV)
                continue;
            o = argh__find_target(p, e->target, &index);
            if (!o || !argh__is_option_kind(o->kind))
                return argh__config_error(p, "ARGH_ENV names a variable that no option on this path is bound to", e);
            if (argh__seen(p, index) || !(v = ARGH_GETENV(e->long_name)))
                continue;
            if (o->kind == ARGH__K_COUNT)
            {
                long n;
                argh_err err = argh__parse_long(v, 0, INT_MAX, &n);
                if (err != ARGH_E_NONE)
                    return argh__fail(p, err, -1, o, v, 0);
                if (ARGH__WRITING(p))
                    *(int *)o->target = (int)n;
                argh__mark(p, index);
                continue;
            }
            if (argh__apply(p, o, index, v, false, -1, 0) != ARGH__S_OK)
                return ARGH__S_ERROR;
        }
#endif
        return ARGH__S_OK;
    }

    static int argh__check_required(argh_parser *p)
    {
        ARGH__EACH(p, o, index)
        {
            if (argh__is_option_kind(o->kind) && (o->flags & ARGH_REQUIRED) && !argh__seen(p, index))
                return argh__fail(p, ARGH_E_MISSING_REQUIRED, -1, o, NULL, 0);
        }
        return ARGH__S_OK;
    }

#ifndef ARGH_NO_COMMANDS
    static bool argh__has_positionals(const argh_opt *o)
    {
        for (; o && o->kind != ARGH__K_END; o++)
            if (o->kind == ARGH__K_POS || o->kind == ARGH__K_REST)
                return true;
        return false;
    }
#endif

#if !defined(NDEBUG) && !defined(ARGH_NO_COMMANDS)
    /* Walks the command tree: names, reserved "help", positionals next to
     * subcommands. Debug builds only, like the duplicate check. */
    static int argh__check_commands(argh_parser *p, const argh_cmd *cmds, int depth)
    {
        const argh_cmd *a, *b;
        if (depth > ARGH_MAX_DEPTH)
            return argh__config_error(p, "commands nested deeper than ARGH_MAX_DEPTH", NULL);
        for (a = cmds; a->name; a++)
        {
            int st;
            if (argh__auto_help(p) && strcmp(a->name, "help") == 0)
                return argh__config_error(p, "command name 'help' is reserved, see ARGH_NO_AUTO_HELP", NULL);
            for (b = a + 1; b->name; b++)
                if (strcmp(a->name, b->name) == 0)
                    return argh__config_error(p, "command defined twice", NULL);
            if (a->subs && argh__has_positionals(a->opts))
                return argh__config_error(p, "a command with subcommands cannot have positional arguments", NULL);
            if (argh__auto_help(p))
            {
                /* -h/--help stay reserved everywhere; -V/--version may be
                 * redefined by a command */
                const argh_opt *o;
                for (o = a->opts; o && o->kind != ARGH__K_END; o++)
                    if (argh__is_option_kind(o->kind) &&
                        (o->short_name == 'h' || (o->long_name && strcmp(o->long_name, "help") == 0)))
                        return argh__config_error(p, "name reserved for help, see ARGH_NO_AUTO_HELP", o);
            }
            if (a->subs && (st = argh__check_commands(p, a->subs, depth + 1)) != ARGH__S_OK)
                return st;
        }
        return ARGH__S_OK;
    }
#endif

#ifndef ARGH_NO_SUGGEST
    /* Longest name considered for suggestions */
#define ARGH__SUGGEST_MAX 64

    /* Optimal string alignment distance: edits (insert, delete, replace)
     * plus swaps of adjacent characters, so "verbsoe" is 1 from "verbose".
     * Three rolling rows on the stack; returns a large value past the cap. */
    static int argh__distance(const char *a, size_t alen, const char *b, size_t blen)
    {
        unsigned char rows[3][ARGH__SUGGEST_MAX + 1];
        unsigned char *prev2 = rows[0], *prev = rows[1], *cur = rows[2];
        size_t i, j;

        if (alen > ARGH__SUGGEST_MAX || blen > ARGH__SUGGEST_MAX)
            return 1000;
        for (j = 0; j <= blen; j++)
            prev[j] = (unsigned char)j;
        for (i = 1; i <= alen; i++)
        {
            unsigned char *t;
            cur[0] = (unsigned char)i;
            for (j = 1; j <= blen; j++)
            {
                int cost = a[i - 1] != b[j - 1];
                int best = prev[j] + 1;              /* delete */
                if (cur[j - 1] + 1 < best)
                    best = cur[j - 1] + 1;           /* insert */
                if (prev[j - 1] + cost < best)
                    best = prev[j - 1] + cost;       /* replace */
                if (i > 1 && j > 1 && a[i - 1] == b[j - 2] && a[i - 2] == b[j - 1] &&
                    prev2[j - 2] + 1 < best)
                    best = prev2[j - 2] + 1;         /* swap */
                cur[j] = (unsigned char)best;
            }
            t = prev2;
            prev2 = prev;
            prev = cur;
            cur = t;
        }
        return prev[blen];
    }

    typedef struct
    {
        const char *typed;
        size_t len;
        const char *best;
        int best_distance;
    } argh__suggester;

    /* Close enough: at most 2 edits, and at most a third of the longer word,
     * so short inputs don't get far-fetched matches ("ad" -> "add" yes,
     * "jb" -> "jobs" no) */
    static void argh__consider(argh__suggester *s, const char *candidate)
    {
        int d;
        size_t clen, longer;
        if (!candidate)
            return;
        clen = strlen(candidate);
        longer = clen > s->len ? clen : s->len;
        d = argh__distance(s->typed, s->len, candidate, clen);
        if (d <= 2 && (size_t)d * 3 <= longer && d < s->best_distance)
        {
            s->best = candidate;
            s->best_distance = d;
        }
    }

    /* Runs only after a failed parse, so it costs nothing on success */
    static void argh__suggest(argh_parser *p)
    {
        argh_error *e = &p->argh__error;
        argh__suggester s;
        s.best = NULL;
        s.best_distance = 1000;

        if (e->code == ARGH_E_UNKNOWN_OPTION && !e->short_name && e->value)
        {
            const char *eq;
            s.typed = e->value + 2; /* skip "--" */
            eq = strchr(s.typed, '=');
            s.len = eq ? (size_t)(eq - s.typed) : strlen(s.typed);
            ARGH__EACH(p, o, index)
            {
                (void)index;
                if (argh__is_option_kind(o->kind) && !(o->flags & ARGH_HIDDEN))
                    argh__consider(&s, o->long_name);
            }
            if (argh__auto_help(p))
            {
                argh__consider(&s, "help");
                if (p->argh__version)
                    argh__consider(&s, "version");
            }
        }
        else if (e->code == ARGH_E_UNKNOWN_COMMAND && e->value)
        {
            const argh_cmd *c;
            s.typed = e->value;
            s.len = strlen(s.typed);
            for (c = argh__level_commands(p); c && c->name; c++)
                argh__consider(&s, c->name);
            if (argh__auto_help(p))
                argh__consider(&s, "help");
        }
        e->suggestion = s.best;
    }
#endif

    /* Duplicate names on the active path: quadratic, so debug builds only */
    static int argh__check_duplicates(argh_parser *p)
    {
#ifndef NDEBUG
        ARGH__EACH_T(p, argh__ta, a, ia)
        {
            if (!argh__is_option_kind(a->kind))
                continue;
            ARGH__EACH_T(p, argh__tb, b, ib)
            {
                if (ib <= ia || !argh__is_option_kind(b->kind))
                    continue;
                if ((a->short_name && a->short_name == b->short_name) ||
                    (a->long_name && b->long_name && a->long_name[0] == b->long_name[0] &&
                     strcmp(a->long_name, b->long_name) == 0))
                    return argh__config_error(p, "option defined twice", b);
            }
        }
#else
        (void)p;
#endif
        return ARGH__S_OK;
    }

    /* Catches mistakes in the definitions before any argument is read */
    static int argh__check_config(argh_parser *p)
    {
        int total = 0;

        if (p->argh__setup_problem == ARGH__P_TABLES)
            return argh__config_error(p, "more tables than ARGH_MAX_TABLES", NULL);
        if (p->argh__setup_problem == ARGH__P_BUILDER)
            return argh__config_error(p, "more builder options than ARGH_BUILDER_CAP", NULL);

        /* Runs on every parse, so the usual option gets through on a few
         * compares: reserved names are tested by first letter before strcmp */
        bool reserved = argh__auto_help(p);
        bool version = reserved && p->argh__version;
        ARGH__EACH(p, o, index)
        {
            const char *l = o->long_name;
            (void)index;
            total++;
            if (!o->target)
            {
                if (o->kind != ARGH__K_GROUP && o->kind != ARGH__K_EXAMPLE)
                    return argh__config_error(p, "option has no target variable", o);
                continue;
            }
            /* Enums and custom types need extra; one compare skips the kinds before them */
            if (o->kind >= ARGH__K_ENUM)
            {
                if (o->kind == ARGH__K_ENUM && !o->extra)
                    return argh__config_error(p, "enum option has no choices", o);
                if (o->kind == ARGH__K_CUSTOM && (!o->extra || !((const argh_type *)o->extra)->parse))
                    return argh__config_error(p, "custom option has no argh_type with a parse function", o);
            }
            if (reserved && argh__is_option_kind(o->kind) &&
                (o->short_name == 'h' || (version && o->short_name == 'V') ||
                 (l && ((l[0] == 'h' && strcmp(l, "help") == 0) || (version && l[0] == 'v' && strcmp(l, "version") == 0)))))
                return argh__config_error(p, "name reserved for help/version, see ARGH_NO_AUTO_HELP", o);
        }
        if (total > ARGH_MAX_OPTS)
            return argh__config_error(p, "more options than ARGH_MAX_OPTS", NULL);

#ifndef ARGH_NO_COMMANDS
        if (p->argh__commands)
        {
            for (int t = 0; t < p->argh__table_count; t++)
                if (argh__has_positionals(p->argh__tables[t]))
                    return argh__config_error(p, "positional arguments cannot be combined with commands", NULL);
#ifndef NDEBUG
            {
                int st = argh__check_commands(p, p->argh__commands, 1);
                if (st != ARGH__S_OK)
                    return st;
            }
#endif
        }
#endif
        return ARGH__S_OK;
    }

    /* Checks that need the selected command path */
    static int argh__check_path(argh_parser *p)
    {
        /* Without a command, argh__check_config has counted these already */
        if (ARGH__DEPTH(p))
        {
            int total = 0;
            ARGH__EACH(p, o, index)
            {
                (void)o;
                total = index + 1;
            }
            if (total > ARGH_MAX_OPTS)
                return argh__config_error(p, "more options than ARGH_MAX_OPTS on this command path", NULL);
        }
        if (argh__level_commands(p))
            return argh__fail(p, ARGH_E_MISSING_COMMAND, -1, NULL, NULL, 0);
        return argh__check_duplicates(p);
    }

#ifndef NDEBUG
/* Longest example and most words in one, for the copy argh_parse checks */
#define ARGH__EXAMPLE_LEN 256
#define ARGH__EXAMPLE_WORDS 32

    /* Splits a command line into words, written to buf: spaces separate
     * words, '...' and "..." keep spaces inside one. Returns the number of
     * words, or -1 if the line doesn't fit or a quote is left open. */
    static int argh__split(const char *s, char *buf, size_t size, char **words, int max)
    {
        size_t n = 0;
        int count = 0;
        for (;;)
        {
            char quote = 0;
            while (*s == ' ')
                s++;
            if (!*s)
                break;
            if (count == max)
                return -1;
            words[count++] = buf + n;
            for (; *s && (quote || *s != ' '); s++)
            {
                if (!quote && (*s == '"' || *s == '\''))
                    quote = *s;
                else if (quote && *s == quote)
                    quote = 0;
                else if (n + 1 < size)
                    buf[n++] = *s;
                else
                    return -1;
            }
            if (quote || n + 1 >= size)
                return -1;
            buf[n++] = '\0';
        }
        words[count] = NULL;
        return count;
    }

    /* Parses one example like a real command line, writing to no variable.
     * buf belongs to argh_parse, so an error can still point into it while
     * the message is printed. --help or --version in an example is fine. */
    /* ARGH_IMPLICIT: directly after its option, which takes a value, has a
     * long name (so a value can still be given with '='), and accepts it */
    /* ARGH_IMPLICIT and ARGH_RANGE: right after their option (both may
     * follow it), and fitting it */
    static int argh__check_attached(argh_parser *p, const argh_opt *table, const argh_opt *e)
    {
        const char *reason = NULL;
        const argh_opt *o = e;
        while (o > table && (o->kind == ARGH__K_IMPLICIT || o->kind == ARGH__K_RANGE) && o->target == e->target)
            o--;
        if (e->kind == ARGH__K_IMPLICIT)
        {
            if (!argh__is_option_kind(o->kind) || o->target != e->target || !argh__implicit(o) || !o->long_name)
                return argh__config_error(p, "ARGH_IMPLICIT must directly follow a value option with a long name, bound to the same variable",
                                          argh__is_option_kind(o->kind) ? o : NULL);
            /* Through the range too, if there is one */
            if (!e->long_name || argh__store(o, e->long_name, false, &reason, false) != ARGH_E_NONE)
                return argh__config_error(p, "ARGH_IMPLICIT value is not valid for its option", o);
            return ARGH__S_OK;
        }
        if (!argh__is_option_kind(o->kind) || o->target != e->target ||
            !(o->kind == ARGH__K_INT || o->kind == ARGH__K_LONG || o->kind == ARGH__K_UINT || o->kind == ARGH__K_SIZE))
            return argh__config_error(p, "ARGH_RANGE must directly follow an integer option, bound to the same variable",
                                      argh__is_option_kind(o->kind) ? o : NULL);
        if (argh__range_lo(e) > argh__range_hi(e) ||
            (o->kind == ARGH__K_INT && (argh__range_lo(e) < INT_MIN || argh__range_hi(e) > INT_MAX)) ||
            (o->kind == ARGH__K_UINT && (argh__range_lo(e) < 0 || (unsigned long)argh__range_hi(e) > UINT_MAX)) ||
            (o->kind == ARGH__K_SIZE && argh__range_lo(e) < 0))
            return argh__config_error(p, "ARGH_RANGE bounds are reversed or outside the variable's type", o);
        return ARGH__S_OK;
    }

    static int argh__check_example(argh_parser *p, const argh_opt *ex, char *buf)
    {
        char *words[ARGH__EXAMPLE_WORDS + 1];
        int count = argh__split(ex->long_name, buf, ARGH__EXAMPLE_LEN, words, ARGH__EXAMPLE_WORDS);
        int positional_count = 0;
        int st;

        if (count < 1)
            return argh__config_error(p, "an example needs at most 256 characters, 32 words and closed quotes", ex);
        /* The first word is the program name, as in argv */
        memset(p->argh__seen, 0, sizeof(p->argh__seen));
        p->argh__status = ARGH__S_CHECKING;
        st = argh__scan(p, count, words, true, &positional_count);
        if (st == ARGH__S_OK)
            st = argh__check_path(p);
        if (st == ARGH__S_OK)
            st = argh__assign_positionals(p, words, positional_count);
        if (st == ARGH__S_OK)
            st = argh__check_required(p);
        if (st == ARGH__S_OK)
            st = p->argh__rule_check ? p->argh__rule_check(p) : ARGH__S_OK;
        p->argh__status = ARGH__S_READY;
        return st == ARGH__S_ERROR ? st : ARGH__S_OK;
    }

    static int argh__check_examples_in(argh_parser *p, const argh_opt *t, char *buf, const argh_opt **bad)
    {
        const argh_opt *table = t;
        for (; t && t->kind != ARGH__K_END; t++)
        {
            if (t->kind == ARGH__K_EXAMPLE && argh__check_example(p, t, buf) != ARGH__S_OK)
            {
                *bad = t;
                return ARGH__S_ERROR;
            }
            if ((t->kind == ARGH__K_IMPLICIT || t->kind == ARGH__K_RANGE) && argh__check_attached(p, table, t) != ARGH__S_OK)
                return ARGH__S_ERROR;
        }
        return ARGH__S_OK;
    }

#ifndef ARGH_NO_COMMANDS
    static int argh__check_examples_tree(argh_parser *p, const argh_cmd *c, char *buf, const argh_opt **bad)
    {
        for (; c && c->name; c++)
        {
            if (argh__check_examples_in(p, c->opts, buf, bad) != ARGH__S_OK ||
                argh__check_examples_tree(p, c->subs, buf, bad) != ARGH__S_OK)
                return ARGH__S_ERROR;
        }
        return ARGH__S_OK;
    }
#endif

    /* Checks the examples in the program's tables and in every command */
    static int argh__check_examples(argh_parser *p, char *buf, const argh_opt **bad)
    {
        int st = ARGH__S_OK;
        int t;
        for (t = 0; t < p->argh__table_count && st == ARGH__S_OK; t++)
            st = argh__check_examples_in(p, p->argh__tables[t], buf, bad);
#ifndef ARGH_NO_COMMANDS
        if (st == ARGH__S_OK)
            st = argh__check_examples_tree(p, p->argh__commands, buf, bad);
#endif
        if (st != ARGH__S_OK)
            return st;
        /* Leave nothing behind for the real parse */
        memset(p->argh__seen, 0, sizeof(p->argh__seen));
        memset(&p->argh__error, 0, sizeof(p->argh__error));
        p->argh__error.argv_index = -1;
#ifndef ARGH_NO_COMMANDS
        p->argh__depth = 0;
#endif
        return ARGH__S_OK;
    }

    /* Prints why an example fails while the error can still point into the
     * example's copy, then keeps it as a configuration error that names the
     * example and points to nothing temporary */
    static void argh__report_example(argh_parser *p, const argh_opt *ex)
    {
        char message[256];
        size_t n = argh_format_error(p, message, sizeof(message));
        argh__out(p, 1, p->argh__name);
        argh__out(p, 1, ": example '");
        argh__out(p, 1, ex->long_name);
        argh__out(p, 1, "' does not work: ");
        argh__outn(p, 1, message, n < sizeof(message) ? n : sizeof(message) - 1);
        argh__out(p, 1, "\n");
        memset(&p->argh__error, 0, sizeof(p->argh__error));
        p->argh__error.code = ARGH_E_CONFIG;
        p->argh__error.argv_index = -1;
        p->argh__error.opt = ex;
        p->argh__error.detail = "this example does not work";
    }
#endif

    /* Cheap check whether a dry pass for help/version is worth running */
    static bool argh__may_want_help(const argh_parser *p, int argc, char **argv)
    {
        int i;
        if (!argh__auto_help(p))
            return false;
        for (i = 1; i < argc && argv[i]; i++)
        {
            const char *a = argv[i];
            if (a[0] != '-')
                continue;
            if (a[1] == '-')
            {
                if (a[2] == '\0')
                    return false;
                if ((a[2] == 'h' && strncmp(a + 2, "help", 4) == 0) ||
                    (a[2] == 'v' && strncmp(a + 2, "version", 7) == 0))
                    return true;
            }
            else
            {
                /* -h or -V anywhere in a cluster, one pass */
                for (a++; *a; a++)
                    if (*a == 'h' || *a == 'V')
                        return true;
            }
        }
        return false;
    }

    /* "tool remote add": program name plus the selected commands */
    static void argh__out_path(const argh_parser *p, bool err)
    {
        int d;
        argh__out(p, err, p->argh__name);
        for (d = 0; d < ARGH__DEPTH(p); d++)
        {
#ifndef ARGH_NO_COMMANDS
            argh__out(p, err, " ");
            argh__out(p, err, p->argh__path[d]->name);
#endif
        }
    }

#ifndef ARGH_NO_COMMANDS
    static void argh__print_commands(const argh_parser *p, bool err, const argh_cmd *cmds)
    {
        const argh_cmd *c;
        int width = 0;
        for (c = cmds; c->name; c++)
            if ((int)strlen(c->name) > width)
                width = (int)strlen(c->name);
        argh__out(p, err, "Commands:\n");
        for (c = cmds; c->name; c++)
        {
            int pad = width - (int)strlen(c->name) + 2;
            argh__out(p, err, "  ");
            argh__out(p, err, c->name);
            while (pad-- > 0)
                argh__out(p, err, " ");
            argh__out(p, err, c->help);
            argh__out(p, err, "\n");
        }
    }
#endif

    static void argh__print_error(const argh_parser *p)
    {
        char buf[256];
        size_t n = argh_format_error(p, buf, sizeof(buf));
        argh__out(p, 1, p->argh__name);
        argh__out(p, 1, ": ");
        argh__outn(p, 1, buf, n < sizeof(buf) ? n : sizeof(buf) - 1);
        argh__out(p, 1, "\n");
#ifndef ARGH_NO_COMMANDS
        if (p->argh__error.code == ARGH_E_MISSING_COMMAND)
        {
            argh__out(p, 1, "\n");
            argh__print_commands(p, 1, argh__level_commands(p));
            argh__out(p, 1, "\n");
        }
#endif
        if (argh__auto_help(p) && p->argh__error.code != ARGH_E_CONFIG)
        {
            argh__out(p, 1, "Try '");
            argh__out_path(p, 1);
            argh__out(p, 1, " --help' for more information.\n");
        }
    }

    static const char *argh__basename(const char *path)
    {
        const char *base = path;
        for (; *path; path++)
            if (*path == '/' || *path == '\\')
                base = path + 1;
        return base;
    }

    /* ------------------------------------------------------------------------
     * Public API: setup and builder
     * ------------------------------------------------------------------------ */

    ARGH__DEF void argh_init(argh_parser *p, const char *name, const char *about)
    {
        /* Everything but the builder storage, most of the struct: argh__add
         * fills its entries and ends the list itself */
        size_t after = offsetof(argh_parser, argh__builder) + sizeof(p->argh__builder);
        memset(p, 0, offsetof(argh_parser, argh__builder));
        memset((char *)p + after, 0, sizeof(*p) - after);
        p->argh__builder[0].kind = ARGH__K_END;
        p->argh__name = name;
        p->argh__about = about;
        p->argh__write = argh__stdio_write;
    }

    ARGH__DEF void argh_version(argh_parser *p, const char *version)
    {
        p->argh__version = version;
    }

    ARGH__DEF void argh_set_flags(argh_parser *p, unsigned flags)
    {
        p->argh__flags = (unsigned char)flags;
    }

    ARGH__DEF void argh_set_writer(argh_parser *p, argh_write_fn write, void *ctx)
    {
        p->argh__write = write ? write : argh__stdio_write;
        p->argh__write_ctx = ctx;
    }

    ARGH__DEF void argh_table(argh_parser *p, const argh_opt *table)
    {
        if (p->argh__table_count >= ARGH_MAX_TABLES)
        {
            p->argh__setup_problem = ARGH__P_TABLES;
            return;
        }
        p->argh__tables[p->argh__table_count] = table;
        p->argh__table_count++;
    }

#ifndef ARGH_NO_COMMANDS
    ARGH__DEF void argh_commands(argh_parser *p, const argh_cmd *commands)
    {
        p->argh__commands = commands;
    }
#endif

    static argh_opt *argh__add(argh_parser *p, char short_name, const char *long_name, int kind,
                               void *target, const void *extra, const char *help)
    {
        argh_opt *o;
        if (p->argh__builder_count >= ARGH_BUILDER_CAP)
        {
            p->argh__setup_problem = ARGH__P_BUILDER;
            return NULL;
        }
        /* The builder joins the table list at its first use, so help order
         * follows the order of argh_table() and builder calls */
        if (p->argh__builder_count == 0)
        {
            argh_table(p, p->argh__builder);
            if (p->argh__setup_problem)
                return NULL;
        }
        o = &p->argh__builder[p->argh__builder_count];
        p->argh__builder_count++;
        o->short_name = short_name;
        o->long_name = long_name;
        o->kind = (unsigned char)kind;
        o->flags = 0;
        o->target = target;
        o->extra = extra;
        o->help = help;
        o->metavar = NULL;
        /* argh_init leaves the builder storage as it is: end the list here */
        p->argh__builder[p->argh__builder_count].kind = ARGH__K_END;
        return o;
    }

    ARGH__DEF argh_opt *argh_flag(argh_parser *p, char s, const char *l, bool *target, const char *help)
    {
        return argh__add(p, s, l, ARGH__K_FLAG, target, NULL, help);
    }

    ARGH__DEF argh_opt *argh_count(argh_parser *p, char s, const char *l, int *target, const char *help)
    {
        return argh__add(p, s, l, ARGH__K_COUNT, target, NULL, help);
    }

    ARGH__DEF argh_opt *argh_int(argh_parser *p, char s, const char *l, int *target, const char *help)
    {
        return argh__add(p, s, l, ARGH__K_INT, target, NULL, help);
    }

    ARGH__DEF argh_opt *argh_long(argh_parser *p, char s, const char *l, long *target, const char *help)
    {
        return argh__add(p, s, l, ARGH__K_LONG, target, NULL, help);
    }

    ARGH__DEF argh_opt *argh_uint(argh_parser *p, char s, const char *l, unsigned *target, const char *help)
    {
        return argh__add(p, s, l, ARGH__K_UINT, target, NULL, help);
    }

    ARGH__DEF argh_opt *argh_size(argh_parser *p, char s, const char *l, size_t *target, const char *help)
    {
        return argh__add(p, s, l, ARGH__K_SIZE, target, NULL, help);
    }

#ifndef ARGH_NO_FLOAT
    ARGH__DEF argh_opt *argh_double(argh_parser *p, char s, const char *l, double *target, const char *help)
    {
        return argh__add(p, s, l, ARGH__K_DOUBLE, target, NULL, help);
    }
#endif

    ARGH__DEF argh_opt *argh_string(argh_parser *p, char s, const char *l, const char **target, const char *help)
    {
        return argh__add(p, s, l, ARGH__K_STRING, (void *)target, NULL, help);
    }

    ARGH__DEF argh_opt *argh_enum(argh_parser *p, char s, const char *l, int *target,
                        const char *const *choices, const char *help)
    {
        return argh__add(p, s, l, ARGH__K_ENUM, target, choices, help);
    }

    ARGH__DEF argh_opt *argh_list(argh_parser *p, char s, const char *l, argh_values *target, const char *help)
    {
        return argh__add(p, s, l, ARGH__K_LIST, target, NULL, help);
    }

    ARGH__DEF argh_opt *argh_pos(argh_parser *p, const char *name, const char **target, const char *help)
    {
        return argh__add(p, 0, name, ARGH__K_POS, (void *)target, NULL, help);
    }

    ARGH__DEF argh_opt *argh_rest(argh_parser *p, const char *name, argh_values *target, const char *help)
    {
        return argh__add(p, 0, name, ARGH__K_REST, target, NULL, help);
    }

    ARGH__DEF argh_opt *argh_custom(argh_parser *p, char s, const char *l, void *target, const argh_type *type,
                          const char *help)
    {
        return argh__add(p, s, l, ARGH__K_CUSTOM, target, type, help);
    }

    ARGH__DEF argh_opt *argh_group(argh_parser *p, const char *title)
    {
        return argh__add(p, 0, NULL, ARGH__K_GROUP, NULL, NULL, title);
    }

    ARGH__DEF argh_opt *argh_example(argh_parser *p, const char *command, const char *help)
    {
        return argh__add(p, 0, command, ARGH__K_EXAMPLE, NULL, NULL, help);
    }

    ARGH__DEF argh_opt *argh_env(argh_parser *p, void *target, const char *name)
    {
        return argh__add(p, 0, name, ARGH__K_ENV, target, NULL, NULL);
    }

    ARGH__DEF argh_opt *argh_implicit(argh_parser *p, void *target, const char *value)
    {
        return argh__add(p, 0, value, ARGH__K_IMPLICIT, target, NULL, NULL);
    }

    ARGH__DEF argh_opt *argh_range(argh_parser *p, void *target, long lo, long hi)
    {
        argh_opt *o = argh__add(p, 0, NULL, ARGH__K_RANGE, target, (const void *)(ptrdiff_t)lo, NULL);
        if (o)
            o->metavar = (const char *)(ptrdiff_t)hi;
        return o;
    }

    static argh_opt *argh__set_flag(argh_opt *opt, int flag)
    {
        if (opt)
            opt->flags = (unsigned char)(opt->flags | flag);
        return opt;
    }

    ARGH__DEF argh_opt *argh_required(argh_opt *opt) { return argh__set_flag(opt, ARGH_REQUIRED); }
    ARGH__DEF argh_opt *argh_optional(argh_opt *opt) { return argh__set_flag(opt, ARGH_OPTIONAL); }
    ARGH__DEF argh_opt *argh_hidden(argh_opt *opt) { return argh__set_flag(opt, ARGH_HIDDEN); }
    ARGH__DEF argh_opt *argh_negatable(argh_opt *opt) { return argh__set_flag(opt, ARGH_NEGATABLE); }
    ARGH__DEF argh_opt *argh_once(argh_opt *opt) { return argh__set_flag(opt, ARGH_ONCE); }

    ARGH__DEF argh_opt *argh_metavar(argh_opt *opt, const char *metavar)
    {
        if (opt)
            opt->metavar = metavar;
        return opt;
    }

    /* ------------------------------------------------------------------------
     * Public API: parsing and results
     * ------------------------------------------------------------------------ */

    ARGH__DEF bool argh_parse(argh_parser *p, int argc, char **argv)
    {
        int positional_count = 0;
        int st;
#ifndef NDEBUG
        char example[ARGH__EXAMPLE_LEN];
        const argh_opt *bad_example = NULL;
#endif

        memset(p->argh__seen, 0, sizeof(p->argh__seen));
        memset(&p->argh__error, 0, sizeof(p->argh__error));
        p->argh__error.argv_index = -1;
        if (!p->argh__name)
            p->argh__name = (argc > 0 && argv[0]) ? argh__basename(argv[0]) : "program";

#ifndef ARGH_NO_COMMANDS
        p->argh__depth = 0;
#endif
        st = argh__check_config(p);
#ifndef NDEBUG
        if (st == ARGH__S_OK)
            st = argh__check_examples(p, example, &bad_example);
#endif
        if (st == ARGH__S_OK && argh__may_want_help(p, argc, argv))
        {
            st = argh__scan(p, argc, argv, false, &positional_count);
            if (st == ARGH__S_OK)
            {
                /* The dry pass records errors it then ignores */
                memset(&p->argh__error, 0, sizeof(p->argh__error));
                p->argh__error.argv_index = -1;
            }
        }
        if (st == ARGH__S_OK)
            st = argh__scan(p, argc, argv, true, &positional_count);
        if (st == ARGH__S_OK)
            st = argh__check_path(p);
        if (st == ARGH__S_OK)
            st = argh__assign_positionals(p, argv, positional_count);
        if (st == ARGH__S_OK)
            st = argh__apply_env(p);
        if (st == ARGH__S_OK)
            st = argh__check_required(p);
        if (st == ARGH__S_OK)
            st = p->argh__rule_check ? p->argh__rule_check(p) : ARGH__S_OK;
        if (st == ARGH__S_OK)
            st = argh__run_validator(p);

#ifndef ARGH_NO_SUGGEST
        if (st == ARGH__S_ERROR)
            argh__suggest(p);
#endif
        p->argh__status = (unsigned char)st;
        switch (st)
        {
        case ARGH__S_OK:
            return true;
        case ARGH__S_HELP:
            argh_print_help(p);
            return false;
        case ARGH__S_VERSION:
            argh__out(p, 0, p->argh__name);
            argh__out(p, 0, " ");
            argh__out(p, 0, p->argh__version);
            argh__out(p, 0, "\n");
            return false;
        default:
#ifndef NDEBUG
            if (bad_example)
            {
                argh__report_example(p, bad_example);
                return false;
            }
#endif
            argh__print_error(p);
            return false;
        }
    }

    ARGH__DEF int argh_exit_code(const argh_parser *p)
    {
        return p->argh__status == ARGH__S_ERROR ? 2 : 0;
    }

    static const argh_opt *argh__find_target(const argh_parser *p, const void *target, int *index)
    {
        ARGH__EACH(p, o, i)
        {
            if (o->target == target && o->kind < ARGH__K_GROUP) /* options and positionals */
            {
                *index = i;
                return o;
            }
        }
        return NULL;
    }

    ARGH__DEF bool argh_given(const argh_parser *p, const void *target)
    {
        int index;
        return argh__find_target(p, target, &index) && argh__seen(p, index);
    }

    ARGH__DEF void argh_rules(argh_parser *p, const argh_rule *rules)
    {
        p->argh__rules = rules;
        p->argh__rule_check = argh__check_rules;
    }

    ARGH__DEF void argh_set_validator(argh_parser *p, argh_validate_fn fn, void *ctx)
    {
        p->argh__validator = fn;
        p->argh__validator_ctx = ctx;
    }

    ARGH__DEF bool argh_fail(argh_parser *p, const char *message)
    {
        argh__fail(p, ARGH_E_CUSTOM, -1, NULL, NULL, 0);
        p->argh__error.detail = message;
        return false;
    }

    ARGH__DEF const argh_error *argh_last_error(const argh_parser *p)
    {
        return &p->argh__error;
    }

#ifndef ARGH_NO_COMMANDS
    ARGH__DEF const argh_cmd *argh_command(const argh_parser *p)
    {
        return ARGH__LEAF(p);
    }

    ARGH__DEF int argh_run(argh_parser *p, void *user)
    {
        const argh_cmd *c = argh_command(p);
        return (c && c->run) ? c->run(p, user) : 0;
    }
#endif

    /* The ARGH_ENV variable of the option bound to target, or NULL. Always
     * NULL without an environment, so help and errors don't mention one. */
    static const char *argh__env_name(const argh_parser *p, const void *target)
    {
#ifdef ARGH__NO_ENV
        (void)p;
        (void)target;
#else
        ARGH__EACH(p, e, index)
        {
            (void)index;
            if (e->kind == ARGH__K_ENV && e->target == target)
                return e->long_name;
        }
#endif
        return NULL;
    }

    /* "1 to 64" in errors, "1..64" in help */
    static void argh__sb_range(argh__sb *b, const argh_opt *r, const char *between)
    {
        char num[24];
        argh__sb_put(b, argh__fmt_long(num, argh__range_lo(r)));
        argh__sb_put(b, between);
        argh__sb_put(b, argh__fmt_long(num, argh__range_hi(r)));
    }

    /* " in NAME" when the value came from the environment, which is when the
     * error has no argv position */
    static void argh__sb_from_env(argh__sb *b, const argh_parser *p, const argh_error *e)
    {
        const char *name = e->argv_index < 0 && e->opt ? argh__env_name(p, e->opt->target) : NULL;
        if (name)
        {
            argh__sb_put(b, " in ");
            argh__sb_put(b, name);
        }
    }

    /* "--name" if the option has a long name, "-x" otherwise, "<name>" for
     * positionals. used_short: the short name the user typed, if any. */
    static void argh__sb_opt_name(argh__sb *b, const argh_opt *o, char used_short)
    {
        if (o && o->kind == ARGH__K_EXAMPLE)
        {
            argh__sb_put(b, "example '");
            argh__sb_put(b, o->long_name);
            argh__sb_char(b, '\'');
        }
        else if (o && (o->kind == ARGH__K_POS || o->kind == ARGH__K_REST))
        {
            argh__sb_char(b, '<');
            argh__sb_put(b, o->long_name);
            argh__sb_char(b, '>');
        }
        else if (used_short)
        {
            argh__sb_char(b, '-');
            argh__sb_char(b, used_short);
        }
        else if (o && o->long_name)
        {
            argh__sb_put(b, "--");
            argh__sb_put(b, o->long_name);
        }
        else if (o)
        {
            argh__sb_char(b, '-');
            argh__sb_char(b, o->short_name);
        }
    }

    static void argh__sb_expected(argh__sb *b, const argh_opt *o)
    {
        switch (o->kind)
        {
        case ARGH__K_INT:
        case ARGH__K_LONG:
            argh__sb_put(b, "expected an integer");
            break;
        case ARGH__K_UINT:
        case ARGH__K_SIZE:
            argh__sb_put(b, "expected a non-negative integer");
            break;
        case ARGH__K_DOUBLE:
            argh__sb_put(b, "expected a number");
            break;
        case ARGH__K_FLAG:
            argh__sb_put(b, "expected true or false");
            break;
        case ARGH__K_ENUM:
        {
            const char *const *choices = (const char *const *)o->extra;
            int i;
            argh__sb_put(b, "expected one of: ");
            for (i = 0; choices && choices[i]; i++)
            {
                if (i)
                    argh__sb_put(b, ", ");
                argh__sb_put(b, choices[i]);
            }
            break;
        }
        default:
            argh__sb_put(b, "invalid value");
            break;
        }
    }

    ARGH__DEF size_t argh_format_error(const argh_parser *p, char *buf, size_t size)
    {
        const argh_error *e = &p->argh__error;
        argh__sb b;
        b.buf = buf;
        b.size = size;
        b.len = 0;
        if (size)
            buf[0] = '\0';

        switch (e->code)
        {
        case ARGH_E_NONE:
            break;
        case ARGH_E_UNKNOWN_OPTION:
            argh__sb_put(&b, "unknown option '");
            if (e->short_name)
            {
                argh__sb_char(&b, '-');
                argh__sb_char(&b, e->short_name);
            }
            else
            {
                const char *eq = strchr(e->value, '=');
                argh__sb_putn(&b, e->value, eq ? (size_t)(eq - e->value) : strlen(e->value));
            }
            argh__sb_char(&b, '\'');
            if (e->suggestion)
            {
                argh__sb_put(&b, " (did you mean '--");
                argh__sb_put(&b, e->suggestion);
                argh__sb_put(&b, "'?)");
            }
            break;
        case ARGH_E_MISSING_VALUE:
            argh__sb_put(&b, "option '");
            argh__sb_opt_name(&b, e->opt, e->short_name);
            argh__sb_put(&b, "' requires a value");
            break;
        case ARGH_E_INVALID_VALUE:
            argh__sb_put(&b, "invalid value '");
            argh__sb_put(&b, e->value);
            argh__sb_char(&b, '\'');
            argh__sb_from_env(&b, p, e);
            argh__sb_put(&b, " for '");
            argh__sb_opt_name(&b, e->opt, e->short_name);
            argh__sb_put(&b, "': ");
            if (e->opt->kind == ARGH__K_CUSTOM && e->detail)
                argh__sb_put(&b, e->detail);
            else
                argh__sb_expected(&b, e->opt);
            break;
        case ARGH_E_OUT_OF_RANGE:
            argh__sb_put(&b, "value '");
            argh__sb_put(&b, e->value);
            argh__sb_char(&b, '\'');
            argh__sb_from_env(&b, p, e);
            argh__sb_put(&b, " for '");
            argh__sb_opt_name(&b, e->opt, e->short_name);
            argh__sb_put(&b, "' is out of range");
            if (argh__after(e->opt, ARGH__K_RANGE))
            {
                argh__sb_put(&b, " (");
                argh__sb_range(&b, argh__after(e->opt, ARGH__K_RANGE), " to ");
                argh__sb_char(&b, ')');
            }
            break;
        case ARGH_E_UNEXPECTED_VALUE:
            argh__sb_put(&b, "option '");
            argh__sb_opt_name(&b, e->opt, e->short_name);
            argh__sb_put(&b, "' does not take a value");
            break;
        case ARGH_E_SHORT_EQUALS:
            argh__sb_put(&b, "short option '-");
            argh__sb_char(&b, e->short_name);
            argh__sb_put(&b, "' does not accept '=': write '-");
            argh__sb_char(&b, e->short_name);
            argh__sb_put(&b, " VALUE'");
            if (e->opt && e->opt->long_name)
            {
                argh__sb_put(&b, " or '--");
                argh__sb_put(&b, e->opt->long_name);
                argh__sb_put(&b, "=VALUE'");
            }
            break;
        case ARGH_E_UNEXPECTED_ARGUMENT:
            argh__sb_put(&b, "unexpected argument '");
            argh__sb_put(&b, e->value);
            argh__sb_char(&b, '\'');
            break;
        case ARGH_E_MISSING_REQUIRED:
            argh__sb_put(&b, e->opt && (e->opt->kind == ARGH__K_POS || e->opt->kind == ARGH__K_REST)
                                 ? "missing required argument '"
                                 : "missing required option '");
            argh__sb_opt_name(&b, e->opt, 0);
            argh__sb_char(&b, '\'');
            {
                const char *env = e->opt ? argh__env_name(p, e->opt->target) : NULL;
                if (env)
                {
                    argh__sb_put(&b, " (or set ");
                    argh__sb_put(&b, env);
                    argh__sb_char(&b, ')');
                }
            }
            break;
        case ARGH_E_REPEATED:
            argh__sb_put(&b, "option '");
            argh__sb_opt_name(&b, e->opt, e->short_name);
            argh__sb_put(&b, "' can only be given once");
            break;
        case ARGH_E_TOO_MANY_VALUES:
        {
            char num[24];
            argh__sb_put(&b, "too many values for '");
            argh__sb_opt_name(&b, e->opt, e->short_name);
            argh__sb_put(&b, "' (at most ");
            argh__sb_put(&b, argh__fmt_long(num, ((const argh_values *)e->opt->target)->capacity));
            argh__sb_char(&b, ')');
            break;
        }
        case ARGH_E_UNKNOWN_COMMAND:
            argh__sb_put(&b, "unknown command '");
            argh__sb_put(&b, e->value);
            argh__sb_char(&b, '\'');
            if (e->suggestion)
            {
                argh__sb_put(&b, " (did you mean '");
                argh__sb_put(&b, e->suggestion);
                argh__sb_put(&b, "'?)");
            }
            break;
        case ARGH_E_MISSING_COMMAND:
            if (ARGH__LEAF(p))
            {
                argh__sb_put(&b, "'");
                argh__sb_put(&b, ARGH__LEAF(p)->name);
                argh__sb_put(&b, "' needs a command");
            }
            else
            {
                argh__sb_put(&b, "missing command");
            }
            break;
        case ARGH_E_CONFLICT:
        case ARGH_E_ONE_REQUIRED:
        {
            /* Names the options of the rule: all of them, or only the given
             * ones for a conflict */
            int i, index, listed = 0, total = 0;
            const argh_opt *names[ARGH_RULE_MAX];
            for (i = 0; e->rule && i < ARGH_RULE_MAX && e->rule->targets[i]; i++)
            {
                const argh_opt *o = argh__find_target(p, e->rule->targets[i], &index);
                if (o && (e->code != ARGH_E_CONFLICT || argh__seen(p, index)))
                    names[total++] = o;
            }
            argh__sb_put(&b, e->code == ARGH_E_CONFLICT ? "options " : "one of ");
            for (listed = 0; listed < total; listed++)
            {
                if (listed)
                    argh__sb_put(&b, listed == total - 1 ? (e->code == ARGH_E_CONFLICT ? " and " : " or ") : ", ");
                argh__sb_char(&b, '\'');
                argh__sb_opt_name(&b, names[listed], 0);
                argh__sb_char(&b, '\'');
            }
            argh__sb_put(&b, e->code == ARGH_E_CONFLICT ? " cannot be used together" : " is required");
            break;
        }
        case ARGH_E_REQUIRES:
        {
            int index;
            const argh_opt *head = e->rule ? argh__find_target(p, e->rule->targets[0], &index) : NULL;
            argh__sb_put(&b, "option '");
            argh__sb_opt_name(&b, head, 0);
            argh__sb_put(&b, "' requires '");
            argh__sb_opt_name(&b, e->opt, 0);
            argh__sb_char(&b, '\'');
            break;
        }
        case ARGH_E_CUSTOM:
            argh__sb_put(&b, e->detail ? e->detail : "invalid arguments");
            break;
        case ARGH_E_CONFIG:
            argh__sb_put(&b, "configuration error: ");
            argh__sb_put(&b, e->detail);
            if (e->opt)
            {
                argh__sb_put(&b, " (");
                argh__sb_opt_name(&b, e->opt, 0);
                argh__sb_char(&b, ')');
            }
            break;
        }
        return b.len;
    }

    /* ------------------------------------------------------------------------
     * Help
     * ------------------------------------------------------------------------ */

    static void argh__sb_metavar(argh__sb *b, const argh_opt *o)
    {
        if (o->metavar)
        {
            argh__sb_put(b, o->metavar);
            return;
        }
        switch (o->kind)
        {
        case ARGH__K_INT:
        case ARGH__K_LONG:
        case ARGH__K_UINT:
        case ARGH__K_SIZE:
            /* <1..64> for a range, so help shows the limits */
            if (!argh__after(o, ARGH__K_RANGE))
            {
                argh__sb_put(b, "<n>");
                break;
            }
            argh__sb_char(b, '<');
            argh__sb_range(b, argh__after(o, ARGH__K_RANGE), "..");
            argh__sb_char(b, '>');
            break;
        case ARGH__K_DOUBLE:
            argh__sb_put(b, "<x>");
            break;
        case ARGH__K_CUSTOM:
        {
            const argh_type *t = (const argh_type *)o->extra;
            argh__sb_put(b, (t && t->metavar) ? t->metavar : "<value>");
            break;
        }
        case ARGH__K_ENUM:
        {
            const char *const *choices = (const char *const *)o->extra;
            int i;
            argh__sb_char(b, '<');
            for (i = 0; choices && choices[i]; i++)
            {
                if (i)
                    argh__sb_char(b, '|');
                argh__sb_put(b, choices[i]);
            }
            argh__sb_char(b, '>');
            break;
        }
        default:
            argh__sb_put(b, "<value>");
            break;
        }
    }

    /* Left help column for one entry, without indentation */
    static size_t argh__left_column(const argh_opt *o, char *buf, size_t size)
    {
        argh__sb b;
        b.buf = buf;
        b.size = size;
        b.len = 0;
        buf[0] = '\0';

        if (o->kind == ARGH__K_POS)
        {
            argh__sb_char(&b, (o->flags & ARGH_OPTIONAL) ? '[' : '<');
            argh__sb_put(&b, o->long_name);
            argh__sb_char(&b, (o->flags & ARGH_OPTIONAL) ? ']' : '>');
            return b.len;
        }
        if (o->kind == ARGH__K_REST)
        {
            argh__sb_char(&b, (o->flags & ARGH_REQUIRED) ? '<' : '[');
            argh__sb_put(&b, o->long_name);
            argh__sb_put(&b, (o->flags & ARGH_REQUIRED) ? ">..." : "...]");
            return b.len;
        }

        if (o->short_name)
        {
            argh__sb_char(&b, '-');
            argh__sb_char(&b, o->short_name);
            if (o->long_name)
                argh__sb_put(&b, ", ");
        }
        else
        {
            argh__sb_put(&b, "    ");
        }
        if (o->long_name)
        {
            argh__sb_put(&b, (o->kind == ARGH__K_FLAG && (o->flags & ARGH_NEGATABLE)) ? "--[no-]" : "--");
            argh__sb_put(&b, o->long_name);
        }
        if (argh__takes_value(o))
        {
            /* -c, --color[=<when>]: the value is optional */
            bool bare = argh__implicit(o) != NULL;
            argh__sb_put(&b, bare ? "[=" : " ");
            argh__sb_metavar(&b, o);
            if (bare)
                argh__sb_char(&b, ']');
        }
        return b.len;
    }

    /* "(default: ...)" from the variable's current value, written to buf, or
     * "(required)"; NULL when there is nothing to show */
    static const char *argh__help_default(const argh_opt *o, char *buf, size_t size)
    {
        char num[64];
        const char *text = NULL;
        argh__sb b;

        if (o->flags & ARGH_REQUIRED)
            return "(required)";
        switch (o->kind)
        {
        case ARGH__K_INT:
            text = argh__fmt_long(num, *(const int *)o->target);
            break;
        case ARGH__K_LONG:
            text = argh__fmt_long(num, *(const long *)o->target);
            break;
        case ARGH__K_UINT:
            text = argh__fmt_uint(num, *(const unsigned *)o->target);
            break;
        case ARGH__K_SIZE:
            text = argh__fmt_uint(num, *(const size_t *)o->target);
            break;
#ifndef ARGH_NO_FLOAT
        case ARGH__K_DOUBLE:
#ifdef ARGH_NO_STDIO
            text = argh__fmt_double(num, *(const double *)o->target);
#else
            snprintf(num, sizeof(num), "%g", *(const double *)o->target);
            text = num;
#endif
            break;
#endif
        case ARGH__K_STRING:
            text = *(const char *const *)o->target;
            break;
        case ARGH__K_CUSTOM:
        {
            const argh_type *t = (const argh_type *)o->extra;
            num[0] = '\0';
            if (t->format && t->format(o->target, num, sizeof(num)) && num[0])
            {
                num[sizeof(num) - 1] = '\0';
                text = num;
            }
            break;
        }
        case ARGH__K_ENUM:
        {
            const char *const *choices = (const char *const *)o->extra;
            int v = *(const int *)o->target;
            int i;
            for (i = 0; choices[i]; i++)
                if (i == v)
                    text = choices[i];
            break;
        }
        default:
            break;
        }
        if (!text)
            return NULL;
        b.buf = buf;
        b.size = size;
        b.len = 0;
        argh__sb_put(&b, "(default: ");
        argh__sb_put(&b, text);
        argh__sb_char(&b, ')');
        return buf;
    }

#if ARGH_HELP_WIDTH > 0
    /* Writes text from column *col, word by word, starting a new line at
     * column indent before a word would pass ARGH_HELP_WIDTH. A newline in
     * the text starts a new line too. A word longer than the line is kept
     * whole, and with whole the entire text is one word, so "(default: x)"
     * never splits. Calls continue where the last one ended, one space apart. */
    static void argh__wrap(const argh_parser *p, const char *text, int indent, int *col, bool whole)
    {
        while (*text)
        {
            const char *word = text;
            int len;
            if (whole)
            {
                text += strlen(text);
            }
            else
            {
                if (*text == ' ')
                {
                    text++;
                    continue;
                }
                if (*text == '\n')
                {
                    argh__out(p, 0, "\n");
                    argh__spaces(p, indent);
                    *col = indent;
                    text++;
                    continue;
                }
                while (*text && *text != ' ' && *text != '\n')
                    text++;
            }
            len = (int)(text - word);
            if (*col > indent)
            {
                if (*col + 1 + len > ARGH_HELP_WIDTH)
                {
                    argh__out(p, 0, "\n");
                    argh__spaces(p, indent);
                    *col = indent;
                }
                else
                {
                    argh__out(p, 0, " ");
                    (*col)++;
                }
            }
            argh__outn(p, 0, word, (size_t)len);
            *col += len;
        }
    }
#else
    /* Wrapping is off: the text as it is, one space after earlier text */
    static void argh__wrap(const argh_parser *p, const char *text, int indent, int *col, bool whole)
    {
        (void)whole;
        if (!*text)
            return;
        if (*col > indent)
            argh__out(p, 0, " ");
        argh__out(p, 0, text);
        *col = indent + 1;
    }
#endif

    static void argh__help_line(const argh_parser *p, const char *left, size_t left_len,
                                const char *help, int column, const argh_opt *o)
    {
        char def[96];
        int indent = 2 + column + 2;
        int col = indent;
        argh__spaces(p, 2);
        argh__outn(p, 0, left, left_len);
        if ((int)left_len > column)
        {
            argh__out(p, 0, "\n");
            argh__spaces(p, indent);
        }
        else
        {
            argh__spaces(p, column - (int)left_len + 2);
        }
        if (help)
            argh__wrap(p, help, indent, &col, false);
        if (o)
        {
            const char *env = argh__env_name(p, o->target);
            const char *d;
            if (env)
            {
                char tag[80];
                argh__sb b;
                b.buf = tag;
                b.size = sizeof(tag);
                b.len = 0;
                argh__sb_put(&b, "[env: ");
                argh__sb_put(&b, env);
                argh__sb_char(&b, ']');
                argh__wrap(p, tag, indent, &col, true);
            }
            d = argh__help_default(o, def, sizeof(def));
            if (d)
                argh__wrap(p, d, indent, &col, true);
        }
        argh__out(p, 0, "\n");
    }

    ARGH__DEF void argh_print_help(const argh_parser *p)
    {
        char left[128];
        int column = 0;
        bool has_positionals = false;
        bool has_globals = false;
        bool in_section = false;
        bool auto_help = argh__auto_help(p);
        const argh_cmd *cmds = argh__level_commands(p);
        const argh_cmd *c;
        /* Slots from own_slot on belong to the selected command (or to the
         * program without commands); earlier slots hold global options */
        int own_slot = ARGH__DEPTH(p) ? argh__slot_count(p) - 1 : 0;

        /* Column width: widest visible entry, capped */
        ARGH__EACH_T(p, slot, o, index)
        {
            size_t len;
            (void)index;
            if (o->kind >= ARGH__K_GROUP || (o->flags & ARGH_HIDDEN))
                continue;
            if (slot < own_slot && !argh__is_option_kind(o->kind))
                continue;
            if (slot < own_slot)
                has_globals = true;
            if (o->kind == ARGH__K_POS || o->kind == ARGH__K_REST)
                has_positionals = true;
            len = argh__left_column(o, left, sizeof(left));
            if ((int)len > column && len <= ARGH__HELP_COLUMN_MAX)
                column = (int)len;
        }
        for (c = cmds; c && c->name; c++)
            if ((int)strlen(c->name) > column && strlen(c->name) <= ARGH__HELP_COLUMN_MAX)
                column = (int)strlen(c->name);
        if (auto_help && column < 13)
            column = 13; /* "-V, --version" */

        argh__out(p, 0, "Usage: ");
        argh__out_path(p, 0);
        argh__out(p, 0, " [OPTIONS]");
        if (cmds)
            argh__out(p, 0, " <command>");
        ARGH__EACH_T(p, slot, o, index)
        {
            (void)index;
            if (slot >= own_slot && (o->kind == ARGH__K_POS || o->kind == ARGH__K_REST))
            {
                size_t len = argh__left_column(o, left, sizeof(left));
                argh__out(p, 0, " ");
                argh__outn(p, 0, left, len);
            }
        }
        argh__out(p, 0, "\n");

        {
            const char *about = ARGH__LEAF(p) ? ARGH__LEAF(p)->help : p->argh__about;
            if (about)
            {
                int col = 0;
                argh__out(p, 0, "\n");
                argh__wrap(p, about, 0, &col, false);
                argh__out(p, 0, "\n");
            }
        }

        if (has_positionals)
        {
            argh__out(p, 0, "\nArguments:\n");
            ARGH__EACH_T(p, slot, o, index)
            {
                (void)index;
                if (slot >= own_slot && (o->kind == ARGH__K_POS || o->kind == ARGH__K_REST) &&
                    !(o->flags & ARGH_HIDDEN))
                {
                    size_t len = argh__left_column(o, left, sizeof(left));
                    argh__help_line(p, left, len, o->help, column, NULL);
                }
            }
        }

        if (cmds)
        {
            argh__out(p, 0, "\nCommands:\n");
            for (c = cmds; c->name; c++)
                argh__help_line(p, c->name, strlen(c->name), c->help, column, NULL);
        }

        ARGH__EACH_T(p, slot, o, index)
        {
            size_t len;
            (void)index;
            if (slot < own_slot)
                continue;
            if (o->kind == ARGH__K_GROUP)
            {
                argh__out(p, 0, "\n");
                argh__out(p, 0, o->help);
                argh__out(p, 0, ":\n");
                in_section = true;
                continue;
            }
            if (!argh__is_option_kind(o->kind) || (o->flags & ARGH_HIDDEN))
                continue;
            if (!in_section)
            {
                argh__out(p, 0, "\nOptions:\n");
                in_section = true;
            }
            len = argh__left_column(o, left, sizeof(left));
            argh__help_line(p, left, len, o->help, column, o);
        }

        /* Options of the program and of parent commands, without group headings */
        if (has_globals)
        {
            argh__out(p, 0, "\nGlobal options:\n");
            ARGH__EACH_T(p, slot, o, index)
            {
                size_t len;
                (void)index;
                if (slot >= own_slot || !argh__is_option_kind(o->kind) || (o->flags & ARGH_HIDDEN))
                    continue;
                len = argh__left_column(o, left, sizeof(left));
                argh__help_line(p, left, len, o->help, column, o);
            }
            in_section = true;
        }

        if (auto_help)
        {
            argh__out(p, 0, in_section ? "\n" : "\nOptions:\n");
            argh__help_line(p, "-h, --help", 10, "Print help", column, NULL);
            if (p->argh__version)
            {
                /* A command may take over --version or -V (see argh__scan);
                 * list only the forms that still print the version */
                int index;
                bool long_free = !argh__find_long(p, "version", 7, &index);
                bool short_free = !argh__find_short(p, 'V', &index);
                if (long_free && short_free)
                    argh__help_line(p, "-V, --version", 13, "Print version", column, NULL);
                else if (long_free)
                    argh__help_line(p, "    --version", 13, "Print version", column, NULL);
                else if (short_free)
                    argh__help_line(p, "-V", 2, "Print version", column, NULL);
            }
        }

        /* Examples from this level's own tables: the program's, or the command's */
        {
            bool first = true;
            ARGH__EACH_T(p, slot, o, index)
            {
                (void)index;
                if (slot < own_slot || o->kind != ARGH__K_EXAMPLE)
                    continue;
                if (first)
                    argh__out(p, 0, "\nExamples:\n");
                first = false;
                argh__out(p, 0, "  ");
                argh__out(p, 0, o->long_name);
                argh__out(p, 0, "\n");
                if (o->help)
                {
                    int col = 6;
                    argh__spaces(p, 6);
                    argh__wrap(p, o->help, 6, &col, false);
                    argh__out(p, 0, "\n");
                }
            }
        }
    }

#undef ARGH__EACH
#undef ARGH__EACH_T

#ifdef __cplusplus
}
#endif

#endif /* ARGH_IMPLEMENTATION_DONE */
#endif /* ARGH_IMPLEMENTATION */

/*
 * ----------------------------------------------------------------------------
 * LICENSE
 * ----------------------------------------------------------------------------
 * MIT License
 *
 * Copyright (c) 2026 Ilya Brin
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
