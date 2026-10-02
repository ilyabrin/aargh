/* README.md, Microcontrollers: the code as shown, with a stand-in UART */
static volatile char uart_data;
static void uart_putc(char c) { uart_data = c; }

#define ARGH_NO_STDIO   /* no <stdio.h>, no printf family */
#define ARGH_NO_FLOAT   /* no argh_double, so no strtod */
#define ARGH_IMPLEMENTATION
#include "argh.h"

static void uart_write(void *ctx, bool to_stderr, const char *text, size_t len)
{
    while (len--)
        uart_putc(*text++);
}

int main(int argc, char **argv)
{
    bool verbose = false;
    argh_parser p;
    argh_init(&p, "fw", NULL);
    argh_flag(&p, 'v', "verbose", &verbose, "Verbose output");

    argh_set_writer(&p, uart_write, NULL);
    return argh_parse(&p, argc, argv) ? 0 : argh_exit_code(&p);
}
