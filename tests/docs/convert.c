/* README.md, Examples in help. Built twice: as shown, and with -DBROKEN,
 * where the example has the typo the second console block shows. */
#define ARGH_IMPLEMENTATION
#include "argh.h"

int main(int argc, char **argv)
{
    int jobs = 4;
    const char *input = NULL;
    argh_parser p;

    argh_init(&p, "convert", "Converts data files");
    argh_int(&p, 'j', "jobs", &jobs, "Parallel jobs");
    argh_pos(&p, "input", &input, "Input file");
#ifndef BROKEN
    argh_example(&p, "convert -j 8 data.csv", "Convert with 8 parallel jobs");
    /* or in a table: ARGH_EXAMPLE("convert -j 8 data.csv", "Convert with 8 parallel jobs") */
#else
    argh_example(&p, "convert --jbos 8 data.csv", "Convert with 8 parallel jobs");
#endif
    return argh_parse(&p, argc, argv) ? 0 : argh_exit_code(&p);
}
