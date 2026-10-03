/* README.md, Shell completion: the code as shown, in a program */
#include <stdio.h>

#define ARGH_IMPLEMENTATION
#include "argh.h"

int main(int argc, char **argv)
{
    static const char *const formats[] = {"text", "json", NULL};
    int format = 0;
    const char *output = NULL;
    argh_parser p;
    argh_init(&p, "tool", NULL);

    argh_enum(&p, 'f', "format", &format, formats, "Output format");
    argh_string(&p, 'o', "output", &output, "Output file");
    argh_completions(&p);   /* adds --completions <shell> */

    if (!argh_parse(&p, argc, argv))
        return argh_exit_code(&p);
    printf("format=%s\n", formats[format]);
    return 0;
}
