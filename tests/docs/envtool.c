/* README.md, Environment variables: the code as shown, in a program */
#include <stdio.h>

#define ARGH_IMPLEMENTATION
#include "argh.h"

int main(int argc, char **argv)
{
    int jobs = 4;
    const char *token = NULL;
    argh_parser p;
    argh_init(&p, "tool", NULL);

    argh_int(&p, 'j', "jobs", &jobs, "Parallel jobs");
    argh_env(&p, &jobs, "TOOL_JOBS");
    argh_required(argh_string(&p, 0, "token", &token, "API token"));
    argh_env(&p, &token, "TOOL_TOKEN");      /* required, but the environment can give it */
    /* or in a table: ARGH_ENV(&jobs, "TOOL_JOBS") */

    if (!argh_parse(&p, argc, argv))
        return argh_exit_code(&p);
    printf("jobs=%d token=%s\n", jobs, token);
    return 0;
}
