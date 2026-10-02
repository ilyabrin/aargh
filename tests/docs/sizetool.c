/* README.md, Your own value types: the code as shown, in a program */
#include <stdio.h>
#include <stdlib.h>

#define ARGH_IMPLEMENTATION
#include "argh.h"

/* Returns NULL on success, or a short reason that ends up in the error message */
static const char *parse_size(const char *text, void *target)
{
    char *end;
    unsigned long long value = strtoull(text, &end, 10);
    if (end == text)
        return "expected a size like 512K or 10M";
    if (*end == 'K') { value <<= 10; end++; }
    else if (*end == 'M') { value <<= 20; end++; }
    if (*end)
        return "expected a size like 512K or 10M";
    *(unsigned long long *)target = value;
    return NULL;
}

/* Optional: lets help show the default */
static bool format_size(const void *target, char *buf, size_t size)
{
    snprintf(buf, size, "%lluM", *(const unsigned long long *)target >> 20);
    return true;
}

static const argh_type size_type = {"<size>", parse_size, format_size};

int main(int argc, char **argv)
{
    argh_parser p;
    argh_init(&p, "tool", NULL);
    unsigned long long max_size = 64ull << 20;
    argh_custom(&p, 's', "max-size", &max_size, &size_type, "Largest file to keep");
    /* or in a table: ARGH_CUSTOM('s', "max-size", &max_size, &size_type, "Largest file to keep") */
    return argh_parse(&p, argc, argv) ? 0 : argh_exit_code(&p);
}
