/* A program that uses aargh (argh.h) through a package: CMake, pkg-config or Meson.
 * tests/package/check.sh builds it each way and runs it. */
#include <stdio.h>

#define ARGH_IMPLEMENTATION
#include <argh.h> /* include/aargh is on the include path */

int main(int argc, char **argv)
{
    int jobs = 1;
    argh_parser p;
    argh_init(&p, "app", NULL);
    argh_int(&p, 'j', "jobs", &jobs, "Parallel jobs");
    if (!argh_parse(&p, argc, argv))
        return argh_exit_code(&p);
    printf("argh %s jobs=%d\n", ARGH_VERSION, jobs);
    return 0;
}
