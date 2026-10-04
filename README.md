<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="assets/logo-dark.svg">
    <img src="assets/logo-light.svg" alt="aargh" width="332" height="120">
  </picture>
</p>

# aargh: argh.h

[![CI](https://github.com/ilyabrin/aargh/actions/workflows/ci.yml/badge.svg)](https://github.com/ilyabrin/aargh/actions/workflows/ci.yml)
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

*The name:* "Aargh!" is what the snail in the comic says when salt hits it, and what parsing `argv` by hand in C feels like.

## Install

The library is one header, `argh.h`; its packages are called **aargh**. Pick the way your build already uses:

| Build | Add aargh with |
| ----- | -------------- |
| Anything | copy [argh.h](argh.h) into your project |
| CMake | `FetchContent`, `add_subdirectory` or `find_package`, then link `aargh::aargh` |
| Meson | the repository in `subprojects/aargh`, then `dependency('aargh')` |
| Conan 2 | `conan create .` in this repository, then require `aargh/1.11.0` |
| clib | `clib install ilyabrin/aargh` |
| Make and others | `cc $(pkg-config --cflags aargh) ...` after installing |

**CMake**, fetched at configure time (nothing to install):

```cmake
include(FetchContent)
FetchContent_Declare(aargh GIT_REPOSITORY https://github.com/ilyabrin/aargh GIT_TAG v1.11.0)
FetchContent_MakeAvailable(aargh)

target_link_libraries(app PRIVATE aargh::aargh)
```

With a copy or a git submodule in `third_party/aargh`, use `add_subdirectory(third_party/aargh)` instead of the first three lines. After installing, `find_package(aargh 1.11 REQUIRED)`: it accepts any later 1.x, never 2.0.

**Install** the header, the CMake package and `aargh.pc` (for pkg-config):

```sh
cmake -S . -B build
cmake --install build --prefix /usr/local
```

The header goes to `include/aargh/argh.h`, and the package puts `include/aargh` on your include path, so your code keeps `#include "argh.h"` and can't pick up another library's `argh.h`.

**Meson**: put the repository in `subprojects/aargh`, then

```meson
aargh_dep = dependency('aargh', fallback: ['aargh', 'aargh_dep'])
executable('app', 'app.c', dependencies: aargh_dep)
```

Whichever way you choose, define `ARGH_IMPLEMENTATION` in exactly one source file before including `argh.h`, as in the quick start below. As a dependency aargh builds nothing and installs nothing; its tests run only when you build the repository itself (`ARGH_BUILD_TESTS`, `ARGH_INSTALL` to change that).

The v1.8 names `find_package(argh)`, `argh::argh` and Meson's `dependency('argh')` still work until 2.0; `ARGH_INSTALL_LEGACY_NAME=OFF` installs without them.

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
| `argh_uint`   | `ARGH_UINT`   | `unsigned`     | same as `int`, a minus sign is an error  |
| `argh_size`   | `ARGH_SIZE`   | `size_t`       | same as `unsigned`                       |
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

### Optional values

Some options work alone and also take a value, like `--color` and `--color=never`. Put `argh_implicit` right after the option, with the value that the option alone stands for:

<!-- docs-check: source=tests/docs/colortool.c -->
```c
argh_metavar(argh_enum(&p, 0, "color", &color, when, "Colorize: never, auto or always"), "<when>");
argh_implicit(&p, &color, "always");   /* --color alone means --color=always */
/* or in a table, right after the option: ARGH_IMPLICIT(&color, "always") */
```

<!-- docs-check: program=colortool -->
```console
$ ./tool --help
Usage: tool [OPTIONS] [files...]

Arguments:
  [files...]            Files to show

Options:
      --color[=<when>]  Colorize: never, auto or always (default: auto)

  -h, --help            Print help

$ ./tool --color
color=always files=0

$ ./tool --color=never
color=never files=0

$ ./tool --color a.txt
color=always files=1

$ ./tool
color=auto files=0
```

- **A value needs `=`.** In `--color a.txt`, `a.txt` is a file, not the value: otherwise adding a value to an option would change what the arguments after it mean. A short name (`-c`) works only alone, like a flag, and can be combined: `-cv`.
- The option needs a long name, so that a value can still be given. Without `NDEBUG` argh checks this, that the entry comes right after its option, and that the option accepts the value.
- `--color` alone counts as given for `argh_given`, required options and rules. A value from the environment is an ordinary value.

### Ranges

Limit an integer option to the values your program can use. Put `argh_range` right after the option; both bounds are included:

<!-- docs-check: source=tests/docs/rangetool.c -->
```c
argh_int(&p, 'j', "jobs", &jobs, "Parallel jobs");
argh_range(&p, &jobs, 1, 64);      /* or in a table, right after the option: ARGH_RANGE(&jobs, 1, 64) */
argh_uint(&p, 'l', "level", &level, "Compression level");
argh_range(&p, &level, 1, 9);
```

<!-- docs-check: program=rangetool -->
```console
$ ./tool --help
Usage: tool [OPTIONS]

Options:
  -j, --jobs <1..64>  Parallel jobs (default: 4)
  -l, --level <1..9>  Compression level (default: 3)

  -h, --help          Print help

$ ./tool -j 0
tool: value '0' for '-j' is out of range (1 to 64)
Try 'tool --help' for more information.
```

- Works with `int`, `long`, `unsigned` and `size_t` options. The bounds are `long`.
- The range applies wherever the value comes from: the command line, the environment, an [optional value](#optional-values). The default you initialized the variable with is not checked, so `0` can still mean "automatic".
- Help shows the range as the value name, unless you set one with `argh_metavar`.
- With `ARGH_IMPLICIT`, the two entries can come in either order after the option. Without `NDEBUG` argh checks that the range follows an integer option and fits its type.

### Shell completion

One call gives your program `--completions <shell>`, which prints a completion script for bash, zsh or fish:

<!-- docs-check: source=tests/docs/completetool.c -->
```c
argh_enum(&p, 'f', "format", &format, formats, "Output format");
argh_string(&p, 'o', "output", &output, "Output file");
argh_completions(&p);   /* adds --completions <shell> */
```

<!-- docs-check: program=completetool -->
```console
$ ./tool --help
Usage: tool [OPTIONS]

Options:
  -f, --format <text|json>   Output format (default: text)
  -o, --output <value>       Output file
      --completions <shell>  Print a completion script for bash, zsh or fish

  -h, --help                 Print help

$ ./tool --completions tcsh
tool: invalid value 'tcsh' for '--completions': expected one of: bash, zsh, fish
Try 'tool --help' for more information.
```

Your users load the script once:

```sh
source <(tool --completions bash)          # in ~/.bashrc
source <(tool --completions zsh)           # in ~/.zshrc
tool --completions fish > ~/.config/fish/completions/tool.fish
```

- Tab then completes options (with `--no-` for negatable flags), commands and subcommands at every level, the choices of an enum, and file names for string options and positional arguments. fish also shows each option's help text. Hidden options stay hidden.
- `--completions` works like `--help`: it prints and exits with 0 even when required options are missing.
- The scripts are generated from your tables, so they never fall out of date with the program, and Tab never runs your program. Regenerate them when you install a new version.
- For your own scheme, such as a `completion` command or an install step, call `argh_print_completion(&p, "bash")`; it returns false for a shell it doesn't know.
- Pay for what you use: the generator is linked only when you call one of the two. Its text adds about 0.6 KB to desktop programs either way, which `ARGH_NO_COMPLETION` removes. Firmware (`ARGH_NO_STDIO`) has no shell, so there it is always left out.

### Environment variables

Let an option fall back to an environment variable, the way tools take settings in containers and CI:

<!-- docs-check: source=tests/docs/envtool.c -->
```c
argh_int(&p, 'j', "jobs", &jobs, "Parallel jobs");
argh_env(&p, &jobs, "TOOL_JOBS");
argh_required(argh_string(&p, 0, "token", &token, "API token"));
argh_env(&p, &token, "TOOL_TOKEN");      /* required, but the environment can give it */
/* or in a table: ARGH_ENV(&jobs, "TOOL_JOBS") */
```

<!-- docs-check: program=envtool -->
```console
$ ./tool --help
Usage: tool [OPTIONS]

Options:
  -j, --jobs <n>       Parallel jobs [env: TOOL_JOBS] (default: 4)
      --token <value>  API token [env: TOOL_TOKEN] (required)

  -h, --help           Print help

$ TOOL_TOKEN=abc TOOL_JOBS=8 ./tool
jobs=8 token=abc

$ TOOL_TOKEN=abc TOOL_JOBS=8 ./tool -j 2
jobs=2 token=abc

$ TOOL_JOBS=lots ./tool --token abc
tool: invalid value 'lots' in TOOL_JOBS for '--jobs': expected an integer
Try 'tool --help' for more information.

$ ./tool
tool: missing required option '--token' (or set TOOL_TOKEN)
Try 'tool --help' for more information.
```

- The command line wins, then the environment, then the default you initialized the variable with.
- A value from the environment is checked like one on the command line. It counts as given: it satisfies a required option, `argh_given` reports it, and rules see it.
- Flags take `1`/`0`, `true`/`false`, `yes`/`no`, `on`/`off`; a counter takes a number; a list takes one value.
- Like rules, an environment entry refers to the variable, so a typo is a compile error. In a command's table it counts only when that command runs.
- `ARGH_GETENV(name)` reads the variable, `getenv` by default. Define it before the implementation to read settings from somewhere else, or to fake the environment in tests. With `ARGH_NO_STDIO` (firmware) there is no environment unless you define it: the entries are accepted and do nothing, and help doesn't mention them.
- Usage examples are command lines only: the environment does not take part in [checking them](#examples-in-help).

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

### Threads

argh keeps no global or static state that changes: everything a parse touches is in your `argh_parser` and your variables. So threads can parse at the same time with no locks, as long as they don't share what is written:

- **One parser per thread.** `argh_parse` writes to the parser, so two threads must not use the same one at once.
- **Your variables are written.** A `static const` table is only read, but it holds the addresses of your variables; two threads parsing with the same table write the same variables. Give each thread its own variables (builder calls, or a table per thread).
- **The C library's rules still apply:** with `ARGH_ENV`, don't change the environment (`setenv`) in another thread during a parse, and don't call `setlocale` meanwhile. A writer you set with `argh_set_writer` is called from the parsing thread.

CI runs 8 threads parsing, printing help and generating completion scripts at once under ThreadSanitizer (`make thread-check`).

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

| Macro               | Default | Meaning                                                                        |
| ------------------- | ------: | ------------------------------------------------------------------------------ |
| `ARGH_BUILDER_CAP`  |      32 | Options that builder calls can add. `0` if you only use tables                 |
| `ARGH_MAX_OPTS`     |      64 | Options on the active command path, all tables combined                        |
| `ARGH_MAX_TABLES`   |       8 | Tables per parser. The builder counts as one                                   |
| `ARGH_HELP_WIDTH`   |      80 | Column where help text wraps. `0` turns wrapping off                           |
| `ARGH_MAX_DEPTH`    |       4 | Levels of nested commands                                                      |
| `ARGH_NO_SUGGEST`   |         | Define to remove "did you mean" suggestions (about 0.8 KB)                     |
| `ARGH_NO_COMMANDS`  |         | Define to remove commands (about 2.3 KB) if you don't use them                 |
| `ARGH_NO_COMPLETION` |        | Define to remove shell completion scripts (about 0.6 KB of text)               |
| `ARGH_NO_STDIO`     |         | Define to build without `<stdio.h>`, see [Microcontrollers](#microcontrollers) |
| `ARGH_NO_FLOAT`     |         | Define to remove `argh_double` and floating point (27 KB on newlib firmware)   |
| `ARGH_STATIC`       |         | Define to include the implementation with every function `static`              |
| `ARGH_GETENV(name)` |         | How `ARGH_ENV` reads a variable: `getenv`, or none with `ARGH_NO_STDIO`        |
| `NDEBUG`            |         | The usual release flag: skips the slower checks of your definitions            |

Sizes are for Linux GCC. The four size settings and `ARGH_NO_COMMANDS` change the size of `argh_parser`, so files built with different values would corrupt memory. argh.h catches that at build time: they fail to link, with a name like `argh_init_settings_b8_o64_t8_d4_cmd` in the error. Define the size settings as plain numbers.

Mistakes in the definitions, such as two options with the same name or a missing variable, are reported by `argh_parse` as `ARGH_E_CONFIG`. The checks for duplicate names, for the command tree and for variables in rules run only in builds without `NDEBUG`.

## API reference

Everything public in `argh.h`. Names marked *commands* are missing with `ARGH_NO_COMMANDS`, and *float* ones with `ARGH_NO_FLOAT`.

### Functions

argh is C99. With C23 or C++17, ignoring the result of `argh_parse`, `argh_exit_code`, `argh_given`, `argh_command` or `argh_last_error` is a warning (`[[nodiscard]]`); write `(void)` in front when you mean it.

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
argh_opt *argh_uint  (argh_parser *p, char s, const char *l, unsigned *target, const char *help);
argh_opt *argh_size  (argh_parser *p, char s, const char *l, size_t *target, const char *help);
argh_opt *argh_double(argh_parser *p, char s, const char *l, double *target, const char *help);  /* float */
argh_opt *argh_string(argh_parser *p, char s, const char *l, const char **target, const char *help);
argh_opt *argh_enum  (argh_parser *p, char s, const char *l, int *target, const char *const *choices, const char *help);
argh_opt *argh_list  (argh_parser *p, char s, const char *l, argh_values *target, const char *help);
argh_opt *argh_pos   (argh_parser *p, const char *name, const char **target, const char *help);
argh_opt *argh_rest  (argh_parser *p, const char *name, argh_values *target, const char *help);
argh_opt *argh_custom(argh_parser *p, char s, const char *l, void *target, const argh_type *type, const char *help);
argh_opt *argh_group (argh_parser *p, const char *title);
argh_opt *argh_example(argh_parser *p, const char *command, const char *help);  /* checked without NDEBUG */
argh_opt *argh_env    (argh_parser *p, void *target, const char *name);          /* fallback for target's option */
argh_opt *argh_implicit(argh_parser *p, void *target, const char *value);      /* right after it: value optional */
argh_opt *argh_range   (argh_parser *p, void *target, long lo, long hi);       /* right after it: lo..hi only */
argh_opt *argh_completions(argh_parser *p);                                    /* --completions <shell> */

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
bool argh_print_completion(const argh_parser *p, const char *shell);  /* "bash", "zsh", "fish" */
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
ARGH_UINT(s, l, &unsigned_var, help, ...)    ARGH_SIZE(s, l, &size_var, help, ...)
ARGH_CUSTOM(s, l, &any_var, &type, help, ...)
ARGH_GROUP(title)                            ARGH_END
ARGH_EXAMPLE(command, help)                  /* a usage example, shown in help */
ARGH_ENV(&var, name)                         /* environment variable for var's option */
ARGH_IMPLICIT(&var, value)                   /* right after var's option: --name alone means --name=value */
ARGH_RANGE(&var, lo, hi)                     /* right after var's integer option: lo..hi, both included */

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
ARGH_VERSION          /* "1.11.0" */
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

- **Help and error text cannot be removed.** On a microcontroller argh adds about 9.4 to 11.9 KB of flash, strings included (see [Microcontrollers](#microcontrollers)).
- Floating-point values follow the C locale's decimal separator, like `strtod`.

## Benchmarks

Time to set up a parser with 30 options and parse 17 arguments, release builds (`-O2 -DNDEBUG`), v1.7 on CI. Each row comes from one machine; compare within a row:

| Platform           | argh (table) | argh (builder) | getopt_long |
| ------------------ | -----------: | -------------: | ----------: |
| macOS, Clang       |       506 ns |         594 ns |      673 ns |
| Linux, Clang       |       375 ns |         409 ns |      466 ns |
| Linux, GCC         |       575 ns |         649 ns |      625 ns |
| Linux ARM64, Clang |       433 ns |         493 ns |      451 ns |
| Linux ARM64, GCC   |       455 ns |         540 ns |      436 ns |
| Windows, MinGW GCC |       887 ns |         945 ns |      894 ns |

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
make coverage   # lines of argh.h the tests run, at least 98% (gcc, gcov)
make package-check # CMake, pkg-config, Conan and Meson builds of a program that uses argh
make completion-check # the completion scripts in bash, zsh and fish
make c23        # the tests as C23, and [[nodiscard]] at work
make thread-check # 8 threads parsing at once, under ThreadSanitizer
make mutation   # do the tests notice small changes to argh.h? (Mull, clang)
```

With CMake instead of make, on any platform: `cmake -S . -B build && cmake --build build && ctest --test-dir build`.

CI runs all of these on every pull request: on Linux (x86-64 and ARM64), macOS (ARM64) and Windows, with GCC, Clang, MinGW and MSVC; as 32-bit x86; and under qemu on 32-bit ARM and on big-endian s390x and PowerPC. On top of that come AddressSanitizer, UndefinedBehaviorSanitizer and ThreadSanitizer, mutation testing, 2 minutes of fuzzing, 5 more per sanitizer (address, undefined, memory) with [ClusterFuzzLite](https://google.github.io/clusterfuzzlite/) on changes to the parser and an hour each every night, the coverage minimum, every way of installing argh, and the firmware size check. The fuzz target ([tests/fuzz_argh.c](tests/fuzz_argh.c)) feeds random command lines to a parser that uses every feature, and checks that `argv` is only reordered, that stored strings point into `argv`, and that error messages are consistent.

## Contributing

Curious how it works inside, and why it is built this way? See [ARCHITECTURE.md](ARCHITECTURE.md).

Using a coding agent? Point it at [llms.txt](llms.txt): a compact guide to setting up and using argh.h correctly.

Bug reports, tests and fixes are welcome. See [CONTRIBUTING.md](CONTRIBUTING.md) for how to build, test and send a pull request, and [CHANGELOG.md](CHANGELOG.md) for what changed between versions.

## License

[MIT](LICENSE)
