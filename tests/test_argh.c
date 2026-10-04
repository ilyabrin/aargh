/*
 * Test suite for argh.h
 *
 * Every test builds its own argv, parses it, and checks the variables, the
 * error, or the captured output. Output never reaches the terminal: a writer
 * collects it so help and error text can be compared exactly.
 */

/* A fake environment for ARGH_ENV tests: set_env("NAME", "value") */
static const char *fake_env_names[4];
static const char *fake_env_values[4];
static const char *fake_getenv(const char *name);
#define ARGH_GETENV(name) fake_getenv(name)

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

static const char *fake_getenv(const char *name)
{
    int i;
    for (i = 0; i < 4; i++)
        if (fake_env_names[i] && strcmp(fake_env_names[i], name) == 0)
            return fake_env_values[i];
    return NULL;
}

static void set_env(const char *name, const char *value)
{
    int i;
    for (i = 0; i < 4 && fake_env_names[i]; i++)
        ;
    if (i < 4)
    {
        fake_env_names[i] = name;
        fake_env_values[i] = value;
    }
}

static void reset_output(void)
{
    int i;
    for (i = 0; i < 4; i++)
        fake_env_names[i] = NULL;
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
/* Command suggestions have no dashes */
#ifdef ARGH_NO_SUGGEST
#define CMD_DID_YOU_MEAN(name) ""
#else
#define CMD_DID_YOU_MEAN(name) " (did you mean '" name "'?)"
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
    (void)argh_parse(&p, 3, argv);
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

TEST(test_uint_and_size)
{
    ARGV("--u=4000000000", "--x", "0xFFFFFFFF", "--p=+3", "--s", "0x10");
    unsigned u = 0, x = 0, plus = 0;
    size_t s = 0;
    argh_parser p;
    setup(&p);
    argh_uint(&p, 0, "u", &u, "");
    argh_uint(&p, 0, "x", &x, "");
    argh_uint(&p, 0, "p", &plus, "");
    argh_size(&p, 0, "s", &s, "");

#if UINT_MAX >= 4294967295U
    ASSERT_TRUE(argh_parse(&p, argc, argv));
    ASSERT_TRUE(u == 4000000000U);
    ASSERT_TRUE(x == 0xFFFFFFFFU);
    ASSERT_EQ(plus, 3);
    ASSERT_TRUE(s == 16);
#endif
}

static argh_err parse_uint_value(const char *text, bool size)
{
    char *argv[] = {(char *)"prog", (char *)"-n", (char *)text, NULL};
    unsigned n = 7;
    size_t z = 7;
    argh_parser p;
    setup(&p);
    if (size)
        argh_size(&p, 'n', "num", &z, "");
    else
        argh_uint(&p, 'n', "num", &n, "");
    (void)argh_parse(&p, 3, argv);
    /* A rejected value must leave the variable alone */
    if (argh_last_error(&p)->code != ARGH_E_NONE && (n != 7 || z != 7))
        return ARGH_E_CONFIG;
    return argh_last_error(&p)->code;
}

TEST(test_uint_rejects_bad_input)
{
    ASSERT_EQ(parse_uint_value("-1", false), ARGH_E_INVALID_VALUE); /* strtoul would give UINT_MAX */
    ASSERT_EQ(parse_uint_value("-0", false), ARGH_E_INVALID_VALUE);
    ASSERT_EQ(parse_uint_value("-1", true), ARGH_E_INVALID_VALUE);
    ASSERT_EQ(parse_uint_value("abc", false), ARGH_E_INVALID_VALUE);
    ASSERT_EQ(parse_uint_value("0x", false), ARGH_E_INVALID_VALUE);
    ASSERT_EQ(parse_uint_value("+", false), ARGH_E_INVALID_VALUE);
    ASSERT_EQ(parse_uint_value(" 5", true), ARGH_E_INVALID_VALUE);
    ASSERT_EQ(parse_uint_value("0", false), ARGH_E_NONE);
    ASSERT_EQ(parse_uint_value("99999999999999999999999", true), ARGH_E_OUT_OF_RANGE);
    ASSERT_EQ(parse_uint_value("0x10000000000000000", true), ARGH_E_OUT_OF_RANGE);
#if UINT_MAX == 4294967295U
    ASSERT_EQ(parse_uint_value("4294967295", false), ARGH_E_NONE);
    ASSERT_EQ(parse_uint_value("4294967296", false), ARGH_E_OUT_OF_RANGE);
    ASSERT_EQ(parse_uint_value("0x100000000", false), ARGH_E_OUT_OF_RANGE);
#endif
#if SIZE_MAX == 18446744073709551615U
    ASSERT_EQ(parse_uint_value("18446744073709551615", true), ARGH_E_NONE);
    ASSERT_EQ(parse_uint_value("18446744073709551616", true), ARGH_E_OUT_OF_RANGE);
#elif SIZE_MAX == 4294967295U
    ASSERT_EQ(parse_uint_value("4294967295", true), ARGH_E_NONE);
    ASSERT_EQ(parse_uint_value("4294967296", true), ARGH_E_OUT_OF_RANGE);
#endif
}

TEST(test_uint_error_message)
{
    ARGV("--size", "-5");
    size_t size = 4;
    argh_parser p;
    setup(&p);
    argh_size(&p, 0, "size", &size, "");

    ASSERT_FALSE(argh_parse(&p, argc, argv));
    ASSERT_STR_EQ(error_text(&p), "invalid value '-5' for '--size': expected a non-negative integer");
}

static unsigned ut_retries = 3;
static size_t ut_limit = SIZE_MAX;
static const argh_opt ut_opts[] = {
    ARGH_UINT('r', "retries", &ut_retries, "Retries"),
    ARGH_SIZE(0, "limit", &ut_limit, "Byte limit"),
    ARGH_ENV(&ut_limit, "TOOL_LIMIT"),
    ARGH_END,
};

TEST(test_uint_table_help)
{
    ARGV("--help");
    char max[32];
    argh_parser p;
    setup(&p);
    argh_table(&p, ut_opts);
    snprintf(max, sizeof(max), "(default: %llu)", (unsigned long long)SIZE_MAX);

    ASSERT_FALSE(argh_parse(&p, argc, argv));
    ASSERT_TRUE(strstr(out_text, "-r, --retries <n>  Retries (default: 3)\n") != NULL);
    ASSERT_TRUE(strstr(out_text, max) != NULL);
}

TEST(test_uint_from_env)
{
    ARGV0();
    argh_parser p;
    setup(&p);
    argh_table(&p, ut_opts);
    set_env("TOOL_LIMIT", "0x400");

    ASSERT_TRUE(argh_parse(&p, argc, argv));
    ASSERT_TRUE(ut_limit == 1024);

    reset_output(); /* clears the fake environment too */
    setup(&p);
    argh_table(&p, ut_opts);
    set_env("TOOL_LIMIT", "-1");
    ASSERT_FALSE(argh_parse(&p, argc, argv));
    ASSERT_STR_EQ(error_text(&p),
                  "invalid value '-1' in TOOL_LIMIT for '--limit': expected a non-negative integer");
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
    (void)argh_parse(&p, 3, argv);
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
    (void)argh_parse(p, argc, argv);
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
    (void)argh_parse(&p, 2, argv);
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

/* ============================================================================
 * Environment variables
 * ============================================================================ */

static int en_jobs;
static bool en_verbose;
static int en_count;
static const char *en_out;
static const char *en_inc_buf[2];
static argh_values en_inc;

static const argh_opt en_opts[] = {
    ARGH_INT('j', "jobs", &en_jobs, "Parallel jobs"),
    ARGH_FLAG(0, "verbose", &en_verbose, "Verbose output"),
    ARGH_COUNT('d', "debug", &en_count, "More debug output"),
    ARGH_LIST('I', "include", &en_inc, "Include dir"),
    ARGH_ENV(&en_jobs, "TOOL_JOBS"),
    ARGH_ENV(&en_verbose, "TOOL_VERBOSE"),
    ARGH_ENV(&en_count, "TOOL_DEBUG"),
    ARGH_ENV(&en_inc, "TOOL_INCLUDE"),
    ARGH_END,
};

static void setup_env(argh_parser *p)
{
    en_jobs = 4;
    en_verbose = false;
    en_count = 0;
    en_out = NULL;
    en_inc.items = en_inc_buf;
    en_inc.count = 0;
    en_inc.capacity = 2;
    setup(p);
    argh_table(p, en_opts);
}

TEST(test_env_fills_missing_options)
{
    ARGV0();
    argh_parser p;
    setup_env(&p);
    set_env("TOOL_JOBS", "8");
    set_env("TOOL_VERBOSE", "yes");
    set_env("TOOL_DEBUG", "3");
    set_env("TOOL_INCLUDE", "/opt/inc");

    ASSERT_TRUE(argh_parse(&p, argc, argv));
    ASSERT_EQ(en_jobs, 8);
    ASSERT_TRUE(en_verbose);
    ASSERT_EQ(en_count, 3);
    ASSERT_EQ(en_inc.count, 1);
    ASSERT_STR_EQ(en_inc.items[0], "/opt/inc");
    ASSERT_TRUE(argh_given(&p, &en_jobs));
}

TEST(test_env_command_line_wins)
{
    ARGV("-j", "2", "-dd");
    argh_parser p;
    setup_env(&p);
    set_env("TOOL_JOBS", "8");
    set_env("TOOL_DEBUG", "5");

    ASSERT_TRUE(argh_parse(&p, argc, argv));
    ASSERT_EQ(en_jobs, 2);
    ASSERT_EQ(en_count, 2);
}

TEST(test_env_unset_keeps_default)
{
    ARGV0();
    argh_parser p;
    setup_env(&p);

    ASSERT_TRUE(argh_parse(&p, argc, argv));
    ASSERT_EQ(en_jobs, 4);
    ASSERT_FALSE(argh_given(&p, &en_jobs));
}

TEST(test_env_value_is_checked)
{
    ARGV0();
    argh_parser p;
    setup_env(&p);
    set_env("TOOL_JOBS", "many");

    ASSERT_FALSE(argh_parse(&p, argc, argv));
    ASSERT_EQ(argh_exit_code(&p), 2);
    ASSERT_STR_EQ(error_text(&p), "invalid value 'many' in TOOL_JOBS for '--jobs': expected an integer");
    ASSERT_EQ(en_jobs, 4);
}

TEST(test_env_value_out_of_range)
{
    ARGV0();
    argh_parser p;
    setup_env(&p);
    set_env("TOOL_DEBUG", "-1");

    ASSERT_FALSE(argh_parse(&p, argc, argv));
    ASSERT_STR_EQ(error_text(&p), "value '-1' in TOOL_DEBUG for '--debug' is out of range");
}

TEST(test_env_in_help)
{
    ARGV("--help");
    argh_parser p;
    setup_env(&p);

    ASSERT_FALSE(argh_parse(&p, argc, argv));
    ASSERT_TRUE(strstr(out_text, "Parallel jobs [env: TOOL_JOBS] (default: 4)\n") != NULL);
    ASSERT_TRUE(strstr(out_text, "TOOL_JOBS\n") == NULL);
}

TEST(test_env_satisfies_required)
{
    ARGV0();
    const char *token = NULL;
    argh_parser p;
    setup(&p);
    argh_required(argh_string(&p, 0, "token", &token, "API token"));
    argh_env(&p, &token, "TOOL_TOKEN");
    set_env("TOOL_TOKEN", "s3cret");

    ASSERT_TRUE(argh_parse(&p, argc, argv));
    ASSERT_STR_EQ(token, "s3cret");
}

TEST(test_env_named_when_required_is_missing)
{
    ARGV0();
    const char *token = NULL;
    argh_parser p;
    setup(&p);
    argh_required(argh_string(&p, 0, "token", &token, "API token"));
    argh_env(&p, &token, "TOOL_TOKEN");

    ASSERT_FALSE(argh_parse(&p, argc, argv));
    ASSERT_STR_EQ(error_text(&p), "missing required option '--token' (or set TOOL_TOKEN)");
}

static bool en_json, en_yaml;
static const argh_rule en_rules[] = {ARGH_AT_MOST_ONE(&en_json, &en_yaml), ARGH_RULES_END};

TEST(test_env_counts_for_rules)
{
    ARGV("--yaml");
    argh_parser p;
    en_json = en_yaml = false;
    setup(&p);
    argh_flag(&p, 0, "json", &en_json, "JSON");
    argh_flag(&p, 0, "yaml", &en_yaml, "YAML");
    argh_env(&p, &en_json, "TOOL_JSON");
    argh_rules(&p, en_rules);
    set_env("TOOL_JSON", "1");

    ASSERT_FALSE(argh_parse(&p, argc, argv));
    ASSERT_STR_EQ(error_text(&p), "options '--json' and '--yaml' cannot be used together");
}

TEST(test_env_needs_an_option)
{
    ARGV0();
    int orphan = 0;
    argh_parser p;
    setup(&p);
    argh_env(&p, &orphan, "TOOL_ORPHAN");

    ASSERT_FALSE(argh_parse(&p, argc, argv));
    ASSERT_EQ(argh_last_error(&p)->code, ARGH_E_CONFIG);
}

#ifndef ARGH_NO_COMMANDS
static int en_port;
static const argh_opt en_serve_opts[] = {
    ARGH_INT('p', "port", &en_port, "Port"),
    ARGH_ENV(&en_port, "TOOL_PORT"),
    ARGH_END,
};
static const argh_cmd en_cmds[] = {
    ARGH_CMD("serve", "Serve", en_serve_opts),
    ARGH_CMD("check", "Check", NULL),
    ARGH_CMD_END,
};

/* A command's variables count only when that command runs */
TEST(test_env_per_command)
{
    char *serve[] = {(char *)"tool", (char *)"serve", NULL};
    char *check[] = {(char *)"tool", (char *)"check", NULL};
    argh_parser p;

    en_port = 80;
    argh_init(&p, "tool", NULL);
    argh_set_writer(&p, capture, NULL);
    argh_commands(&p, en_cmds);
    set_env("TOOL_PORT", "8080");
    ASSERT_TRUE(argh_parse(&p, 2, serve));
    ASSERT_EQ(en_port, 8080);

    en_port = 80;
    argh_init(&p, "tool", NULL);
    argh_set_writer(&p, capture, NULL);
    argh_commands(&p, en_cmds);
    ASSERT_TRUE(argh_parse(&p, 2, check));
    ASSERT_EQ(en_port, 80);
}
#endif

#ifndef NDEBUG
/* Examples are command lines only: the environment does not reach them */
TEST(test_env_not_used_by_examples)
{
    ARGV0();
    argh_parser p;
    setup_env(&p);
    argh_example(&p, "prog -j 2", NULL);
    set_env("TOOL_JOBS", "not a number");

    ASSERT_FALSE(argh_parse(&p, argc, argv));
    /* The real parse reads it and fails; the example check did not */
    ASSERT_STR_EQ(error_text(&p), "invalid value 'not a number' in TOOL_JOBS for '--jobs': expected an integer");
}
#endif

/* ============================================================================
 * Wrapping long help text
 * ============================================================================ */

#if ARGH_HELP_WIDTH >= 60 || ARGH_HELP_WIDTH == 0
static int wr_retries = 3;
static const char *wr_cache = "/var/cache/tool";
static bool wr_force;

static void setup_wrap(argh_parser *p)
{
    argh_init(p, "tool", "Synchronizes a local directory with a remote bucket, uploading new and changed "
                         "files and optionally deleting files that no longer exist locally.");
    argh_set_writer(p, capture, NULL);
    argh_int(p, 'r', "retries", &wr_retries,
             "How many times to retry a failed upload before giving up on that file and moving on to the next one");
    argh_string(p, 0, "cache-dir", &wr_cache, "Directory for the upload cache.\nDelete it to force a full upload.");
    argh_flag(p, 'f', "force", &wr_force,
              "Upload even if https://example.com/a/very/long/url/that/does/not/fit/on/one/line/at/all says no");
    argh_example(p, "tool --retries 5 --force",
                 "Upload, retrying each failed file up to five times before reporting it as failed");
}
#endif

#if ARGH_HELP_WIDTH == 80
TEST(test_help_wraps_at_80)
{
    ARGV("--help");
    argh_parser p;
    setup_wrap(&p);

    ASSERT_FALSE(argh_parse(&p, argc, argv));
    ASSERT_STR_EQ(out_text,
                  "Usage: tool [OPTIONS]\n"
                  "\n"
                  "Synchronizes a local directory with a remote bucket, uploading new and changed\n"
                  "files and optionally deleting files that no longer exist locally.\n"
                  "\n"
                  "Options:\n"
                  "  -r, --retries <n>        How many times to retry a failed upload before giving\n"
                  "                           up on that file and moving on to the next one\n"
                  "                           (default: 3)\n"
                  "      --cache-dir <value>  Directory for the upload cache.\n"
                  "                           Delete it to force a full upload.\n"
                  "                           (default: /var/cache/tool)\n"
                  "  -f, --force              Upload even if\n"
                  "                           https://example.com/a/very/long/url/that/does/not/fit/on/one/line/at/all\n"
                  "                           says no\n"
                  "\n"
                  "  -h, --help               Print help\n"
                  "\n"
                  "Examples:\n"
                  "  tool --retries 5 --force\n"
                  "      Upload, retrying each failed file up to five times before reporting it as\n"
                  "      failed\n");
}
#endif

#if ARGH_HELP_WIDTH >= 60
/* From 60 columns on, no line passes the width unless it holds a single word,
 * or a "(default: ...)" that is never split. Narrower widths leave too little
 * room next to long option names to promise that. */
TEST(test_help_lines_fit_width)
{
    ARGV("--help");
    const char *line;
    argh_parser p;
    setup_wrap(&p);

    ASSERT_FALSE(argh_parse(&p, argc, argv));
    for (line = out_text; *line;)
    {
        const char *end = strchr(line, '\n');
        const char *word = line;
        size_t len = end ? (size_t)(end - line) : strlen(line);
        while (*word == ' ')
            word++;
        if ((int)len > ARGH_HELP_WIDTH && strncmp(word, "(default: ", 10) != 0)
            ASSERT_TRUE(memchr(word, ' ', len - (size_t)(word - line)) == NULL);
        line += len + (end ? 1 : 0);
    }
}
#endif
#if ARGH_HELP_WIDTH == 0
TEST(test_help_no_wrap)
{
    ARGV("--help");
    argh_parser p;
    setup_wrap(&p);

    ASSERT_FALSE(argh_parse(&p, argc, argv));
    ASSERT_TRUE(strstr(out_text, "giving up on that file and moving on to the next one (default: 3)\n") != NULL);
}
#endif

/* ============================================================================
 * Examples in help
 * ============================================================================ */

static int ex_jobs;
static bool ex_verbose;
static const char *ex_name;
static const char *ex_in;
static const char *ex_inc_buf[2];
static argh_values ex_inc;

static const argh_opt ex_opts[] = {
    ARGH_FLAG('v', "verbose", &ex_verbose, "Verbose output"),
    ARGH_INT('j', "jobs", &ex_jobs, "Parallel jobs"),
    ARGH_STRING('n', "name", &ex_name, "Name"),
    ARGH_LIST('I', "include", &ex_inc, "Include dir"),
    ARGH_POS("input", &ex_in, "Input file", ARGH_OPTIONAL),
    ARGH_EXAMPLE("prog -j 8 in.txt", "Eight jobs"),
    ARGH_EXAMPLE("prog -v --name 'two words' -I a -I b -I c", NULL),
    ARGH_END,
};

static void setup_examples(argh_parser *p)
{
    ex_jobs = 4;
    ex_verbose = false;
    ex_name = "default";
    ex_in = NULL;
    ex_inc.items = ex_inc_buf;
    ex_inc.count = 0;
    ex_inc.capacity = 2;
    setup(p);
    argh_table(p, ex_opts);
}

TEST(test_example_in_help)
{
    ARGV("--help");
    argh_parser p;
    setup_examples(&p);

    ASSERT_FALSE(argh_parse(&p, argc, argv));
    ASSERT_EQ(argh_exit_code(&p), 0);
    /* Last in help, after the built-in options, and not listed as options */
    ASSERT_TRUE(strstr(out_text, "  -h, --help             Print help\n"
                                 "\n"
                                 "Examples:\n"
                                 "  prog -j 8 in.txt\n"
                                 "      Eight jobs\n"
                                 "  prog -v --name 'two words' -I a -I b -I c\n") != NULL);
    ASSERT_STR_EQ(out_text + strlen(out_text) - strlen("-I c\n"), "-I c\n");
    ASSERT_TRUE(strstr(out_text, "--prog") == NULL);
}

TEST(test_example_builder)
{
    ARGV("--help");
    int jobs = 1;
    argh_parser p;
    setup(&p);
    argh_int(&p, 'j', "jobs", &jobs, "Jobs");
    argh_example(&p, "prog -j 2", "Two jobs");

    ASSERT_FALSE(argh_parse(&p, argc, argv));
    ASSERT_TRUE(strstr(out_text, "Examples:\n  prog -j 2\n      Two jobs\n") != NULL);
}

TEST(test_no_examples_no_section)
{
    ARGV("--help");
    int jobs = 1;
    argh_parser p;
    setup(&p);
    argh_int(&p, 'j', "jobs", &jobs, "Jobs");

    ASSERT_FALSE(argh_parse(&p, argc, argv));
    ASSERT_TRUE(strstr(out_text, "Examples") == NULL);
}

#ifndef NDEBUG
/* Examples are parsed in full but write to no variable: -j 8, -v, --name,
 * the include list (three values with room for two) and the positional
 * keep their values from before argh_parse */
TEST(test_example_checked_writes_nothing)
{
    ARGV0();
    argh_parser p;
    setup_examples(&p);

    ASSERT_TRUE(argh_parse(&p, argc, argv));
    ASSERT_EQ(ex_jobs, 4);
    ASSERT_FALSE(ex_verbose);
    ASSERT_STR_EQ(ex_name, "default");
    ASSERT_EQ(ex_inc.count, 0);
    ASSERT_TRUE(ex_in == NULL);
    ASSERT_FALSE(argh_given(&p, &ex_jobs));
    ASSERT_STR_EQ(err_text, "");
}

/* Parses with a table plus one example, returns what went to stderr */
static const char *example_error(argh_parser *p, const char *example)
{
    static argh_opt table[2];
    static const argh_opt end = ARGH_END;
    char *argv[] = {(char *)"prog", NULL};
    argh_opt ex = ARGH_EXAMPLE(NULL, NULL);
    ex.long_name = example;
    table[0] = ex;
    table[1] = end;
    argh_table(p, table);
    reset_output();
    (void)argh_parse(p, 1, argv);
    return err_text;
}

TEST(test_example_broken_option)
{
    int jobs = 1;
    argh_parser p;
    setup(&p);
    argh_int(&p, 'j', "jobs", &jobs, "Jobs");

    ASSERT_STR_EQ(example_error(&p, "prog --jbos 8"),
                  "prog: example 'prog --jbos 8' does not work: unknown option '--jbos'" DID_YOU_MEAN("--jobs") "\n");
    ASSERT_EQ(argh_exit_code(&p), 2);
    /* Afterwards it is a definition mistake that names the example */
    ASSERT_EQ(argh_last_error(&p)->code, ARGH_E_CONFIG);
    ASSERT_STR_EQ(error_text(&p), "configuration error: this example does not work (example 'prog --jbos 8')");
    ASSERT_TRUE(argh_last_error(&p)->value == NULL);
}

TEST(test_example_broken_value)
{
    int jobs = 1;
    argh_parser p;
    setup(&p);
    argh_int(&p, 'j', "jobs", &jobs, "Jobs");

    ASSERT_STR_EQ(example_error(&p, "prog -j many"),
                  "prog: example 'prog -j many' does not work: invalid value 'many' for '-j': expected an integer\n");
    ASSERT_EQ(jobs, 1);
}

TEST(test_example_missing_required)
{
    const char *out = NULL;
    bool force = false;
    argh_parser p;
    setup(&p);
    argh_required(argh_string(&p, 'o', "out", &out, "Output"));
    argh_flag(&p, 'f', "force", &force, "Force");

    ASSERT_STR_EQ(example_error(&p, "prog -f"),
                  "prog: example 'prog -f' does not work: missing required option '--out'\n");
}

TEST(test_example_extra_argument)
{
    const char *in = NULL;
    argh_parser p;
    setup(&p);
    argh_pos(&p, "input", &in, "Input");

    ASSERT_STR_EQ(example_error(&p, "prog a.txt b.txt"),
                  "prog: example 'prog a.txt b.txt' does not work: unexpected argument 'b.txt'\n");
    ASSERT_TRUE(in == NULL);
}

static bool ex_json, ex_yaml;
static const argh_rule ex_rules[] = {ARGH_AT_MOST_ONE(&ex_json, &ex_yaml), ARGH_RULES_END};

TEST(test_example_breaks_rule)
{
    argh_parser p;
    setup(&p);
    argh_flag(&p, 0, "json", &ex_json, "JSON");
    argh_flag(&p, 0, "yaml", &ex_yaml, "YAML");
    argh_rules(&p, ex_rules);

    ASSERT_STR_EQ(example_error(&p, "prog --json --yaml"),
                  "prog: example 'prog --json --yaml' does not work: options '--json' and '--yaml' cannot be used together\n");
}

TEST(test_example_quotes)
{
    const char *name = NULL;
    argh_parser p;
    setup(&p);
    argh_string(&p, 'n', "name", &name, "Name");

    ASSERT_STR_EQ(example_error(&p, "prog --name \"open"),
                  "prog: example 'prog --name \"open' does not work: configuration error: an example needs at most 256 "
                  "characters, 32 words and closed quotes (example 'prog --name \"open')\n");
}

TEST(test_example_help_is_fine)
{
    int jobs = 1;
    argh_parser p;
    setup(&p);
    argh_int(&p, 'j', "jobs", &jobs, "Jobs");

    ASSERT_STR_EQ(example_error(&p, "prog --help"), "");
    ASSERT_EQ(argh_exit_code(&p), 0);
}

#ifndef ARGH_NO_COMMANDS
static const char *exc_name, *exc_url;
static const argh_opt exc_add_opts[] = {
    ARGH_POS("name", &exc_name, "Name"),
    ARGH_POS("url", &exc_url, "URL"),
    ARGH_EXAMPLE("tool remote add origin https://example.com", "Add a remote"),
    ARGH_END,
};
static const argh_cmd exc_remote[] = {ARGH_CMD("add", "Add a remote", exc_add_opts), ARGH_CMD_END};
static const argh_cmd exc_cmds[] = {ARGH_CMD_GROUP("remote", "Remotes", exc_remote), ARGH_CMD_END};
static const argh_opt exc_top[] = {ARGH_EXAMPLE("tool remote add o u", "Top-level example"), ARGH_END};

TEST(test_example_per_command)
{
    char *top[] = {(char *)"tool", (char *)"--help", NULL};
    char *sub[] = {(char *)"tool", (char *)"remote", (char *)"add", (char *)"--help", NULL};
    argh_parser p;

    /* The program's help shows the program's examples */
    argh_init(&p, "tool", NULL);
    argh_set_writer(&p, capture, NULL);
    argh_table(&p, exc_top);
    argh_commands(&p, exc_cmds);
    ASSERT_FALSE(argh_parse(&p, 2, top));
    ASSERT_TRUE(strstr(out_text, "Examples:\n  tool remote add o u\n") != NULL);
    ASSERT_TRUE(strstr(out_text, "origin") == NULL);

    /* A command's help shows its own */
    reset_output();
    argh_init(&p, "tool", NULL);
    argh_set_writer(&p, capture, NULL);
    argh_table(&p, exc_top);
    argh_commands(&p, exc_cmds);
    ASSERT_FALSE(argh_parse(&p, 4, sub));
    ASSERT_TRUE(strstr(out_text, "Examples:\n  tool remote add origin https://example.com\n      Add a remote\n") != NULL);
    ASSERT_TRUE(strstr(out_text, "Top-level") == NULL);
    ASSERT_TRUE(exc_name == NULL && exc_url == NULL);
}

static const argh_opt exc_bad_opts[] = {
    ARGH_POS("name", &exc_name, "Name"),
    ARGH_EXAMPLE("tool remote ad x", NULL),
    ARGH_END,
};
static const argh_cmd exc_bad_remote[] = {ARGH_CMD("add", "Add", exc_bad_opts), ARGH_CMD_END};
static const argh_cmd exc_bad_cmds[] = {ARGH_CMD_GROUP("remote", "Remotes", exc_bad_remote), ARGH_CMD_END};

/* Examples inside commands are checked too */
TEST(test_example_in_command_checked)
{
    char *argv[] = {(char *)"tool", NULL};
    argh_parser p;
    argh_init(&p, "tool", NULL);
    argh_set_writer(&p, capture, NULL);
    argh_commands(&p, exc_bad_cmds);

    ASSERT_FALSE(argh_parse(&p, 1, argv));
    ASSERT_STR_EQ(err_text, "tool: example 'tool remote ad x' does not work: unknown command 'ad'" CMD_DID_YOU_MEAN("add") "\n");
}
#endif /* ARGH_NO_COMMANDS */
#else
/* Release builds don't check examples */
TEST(test_example_not_checked_in_release)
{
    char *argv[] = {(char *)"prog", NULL};
    static const argh_opt table[] = {ARGH_EXAMPLE("prog --no-such-option", NULL), ARGH_END};
    argh_parser p;
    setup(&p);
    argh_table(&p, table);

    ASSERT_TRUE(argh_parse(&p, 1, argv));
}
#endif /* NDEBUG */

TEST(test_parser_size)
{
    /* Budget from the design: the core stays small, the builder is extra */
    size_t builder = sizeof(argh_opt) * (ARGH_BUILDER_CAP + 1);
    printf("  sizeof(argh_parser) = %u bytes (%u core + %u builder)\n",
           (unsigned)sizeof(argh_parser), (unsigned)(sizeof(argh_parser) - builder), (unsigned)builder);
    ASSERT_TRUE(sizeof(argh_parser) - builder < 256);
}

/* ============================================================================
 * Optional values (ARGH_IMPLICIT)
 * ============================================================================ */

static const char *const im_modes[] = {"never", "auto", "always", NULL};
static int im_color;
static bool im_verbose;
static argh_values im_rest;
static const argh_opt im_opts[] = {
    ARGH_ENUM('c', "color", &im_color, im_modes, "Colorize output"),
    ARGH_IMPLICIT(&im_color, "always"),
    ARGH_ENV(&im_color, "TOOL_COLOR"),
    ARGH_FLAG('v', "verbose", &im_verbose, "Verbose"),
    ARGH_REST("files", &im_rest, "Files"),
    ARGH_END,
};

static argh_parser im_p;

static bool im_parse(int argc, char **argv)
{
    im_color = 1;
    im_verbose = false;
    memset(&im_rest, 0, sizeof(im_rest));
    setup(&im_p);
    argh_table(&im_p, im_opts);
    return argh_parse(&im_p, argc, argv);
}

TEST(test_implicit_long)
{
    {
        ARGV("--color");
        ASSERT_TRUE(im_parse(argc, argv));
        ASSERT_EQ(im_color, 2);
    }
    {
        ARGV("--color=never");
        ASSERT_TRUE(im_parse(argc, argv));
        ASSERT_EQ(im_color, 0);
    }
    {
        ARGV0();
        ASSERT_TRUE(im_parse(argc, argv));
        ASSERT_EQ(im_color, 1); /* the default, not the implicit value */
    }
}

TEST(test_implicit_next_argument_is_positional)
{
    ARGV("--color", "never");
    ASSERT_TRUE(im_parse(argc, argv));
    ASSERT_EQ(im_color, 2);
    ASSERT_EQ(im_rest.count, 1);
    ASSERT_STR_EQ(im_rest.items[0], "never");
}

TEST(test_implicit_short)
{
    {
        ARGV("-cv");
        ASSERT_TRUE(im_parse(argc, argv));
        ASSERT_EQ(im_color, 2);
        ASSERT_TRUE(im_verbose);
    }
    {
        ARGV("-c", "never");
        ASSERT_TRUE(im_parse(argc, argv));
        ASSERT_EQ(im_color, 2);
        ASSERT_EQ(im_rest.count, 1);
    }
}

TEST(test_implicit_bad_value)
{
    ARGV("--color=sometimes");
    ASSERT_FALSE(im_parse(argc, argv));
    ASSERT_STR_EQ(error_text(&im_p),
                  "invalid value 'sometimes' for '--color': expected one of: never, auto, always");
}

TEST(test_implicit_env)
{
    ARGV0();
    reset_output();
    set_env("TOOL_COLOR", "never");
    {
        static argh_parser p;
        im_color = 1;
        setup(&p);
        argh_table(&p, im_opts);
        ASSERT_TRUE(argh_parse(&p, argc, argv));
        ASSERT_EQ(im_color, 0);
    }
}

TEST(test_implicit_builder_and_help)
{
    ARGV("--level");
    int level = 0;
    int width = 0;
    argh_parser p;
    setup(&p);
    argh_int(&p, 'l', "level", &level, "Compression level");
    argh_implicit(&p, &level, "6");
    argh_metavar(argh_int(&p, 0, "width", &width, "Width"), "<cols>");
    argh_implicit(&p, &width, "80");

    ASSERT_TRUE(argh_parse(&p, argc, argv));
    ASSERT_EQ(level, 6);
    ASSERT_TRUE(argh_given(&p, &level));
    ASSERT_FALSE(argh_given(&p, &width));

    argh_print_help(&p);
    ASSERT_TRUE(strstr(out_text, "  -l, --level[=<n>]     Compression level (default: 6)\n") != NULL);
    ASSERT_TRUE(strstr(out_text, "      --width[=<cols>]") != NULL);
}

#ifndef NDEBUG
static argh_err implicit_config(const argh_opt *opts, const char **detail)
{
    ARGV0();
    argh_parser p;
    setup(&p);
    argh_table(&p, opts);
    (void)argh_parse(&p, argc, argv);
    *detail = error_text(&p);
    return argh_last_error(&p)->code;
}

TEST(test_implicit_config_errors)
{
    static int mode, n;
    static bool flag;
    static const argh_opt typo[] = {ARGH_ENUM(0, "color", &mode, im_modes, ""), ARGH_IMPLICIT(&mode, "alwys"), ARGH_END};
    static const argh_opt on_flag[] = {ARGH_FLAG(0, "force", &flag, ""), ARGH_IMPLICIT(&flag, "yes"), ARGH_END};
    static const argh_opt short_only[] = {ARGH_INT('n', NULL, &n, ""), ARGH_IMPLICIT(&n, "1"), ARGH_END};
    static const argh_opt unbound[] = {ARGH_IMPLICIT(&n, "1"), ARGH_END};
    static const argh_opt apart[] = {ARGH_INT(0, "num", &n, ""), ARGH_FLAG(0, "force", &flag, ""), ARGH_IMPLICIT(&n, "1"), ARGH_END};
    const char *text;

    ASSERT_EQ(implicit_config(typo, &text), ARGH_E_CONFIG);
    ASSERT_STR_EQ(text, "configuration error: ARGH_IMPLICIT value is not valid for its option (--color)");
    ASSERT_EQ(implicit_config(on_flag, &text), ARGH_E_CONFIG);
    ASSERT_TRUE(strstr(text, "ARGH_IMPLICIT must directly follow a value option") != NULL);
    ASSERT_EQ(implicit_config(short_only, &text), ARGH_E_CONFIG);
    ASSERT_EQ(implicit_config(unbound, &text), ARGH_E_CONFIG);
    ASSERT_EQ(implicit_config(apart, &text), ARGH_E_CONFIG);
}
#endif

/* ============================================================================
 * Ranges (ARGH_RANGE)
 * ============================================================================ */

static int rg_jobs;
static long rg_offset;
static unsigned rg_level;
static size_t rg_cache;
static const argh_opt rg_opts[] = {
    ARGH_INT('j', "jobs", &rg_jobs, "Parallel jobs"),
    ARGH_RANGE(&rg_jobs, 1, 64),
    ARGH_ENV(&rg_jobs, "TOOL_JOBS"),
    ARGH_LONG(0, "offset", &rg_offset, "Offset"),
    ARGH_RANGE(&rg_offset, -100, 100),
    ARGH_UINT(0, "level", &rg_level, "Level"),
    ARGH_IMPLICIT(&rg_level, "6"),
    ARGH_RANGE(&rg_level, 1, 9),
    ARGH_SIZE(0, "cache", &rg_cache, "Cache size"),
    ARGH_RANGE(&rg_cache, 0, 1024),
    ARGH_END,
};

static argh_parser rg_p;

static bool rg_parse(int argc, char **argv)
{
    rg_jobs = 4;
    rg_offset = 0;
    rg_level = 3;
    rg_cache = 0;
    setup(&rg_p);
    argh_table(&rg_p, rg_opts);
    return argh_parse(&rg_p, argc, argv);
}

TEST(test_range_accepts_bounds)
{
    ARGV("-j", "64", "--offset=-100", "--level", "--cache=1024");
    ASSERT_TRUE(rg_parse(argc, argv));
    ASSERT_EQ(rg_jobs, 64);
    ASSERT_TRUE(rg_offset == -100);
    ASSERT_EQ(rg_level, 6);
    ASSERT_TRUE(rg_cache == 1024);
}

TEST(test_range_rejects_outside)
{
    {
        ARGV("-j", "0");
        ASSERT_FALSE(rg_parse(argc, argv));
        ASSERT_STR_EQ(error_text(&rg_p), "value '0' for '-j' is out of range (1 to 64)");
        ASSERT_EQ(rg_jobs, 4);
    }
    {
        ARGV("--offset=101");
        ASSERT_FALSE(rg_parse(argc, argv));
        ASSERT_STR_EQ(error_text(&rg_p), "value '101' for '--offset' is out of range (-100 to 100)");
    }
    {
        ARGV("--level=0");
        ASSERT_FALSE(rg_parse(argc, argv));
        ASSERT_STR_EQ(error_text(&rg_p), "value '0' for '--level' is out of range (1 to 9)");
    }
    {
        ARGV("--cache", "99999999999999999999999");
        ASSERT_FALSE(rg_parse(argc, argv));
        ASSERT_STR_EQ(error_text(&rg_p), "value '99999999999999999999999' for '--cache' is out of range (0 to 1024)");
    }
    {
        ARGV("--cache", "-1");
        ASSERT_FALSE(rg_parse(argc, argv));
        ASSERT_EQ(argh_last_error(&rg_p)->code, ARGH_E_INVALID_VALUE);
    }
}

TEST(test_range_env)
{
    ARGV0();
    reset_output();
    set_env("TOOL_JOBS", "100");
    ASSERT_FALSE(rg_parse(argc, argv));
    ASSERT_STR_EQ(error_text(&rg_p), "value '100' in TOOL_JOBS for '--jobs' is out of range (1 to 64)");
}

TEST(test_range_help)
{
    ARGV("--help");
    ASSERT_FALSE(rg_parse(argc, argv));
    ASSERT_TRUE(strstr(out_text, "  -j, --jobs <1..64>  ") != NULL);
    ASSERT_TRUE(strstr(out_text, "      --offset <-100..100>  ") != NULL);
    ASSERT_TRUE(strstr(out_text, "      --level[=<1..9>]  ") != NULL);
}

TEST(test_range_builder)
{
    ARGV("--port", "70000");
    int port = 8080;
    argh_parser p;
    setup(&p);
    argh_metavar(argh_int(&p, 'p', "port", &port, "Port"), "<port>");
    argh_range(&p, &port, 1, 65535);

    ASSERT_FALSE(argh_parse(&p, argc, argv));
    ASSERT_STR_EQ(error_text(&p), "value '70000' for '--port' is out of range (1 to 65535)");
    argh_print_help(&p);
    ASSERT_TRUE(strstr(out_text, "--port <port>") != NULL); /* your metavar wins */
}

#ifndef NDEBUG
TEST(test_range_config_errors)
{
    static int n;
    static unsigned u;
    static bool flag;
    static const argh_opt reversed[] = {ARGH_INT(0, "n", &n, ""), ARGH_RANGE(&n, 9, 1), ARGH_END};
    static const argh_opt on_flag[] = {ARGH_FLAG(0, "f", &flag, ""), ARGH_RANGE(&flag, 0, 1), ARGH_END};
    static const argh_opt negative_unsigned[] = {ARGH_UINT(0, "u", &u, ""), ARGH_RANGE(&u, -1, 5), ARGH_END};
    static const argh_opt apart[] = {ARGH_INT(0, "n", &n, ""), ARGH_FLAG(0, "f", &flag, ""), ARGH_RANGE(&n, 1, 2), ARGH_END};
    static const argh_opt implicit_outside[] = {ARGH_INT(0, "n", &n, ""), ARGH_RANGE(&n, 1, 5), ARGH_IMPLICIT(&n, "9"), ARGH_END};
    const char *text;

    ASSERT_EQ(implicit_config(reversed, &text), ARGH_E_CONFIG);
    ASSERT_STR_EQ(text, "configuration error: ARGH_RANGE bounds are reversed or outside the variable's type (--n)");
    ASSERT_EQ(implicit_config(on_flag, &text), ARGH_E_CONFIG);
    ASSERT_TRUE(strstr(text, "ARGH_RANGE must directly follow an integer option") != NULL);
    ASSERT_EQ(implicit_config(negative_unsigned, &text), ARGH_E_CONFIG);
    ASSERT_EQ(implicit_config(apart, &text), ARGH_E_CONFIG);
    ASSERT_EQ(implicit_config(implicit_outside, &text), ARGH_E_CONFIG);
    ASSERT_STR_EQ(text, "configuration error: ARGH_IMPLICIT value is not valid for its option (--n)");
}
#endif

/* ============================================================================
 * Limits and rare paths
 * ============================================================================ */

TEST(test_int_syntax_edges)
{
    ASSERT_EQ(parse_int_value("0x1g"), ARGH_E_INVALID_VALUE);
    ASSERT_EQ(parse_int_value("-+5"), ARGH_E_INVALID_VALUE);
    ASSERT_EQ(parse_int_value("+-5"), ARGH_E_INVALID_VALUE);
    ASSERT_EQ(parse_int_value("-0x10"), ARGH_E_NONE);
}

TEST(test_too_many_tables)
{
    static const argh_opt empty[] = {ARGH_END};
    ARGV0();
    bool flag = false;
    int i;
    argh_parser p;
    setup(&p);
    for (i = 0; i < ARGH_MAX_TABLES; i++)
        argh_table(&p, empty);
    /* The builder needs a table slot of its own */
    ASSERT_TRUE(argh_flag(&p, 'f', "flag", &flag, "") == NULL);
    argh_table(&p, empty);

    ASSERT_FALSE(argh_parse(&p, argc, argv));
    ASSERT_STR_EQ(error_text(&p), "configuration error: more tables than ARGH_MAX_TABLES");
}

/* ARGH_MAX_OPTS + 1 flags, each with its own name and variable */
static bool lim_flags[ARGH_MAX_OPTS + 1];
static char lim_names[ARGH_MAX_OPTS + 1][8];
static argh_opt lim_opts[ARGH_MAX_OPTS + 2];

static const argh_opt *lim_table(void)
{
    static const argh_opt end = ARGH_END;
    int i;
    for (i = 0; i <= ARGH_MAX_OPTS; i++)
    {
        argh_opt o = ARGH_FLAG(0, NULL, NULL, "");
        snprintf(lim_names[i], sizeof(lim_names[i]), "f%d", i);
        o.long_name = lim_names[i];
        o.target = &lim_flags[i];
        lim_opts[i] = o;
    }
    lim_opts[ARGH_MAX_OPTS + 1] = end;
    return lim_opts;
}

TEST(test_too_many_options)
{
    ARGV0();
    argh_parser p;
    setup(&p);
    argh_table(&p, lim_table());

    ASSERT_FALSE(argh_parse(&p, argc, argv));
    ASSERT_STR_EQ(error_text(&p), "configuration error: more options than ARGH_MAX_OPTS");
}

TEST(test_enum_without_choices)
{
    static int mode;
    static const argh_opt opts[] = {ARGH_ENUM('m', "mode", &mode, NULL, "Mode"), ARGH_END};
    ARGV0();
    argh_parser p;
    setup(&p);
    argh_table(&p, opts);

    ASSERT_FALSE(argh_parse(&p, argc, argv));
    ASSERT_STR_EQ(error_text(&p), "configuration error: enum option has no choices (--mode)");
}

TEST(test_no_suggestion_for_long_names)
{
    ARGV("--a-very-long-option-name-that-is-longer-than-anything-a-suggestion-would-compare");
    bool flag = false;
    argh_parser p;
    setup(&p);
    argh_flag(&p, 0, "a-very-long-option", &flag, "");

    ASSERT_FALSE(argh_parse(&p, argc, argv));
    ASSERT_TRUE(strstr(error_text(&p), "did you mean") == NULL);
}

TEST(test_group_builder)
{
    ARGV("--help");
    bool a = false, b = false;
    argh_parser p;
    setup(&p);
    argh_flag(&p, 'a', "alpha", &a, "Alpha");
    argh_group(&p, "Advanced");
    argh_flag(&p, 'b', "beta", &b, "Beta");

    ASSERT_FALSE(argh_parse(&p, argc, argv));
    ASSERT_TRUE(strstr(out_text, "\nAdvanced:\n  -b, --beta") != NULL);
}

TEST(test_format_error_without_error)
{
    ARGV0();
    char buf[16] = "x";
    argh_parser p;
    setup(&p);

    ASSERT_TRUE(argh_parse(&p, argc, argv));
    ASSERT_EQ(argh_format_error(&p, buf, sizeof(buf)), 0);
    ASSERT_STR_EQ(buf, "");
}

#ifndef ARGH_NO_COMMANDS
/* ARGH_MAX_DEPTH + 1 levels: a b c d e */
static const argh_cmd deep5[] = {ARGH_CMD("e", "E", NULL), ARGH_CMD_END};
static const argh_cmd deep4[] = {ARGH_CMD_GROUP("d", "D", deep5), ARGH_CMD_END};
static const argh_cmd deep3[] = {ARGH_CMD_GROUP("c", "C", deep4), ARGH_CMD_END};
static const argh_cmd deep2[] = {ARGH_CMD_GROUP("b", "B", deep3), ARGH_CMD_END};
static const argh_cmd deep1[] = {ARGH_CMD_GROUP("a", "A", deep2), ARGH_CMD_END};

TEST(test_commands_too_deep)
{
    ARGV("a", "b", "c", "d", "e");
    argh_parser p;
    setup(&p);
    argh_commands(&p, deep1);

    ASSERT_FALSE(argh_parse(&p, argc, argv));
    ASSERT_STR_EQ(error_text(&p), "configuration error: commands nested deeper than ARGH_MAX_DEPTH");
}

#ifdef NDEBUG
/* Without the definition check, the limit holds where the path is walked */
TEST(test_help_path_too_deep)
{
    ARGV("help", "a", "b", "c", "d", "e");
    argh_parser p;
    setup(&p);
    argh_commands(&p, deep1);

    ASSERT_FALSE(argh_parse(&p, argc, argv));
    ASSERT_STR_EQ(error_text(&p), "configuration error: commands nested deeper than ARGH_MAX_DEPTH");
}
#endif

TEST(test_help_command_extra_word)
{
    static const argh_cmd cmds[] = {ARGH_CMD("build", "Build", NULL), ARGH_CMD_END};
    ARGV("help", "build", "now");
    argh_parser p;
    setup(&p);
    argh_commands(&p, cmds);

    ASSERT_FALSE(argh_parse(&p, argc, argv));
    ASSERT_STR_EQ(error_text(&p), "unexpected argument 'now'");
}

TEST(test_too_many_options_on_command_path)
{
    static const argh_cmd cmds[] = {ARGH_CMD("run", "Run", lim_opts), ARGH_CMD_END};
    ARGV("run");
    argh_parser p;
    lim_table();
    setup(&p);
    argh_commands(&p, cmds);

    ASSERT_FALSE(argh_parse(&p, argc, argv));
    ASSERT_STR_EQ(error_text(&p), "configuration error: more options than ARGH_MAX_OPTS on this command path");
}

/* A command that takes over -V or --version: help lists only the free form */
TEST(test_help_version_partly_taken)
{
    static bool v;
    static const argh_opt long_taken[] = {ARGH_FLAG(0, "version", &v, "Show the package version"), ARGH_END};
    static const argh_opt short_taken[] = {ARGH_FLAG('V', "verify", &v, "Verify"), ARGH_END};
    static const argh_cmd cmds[] = {ARGH_CMD("install", "Install", long_taken), ARGH_CMD("check", "Check", short_taken),
                                    ARGH_CMD_END};
    argh_parser p;
    {
        ARGV("install", "--help");
        setup(&p);
        argh_version(&p, "1.0");
        argh_commands(&p, cmds);
        ASSERT_FALSE(argh_parse(&p, argc, argv));
        ASSERT_TRUE(strstr(out_text, "-V  ") != NULL || strstr(out_text, "  -V ") != NULL);
        ASSERT_TRUE(strstr(out_text, "Print version") != NULL);
        ASSERT_TRUE(strstr(out_text, "--version  Print version") == NULL);
    }
    {
        ARGV("check", "--help");
        reset_output();
        setup(&p);
        argh_version(&p, "1.0");
        argh_commands(&p, cmds);
        ASSERT_FALSE(argh_parse(&p, argc, argv));
        ASSERT_TRUE(strstr(out_text, "    --version") != NULL);
        ASSERT_TRUE(strstr(out_text, "-V, --version") == NULL);
    }
}

#ifndef NDEBUG
TEST(test_command_config_twice_and_positionals)
{
    static const char *file;
    static const argh_opt with_pos[] = {ARGH_POS("file", &file, "File"), ARGH_END};
    static const argh_cmd subs[] = {ARGH_CMD("x", "X", NULL), ARGH_CMD_END};
    static const argh_cmd twice[] = {ARGH_CMD("run", "Run", NULL), ARGH_CMD("run", "Again", NULL), ARGH_CMD_END};
    static const argh_cmd mixed[] = {{"group", "Group", with_pos, subs, NULL, 0}, ARGH_CMD_END};
    ARGV0();
    argh_parser p;

    setup(&p);
    argh_commands(&p, twice);
    ASSERT_FALSE(argh_parse(&p, argc, argv));
    ASSERT_STR_EQ(error_text(&p), "configuration error: command defined twice");

    setup(&p);
    argh_commands(&p, mixed);
    ASSERT_FALSE(argh_parse(&p, argc, argv));
    ASSERT_STR_EQ(error_text(&p), "configuration error: a command with subcommands cannot have positional arguments");
}
#endif
#endif

#ifndef NDEBUG
TEST(test_example_too_long)
{
    char words[ARGH__EXAMPLE_WORDS * 2 + 8] = "prog";
    size_t n = 4;
    int i;
    argh_parser p;
    for (i = 0; i < ARGH__EXAMPLE_WORDS; i++, n += 2)
        memcpy(words + n, " a", 3);
    /* 33 words with the program name: one more than an example may have */
    setup(&p);
    ASSERT_TRUE(strstr(example_error(&p, words), "an example needs at most 256 characters, 32 words") != NULL);
    ASSERT_EQ(argh_last_error(&p)->code, ARGH_E_CONFIG);
}
#endif

/* ============================================================================
 * Shell completion
 * ============================================================================ */

#if !defined(ARGH_NO_STDIO) && !defined(ARGH_NO_COMPLETION)

static const char *const sc_modes[] = {"fast", "safe", NULL};
static bool sc_color, sc_secret;
static int sc_mode, sc_level;
static const char *sc_out;
static const char *sc_input;
static const argh_opt sc_opts[] = {
    ARGH_FLAG(0, "color", &sc_color, "Colored output", ARGH_NEGATABLE),
    ARGH_ENUM('m', "mode", &sc_mode, sc_modes, "Don't guess the mode"),
    ARGH_STRING('o', "output", &sc_out, "Output file", ARGH_REQUIRED),
    ARGH_INT(0, "level", &sc_level, "Level"),
    ARGH_IMPLICIT(&sc_level, "6"),
    ARGH_FLAG(0, "secret", &sc_secret, "Not in help", ARGH_HIDDEN),
    ARGH_POS("input", &sc_input, "Input file"),
    ARGH_END,
};

static argh_parser sc_p;

static bool sc_parse(const char *name, int argc, char **argv)
{
    reset_output();
    argh_init(&sc_p, name, NULL);
    argh_set_writer(&sc_p, capture, NULL);
    argh_table(&sc_p, sc_opts);
    argh_completions(&sc_p);
    return argh_parse(&sc_p, argc, argv);
}

TEST(test_completions_print_like_help)
{
    ARGV("--completions", "bash");
    /* --output is required and missing: the script still comes out */
    ASSERT_FALSE(sc_parse("prog", argc, argv));
    ASSERT_EQ(argh_exit_code(&sc_p), 0);
    ASSERT_EQ(argh_last_error(&sc_p)->code, ARGH_E_NONE);
    ASSERT_TRUE(strncmp(out_text, "# bash completion for prog", 26) == 0);
    ASSERT_TRUE(strstr(out_text, "\ncomplete -F _prog prog\n") != NULL);
    ASSERT_STR_EQ(err_text, "");
}

TEST(test_completions_bash_content)
{
    ARGV("--completions", "bash");
    ASSERT_FALSE(sc_parse("prog", argc, argv));
    /* Options, the negated flag, built-ins; never the hidden one */
    ASSERT_TRUE(strstr(out_text, "\"\") echo ' --color --no-color -m --mode -o --output --level --completions -h --help ' ;;") != NULL);
    ASSERT_TRUE(strstr(out_text, "secret") == NULL);
    /* Values: choices, files for strings; --level=6 is optional, never the next word */
    ASSERT_TRUE(strstr(out_text, "*\"|-m\"|*\"|--mode\") echo 'fast safe' ;;") != NULL);
    ASSERT_TRUE(strstr(out_text, "*\"|-o\"|*\"|--output\") echo '@file' ;;") != NULL);
    ASSERT_TRUE(strstr(out_text, "--level\") return 0") == NULL);
    /* A positional takes file names */
    ASSERT_TRUE(strstr(out_text, "_prog_files() {\n    case \"$1\" in\n        \"\") return 0 ;;") != NULL);
}

TEST(test_completions_fish_content)
{
    ARGV("--completions", "fish");
    ASSERT_FALSE(sc_parse("my-tool", argc, argv));
    ASSERT_TRUE(strstr(out_text, "function __my_tool_at\n") != NULL);
    ASSERT_TRUE(strstr(out_text, "complete -c my-tool -n \"__my_tool_at ''\" -s m -l mode -d 'Don\\'t guess the mode' -x -a 'fast safe'\n") != NULL);
    ASSERT_TRUE(strstr(out_text, "-s o -l output -d 'Output file' -r\n") != NULL);
    ASSERT_TRUE(strstr(out_text, "-l no-color\n") != NULL);
    ASSERT_TRUE(strstr(out_text, "-s h -l help -d 'Print help'\n") != NULL);
    ASSERT_TRUE(strstr(out_text, "secret") == NULL);
}

TEST(test_completions_bad_shell)
{
    ARGV("--completions", "tcsh");
    ASSERT_FALSE(sc_parse("prog", argc, argv));
    ASSERT_EQ(argh_exit_code(&sc_p), 2);
    ASSERT_STR_EQ(error_text(&sc_p), "invalid value 'tcsh' for '--completions': expected one of: bash, zsh, fish");
}

TEST(test_completions_in_help)
{
    ARGV("--help");
    ASSERT_FALSE(sc_parse("prog", argc, argv));
    ASSERT_TRUE(strstr(out_text, "      --completions <shell>") != NULL);
    ASSERT_TRUE(strstr(out_text, "Print a completion script for bash, zsh or fish") != NULL);
}

TEST(test_print_completion)
{
    argh_parser p;
    setup(&p);
    argh_table(&p, sc_opts);
    ASSERT_FALSE(argh_print_completion(&p, "tcsh"));
    ASSERT_FALSE(argh_print_completion(&p, NULL));
    ASSERT_STR_EQ(out_text, "");
    ASSERT_TRUE(argh_print_completion(&p, "zsh"));
    ASSERT_TRUE(strstr(out_text, "\nautoload -U +X bashcompinit && bashcompinit\n") != NULL);
    ASSERT_TRUE(strstr(out_text, "\ncomplete -F _prog prog\n") != NULL);
}

#ifndef ARGH_NO_COMMANDS
TEST(test_completions_commands)
{
    static bool force;
    static const char *spec;
    static const argh_opt install_opts[] = {
        ARGH_STRING(0, "version", &spec, "Version to install"),
        ARGH_FLAG('f', "force", &force, "Force"),
        ARGH_END,
    };
    static const argh_cmd remote_cmds[] = {ARGH_CMD("add", "Add a remote", NULL), ARGH_CMD_END};
    static const argh_cmd cmds[] = {
        ARGH_CMD("install", "Install", install_opts),
        ARGH_CMD_GROUP("remote", "Remotes", remote_cmds),
        ARGH_CMD_END,
    };
    ARGV("--completions", "bash");
    argh_parser p;
    setup(&p);
    argh_version(&p, "1.0");
    argh_completions(&p);
    argh_commands(&p, cmds);

    ASSERT_FALSE(argh_parse(&p, argc, argv));
    ASSERT_TRUE(strstr(out_text, "\"\") echo 'install remote' ;;") != NULL);
    ASSERT_TRUE(strstr(out_text, "\"remote\") echo 'add' ;;") != NULL);
    /* install has its own --version: only -V stays built in there */
    ASSERT_TRUE(strstr(out_text, "\"install\") echo ' --completions --version -f --force -h --help -V ' ;;") != NULL);
    ASSERT_TRUE(strstr(out_text, "\"remote add\") echo ' --completions -h --help -V --version ' ;;") != NULL);
    ASSERT_TRUE(strstr(out_text, "\"install|--version\") echo '@file' ;;") != NULL);
}

TEST(test_completions_fish_commands)
{
    static bool verify;
    static const char *file;
    static const argh_opt check_opts[] = {
        ARGH_FLAG('V', "verify", &verify, "Verify"),
        ARGH_POS("file", &file, "File"),
        ARGH_END,
    };
    static const argh_cmd cmds[] = {ARGH_CMD("check", "Check a file", check_opts), ARGH_CMD("list", NULL, NULL),
                                    ARGH_CMD_END};
    argh_parser p;
    setup(&p);
    argh_version(&p, "1.0");
    argh_commands(&p, cmds);

    ASSERT_TRUE(argh_print_completion(&p, "fish"));
    ASSERT_TRUE(strstr(out_text, "set -g __prog_cmds '|check' '|list'\n") != NULL);
    ASSERT_TRUE(strstr(out_text, "complete -c prog -n \"__prog_at ''\" -f -a check -d 'Check a file'\n") != NULL);
    ASSERT_TRUE(strstr(out_text, "complete -c prog -n \"__prog_at ''\" -f -a list\n") != NULL);
    /* No positionals at the top or in list: no file names there */
    ASSERT_TRUE(strstr(out_text, "complete -c prog -n \"__prog_at ''\" -f\n") != NULL);
    ASSERT_TRUE(strstr(out_text, "complete -c prog -n \"__prog_at 'list'\" -f\n") != NULL);
    ASSERT_TRUE(strstr(out_text, "complete -c prog -n \"__prog_at 'check'\" -f\n") == NULL);
    /* check takes -V: only --version stays built in there */
    ASSERT_TRUE(strstr(out_text, "complete -c prog -n \"__prog_at 'check'\" -l version -d 'Print version'\n") != NULL);

    /* Without built-in help, no -h/--help to complete */
    reset_output();
    argh_set_flags(&p, ARGH_NO_AUTO_HELP);
    ASSERT_TRUE(argh_print_completion(&p, "bash"));
    ASSERT_TRUE(strstr(out_text, "--help") == NULL);
}
#endif
#endif /* completion */

/* ============================================================================
 * Gaps found by mutation testing (make mutation)
 * ============================================================================ */

static int flag_value(const char *text)
{
    char arg[64];
    char *argv[] = {(char *)"prog", arg, NULL};
    bool flag = false;
    argh_parser p;
    snprintf(arg, sizeof(arg), "--flag=%s", text);
    setup(&p);
    argh_flag(&p, 0, "flag", &flag, "");
    if (!argh_parse(&p, 2, argv))
        return -1;
    return flag;
}

TEST(test_flag_value_words)
{
    ASSERT_EQ(flag_value("true"), 1);
    ASSERT_EQ(flag_value("yes"), 1);
    ASSERT_EQ(flag_value("on"), 1);
    ASSERT_EQ(flag_value("1"), 1);
    ASSERT_EQ(flag_value("TRUE"), 1);
    ASSERT_EQ(flag_value("On"), 1);
    ASSERT_EQ(flag_value("false"), 0);
    ASSERT_EQ(flag_value("no"), 0);
    ASSERT_EQ(flag_value("off"), 0);
    ASSERT_EQ(flag_value("0"), 0);
    ASSERT_EQ(flag_value("NO"), 0);
    ASSERT_EQ(flag_value("Off"), 0);
    ASSERT_EQ(flag_value("maybe"), -1);
    ASSERT_EQ(flag_value("tru"), -1);
    ASSERT_EQ(flag_value("2"), -1);
    ASSERT_EQ(flag_value(""), -1);
}

TEST(test_given_positionals)
{
    ARGV("in.txt", "a", "b");
    const char *input = NULL, *missing = NULL;
    argh_values rest = {0};
    argh_parser p;
    setup(&p);
    argh_pos(&p, "input", &input, "");
    argh_rest(&p, "files", &rest, "");
    ASSERT_TRUE(argh_parse(&p, argc, argv));
    ASSERT_TRUE(argh_given(&p, &input));
    ASSERT_TRUE(argh_given(&p, &rest));

    {
        ARGV("in.txt");
        argh_parser q;
        setup(&q);
        argh_pos(&q, "input", &input, "");
        argh_optional(argh_pos(&q, "extra", &missing, ""));
        argh_rest(&q, "files", &rest, "");
        ASSERT_TRUE(argh_parse(&q, argc, argv));
        ASSERT_TRUE(argh_given(&q, &input));
        ASSERT_FALSE(argh_given(&q, &missing));
        ASSERT_FALSE(argh_given(&q, &rest));
        ASSERT_EQ(rest.count, 0);
    }
}

TEST(test_given_counter_from_env)
{
    ARGV0();
    int debug = 0;
    argh_parser p;
    reset_output();
    setup(&p);
    argh_count(&p, 'd', "debug", &debug, "");
    argh_env(&p, &debug, "TOOL_DEBUG");
    set_env("TOOL_DEBUG", "2");
    ASSERT_TRUE(argh_parse(&p, argc, argv));
    ASSERT_EQ(debug, 2);
    ASSERT_TRUE(argh_given(&p, &debug));
}

TEST(test_counter_stops_at_int_max)
{
    ARGV("-vvv");
    int level = INT_MAX - 1;
    argh_parser p;
    setup(&p);
    argh_count(&p, 'v', "verbose", &level, "");
    ASSERT_TRUE(argh_parse(&p, argc, argv));
    ASSERT_EQ(level, INT_MAX);
}

TEST(test_range_edges)
{
    ARGV("--one", "5", "--level", "1", "--size", "0");
    int one = 0;
    unsigned level = 3;
    size_t size = 9;
    argh_parser p;
    setup(&p);
    argh_int(&p, 0, "one", &one, "");
    argh_range(&p, &one, 5, 5); /* a single value is a valid range */
    argh_uint(&p, 0, "level", &level, "");
    argh_range(&p, &level, 1, 9);
    argh_size(&p, 0, "size", &size, "");
    argh_range(&p, &size, 0, 100);
    ASSERT_TRUE(argh_parse(&p, argc, argv));
    ASSERT_EQ(one, 5);
    ASSERT_EQ(level, 1);
    ASSERT_TRUE(size == 0);
}

#ifndef NDEBUG
TEST(test_range_bounds_fit_the_type)
{
    static int n;
    static unsigned u;
    static const argh_opt int_max[] = {ARGH_INT(0, "n", &n, ""), ARGH_RANGE(&n, 0, INT_MAX), ARGH_END};
    static const argh_opt uint_max[] = {ARGH_UINT(0, "u", &u, ""), ARGH_RANGE(&u, 0, (long)(UINT_MAX > LONG_MAX ? LONG_MAX : UINT_MAX)), ARGH_END};
    const char *text;

    ASSERT_EQ(implicit_config(int_max, &text), ARGH_E_NONE);
    ASSERT_EQ(implicit_config(uint_max, &text), ARGH_E_NONE);
#if LONG_MAX > INT_MAX
    {
        static const argh_opt past_int[] = {ARGH_INT(0, "n", &n, ""), ARGH_RANGE(&n, 0, (long)INT_MAX + 1), ARGH_END};
        static const argh_opt below_int[] = {ARGH_INT(0, "n", &n, ""), ARGH_RANGE(&n, (long)INT_MIN - 1, 0), ARGH_END};
        static const argh_opt past_uint[] = {ARGH_UINT(0, "u", &u, ""), ARGH_RANGE(&u, 0, (long)UINT_MAX + 1), ARGH_END};
        ASSERT_EQ(implicit_config(past_int, &text), ARGH_E_CONFIG);
        ASSERT_EQ(implicit_config(below_int, &text), ARGH_E_CONFIG);
        ASSERT_EQ(implicit_config(past_uint, &text), ARGH_E_CONFIG);
    }
#endif
}
#endif

TEST(test_version_after_an_error)
{
    argh_parser p;
    {
        ARGV("--jbos", "--version");
        setup(&p);
        argh_version(&p, "2.0");
        ASSERT_FALSE(argh_parse(&p, argc, argv));
        ASSERT_EQ(argh_exit_code(&p), 0);
        ASSERT_STR_EQ(out_text, "prog 2.0\n");
    }
    {
        ARGV("--jbos", "-qV");
        bool quiet = false;
        reset_output();
        setup(&p);
        argh_version(&p, "2.0");
        argh_flag(&p, 'q', "quiet", &quiet, "");
        ASSERT_FALSE(argh_parse(&p, argc, argv));
        ASSERT_EQ(argh_exit_code(&p), 0);
        ASSERT_STR_EQ(out_text, "prog 2.0\n");
    }
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
    RUN_TEST(test_uint_and_size);
    RUN_TEST(test_uint_rejects_bad_input);
    RUN_TEST(test_uint_error_message);
    RUN_TEST(test_uint_table_help);
    RUN_TEST(test_uint_from_env);
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
    RUN_TEST(test_env_fills_missing_options);
    RUN_TEST(test_env_command_line_wins);
    RUN_TEST(test_env_unset_keeps_default);
    RUN_TEST(test_env_value_is_checked);
    RUN_TEST(test_env_value_out_of_range);
    RUN_TEST(test_env_in_help);
    RUN_TEST(test_env_satisfies_required);
    RUN_TEST(test_env_named_when_required_is_missing);
    RUN_TEST(test_env_counts_for_rules);
    RUN_TEST(test_env_needs_an_option);
#ifndef ARGH_NO_COMMANDS
    RUN_TEST(test_env_per_command);
#endif
#ifndef NDEBUG
    RUN_TEST(test_env_not_used_by_examples);
#endif
    RUN_TEST(test_implicit_long);
    RUN_TEST(test_implicit_next_argument_is_positional);
    RUN_TEST(test_implicit_short);
    RUN_TEST(test_implicit_bad_value);
    RUN_TEST(test_implicit_env);
    RUN_TEST(test_implicit_builder_and_help);
    RUN_TEST(test_range_accepts_bounds);
    RUN_TEST(test_range_rejects_outside);
    RUN_TEST(test_range_env);
    RUN_TEST(test_range_help);
    RUN_TEST(test_range_builder);
    RUN_TEST(test_flag_value_words);
    RUN_TEST(test_given_positionals);
    RUN_TEST(test_given_counter_from_env);
    RUN_TEST(test_counter_stops_at_int_max);
    RUN_TEST(test_range_edges);
#ifndef NDEBUG
    RUN_TEST(test_range_bounds_fit_the_type);
#endif
    RUN_TEST(test_version_after_an_error);
#if !defined(ARGH_NO_STDIO) && !defined(ARGH_NO_COMPLETION)
    RUN_TEST(test_completions_print_like_help);
    RUN_TEST(test_completions_bash_content);
    RUN_TEST(test_completions_fish_content);
    RUN_TEST(test_completions_bad_shell);
    RUN_TEST(test_completions_in_help);
    RUN_TEST(test_print_completion);
#ifndef ARGH_NO_COMMANDS
    RUN_TEST(test_completions_commands);
    RUN_TEST(test_completions_fish_commands);
#endif
#endif
    RUN_TEST(test_int_syntax_edges);
    RUN_TEST(test_too_many_tables);
    RUN_TEST(test_too_many_options);
    RUN_TEST(test_enum_without_choices);
    RUN_TEST(test_no_suggestion_for_long_names);
    RUN_TEST(test_group_builder);
    RUN_TEST(test_format_error_without_error);
#ifndef ARGH_NO_COMMANDS
    RUN_TEST(test_commands_too_deep);
#ifdef NDEBUG
    RUN_TEST(test_help_path_too_deep);
#endif
    RUN_TEST(test_help_command_extra_word);
    RUN_TEST(test_too_many_options_on_command_path);
    RUN_TEST(test_help_version_partly_taken);
#ifndef NDEBUG
    RUN_TEST(test_command_config_twice_and_positionals);
#endif
#endif
#ifndef NDEBUG
    RUN_TEST(test_example_too_long);
#endif
#ifndef NDEBUG
    RUN_TEST(test_range_config_errors);
#endif
#ifndef NDEBUG
    RUN_TEST(test_implicit_config_errors);
#endif
#if ARGH_HELP_WIDTH == 80
    RUN_TEST(test_help_wraps_at_80);
#endif
#if ARGH_HELP_WIDTH >= 60
    RUN_TEST(test_help_lines_fit_width);
#endif
#if ARGH_HELP_WIDTH == 0
    RUN_TEST(test_help_no_wrap);
#endif
    RUN_TEST(test_example_in_help);
    RUN_TEST(test_example_builder);
    RUN_TEST(test_no_examples_no_section);
#ifndef NDEBUG
    RUN_TEST(test_example_checked_writes_nothing);
    RUN_TEST(test_example_broken_option);
    RUN_TEST(test_example_broken_value);
    RUN_TEST(test_example_missing_required);
    RUN_TEST(test_example_extra_argument);
    RUN_TEST(test_example_breaks_rule);
    RUN_TEST(test_example_quotes);
    RUN_TEST(test_example_help_is_fine);
#ifndef ARGH_NO_COMMANDS
    RUN_TEST(test_example_per_command);
    RUN_TEST(test_example_in_command_checked);
#endif
#else
    RUN_TEST(test_example_not_checked_in_release);
#endif
    RUN_TEST(test_parser_size);

    printf("\nRun: %d\nPassed: %d\nFailed: %d\n", tests_run, tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
