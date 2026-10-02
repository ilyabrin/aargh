/*
 * Fuzz target for argh_parse (libFuzzer).
 *
 *   make fuzz                  build with clang, run for FUZZ_TIME seconds
 *   ./fuzz_argh tests/fuzz     or replay/extend a corpus directory
 *
 * Input: the first byte picks parser flags, the rest is split on '\0' into
 * argv. The configuration uses every feature: builder and table options,
 * all value types, commands with a POSIX command, rules and a validator.
 * After each parse the target checks invariants and aborts if one breaks:
 *   - argv is only reordered, never changed or lost
 *   - strings stored in variables point into argv
 *   - argh_format_error is consistent at every buffer size
 *   - help and error output are well formed text
 */

#define ARGH_IMPLEMENTATION
#include "../argh.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define MAX_ARGS 48

static struct
{
    bool verbose, force, color, dev, env;
    int count, jobs, mode, level;
    long seek;
#ifndef ARGH_NO_FLOAT
    double ratio;
#endif
    const char *output, *name, *url, *pkg;
    const char *include_items[4];
    argh_values include, files, rest;
} v;

static const char *const modes[] = {"fast", "safe", "debug", NULL};

static const char *parse_level(const char *text, void *target)
{
    if (text[0] < '0' || text[0] > '9' || text[1] != '\0')
        return "expected a digit";
    *(int *)target = text[0] - '0';
    return NULL;
}

static bool format_level(const void *target, char *buf, size_t size)
{
    if (size < 2)
        return false;
    buf[0] = (char)('0' + *(const int *)target % 10);
    buf[1] = '\0';
    return true;
}

static const argh_type level_type = {"<level>", parse_level, format_level};

static const argh_opt table[] = {
    ARGH_GROUP("Output"),
    ARGH_STRING('o', "output", &v.output, "Output file", 0, "<file>"),
    ARGH_LIST('I', "include", &v.include, "Include dir"),
    ARGH_CUSTOM('L', "level", &v.level, &level_type, "Level", ARGH_ONCE),
    ARGH_FLAG(0, "color", &v.color, "Color", ARGH_NEGATABLE),
    ARGH_END,
};

#ifndef ARGH_NO_COMMANDS
static const argh_opt add_opts[] = {
    ARGH_FLAG('f', "force", &v.force, "Force"),
    ARGH_POS("name", &v.name, "Name"),
    ARGH_POS("url", &v.url, "URL", ARGH_OPTIONAL),
    ARGH_END,
};

static const argh_opt install_opts[] = {
    ARGH_FLAG('D', "dev", &v.dev, "Dev"),
    ARGH_STRING(0, "version", &v.pkg, "Package version"),
    ARGH_REST("packages", &v.rest, "Packages"),
    ARGH_END,
};

static const argh_opt exec_opts[] = {
    ARGH_FLAG('e', "env", &v.env, "Env"),
    ARGH_REST("command", &v.rest, "Command", ARGH_REQUIRED),
    ARGH_END,
};

static int run_cmd(argh_parser *p, void *user)
{
    (void)p;
    (void)user;
    return 0;
}

static const argh_cmd remote_cmds[] = {
    ARGH_CMD("add", "Add", add_opts, run_cmd),
    ARGH_CMD("remove", "Remove", NULL),
    ARGH_CMD_END,
};

static const argh_cmd commands[] = {
    ARGH_CMD("install", "Install", install_opts, run_cmd),
    ARGH_CMD("exec", "Exec", exec_opts, run_cmd, ARGH_POSIX),
    ARGH_CMD_GROUP("remote", "Remotes", remote_cmds),
    ARGH_CMD_END,
};
#endif

static const argh_rule rules[] = {
    ARGH_AT_MOST_ONE(&v.force, &v.dev),
    ARGH_REQUIRES(&v.pkg, &v.dev),
    ARGH_RULES_END,
};

static bool validate(argh_parser *p, void *ctx)
{
    (void)ctx;
    return v.jobs >= 0 || argh_fail(p, "jobs must not be negative");
}

/* Output sink: checks that every write is plain text */
static size_t out_bytes;

static void sink(void *ctx, bool to_stderr, const char *text, size_t len)
{
    (void)ctx;
    (void)to_stderr;
    if (len && (!text || memchr(text, '\0', len)))
        abort();
    out_bytes += len;
}

static char *const *argv_copy;
static int argc_copy;

static void check_points_into_argv(const char *s)
{
    int i;
    if (!s)
        return;
    for (i = 0; i < argc_copy; i++)
        if (s >= argv_copy[i] && s <= argv_copy[i] + strlen(argv_copy[i]))
            return;
    abort();
}

static int cmp_ptr(const void *a, const void *b)
{
    uintptr_t x = (uintptr_t)*(char *const *)a, y = (uintptr_t)*(char *const *)b;
    return x < y ? -1 : x > y;
}

static void check_values(const argh_values *vals)
{
    int i;
    if (vals->count < 0 || (vals->capacity && vals->count > vals->capacity))
        abort();
    for (i = 0; i < vals->count; i++)
        check_points_into_argv(vals->items[i]);
}

static void check_format_error(const argh_parser *p)
{
    char big[1024], small[8];
    size_t n = argh_format_error(p, big, sizeof(big));
    size_t k;

    if (n < sizeof(big) && strlen(big) != n)
        abort();
    if (argh_format_error(p, NULL, 0) != n)
        abort();
    k = argh_format_error(p, small, sizeof(small));
    if (k != n || strlen(small) != (n < sizeof(small) ? n : sizeof(small) - 1))
        abort();
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    char buf[4096];
    char *argv[MAX_ARGS + 1];
    char *before[MAX_ARGS + 1];
    char *sorted_before[MAX_ARGS + 1];
    char *after[MAX_ARGS + 1];
    int argc = 1;
    unsigned flags;
    size_t pos;
    argh_parser p;

    if (size < 1 || size >= sizeof(buf) - 8)
        return 0;
    flags = data[0];

    /* argv[0], then NUL-separated arguments */
    memcpy(buf, "fz", 3);
    memcpy(buf + 3, data + 1, size - 1);
    buf[size + 2] = '\0';
    argv[0] = buf;
    for (pos = 3; pos < size + 2 && argc < MAX_ARGS;)
    {
        argv[argc++] = buf + pos;
        pos += strlen(buf + pos) + 1;
    }
    argv[argc] = NULL;
    memcpy(before, argv, sizeof(argv[0]) * (size_t)argc);

    memset(&v, 0, sizeof(v));
    v.jobs = 4;
    v.output = "out.txt";
    v.include.items = v.include_items;
    v.include.capacity = 4;
    out_bytes = 0;

    argh_init(&p, "fz", "Fuzz target");
    argh_set_writer(&p, sink, NULL);
    if (flags & 1)
        argh_version(&p, "1.0");
    argh_set_flags(&p, (flags >> 1) & (ARGH_POSIX | ARGH_NO_AUTO_HELP));
    argh_count(&p, 'v', "verbose", &v.count, "Verbose");
    argh_once(argh_int(&p, 'j', "jobs", &v.jobs, "Jobs"));
    argh_long(&p, 's', "seek", &v.seek, "Seek");
#ifndef ARGH_NO_FLOAT
    argh_double(&p, 'r', "ratio", &v.ratio, "Ratio");
#endif
    argh_enum(&p, 'm', "mode", &v.mode, modes, "Mode");
    argh_hidden(argh_flag(&p, 0, "secret", &v.verbose, "Hidden"));
    argh_table(&p, table);
#ifndef ARGH_NO_COMMANDS
    if (flags & 0x10)
        argh_commands(&p, commands);
    else
#endif
        argh_rest(&p, "files", &v.files, "Files");
    if (flags & 0x20)
        argh_rules(&p, rules);
    if (flags & 0x40)
        argh_set_validator(&p, validate, NULL);

    argv_copy = before;
    argc_copy = argc;

    if (argh_parse(&p, argc, argv))
    {
        if (argh_exit_code(&p) != 0 || argh_last_error(&p)->code != ARGH_E_NONE)
            abort();
#ifndef ARGH_NO_COMMANDS
        if (flags & 0x10)
            argh_run(&p, NULL);
#endif
    }
    else
    {
        int code = argh_exit_code(&p);
        if (code != 0 && code != 2)
            abort();
        if (code == 2 && argh_last_error(&p)->code == ARGH_E_NONE)
            abort();
        if (argh_last_error(&p)->suggestion && !argh_last_error(&p)->suggestion[0])
            abort();
        check_format_error(&p);
    }

    /* argv is a permutation of what it was, argv[0] and the terminator kept */
    if (argv[0] != before[0] || argv[argc] != NULL)
        abort();
    memcpy(sorted_before, before, sizeof(argv[0]) * (size_t)argc);
    memcpy(after, argv, sizeof(argv[0]) * (size_t)argc);
    qsort(sorted_before, (size_t)argc, sizeof(sorted_before[0]), cmp_ptr);
    qsort(after, (size_t)argc, sizeof(after[0]), cmp_ptr);
    if (memcmp(sorted_before, after, sizeof(after[0]) * (size_t)argc) != 0)
        abort();

    check_points_into_argv(v.name);
    check_points_into_argv(v.url);
    check_points_into_argv(v.pkg);
    if (v.output && strcmp(v.output, "out.txt") != 0)
        check_points_into_argv(v.output);
    check_values(&v.include);
    check_values(&v.files);
    check_values(&v.rest);
    if (v.level < 0 || v.level > 9 || v.mode < 0 || v.mode > 2)
        abort();

    argh_print_help(&p);
    if (out_bytes == 0)
        abort();
    return 0;
}

#ifdef FUZZ_REPLAY
/* Without libFuzzer: run each file given on the command line once */
#include <stdio.h>

int main(int argc, char **argv)
{
    static uint8_t data[4096];
    int i;
    for (i = 1; i < argc; i++)
    {
        FILE *f = fopen(argv[i], "rb");
        size_t n;
        if (!f)
            return 1;
        n = fread(data, 1, sizeof(data), f);
        fclose(f);
        LLVMFuzzerTestOneInput(data, n);
    }
    printf("replayed %d inputs\n", argc - 1);
    return 0;
}
#endif
