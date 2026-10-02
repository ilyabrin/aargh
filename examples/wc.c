/*
 * wc: count lines, words and bytes, like the Unix tool.
 *
 *   $ ./wc data/app.log data/db.log
 *   $ ./wc -l argh.h
 *   $ cat argh.h | ./wc -w
 *
 * Shows the basics: flags, the rest of the arguments as a file list,
 * and --help / --version without writing them.
 */

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define ARGH_IMPLEMENTATION
#include "../argh.h"

typedef struct
{
    long lines, words, bytes;
} counts;

static bool show_lines, show_words, show_bytes;

static bool count_stream(FILE *f, counts *c)
{
    int ch;
    bool in_word = false;
    while ((ch = fgetc(f)) != EOF)
    {
        c->bytes++;
        if (ch == '\n')
            c->lines++;
        if (isspace(ch))
            in_word = false;
        else if (!in_word)
        {
            in_word = true;
            c->words++;
        }
    }
    return !ferror(f);
}

static void print_counts(const counts *c, const char *name)
{
    if (show_lines)
        printf("%8ld", c->lines);
    if (show_words)
        printf("%8ld", c->words);
    if (show_bytes)
        printf("%8ld", c->bytes);
    if (name)
        printf(" %s", name);
    putchar('\n');
}

int main(int argc, char **argv)
{
    argh_values files = {0};
    counts total = {0, 0, 0};
    int status = 0;

    argh_parser p;
    argh_init(&p, "wc", "Count lines, words and bytes");
    argh_version(&p, "1.0.0");
    argh_flag(&p, 'l', "lines", &show_lines, "Count lines");
    argh_flag(&p, 'w', "words", &show_words, "Count words");
    argh_flag(&p, 'c', "bytes", &show_bytes, "Count bytes");
    argh_rest(&p, "files", &files, "Files to read, '-' for standard input");
    /* Shown at the end of --help; debug builds check that they parse */
    argh_example(&p, "wc data/app.log data/db.log", "Lines, words and bytes of two files, and a total");
    argh_example(&p, "wc -l src/main.c src/util.c", "Only count lines");

    if (!argh_parse(&p, argc, argv))
        return argh_exit_code(&p);

    /* Like wc: no choice means all three */
    if (!show_lines && !show_words && !show_bytes)
        show_lines = show_words = show_bytes = true;

    if (files.count == 0)
    {
        counts c = {0, 0, 0};
        count_stream(stdin, &c);
        print_counts(&c, NULL);
        return 0;
    }

    for (int i = 0; i < files.count; i++)
    {
        const char *name = files.items[i];
        FILE *f = strcmp(name, "-") == 0 ? stdin : fopen(name, "rb");
        counts c = {0, 0, 0};

        if (!f)
        {
            fprintf(stderr, "wc: %s: cannot open\n", name);
            status = 1;
            continue;
        }
        if (!count_stream(f, &c))
        {
            fprintf(stderr, "wc: %s: read error\n", name);
            status = 1;
        }
        if (f != stdin)
            fclose(f);

        print_counts(&c, name);
        total.lines += c.lines;
        total.words += c.words;
        total.bytes += c.bytes;
    }
    if (files.count > 1)
        print_counts(&total, "total");
    return status;
}
