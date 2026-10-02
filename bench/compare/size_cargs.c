/* Code-size probe: the program of bench/size_argh.c written with cargs */
#include <stdio.h>
#include <stdlib.h>
#include "cargs.h"

static const struct cag_option options[] = {
    {'v', "v", "verbose", NULL, "Verbose output"},
    {'n', "n", "count", "N", "Iterations"},
    {'o', "o", "output", "FILE", "Output file"},
    {'h', "h", "help", NULL, "Print help"},
};

int main(int argc, char **argv)
{
    int verbose = 0;
    int count = 10;
    const char *output = "out.txt";
    cag_option_context ctx;

    cag_option_init(&ctx, options, CAG_ARRAY_SIZE(options), argc, argv);
    while (cag_option_fetch(&ctx))
    {
        switch (cag_option_get_identifier(&ctx))
        {
        case 'v': verbose = 1; break;
        case 'n': count = atoi(cag_option_get_value(&ctx)); break;
        case 'o': output = cag_option_get_value(&ctx); break;
        case 'h': cag_option_print(options, CAG_ARRAY_SIZE(options), stdout); return 0;
        case '?': cag_option_print_error(&ctx, stderr); return 2;
        }
    }
    printf("%d %d %s\n", verbose, count, output);
    return 0;
}
