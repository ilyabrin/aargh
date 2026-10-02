# argh.h

[![CI](https://github.com/ilyabrin/argh/actions/workflows/ci.yml/badge.svg)](https://github.com/ilyabrin/argh/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

A single-header command-line argument parser for C. Your options write straight into your variables.

<!-- docs-check: source=tests/docs/mytool.c -->
```c
int jobs = 4;
argh_int(&p, 'j', "jobs", &jobs, "Parallel jobs");
```

> **Stable since v1.0.** The API follows [Semantic Versioning](https://semver.org/): no breaking changes before v2.0. Tested in CI on every pull request.

## Why argh

- **One file, no dependencies.** Copy `argh.h` into your project. Plain C99 that also compiles as C++.
- **No lookups after parsing.** Options are bound to variables, so a typo in an option name can't fail silently at runtime, and the compiler warns when a variable has the wrong type.
- **Zero heap allocations, no global state.** Strings point into `argv`. Option tables can be `static const`, so they live in read-only memory (flash on microcontrollers).
- **Help and errors included.** `--help`, `--version`, clear error messages and the right exit codes, without writing any of it.
- **Strict by design.** Ambiguous input is an error, never a guess. No octal surprises, no prefix matching, no `-o=file`.
- **Fits on a microcontroller.** Without stdio and floating point, argh adds about 11 KB of flash on a Cortex-M0, checked in CI. See [Microcontrollers](#microcontrollers).
- **Fuzzed and sanitized.** Every pull request runs the tests with ASan and UBSan and fuzzes the parser.
- **On par with `getopt_long` in speed**, while validating every value. See [Benchmarks](#benchmarks).

## Quick start

Copy `argh.h` next to your code:

<!-- docs-check: source=tests/docs/mytool.c -->
```c
#include <stdio.h>

#define ARGH_IMPLEMENTATION
#include "argh.h"

int main(int argc, char **argv)
{
    bool verbose = false;
    int jobs = 4;
    const char *output = "out.txt";
    const char *input = NULL;

    argh_parser p;
    argh_init(&p, "mytool", "Converts things");
    argh_flag(&p, 'v', "verbose", &verbose, "Verbose output");
    argh_int(&p, 'j', "jobs", &jobs, "Parallel jobs");
    argh_string(&p, 'o', "output", &output, "Output file");
    argh_pos(&p, "input", &input, "Input file");

    if (!argh_parse(&p, argc, argv))
        return argh_exit_code(&p);  /* --help, --version or an error was handled */

    printf("verbose=%d jobs=%d output=%s input=%s\n", verbose, jobs, output, input);
    return 0;
}
```

That's the whole setup. Defaults are the values you initialize your variables with, and there is nothing to free.

<!-- docs-check: program=mytool -->
```console
$ ./mytool -v --jobs=8 data.csv
verbose=1 jobs=8 output=out.txt input=data.csv

$ ./mytool --help
Usage: mytool [OPTIONS] <input>

Converts things

Arguments:
  <input>               Input file

Options:
  -v, --verbose         Verbose output
  -j, --jobs <n>        Parallel jobs (default: 4)
  -o, --output <value>  Output file (default: out.txt)

  -h, --help            Print help

$ ./mytool --jobs many data.csv
mytool: invalid value 'many' for '--jobs': expected an integer
Try 'mytool --help' for more information.
$ echo $?
2
```

`#define ARGH_IMPLEMENTATION` goes in exactly one `.c` file. Other files just `#include "argh.h"`.

For a program in one file, or a library that ships its own copy of argh.h, `#define ARGH_STATIC` instead: the implementation is included and every function is `static`, so two copies in one program never clash.

## Guide

### Option types

| Builder       | Table macro   | Variable       | Command line                             |
| ------------- | ------------- | -------------- | ---------------------------------------- |
| `argh_flag`   | `ARGH_FLAG`   | `bool`         | `-v`, `--verbose`, `--verbose=false`     |
| `argh_count`  | `ARGH_COUNT`  | `int`          | `-vvv` adds 3                            |
| `argh_int`    | `ARGH_INT`    | `int`          | `-j4`, `-j 4`, `--jobs=4`, `--jobs 0x10` |
| `argh_long`   | `ARGH_LONG`   | `long`         | same as `int`                            |
| `argh_double` | `ARGH_DOUBLE` | `double`       | `--ratio 0.5`, `--ratio=1e-3`            |
| `argh_string` | `ARGH_STRING` | `const char *` | `-o file`, `-ofile`, `--output=file`     |
| `argh_enum`   | `ARGH_ENUM`   | `int` (index)  | `--mode fast`                            |
| `argh_list`   | `ARGH_LIST`   | `argh_values`  | `-I a -I b`                              |
| `argh_pos`    | `ARGH_POS`    | `const char *` | one positional argument                  |
| `argh_rest`   | `ARGH_REST`   | `argh_values`  | all remaining positional arguments       |
| `argh_custom` | `ARGH_CUSTOM` | anything       | your parser, see below                   |

Pass `0` as the short name for a long-only option, and `NULL` as the long name for a short-only one.

### Choices, lists and the rest of the arguments

<!-- docs-check: source=tests/docs/guide.c -->
```c
static const char *const formats[] = {"json", "yaml", "toml", NULL};
int format = 0;                           /* index into formats: "json" */
argh_enum(&p, 'f', "format", &format, formats, "Output format");

const char *dirs[16];
argh_values includes = ARGH_VALUES(dirs); /* up to 16 values */
argh_list(&p, 'I', "include", &includes, "Include directory");

argh_values files = {0};
argh_rest(&p, "files", &files, "Files to process");

/* after argh_parse(): */
for (int i = 0; i < files.count; i++)
    puts(files.items[i]);
```

### Your own value types

Sizes like `10M`, durations like `30s`, `host:port` pairs: describe the type once with a parse function, then use it for any number of options.

<!-- docs-check: source=tests/docs/sizetool.c -->
```c
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

unsigned long long max_size = 64ull << 20;
argh_custom(&p, 's', "max-size", &max_size, &size_type, "Largest file to keep");
/* or in a table: ARGH_CUSTOM('s', "max-size", &max_size, &size_type, "Largest file to keep") */
```

<!-- docs-check: program=sizetool -->
```console
$ ./tool --help
Usage: tool [OPTIONS]

Options:
  -s, --max-size <size>  Largest file to keep (default: 64M)

  -h, --help             Print help

$ ./tool --max-size 10Q
tool: invalid value '10Q' for '--max-size': expected a size like 512K or 10M
Try 'tool --help' for more information.
```

The target can be anything, including a struct. `argh_type` is a constant, so it lives in read-only memory and can be shared between programs. The format function is optional: without it, help shows no default.

### Required, hidden, negatable

Builder calls return the option, and modifiers can be chained:

<!-- docs-check: source=tests/docs/guide.c -->
```c
argh_required(argh_string(&p, 'o', "output", &output, "Output file"));
argh_negatable(argh_flag(&p, 0, "color", &color, "Colored output")); /* --color / --no-color */
argh_hidden(argh_flag(&p, 0, "debug-dump", &dump, "Not shown in help"));
argh_once(argh_string(&p, 'c', "config", &config, "Giving it twice is an error"));
argh_optional(argh_pos(&p, "output", &out_path, "Positional that may be omitted"));
```

Did the user actually pass an option, or is it the default? Ask by variable:

<!-- docs-check: source=tests/docs/guide.c -->
```c
if (argh_given(&p, &jobs))
    printf("jobs set explicitly\n");
```

### Rules between options

Some options only make sense together, or not at all together. Say so in a table that refers to your variables, and argh checks it and explains the problem:

<!-- docs-check: source=tests/docs/export.c -->
```c
static const argh_rule rules[] = {
    ARGH_AT_MOST_ONE(&json, &yaml, &csv),    /* one output format */
    ARGH_EXACTLY_ONE(&input, &use_stdin),    /* one input source */
    ARGH_REQUIRES(&tls_key, &tls_cert),      /* a key needs its certificate */
    ARGH_RULES_END
};

argh_rules(&p, rules);
```

<!-- docs-check: program=export -->
```console
$ ./export --json --csv --stdin
export: options '--json' and '--csv' cannot be used together
Try 'export --help' for more information.

$ ./export --yaml
export: one of '--input' or '--stdin' is required
Try 'export --help' for more information.

$ ./export --stdin --tls-key k.pem
export: option '--tls-key' requires '--tls-cert'
Try 'export --help' for more information.
```

| Rule                              | Meaning                                  |
| --------------------------------- | ---------------------------------------- |
| `ARGH_AT_MOST_ONE(&a, &b, ...)`   | no two of them together                  |
| `ARGH_EXACTLY_ONE(&a, &b, ...)`   | one of them, and only one                |
| `ARGH_AT_LEAST_ONE(&a, &b, ...)`  | one of them or more                      |
| `ARGH_REQUIRES(&a, &b, ...)`      | if `a` is given, all the others must be  |

A rule takes 2 to 4 variables. Because rules refer to variables rather than names, a typo is a compile error. With commands, a rule only applies when all its options are active, so rules for different commands can share one table.

For anything else, add a validator. It runs after every other check has passed:

<!-- docs-check: source=tests/docs/export.c -->
```c
static bool check_sizes(argh_parser *p, void *ctx)
{
    if (min_size > max_size)
        return argh_fail(p, "--min-size must not be greater than --max-size");
    return true;
}

argh_set_validator(&p, check_sizes, NULL);
```

<!-- docs-check: program=export -->
```console
$ ./export --stdin --min-size 50 --max-size 10
export: --min-size must not be greater than --max-size
Try 'export --help' for more information.
```

### Option tables

For larger tools, or to keep the definitions in read-only memory, describe options as data. It's the same parser underneath.

<!-- docs-check: source=tests/docs/guide.c -->
```c
static struct { bool verbose; int jobs; const char *host; int port; } cfg = {false, 4, "localhost", 5432};

static const argh_opt options[] = {
    ARGH_FLAG('v', "verbose", &cfg.verbose, "Verbose output"),
    ARGH_INT('j', "jobs", &cfg.jobs, "Parallel jobs"),
    ARGH_GROUP("Database"),
    ARGH_STRING(0, "db-host", &cfg.host, "Database host", ARGH_REQUIRED),
    ARGH_INT(0, "db-port", &cfg.port, "Database port"),
    ARGH_END
};

argh_parser p;
argh_init(&p, "mytool", "Does useful things");
argh_table(&p, options);
```

- After the help text come two optional arguments: flags such as `ARGH_REQUIRED | ARGH_ONCE`, then the value name for help. The value name needs flags before it: `ARGH_STRING(0, "tls-key", &key, "Client key", 0, "<file>")`.
- The compiler warns if a variable has the wrong type, for example `ARGH_INT` on a `bool`. In C++ it's an error.
- `argh_table` can be called several times, so each module of a program can define its own options. Tables and builder calls can be mixed.
- `ARGH_GROUP` starts a new section in the help output.

### Commands

Tools like `git` or `docker` group their work into commands: `tool build`, `tool remote add`. Describe them as a table, each with its own options:

<!-- docs-check: source=tests/docs/tool.c -->
```c
static bool verbose, release, force;
static const char *name, *url;

static int build(argh_parser *p, void *app)
{
    printf("building%s\n", release ? " (release)" : "");
    return 0;
}

static const argh_opt build_opts[] = {
    ARGH_FLAG('r', "release", &release, "Optimized build"),
    ARGH_END
};

static const argh_opt add_opts[] = {
    ARGH_FLAG('f', "force", &force, "Overwrite an existing remote"),
    ARGH_POS("name", &name, "Remote name"),
    ARGH_POS("url", &url, "Remote URL"),
    ARGH_END
};

static const argh_cmd remote_cmds[] = {
    ARGH_CMD("add", "Add a remote", add_opts),
    ARGH_CMD("list", "List remotes", NULL),
    ARGH_CMD_END
};

static const argh_cmd commands[] = {
    ARGH_CMD("build", "Build the project", build_opts, build),
    ARGH_CMD_GROUP("remote", "Manage remotes", remote_cmds),
    ARGH_CMD_END
};

int main(int argc, char **argv)
{
    argh_parser p;
    argh_init(&p, "tool", "Builds things");
    argh_flag(&p, 'v', "verbose", &verbose, "Verbose output");   /* a global option */
    argh_commands(&p, commands);

    if (!argh_parse(&p, argc, argv))
        return argh_exit_code(&p);

    if (argh_command(&p) == &remote_cmds[0])
        printf("adding %s -> %s\n", name, url);
    return argh_run(&p, NULL);   /* calls the handler of the selected command, if any */
}
```

- `ARGH_CMD(name, help, options[, handler[, flags]])`: the handler is optional. Pass `NULL` for a command without options.
- `ARGH_POSIX` as the flags makes a pass-through command: options end at its first positional, so `tool exec node --version` hands `--version` to `node` without a `--`. Options for the command itself, and global ones, go before the program name. Write `ARGH_CMD("exec", "Run a program", exec_opts, NULL, ARGH_POSIX)` when there is no handler.
- `ARGH_CMD_GROUP(name, help, subcommands)`: a command that only holds other commands, like `remote`.
- Options added to the parser itself are **global**: they work before and after the command name (`tool -v build` and `tool build -v`). A command's own options only work after its name.
- Dispatch either way: `argh_run(&p, app)` calls the handler and passes `app` through, or `argh_command(&p)` returns the selected command for your own `switch`.

Every level gets its own help, and `tool help remote add` works like `tool remote add --help`:

<!-- docs-check: program=tool -->
```console
$ ./tool remote add --help
Usage: tool remote add [OPTIONS] <name> <url>

Add a remote

Arguments:
  <name>         Remote name
  <url>          Remote URL

Options:
  -f, --force    Overwrite an existing remote

Global options:
  -v, --verbose  Verbose output

  -h, --help     Print help
```

When a command is missing, the error lists what's available:

<!-- docs-check: program=tool -->
```console
$ ./tool remote
tool: 'remote' needs a command

Commands:
  add   Add a remote
  list  List remotes

Try 'tool remote --help' for more information.
```

Command names match exactly, like options. A program with commands can't have positional arguments of its own, and neither can a command group: positionals belong to the commands that do the work.

### Help and version

`-h`/`--help` works out of the box, and so does `tool help <command>` in a program with commands. `-V`/`--version` works once you set a version:

<!-- docs-check: source=tests/docs/guide.c -->
```c
argh_version(&p, "1.4.2");   /* ./mytool --version  ->  mytool 1.4.2 */
```

Help shows the defaults from your variables before parsing, so they are always accurate. `argh_parse` returns `false` after printing help, and `argh_exit_code` returns 0.

Long descriptions wrap at 80 columns, lined up under the description column, and a default stays whole on one line:

<!-- docs-check: skip -->
```console
  -r, --retries <n>        How many times to retry a failed upload before giving
                           up on that file and moving on to the next one
                           (default: 3)
```

A `\n` in a description starts a new line at the same column. Set the width with `ARGH_HELP_WIDTH`, or turn wrapping off with `0`, which also leaves its code out (about 250 bytes on a microcontroller). Below 60 columns, lines next to long option names can pass the width. Usage examples are never wrapped, so they can be copied as they are.

Need `-h` for something else, like `--host`? Turn the built-ins off with `argh_set_flags(&p, ARGH_NO_AUTO_HELP)` and call `argh_print_help(&p)` yourself.

### Examples in help

Show how the tool is used, and never let those examples go stale:

<!-- docs-check: source=tests/docs/convert.c -->
```c
argh_init(&p, "convert", "Converts data files");
argh_int(&p, 'j', "jobs", &jobs, "Parallel jobs");
argh_pos(&p, "input", &input, "Input file");
argh_example(&p, "convert -j 8 data.csv", "Convert with 8 parallel jobs");
/* or in a table: ARGH_EXAMPLE("convert -j 8 data.csv", "Convert with 8 parallel jobs") */
```

<!-- docs-check: program=convert -->
```console
$ ./convert --help
Usage: convert [OPTIONS] <input>

Converts data files

Arguments:
  <input>         Input file

Options:
  -j, --jobs <n>  Parallel jobs (default: 4)

  -h, --help      Print help

Examples:
  convert -j 8 data.csv
      Convert with 8 parallel jobs
```

**argh checks every example.** In builds without `NDEBUG`, `argh_parse` first parses each example exactly like a real command line, against your current options, commands and rules, and writes nothing to your variables. If one doesn't work, because an option was renamed, a value is wrong, a required option or argument is missing, or a rule is broken, the first run says so, with the usual suggestion:

<!-- docs-check: program=convert_broken -->
```console
$ ./convert data.csv
convert: example 'convert --jbos 8 data.csv' does not work: unknown option '--jbos' (did you mean '--jobs'?)
```

`argh_parse` then returns `false` with `ARGH_E_CONFIG`, like any other mistake in the definitions, so a test suite or CI run catches it. Release builds with `-DNDEBUG` skip the check.

- Write the program name first, then the arguments, separated by spaces. `'...'` and `"..."` keep spaces inside one argument. Up to 256 characters and 32 words. No pipes or shell variables: an example is one command line.
- An example in a command's table is shown in that command's help (`tool remote add --help`), the program's own examples in the program's help. All of them are checked.
- Not checked: values of [custom types](#your-own-value-types) and whether a list overflows, because both need to write to your variables; and the [validator](#rules-between-options), which reads them.
- Examples are entries in the option tables, so they count toward `ARGH_MAX_OPTS` and, when added with `argh_example`, toward `ARGH_BUILDER_CAP`.

### Errors

On an error, `argh_parse` prints a one-line message plus a hint to stderr and returns `false`. `argh_exit_code` then returns 2, the Unix convention for usage errors.

Typos in long options and command names get a suggestion:

<!-- docs-check: program=tool -->
```console
$ ./tool --verbsoe build
tool: unknown option '--verbsoe' (did you mean '--verbose'?)
Try 'tool --help' for more information.

$ ./tool remote ad origin https://example.com/app.git
tool: unknown command 'ad' (did you mean 'add'?)
Try 'tool remote --help' for more information.
```

Suggestions only name options and commands that are valid at that point, never hidden options, and only when the match is close: at most 2 edits, and no more than a third of the longer name (a swap of two letters counts as one edit). They are computed only after an error, so they cost nothing on a successful parse. `ARGH_NO_SUGGEST` removes them.

To handle errors yourself:

<!-- docs-check: source=tests/docs/guide.c -->
```c
const argh_error *err = argh_last_error(&p);    /* err->code: ARGH_E_UNKNOWN_OPTION, ... */

char message[200];
argh_format_error(&p, message, sizeof message); /* same text, no stdio needed */
```

To send all output somewhere else (a log, a UART, a test buffer), set a writer:

<!-- docs-check: source=tests/docs/guide.c -->
```c
static void my_writer(void *ctx, bool to_stderr, const char *text, size_t len)
{
    /* write len bytes of text */
}

argh_set_writer(&p, my_writer, NULL);
```

### Microcontrollers

Define two macros and give argh a writer:

<!-- docs-check: source=tests/docs/mcu.c -->
```c
#define ARGH_NO_STDIO   /* no <stdio.h>, no printf family */
#define ARGH_NO_FLOAT   /* no argh_double, so no strtod */
#define ARGH_IMPLEMENTATION
#include "argh.h"

static void uart_write(void *ctx, bool to_stderr, const char *text, size_t len)
{
    while (len--)
        uart_putc(*text++);
}

argh_set_writer(&p, uart_write, NULL);
```

- **`ARGH_NO_STDIO`** keeps stdio out of your firmware. Output goes only to your writer; without one it is discarded. Help, errors and defaults in help work the same.
- **`ARGH_NO_FLOAT`** matters more than it looks: the C library's `strtod` pulls in a large float parser, and on newlib also printf, about 27 KB on a Cortex-M0. Without it argh adds about 11 KB of flash, or 9.4 to 9.8 KB with `ARGH_NO_COMMANDS`, `ARGH_NO_SUGGEST` and `ARGH_HELP_WIDTH=0` (see [BENCHMARKS.md](BENCHMARKS.md#microcontrollers)). If you need fractions, a [custom type](#your-own-value-types) that parses fixed-point values costs far less.
- **RAM:** the parser lives on the stack or wherever you put it. On a 32-bit MCU it is 136 bytes plus 28 bytes for each of the `ARGH_BUILDER_CAP` + 1 builder slots: 1,060 bytes by default. Set `ARGH_BUILDER_CAP` to what you use, or to 0 with `static const` tables, which stay in flash: then it is 164 bytes.

With `ARGH_NO_STDIO` alone, doubles in help are shown with up to 6 decimals, and very large or very small ones are left out.

### Parsing rules

| Input                 | Meaning                                                                     |
| --------------------- | --------------------------------------------------------------------------- |
| `-abc`                | `-a -b -c` when all are flags                                               |
| `-vo file`, `-vofile` | flags, then an option with a value                                          |
| `-o -x`               | an option that needs a value always takes the next argument (`-n -5` works) |
| `--`                  | everything after it is positional. Never taken as a value                   |
| `-`                   | a positional argument (stdin by convention)                                 |
| `tool a -v b`         | options and positionals can be mixed                                        |

argh is strict where other parsers guess:

- `-o=file` is an error: write `-o file` or `--output=file`.
- `--verb` does not match `--verbose`. Prefix matching breaks scripts when new options are added.
- `010` is ten, not eight. Hex needs `0x`.
- Numbers are checked completely: `10abc`, `" 5"` and out-of-range values are errors.
- A negative number on its own (`-5`) is an unknown option. Pass it after `--`.

`argh_parse` reorders `argv` in place so that positional arguments come first, in their original order. That's what lets `argh_rest` point into `argv` without copying. With `argh_set_flags(&p, ARGH_POSIX)`, parsing stops at the first positional argument, which suits wrapper tools like `sudo` or `time`. For one command only, see [Commands](#commands).

### Configuration

Define before including `argh.h`, the same way in every file that includes it. The simplest way is a compiler flag such as `-DARGH_BUILDER_CAP=8`, or one header of your own that sets them and includes `argh.h`:

| Macro              | Default | Meaning                                                                        |
| ------------------ | ------: | ------------------------------------------------------------------------------ |
| `ARGH_BUILDER_CAP` |      32 | Options that builder calls can add. `0` if you only use tables                 |
| `ARGH_MAX_OPTS`    |      64 | Options on the active command path, all tables combined                        |
| `ARGH_MAX_TABLES`  |       8 | Tables per parser. The builder counts as one                                   |
| `ARGH_HELP_WIDTH`  |      80 | Column where help text wraps. `0` turns wrapping off                           |
| `ARGH_MAX_DEPTH`   |       4 | Levels of nested commands                                                      |
| `ARGH_NO_SUGGEST`  |         | Define to remove "did you mean" suggestions (about 0.8 KB)                     |
| `ARGH_NO_COMMANDS` |         | Define to remove commands (about 2.3 KB) if you don't use them                 |
| `ARGH_NO_STDIO`    |         | Define to build without `<stdio.h>`, see [Microcontrollers](#microcontrollers) |
| `ARGH_NO_FLOAT`    |         | Define to remove `argh_double` and floating point (27 KB on newlib firmware)   |
| `ARGH_STATIC`      |         | Define to include the implementation with every function `static`              |
| `NDEBUG`           |         | The usual release flag: skips the slower checks of your definitions            |

Sizes are for Linux GCC. The four size settings and `ARGH_NO_COMMANDS` change the size of `argh_parser`, so files built with different values would corrupt memory. argh.h catches that at build time: they fail to link, with a name like `argh_init_settings_b8_o64_t8_d4_cmd` in the error. Define the size settings as plain numbers.

Mistakes in the definitions, such as two options with the same name or a missing variable, are reported by `argh_parse` as `ARGH_E_CONFIG`. The checks for duplicate names, for the command tree and for variables in rules run only in builds without `NDEBUG`.

## API reference

Everything public in `argh.h`. Names marked *commands* are missing with `ARGH_NO_COMMANDS`, and *float* ones with `ARGH_NO_FLOAT`.

### Functions

<!-- docs-check: skip -->
```c
/* Setup */
void argh_init(argh_parser *p, const char *name, const char *about);  /* name NULL: from argv[0] */
void argh_version(argh_parser *p, const char *version);                /* enables -V/--version */
void argh_set_flags(argh_parser *p, unsigned flags);                   /* replaces them: ARGH_POSIX | ARGH_NO_AUTO_HELP */
void argh_set_writer(argh_parser *p, argh_write_fn write, void *ctx);  /* NULL: back to the default */
void argh_table(argh_parser *p, const argh_opt *table);                /* up to ARGH_MAX_TABLES */
void argh_commands(argh_parser *p, const argh_cmd *commands);          /* commands */
void argh_rules(argh_parser *p, const argh_rule *rules);
void argh_set_validator(argh_parser *p, argh_validate_fn fn, void *ctx);
bool argh_fail(argh_parser *p, const char *message);                   /* inside a validator */

/* Builder: each returns the option, or NULL when ARGH_BUILDER_CAP is exceeded */
argh_opt *argh_flag  (argh_parser *p, char s, const char *l, bool *target, const char *help);
argh_opt *argh_count (argh_parser *p, char s, const char *l, int *target, const char *help);
argh_opt *argh_int   (argh_parser *p, char s, const char *l, int *target, const char *help);
argh_opt *argh_long  (argh_parser *p, char s, const char *l, long *target, const char *help);
argh_opt *argh_double(argh_parser *p, char s, const char *l, double *target, const char *help);  /* float */
argh_opt *argh_string(argh_parser *p, char s, const char *l, const char **target, const char *help);
argh_opt *argh_enum  (argh_parser *p, char s, const char *l, int *target, const char *const *choices, const char *help);
argh_opt *argh_list  (argh_parser *p, char s, const char *l, argh_values *target, const char *help);
argh_opt *argh_pos   (argh_parser *p, const char *name, const char **target, const char *help);
argh_opt *argh_rest  (argh_parser *p, const char *name, argh_values *target, const char *help);
argh_opt *argh_custom(argh_parser *p, char s, const char *l, void *target, const argh_type *type, const char *help);
argh_opt *argh_group (argh_parser *p, const char *title);
argh_opt *argh_example(argh_parser *p, const char *command, const char *help);  /* checked without NDEBUG */

/* Modifiers: accept NULL, return their argument */
argh_opt *argh_required(argh_opt *o);
argh_opt *argh_optional(argh_opt *o);
argh_opt *argh_hidden(argh_opt *o);
argh_opt *argh_negatable(argh_opt *o);
argh_opt *argh_once(argh_opt *o);
argh_opt *argh_metavar(argh_opt *o, const char *metavar);   /* "<file>" instead of "<value>" */

/* Parsing and results */
bool argh_parse(argh_parser *p, int argc, char **argv);       /* false: help, version or error shown */
int argh_exit_code(const argh_parser *p);                    /* 0, or 2 after an error */
bool argh_given(const argh_parser *p, const void *target);
const argh_cmd *argh_command(const argh_parser *p);         /* selected command, or NULL; commands */
int argh_run(argh_parser *p, void *user);                     /* calls its handler, or returns 0; commands */
const argh_error *argh_last_error(const argh_parser *p);
size_t argh_format_error(const argh_parser *p, char *buf, size_t size);  /* returns the full length */
void argh_print_help(const argh_parser *p);
```

### Macros

<!-- docs-check: skip -->
```c
/* Options, in a table ending with ARGH_END. Trailing arguments: [flags[, metavar]] */
ARGH_FLAG(s, l, &bool_var, help, ...)        ARGH_STRING(s, l, &str_var, help, ...)
ARGH_COUNT(s, l, &int_var, help, ...)        ARGH_ENUM(s, l, &int_var, choices, help, ...)
ARGH_INT(s, l, &int_var, help, ...)          ARGH_LIST(s, l, &values_var, help, ...)
ARGH_LONG(s, l, &long_var, help, ...)        ARGH_POS(name, &str_var, help, ...)
ARGH_DOUBLE(s, l, &double_var, help, ...)    ARGH_REST(name, &values_var, help, ...)
ARGH_CUSTOM(s, l, &any_var, &type, help, ...)
ARGH_GROUP(title)                            ARGH_END
ARGH_EXAMPLE(command, help)                  /* a usage example, shown in help */

/* Commands, in a table ending with ARGH_CMD_END (commands) */
ARGH_CMD(name, help, options[, handler[, flags]])   /* flags: ARGH_POSIX */
ARGH_CMD_GROUP(name, help, subcommands)
ARGH_CMD_END

/* Rules, 2 to 4 variables each, in a table ending with ARGH_RULES_END */
ARGH_AT_MOST_ONE(&a, &b, ...)     ARGH_EXACTLY_ONE(&a, &b, ...)
ARGH_AT_LEAST_ONE(&a, &b, ...)    ARGH_REQUIRES(&a, &b, ...)
ARGH_RULES_END

/* A list backed by a fixed array */
const char *buf[8];
argh_values list = ARGH_VALUES(buf);

/* The version of argh.h, for compile-time checks */
ARGH_VERSION_MAJOR    ARGH_VERSION_MINOR    ARGH_VERSION_PATCH
ARGH_VERSION          /* "1.1.0" */
```

Option flags, combined with `|`: `ARGH_REQUIRED`, `ARGH_OPTIONAL` (positionals), `ARGH_HIDDEN`, `ARGH_NEGATABLE` (flags), `ARGH_ONCE`. Parser flags: `ARGH_POSIX`, `ARGH_NO_AUTO_HELP`.

### Types

<!-- docs-check: skip -->
```c
typedef struct argh_values { const char **items; int count; int capacity; } argh_values;

typedef struct argh_type {
    const char *metavar;                                   /* "<size>", NULL for "<value>" */
    const char *(*parse)(const char *text, void *target);  /* NULL, or the reason it failed */
    bool (*format)(const void *target, char *buf, size_t size);  /* optional, for defaults */
} argh_type;

typedef struct argh_error {
    argh_err code;
    int argv_index;          /* argv position of the problem, -1 if none */
    const argh_opt *opt;     /* option involved, if any */
    const char *value;       /* offending text, if any */
    const char *detail;      /* ARGH_E_CONFIG, custom types, the message of argh_fail() */
    const char *suggestion;  /* closest valid name, without dashes, or NULL */
    const argh_rule *rule;   /* the rule that failed */
    char short_name;         /* offending short option, if any */
} argh_error;

typedef void (*argh_write_fn)(void *ctx, bool to_stderr, const char *text, size_t len);
typedef bool (*argh_validate_fn)(argh_parser *p, void *ctx);
```

`argh_parser`, `argh_opt`, `argh_cmd` and `argh_rule` are plain structs: create them with the functions and macros above, and treat their fields as internal. The flags come from `enum argh_opt_flag` and `enum argh_parser_flag`. `ARGH_RULE_MAX` (4) is the most variables one rule can take.

### Error codes

The values are stable: new codes are only ever added at the end, so it is safe to store or log them.

| Code                         | Example                                                  |
| ---------------------------- | -------------------------------------------------------- |
| `ARGH_E_NONE`                | no error                                                 |
| `ARGH_E_UNKNOWN_OPTION`      | `--verbos`                                               |
| `ARGH_E_MISSING_VALUE`       | `--jobs` at the end of the line                          |
| `ARGH_E_INVALID_VALUE`       | `--jobs abc`, `--mode slow`, a custom type's error       |
| `ARGH_E_OUT_OF_RANGE`        | `--jobs 99999999999`                                     |
| `ARGH_E_UNEXPECTED_VALUE`    | `--count=3` on a counter                                 |
| `ARGH_E_SHORT_EQUALS`        | `-o=file`                                                |
| `ARGH_E_UNEXPECTED_ARGUMENT` | a positional argument nobody asked for                   |
| `ARGH_E_MISSING_REQUIRED`    | a required option or positional is absent                |
| `ARGH_E_REPEATED`            | an `ARGH_ONCE` option given twice                        |
| `ARGH_E_TOO_MANY_VALUES`     | a list is full                                           |
| `ARGH_E_CONFIG`              | a mistake in the definitions                             |
| `ARGH_E_UNKNOWN_COMMAND`     | `tool remtoe`                                            |
| `ARGH_E_MISSING_COMMAND`     | `tool remote` when `remote` needs a command              |
| `ARGH_E_CONFLICT`            | `--json --yaml` with `ARGH_AT_MOST_ONE`                  |
| `ARGH_E_ONE_REQUIRED`        | none of an `ARGH_EXACTLY_ONE` or `ARGH_AT_LEAST_ONE` set |
| `ARGH_E_REQUIRES`            | `--tls-key` without `--tls-cert`                         |
| `ARGH_E_CUSTOM`              | `argh_fail()` from a validator                           |

## Upgrading from 0.4

v1.0 freezes the API. Most programs compile unchanged; check these:

- **Writers take `bool to_stderr`** instead of `int`: change the parameter type of your `argh_write_fn`.
- **Every file must include argh.h with the same settings** (`ARGH_BUILDER_CAP`, `ARGH_MAX_OPTS`, `ARGH_MAX_TABLES`, `ARGH_MAX_DEPTH`, `ARGH_NO_COMMANDS`). If they differ, the program now fails to link with a name like `argh_init_settings_b8_o64_t8_d4_cmd`; before, it could corrupt memory. Define the settings once, for example with `-D` flags.
- **`ARGH_K_*` and `ARGH_R_*` are internal now** (`ARGH__K_*`, `ARGH__R_*`). Only code that built `argh_opt` or `argh_rule` entries by hand used them; use the macros instead.

## Upgrading from 0.1

v0.2 replaces the API. The idea stays the same, but values now go straight into variables:

<!-- docs-check: skip -->
```c
/* v0.1 */
argh_Parser parser;
argh_init(&parser, argc, argv);
argh_add(&parser, "n", "count", ARGH_INT, "10", "Iterations");
if (!argh_parse(&parser)) { argh_print_error(&parser); return 1; }
int count = argh_get_int(&parser, "count");
argh_free(&parser);

/* v0.2 */
int count = 10;
argh_parser p;
argh_init(&p, NULL, NULL);
argh_int(&p, 'n', "count", &count, "Iterations");
if (!argh_parse(&p, argc, argv)) return argh_exit_code(&p);
```

| v0.1                               | v0.2                                   |
| ---------------------------------- | -------------------------------------- |
| `argh_Parser`                      | `argh_parser`                          |
| `argh_add(..., ARGH_BOOL, ...)`    | `argh_flag(...)`                       |
| `ARGH_FLOAT`                       | `argh_double` (no separate float type) |
| default value as a string          | initialize the variable                |
| `argh_get_*(&p, "name")`           | read the variable                      |
| `argh_has(&p, "name")`             | `argh_given(&p, &variable)`            |
| `argh_require(&p, "name")`         | `argh_required(argh_...(...))`         |
| `parser.positional[i]`             | `argh_pos` / `argh_rest`               |
| define and check your own `--help` | built in                               |
| `argh_free`                        | nothing to free                        |

## Examples

Three complete programs in [examples/](examples), each a real kind of tool:

- **[wc](examples/wc.c)**: counts lines, words and bytes like the Unix tool. The basics: 8 lines of argh code.
- **[logship](examples/logship.c)**: sends log files to a collector. An option table with groups, sizes and durations as custom types, rules and a validator.
- **[pkg](examples/pkg)**: a package manager front end in the style of `cargo`. Nested commands spread over several files, global options, handlers with an application context.

`make examples` builds them, `make smoke` runs them and checks their output.

## Known limitations

- **Help and error text cannot be removed.** On a microcontroller argh adds about 9.4 to 11.7 KB of flash, strings included (see [Microcontrollers](#microcontrollers)).
- Floating-point values follow the C locale's decimal separator, like `strtod`.

## Benchmarks

Time to set up a parser with 30 options and parse 17 arguments, release builds (`-O2 -DNDEBUG`), v1.0 on CI. Each row comes from one machine; compare within a row:

| Platform           | argh (table) | argh (builder) | getopt_long |
| ------------------ | -----------: | -------------: | ----------: |
| macOS, Clang       |       508 ns |         557 ns |      635 ns |
| Linux, Clang       |       551 ns |         628 ns |      628 ns |
| Linux, GCC         |       605 ns |         706 ns |      622 ns |
| Windows, MinGW GCC |       953 ns |       1,009 ns |      894 ns |

argh makes zero heap allocations. Details, memory, code size and the method: [BENCHMARKS.md](BENCHMARKS.md). Run them with `make bench`.

How argh compares with getopt_long, cargs and argparse in features, strictness, speed and size: [COMPARISON.md](COMPARISON.md).

## Running the tests

```sh
make test       # test suite, a build without stdio, the saved fuzz inputs
make smoke      # run the examples and check their output
make cxx        # check that argh.h compiles as C++
make fuzz       # fuzz the parser with libFuzzer (needs clang), 60 s by default
make size-arm   # flash added to ARM firmware, checked against budgets
make docs-check # README examples and llms.txt match the code (Python 3)
```

CI runs all of these on every pull request: on Linux (x86-64 and ARM64), macOS (ARM64) and Windows, with GCC, Clang, MinGW and MSVC; as 32-bit x86; and under qemu on 32-bit ARM and on big-endian s390x and PowerPC. On top of that come AddressSanitizer and UndefinedBehaviorSanitizer, 2 minutes of fuzzing, and the firmware size check. The fuzz target ([tests/fuzz_argh.c](tests/fuzz_argh.c)) feeds random command lines to a parser that uses every feature, and checks that `argv` is only reordered, that stored strings point into `argv`, and that error messages are consistent.

## Contributing

Curious how it works inside, and why it is built this way? See [ARCHITECTURE.md](ARCHITECTURE.md).

Using a coding agent? Point it at [llms.txt](llms.txt): a compact guide to setting up and using argh.h correctly.

Bug reports, tests and fixes are welcome. See [CONTRIBUTING.md](CONTRIBUTING.md) for how to build, test and send a pull request, and [CHANGELOG.md](CHANGELOG.md) for what changed between versions.

## License

[MIT](LICENSE)
