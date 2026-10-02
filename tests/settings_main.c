/*
 * Must NOT link with tests/settings_impl.c: this file uses a smaller
 * ARGH_BUILDER_CAP than the implementation, so the parser it puts on the
 * stack is too small for the code that fills it. `make test` checks that
 * the link fails instead of the program corrupting memory.
 */
#define ARGH_BUILDER_CAP 2
#include "../argh.h"

int main(int argc, char **argv)
{
    bool verbose = false;
    argh_parser p;
    argh_init(&p, "settings", NULL);
    argh_flag(&p, 'v', "verbose", &verbose, "Verbose output");
    return argh_parse(&p, argc, argv) ? 0 : argh_exit_code(&p);
}
