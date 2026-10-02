# Benchmarks

How fast and how small argh.h is, compared with `getopt_long`, the parser most C programs already use.

Run them yourself:

```sh
make bench      # speed, memory and code size on your machine
make size-arm   # flash added to ARM firmware (needs arm-none-eabi-gcc)
```

CI runs the same benchmarks on Linux, macOS and Windows (MinGW) on every pull request and every push to `main`. CI does not compare the timings, but it does check that every parser reads identical values from the workload, and it fails if the firmware size goes over budget.

## Parse speed

The workload: 30 options defined, 17 arguments on the command line (short and long options, `--name=value`, combined flags `-abc`, numbers, positionals). One iteration is the full job a program does at startup: set up the parser, parse, read every value back.

argh is measured two ways: with builder calls (`argh_int(&p, ...)`) and with a `static const` table (`ARGH_INT(...)`).

Release builds (`-O2 -DNDEBUG`) of v1.0 on CI runners. CI machines differ from run to run, so each row comes from a single run: the one with the median argh-to-getopt ratio out of five.

| Platform           | argh (table) | argh (builder) | getopt_long |
| ------------------ | -----------: | -------------: | ----------: |
| macOS, Clang       |       508 ns |         557 ns |      635 ns |
| Linux, Clang       |       551 ns |         628 ns |      628 ns |
| Linux, GCC         |       605 ns |         706 ns |      622 ns |
| Windows, MinGW GCC |       953 ns |       1,009 ns |      894 ns |

At the median, argh with a table is 20% faster than `getopt_long` on macOS, 12% faster with Clang and 3% faster with GCC on Linux, and 7% slower with MinGW; across all five runs the range is 28% faster to 7% slower. That is while also validating every value and supporting commands, rules and generated help. In absolute terms the difference is a fraction of a microsecond, once, at program start. The builder costs a little more than the table because it fills in the option list on every run.

### Between versions

On one machine (Intel Core i5-12400F, Windows 11, MinGW GCC 13.2), best of 9 alternating runs:

| Change          | argh (table)               | argh (builder)             | getopt_long (control) |
| --------------- | -------------------------- | -------------------------- | --------------------- |
| v0.3.1 → v0.4.0 | 986 → 1,005 ns (+2%)       | 1,040 → 1,083 ns (+4%)     | 860 → 863 ns          |
| v0.4.0 → v1.0.0 | 971 → 871 ns (**−10%**)    | 1,043 → 945 ns (**−9%**)   | 825 → 813 ns          |

Each row was measured on its own day with both versions side by side; `getopt_long` shows how steady the machine was. v1.0 got faster after profiling: the checks for names reserved by `--help` and `--version` no longer call `strcmp` for every option on every parse, and `argh_init` no longer clears builder storage that table-only programs don't use.

### Debug builds

Without `NDEBUG`, `argh_parse` also checks the definitions for mistakes such as duplicate option names. The check compares every pair of options, and with 30 options it adds about 1.6 µs. That is a one-time cost at program start, like an `assert`, and it catches real mistakes. Release builds with `-DNDEBUG` skip it.

### Compared with v0.1

Same machine, same workload:

|                  |  v0.1.0 |                  v1.0.0 (table) |
| ---------------- | ------: | ------------------------------: |
| Time per parse   | 2.73 µs |                         0.87 µs |
| Heap allocations |      11 |                               0 |
| `sizeof` parser  | 4,880 B | 248 B + 1,848 B builder storage |

v0.1 copied every value to the heap and looked options up by name twice: once while parsing and once in every `argh_get_*` call. Since v0.2 values go straight into variables, so both costs are gone.

## Memory

- **Heap:** zero allocations, on success, on errors and when printing help. The benchmark counts calls to `malloc`/`realloc`, and valgrind confirms it on Linux.
- **Parser:** on 64-bit systems, 248 bytes of state (208 with `ARGH_NO_COMMANDS`), plus the builder storage: `ARGH_BUILDER_CAP` + 1 slots of `sizeof(argh_opt)`, one of them for the terminator. `argh_opt` is 56 bytes on 64-bit systems and 28 bytes on 32-bit microcontrollers, so the default of 32 takes 1,848 bytes on 64-bit. Programs that only use tables can set `ARGH_BUILDER_CAP` to 0.
- **Option tables:** a `static const` table goes into read-only memory. On a Cortex-M each option takes 28 bytes of flash and no RAM.

## Code size

The `.text` added to a minimal 3-option program, compared with the same program without a parser. Built with `-Os -ffunction-sections -fdata-sections -Wl,--gc-sections`, v1.0:

| Platform                     | argh    | argh reduced | getopt_long |
| ---------------------------- | ------: | -----------: | ----------: |
| Linux, GCC 13.3 (CI)         | 17.7 KB |      14.0 KB |      0.6 KB |
| Linux, Clang 18.1 (CI)       | 21.4 KB |      16.0 KB |      0.5 KB |
| Windows, MinGW GCC 15.2 (CI) | 18.1 KB |              |     28.0 KB |

"Reduced" is `-DARGH_NO_COMMANDS -DARGH_NO_SUGGEST`, for programs that don't need commands or "did you mean" suggestions. What each option saves on Linux GCC: `ARGH_NO_COMMANDS` 3.0 KB, `ARGH_NO_SUGGEST` 0.8 KB, `ARGH_NO_FLOAT` 0.4 KB (far more on firmware, see below).

Linux GCC at each release, as measured then: v0.1 7.3 KB, v0.2 12.1 KB, v0.3 17.1 KB, v0.4 17.6 KB, v1.0 17.7 KB.

Read this one with care:

- **Each version is bigger than the last** because it does more. v0.2 added formatted help, detailed errors, enums, lists and definition checks; v0.3 added commands, suggestions, custom types and rules; v0.4 added the per-command POSIX mode and its own number formatting. On Linux, `size` counts read-only data as text, so the help and error strings are part of these numbers.
- **Unused features cost less than they look.** The rule checker is only linked when `argh_rules` is called, and the `ARGH_NO_*` flags remove commands, suggestions and floating point.
- **getopt_long is only counted where it is linked statically**, as in MinGW. On Linux it lives in the shared C library and adds almost nothing to your binary, so for desktop Linux getopt is the smaller choice. On MinGW it pulls in error-printing and locale code.
- macOS is not measured: its `size` tool reports page-aligned segments, which hides differences of a few KB.

## Microcontrollers

Flash added to a bare-metal firmware shell command with three options, compared with the same firmware without a parser. `arm-none-eabi-gcc` 13.2, newlib-nano, `-Os -ffunction-sections -fdata-sections -Wl,--gc-sections`, `-DNDEBUG -DARGH_NO_STDIO`. CI fails if a build goes over its budget.

| Build                                            | Cortex-M0 | Cortex-M4 | Budget  |
| ------------------------------------------------ | --------: | --------: | ------: |
| `ARGH_NO_FLOAT`                                  |   11.0 KB |   11.3 KB | 12.0 KB |
| `ARGH_NO_FLOAT ARGH_NO_COMMANDS ARGH_NO_SUGGEST` |    9.2 KB |    9.5 KB | 10.0 KB |
| with `argh_double` (`ARGH_NO_STDIO` only)        |   38.0 KB |   31.7 KB |       - |

- **Numbers include everything linked because of argh**: help and error strings, and the C library functions it calls (`strtol`, `strcmp` and others).
- **Why doubles cost 27 KB:** newlib's `strtod` brings its float parser and soft-float arithmetic, and through an internal `assert` also `fprintf`. `ARGH_NO_FLOAT` removes `argh_double` so none of it is linked.
- **RAM:** `sizeof(argh_parser)` is 136 bytes plus 28 bytes for each of the `ARGH_BUILDER_CAP` + 1 builder slots: 1,060 bytes with the default of 32. With tables only, set `ARGH_BUILDER_CAP` to 0 and it is 164 bytes.
- Run it yourself: `make size-arm` ([bench/size_arm.sh](bench/size_arm.sh), [bench/size_fw.c](bench/size_fw.c)).

## Method

- Source: [bench/bench_parse.c](bench/bench_parse.c) (speed and memory), [bench/size.sh](bench/size.sh) (code size on the host), [bench/size_arm.sh](bench/size_arm.sh) (firmware).
- Speed: best of 5 runs, 200,000 iterations each, `-O2 -DNDEBUG`. Each iteration gets a fresh copy of `argv`, because both parsers reorder it.
- Allocations: calls to `malloc`/`realloc` are counted by wrapping them in the benchmark.
- The `getopt_long` code does the same work as argh: typed values, numbers converted with `strtol`/`strtod`.
- CI runners are shared machines, so absolute times vary between runs. Compare columns within one row.
