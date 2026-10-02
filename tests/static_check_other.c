/* A second private copy of argh.h, for tests/static_check.c */
#define ARGH_STATIC
#include "../argh.h"

int other_file_parse(int argc, char **argv);

int other_file_parse(int argc, char **argv)
{
    int jobs = 1;
    bool verbose = false;
    argh_parser p;
    argh_init(&p, "other", NULL);
    argh_int(&p, 'j', "jobs", &jobs, "Jobs");
    argh_flag(&p, 'v', "verbose", &verbose, "Verbose output");
    return argh_parse(&p, argc, argv) ? 0 : argh_exit_code(&p);
}
