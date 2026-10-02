/* README.md, Ranges: the code as shown, in a program */
#include <stdio.h>

#define ARGH_IMPLEMENTATION
#include "argh.h"

int main(int argc, char **argv)
{
    int jobs = 4;
    unsigned level = 3;
    argh_parser p;
    argh_init(&p, "tool", NULL);

    argh_int(&p, 'j', "jobs", &jobs, "Parallel jobs");
    argh_range(&p, &jobs, 1, 64);      /* or in a table, right after the option: ARGH_RANGE(&jobs, 1, 64) */
    argh_uint(&p, 'l', "level", &level, "Compression level");
    argh_range(&p, &level, 1, 9);

    if (!argh_parse(&p, argc, argv))
        return argh_exit_code(&p);
    printf("jobs=%d level=%u\n", jobs, level);
    return 0;
}
