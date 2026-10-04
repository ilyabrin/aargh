# Changelog

All notable changes to argh.h are listed here.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the project uses [Semantic Versioning](https://semver.org/). Since v1.0, breaking changes only come with a new major version.

## [Unreleased]

### Added

- Mutation testing with Mull in CI (`make mutation`): every pull request checks that the tests notice small changes to argh.h. It started at 86% of 1,434 changes caught; new tests for flag words (`on`, `off`, `yes`, `no`), `argh_given` on positionals and environment counters, range edges and `--version` after an error bring it to 87%.
- Continuous fuzzing with ClusterFuzzLite: 5 minutes per sanitizer (address, undefined, memory) on every pull request that touches the parser, and an hour each every night. The fuzz target gets a dictionary of its options and commands (`tests/fuzz_argh.dict`), which `make fuzz` uses too.

## [1.11.0] - 2026-10-04

### Added

- With C23 or C++17, ignoring the result of `argh_parse`, `argh_exit_code`, `argh_given`, `argh_command` or `argh_last_error` is a warning (`[[nodiscard]]`); `(void)` silences it. C99 to C17 and older C++ see no change. argh still needs only C99.
- README: a "Threads" section. argh has no global or static state that changes, so parsers in different threads are independent; it lists what threads must not share.
- CI parses in 8 threads at once under ThreadSanitizer (`make thread-check`), with help and completion scripts too.
- CI builds the tests as C23 with GCC and Clang on Linux and macOS (`make c23`), and checks that a dropped `argh_parse` result is caught there.

## [1.10.0] - 2026-10-04

### Added

- Shell completion: `argh_completions(&p)` adds `--completions <shell>`, which prints a completion script for bash, zsh or fish and exits like `--help`. It completes options (and `--no-` forms), commands at every level, enum choices, and file names for strings and positionals; fish shows each option's help. `argh_print_completion(&p, shell)` prints a script for your own command or install step. Scripts are generated from the tables, so Tab never runs the program.
- `ARGH_NO_COMPLETION` removes completion entirely. Its code is linked only when used; its text adds about 0.6 KB to desktop programs unless removed. Firmware (`ARGH_NO_STDIO`) never has it.
- `pkg` example: `pkg --completions bash|zsh|fish`. CI loads the scripts in bash, zsh and fish and checks what Tab offers (`make completion-check`).

## [1.9.0] - 2026-10-03

### Changed

- The project is now **aargh**, after the snail's cry when salt hits it; the repository moved to github.com/ilyabrin/aargh (old links redirect). The name `argh` belongs to another library, a C++ parser, in vcpkg, ConanCenter and xmake-repo. The header stays `argh.h` and the API stays `argh_*` / `ARGH_*`: no code changes.
- Packages use the new name: `find_package(aargh)` with `aargh::aargh`, `pkg-config aargh`, Meson `dependency('aargh')`, Conan `aargh/<version>`, clib `ilyabrin/aargh`. The header installs to `include/aargh/argh.h`, and each package puts `include/aargh` on the include path, so `#include "argh.h"` keeps working and can't collide with another `argh.h`.

### Deprecated

- The v1.8 package names `find_package(argh)`, `argh::argh` and Meson's `dependency('argh')`. They keep working until 2.0; `-DARGH_INSTALL_LEGACY_NAME=OFF` installs without them.

### Added

- A logo: the snail with salt, antennae drawn as `--`. In `assets/`, with light and dark versions for the README and a social preview image.

## [1.8.0] - 2026-10-03

### Added

- Install with your build system: a CMake package (`argh::argh` through `FetchContent`, `add_subdirectory` or `find_package` after `cmake --install`, version-checked as SemVer), `argh.pc` for pkg-config, and a `meson.build` for Meson subprojects. The version comes from argh.h. As a dependency argh builds and installs nothing; built on its own, CMake runs the tests with `ctest`.
- Conan 2: `conan create .` packages argh from the repository (header-only, `argh::argh` for CMakeDeps, `argh` for pkg-config), with a test_package. clib: `clib install ilyabrin/aargh`.
- CI builds and runs a program through each of these on Linux and Windows (`make package-check`).

## [1.7.0] - 2026-10-03

### Changed

- Parsing runs about 9% fewer instructions (callgrind): long names are compared in place instead of with `strncmp`, and the per-parse definition checks pass the usual option on a few compares. On the ARM64 CI runner with GCC the gap to `getopt_long` shrank from 13% toward parity.
- Tests cover 99% of the lines in argh.h, and CI fails if that drops below 98% (`make coverage`).

## [1.6.0] - 2026-10-03

### Added

- Ranges: `argh_range(&p, &jobs, 1, 64)` or `ARGH_RANGE(&jobs, 1, 64)` right after an `int`, `long`, `unsigned` or `size_t` option. Values outside the bounds (both included) fail with `value '0' for '-j' is out of range (1 to 64)`, from the command line, the environment or an implicit value. Help shows `<1..64>` unless you set a value name. Without `NDEBUG` argh checks that the range follows an integer option and fits its type. Costs about 200 to 260 bytes on firmware and nothing measurable in parse time.

## [1.5.0] - 2026-10-03

### Added

- Optional values: `argh_implicit(&p, &color, "always")` or `ARGH_IMPLICIT(&color, "always")` right after an option. `--color` alone means `--color=always`; a value needs `=` (`--color=never`), so in `--color a.txt` the file stays a positional. A short name works alone, like a flag (`-cv`). Help shows `--color[=<when>]`. Without `NDEBUG` argh checks that the entry follows a value option with a long name and that the option accepts the value. Costs about 120 bytes on firmware and nothing in parse time.

## [1.4.0] - 2026-10-03

### Added

- Unsigned options: `argh_uint` / `ARGH_UINT` for `unsigned` and `argh_size` / `ARGH_SIZE` for `size_t`. They take the same decimal and `0x` forms as `int`. A minus sign is an error (`expected a non-negative integer`) instead of wrapping around to a huge value as `strtoul` does, and values past the type's maximum are out of range.

### Changed

- Integers are parsed by argh's own digit loop instead of `strtol`. Same rules and messages. Parsing is about 7% faster, and firmware images are about 290 bytes smaller even with the new types; desktop builds grow by 0.2 to 0.6 KB, where `strtol` comes from the shared C library anyway.

## [1.3.0] - 2026-10-02

### Added

- Environment variables: `argh_env(&p, &jobs, "TOOL_JOBS")` or `ARGH_ENV(&jobs, "TOOL_JOBS")` in a table. When the command line doesn't set the option, its value comes from the variable, checked like a command-line value; it counts as given for required options, `argh_given` and rules. Help shows `[env: TOOL_JOBS]`, errors name the variable (`invalid value 'x' in TOOL_JOBS for '--jobs'`), and a missing required option says `(or set TOOL_TOKEN)`.
- `ARGH_GETENV(name)` decides how variables are read: `getenv` by default; redefine it to read from elsewhere or to fake the environment in tests. With `ARGH_NO_STDIO` there is no environment unless it is defined, so firmware doesn't pull in `getenv`.
- The logship and pkg examples take `LOGSHIP_TO` and `PKG_REGISTRY`.
- Cost: about 1 KB of code on a desktop (Linux GCC, release), whether or not a program uses it; about 12 bytes on firmware built with `ARGH_NO_STDIO`, which has no environment. Firmware that keeps stdio links newlib's `getenv` too (about 1 KB more); define `ARGH_GETENV(name)` as `NULL` there to leave it out.

### Fixed

- Programs built with MSVC `/W4 /WX` failed on the deprecation warning for `getenv` (C4996); argh.h now silences it locally.

## [1.2.0] - 2026-10-02

### Added

- Help wraps long descriptions at 80 columns, lined up under the description column. A default value always stays whole on one line, and a `\n` in a description starts a new line at the same column. `ARGH_HELP_WIDTH` sets the width; `0` turns wrapping off and leaves its code out. Usage examples are not wrapped.
- `make docs-check` and a `docs` CI job: the code in README.md compiles, every `$ ./...` command in README.md and examples/README.md prints exactly what the docs show, every public name is documented, and the snippets in llms.txt compile. The programs behind README live in `tests/docs`.
- The reduced firmware build in the size budget now also sets `ARGH_HELP_WIDTH=0`; wrapping costs about 250 bytes of flash on a Cortex-M.

### Fixed

- The `wc` and `logship` output shown in examples/README.md was only right on Windows: it read LICENSE and SECURITY.md, whose size depends on the line endings git checks out. The examples now read sample logs in `examples/data` with LF line endings on every platform.

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

[Unreleased]: https://github.com/ilyabrin/aargh/compare/v1.11.0...HEAD
[1.11.0]: https://github.com/ilyabrin/aargh/compare/v1.10.0...v1.11.0
[1.10.0]: https://github.com/ilyabrin/aargh/compare/v1.9.0...v1.10.0
[1.9.0]: https://github.com/ilyabrin/aargh/compare/v1.8.0...v1.9.0
[1.8.0]: https://github.com/ilyabrin/aargh/compare/v1.7.0...v1.8.0
[1.7.0]: https://github.com/ilyabrin/aargh/compare/v1.6.0...v1.7.0
[1.6.0]: https://github.com/ilyabrin/aargh/compare/v1.5.0...v1.6.0
[1.5.0]: https://github.com/ilyabrin/aargh/compare/v1.4.0...v1.5.0
[1.4.0]: https://github.com/ilyabrin/aargh/compare/v1.3.0...v1.4.0
[1.3.0]: https://github.com/ilyabrin/aargh/compare/v1.2.0...v1.3.0
[1.2.0]: https://github.com/ilyabrin/aargh/compare/v1.1.0...v1.2.0
[1.1.0]: https://github.com/ilyabrin/aargh/compare/v1.0.0...v1.1.0
[1.0.0]: https://github.com/ilyabrin/aargh/compare/v0.4.0...v1.0.0
[0.4.0]: https://github.com/ilyabrin/aargh/compare/v0.3.1...v0.4.0
[0.3.1]: https://github.com/ilyabrin/aargh/compare/v0.3.0...v0.3.1
[0.3.0]: https://github.com/ilyabrin/aargh/compare/v0.2.0...v0.3.0
[0.2.0]: https://github.com/ilyabrin/aargh/compare/v0.1.0...v0.2.0
[0.1.0]: https://github.com/ilyabrin/aargh/releases/tag/v0.1.0
