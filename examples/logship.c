/*
 * logship: send log files to a collector.
 *
 *   $ ./logship --to logs.example.com:6514 --tls-key client.key --tls-cert client.crt \
 *               --chunk 1M --format json --gzip -v app.log worker.log
 *
 * This is a dry run: it checks the files and prints the upload plan instead
 * of opening a connection, so it runs anywhere.
 *
 * Shows what a real tool needs beyond flags:
 *   - an option table with help groups, filled into one config struct
 *   - custom types: sizes (64M), durations (30s, 500ms), host:port
 *   - an enum, a repeatable option, a negatable flag, a counter
 *   - value names in help ("<file>") set in the table
 *   - rules between options and a validator for everything else
 *   - argh_given() to tell a default from an explicit value
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ARGH_IMPLEMENTATION
#include "../argh.h"

/* ----------------------------------------------------------------------------
 * Custom types
 * ---------------------------------------------------------------------------- */

/* Sizes: 512, 64K, 10M, 1G */
static const char *parse_size(const char *text, void *target)
{
    char *end;
    unsigned long long value;
    if (*text < '0' || *text > '9')
        return "expected a size like 512K, 10M or 1G";
    value = strtoull(text, &end, 10);
    switch (*end)
    {
    case 'K': value <<= 10; end++; break;
    case 'M': value <<= 20; end++; break;
    case 'G': value <<= 30; end++; break;
    default: break;
    }
    if (*end)
        return "expected a size like 512K, 10M or 1G";
    *(unsigned long long *)target = value;
    return NULL;
}

static void format_bytes(unsigned long long v, char *buf, size_t size)
{
    if (v >= (1ull << 30) && v % (1ull << 30) == 0)
        snprintf(buf, size, "%lluG", v >> 30);
    else if (v >= (1ull << 20) && v % (1ull << 20) == 0)
        snprintf(buf, size, "%lluM", v >> 20);
    else if (v >= (1ull << 10) && v % (1ull << 10) == 0)
        snprintf(buf, size, "%lluK", v >> 10);
    else
        snprintf(buf, size, "%llu", v);
}

static bool format_size(const void *target, char *buf, size_t size)
{
    format_bytes(*(const unsigned long long *)target, buf, size);
    return true;
}

/* Durations in milliseconds: 500ms, 30s, 5m, 1h */
static const char *parse_duration(const char *text, void *target)
{
    char *end;
    unsigned long value;
    if (*text < '0' || *text > '9')
        return "expected a duration like 500ms, 30s or 5m";
    value = strtoul(text, &end, 10);
    if (strcmp(end, "ms") == 0)
        ;
    else if (strcmp(end, "s") == 0)
        value *= 1000;
    else if (strcmp(end, "m") == 0)
        value *= 60 * 1000;
    else if (strcmp(end, "h") == 0)
        value *= 60 * 60 * 1000;
    else
        return "expected a duration like 500ms, 30s or 5m";
    *(unsigned long *)target = value;
    return NULL;
}

static bool format_duration(const void *target, char *buf, size_t size)
{
    unsigned long ms = *(const unsigned long *)target;
    if (ms % 1000)
        snprintf(buf, size, "%lums", ms);
    else
        snprintf(buf, size, "%lus", ms / 1000);
    return true;
}

/* host:port into a struct */
typedef struct
{
    char host[128];
    int port;
} endpoint;

static const char *parse_endpoint(const char *text, void *target)
{
    endpoint *ep = (endpoint *)target;
    const char *colon = strrchr(text, ':');
    char *end;
    long port;
    if (!colon || colon == text)
        return "expected host:port";
    if ((size_t)(colon - text) >= sizeof(ep->host))
        return "host name is too long";
    port = strtol(colon + 1, &end, 10);
    if (end == colon + 1 || *end || port < 1 || port > 65535)
        return "port must be a number from 1 to 65535";
    memcpy(ep->host, text, (size_t)(colon - text));
    ep->host[colon - text] = '\0';
    ep->port = (int)port;
    return NULL;
}

static const argh_type size_type = {"<size>", parse_size, format_size};
static const argh_type duration_type = {"<duration>", parse_duration, format_duration};
static const argh_type endpoint_type = {"<host:port>", parse_endpoint, NULL};

/* ----------------------------------------------------------------------------
 * Configuration: defaults are plain initializers
 * ---------------------------------------------------------------------------- */

static const char *const formats[] = {"json", "syslog", "raw", NULL};

static const char *exclude_buf[16];

static struct
{
    int verbose;
    bool use_stdin;
    argh_values exclude;
    unsigned long long max_size;
    endpoint to;
    int format;
    bool gzip, zstd;
    unsigned long long chunk;
    unsigned long timeout_ms;
    int retries;
    unsigned long retry_delay_ms;
    const char *tls_key, *tls_cert;
    bool verify;
    argh_values files;
} cfg = {
    0,                        /* verbose */
    false,                    /* use_stdin */
    ARGH_VALUES(exclude_buf), /* exclude */
    64ull << 20,              /* max_size: 64M */
    {"", 0},                  /* to */
    0,                        /* format: json */
    false, false,             /* gzip, zstd */
    1ull << 20,               /* chunk: 1M */
    30 * 1000,                /* timeout: 30s */
    3,                        /* retries */
    500,                      /* retry_delay: 500ms */
    NULL, NULL,               /* tls_key, tls_cert */
    true,                     /* verify */
    {NULL, 0, 0},             /* files */
};

static const argh_opt options[] = {
    ARGH_COUNT('v', "verbose", &cfg.verbose, "More output, repeat for more"),

    ARGH_GROUP("Input"),
    ARGH_FLAG(0, "stdin", &cfg.use_stdin, "Read logs from standard input"),
    ARGH_LIST('x', "exclude", &cfg.exclude, "Skip files whose name contains this text", 0, "<text>"),
    ARGH_CUSTOM(0, "max-size", &cfg.max_size, &size_type, "Skip files larger than this"),

    ARGH_GROUP("Output"),
    ARGH_CUSTOM('t', "to", &cfg.to, &endpoint_type, "Collector address", ARGH_REQUIRED),
    ARGH_ENUM('f', "format", &cfg.format, formats, "Record format"),
    ARGH_FLAG(0, "gzip", &cfg.gzip, "Compress with gzip"),
    ARGH_FLAG(0, "zstd", &cfg.zstd, "Compress with zstd"),
    ARGH_CUSTOM(0, "chunk", &cfg.chunk, &size_type, "Bytes per request"),

    ARGH_GROUP("Network"),
    ARGH_CUSTOM(0, "timeout", &cfg.timeout_ms, &duration_type, "Give up on a request after"),
    ARGH_INT('r', "retries", &cfg.retries, "Attempts per request"),
    ARGH_CUSTOM(0, "retry-delay", &cfg.retry_delay_ms, &duration_type, "Wait between attempts"),

    ARGH_GROUP("TLS"),
    ARGH_STRING(0, "tls-key", &cfg.tls_key, "Client key (PEM)", 0, "<file>"),
    ARGH_STRING(0, "tls-cert", &cfg.tls_cert, "Client certificate (PEM)", 0, "<file>"),
    ARGH_FLAG(0, "verify", &cfg.verify, "Check the collector's certificate", ARGH_NEGATABLE),

    ARGH_REST("files", &cfg.files, "Log files to send"),

    /* Shown at the end of --help; debug builds check that they parse and
     * keep to the rules below */
    ARGH_EXAMPLE("logship --to logs.example.com:6514 app.log", "Send one file as JSON"),
    ARGH_EXAMPLE("logship -t 10.0.0.5:514 -f syslog --gzip -x .gz app.log db.log", "Syslog records, gzipped, skipping archives"),
    ARGH_EXAMPLE("logship --stdin --to localhost:9000 --format raw", "Forward whatever arrives on standard input"),
    ARGH_EXAMPLE("logship --to logs.example.com:6514 --tls-key k.pem --tls-cert c.pem app.log", "With a client certificate"),
    ARGH_END
};

static const argh_rule rules[] = {
    ARGH_EXACTLY_ONE(&cfg.files, &cfg.use_stdin), /* files or stdin, not both */
    ARGH_AT_MOST_ONE(&cfg.gzip, &cfg.zstd),       /* one compression */
    ARGH_REQUIRES(&cfg.tls_key, &cfg.tls_cert),   /* key and certificate */
    ARGH_REQUIRES(&cfg.tls_cert, &cfg.tls_key),   /* come as a pair */
    ARGH_RULES_END
};

/* What rules can't say */
static bool validate(argh_parser *p, void *ctx)
{
    (void)ctx;
    if (cfg.chunk == 0)
        return argh_fail(p, "--chunk must be larger than 0");
    if (cfg.chunk > cfg.max_size)
        return argh_fail(p, "--chunk must not be larger than --max-size");
    if (cfg.retries < 1 || cfg.retries > 10)
        return argh_fail(p, "--retries must be between 1 and 10");
    return true;
}

/* ----------------------------------------------------------------------------
 * The work
 * ---------------------------------------------------------------------------- */

static bool excluded(const char *name)
{
    for (int i = 0; i < cfg.exclude.count; i++)
        if (strstr(name, cfg.exclude.items[i]))
            return true;
    return false;
}

/* Size of a file with standard C only, -1 if it can't be read */
static long file_size(const char *name)
{
    long size;
    FILE *f = fopen(name, "rb");
    if (!f)
        return -1;
    if (fseek(f, 0, SEEK_END) != 0)
    {
        fclose(f);
        return -1;
    }
    size = ftell(f);
    fclose(f);
    return size;
}

int main(int argc, char **argv)
{
    unsigned long long total = 0, requests = 0;
    int sent = 0, status = 0;
    char buf[32];

    argh_parser p;
    argh_init(&p, "logship", "Send log files to a collector (dry run: prints the plan)");
    argh_version(&p, "2.1.0");
    argh_table(&p, options);
    argh_rules(&p, rules);
    argh_set_validator(&p, validate, NULL);

    if (!argh_parse(&p, argc, argv))
        return argh_exit_code(&p);

    if (cfg.use_stdin)
    {
        printf("Would stream standard input to %s:%d\n", cfg.to.host, cfg.to.port);
        return 0;
    }

    for (int i = 0; i < cfg.files.count; i++)
    {
        const char *name = cfg.files.items[i];
        long size;

        if (excluded(name))
        {
            if (cfg.verbose)
                printf("  skip  %s (excluded)\n", name);
            continue;
        }
        size = file_size(name);
        if (size < 0)
        {
            fprintf(stderr, "logship: %s: cannot read\n", name);
            status = 1;
            continue;
        }
        if ((unsigned long long)size > cfg.max_size)
        {
            format_bytes(cfg.max_size, buf, sizeof buf);
            printf("  skip  %s (larger than %s)\n", name, buf);
            continue;
        }

        unsigned long long parts = ((unsigned long long)size + cfg.chunk - 1) / cfg.chunk;
        if (parts == 0)
            parts = 1; /* an empty file is still one request */
        if (cfg.verbose)
            printf("  send  %s: %ld bytes in %llu request%s\n", name, size, parts, parts == 1 ? "" : "s");
        total += (unsigned long long)size;
        requests += parts;
        sent++;
    }

    printf("Plan: %d file%s, %llu bytes in %llu request%s to %s:%d\n", sent, sent == 1 ? "" : "s", total,
           requests, requests == 1 ? "" : "s", cfg.to.host, cfg.to.port);
    printf("      format %s, %s, %s\n", formats[cfg.format], cfg.gzip ? "gzip" : cfg.zstd ? "zstd" : "uncompressed",
           cfg.tls_key ? (cfg.verify ? "TLS" : "TLS without certificate check") : "no TLS");

    if (cfg.verbose > 1)
    {
        format_duration(&cfg.timeout_ms, buf, sizeof buf);
        printf("      timeout %s%s, %d attempts\n", buf, argh_given(&p, &cfg.timeout_ms) ? "" : " (default)",
               cfg.retries);
    }
    return status;
}
