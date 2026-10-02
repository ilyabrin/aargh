/* README.md, the smaller code fragments of the guide, verbatim, gathered in
 * one program so that each of them is known to compile. */
#include <stdio.h>

#define ARGH_IMPLEMENTATION
#include "argh.h"

/* Help and errors: a writer */
static void my_writer(void *ctx, bool to_stderr, const char *text, size_t len)
{
    /* write len bytes of text */
}

/* Option tables */
static struct { bool verbose; int jobs; const char *host; int port; } cfg = {false, 4, "localhost", 5432};

static const argh_opt options[] = {
    ARGH_FLAG('v', "verbose", &cfg.verbose, "Verbose output"),
    ARGH_INT('j', "jobs", &cfg.jobs, "Parallel jobs"),
    ARGH_GROUP("Database"),
    ARGH_STRING(0, "db-host", &cfg.host, "Database host", ARGH_REQUIRED),
    ARGH_INT(0, "db-port", &cfg.port, "Database port"),
    ARGH_END
};

static int table_program(int argc, char **argv)
{
    argh_parser p;
    argh_init(&p, "mytool", "Does useful things");
    argh_table(&p, options);
    return argh_parse(&p, argc, argv) ? 0 : argh_exit_code(&p);
}

int main(int argc, char **argv)
{
    const char *output = "out.txt", *config = NULL, *out_path = NULL;
    bool color = true, dump = false;
    int jobs = 4;
    argh_parser p;

    argh_init(&p, "guide", NULL);
    argh_version(&p, "1.4.2");   /* ./mytool --version  ->  mytool 1.4.2 */
    argh_int(&p, 'j', "jobs", &jobs, "Parallel jobs");

    /* Choices, lists and the rest of the arguments */
    static const char *const formats[] = {"json", "yaml", "toml", NULL};
    int format = 0;                           /* index into formats: "json" */
    argh_enum(&p, 'f', "format", &format, formats, "Output format");

    const char *dirs[16];
    argh_values includes = ARGH_VALUES(dirs); /* up to 16 values */
    argh_list(&p, 'I', "include", &includes, "Include directory");

    argh_values files = {0};
    argh_rest(&p, "files", &files, "Files to process");

    /* Required, hidden, negatable */
    argh_required(argh_string(&p, 'o', "output", &output, "Output file"));
    argh_negatable(argh_flag(&p, 0, "color", &color, "Colored output")); /* --color / --no-color */
    argh_hidden(argh_flag(&p, 0, "debug-dump", &dump, "Not shown in help"));
    argh_once(argh_string(&p, 'c', "config", &config, "Giving it twice is an error"));
    argh_optional(argh_pos(&p, "output", &out_path, "Positional that may be omitted"));

    argh_set_writer(&p, my_writer, NULL);

    if (!argh_parse(&p, argc, argv))
        return argh_exit_code(&p);

    /* after argh_parse(): */
    for (int i = 0; i < files.count; i++)
        puts(files.items[i]);

    if (argh_given(&p, &jobs))
        printf("jobs set explicitly\n");

    /* Errors */
    const argh_error *err = argh_last_error(&p);    /* err->code: ARGH_E_UNKNOWN_OPTION, ... */

    char message[200];
    argh_format_error(&p, message, sizeof message); /* same text, no stdio needed */
    (void)err;

    /* The table program on its own: --db-host is required, so without
     * arguments it must fail with exactly that, after checking the table */
    {
        char *no_args[] = {argv[0], NULL};
        return table_program(1, no_args) == 2 ? 0 : 1;
    }
}
