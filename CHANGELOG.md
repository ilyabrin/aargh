# Changelog

All notable changes to argh.h are listed here.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the project uses [Semantic Versioning](https://semver.org/). Since v1.0, breaking changes only come with a new major version.

## [Unreleased]

### Added

- Help wraps long descriptions at 80 columns, lined up under the description column. A default value always stays whole on one line, and a `\n` in a description starts a new line at the same column. `ARGH_HELP_WIDTH` sets the width; `0` turns wrapping off and leaves its code out. Usage examples are not wrapped.
- The reduced firmware build in the size budget now also sets `ARGH_HELP_WIDTH=0`; wrapping costs about 250 bytes of flash on a Cortex-M.

## [1.1.0] - 2026-10-02

### Added

- CI runs the full test suite on more platforms: Linux ARM64, 32-bit x86, and under qemu 32-bit ARM and big-endian s390x and PowerPC.
- Usage examples in help: `argh_example(&p, "tool -j 8 data.csv", "What it does")` or `ARGH_EXAMPLE(...)` in a table. They are listed at the end of the program's help, or of a command's help when they sit in the command's table.
- Examples are checked: in builds without `NDEBUG`, `argh_parse` parses every example against the current options, commands and rules, without writing to any variable, and fails with `ARGH_E_CONFIG` and a message naming the example if one doesn't work. Release builds skip the check, and the parser does not grow.
- Cost in a release build: about 180 bytes of flash on a Cortex-M0 for the help section, even without examples. The check itself exists only in debug builds.

### Changed

- Option kinds are renumbered so that "is this an option" is one comparison. They are internal since 1.0, so no program is affected.
- CI runs with a read-only token (`permissions: contents: read`), as CodeQL recommends.
- Code size is now measured on release builds (`-DNDEBUG`), what programs ship; debug builds also carry the definition and example checks.

## [1.0.0] - 2026-10-02

The first stable release: the API is frozen and follows Semantic Versioning. See [Upgrading from 0.4](README.md#upgrading-from-04) for the three breaking changes.

### Added

- `ARGH_VERSION_MAJOR`, `ARGH_VERSION_MINOR`, `ARGH_VERSION_PATCH` and `ARGH_VERSION`, to check the version at compile time.
- [ARCHITECTURE.md](ARCHITECTURE.md): how argh.h is organized, how a parse runs, the techniques behind it, the design decisions and the project's history.
- [COMPARISON.md](COMPARISON.md): argh compared with getopt_long, cargs and argparse in features, behavior on tricky input, speed and size, reproducible with `bench/compare/compare.sh`.
- [llms.txt](llms.txt): a guide for coding agents, with the setup, the rules that are easy to get wrong and every feature in short.
- `ARGH_STATIC`: includes the implementation and makes every function `static`, for one-file programs and libraries that embed their own copy.

### Changed

- Parsing is 10% to 18% faster: the checks for names reserved by `--help` and `--version` compare the first letter before calling `strcmp`, and `argh_init` no longer clears the builder storage, which builder calls fill in themselves.

### Changed (breaking)

- `argh_write_fn` takes `bool to_stderr` instead of `int`. Change the parameter type in your writer.
- `ARGH_K_*`, `ARGH_R_*`, `enum argh_kind` and `enum argh_rule_kind` are internal now (`ARGH__K_*`, `ARGH__R_*`). Code that uses the macros and functions is not affected.
- `argh_init` is a macro for a function whose name encodes the size settings (`ARGH_BUILDER_CAP`, `ARGH_MAX_OPTS`, `ARGH_MAX_TABLES`, `ARGH_MAX_DEPTH`, `ARGH_NO_COMMANDS`). Files that include argh.h with different settings now fail to link instead of corrupting memory at run time. Calls stay the same; the settings must be plain numbers.

### Documented

- Error code values are stable: new codes are only added at the end.
- `argh_set_flags` replaces the flags set before.

## [0.4.0] - 2026-09-26

### Added

- POSIX mode for one command: `ARGH_CMD(name, help, options, handler, ARGH_POSIX)`. Options end at that command's first positional, so `tool exec node --version` passes `--version` on without `--`. `argh_cmd` has a new `flags` field; tables built with the `ARGH_CMD*` macros need no changes.
- `ARGH_NO_STDIO` builds argh.h without `<stdio.h>` and the printf family, for firmware. Output goes only to the writer set with `argh_set_writer()`, and is discarded without one.
- `ARGH_NO_FLOAT` removes `argh_double` and `ARGH_DOUBLE`, so `strtod` and floating point are not linked. On newlib this saves about 27 KB of flash.
- Fuzzing: [tests/fuzz_argh.c](tests/fuzz_argh.c) runs for 2 minutes with ASan and UBSan on every pull request (`make fuzz`), and `make test` replays the saved inputs with any compiler.
- Firmware size is measured in CI on Cortex-M0 and Cortex-M4 (`make size-arm`), with budgets: 12 KB for the full build and 10 KB for the reduced one.

### Changed

- Integers in help defaults and error messages are formatted without `snprintf`.
- The `pkg exec` example uses the new per-command POSIX mode: `pkg exec node --version` needs no `--`.

## [0.3.1] - 2026-09-25

### Added

- Three example programs in `examples/`: `wc`, `logship` and a multi-file `pkg` package manager, built and smoke-tested in CI. They replace `example.c`.
- Table macros take an optional value name for help after the flags: `ARGH_STRING(0, "tls-key", &key, "Client key", 0, "<file>")`.

### Fixed

- A command's own `--version` or `-V` option was taken over by the built-in version flag. The command's option now wins, like `cargo install --version`, and help lists only the built-in forms that still apply.
- Debug builds now also report `-h`/`--help` defined in a command's options, not only at the top level.

## [0.3.0] - 2026-09-24

Commands, suggestions, custom types and rules. No breaking changes to the v0.2 API.

### Added

- **Commands**, like `git remote add`: `argh_commands` with `ARGH_CMD` and `ARGH_CMD_GROUP` tables, nested up to `ARGH_MAX_DEPTH` levels. Options of the parser are global and work before and after the command name.
- `argh_command` returns the selected command, `argh_run` calls its handler.
- Help for every command level, with a `Global options` section, and `tool help <command>`.
- A missing command is an error that lists the available commands.
- "Did you mean" suggestions for mistyped long options and commands: `unknown option '--verbsoe' (did you mean '--verbose'?)`. Also available as `argh_error.suggestion`. `ARGH_NO_SUGGEST` removes them.
- **Your own value types**: describe a type once as a constant `argh_type` (value name, parse function, optional format function) and use it with `argh_custom` / `ARGH_CUSTOM`. The parse function's reason appears in the error message, and the format function lets help show the default.
- **Rules between options**, referring to variables: `ARGH_AT_MOST_ONE`, `ARGH_EXACTLY_ONE`, `ARGH_AT_LEAST_ONE`, `ARGH_REQUIRES`, set with `argh_rules`. Errors like `options '--json' and '--csv' cannot be used together`. Programs that don't call `argh_rules` don't link the rule checker.
- **Validators** for anything rules can't express: `argh_set_validator` and `argh_fail(p, "message")`.
- `ARGH_NO_COMMANDS` removes command support, so programs without commands keep the code size of v0.2.

## [0.2.0] - 2026-09-24

v0.2 replaces the API. See [Upgrading from 0.1](README.md#upgrading-from-01).

### Changed

- **Options are bound to variables.** Builder calls (`argh_flag`, `argh_int`, `argh_string`, ...) or `static const` tables (`ARGH_FLAG`, `ARGH_INT`, ...) write parsed values straight into your variables. Defaults are the values the variables start with.
- **Zero heap allocations** and no global state. Strings point into `argv`. `argh_free` is gone.
- `argh_Parser` is now `argh_parser`, and `argh_parse` takes `argc` and `argv`.
- `argv` is reordered in place so that positional arguments come first.
- Stricter input: no octal numbers, no prefix matching of long options, `-o=file` is an error, numbers must be valid as a whole.
- About 2.7x faster than v0.1 and on par with `getopt_long`. See [BENCHMARKS.md](BENCHMARKS.md).

### Added

- Option kinds: counters (`-vvv`), `long`, enums, repeatable lists, named positionals, the rest of the positionals.
- Per-option flags: required, optional, hidden, negatable (`--no-color`), once.
- Built-in `-h`/`--help` and `-V`/`--version`, with defaults shown from your variables.
- Help sections with `ARGH_GROUP`.
- Error messages that name the option and the expected value, exit code 2 for usage errors, structured errors via `argh_last_error` and `argh_format_error`.
- `argh_given` to tell whether an option was passed.
- `argh_set_writer` to send all output elsewhere.
- POSIX mode that stops at the first positional argument.
- Compile-time warning when a table option points to a variable of the wrong type.
- Checks for mistakes in the definitions, such as duplicate names.
- The header compiles as C++, checked in CI.
- Benchmarks for parse speed, heap allocations, parser size and code size, compared with `getopt_long`. Run them with `make bench`, results in [BENCHMARKS.md](BENCHMARKS.md).
- Security policy ([SECURITY.md](SECURITY.md)) with private vulnerability reporting.
- Contribution guide ([CONTRIBUTING.md](CONTRIBUTING.md)) and this changelog.

### Removed

- `argh_add`, `argh_get_*`, `argh_has`, `argh_require`, `argh_free`, `argh_print_error` and the `ARGH_FLOAT` type.

## [0.1.0] - 2026-09-24

First public release.

### Added

- Single-header parser for C99: short (`-v`) and long (`--verbose`) options, `--name=value`, combined flags (`-abc`), `--` to end options.
- Typed values: `bool`, `int`, `float`, `double`, strings, with validation and range checks.
- Required options, positional arguments, generated help text.
- CI on Linux, macOS and Windows with GCC, Clang, MSVC and sanitizers.

### Fixed

Compared with the code before the public release:

- `--` is no longer taken as the value of an option.
- An invalid boolean value (`--flag=maybe`) no longer reads an uninitialized variable.
- Missing `<limits.h>` include, and a non-portable `strcasecmp` dependency.
- The test runner no longer reports failed tests as passed.

### Removed

- `argh_set_description` and `argh_set_help_width`, which had no effect.

[Unreleased]: https://github.com/ilyabrin/argh/compare/v1.1.0...HEAD
[1.1.0]: https://github.com/ilyabrin/argh/compare/v1.0.0...v1.1.0
[1.0.0]: https://github.com/ilyabrin/argh/compare/v0.4.0...v1.0.0
[0.4.0]: https://github.com/ilyabrin/argh/compare/v0.3.1...v0.4.0
[0.3.1]: https://github.com/ilyabrin/argh/compare/v0.3.0...v0.3.1
[0.3.0]: https://github.com/ilyabrin/argh/compare/v0.2.0...v0.3.0
[0.2.0]: https://github.com/ilyabrin/argh/compare/v0.1.0...v0.2.0
[0.1.0]: https://github.com/ilyabrin/argh/releases/tag/v0.1.0
