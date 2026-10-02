/*
 * Test suite for argh.h
 *
 * Every test builds its own argv, parses it, and checks the variables, the
 * error, or the captured output. Output never reaches the terminal: a writer
 * collects it so help and error text can be compared exactly.
 */

#define ARGH_IMPLEMENTATION
#include "../argh.h"

#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) static void name(void)

/* A test passes only if it did not bump tests_failed */
#define RUN_TEST(name)                         \
    do                                         \
    {                                          \
        int failed_before = tests_failed;      \
        tests_run++;                           \
        reset_output();                        \
        name();                                \
        if (tests_failed == failed_before)     \
            tests_passed++;                    \
        else                                   \
            printf("  in %s\n", #name);        \
    } while (0)

#define ASSERT(cond)                                                                  \
    do                                                                                \
    {                                                                                 \
        if (!(cond))                                                                  \
        {                                                                             \
            printf("FAILED %s:%d: %s\n", __FILE__, __LINE__, #cond);                  \
            tests_failed++;                                                           \
            return;                                                                   \
        }                                                                             \
    } while (0)

#define ASSERT_EQ(a, b) ASSERT((a) == (b))
#define ASSERT_STR_EQ(a, b) ASSERT((a) != NULL && strcmp((a), (b)) == 0)
#define ASSERT_TRUE(a) ASSERT(a)
#define ASSERT_FALSE(a) ASSERT(!(a))

/* argv from a list of strings; argv[0] is "prog" */
#define ARGV(...)                                                  \
    char *argv[] = {(char *)"prog", __VA_ARGS__, NULL};            \
    int argc = (int)(sizeof(argv) / sizeof(argv[0])) - 1
#define ARGV0()                                  \
    char *argv[] = {(char *)"prog", NULL};       \
    int argc = 1

/* ----------------------------------------------------------------------------
 * Output capture
 * ---------------------------------------------------------------------------- */

static char out_text[8192];
static size_t out_len;
static char err_text[2048];
static size_t err_len;

static void reset_output(void)
{
    out_len = err_len = 0;
    out_text[0] = err_text[0] = '\0';
}

static void capture(void *ctx, bool to_stderr, const char *text, size_t len)
{
    char *buf = to_stderr ? err_text : out_text;
    size_t *used = to_stderr ? &err_len : &out_len;
    size_t cap = to_stderr ? sizeof(err_text) : sizeof(out_text);
    (void)ctx;
    if (*used + len >= cap)
        len = cap - *used - 1;
    memcpy(buf + *used, text, len);
    *used += len;
    buf[*used] = '\0';
}

static void setup(argh_parser *p)
{
    argh_init(p, "prog", NULL);
    argh_set_writer(p, capture, NULL);
}

/* Expected suffix of an error message, empty when suggestions are off */
#ifdef ARGH_NO_SUGGEST
#define DID_YOU_MEAN(name) ""
#else
#define DID_YOU_MEAN(name) " (did you mean '" name "'?)"
#endif

/* The one-line error message of the last parse */
static const char *error_text(const argh_parser *p)
{
    static char buf[256];
    argh_format_error(p, buf, sizeof(buf));
    return buf;
}

/* ============================================================================
 * Flags and counters
 * ============================================================================ */

TEST(test_flag_short_and_long)
{
    ARGV("-v", "--quiet");
    bool verbose = false, quiet = false, other = false;
    argh_parser p;
    setup(&p);
    argh_flag(&p, 'v', "verbose", &verbose, "");
    argh_flag(&p, 'q', "quiet", &quiet, "");
    argh_flag(&p, 'x', "other", &other, "");

    ASSERT_TRUE(argh_parse(&p, argc, argv));
    ASSERT_TRUE(verbose);
    ASSERT_TRUE(quiet);
    ASSERT_FALSE(other);
}

TEST(test_flag_cluster)
{
    ARGV("-abc");
    bool a = false, b = false, c = false;
    argh_parser p;
    setup(&p);
    argh_flag(&p, 'a', NULL, &a, "");
    argh_flag(&p, 'b', NULL, &b, "");
    argh_flag(&p, 'c', NULL, &c, "");

    ASSERT_TRUE(argh_parse(&p, argc, argv));
    ASSERT_TRUE(a && b && c);
}

TEST(test_flag_explicit_value)
{
    ARGV("--a=false", "--b=YES", "--c=0");
    bool a = true, b = false, c = true;
    argh_parser p;
    setup(&p);
    argh_flag(&p, 0, "a", &a, "");
    argh_flag(&p, 0, "b", &b, "");
    argh_flag(&p, 0, "c", &c, "");

    ASSERT_TRUE(argh_parse(&p, argc, argv));
    ASSERT_FALSE(a);
    ASSERT_TRUE(b);
    ASSERT_FALSE(c);
}

TEST(test_flag_invalid_value)
{
    ARGV("--verbose=maybe");
    bool verbose = false;
    argh_parser p;
    setup(&p);
    argh_flag(&p, 'v', "verbose", &verbose, "");

    ASSERT_FALSE(argh_parse(&p, argc, argv));
    ASSERT_EQ(argh_last_error(&p)->code, ARGH_E_INVALID_VALUE);
    ASSERT_STR_EQ(error_text(&p), "invalid value 'maybe' for '--verbose': expected true or false");
    ASSERT_FALSE(verbose);
}

TEST(test_flag_negatable)
{
    ARGV("--no-color");
    bool color = true;
    argh_parser p;
    setup(&p);
    argh_negatable(argh_flag(&p, 0, "color", &color, ""));

    ASSERT_TRUE(argh_parse(&p, argc, argv));
    ASSERT_FALSE(color);
}

TEST(test_flag_not_negatable)
{
    ARGV("--no-color");
    bool color = true;
    argh_parser p;
    setup(&p);
    argh_flag(&p, 0, "color", &color, "");

    ASSERT_FALSE(argh_parse(&p, argc, argv));
    ASSERT_STR_EQ(error_text(&p), "unknown option '--no-color'");
}

TEST(test_negated_flag_rejects_value)
{
    ARGV("--no-color=yes");
    bool color = true;
    argh_parser p;
    setup(&p);
    argh_negatable(argh_flag(&p, 0, "color", &color, ""));

    ASSERT_FALSE(argh_parse(&p, argc, argv));
    ASSERT_EQ(argh_last_error(&p)->code, ARGH_E_UNEXPECTED_VALUE);
}

TEST(test_count)
{
    ARGV("-vvv", "--verbose", "-v");
    int level = 0;
    argh_parser p;
    setup(&p);
    argh_count(&p, 'v', "verbose", &level, "");

    ASSERT_TRUE(argh_parse(&p, argc, argv));
    ASSERT_EQ(level, 5);
}

TEST(test_count_rejects_value)
{
    ARGV("--verbose=3");
    int level = 0;
    argh_parser p;
    setup(&p);
    argh_count(&p, 'v', "verbose", &level, "");

    ASSERT_FALSE(argh_parse(&p, argc, argv));
    ASSERT_STR_EQ(error_text(&p), "option '--verbose' does not take a value");
}

/* ============================================================================
 * Numbers
 * ============================================================================ */

TEST(test_int_forms)
{
    ARGV("-a4", "-b", "5", "--c=6", "--d", "7");
    int a = 0, b = 0, c = 0, d = 0;
    argh_parser p;
    setup(&p);
    argh_int(&p, 'a', NULL, &a, "");
    argh_int(&p, 'b', NULL, &b, "");
    argh_int(&p, 0, "c", &c, "");
    argh_int(&p, 0, "d", &d, "");

    ASSERT_TRUE(argh_parse(&p, argc, argv));
    ASSERT_EQ(a, 4);
    ASSERT_EQ(b, 5);
    ASSERT_EQ(c, 6);
    ASSERT_EQ(d, 7);
}

TEST(test_int_bases_and_signs)
{
    ARGV("--hex=0x1F", "--dec=010", "--neg", "-5", "--pos=+7", "--nhex=-0x10");
    int hex = 0, dec = 0, neg = 0, pos = 0, nhex = 0;
    argh_parser p;
    setup(&p);
    argh_int(&p, 0, "hex", &hex, "");
    argh_int(&p, 0, "dec", &dec, "");
    argh_int(&p, 0, "neg", &neg, "");
    argh_int(&p, 0, "pos", &pos, "");
    argh_int(&p, 0, "nhex", &nhex, "");

    ASSERT_TRUE(argh_parse(&p, argc, argv));
    ASSERT_EQ(hex, 31);
    ASSERT_EQ(dec, 10); /* never octal */
    ASSERT_EQ(neg, -5);
    ASSERT_EQ(pos, 7);
    ASSERT_EQ(nhex, -16);
}

static argh_err parse_int_value(const char *text)
{
    char *argv[] = {(char *)"prog", (char *)"-n", (char *)text, NULL};
    int n = 0;
    argh_parser p;
    setup(&p);
    argh_int(&p, 'n', "num", &n, "");
    argh_parse(&p, 3, argv);
    return argh_last_error(&p)->code;
}

TEST(test_int_rejects_bad_input)
{
    ASSERT_EQ(parse_int_value("abc"), ARGH_E_INVALID_VALUE);
    ASSERT_EQ(parse_int_value("10abc"), ARGH_E_INVALID_VALUE);
    ASSERT_EQ(parse_int_value(" 5"), ARGH_E_INVALID_VALUE);
    ASSERT_EQ(parse_int_value("5 "), ARGH_E_INVALID_VALUE);
    ASSERT_EQ(parse_int_value(""), ARGH_E_INVALID_VALUE);
    ASSERT_EQ(parse_int_value("-"), ARGH_E_INVALID_VALUE);
    ASSERT_EQ(parse_int_value("0x"), ARGH_E_INVALID_VALUE);
    ASSERT_EQ(parse_int_value("1.5"), ARGH_E_INVALID_VALUE);
    ASSERT_EQ(parse_int_value("99999999999999999999"), ARGH_E_OUT_OF_RANGE);
    ASSERT_EQ(parse_int_value("2147483648"), ARGH_E_OUT_OF_RANGE);
    ASSERT_EQ(parse_int_value("2147483647"), ARGH_E_NONE);
    ASSERT_EQ(parse_int_value("-2147483648"), ARGH_E_NONE);
}

TEST(test_int_error_message)
{
    ARGV("--jobs", "abc");
    int jobs = 4;
    argh_parser p;
    setup(&p);
    argh_int(&p, 'j', "jobs", &jobs, "");

    ASSERT_FALSE(argh_parse(&p, argc, argv));
    ASSERT_STR_EQ(error_text(&p), "invalid value 'abc' for '--jobs': expected an integer");
    ASSERT_EQ(jobs, 4);
}

TEST(test_long)
{
    ARGV("--big", "-2147483649");
    long big = 0;
    argh_parser p;
    setup(&p);
    argh_long(&p, 0, "big", &big, "");

#if LONG_MAX > 2147483647L
    ASSERT_TRUE(argh_parse(&p, argc, argv));
    ASSERT_TRUE(big == -2147483649L);
#else
    /* 32-bit long (Windows, many MCUs) */
    ASSERT_FALSE(argh_parse(&p, argc, argv));
    ASSERT_EQ(argh_last_error(&p)->code, ARGH_E_OUT_OF_RANGE);
    ASSERT_EQ(big, 0);
#endif
}

#ifndef ARGH_NO_FLOAT
TEST(test_double)
{
    ARGV("--a=1.5", "--b", "1e-3", "--c=-.25");
    double a = 0, b = 0, c = 0;
    argh_parser p;
    setup(&p);
    argh_double(&p, 0, "a", &a, "");
    argh_double(&p, 0, "b", &b, "");
    argh_double(&p, 0, "c", &c, "");

    ASSERT_TRUE(argh_parse(&p, argc, argv));
    ASSERT_TRUE(fabs(a - 1.5) < 1e-12);
    ASSERT_TRUE(fabs(b - 0.001) < 1e-12);
    ASSERT_TRUE(fabs(c + 0.25) < 1e-12);
}

static argh_err parse_double_value(const char *text)
{
    char *argv[] = {(char *)"prog", (char *)"--x", (char *)text, NULL};
    double x = 0;
    argh_parser p;
    setup(&p);
    argh_double(&p, 0, "x", &x, "");
    argh_parse(&p, 3, argv);
    return argh_last_error(&p)->code;
}

TEST(test_double_rejects_bad_input)
{
    ASSERT_EQ(parse_double_value("inf"), ARGH_E_INVALID_VALUE);
    ASSERT_EQ(parse_double_value("nan"), ARGH_E_INVALID_VALUE);
    ASSERT_EQ(parse_double_value("1.5x"), ARGH_E_INVALID_VALUE);
    ASSERT_EQ(parse_double_value(" 1"), ARGH_E_INVALID_VALUE);
    ASSERT_EQ(parse_double_value(""), ARGH_E_INVALID_VALUE);
    ASSERT_EQ(parse_double_value("."), ARGH_E_INVALID_VALUE);
    ASSERT_EQ(parse_double_value("1e999"), ARGH_E_OUT_OF_RANGE);
}
#endif

/* ============================================================================
 * Strings, enums, lists
 * ============================================================================ */

TEST(test_string_forms)
{
    ARGV("-ofile.txt", "--name=", "--dir", "-not-an-option");
    const char *out = NULL, *name = "x", *dir = NULL;
    argh_parser p;
    setup(&p);
    argh_string(&p, 'o', "output", &out, "");
    argh_string(&p, 0, "name", &name, "");
    argh_string(&p, 0, "dir", &dir, "");

    ASSERT_TRUE(argh_parse(&p, argc, argv));
    ASSERT_STR_EQ(out, "file.txt");
    ASSERT_STR_EQ(name, "");
    ASSERT_STR_EQ(dir, "-not-an-option"); /* a value option always takes the next argument */
}

TEST(test_value_cluster)
{
    ARGV("-vo", "file");
    bool verbose = false;
    const char *out = NULL;
    argh_parser p;
    setup(&p);
    argh_flag(&p, 'v', NULL, &verbose, "");
    argh_string(&p, 'o', NULL, &out, "");

    ASSERT_TRUE(argh_parse(&p, argc, argv));
    ASSERT_TRUE(verbose);
    ASSERT_STR_EQ(out, "file");
}

static const char *const modes[] = {"fast", "safe", "debug", NULL};

TEST(test_enum)
{
    ARGV("--mode", "safe");
    int mode = 0;
    argh_parser p;
    setup(&p);
    argh_enum(&p, 'm', "mode", &mode, modes, "");

    ASSERT_TRUE(argh_parse(&p, argc, argv));
    ASSERT_EQ(mode, 1);
}

TEST(test_enum_invalid)
{
    ARGV("--mode", "Safe");
    int mode = 0;
    argh_parser p;
    setup(&p);
    argh_enum(&p, 'm', "mode", &mode, modes, "");

    ASSERT_FALSE(argh_parse(&p, argc, argv));
    ASSERT_STR_EQ(error_text(&p), "invalid value 'Safe' for '--mode': expected one of: fast, safe, debug");
}

TEST(test_list)
{
    ARGV("-I", "a", "-Ib", "--include=c");
    const char *buf[4];
    argh_values inc = ARGH_VALUES(buf);
    argh_parser p;
    setup(&p);
    argh_list(&p, 'I', "include", &inc, "");

    ASSERT_TRUE(argh_parse(&p, argc, argv));
    ASSERT_EQ(inc.count, 3);
    ASSERT_STR_EQ(inc.items[0], "a");
    ASSERT_STR_EQ(inc.items[1], "b");
    ASSERT_STR_EQ(inc.items[2], "c");
}

TEST(test_list_full)
{
    ARGV("-I", "a", "-I", "b", "-I", "c");
    const char *buf[2];
    argh_values inc = ARGH_VALUES(buf);
    argh_parser p;
    setup(&p);
    argh_list(&p, 'I', "include", &inc, "");

    ASSERT_FALSE(argh_parse(&p, argc, argv));
    ASSERT_STR_EQ(error_text(&p), "too many values for '-I' (at most 2)");
    ASSERT_EQ(inc.count, 2);
}

/* ============================================================================
 * Positionals
 * ============================================================================ */

TEST(test_positionals_mixed_with_options)
{
    ARGV("in.txt", "-v", "out.txt", "extra1", "-j", "2", "extra2");
    bool verbose = false;
    int jobs = 0;
    const char *in = NULL, *out = NULL;
    argh_values rest = {0};
    argh_parser p;
    setup(&p);
    argh_flag(&p, 'v', NULL, &verbose, "");
    argh_int(&p, 'j', NULL, &jobs, "");
    argh_pos(&p, "input", &in, "");
    argh_pos(&p, "output", &out, "");
    argh_rest(&p, "extra", &rest, "");

    ASSERT_TRUE(argh_parse(&p, argc, argv));
    ASSERT_STR_EQ(in, "in.txt");
    ASSERT_STR_EQ(out, "out.txt");
    ASSERT_EQ(rest.count, 2);
    ASSERT_STR_EQ(rest.items[0], "extra1");
    ASSERT_STR_EQ(rest.items[1], "extra2");
    /* argv is a permutation with positionals first */
    ASSERT_STR_EQ(argv[1], "in.txt");
    ASSERT_STR_EQ(argv[4], "extra2");
    ASSERT_TRUE(rest.items == (const char **)(void *)(argv + 3));
}

TEST(test_double_dash)
{
    ARGV("-v", "--", "-x", "--", "file");
    bool verbose = false;
    argh_values rest = {0};
    argh_parser p;
    setup(&p);
    argh_flag(&p, 'v', NULL, &verbose, "");
    argh_rest(&p, "args", &rest, "");

    ASSERT_TRUE(argh_parse(&p, argc, argv));
    ASSERT_TRUE(verbose);
    ASSERT_EQ(rest.count, 3);
    ASSERT_STR_EQ(rest.items[0], "-x");
    ASSERT_STR_EQ(rest.items[1], "--");
    ASSERT_STR_EQ(rest.items[2], "file");
}

TEST(test_double_dash_is_not_a_value)
{
    ARGV("--output", "--", "file");
    const char *out = NULL;
    argh_values rest = {0};
    argh_parser p;
    setup(&p);
    argh_string(&p, 'o', "output", &out, "");
    argh_rest(&p, "files", &rest, "");

    ASSERT_FALSE(argh_parse(&p, argc, argv));
    ASSERT_STR_EQ(error_text(&p), "option '--output' requires a value");
}

TEST(test_single_dash_is_positional)
{
    ARGV("-");
    const char *in = NULL;
    argh_parser p;
    setup(&p);
    argh_pos(&p, "input", &in, "");

    ASSERT_TRUE(argh_parse(&p, argc, argv));
    ASSERT_STR_EQ(in, "-");
}

TEST(test_optional_positional)
{
    ARGV0();
    const char *in = "default";
    argh_parser p;
    setup(&p);
    argh_optional(argh_pos(&p, "input", &in, ""));

    ASSERT_TRUE(argh_parse(&p, argc, argv));
    ASSERT_STR_EQ(in, "default");
}

TEST(test_missing_positional)
{
    ARGV0();
    const char *in = NULL;
    argh_parser p;
    setup(&p);
    argh_pos(&p, "input", &in, "");

    ASSERT_FALSE(argh_parse(&p, argc, argv));
    ASSERT_STR_EQ(error_text(&p), "missing required argument '<input>'");
}

TEST(test_unexpected_argument)
{
    ARGV("a", "b");
    const char *in = NULL;
    argh_parser p;
    setup(&p);
    argh_pos(&p, "input", &in, "");

    ASSERT_FALSE(argh_parse(&p, argc, argv));
    ASSERT_STR_EQ(error_text(&p), "unexpected argument 'b'");
}

TEST(test_required_rest)
{
    ARGV0();
    argh_values files = {0};
    argh_parser p;
    setup(&p);
    argh_required(argh_rest(&p, "files", &files, ""));

    ASSERT_FALSE(argh_parse(&p, argc, argv));
    ASSERT_EQ(argh_last_error(&p)->code, ARGH_E_MISSING_REQUIRED);
}

TEST(test_posix_mode)
{
    ARGV("-v", "run", "-x", "--flag");
    bool verbose = false;
    const char *cmd = NULL;
    argh_values args = {0};
    argh_parser p;
    setup(&p);
    argh_set_flags(&p, ARGH_POSIX);
    argh_flag(&p, 'v', NULL, &verbose, "");
    argh_pos(&p, "command", &cmd, "");
    argh_rest(&p, "args", &args, "");

    ASSERT_TRUE(argh_parse(&p, argc, argv));
    ASSERT_TRUE(verbose);
    ASSERT_STR_EQ(cmd, "run");
    ASSERT_EQ(args.count, 2);
    ASSERT_STR_EQ(args.items[0], "-x");
}

/* ============================================================================
 * Errors
 * ============================================================================ */

TEST(test_unknown_long)
{
    ARGV("--verbos=1");
    bool verbose = false;
    argh_parser p;
    setup(&p);
    argh_flag(&p, 'v', "verbose", &verbose, "");

    ASSERT_FALSE(argh_parse(&p, argc, argv));
    ASSERT_EQ(argh_last_error(&p)->argv_index, 1);
    ASSERT_STR_EQ(error_text(&p), "unknown option '--verbos'" DID_YOU_MEAN("--verbose"));
    ASSERT_STR_EQ(err_text, "prog: unknown option '--verbos'" DID_YOU_MEAN("--verbose") "\n"
                            "Try 'prog --help' for more information.\n");
    ASSERT_EQ(argh_exit_code(&p), 2);
}

TEST(test_unknown_short_in_cluster)
{
    ARGV("-vxz");
    bool verbose = false;
    argh_parser p;
    setup(&p);
    argh_flag(&p, 'v', NULL, &verbose, "");

    ASSERT_FALSE(argh_parse(&p, argc, argv));
    ASSERT_STR_EQ(error_text(&p), "unknown option '-x'");
}

TEST(test_no_prefix_matching)
{
    ARGV("--verb");
    bool verbose = false;
    argh_parser p;
    setup(&p);
    argh_flag(&p, 'v', "verbose", &verbose, "");

    ASSERT_FALSE(argh_parse(&p, argc, argv));
    ASSERT_EQ(argh_last_error(&p)->code, ARGH_E_UNKNOWN_OPTION);
}

TEST(test_negative_number_is_not_positional)
{
    ARGV("-5");
    const char *in = NULL;
    argh_parser p;
    setup(&p);
    argh_optional(argh_pos(&p, "input", &in, ""));

    ASSERT_FALSE(argh_parse(&p, argc, argv));
    ASSERT_EQ(argh_last_error(&p)->code, ARGH_E_UNKNOWN_OPTION);
}

TEST(test_short_equals)
{
    ARGV("-o=file");
    const char *out = NULL;
    argh_parser p;
    setup(&p);
    argh_string(&p, 'o', "output", &out, "");

    ASSERT_FALSE(argh_parse(&p, argc, argv));
    ASSERT_STR_EQ(error_text(&p),
                  "short option '-o' does not accept '=': write '-o VALUE' or '--output=VALUE'");
}

TEST(test_short_flag_equals)
{
    ARGV("-v=1");
    bool verbose = false;
    argh_parser p;
    setup(&p);
    argh_flag(&p, 'v', "verbose", &verbose, "");

    ASSERT_FALSE(argh_parse(&p, argc, argv));
    ASSERT_EQ(argh_last_error(&p)->code, ARGH_E_SHORT_EQUALS);
}

TEST(test_missing_value)
{
    ARGV("-j");
    int jobs = 0;
    argh_parser p;
    setup(&p);
    argh_int(&p, 'j', "jobs", &jobs, "");

    ASSERT_FALSE(argh_parse(&p, argc, argv));
    ASSERT_STR_EQ(error_text(&p), "option '-j' requires a value");
}

TEST(test_required_option)
{
    ARGV0();
    const char *out = NULL;
    argh_parser p;
    setup(&p);
    argh_required(argh_string(&p, 'o', "output", &out, ""));

    ASSERT_FALSE(argh_parse(&p, argc, argv));
    ASSERT_STR_EQ(error_text(&p), "missing required option '--output'");
}

TEST(test_once)
{
    ARGV("-o", "a", "--output", "b");
    const char *out = NULL;
    argh_parser p;
    setup(&p);
    argh_once(argh_string(&p, 'o', "output", &out, ""));

    ASSERT_FALSE(argh_parse(&p, argc, argv));
    ASSERT_STR_EQ(error_text(&p), "option '--output' can only be given once");
}

TEST(test_last_value_wins)
{
    ARGV("-o", "a", "--output", "b");
    const char *out = NULL;
    argh_parser p;
    setup(&p);
    argh_string(&p, 'o', "output", &out, "");

    ASSERT_TRUE(argh_parse(&p, argc, argv));
    ASSERT_STR_EQ(out, "b");
}

TEST(test_format_error_truncates)
{
    ARGV("--this-option-does-not-exist");
    char small[10];
    size_t full;
    argh_parser p;
    setup(&p);

    ASSERT_FALSE(argh_parse(&p, argc, argv));
    full = argh_format_error(&p, small, sizeof(small));
    ASSERT_EQ(full, strlen("unknown option '--this-option-does-not-exist'"));
    ASSERT_STR_EQ(small, "unknown o");
}

TEST(test_no_error_after_success)
{
    ARGV("-v");
    bool verbose = false;
    argh_parser p;
    setup(&p);
    argh_flag(&p, 'v', NULL, &verbose, "");

    ASSERT_TRUE(argh_parse(&p, argc, argv));
    ASSERT_EQ(argh_last_error(&p)->code, ARGH_E_NONE);
    ASSERT_EQ(argh_exit_code(&p), 0);
    ASSERT_EQ(err_len, 0u);
}

/* ============================================================================
 * Configuration errors
 * ============================================================================ */

TEST(test_config_reserved_help)
{
    ARGV0();
    const char *host = NULL;
    argh_parser p;
    setup(&p);
    argh_string(&p, 'h', "host", &host, "");

    ASSERT_FALSE(argh_parse(&p, argc, argv));
    ASSERT_EQ(argh_last_error(&p)->code, ARGH_E_CONFIG);
}

TEST(test_config_no_auto_help_frees_h)
{
    ARGV("-h", "example.com");
    const char *host = NULL;
    argh_parser p;
    setup(&p);
    argh_set_flags(&p, ARGH_NO_AUTO_HELP);
    argh_string(&p, 'h', "host", &host, "");

    ASSERT_TRUE(argh_parse(&p, argc, argv));
    ASSERT_STR_EQ(host, "example.com");
}

/* Definition checks run only without NDEBUG */
#ifndef NDEBUG
TEST(test_config_duplicate)
{
    ARGV0();
    bool a = false, b = false;
    argh_parser p;
    setup(&p);
    argh_flag(&p, 'v', "verbose", &a, "");
    argh_flag(&p, 'x', "verbose", &b, "");

    ASSERT_FALSE(argh_parse(&p, argc, argv));
    ASSERT_STR_EQ(error_text(&p), "configuration error: option defined twice (--verbose)");
}
#endif

TEST(test_config_builder_overflow)
{
    ARGV0();
    bool flags[ARGH_BUILDER_CAP + 1];
    argh_opt *last = NULL;
    int i;
    argh_parser p;
    setup(&p);
    for (i = 0; i <= ARGH_BUILDER_CAP; i++)
        last = argh_flag(&p, 0, NULL, &flags[i], "");

    ASSERT_TRUE(last == NULL);
    ASSERT_TRUE(argh_required(last) == NULL); /* modifiers accept NULL */
    ASSERT_FALSE(argh_parse(&p, argc, argv));
    ASSERT_EQ(argh_last_error(&p)->code, ARGH_E_CONFIG);
}

TEST(test_config_missing_target)
{
    ARGV0();
    argh_parser p;
    setup(&p);
    argh_int(&p, 'n', "num", NULL, "");

    ASSERT_FALSE(argh_parse(&p, argc, argv));
    ASSERT_STR_EQ(error_text(&p), "configuration error: option has no target variable (--num)");
}

/* ============================================================================
 * Help and version
 * ============================================================================ */

static bool h_verbose;
static int h_jobs = 4;
static const char *h_output = "out.txt";
static int h_mode = 1;
static bool h_color = true;
static bool h_secret;
static const char *h_input;
static argh_values h_files;

static const argh_opt help_opts[] = {
    ARGH_FLAG('v', "verbose", &h_verbose, "Verbose output"),
    ARGH_INT('j', "jobs", &h_jobs, "Parallel jobs"),
    ARGH_STRING('o', "output", &h_output, "Output file"),
    ARGH_ENUM('m', "mode", &h_mode, modes, "Execution mode"),
    ARGH_GROUP("Display"),
    ARGH_FLAG(0, "color", &h_color, "Colored output", ARGH_NEGATABLE),
    ARGH_FLAG(0, "secret", &h_secret, "Not shown", ARGH_HIDDEN),
    ARGH_POS("input", &h_input, "Input file"),
    ARGH_REST("files", &h_files, "More files"),
    ARGH_END,
};

static const char *const expected_help =
    "Usage: prog [OPTIONS] <input> [files...]\n"
    "\n"
    "Tests help output\n"
    "\n"
    "Arguments:\n"
    "  <input>                       Input file\n"
    "  [files...]                    More files\n"
    "\n"
    "Options:\n"
    "  -v, --verbose                 Verbose output\n"
    "  -j, --jobs <n>                Parallel jobs (default: 4)\n"
    "  -o, --output <value>          Output file (default: out.txt)\n"
    "  -m, --mode <fast|safe|debug>  Execution mode (default: safe)\n"
    "\n"
    "Display:\n"
    "      --[no-]color              Colored output\n"
    "\n"
    "  -h, --help                    Print help\n"
    "  -V, --version                 Print version\n";

TEST(test_help_wraps_long_entries)
{
    ARGV("--help");
    int n = 0;
    bool b = false;
    argh_parser p;
    setup(&p);
    argh_flag(&p, 'b', "brief", &b, "Short one");
    argh_metavar(argh_int(&p, 'n', "number-of-parallel-jobs", &n, "Long one"), "<count>");

    ASSERT_FALSE(argh_parse(&p, argc, argv));
    ASSERT_TRUE(strstr(out_text,
                       "  -b, --brief    Short one\n"
                       "  -n, --number-of-parallel-jobs <count>\n"
                       "                 Long one (default: 0)\n") != NULL);
}

/* Defaults are formatted without printf, so ARGH_NO_STDIO shows the same */
TEST(test_help_number_defaults)
{
    ARGV("--help");
    int i = -42;
    long l = LONG_MIN;
    argh_parser p;
    setup(&p);
    argh_int(&p, 0, "int", &i, "I");
    argh_long(&p, 0, "long", &l, "L");
#ifndef ARGH_NO_FLOAT
    double d1 = 0.5, d2 = 2.0, d3 = -1.25;
    argh_double(&p, 0, "half", &d1, "D1");
    argh_double(&p, 0, "two", &d2, "D2");
    argh_double(&p, 0, "neg", &d3, "D3");
#endif

    ASSERT_FALSE(argh_parse(&p, argc, argv));
    ASSERT_TRUE(strstr(out_text, "I (default: -42)\n") != NULL);
    ASSERT_TRUE(strstr(out_text, LONG_MIN == -2147483647L - 1 ? "L (default: -2147483648)\n"
                                                                 : "L (default: -9223372036854775808)\n") != NULL);
#ifndef ARGH_NO_FLOAT
    ASSERT_TRUE(strstr(out_text, "D1 (default: 0.5)\n") != NULL);
    ASSERT_TRUE(strstr(out_text, "D2 (default: 2)\n") != NULL);
    ASSERT_TRUE(strstr(out_text, "D3 (default: -1.25)\n") != NULL);
#endif
}

TEST(test_help_output)
{
    ARGV("-j", "8", "--help");
    argh_parser p;
    argh_init(&p, "prog", "Tests help output");
    argh_set_writer(&p, capture, NULL);
    argh_version(&p, "1.2.3");
    argh_table(&p, help_opts);

    ASSERT_FALSE(argh_parse(&p, argc, argv));
    ASSERT_EQ(argh_exit_code(&p), 0);
    ASSERT_EQ(h_jobs, 4); /* help shows defaults and changes nothing */
    if (strcmp(out_text, expected_help) != 0)
        printf("--- got ---\n%s--- expected ---\n%s", out_text, expected_help);
    ASSERT_STR_EQ(out_text, expected_help);
}

TEST(test_help_in_cluster)
{
    ARGV("-vh");
    bool verbose = false;
    argh_parser p;
    setup(&p);
    argh_flag(&p, 'v', NULL, &verbose, "");

    ASSERT_FALSE(argh_parse(&p, argc, argv));
    ASSERT_EQ(argh_exit_code(&p), 0);
    ASSERT_FALSE(verbose);
    ASSERT_TRUE(strncmp(out_text, "Usage: prog", 11) == 0);
}

TEST(test_help_wins_over_errors)
{
    ARGV("--bogus", "--help");
    argh_parser p;
    setup(&p);

    ASSERT_FALSE(argh_parse(&p, argc, argv));
    ASSERT_EQ(argh_exit_code(&p), 0);
    ASSERT_EQ(err_len, 0u);
}

TEST(test_help_as_value_is_a_value)
{
    ARGV("--output", "--help");
    const char *out = NULL;
    argh_parser p;
    setup(&p);
    argh_string(&p, 'o', "output", &out, "");

    ASSERT_TRUE(argh_parse(&p, argc, argv));
    ASSERT_STR_EQ(out, "--help");
}

TEST(test_help_after_double_dash_is_positional)
{
    ARGV("--", "--help");
    argh_values rest = {0};
    argh_parser p;
    setup(&p);
    argh_rest(&p, "args", &rest, "");

    ASSERT_TRUE(argh_parse(&p, argc, argv));
    ASSERT_EQ(rest.count, 1);
}

TEST(test_value_containing_h_is_not_help)
{
    ARGV("-ohome", "--path=-h");
    const char *out = NULL, *path = NULL;
    argh_parser p;
    setup(&p);
    argh_string(&p, 'o', NULL, &out, "");
    argh_string(&p, 0, "path", &path, "");

    ASSERT_TRUE(argh_parse(&p, argc, argv));
    ASSERT_STR_EQ(out, "home");
    ASSERT_STR_EQ(path, "-h");
    ASSERT_EQ(argh_last_error(&p)->code, ARGH_E_NONE);
}

TEST(test_version)
{
    ARGV("--version");
    argh_parser p;
    setup(&p);
    argh_version(&p, "1.2.3");

    ASSERT_FALSE(argh_parse(&p, argc, argv));
    ASSERT_EQ(argh_exit_code(&p), 0);
    ASSERT_STR_EQ(out_text, "prog 1.2.3\n");
}

TEST(test_no_version_without_string)
{
    ARGV("-V");
    argh_parser p;
    setup(&p);

    ASSERT_FALSE(argh_parse(&p, argc, argv));
    ASSERT_EQ(argh_last_error(&p)->code, ARGH_E_UNKNOWN_OPTION);
}

TEST(test_name_from_argv0)
{
    char *argv[] = {(char *)"/usr/local/bin/mytool", (char *)"--bogus", NULL};
    argh_parser p;
    argh_init(&p, NULL, NULL);
    argh_set_writer(&p, capture, NULL);

    ASSERT_FALSE(argh_parse(&p, 2, argv));
    ASSERT_TRUE(strncmp(err_text, "mytool: ", 8) == 0);
}

/* ============================================================================
 * Tables, builder, misc
 * ============================================================================ */

static bool t_verbose;
static int t_jobs = 1;
static const char *t_out;

TEST(test_table_with_flags_argument)
{
    static const argh_opt opts[] = {
        ARGH_FLAG('v', "verbose", &t_verbose, "Verbose"),
        ARGH_INT('j', "jobs", &t_jobs, "Jobs"),
        ARGH_STRING('o', "output", &t_out, "Output", ARGH_REQUIRED | ARGH_ONCE),
        ARGH_END,
    };
    ARGV("-v", "-j3", "-o", "x");
    argh_parser p;
    setup(&p);
    argh_table(&p, opts);

    ASSERT_EQ(opts[0].flags, 0);
    ASSERT_EQ(opts[2].flags, ARGH_REQUIRED | ARGH_ONCE);
    ASSERT_TRUE(argh_parse(&p, argc, argv));
    ASSERT_TRUE(t_verbose);
    ASSERT_EQ(t_jobs, 3);
    ASSERT_STR_EQ(t_out, "x");
}

TEST(test_table_metavar)
{
    static const char *key;
    static int n;
    static const argh_opt opts[] = {
        ARGH_STRING(0, "tls-key", &key, "Client key", 0, "<file>"),
        ARGH_INT('n', "count", &n, "Count", ARGH_REQUIRED, "<count>"),
        ARGH_INT(0, "plain", &n, "Plain"),
        ARGH_END,
    };
    ARGV("--help");
    argh_parser p;
    setup(&p);
    argh_table(&p, opts);

    ASSERT_STR_EQ(opts[0].metavar, "<file>");
    ASSERT_EQ(opts[1].flags, ARGH_REQUIRED);
    ASSERT_TRUE(opts[2].metavar == NULL);
    ASSERT_FALSE(argh_parse(&p, argc, argv));
    ASSERT_TRUE(strstr(out_text, "--tls-key <file>") != NULL);
    ASSERT_TRUE(strstr(out_text, "--count <count>") != NULL);
}

TEST(test_table_and_builder_mixed)
{
    static int table_value;
    static const argh_opt opts[] = {ARGH_INT('a', "alpha", &table_value, ""), ARGH_END};
    ARGV("-a", "1", "-b", "2");
    int builder_value = 0;
    argh_parser p;
    setup(&p);
    argh_table(&p, opts);
    argh_int(&p, 'b', "beta", &builder_value, "");

    ASSERT_TRUE(argh_parse(&p, argc, argv));
    ASSERT_EQ(table_value, 1);
    ASSERT_EQ(builder_value, 2);
}

TEST(test_given)
{
    ARGV("-v");
    bool verbose = false, quiet = false;
    int unused = 0;
    argh_parser p;
    setup(&p);
    argh_flag(&p, 'v', NULL, &verbose, "");
    argh_flag(&p, 'q', NULL, &quiet, "");

    ASSERT_TRUE(argh_parse(&p, argc, argv));
    ASSERT_TRUE(argh_given(&p, &verbose));
    ASSERT_FALSE(argh_given(&p, &quiet));
    ASSERT_FALSE(argh_given(&p, &unused));
}

TEST(test_empty_argv)
{
    char *argv[] = {NULL};
    bool verbose = false;
    argh_parser p;
    argh_init(&p, NULL, NULL);
    argh_set_writer(&p, capture, NULL);
    argh_flag(&p, 'v', NULL, &verbose, "");

    ASSERT_TRUE(argh_parse(&p, 0, argv));
    ASSERT_FALSE(verbose);
}

TEST(test_parse_twice_resets_state)
{
    ARGV("--bogus");
    char *good[] = {(char *)"prog", (char *)"-v", NULL};
    bool verbose = false;
    argh_parser p;
    setup(&p);
    argh_flag(&p, 'v', NULL, &verbose, "");

    ASSERT_FALSE(argh_parse(&p, argc, argv));
    ASSERT_TRUE(argh_parse(&p, 2, good));
    ASSERT_EQ(argh_last_error(&p)->code, ARGH_E_NONE);
    ASSERT_TRUE(argh_given(&p, &verbose));
}

/* ============================================================================
 * Custom types
 * ============================================================================ */

/* A size with an optional K/M/G suffix: 512, 64K, 10M, 1G */
static const char *parse_size(const char *text, void *target)
{
    unsigned long long value = 0;
    const char *s = text;
    if (*s < '0' || *s > '9')
        return "expected a size like 512K, 10M or 1G";
    for (; *s >= '0' && *s <= '9'; s++)
        value = value * 10 + (unsigned long long)(*s - '0');
    switch (*s)
    {
    case 'K': value <<= 10; s++; break;
    case 'M': value <<= 20; s++; break;
    case 'G': value <<= 30; s++; break;
    default: break;
    }
    if (*s)
        return "expected a size like 512K, 10M or 1G";
    *(unsigned long long *)target = value;
    return NULL;
}

static bool format_size(const void *target, char *buf, size_t size)
{
    unsigned long long v = *(const unsigned long long *)target;
    if (v && v % (1ull << 20) == 0)
        snprintf(buf, size, "%lluM", v >> 20);
    else if (v && v % (1ull << 10) == 0)
        snprintf(buf, size, "%lluK", v >> 10);
    else
        snprintf(buf, size, "%llu", v);
    return true;
}

static const argh_type size_type = {"<size>", parse_size, format_size};

/* host:port, into a struct */
typedef struct
{
    char host[64];
    int port;
} endpoint;

static const char *parse_endpoint(const char *text, void *target)
{
    endpoint *ep = (endpoint *)target;
    const char *colon = strrchr(text, ':');
    size_t host_len;
    long port;
    char *end;
    if (!colon || colon == text)
        return "expected host:port";
    host_len = (size_t)(colon - text);
    if (host_len >= sizeof(ep->host))
        return "host name too long";
    port = strtol(colon + 1, &end, 10);
    if (*end || end == colon + 1 || port < 1 || port > 65535)
        return "port must be 1-65535";
    memcpy(ep->host, text, host_len);
    ep->host[host_len] = '\0';
    ep->port = (int)port;
    return NULL;
}

/* No format function: help shows no default */
static const argh_type endpoint_type = {"<host:port>", parse_endpoint, NULL};

TEST(test_custom_builder)
{
    ARGV("--max-size", "10M", "-c", "db.local:5432");
    unsigned long long max_size = 0;
    endpoint connect = {"localhost", 80};
    argh_parser p;
    setup(&p);
    argh_custom(&p, 's', "max-size", &max_size, &size_type, "Largest file to keep");
    argh_custom(&p, 'c', "connect", &connect, &endpoint_type, "Server to connect to");

    ASSERT_TRUE(argh_parse(&p, argc, argv));
    ASSERT_TRUE(max_size == 10ull << 20);
    ASSERT_STR_EQ(connect.host, "db.local");
    ASSERT_EQ(connect.port, 5432);
    ASSERT_TRUE(argh_given(&p, &connect));
}

static unsigned long long t_cache_size = 64ull << 10;

TEST(test_custom_table)
{
    static const argh_opt opts[] = {
        ARGH_CUSTOM(0, "cache", &t_cache_size, &size_type, "Cache size", ARGH_ONCE),
        ARGH_END,
    };
    ARGV("--cache=1G");
    argh_parser p;
    setup(&p);
    argh_table(&p, opts);

    ASSERT_EQ(opts[0].flags, ARGH_ONCE);
    ASSERT_TRUE(argh_parse(&p, argc, argv));
    ASSERT_TRUE(t_cache_size == 1ull << 30);
}

TEST(test_custom_error_uses_reason)
{
    ARGV("--max-size", "10Q");
    unsigned long long max_size = 5;
    argh_parser p;
    setup(&p);
    argh_custom(&p, 's', "max-size", &max_size, &size_type, "");

    ASSERT_FALSE(argh_parse(&p, argc, argv));
    ASSERT_EQ(argh_last_error(&p)->code, ARGH_E_INVALID_VALUE);
    ASSERT_STR_EQ(error_text(&p), "invalid value '10Q' for '--max-size': expected a size like 512K, 10M or 1G");
    ASSERT_TRUE(max_size == 5); /* a failed parse must not touch the variable */
}

TEST(test_custom_struct_error)
{
    ARGV("-c", "db.local:99999");
    endpoint connect = {"localhost", 80};
    argh_parser p;
    setup(&p);
    argh_custom(&p, 'c', "connect", &connect, &endpoint_type, "");

    ASSERT_FALSE(argh_parse(&p, argc, argv));
    ASSERT_STR_EQ(error_text(&p), "invalid value 'db.local:99999' for '-c': port must be 1-65535");
}

TEST(test_custom_help)
{
    ARGV("--help");
    unsigned long long max_size = 64ull << 20;
    endpoint connect = {"localhost", 80};
    argh_parser p;
    setup(&p);
    argh_custom(&p, 's', "max-size", &max_size, &size_type, "Largest file to keep");
    argh_custom(&p, 'c', "connect", &connect, &endpoint_type, "Server to connect to");

    ASSERT_FALSE(argh_parse(&p, argc, argv));
    ASSERT_TRUE(strstr(out_text, "  -s, --max-size <size>      Largest file to keep (default: 64M)\n") != NULL);
    ASSERT_TRUE(strstr(out_text, "  -c, --connect <host:port>  Server to connect to\n") != NULL);
}

TEST(test_custom_required)
{
    ARGV0();
    endpoint connect = {"", 0};
    argh_parser p;
    setup(&p);
    argh_required(argh_custom(&p, 'c', "connect", &connect, &endpoint_type, ""));

    ASSERT_FALSE(argh_parse(&p, argc, argv));
    ASSERT_STR_EQ(error_text(&p), "missing required option '--connect'");
}

TEST(test_custom_config_without_type)
{
    ARGV0();
    unsigned long long max_size = 0;
    argh_parser p;
    setup(&p);
    argh_custom(&p, 's', "max-size", &max_size, NULL, "");

    ASSERT_FALSE(argh_parse(&p, argc, argv));
    ASSERT_STR_EQ(error_text(&p),
                  "configuration error: custom option has no argh_type with a parse function (--max-size)");
}

/* ============================================================================
 * Rules and validators
 * ============================================================================ */

static bool r_json, r_yaml, r_csv, r_stdin, r_all;
static const char *r_input, *r_key, *r_cert, *r_package;

static const argh_rule r_rules[] = {
    ARGH_AT_MOST_ONE(&r_json, &r_yaml, &r_csv),
    ARGH_EXACTLY_ONE(&r_input, &r_stdin),
    ARGH_REQUIRES(&r_key, &r_cert),
    ARGH_RULES_END,
};

/* An export tool: one output format, one input source, TLS key needs a cert */
static argh_err parse_with_rules(int argc, char **argv, argh_parser *p)
{
    r_json = r_yaml = r_csv = r_stdin = false;
    r_input = r_key = r_cert = NULL;
    setup(p);
    argh_flag(p, 0, "json", &r_json, "");
    argh_flag(p, 0, "yaml", &r_yaml, "");
    argh_flag(p, 0, "csv", &r_csv, "");
    argh_string(p, 'i', "input", &r_input, "");
    argh_flag(p, 0, "stdin", &r_stdin, "");
    argh_string(p, 0, "tls-key", &r_key, "");
    argh_string(p, 0, "tls-cert", &r_cert, "");
    argh_rules(p, r_rules);
    argh_parse(p, argc, argv);
    return argh_last_error(p)->code;
}

TEST(test_rules_satisfied)
{
    ARGV("--json", "--input", "data.db", "--tls-key", "k.pem", "--tls-cert", "c.pem");
    argh_parser p;
    ASSERT_EQ(parse_with_rules(argc, argv, &p), ARGH_E_NONE);
}

TEST(test_rule_at_most_one)
{
    ARGV("--json", "--stdin", "--csv");
    argh_parser p;
    ASSERT_EQ(parse_with_rules(argc, argv, &p), ARGH_E_CONFLICT);
    ASSERT_STR_EQ(error_text(&p), "options '--json' and '--csv' cannot be used together");
    ASSERT_EQ(argh_exit_code(&p), 2);
}

TEST(test_rule_exactly_one_missing)
{
    ARGV("--yaml");
    argh_parser p;
    ASSERT_EQ(parse_with_rules(argc, argv, &p), ARGH_E_ONE_REQUIRED);
    ASSERT_STR_EQ(error_text(&p), "one of '--input' or '--stdin' is required");
}

TEST(test_rule_exactly_one_both)
{
    ARGV("--stdin", "-i", "x");
    argh_parser p;
    ASSERT_EQ(parse_with_rules(argc, argv, &p), ARGH_E_CONFLICT);
    ASSERT_STR_EQ(error_text(&p), "options '--input' and '--stdin' cannot be used together");
}

TEST(test_rule_requires)
{
    ARGV("--stdin", "--tls-key", "k.pem");
    argh_parser p;
    ASSERT_EQ(parse_with_rules(argc, argv, &p), ARGH_E_REQUIRES);
    ASSERT_STR_EQ(error_text(&p), "option '--tls-key' requires '--tls-cert'");
}

TEST(test_rule_requires_only_when_given)
{
    ARGV("--stdin", "--tls-cert", "c.pem");
    argh_parser p;
    ASSERT_EQ(parse_with_rules(argc, argv, &p), ARGH_E_NONE);
}

TEST(test_rule_at_least_one)
{
    static const argh_rule rules[] = {ARGH_AT_LEAST_ONE(&r_all, &r_package), ARGH_RULES_END};
    char *none[] = {(char *)"prog", NULL};
    char *both[] = {(char *)"prog", (char *)"--all", (char *)"-p", (char *)"core", NULL};
    argh_parser p;

    r_all = false;
    r_package = NULL;
    setup(&p);
    argh_flag(&p, 0, "all", &r_all, "");
    argh_string(&p, 'p', "package", &r_package, "");
    argh_rules(&p, rules);
    ASSERT_FALSE(argh_parse(&p, 1, none));
    ASSERT_STR_EQ(error_text(&p), "one of '--all' or '--package' is required");
    ASSERT_TRUE(argh_parse(&p, 4, both));
}

TEST(test_rule_three_names)
{
    static const argh_rule rules[] = {ARGH_EXACTLY_ONE(&r_json, &r_yaml, &r_csv), ARGH_RULES_END};
    ARGV0();
    argh_parser p;
    setup(&p);
    argh_flag(&p, 0, "json", &r_json, "");
    argh_flag(&p, 0, "yaml", &r_yaml, "");
    argh_flag(&p, 0, "csv", &r_csv, "");
    argh_rules(&p, rules);

    ASSERT_FALSE(argh_parse(&p, argc, argv));
    ASSERT_STR_EQ(error_text(&p), "one of '--json', '--yaml' or '--csv' is required");
}

TEST(test_rule_config_one_variable)
{
    static const argh_rule rules[] = {ARGH_AT_MOST_ONE(&r_json), ARGH_RULES_END};
    ARGV0();
    argh_parser p;
    setup(&p);
    argh_flag(&p, 0, "json", &r_json, "");
    argh_rules(&p, rules);

    ASSERT_FALSE(argh_parse(&p, argc, argv));
    ASSERT_STR_EQ(error_text(&p), "configuration error: a rule needs at least two variables");
}

/* Definition checks run only without NDEBUG */
#ifndef NDEBUG
TEST(test_rule_config_unbound_variable)
{
    static int not_an_option;
    static const argh_rule rules[] = {ARGH_AT_MOST_ONE(&r_json, &not_an_option), ARGH_RULES_END};
    ARGV0();
    argh_parser p;
    setup(&p);
    argh_flag(&p, 0, "json", &r_json, "");
    argh_rules(&p, rules);

    ASSERT_FALSE(argh_parse(&p, argc, argv));
    ASSERT_STR_EQ(error_text(&p), "configuration error: a rule refers to a variable that no option is bound to");
}
#endif

#ifndef ARGH_NO_COMMANDS
/* A rule on a command's options only applies when that command is selected */
TEST(test_rule_skipped_for_other_command)
{
    static bool fast, safe;
    static const argh_opt run_opts[] = {
        ARGH_FLAG(0, "fast", &fast, ""),
        ARGH_FLAG(0, "safe", &safe, ""),
        ARGH_END,
    };
    static const argh_cmd cmds[] = {
        ARGH_CMD("run", "", run_opts),
        ARGH_CMD("list", "", NULL),
        ARGH_CMD_END,
    };
    static const argh_rule rules[] = {ARGH_EXACTLY_ONE(&fast, &safe), ARGH_RULES_END};
    char *list[] = {(char *)"prog", (char *)"list", NULL};
    char *run[] = {(char *)"prog", (char *)"run", NULL};
    argh_parser p;

    setup(&p);
    argh_commands(&p, cmds);
    argh_rules(&p, rules);
    ASSERT_TRUE(argh_parse(&p, 2, list));
    ASSERT_FALSE(argh_parse(&p, 2, run));
    ASSERT_STR_EQ(error_text(&p), "one of '--fast' or '--safe' is required");
}
#endif

/* ----------------------------------------------------------------------------
 * Validators
 * ---------------------------------------------------------------------------- */

typedef struct
{
    int min, max;
    int calls;
} range;

static bool check_range(argh_parser *p, void *ctx)
{
    range *r = (range *)ctx;
    r->calls++;
    if (r->min > r->max)
        return argh_fail(p, "--min must not be greater than --max");
    return true;
}

TEST(test_validator)
{
    ARGV("--min", "5", "--max", "3");
    range r = {0, 10, 0};
    argh_parser p;
    setup(&p);
    argh_int(&p, 0, "min", &r.min, "");
    argh_int(&p, 0, "max", &r.max, "");
    argh_set_validator(&p, check_range, &r);

    ASSERT_FALSE(argh_parse(&p, argc, argv));
    ASSERT_EQ(r.calls, 1);
    ASSERT_EQ(argh_last_error(&p)->code, ARGH_E_CUSTOM);
    ASSERT_EQ(argh_exit_code(&p), 2);
    ASSERT_STR_EQ(err_text, "prog: --min must not be greater than --max\n"
                            "Try 'prog --help' for more information.\n");
}

TEST(test_validator_passes)
{
    ARGV("--min", "2");
    range r = {0, 10, 0};
    argh_parser p;
    setup(&p);
    argh_int(&p, 0, "min", &r.min, "");
    argh_int(&p, 0, "max", &r.max, "");
    argh_set_validator(&p, check_range, &r);

    ASSERT_TRUE(argh_parse(&p, argc, argv));
    ASSERT_EQ(r.calls, 1);
}

static bool reject_silently(argh_parser *p, void *ctx)
{
    (void)p;
    (void)ctx;
    return false;
}

TEST(test_validator_without_message)
{
    ARGV0();
    argh_parser p;
    setup(&p);
    argh_set_validator(&p, reject_silently, NULL);

    ASSERT_FALSE(argh_parse(&p, argc, argv));
    ASSERT_STR_EQ(error_text(&p), "invalid arguments");
}

TEST(test_validator_not_called_after_errors)
{
    ARGV("--min", "x");
    range r = {0, 10, 0};
    argh_parser p;
    setup(&p);
    argh_int(&p, 0, "min", &r.min, "");
    argh_set_validator(&p, check_range, &r);

    ASSERT_FALSE(argh_parse(&p, argc, argv));
    ASSERT_EQ(argh_last_error(&p)->code, ARGH_E_INVALID_VALUE);
    ASSERT_EQ(r.calls, 0);
}

TEST(test_validator_not_called_for_help)
{
    ARGV("--help");
    range r = {0, 10, 0};
    argh_parser p;
    setup(&p);
    argh_set_validator(&p, check_range, &r);

    ASSERT_FALSE(argh_parse(&p, argc, argv));
    ASSERT_EQ(argh_exit_code(&p), 0);
    ASSERT_EQ(r.calls, 0);
}

#ifndef ARGH_NO_COMMANDS
/* ============================================================================
 * Commands
 * ============================================================================ */

static bool c_verbose;
static bool c_release;
static bool c_force;
static const char *c_name;
static const char *c_url;
static argh_values c_args;
static int c_handler_calls;

static int c_run_build(argh_parser *p, void *user)
{
    (void)p;
    c_handler_calls++;
    return *(int *)user;
}

static const argh_opt c_build_opts[] = {
    ARGH_FLAG('r', "release", &c_release, "Optimized build"),
    ARGH_END,
};

static const argh_opt c_add_opts[] = {
    ARGH_FLAG('f', "force", &c_force, "Overwrite an existing remote"),
    ARGH_POS("name", &c_name, "Remote name"),
    ARGH_POS("url", &c_url, "Remote URL"),
    ARGH_END,
};

static const argh_opt c_exec_opts[] = {
    ARGH_REST("args", &c_args, "Arguments to pass on"),
    ARGH_END,
};

static const argh_cmd c_remote_cmds[] = {
    ARGH_CMD("add", "Add a remote", c_add_opts),
    ARGH_CMD("remove", "Remove a remote", NULL),
    ARGH_CMD_END,
};

static const argh_cmd c_cmds[] = {
    ARGH_CMD("build", "Build the project", c_build_opts, c_run_build),
    ARGH_CMD("exec", "Run a program", c_exec_opts),
    ARGH_CMD_GROUP("remote", "Manage remotes", c_remote_cmds),
    ARGH_CMD_END,
};

static void setup_commands(argh_parser *p)
{
    c_verbose = c_release = c_force = false;
    c_name = c_url = NULL;
    memset(&c_args, 0, sizeof(c_args));
    c_handler_calls = 0;
    argh_init(p, "tool", "Example tool");
    argh_set_writer(p, capture, NULL);
    argh_flag(p, 'v', "verbose", &c_verbose, "Verbose output");
    argh_commands(p, c_cmds);
}

TEST(test_command_dispatch)
{
    ARGV("build", "--release");
    int result = 7;
    argh_parser p;
    setup_commands(&p);

    ASSERT_TRUE(argh_parse(&p, argc, argv));
    ASSERT_TRUE(argh_command(&p) == &c_cmds[0]);
    ASSERT_TRUE(c_release);
    ASSERT_EQ(argh_run(&p, &result), 7);
    ASSERT_EQ(c_handler_calls, 1);
}

TEST(test_command_global_option_before_and_after)
{
    char *before[] = {(char *)"tool", (char *)"-v", (char *)"build", NULL};
    char *after[] = {(char *)"tool", (char *)"build", (char *)"-v", NULL};
    argh_parser p;

    setup_commands(&p);
    ASSERT_TRUE(argh_parse(&p, 3, before));
    ASSERT_TRUE(c_verbose);

    setup_commands(&p);
    ASSERT_TRUE(argh_parse(&p, 3, after));
    ASSERT_TRUE(c_verbose);
    ASSERT_TRUE(argh_given(&p, &c_verbose));
}

TEST(test_command_option_before_command_is_unknown)
{
    ARGV("--release", "build");
    argh_parser p;
    setup_commands(&p);

    ASSERT_FALSE(argh_parse(&p, argc, argv));
    ASSERT_STR_EQ(error_text(&p), "unknown option '--release'");
}

TEST(test_command_options_of_other_commands_are_unknown)
{
    ARGV("build", "--force");
    argh_parser p;
    setup_commands(&p);

    ASSERT_FALSE(argh_parse(&p, argc, argv));
    ASSERT_EQ(argh_last_error(&p)->code, ARGH_E_UNKNOWN_OPTION);
    ASSERT_FALSE(argh_given(&p, &c_force));
}

TEST(test_command_nested)
{
    ARGV("remote", "add", "-f", "origin", "https://example.com/repo.git");
    argh_parser p;
    setup_commands(&p);

    ASSERT_TRUE(argh_parse(&p, argc, argv));
    ASSERT_TRUE(argh_command(&p) == &c_remote_cmds[0]);
    ASSERT_TRUE(c_force);
    ASSERT_STR_EQ(c_name, "origin");
    ASSERT_STR_EQ(c_url, "https://example.com/repo.git");
    ASSERT_EQ(argh_run(&p, NULL), 0); /* no handler */
}

TEST(test_command_without_options)
{
    ARGV("remote", "remove");
    argh_parser p;
    setup_commands(&p);

    ASSERT_TRUE(argh_parse(&p, argc, argv));
    ASSERT_TRUE(argh_command(&p) == &c_remote_cmds[1]);
}

TEST(test_command_rest_and_double_dash)
{
    ARGV("exec", "-v", "--", "ls", "-la");
    argh_parser p;
    setup_commands(&p);

    ASSERT_TRUE(argh_parse(&p, argc, argv));
    ASSERT_TRUE(c_verbose);
    ASSERT_EQ(c_args.count, 2);
    ASSERT_STR_EQ(c_args.items[0], "ls");
    ASSERT_STR_EQ(c_args.items[1], "-la");
}

TEST(test_command_unknown)
{
    ARGV("biuld");
    argh_parser p;
    setup_commands(&p);

    ASSERT_FALSE(argh_parse(&p, argc, argv));
    ASSERT_EQ(argh_exit_code(&p), 2);
    ASSERT_STR_EQ(error_text(&p), "unknown command 'biuld'" DID_YOU_MEAN("build"));
    ASSERT_STR_EQ(err_text, "tool: unknown command 'biuld'" DID_YOU_MEAN("build") "\n"
                            "Try 'tool --help' for more information.\n");
}

TEST(test_command_missing)
{
    ARGV("-v");
    argh_parser p;
    setup_commands(&p);

    ASSERT_FALSE(argh_parse(&p, argc, argv));
    ASSERT_EQ(argh_exit_code(&p), 2);
    ASSERT_EQ(out_len, 0u); /* nothing on stdout: the run failed */
    ASSERT_STR_EQ(err_text,
                  "tool: missing command\n"
                  "\n"
                  "Commands:\n"
                  "  build   Build the project\n"
                  "  exec    Run a program\n"
                  "  remote  Manage remotes\n"
                  "\n"
                  "Try 'tool --help' for more information.\n");
}

TEST(test_command_missing_nested)
{
    ARGV("remote");
    argh_parser p;
    setup_commands(&p);

    ASSERT_FALSE(argh_parse(&p, argc, argv));
    ASSERT_STR_EQ(err_text,
                  "tool: 'remote' needs a command\n"
                  "\n"
                  "Commands:\n"
                  "  add     Add a remote\n"
                  "  remove  Remove a remote\n"
                  "\n"
                  "Try 'tool remote --help' for more information.\n");
}

TEST(test_command_help_root)
{
    ARGV("--help");
    argh_parser p;
    setup_commands(&p);

    ASSERT_FALSE(argh_parse(&p, argc, argv));
    ASSERT_EQ(argh_exit_code(&p), 0);
    ASSERT_STR_EQ(out_text,
                  "Usage: tool [OPTIONS] <command>\n"
                  "\n"
                  "Example tool\n"
                  "\n"
                  "Commands:\n"
                  "  build          Build the project\n"
                  "  exec           Run a program\n"
                  "  remote         Manage remotes\n"
                  "\n"
                  "Options:\n"
                  "  -v, --verbose  Verbose output\n"
                  "\n"
                  "  -h, --help     Print help\n");
}

static const char *const expected_add_help =
    "Usage: tool remote add [OPTIONS] <name> <url>\n"
    "\n"
    "Add a remote\n"
    "\n"
    "Arguments:\n"
    "  <name>         Remote name\n"
    "  <url>          Remote URL\n"
    "\n"
    "Options:\n"
    "  -f, --force    Overwrite an existing remote\n"
    "\n"
    "Global options:\n"
    "  -v, --verbose  Verbose output\n"
    "\n"
    "  -h, --help     Print help\n";

TEST(test_command_help_leaf)
{
    ARGV("remote", "add", "--help");
    argh_parser p;
    setup_commands(&p);

    ASSERT_FALSE(argh_parse(&p, argc, argv));
    ASSERT_EQ(argh_exit_code(&p), 0);
    if (strcmp(out_text, expected_add_help) != 0)
        printf("--- got ---\n%s--- expected ---\n%s", out_text, expected_add_help);
    ASSERT_STR_EQ(out_text, expected_add_help);
}

TEST(test_command_help_subcommand)
{
    ARGV("help", "remote", "add");
    argh_parser p;
    setup_commands(&p);

    ASSERT_FALSE(argh_parse(&p, argc, argv));
    ASSERT_EQ(argh_exit_code(&p), 0);
    ASSERT_STR_EQ(out_text, expected_add_help);
}

TEST(test_command_help_group)
{
    ARGV("remote", "-h");
    argh_parser p;
    setup_commands(&p);

    ASSERT_FALSE(argh_parse(&p, argc, argv));
    ASSERT_TRUE(strncmp(out_text, "Usage: tool remote [OPTIONS] <command>\n", 39) == 0);
    ASSERT_TRUE(strstr(out_text, "Commands:\n  add ") != NULL);
    ASSERT_TRUE(strstr(out_text, "Global options:\n  -v, --verbose") != NULL);
}

TEST(test_command_help_unknown)
{
    ARGV("help", "nope");
    argh_parser p;
    setup_commands(&p);

    ASSERT_FALSE(argh_parse(&p, argc, argv));
    ASSERT_EQ(argh_exit_code(&p), 2);
    ASSERT_STR_EQ(error_text(&p), "unknown command 'nope'");
}

TEST(test_command_config_root_positional)
{
    ARGV("build");
    const char *file = NULL;
    argh_parser p;
    setup_commands(&p);
    argh_pos(&p, "file", &file, "");

    ASSERT_FALSE(argh_parse(&p, argc, argv));
    ASSERT_EQ(argh_last_error(&p)->code, ARGH_E_CONFIG);
}

/* Definition checks run only without NDEBUG */
#ifndef NDEBUG
TEST(test_command_config_reserved_help)
{
    static const argh_cmd cmds[] = {ARGH_CMD("help", "Mine", NULL), ARGH_CMD_END};
    ARGV("help");
    argh_parser p;
    setup(&p);
    argh_commands(&p, cmds);

    ASSERT_FALSE(argh_parse(&p, argc, argv));
    ASSERT_STR_EQ(error_text(&p), "configuration error: command name 'help' is reserved, see ARGH_NO_AUTO_HELP");
}
#endif

/* Definition checks run only without NDEBUG */
#ifndef NDEBUG
TEST(test_command_config_duplicate_with_global)
{
    static bool clash;
    static const argh_opt opts[] = {ARGH_FLAG('v', "verbose", &clash, ""), ARGH_END};
    static const argh_cmd cmds[] = {ARGH_CMD("run", "", opts), ARGH_CMD_END};
    ARGV("run");
    bool verbose = false;
    argh_parser p;
    setup(&p);
    argh_flag(&p, 'v', "verbose", &verbose, "");
    argh_commands(&p, cmds);

    ASSERT_FALSE(argh_parse(&p, argc, argv));
    ASSERT_STR_EQ(error_text(&p), "configuration error: option defined twice (--verbose)");
}
#endif

/* Like `cargo install --version 1.0`: a command's own --version wins */
TEST(test_command_own_version_option)
{
    static const char *wanted;
    static bool vflag;
    static const argh_opt opts[] = {
        ARGH_STRING(0, "version", &wanted, "Version to install"),
        ARGH_FLAG('V', "verify", &vflag, "Verify"),
        ARGH_END,
    };
    static const argh_cmd cmds[] = {ARGH_CMD("install", "", opts), ARGH_CMD("list", "", NULL), ARGH_CMD_END};
    char *in_cmd[] = {(char *)"tool", (char *)"install", (char *)"--version", (char *)"1.2", (char *)"-V", NULL};
    char *at_root[] = {(char *)"tool", (char *)"--version", NULL};
    char *other_cmd[] = {(char *)"tool", (char *)"list", (char *)"-V", NULL};
    argh_parser p;

    setup(&p);
    argh_version(&p, "3.0");
    argh_commands(&p, cmds);
    ASSERT_TRUE(argh_parse(&p, 5, in_cmd));
    ASSERT_STR_EQ(wanted, "1.2");
    ASSERT_TRUE(vflag);

    reset_output();
    ASSERT_FALSE(argh_parse(&p, 2, at_root));
    ASSERT_STR_EQ(out_text, "prog 3.0\n");

    {
        /* help inside the command lists no built-in version line */
        char *help[] = {(char *)"tool", (char *)"install", (char *)"--help", NULL};
        reset_output();
        ASSERT_FALSE(argh_parse(&p, 3, help));
        ASSERT_TRUE(strstr(out_text, "Print version") == NULL);
    }

    reset_output();
    ASSERT_FALSE(argh_parse(&p, 3, other_cmd)); /* list has no -V of its own */
    ASSERT_STR_EQ(out_text, "prog 3.0\n");
}

#ifndef NDEBUG
TEST(test_command_config_reserved_help_option)
{
    static const char *host;
    static const argh_opt opts[] = {ARGH_STRING('h', "host", &host, ""), ARGH_END};
    static const argh_cmd cmds[] = {ARGH_CMD("connect", "", opts), ARGH_CMD_END};
    ARGV("connect");
    argh_parser p;
    setup(&p);
    argh_commands(&p, cmds);

    ASSERT_FALSE(argh_parse(&p, argc, argv));
    ASSERT_STR_EQ(error_text(&p), "configuration error: name reserved for help, see ARGH_NO_AUTO_HELP (--host)");
}
#endif

TEST(test_command_posix_mode_at_leaf)
{
    ARGV("exec", "ls", "-v");
    argh_parser p;
    setup_commands(&p);
    argh_set_flags(&p, ARGH_POSIX);

    ASSERT_TRUE(argh_parse(&p, argc, argv));
    ASSERT_FALSE(c_verbose);
    ASSERT_EQ(c_args.count, 2);
    ASSERT_STR_EQ(c_args.items[1], "-v");
}

/* ARGH_POSIX on one command: `tool exec prog --flags` without `--` */
static bool px_env;
static argh_values px_rest;
static const argh_opt px_exec_opts[] = {
    ARGH_FLAG('e', "env", &px_env, "Load .env"),
    ARGH_REST("command", &px_rest, "Program and its arguments"),
    ARGH_END,
};
static const argh_opt px_build_opts[] = {
    ARGH_REST("targets", &px_rest, "Targets"),
    ARGH_END,
};
static const argh_cmd px_cmds[] = {
    ARGH_CMD("exec", "Run a program", px_exec_opts, NULL, ARGH_POSIX),
    ARGH_CMD("build", "Build", px_build_opts),
    ARGH_CMD_END,
};

static void setup_posix_commands(argh_parser *p)
{
    px_env = c_verbose = false;
    memset(&px_rest, 0, sizeof(px_rest));
    argh_init(p, "tool", NULL);
    argh_set_writer(p, capture, NULL);
    argh_flag(p, 'v', "verbose", &c_verbose, "Verbose output");
    argh_commands(p, px_cmds);
}

TEST(test_command_posix_flag)
{
    ARGV("-v", "exec", "-e", "ls", "-v", "--color", "--help");
    argh_parser p;
    setup_posix_commands(&p);

    ASSERT_TRUE(argh_parse(&p, argc, argv));
    ASSERT_TRUE(c_verbose);
    ASSERT_TRUE(px_env);
    ASSERT_EQ(px_rest.count, 4);
    ASSERT_STR_EQ(px_rest.items[0], "ls");
    ASSERT_STR_EQ(px_rest.items[1], "-v");
    ASSERT_STR_EQ(px_rest.items[2], "--color");
    ASSERT_STR_EQ(px_rest.items[3], "--help");
    ASSERT_STR_EQ(out_text, "");
}

TEST(test_command_posix_flag_is_per_command)
{
    ARGV("build", "app", "-v");
    argh_parser p;
    setup_posix_commands(&p);

    ASSERT_TRUE(argh_parse(&p, argc, argv));
    ASSERT_TRUE(c_verbose);
    ASSERT_EQ(px_rest.count, 1);
}

TEST(test_command_posix_flag_accepts_double_dash)
{
    ARGV("exec", "--", "ls", "-l");
    argh_parser p;
    setup_posix_commands(&p);

    ASSERT_TRUE(argh_parse(&p, argc, argv));
    ASSERT_EQ(px_rest.count, 2);
    ASSERT_STR_EQ(px_rest.items[0], "ls");
    ASSERT_STR_EQ(px_rest.items[1], "-l");
}

#endif /* ARGH_NO_COMMANDS */

#ifndef ARGH_NO_SUGGEST
/* ============================================================================
 * Suggestions
 * ============================================================================ */

/* Parses one argument against a fixed set of options, returns the suggestion */
static const char *suggestion_for(const char *arg)
{
    static bool verbose, dry_run, color, secret;
    static int jobs;
    char *argv[] = {(char *)"prog", (char *)arg, NULL};
    argh_parser p;
    setup(&p);
    argh_version(&p, "1.0");
    argh_flag(&p, 'v', "verbose", &verbose, "");
    argh_flag(&p, 'n', "dry-run", &dry_run, "");
    argh_flag(&p, 0, "color", &color, "");
    argh_hidden(argh_flag(&p, 0, "secret-mode", &secret, ""));
    argh_int(&p, 'j', "jobs", &jobs, "");
    argh_parse(&p, 2, argv);
    return argh_last_error(&p)->suggestion;
}

TEST(test_suggest_options)
{
    ASSERT_STR_EQ(suggestion_for("--verbos"), "verbose");    /* deletion */
    ASSERT_STR_EQ(suggestion_for("--verbosee"), "verbose");  /* insertion */
    ASSERT_STR_EQ(suggestion_for("--verbsoe"), "verbose");   /* swap */
    ASSERT_STR_EQ(suggestion_for("--dryrun"), "dry-run");
    ASSERT_STR_EQ(suggestion_for("--colour"), "color");
}

TEST(test_suggest_value_form)
{
    ASSERT_STR_EQ(suggestion_for("--job=4"), "jobs");
}

TEST(test_suggest_builtins)
{
    ASSERT_STR_EQ(suggestion_for("--hlep"), "help");
    ASSERT_STR_EQ(suggestion_for("--versoin"), "version");
}

TEST(test_suggest_nothing_far_away)
{
    ASSERT_TRUE(suggestion_for("--output") == NULL);
    ASSERT_TRUE(suggestion_for("--jb") == NULL);      /* 2 edits on a 2-letter word */
    ASSERT_TRUE(suggestion_for("-x") == NULL);        /* short options: no suggestion */
    ASSERT_TRUE(suggestion_for("--secret-mod") == NULL); /* hidden options stay hidden */
}

#ifndef ARGH_NO_COMMANDS
TEST(test_suggest_commands)
{
    char *nested[] = {(char *)"tool", (char *)"remote", (char *)"ad", NULL};
    char *help[] = {(char *)"tool", (char *)"hepl", NULL};
    argh_parser p;

    setup_commands(&p);
    ASSERT_FALSE(argh_parse(&p, 3, nested));
    ASSERT_STR_EQ(error_text(&p), "unknown command 'ad' (did you mean 'add'?)");

    setup_commands(&p);
    ASSERT_FALSE(argh_parse(&p, 2, help));
    ASSERT_STR_EQ(argh_last_error(&p)->suggestion, "help");
}

TEST(test_suggest_on_command_path)
{
    char *argv[] = {(char *)"tool", (char *)"remote", (char *)"add", (char *)"--forse", NULL};
    char *global[] = {(char *)"tool", (char *)"build", (char *)"--verbos", NULL};
    char *other[] = {(char *)"tool", (char *)"build", (char *)"--forc", NULL};
    argh_parser p;

    setup_commands(&p);
    ASSERT_FALSE(argh_parse(&p, 4, argv));
    ASSERT_STR_EQ(argh_last_error(&p)->suggestion, "force");

    setup_commands(&p);
    ASSERT_FALSE(argh_parse(&p, 3, global));
    ASSERT_STR_EQ(argh_last_error(&p)->suggestion, "verbose");

    /* --force belongs to another command, so it is not suggested */
    setup_commands(&p);
    ASSERT_FALSE(argh_parse(&p, 3, other));
    ASSERT_TRUE(argh_last_error(&p)->suggestion == NULL);
}
#endif /* ARGH_NO_COMMANDS */
#endif /* ARGH_NO_SUGGEST */

TEST(test_parser_size)
{
    /* Budget from the design: the core stays small, the builder is extra */
    size_t builder = sizeof(argh_opt) * (ARGH_BUILDER_CAP + 1);
    printf("  sizeof(argh_parser) = %u bytes (%u core + %u builder)\n",
           (unsigned)sizeof(argh_parser), (unsigned)(sizeof(argh_parser) - builder), (unsigned)builder);
    ASSERT_TRUE(sizeof(argh_parser) - builder < 256);
}

/* ============================================================================ */

int main(void)
{
    printf("argh.h tests\n");

    RUN_TEST(test_flag_short_and_long);
    RUN_TEST(test_flag_cluster);
    RUN_TEST(test_flag_explicit_value);
    RUN_TEST(test_flag_invalid_value);
    RUN_TEST(test_flag_negatable);
    RUN_TEST(test_flag_not_negatable);
    RUN_TEST(test_negated_flag_rejects_value);
    RUN_TEST(test_count);
    RUN_TEST(test_count_rejects_value);

    RUN_TEST(test_int_forms);
    RUN_TEST(test_int_bases_and_signs);
    RUN_TEST(test_int_rejects_bad_input);
    RUN_TEST(test_int_error_message);
    RUN_TEST(test_long);
#ifndef ARGH_NO_FLOAT
    RUN_TEST(test_double);
    RUN_TEST(test_double_rejects_bad_input);
#endif

    RUN_TEST(test_string_forms);
    RUN_TEST(test_value_cluster);
    RUN_TEST(test_enum);
    RUN_TEST(test_enum_invalid);
    RUN_TEST(test_list);
    RUN_TEST(test_list_full);

    RUN_TEST(test_positionals_mixed_with_options);
    RUN_TEST(test_double_dash);
    RUN_TEST(test_double_dash_is_not_a_value);
    RUN_TEST(test_single_dash_is_positional);
    RUN_TEST(test_optional_positional);
    RUN_TEST(test_missing_positional);
    RUN_TEST(test_unexpected_argument);
    RUN_TEST(test_required_rest);
    RUN_TEST(test_posix_mode);

    RUN_TEST(test_unknown_long);
    RUN_TEST(test_unknown_short_in_cluster);
    RUN_TEST(test_no_prefix_matching);
    RUN_TEST(test_negative_number_is_not_positional);
    RUN_TEST(test_short_equals);
    RUN_TEST(test_short_flag_equals);
    RUN_TEST(test_missing_value);
    RUN_TEST(test_required_option);
    RUN_TEST(test_once);
    RUN_TEST(test_last_value_wins);
    RUN_TEST(test_format_error_truncates);
    RUN_TEST(test_no_error_after_success);

    RUN_TEST(test_config_reserved_help);
    RUN_TEST(test_config_no_auto_help_frees_h);
#ifndef NDEBUG
    RUN_TEST(test_config_duplicate);
#endif
    RUN_TEST(test_config_builder_overflow);
    RUN_TEST(test_config_missing_target);

    RUN_TEST(test_help_output);
    RUN_TEST(test_help_wraps_long_entries);
    RUN_TEST(test_help_number_defaults);
    RUN_TEST(test_help_in_cluster);
    RUN_TEST(test_help_wins_over_errors);
    RUN_TEST(test_help_as_value_is_a_value);
    RUN_TEST(test_help_after_double_dash_is_positional);
    RUN_TEST(test_value_containing_h_is_not_help);
    RUN_TEST(test_version);
    RUN_TEST(test_no_version_without_string);
    RUN_TEST(test_name_from_argv0);

    RUN_TEST(test_table_with_flags_argument);
    RUN_TEST(test_table_metavar);
    RUN_TEST(test_table_and_builder_mixed);
    RUN_TEST(test_given);
    RUN_TEST(test_empty_argv);
    RUN_TEST(test_parse_twice_resets_state);
    RUN_TEST(test_custom_builder);
    RUN_TEST(test_custom_table);
    RUN_TEST(test_custom_error_uses_reason);
    RUN_TEST(test_custom_struct_error);
    RUN_TEST(test_custom_help);
    RUN_TEST(test_custom_required);
    RUN_TEST(test_custom_config_without_type);
    RUN_TEST(test_rules_satisfied);
    RUN_TEST(test_rule_at_most_one);
    RUN_TEST(test_rule_exactly_one_missing);
    RUN_TEST(test_rule_exactly_one_both);
    RUN_TEST(test_rule_requires);
    RUN_TEST(test_rule_requires_only_when_given);
    RUN_TEST(test_rule_at_least_one);
    RUN_TEST(test_rule_three_names);
    RUN_TEST(test_rule_config_one_variable);
#ifndef NDEBUG
    RUN_TEST(test_rule_config_unbound_variable);
#endif
#if !defined(ARGH_NO_COMMANDS)
    RUN_TEST(test_rule_skipped_for_other_command);
#endif
    RUN_TEST(test_validator);
    RUN_TEST(test_validator_passes);
    RUN_TEST(test_validator_without_message);
    RUN_TEST(test_validator_not_called_after_errors);
    RUN_TEST(test_validator_not_called_for_help);
#if !defined(ARGH_NO_COMMANDS)
    RUN_TEST(test_command_dispatch);
#endif
#if !defined(ARGH_NO_COMMANDS)
    RUN_TEST(test_command_global_option_before_and_after);
#endif
#if !defined(ARGH_NO_COMMANDS)
    RUN_TEST(test_command_option_before_command_is_unknown);
#endif
#if !defined(ARGH_NO_COMMANDS)
    RUN_TEST(test_command_options_of_other_commands_are_unknown);
#endif
#if !defined(ARGH_NO_COMMANDS)
    RUN_TEST(test_command_nested);
#endif
#if !defined(ARGH_NO_COMMANDS)
    RUN_TEST(test_command_without_options);
#endif
#if !defined(ARGH_NO_COMMANDS)
    RUN_TEST(test_command_rest_and_double_dash);
#endif
#if !defined(ARGH_NO_COMMANDS)
    RUN_TEST(test_command_unknown);
#endif
#if !defined(ARGH_NO_COMMANDS)
    RUN_TEST(test_command_missing);
#endif
#if !defined(ARGH_NO_COMMANDS)
    RUN_TEST(test_command_missing_nested);
#endif
#if !defined(ARGH_NO_COMMANDS)
    RUN_TEST(test_command_help_root);
#endif
#if !defined(ARGH_NO_COMMANDS)
    RUN_TEST(test_command_help_leaf);
#endif
#if !defined(ARGH_NO_COMMANDS)
    RUN_TEST(test_command_help_subcommand);
#endif
#if !defined(ARGH_NO_COMMANDS)
    RUN_TEST(test_command_help_group);
#endif
#if !defined(ARGH_NO_COMMANDS)
    RUN_TEST(test_command_help_unknown);
#endif
#if !defined(ARGH_NO_COMMANDS)
    RUN_TEST(test_command_config_root_positional);
#endif
#if !defined(ARGH_NO_COMMANDS)
#ifndef NDEBUG
    RUN_TEST(test_command_config_reserved_help);
#endif
#endif
#if !defined(ARGH_NO_COMMANDS)
#ifndef NDEBUG
    RUN_TEST(test_command_config_duplicate_with_global);
#endif
#endif
#if !defined(ARGH_NO_COMMANDS)
    RUN_TEST(test_command_posix_mode_at_leaf);
    RUN_TEST(test_command_posix_flag);
    RUN_TEST(test_command_posix_flag_is_per_command);
    RUN_TEST(test_command_posix_flag_accepts_double_dash);
    RUN_TEST(test_command_own_version_option);
#ifndef NDEBUG
    RUN_TEST(test_command_config_reserved_help_option);
#endif
#endif
#if !defined(ARGH_NO_SUGGEST)
    RUN_TEST(test_suggest_options);
#endif
#if !defined(ARGH_NO_SUGGEST)
    RUN_TEST(test_suggest_value_form);
#endif
#if !defined(ARGH_NO_SUGGEST)
    RUN_TEST(test_suggest_builtins);
#endif
#if !defined(ARGH_NO_SUGGEST)
    RUN_TEST(test_suggest_nothing_far_away);
#endif
#if !defined(ARGH_NO_SUGGEST) && !defined(ARGH_NO_COMMANDS)
    RUN_TEST(test_suggest_commands);
#endif
#if !defined(ARGH_NO_SUGGEST) && !defined(ARGH_NO_COMMANDS)
    RUN_TEST(test_suggest_on_command_path);
#endif
    RUN_TEST(test_parser_size);

    printf("\nRun: %d\nPassed: %d\nFailed: %d\n", tests_run, tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
