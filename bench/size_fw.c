/*
 * Code-size probe for microcontrollers: a firmware shell command that parses
 * its arguments with argh and writes output to a UART.
 * Built with and without ARGH_PROBE_PARSER to measure what argh adds.
 */
#include <stdbool.h>
#include <stddef.h>

#ifdef ARGH_PROBE_PARSER
#ifndef ARGH_NO_STDIO
#define ARGH_NO_STDIO
#endif
#define ARGH_IMPLEMENTATION
#include "../argh.h"
#endif

/* Stand-in for a UART data register */
static volatile char uart;

static void uart_write(void *ctx, bool to_stderr, const char *text, size_t len)
{
    (void)ctx;
    (void)to_stderr;
    while (len--)
        uart = *text++;
}

int shell_main(int argc, char **argv)
{
    bool verbose = false;
    int count = 10;
    const char *output = "out.txt";

#ifdef ARGH_PROBE_PARSER
    argh_parser p;
    argh_init(&p, "cmd", NULL);
    argh_set_writer(&p, uart_write, NULL);
    argh_flag(&p, 'v', "verbose", &verbose, "Verbose output");
    argh_int(&p, 'n', "count", &count, "Iterations");
    argh_string(&p, 'o', "output", &output, "Output file");
    if (!argh_parse(&p, argc, argv))
        return argh_exit_code(&p);
#else
    (void)argc;
    (void)argv;
#endif

    uart_write(NULL, 0, output, (size_t)count);
    return verbose;
}

int main(void)
{
    static char a0[] = "cmd", a1[] = "-v";
    static char *argv[] = {a0, a1, NULL};
    return shell_main(2, argv);
}
