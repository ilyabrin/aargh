# Contributing to argh.h

Thanks for helping! Bug reports, test cases, docs fixes and code are all welcome.

## Before you start

argh.h is in early development (0.x), and the API can still change until v1.0. Because of that:

- **Bug fixes, tests, docs and portability fixes:** open a pull request directly.
- **New features or API changes:** please open an issue first. The feature may already be planned, or it may need a different shape. A short discussion saves you from writing code that has to be redone.

Found a security problem? Don't open an issue, see [SECURITY.md](SECURITY.md).

Before larger changes, [ARCHITECTURE.md](ARCHITECTURE.md) explains how the code is organized, how a parse runs and why the design is the way it is.

## Build and test

You need a C99 compiler and `make`. That's it.

```sh
make test     # build and run the test suite
make smoke    # run the examples and check their output
make cxx      # check that argh.h compiles as C++
make bench    # run benchmarks (speed and code size)
make clean
```

Use a different compiler with `make CC=clang test`.

CI runs the same commands on Linux (GCC, Clang, plus AddressSanitizer and UndefinedBehaviorSanitizer and 2 minutes of fuzzing), macOS (Clang) and Windows (MinGW, MSVC), and checks the firmware size on ARM (`make size-arm`). If you can, run the sanitizers locally before sending a change that touches parsing:

```sh
make CC=clang test CFLAGS="-std=c99 -Wall -Wextra -Wpedantic -Werror -O1 -g -fsanitize=address,undefined"
```

For parsing changes, also fuzz for a few minutes: `make fuzz FUZZ_TIME=300` (needs clang with libFuzzer, for example on Linux or WSL). If it finds a crash, it saves the input as `crash-*`; add that file to [tests/fuzz](tests/fuzz) so `make test` replays it from then on.

## What a good pull request looks like

- **One topic per PR.** A bug fix and a refactor are two PRs.
- **A test for every behavior change.** For a bug fix, add a test that fails without the fix. Tests live in [tests/test_argh.c](tests/test_argh.c) and use the `TEST` / `RUN_TEST` / `ASSERT_*` macros at the top of the file.
- **CI is green.** The build uses `-Werror` on GCC and Clang, so a warning is a failure.
- **Docs follow the code.** If you change behavior, update [README.md](README.md) and [llms.txt](llms.txt). Performance changes should also update [BENCHMARKS.md](BENCHMARKS.md).
- **A line in [CHANGELOG.md](CHANGELOG.md)** under `Unreleased` for anything a user would notice.

## Code rules

These keep argh.h small and portable:

- **Plain C99.** Only the C standard library. No compiler extensions without a portable fallback, and the header must also compile as C++.
- **No heap allocations.** argh never calls `malloc`, and the benchmark checks it. Keep it that way.
- **No global state.** Everything lives in the parser struct.
- **Names:** public API starts with `argh_` / `ARGH_`, internals with `argh__` / `ARGH__`.
- **Style:** match the surrounding code. 4-space indent, braces on their own line, `/* */` comments. Comments are in English and explain *why*, not *what*.
- **Ambiguous input is an error.** The parser never guesses what the user meant.

## Commit messages

Short imperative subject line, for example `Fix -- being taken as an option value`. Add a body when the reason for a change isn't obvious from the diff.

## License

By contributing, you agree that your contributions are licensed under the [MIT License](LICENSE).
