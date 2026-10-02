# Examples

Three programs, from a five-minute tool to a multi-file application. Each one is a real kind of tool, and together they use most of argh.h: every option type except `long` and `double`, tables, commands, custom types, rules and a validator.

```sh
make examples   # build all three
make smoke      # run them and check their output (CI does this on every push)
```

| Example                    | What it is                                  | What it shows                                                                                   |
| -------------------------- | ------------------------------------------- | ----------------------------------------------------------------------------------------------- |
| [wc.c](wc.c)               | Counts lines, words and bytes, like `wc`    | Flags, a file list, `--help` and `--version` for free                                           |
| [logship.c](logship.c)     | Sends log files to a collector              | An option table, help groups, custom types, enums, lists, rules, a validator, `argh_given`      |
| [pkg/](pkg)                | A package manager front end, like `cargo`   | Commands over several files, global options, handlers with a context, a version type, pass-through|

`logship` and `pkg` are dry runs: they do all the checking a real tool would do, then print what they would do instead of touching the network.

## wc: the basics

Eight lines of argh code, and it counts the same as the real `wc`.

```console
$ ./wc LICENSE SECURITY.md
      21     169    1087 LICENSE
      33     147     998 SECURITY.md
      54     316    2085 total

$ ./wc --line LICENSE
wc: unknown option '--line' (did you mean '--lines'?)
Try 'wc --help' for more information.
```

## logship: a tool with real configuration

One `static const` table fills a config struct. Sizes, durations and `host:port` are custom types with their own error messages, and help shows their defaults.

```console
$ ./logship --help
Usage: logship [OPTIONS] [files...]

Send log files to a collector (dry run: prints the plan)

Arguments:
  [files...]                      Log files to send

Options:
  -v, --verbose                   More output, repeat for more

Input:
      --stdin                     Read logs from standard input
  -x, --exclude <text>            Skip files whose name contains this text
      --max-size <size>           Skip files larger than this (default: 64M)

Output:
  -t, --to <host:port>            Collector address (required)
  -f, --format <json|syslog|raw>  Record format (default: json)
      --gzip                      Compress with gzip
      --zstd                      Compress with zstd
      --chunk <size>              Bytes per request (default: 1M)

Network:
      --timeout <duration>        Give up on a request after (default: 30s)
  -r, --retries <n>               Attempts per request (default: 3)
      --retry-delay <duration>    Wait between attempts (default: 500ms)

TLS:
      --tls-key <file>            Client key (PEM)
      --tls-cert <file>           Client certificate (PEM)
      --[no-]verify               Check the collector's certificate

  -h, --help                      Print help
  -V, --version                   Print version

$ ./logship --to logs.example.com:6514 --tls-key k.pem --tls-cert c.pem --chunk 1K --gzip -vv -x bench LICENSE SECURITY.md bench/size.sh
  send  LICENSE: 1087 bytes in 2 requests
  send  SECURITY.md: 998 bytes in 1 request
  skip  bench/size.sh (excluded)
Plan: 2 files, 2085 bytes in 3 requests to logs.example.com:6514
      format json, gzip, TLS
      timeout 30s (default), 3 attempts
```

Every mistake gets a precise message and exit code 2. The first line of each, a `Try 'logship --help'` hint follows:

```console
logship: missing required option '--to'
logship: invalid value 'x:99999' for '--to': port must be a number from 1 to 65535
logship: invalid value '5x' for '--timeout': expected a duration like 500ms, 30s or 5m
logship: options '--gzip' and '--zstd' cannot be used together
logship: option '--tls-key' requires '--tls-cert'
logship: --chunk must not be larger than --max-size
logship: unknown option '--fromat' (did you mean '--format'?)
```

## pkg: the hard case, made plain

Nested commands, each in its own file with its own options, plus global options that work before and after the command name. Writing this with `getopt` means dispatching by hand, re-parsing for every level and writing all the help and errors yourself.

```
pkg/
  pkg.h       shared option struct and declarations
  main.c      global options, the command tree, rules, validator, dispatch
  install.c   install and remove, and a custom type for version requirements
  remote.c    the remote add / remove / list group
  exec.c      exec <program> [args...], options passed on untouched
```

```console
$ ./pkg install left-pad --version ^1.3 --dev
+ left-pad ^1.3.0 (dev)

$ ./pkg -v -C ./app --offline install --dry-run
[pkg] in ./app, source: local cache
Would install packages from the lockfile, source: local cache

$ ./pkg exec node --version
Would run: node --version

$ ./pkg --version
pkg 0.9.0
```

`install` has its own `--version` option for the package version, like `cargo install --version`, while `pkg --version` still prints the program's version. And `exec` is registered with `ARGH_POSIX`, so everything after the program name, `--version` included, goes to that program without a `--`.

Help follows the command you ask about, with the global options listed separately:

```console
$ ./pkg help remote add
Usage: pkg remote add [OPTIONS] <name> <url>

Add a package source

Arguments:
  <name>                Short name, such as origin
  <url>                 Where the packages come from

Options:
  -f, --force           Replace a remote with the same name

Global options:
  -v, --verbose         More output, repeat for more
  -C <dir>              Run as if started in this directory (default: .)
      --registry <url>  Package registry URL
      --offline         Use only the local cache
      --color <auto|always|never>
                        When to use colors (default: auto)

  -h, --help            Print help
  -V, --version         Print version
```

Rules and the validator mix global and command options, and know which command runs:

```console
$ ./pkg instal left-pad
pkg: unknown command 'instal' (did you mean 'install'?)
Try 'pkg --help' for more information.

$ ./pkg install x --version 1.02
pkg: invalid value '1.02' for '--version': each part of a version must be a number without leading zeros
Try 'pkg install --help' for more information.

$ ./pkg install -D -O x
pkg: options '--dev' and '--optional' cannot be used together
Try 'pkg install --help' for more information.

$ ./pkg --offline remote add o https://u
pkg: 'remote add' needs the network; drop --offline
Try 'pkg remote add --help' for more information.

$ ./pkg remote
pkg: 'remote' needs a command

Commands:
  add     Add a package source
  remove  Remove a package source
  list    Show package sources

Try 'pkg remote --help' for more information.
```
