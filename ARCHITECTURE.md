# How argh.h works

For anyone who wants to understand argh.h from the inside: contributors, reviewers, and people deciding whether to depend on it. It covers how the code is organized, how a parse runs, the techniques that are not obvious from reading the code once, the decisions behind the design and why they were made, and how the project got here.

The [README](README.md) explains how to use argh.h; this document explains why it is built the way it is.

## In one paragraph

argh.h is a single C99 header. You describe options as data (`argh_opt` entries in `static const` tables, or builder calls that fill a table inside the parser), each one bound to a variable of yours. `argh_parse` walks `argv` once, writes converted and checked values straight into those variables, and reorders `argv` so positional arguments end up together. Nothing is allocated, nothing is global, and everything that is not needed can be compiled out or dropped by the linker.

## Principles

Every design question was settled against these, in this order:

| Principle                                   | What it means in the code                                                                                                |
| ------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------ |
| **Wrong input never becomes a wrong value** | Numbers are checked completely, ambiguous syntax is an error, the parser never guesses.                                  |
| **Mistakes show up as early as possible**   | Wrong variable types warn at compile time, mismatched settings fail at link time, bad definitions fail on the first run. |
| **Bind to variables**                       | No lookups by name after parsing, so a typo in a name can't fail silently. Defaults are the variables' initial values.   |
| **Zero allocation, no global state**        | Strings point into `argv`; lists use buffers the caller owns; the parser can live on the stack.                          |
| **Data first**                              | The option table is the source of truth. Builder calls are sugar that fill a table.                                      |
| **Pay for what you use**                    | Commands, suggestions, stdio and floating point can be compiled out; rule checking is dropped by the linker when unused. |
| **Never take control away**                 | The library never calls `exit()` and never prints unless asked; the program decides.                                     |
| **Measured, not claimed**                   | Every number in the docs comes from code in `bench/` that anyone can run.                                                |

## Layout of argh.h

The file has two parts. The first is the public header, read by every file that includes it; the second is the implementation, compiled once under `ARGH_IMPLEMENTATION` (or in every file with `ARGH_STATIC`).

**Public part, in order:**

1. Version macros, `ARGH_STATIC` handling, and the size settings (`ARGH_BUILDER_CAP`, `ARGH_MAX_OPTS`, `ARGH_MAX_TABLES`, `ARGH_MAX_DEPTH`). Right after them, `argh_init` is renamed after those settings (see [Settings checked at link time](#settings-checked-at-link-time)).
2. Types: option kinds (internal), option and parser flags, error codes, `argh_opt`, `argh_values`, `argh_type`, `argh_rule`, `argh_error`, `argh_cmd`, `argh_parser`.
3. Function declarations: setup, builder and modifiers, parsing and results.
4. Table macros (`ARGH_FLAG` ... `ARGH_END`) and command macros (`ARGH_CMD` ...).

**Implementation, in order:**

| Section                  | What is there                                                                                 |
| ------------------------ | --------------------------------------------------------------------------------------------- |
| Iteration and slots      | `ARGH__EACH`, `argh__slot_count`, `argh__slot`, command lookup                                |
| Output helpers           | the default writer, a bounded string builder (`argh__sb_*`), number formatting without printf |
| Option lookup            | `argh__find_long`, `argh__find_short`, the "seen" bitset                                      |
| Value conversion         | `argh__parse_long`, `argh__parse_double`, `argh__parse_bool`, `argh__store`                   |
| The scanner              | `argh__apply`, `argh__move_positional`, `argh__scan`, `tool help <command>`                   |
| Checks after the scan    | positionals, required options, rules, the validator, definition checks                        |
| Suggestions              | edit distance and the search for the closest name                                             |
| Public setup functions   | `argh_init` ... `argh_metavar`                                                                |
| `argh_parse` and results | the pipeline below, `argh_exit_code`, `argh_given`, `argh_run`                                |
| Errors and help          | `argh_format_error`, help layout and `argh_print_help`                                        |

Internal names start with `argh__` / `ARGH__`; everything public starts with `argh_` / `ARGH_`, so argh never collides with your names and you can tell at a glance what is API.

## The data model

**Options are table entries.** An `argh_opt` holds a short and a long name, a kind, flags, a pointer to your variable, an `extra` pointer (enum choices or an `argh_type`), help text and a value name. Tables end with an `ARGH_END` entry (kind 0). On a 32-bit MCU an entry is 28 bytes, and a `static const` table stays in flash.

**The builder is just another table.** `argh_flag(&p, ...)` writes an entry into storage inside the parser (`ARGH_BUILDER_CAP` + 1 entries, the last for the terminator) and, on first use, registers that storage as a table. From then on the parser does not care how an option was defined. Help lists options in the order of `argh_table` and builder calls.

**Slots: the options that are active right now.** The parser keeps up to `ARGH_MAX_TABLES` tables. With commands, each command on the selected path (`tool remote add`) adds its own table. `argh__slot(p, i)` returns slot `i`: first the parser's tables, then one per command on the path. `ARGH__EACH` walks all active options across slots and gives each a stable index, used for the `argh__seen` bitset (`ARGH_MAX_OPTS` bits) that records what was on the command line. That bitset is what `argh_given`, `ARGH_ONCE`, required options and rules read.

**Errors are data.** The first problem is recorded in `argh_error` (code, `argv` index, option, offending text, detail, suggestion, rule). Messages are produced from it only when needed, by `argh_format_error`, so the scanner never formats strings.

## How a parse runs

`argh_parse` is a pipeline; each step runs only if the previous one succeeded:

1. **Reset** the seen bitset and the error; take the program name from `argv[0]` if none was given.
2. **Check the definitions** (`argh__check_config`): too many tables or builder entries, names reserved for `--help`/`--version`, options without a variable, custom types without a parser, enums without choices, positionals mixed with commands. In debug builds also the command tree. These run on every parse; they are written to be cheap.
3. **Dry pass, only if help may be wanted.** If any argument could be `-h`, `--help`, `-V` or `--version` (`argh__may_want_help`, a quick scan), the scanner runs once with `apply = false`: it selects commands and finds out whether help was requested, but writes nothing. This is why `--help` shows the defaults from your variables even when `--jobs 8 --help` is on the command line.
4. **Scan** (`argh__scan` with `apply = true`). For each argument:
   - A positional either names a command (at a level that has subcommands) or is moved to the front of `argv` by `argh__move_positional`. With `ARGH_POSIX`, or after the first positional of a command marked `ARGH_POSIX`, everything after it is positional.
   - `--` makes the rest positional.
   - `--name`, `--name=value`, `--no-name`: looked up with `argh__find_long`; the value comes after `=` or from the next argument.
   - `-abc`, `-ovalue`, `-o value`: a cluster is read letter by letter with `argh__find_short`; the first letter that takes a value takes the rest of the cluster or the next argument.
   - `argh__apply` converts the value into the variable (`argh__store`) and marks the option as seen. A conversion error records which option and value failed.
5. **Check the command path**: options on the path fit `ARGH_MAX_OPTS`, a command group got its subcommand, and (debug) no duplicate names.
6. **Assign positionals**: `ARGH_POS` entries take the moved arguments in order; `ARGH_REST` points into `argv` at the remainder, with no copy.
7. **Environment**: options the command line left out take their `ARGH_ENV` variable, through the same conversion as on the command line, and count as seen. Such an error has no `argv` position, which is how its message knows to name the variable.
8. **Required options**, then **rules**, then your **validator**, in that order, so the validator can rely on everything else being valid.
9. **On an error**, compute a suggestion (only now, so a successful parse never pays for it) and print the message and the hint to stderr. On help or version, print them to stdout. Return `false`; `argh_exit_code` gives 2 for errors and 0 otherwise.

## Techniques worth knowing

### Type-checked tables that stay constant expressions

`ARGH_INT('j', "jobs", &jobs, "...")` has to produce an initializer for a `static const` table, so it can't call a function, yet it should reject `&some_bool`. The macro stores the target as

```c
(void *)(1 ? (ptr) : (int *)0)
```

The conditional operator requires both branches to have compatible pointer types, so a mismatch is a warning in C (`-Wpointer-type-mismatch`, MSVC C4133) and an error in C++, while the whole expression remains a constant.

### Optional trailing macro arguments, also on MSVC

Table macros take `help` and then optionally `flags` and a value name. C99 has no default arguments, so the macros pick arguments by position from `__VA_ARGS__` padded with defaults: `ARGH__FIRST(__VA_ARGS__, ~)`, `ARGH__SECOND(__VA_ARGS__, 0, ~)`, `ARGH__THIRD(...)`. The traditional MSVC preprocessor passes `__VA_ARGS__` on as a single token, so every use goes through one extra `ARGH__EXPAND` pass, which splits it. Tested with both MSVC preprocessors.

### Settings checked at link time

`argh_parser` contains arrays sized by `ARGH_BUILDER_CAP` and friends. If two files include argh.h with different settings, they disagree about the size of the struct, and the implementation writes past the caller's object. That used to be a silent crash. Now `argh_init` is a macro for a function whose name contains the settings, `argh_init_settings_b32_o64_t8_d4_cmd`. A file built with other settings asks the linker for a name that doesn't exist, and the mistake becomes a build error that names the settings. It costs nothing at run time. `make test` and the MSVC CI job check that such a program fails to link.

### Code the linker can drop

`argh_rules` stores a pointer to the rule checker in the parser, and `argh_parse` calls it through that pointer. A program that never calls `argh_rules` never references the checker, so `--gc-sections` removes it. With `ARGH_NO_COMMANDS`, the command state becomes constants (`ARGH__DEPTH(p)` is `0`), so the compiler deletes every branch that deals with commands without a forest of `#ifdef`s.

### Numbers without printf

Help shows defaults (`(default: 4)`) and some errors include numbers. `snprintf` would pull the printf machinery into firmware, so integers are formatted by `argh__fmt_long` (handles `LONG_MIN` by negating in unsigned arithmetic), and with `ARGH_NO_STDIO` doubles by a small formatter with up to 6 decimals.

### Examples that can't go stale

Usage examples are entries in the option tables (`ARGH__K_EXAMPLE`, with the command line in `long_name`), so they need no field in the parser and a command's table can carry its own. In builds without `NDEBUG`, `argh_parse` first splits each example into words in a buffer on its own stack and runs the normal pipeline on them: scan, command path, positionals, required options, rules. The parser status is set to a checking mode in which `argh__store` converts and checks every value but writes nothing, so your variables keep their defaults. If an example fails, the message is printed while the error can still point into that buffer; then the stored error becomes `ARGH_E_CONFIG` pointing at the example entry, so nothing refers to memory that is gone. In release builds the check is not compiled and the writes are unconditional.

### Suggestions

"Did you mean" uses the optimal string alignment distance (Levenshtein plus swapped neighbours as one edit), computed with three rows on the stack. A candidate is accepted if it is at most 2 edits away and the edits are at most a third of the longer name, so `ad` suggests `add` but `x` doesn't suggest `xz`. Candidates are only names valid at that point; hidden options are never suggested.

## Decisions and why

The decisions that shaped the library, with the reason that settled each one.

| Decision                                                               | Why                                                                                                                                                                                        |
| ---------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| Values are written into your variables; no `get` calls                 | A misspelled name in `get_int("jobs")` compiles and fails at run time; a misspelled variable doesn't compile.                                                                              |
| Defaults are the variables' initial values                             | No default strings to parse, and help can always show the real value.                                                                                                                      |
| No heap, ever                                                          | Works on MCUs, can't leak, nothing to free. The benchmark counts allocations to keep it that way.                                                                                          |
| `010` is ten; hex needs `0x`; no octal                                 | Octal surprises are a classic source of bugs in config and scripts.                                                                                                                        |
| `-o=file` is an error                                                  | getopt stores `=file`. Rejecting it with a hint costs the user one second.                                                                                                                 |
| No abbreviations of long options                                       | Prefix matching breaks scripts when a new option with the same prefix is added.                                                                                                            |
| A value option always takes the next argument                          | So `-n -5` and `--pattern -foo` work; the only exception is `--`.                                                                                                                          |
| Usage errors exit with 2                                               | The Unix convention (GNU tools, Python's argparse); scripts can tell misuse from failure.                                                                                                  |
| Help on stdout, errors on stderr                                       | `tool --help                                                                                                                                                                               | less` works, and a failed run never looks like success in a pipeline. |
| A missing command is an error with the list of commands, not full help | Same reason: exit code and output must say "this failed".                                                                                                                                  |
| `-h` and `-V` reserved, with `ARGH_NO_AUTO_HELP` to opt out            | The GNU convention; tools that need `-h` for host can turn it off.                                                                                                                         |
| Rules refer to variables, not names                                    | A typo is a compile error, and matching is a pointer comparison.                                                                                                                           |
| Custom types are a constant `argh_type` struct                         | ISO C can't store a function pointer in `const void *`, and a new field would grow every option. A shared constant costs nothing per option and also lets help show defaults.              |
| Duplicate-name and command-tree checks only without `NDEBUG`           | They compare every pair of options; like `assert`, they catch mistakes during development and cost nothing in release builds.                                                              |
| Settings mismatch caught at link time                                  | Silent memory corruption is the worst failure a C library can have.                                                                                                                        |
| `argh_set_flags` replaces flags; error code values are stable          | Settled at the v1.0 API review so they can be relied on.                                                                                                                                   |
| No hand-written assembly or SIMD                                       | Considered for `strncmp`: the C library's versions are already vectorized, assembly would break portability, and profiling showed the time went elsewhere.                                 |
| `ARGH_NO_FLOAT` exists                                                 | Measuring a real firmware link showed `strtod` pulling in 27 KB, printf included, out of 38 KB.                                                                                            |
| Examples in help are checked by parsing them                           | Documentation that compiles but lies is worse than none; a renamed option should break the build of the docs, not the user's copy-paste.                                                   |
| Examples live in option tables, not in a parser field                  | Keeps the parser state at 248 bytes and gives commands their own examples for free.                                                                                                        |
| An optional value is an entry right after its option                   | No parser or option field grows, and finding it is one look at the next entry, so parsing pays nothing for it. |
| An optional value needs `=`                                            | With `--color never` allowed, adding a value to an option would change what the arguments after it mean. |
| Environment variables are table entries bound to a variable            | Same reasons as examples, plus a typo in the variable is a compile error, like in rules. Firmware has no environment by default, so `getenv` and the code around it stay out of the image. |

The full design notes, including alternatives that were rejected, are kept by the maintainer and summarized here.

## How it is verified

- **Unit tests** (`tests/test_argh.c`, about 120) cover every parsing rule, every error, help layout, commands, rules and custom types. They run in the full build and in the fully reduced one.
- **Builds that must work or must fail:** C++ (`tests/cxx_check.cpp`), no stdio (`tests/nostdio_check.c`), two `ARGH_STATIC` copies in one program, and two files with different settings that must not link.
- **Sanitizers and fuzzing:** every pull request runs the tests under ASan and UBSan and fuzzes `argh_parse` for 2 minutes with libFuzzer. The fuzz target checks invariants: `argv` is only reordered, stored strings point into `argv`, error messages are consistent at every buffer size.
- **Examples as tests:** `make smoke` runs the three example programs and checks their output.
- **Budgets:** CI fails if the firmware build grows past 12 KB (10 KB reduced) on Cortex-M0 or M4.
- **Platforms:** Linux x86-64 and ARM64 (GCC, Clang), macOS ARM64 (Clang), Windows (MinGW, MSVC), 32-bit x86, and under qemu 32-bit ARM and big-endian s390x and PowerPC, so both byte orders and both word sizes run the full test suite. Firmware is built for Cortex-M0 and M4 (arm-none-eabi-gcc).
- **Docs:** `make docs-check`, run on every pull request, compiles the code shown in README.md (the programs live in `tests/docs`), runs every `$ ./...` command in README.md and examples/README.md and compares the output byte for byte, checks that every public name in the header is documented, and compiles the snippets in llms.txt. Every code and console block in README.md must say which program it belongs to, so a block can't silently drop out of the check.

## History

argh.h started as a way to learn C properly: write a small, fast argument parser, and understand every line of it. Along the way the goal grew into making one of the best parsers available for C.

**v0.1 (first public release).** A classic design: options registered by name with default values as strings, values copied to the heap, then read back with `argh_get_int(&p, "count")`. It worked, but every value was looked up by name twice, a typo in a name failed at run time, and there was memory to free. Preparing it for publication meant fixing parsing bugs, adding CI on three systems with sanitizers, a security policy and benchmarks.

**v0.2: the rewrite.** The API was replaced by the one that exists today: options bound to variables, tables as data, a builder on top, zero allocations, strict parsing, built-in help and errors. It was about 2.7 times faster than v0.1 and close to `getopt_long`. A prototype first proved that optional macro arguments and the type check work on every compiler, including both MSVC preprocessors.

**v0.3: tools the size of git.** Nested commands with global options and help per level, "did you mean" suggestions, custom value types, rules between options and validators. The parser state stayed under 256 bytes.

**v0.3.1: real examples.** Three programs of increasing size (`wc`, `logship`, `pkg`) were written against the library, and doing so found a bug: a command's own `--version` option was taken over by the built-in one.

**v0.4: firmware.** `ARGH_NO_STDIO`, then a measurement of a real Cortex-M0 link that showed where the size actually went, which led to `ARGH_NO_FLOAT`. Flash budgets went into CI, along with fuzzing and a POSIX mode for a single command.

**v1.0: stable.** An audit of every document against the code, an API review (the link-time settings check, version macros, `ARGH_STATIC`, internal names made internal), a guide for coding agents ([llms.txt](llms.txt)), a measured comparison with other parsers ([COMPARISON.md](COMPARISON.md)), and profiling that made parsing 10% to 18% faster. With that, the API was frozen.

## Not in scope

Config files, environment variables, localized messages, wide-character `argv` and shell completion are not part of argh.h today. Some may come after 1.0 as optional features (an epilog in help, wrapping long help texts, environment variable fallbacks, optional option values), always under the same rules: no allocation, pay only for what you use, and wrong input never becomes a wrong value.
