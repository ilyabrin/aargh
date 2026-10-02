/*
 * ARGH_STATIC: the whole library in this file, every function static.
 * Built with warnings as errors, so unused static functions must not warn.
 * static_check_other.c includes argh.h the same way; linking both proves the
 * two copies don't clash.
 */
#define ARGH_STATIC
#include "../argh.h"

#if ARGH_VERSION_MAJOR < 0 || !defined(ARGH_VERSION)
#error "version macros missing"
#endif

int other_file_parse(int argc, char **argv);

int main(int argc, char **argv)
{
    bool verbose = false;
    argh_parser p;
    argh_init(&p, "static", NULL);
    argh_flag(&p, 'v', "verbose", &verbose, "Verbose output");
    if (!argh_parse(&p, argc, argv))
        return argh_exit_code(&p);
    return other_file_parse(argc, argv);
}
