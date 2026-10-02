/*
 * Parse speed of argh, getopt_long, cargs and argparse on one workload:
 * the one from bench/bench_parse.c (30 options, 17 arguments). Each
 * iteration sets up the parser, parses and reads every value back, with
 * numbers converted and checked. All four must read identical values.
 *
 * Build and run with bench/compare/compare.sh, which fetches cargs and
 * argparse at pinned versions.
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <getopt.h>

#define ARGH_IMPLEMENTATION
#include "../../argh.h"
#include "cargs.h"
#include "argparse.h"

#define ITERATIONS 200000
#define RUNS 5
#define NUM_FILLER 18

static char *const workload[] = {
    "prog", "-v", "--quiet", "-o", "out.txt", "--jobs=8", "input1.txt", "-abc", "--level", "3",
    "--ratio=0.75", "input2.txt", "--name", "benchmark", "-t", "42", "--force", "input3.txt",
};
#define ARGC ((int)(sizeof(workload) / sizeof(workload[0])))

static const char *const filler_long[NUM_FILLER] = {
    "opt01", "opt02", "opt03", "opt04", "opt05", "opt06", "opt07", "opt08", "opt09",
    "opt10", "opt11", "opt12", "opt13", "opt14", "opt15", "opt16", "opt17", "opt18",
};

static volatile long g_sink;

/* The values every parser fills in */
typedef struct
{
    int verbose, quiet, all, brief, color, force, jobs, level, threads, positionals;
    double ratio;
    const char *output, *name;
} values;

static void reset(values *x)
{
    memset(x, 0, sizeof(*x));
    x->jobs = 1;
    x->threads = 1;
    x->ratio = 1.0;
}

static long checksum(const values *x)
{
    return x->verbose + x->quiet + x->all + x->brief + x->color + x->force + x->jobs + x->level +
           x->threads + (long)(x->ratio * 100 + 0.5) + (long)strlen(x->output) + (long)strlen(x->name) +
           x->positionals;
}

static double now_ns(void)
{
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    return (double)ts.tv_sec * 1e9 + (double)ts.tv_nsec;
}

/* ---------------------------------------------------------------- argh */

static values av;
static bool a_verbose, a_quiet, a_all, a_brief, a_color, a_force;
static const char *a_filler[NUM_FILLER];
static argh_values a_rest;

static const argh_opt argh_table_opts[] = {
    ARGH_FLAG('v', "verbose", &a_verbose, ""),
    ARGH_FLAG('q', "quiet", &a_quiet, ""),
    ARGH_STRING('o', "output", &av.output, ""),
    ARGH_INT('j', "jobs", &av.jobs, ""),
    ARGH_FLAG('a', "all", &a_all, ""),
    ARGH_FLAG('b', "brief", &a_brief, ""),
    ARGH_FLAG('c', "color", &a_color, ""),
    ARGH_INT('l', "level", &av.level, ""),
    ARGH_DOUBLE('r', "ratio", &av.ratio, ""),
    ARGH_STRING('n', "name", &av.name, ""),
    ARGH_INT('t', "threads", &av.threads, ""),
    ARGH_FLAG('f', "force", &a_force, ""),
    ARGH_STRING(0, "opt01", &a_filler[0], ""), ARGH_STRING(0, "opt02", &a_filler[1], ""),
    ARGH_STRING(0, "opt03", &a_filler[2], ""), ARGH_STRING(0, "opt04", &a_filler[3], ""),
    ARGH_STRING(0, "opt05", &a_filler[4], ""), ARGH_STRING(0, "opt06", &a_filler[5], ""),
    ARGH_STRING(0, "opt07", &a_filler[6], ""), ARGH_STRING(0, "opt08", &a_filler[7], ""),
    ARGH_STRING(0, "opt09", &a_filler[8], ""), ARGH_STRING(0, "opt10", &a_filler[9], ""),
    ARGH_STRING(0, "opt11", &a_filler[10], ""), ARGH_STRING(0, "opt12", &a_filler[11], ""),
    ARGH_STRING(0, "opt13", &a_filler[12], ""), ARGH_STRING(0, "opt14", &a_filler[13], ""),
    ARGH_STRING(0, "opt15", &a_filler[14], ""), ARGH_STRING(0, "opt16", &a_filler[15], ""),
    ARGH_STRING(0, "opt17", &a_filler[16], ""), ARGH_STRING(0, "opt18", &a_filler[17], ""),
    ARGH_REST("files", &a_rest, ""),
    ARGH_END,
};

static void run_argh(char **argv)
{
    argh_parser p;
    reset(&av);
    a_verbose = a_quiet = a_all = a_brief = a_color = a_force = false;
    memset(&a_rest, 0, sizeof(a_rest));
    argh_init(&p, "prog", NULL);
    argh_table(&p, argh_table_opts);
    if (!argh_parse(&p, ARGC, argv))
        exit(1);
    av.verbose = a_verbose;
    av.quiet = a_quiet;
    av.all = a_all;
    av.brief = a_brief;
    av.color = a_color;
    av.force = a_force;
    av.positionals = a_rest.count;
    g_sink += checksum(&av);
}

/* ---------------------------------------------------- shared conversion */

/* getopt and cargs return strings: convert and check them like argh does */
static int to_int(const char *s)
{
    char *end;
    long n = strtol(s, &end, 10);
    if (end == s || *end || n < -2147483647L - 1 || n > 2147483647L)
        exit(1);
    return (int)n;
}

static double to_double(const char *s)
{
    char *end;
    double d = strtod(s, &end);
    if (end == s || *end)
        exit(1);
    return d;
}

static void store(values *x, int c, const char *arg)
{
    switch (c)
    {
    case 'v': x->verbose = 1; break;
    case 'q': x->quiet = 1; break;
    case 'a': x->all = 1; break;
    case 'b': x->brief = 1; break;
    case 'c': x->color = 1; break;
    case 'f': x->force = 1; break;
    case 'o': x->output = arg; break;
    case 'n': x->name = arg; break;
    case 'j': x->jobs = to_int(arg); break;
    case 'l': x->level = to_int(arg); break;
    case 't': x->threads = to_int(arg); break;
    case 'r': x->ratio = to_double(arg); break;
    case 'F': break; /* filler */
    default: exit(1);
    }
}

/* ------------------------------------------------------------- getopt */

static void run_getopt(char **argv)
{
    static struct option longopts[12 + NUM_FILLER + 1];
    static int ready;
    values x;
    int c;
    if (!ready)
    {
        static const struct option base[12] = {
            {"verbose", no_argument, 0, 'v'}, {"quiet", no_argument, 0, 'q'},
            {"output", required_argument, 0, 'o'}, {"jobs", required_argument, 0, 'j'},
            {"all", no_argument, 0, 'a'}, {"brief", no_argument, 0, 'b'},
            {"color", no_argument, 0, 'c'}, {"level", required_argument, 0, 'l'},
            {"ratio", required_argument, 0, 'r'}, {"name", required_argument, 0, 'n'},
            {"threads", required_argument, 0, 't'}, {"force", no_argument, 0, 'f'},
        };
        memcpy(longopts, base, sizeof(base));
        for (int i = 0; i < NUM_FILLER; i++)
        {
            longopts[12 + i].name = filler_long[i];
            longopts[12 + i].has_arg = required_argument;
            longopts[12 + i].val = 'F';
        }
        ready = 1;
    }
    reset(&x);
    optind = 0;
    while ((c = getopt_long(ARGC, argv, "vqo:j:abcl:r:n:t:f", longopts, NULL)) != -1)
        store(&x, c, optarg);
    x.positionals = ARGC - optind;
    g_sink += checksum(&x);
}

/* -------------------------------------------------------------- cargs */

static struct cag_option cargs_opts[12 + NUM_FILLER];

static void setup_cargs(void)
{
    static const struct cag_option base[12] = {
        {'v', "v", "verbose", NULL, ""}, {'q', "q", "quiet", NULL, ""},
        {'o', "o", "output", "FILE", ""}, {'j', "j", "jobs", "N", ""},
        {'a', "a", "all", NULL, ""}, {'b', "b", "brief", NULL, ""},
        {'c', "c", "color", NULL, ""}, {'l', "l", "level", "N", ""},
        {'r', "r", "ratio", "X", ""}, {'n', "n", "name", "NAME", ""},
        {'t', "t", "threads", "N", ""}, {'f', "f", "force", NULL, ""},
    };
    memcpy(cargs_opts, base, sizeof(base));
    for (int i = 0; i < NUM_FILLER; i++)
    {
        struct cag_option o = {'F', NULL, filler_long[i], "S", ""};
        memcpy(&cargs_opts[12 + i], &o, sizeof(o));
    }
}

static void run_cargs(char **argv)
{
    cag_option_context ctx;
    values x;
    reset(&x);
    cag_option_init(&ctx, cargs_opts, 12 + NUM_FILLER, ARGC, argv);
    while (cag_option_fetch(&ctx))
    {
        char id = cag_option_get_identifier(&ctx);
        if (id == '?')
            exit(1);
        store(&x, id, cag_option_get_value(&ctx));
    }
    x.positionals = ARGC - cag_option_get_index(&ctx);
    g_sink += checksum(&x);
}

/* ----------------------------------------------------------- argparse */

static values pv;
static int p_verbose, p_quiet, p_all, p_brief, p_color, p_force;
static float p_ratio;
static const char *p_filler[NUM_FILLER];
static struct argparse_option argparse_opts[12 + NUM_FILLER + 1];

static void setup_argparse(void)
{
    struct argparse_option base[12] = {
        OPT_BOOLEAN('v', "verbose", &p_verbose, "", NULL, 0, 0),
        OPT_BOOLEAN('q', "quiet", &p_quiet, "", NULL, 0, 0),
        OPT_STRING('o', "output", &pv.output, "", NULL, 0, 0),
        OPT_INTEGER('j', "jobs", &pv.jobs, "", NULL, 0, 0),
        OPT_BOOLEAN('a', "all", &p_all, "", NULL, 0, 0),
        OPT_BOOLEAN('b', "brief", &p_brief, "", NULL, 0, 0),
        OPT_BOOLEAN('c', "color", &p_color, "", NULL, 0, 0),
        OPT_INTEGER('l', "level", &pv.level, "", NULL, 0, 0),
        OPT_FLOAT('r', "ratio", &p_ratio, "", NULL, 0, 0),
        OPT_STRING('n', "name", &pv.name, "", NULL, 0, 0),
        OPT_INTEGER('t', "threads", &pv.threads, "", NULL, 0, 0),
        OPT_BOOLEAN('f', "force", &p_force, "", NULL, 0, 0),
    };
    memcpy(argparse_opts, base, sizeof(base));
    for (int i = 0; i < NUM_FILLER; i++)
    {
        struct argparse_option o = OPT_STRING(0, filler_long[i], &p_filler[i], "", NULL, 0, 0);
        memcpy(&argparse_opts[12 + i], &o, sizeof(o));
    }
    struct argparse_option end = OPT_END();
    memcpy(&argparse_opts[12 + NUM_FILLER], &end, sizeof(end));
}

static void run_argparse(char **argv)
{
    struct argparse ap;
    int left;
    reset(&pv);
    p_verbose = p_quiet = p_all = p_brief = p_color = p_force = 0;
    p_ratio = 1.0f;
    argparse_init(&ap, argparse_opts, NULL, 0);
    left = argparse_parse(&ap, ARGC, (const char **)argv);
    pv.verbose = p_verbose;
    pv.quiet = p_quiet;
    pv.all = p_all;
    pv.brief = p_brief;
    pv.color = p_color;
    pv.force = p_force;
    pv.ratio = p_ratio;
    pv.positionals = left;
    g_sink += checksum(&pv);
}

/* -------------------------------------------------------------- driver */

static long checksum_of(void (*fn)(char **))
{
    char *argv[ARGC + 1];
    memcpy(argv, workload, sizeof(workload));
    argv[ARGC] = NULL;
    g_sink = 0;
    fn(argv);
    return g_sink;
}

static double bench(void (*fn)(char **))
{
    char *argv[ARGC + 1];
    double best = 1e30;
    for (int run = 0; run < RUNS; run++)
    {
        double start = now_ns();
        for (int i = 0; i < ITERATIONS; i++)
        {
            memcpy(argv, workload, sizeof(workload));
            argv[ARGC] = NULL;
            fn(argv);
        }
        double t = (now_ns() - start) / ITERATIONS;
        if (t < best)
            best = t;
    }
    return best;
}

int main(void)
{
    static const struct
    {
        const char *name;
        void (*fn)(char **);
    } parsers[] = {
        {"argh (table)", run_argh},
        {"getopt_long", run_getopt},
        {"cargs", run_cargs},
        {"argparse", run_argparse},
    };
    long expected;

    setup_cargs();
    setup_argparse();
    expected = checksum_of(run_getopt);
    for (size_t i = 0; i < sizeof(parsers) / sizeof(parsers[0]); i++)
    {
        long got = checksum_of(parsers[i].fn);
        if (got != expected)
        {
            fprintf(stderr, "%s read different values: %ld, expected %ld\n", parsers[i].name, got, expected);
            return 1;
        }
    }
    printf("%-14s %10s\n", "parser", "ns/parse");
    for (size_t i = 0; i < sizeof(parsers) / sizeof(parsers[0]); i++)
        printf("%-14s %10.0f\n", parsers[i].name, bench(parsers[i].fn));
    return 0;
}
