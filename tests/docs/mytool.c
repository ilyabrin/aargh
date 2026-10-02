/* README.md, Quick start: the program exactly as shown */
#include <stdio.h>

#define ARGH_IMPLEMENTATION
#include "argh.h"

int main(int argc, char **argv)
{
    bool verbose = false;
    int jobs = 4;
    const char *output = "out.txt";
    const char *input = NULL;

    argh_parser p;
    argh_init(&p, "mytool", "Converts things");
    argh_flag(&p, 'v', "verbose", &verbose, "Verbose output");
    argh_int(&p, 'j', "jobs", &jobs, "Parallel jobs");
    argh_string(&p, 'o', "output", &output, "Output file");
    argh_pos(&p, "input", &input, "Input file");

    if (!argh_parse(&p, argc, argv))
        return argh_exit_code(&p);  /* --help, --version or an error was handled */

    printf("verbose=%d jobs=%d output=%s input=%s\n", verbose, jobs, output, input);
    return 0;
}
