/* Must NOT compile with -std=c2x -Werror: argh_parse's result is dropped,
 * and C23 compilers warn about that ([[nodiscard]]). `make c23` checks that
 * it fails; in C99 it builds without a word. */
#define ARGH_IMPLEMENTATION
#include "../argh.h"

int main(int argc, char **argv)
{
    argh_parser p;
    argh_init(&p, "check", NULL);
    argh_parse(&p, argc, argv);
    return 0;
}
