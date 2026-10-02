/* README.md, Optional values: the code as shown, in a program */
#include <stdio.h>

#define ARGH_IMPLEMENTATION
#include "argh.h"

int main(int argc, char **argv)
{
    static const char *const when[] = {"never", "auto", "always", NULL};
    int color = 1; /* auto */
    argh_values files = {0};
    argh_parser p;
    argh_init(&p, "tool", NULL);

    argh_metavar(argh_enum(&p, 0, "color", &color, when, "Colorize: never, auto or always"), "<when>");
    argh_implicit(&p, &color, "always");   /* --color alone means --color=always */
    /* or in a table, right after the option: ARGH_IMPLICIT(&color, "always") */
    argh_rest(&p, "files", &files, "Files to show");

    if (!argh_parse(&p, argc, argv))
        return argh_exit_code(&p);
    printf("color=%s files=%d\n", when[color], files.count);
    return 0;
}
