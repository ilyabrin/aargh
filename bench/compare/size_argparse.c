/* Code-size probe: the program of bench/size_argh.c written with argparse */
#include <stdio.h>
#include "argparse.h"

int main(int argc, char **argv)
{
    int verbose = 0;
    int count = 10;
    const char *output = "out.txt";
    struct argparse_option options[] = {
        OPT_HELP(),
        OPT_BOOLEAN('v', "verbose", &verbose, "Verbose output", NULL, 0, 0),
        OPT_INTEGER('n', "count", &count, "Iterations", NULL, 0, 0),
        OPT_STRING('o', "output", &output, "Output file", NULL, 0, 0),
        OPT_END(),
    };
    struct argparse ap;

    argparse_init(&ap, options, NULL, 0);
    argparse_parse(&ap, argc, (const char **)argv);
    printf("%d %d %s\n", verbose, count, output);
    return 0;
}
