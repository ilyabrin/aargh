# argh.h compared

How argh.h compares with three parsers C programs often use: `getopt_long` (the C library's), [cargs](https://github.com/likle/cargs) v1.2.0 and [argparse](https://github.com/cofyc/argparse) v1.1.0. The three libraries are MIT-licensed and make no heap allocations; `getopt_long` is part of the C library on Linux and BSD.

The short version: argh does the most work for you (typed values that are fully checked, help, errors with suggestions, commands, rules) and is the strictest about input. It pays for that in code size; in speed it is ahead of getopt_long, on par with cargs and close to argparse. cargs and argparse are much smaller, and leave more to your program.

Everything below was measured or tried, not taken from documentation. Reproduce it with [bench/compare/compare.sh](bench/compare/compare.sh), which fetches cargs and argparse at the versions above.

## Features

|                                        | argh.h                            | getopt_long                  | cargs                     | argparse                         |
| -------------------------------------- | --------------------------------- | ---------------------------- | ------------------------- | -------------------------------- |
| Files to add                           | 1 header                          | none (libc; not on MSVC)     | `.h` + `.c`               | `.h` + `.c`                      |
| Values                                 | written into typed variables      | strings                      | strings                   | written into typed variables     |
| Types                                  | bool, count, int, long, double, string, enum, list, custom | -   | -                         | bool, bit, int, float, string    |
| Numbers checked (range, garbage)       | yes                               | your code                    | your code                 | partly, see below                |
| Generated help                         | usage, arguments, groups, defaults | no                          | option list               | usage, options, groups           |
| Required options and positionals       | yes                               | no                           | no                        | no                               |
| Named positionals in help              | yes                               | no                           | no                        | no                               |
| Commands (`tool remote add`)           | yes, nested, with help per level  | no                           | no                        | no (by hand)                     |
| Rules between options                  | yes                               | no                           | no                        | no                               |
| "Did you mean" suggestions             | yes                               | no                           | no                        | no                               |
| `--no-flag`                            | opt-in per flag                   | no                           | no                        | automatic for booleans           |
| Abbreviated long options (`--jo`)      | no (an error)                     | yes                          | no                        | no                               |
| Returns to your program on errors and `--help` | yes                       | yes                          | yes                       | no: calls `exit()`               |
| Exit code on a usage error             | 2 (`argh_exit_code`)              | your choice                  | your choice               | 1                                |
| Global state                           | none                              | `optind`, `optarg`           | none                      | none                             |
| Output redirectable (no stdio needed)  | yes                               | no (can be silenced)         | yes (printer function)    | no                               |

## Behavior on tricky input

One integer option, `-j`/`--jobs`. What each program ends up with:

| Input                  | argh.h                                        | cargs (+ `atoi`)       | argparse                          |
| ---------------------- | --------------------------------------------- | ---------------------- | --------------------------------- |
| `--jobs 8`             | 8                                             | 8                      | 8                                 |
| `--jobs 010`           | 10                                            | 10                     | **8** (read as octal)             |
| `--jobs 99999999999`   | error: out of range, exit 2                   | **1215752191**         | **1215752191**, no error          |
| `--jobs 5x`            | error: expected an integer, exit 2            | **5**                  | error, exit 1                     |
| `--jobs=`              | error: expected an integer, exit 2            | **0**                  | **0**, no error                   |
| `--jbos 3`             | error with "did you mean '--jobs'?", exit 2   | error, exit 1          | error and usage, exit 1           |
| `-j=3`                 | error: write `-j VALUE` or `--jobs=VALUE`     | 3                      | error, exit 1                     |
| `--jobs` (no value)    | error: requires a value, exit 2               | **no error, value NULL** | error, exit 1                   |

Bold marks a wrong value that the program would go on to use. cargs returns strings and leaves checking to your program; the column shows the shortest conversion, `atoi`. With `strtol` and your own checks, the results can match argh's. `getopt_long` is in the same position as cargs; in addition it accepts `--jo 3` as `--jobs 3`, and gives `=3` as the value of `-j=3`.

## Speed

The workload of [BENCHMARKS.md](BENCHMARKS.md): 30 options defined, 17 arguments parsed, every value read back and numbers checked (for getopt and cargs, with `strtol`/`strtod` and the same checks argh does). All four parsers must read identical values before timing starts.

Intel Core i5-12400F, Ubuntu 24.04 on WSL 2, `-O2 -DNDEBUG`, median of three runs, each the best of 5 × 200,000 iterations:

| Parser       | GCC 13.3 | Clang 18.1 |
| ------------ | -------: | ---------: |
| argh (table) |   444 ns |     426 ns |
| getopt_long  |   492 ns |     497 ns |
| cargs        |   457 ns |     421 ns |
| argparse     |   419 ns |     371 ns |

argh is 10% to 14% faster than `getopt_long`, on par with cargs, and 6% to 15% slower than argparse, which also checks the least of the four. In absolute terms all four are within 0.13 µs of each other, once, at program start.

## Code size

`.text` added to the same 3-option program (bench/size_none.c as the baseline), Linux, `-Os -ffunction-sections -fdata-sections -Wl,--gc-sections`:

| Parser                                             | GCC 13.3 | Clang 18.1 |
| -------------------------------------------------- | -------: | ---------: |
| argh                                               |  17.7 KB |    21.4 KB |
| argh (`ARGH_NO_COMMANDS ARGH_NO_SUGGEST`)          |  14.0 KB |    16.0 KB |
| cargs                                              |   3.2 KB |     3.2 KB |
| argparse                                           |   4.4 KB |     4.1 KB |
| getopt_long                                        |   0.6 KB |     0.5 KB |

On a Cortex-M0 (newlib-nano, the same program printing with `printf`):

| Parser                    | Flash added |
| ------------------------- | ----------: |
| argh                      |     31.7 KB |
| argh with `ARGH_NO_FLOAT` |     10.0 KB |
| cargs                     |      1.7 KB |
| argparse                  |     22.6 KB |

argh is the largest because it carries help formatting, error messages, value checking, commands, suggestions and rules; the strings alone are a few KB. Its reduced builds remove what you don't use. On firmware the C library's float parser dominates: argparse pulls in `strtof` for its float type, argh `strtod` unless you define `ARGH_NO_FLOAT`.

## Which one to pick

- **getopt_long**: a few string options on Linux or BSD, and you are happy to write help, checks and error handling yourself.
- **cargs**: the smallest portable option list with help output, when your program converts and checks values itself.
- **argparse**: typed options with little code, when calling `exit()` from the parser is fine and you trust the input.
- **argh.h**: when input should be checked for you and wrong input should never become a wrong value; when you want help, errors, commands or rules without writing them; when the parser must never exit your program or allocate.
