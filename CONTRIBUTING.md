# Contributing to argh.h

Thanks for helping! Bug reports, test cases, docs fixes and code are all welcome.

## Before you start

Since v1.0 the API is stable and follows Semantic Versioning, so breaking changes wait for v2.0. Because of that:

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
make coverage # lines of argh.h the tests run; lists the ones they miss
make package-check # use argh through CMake, pkg-config, Conan and Meson
make completion-check # load the completion scripts in bash, zsh and fish
make c23      # build the tests as C23 (argh itself stays C99)
make thread-check # many threads parsing at once, under ThreadSanitizer
make mutation # mutation testing: lists changes to argh.h that no test notices
make bench    # run benchmarks (speed and code size)
make clean
```

Use a different compiler with `make CC=clang test`.

CI runs the same commands on Linux x86-64 and ARM64 (GCC, Clang, plus AddressSanitizer and UndefinedBehaviorSanitizer and 2 minutes of fuzzing), macOS (Clang) and Windows (MinGW, MSVC), as 32-bit x86, and under qemu on 32-bit ARM and big-endian s390x and PowerPC. It also checks the firmware size on ARM (`make size-arm`). If you can, run the sanitizers locally before sending a change that touches parsing:

```sh
make CC=clang test CFLAGS="-std=c99 -Wall -Wextra -Wpedantic -Werror -O1 -g -fsanitize=address,undefined"
```

For parsing changes, also fuzz for a few minutes: `make fuzz FUZZ_TIME=300` (needs clang with libFuzzer, for example on Linux or WSL). If it finds a crash, it saves the input as `crash-*`; add that file to [tests/fuzz](tests/fuzz) so `make test` replays it from then on. ClusterFuzzLite also fuzzes every pull request that touches argh.h (5 minutes per sanitizer) and runs an hour per sanitizer every night ([.clusterfuzzlite](.clusterfuzzlite)); a crash there attaches the input to the run, which goes to tests/fuzz the same way. New options or commands in the fuzz target belong in [tests/fuzz_argh.dict](tests/fuzz_argh.dict) too.

CI also runs mutation testing (`make mutation`, [Mull](https://github.com/mull-project/mull)): it changes argh.h in small ways, one at a time, and runs the tests on each change. It lists the changes no test noticed as `argh.h:LINE: Replaced < with <=` and fails if the share caught drops below the minimum in [tests/mutation.sh](tests/mutation.sh). New code should come with tests that notice such changes; raise the minimum when they do.

## What a good pull request looks like

- **One topic per PR.** A bug fix and a refactor are two PRs.
- **A test for every behavior change.** For a bug fix, add a test that fails without the fix. Tests live in [tests/test_argh.c](tests/test_argh.c) and use the `TEST` / `RUN_TEST` / `ASSERT_*` macros at the top of the file.
- **CI is green.** The build uses `-Werror` on GCC and Clang, so a warning is a failure.
- **Docs follow the code.** If you change behavior, update [README.md](README.md) and [llms.txt](llms.txt), and run `make docs-check`: it compiles the code shown in the docs, runs every `$ ./...` command and compares the output, and checks that every public name is documented. In README.md each code and console block carries a `<!-- docs-check: ... -->` comment that says which program in [tests/docs](tests/docs) it belongs to; when the output of a program changes, paste the real output into the block. Performance changes should also update [BENCHMARKS.md](BENCHMARKS.md).
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
