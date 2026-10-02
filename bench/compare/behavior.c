/*
 * How argh, cargs and argparse treat tricky input. One option each:
 * -j/--jobs (an integer). Run: behavior <argh|cargs|argparse> args...
 * Prints "jobs=<value>" when parsing succeeded; each library reports errors
 * its own way. compare.sh runs it on a list of inputs and records the
 * output and exit code.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ARGH_IMPLEMENTATION
#include "../../argh.h"
#include "cargs.h"
#include "argparse.h"

static int with_argh(int argc, char **argv)
{
    int jobs = 1;
    argh_parser p;
    argh_init(&p, "tool", NULL);
    argh_int(&p, 'j', "jobs", &jobs, "Jobs");
    if (!argh_parse(&p, argc, argv))
        return argh_exit_code(&p);
    printf("jobs=%d\n", jobs);
    return 0;
}

/* cargs returns strings and leaves converting and checking them to the
 * program. Here: atoi, the shortest way, with no checks of its own. */
static int with_cargs(int argc, char **argv)
{
    static const struct cag_option opts[] = {
        {'j', "j", "jobs", "N", "Jobs"},
        {'h', "h", "help", NULL, "Help"},
    };
    int jobs = 1;
    cag_option_context ctx;
    cag_option_init(&ctx, opts, 2, argc, argv);
    while (cag_option_fetch(&ctx))
    {
        switch (cag_option_get_identifier(&ctx))
        {
        case 'j':
            /* cargs does not report a missing value: it returns NULL */
            if (!cag_option_get_value(&ctx))
            {
                printf("no error, value is NULL\n");
                return 0;
            }
            jobs = atoi(cag_option_get_value(&ctx));
            break;
        case 'h':
            cag_option_print(opts, 2, stdout);
            return 0;
        case '?':
            cag_option_print_error(&ctx, stderr);
            return 1;
        }
    }
    printf("jobs=%d\n", jobs);
    return 0;
}

static int with_argparse(int argc, char **argv)
{
    int jobs = 1;
    struct argparse_option opts[] = {
        OPT_HELP(),
        OPT_INTEGER('j', "jobs", &jobs, "Jobs", NULL, 0, 0),
        OPT_END(),
    };
    struct argparse ap;
    argparse_init(&ap, opts, NULL, 0);
    argparse_parse(&ap, argc, (const char **)argv);
    printf("jobs=%d\n", jobs);
    return 0;
}

int main(int argc, char **argv)
{
    static char name[] = "tool";
    const char *lib;
    if (argc < 2)
        return 100;
    /* behavior <lib> args...  ->  tool args... */
    lib = argv[1];
    argv[1] = name;
    argc--;
    argv++;
    if (strcmp(lib, "argh") == 0)
        return with_argh(argc, argv);
    if (strcmp(lib, "cargs") == 0)
        return with_cargs(argc, argv);
    if (strcmp(lib, "argparse") == 0)
        return with_argparse(argc, argv);
    return 100;
}
