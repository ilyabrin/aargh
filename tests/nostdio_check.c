/*
 * Builds argh.h with ARGH_NO_STDIO and without <stdio.h>, the way firmware
 * would use it: output goes through a writer into a buffer.
 * Any use of printf-family functions left in argh.h fails this build.
 */

#ifndef ARGH_NO_STDIO
#define ARGH_NO_STDIO
#endif
#define ARGH_IMPLEMENTATION
#include "../argh.h"

#include <string.h>

#ifdef EOF
#error "<stdio.h> was included"
#endif

static char out[2048];
static size_t out_len;

static void to_buffer(void *ctx, bool to_stderr, const char *text, size_t len)
{
    (void)ctx;
    (void)to_stderr;
    if (out_len + len < sizeof(out))
    {
        memcpy(out + out_len, text, len);
        out_len += len;
        out[out_len] = '\0';
    }
}

static int check(bool ok, int code)
{
    return ok ? 0 : code;
}

int main(void)
{
    char a0[] = "fw", a1[] = "--rate", a2[] = "115200", a3[] = "--help";
    char *argv[] = {a0, a1, a2, a3, NULL};
    long rate = 9600;
#ifndef ARGH_NO_FLOAT
    double gain = 0.25;
#endif
    int fails = 0;

    argh_parser p;
    argh_init(&p, "fw", "Firmware shell");
    argh_long(&p, 'r', "rate", &rate, "Baud rate");
#ifndef ARGH_NO_FLOAT
    argh_double(&p, 'g', "gain", &gain, "Input gain");
#endif

    /* The default writer discards output: nothing to check but no crash */
    fails += check(!argh_parse(&p, 4, argv) && argh_exit_code(&p) == 0, 1);

    argh_init(&p, "fw", "Firmware shell");
    argh_set_writer(&p, to_buffer, NULL);
    argh_long(&p, 'r', "rate", &rate, "Baud rate");
#ifndef ARGH_NO_FLOAT
    argh_double(&p, 'g', "gain", &gain, "Input gain");
#endif
    fails += check(!argh_parse(&p, 4, argv), 2);
    fails += check(strstr(out, "Baud rate (default: 9600)") != NULL, 4);
#ifndef ARGH_NO_FLOAT
    fails += check(strstr(out, "Input gain (default: 0.25)") != NULL, 8);
#endif

    out_len = 0;
    out[0] = '\0';
    argh_init(&p, "fw", "Firmware shell");
    argh_set_writer(&p, to_buffer, NULL);
    argh_long(&p, 'r', "rate", &rate, "Baud rate");
    fails += check(argh_parse(&p, 3, argv) && rate == 115200, 16);

    return fails;
}
