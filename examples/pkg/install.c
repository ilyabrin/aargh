/*
 * pkg install / pkg remove, and the version requirement type.
 */

#include <stdlib.h>
#include <string.h>

#include "pkg.h"

/* ----------------------------------------------------------------------------
 * Version requirements: "1.2.3", "^1.2", "~1.4.0", ">=2.0.0", "*"
 * ---------------------------------------------------------------------------- */

/* Reads one version component; returns the text after it, or NULL */
static const char *read_number(const char *s, int *out)
{
    long v;
    char *end;
    if (*s < '0' || *s > '9')
        return NULL;
    if (s[0] == '0' && s[1] >= '0' && s[1] <= '9')
        return NULL; /* no leading zeros, as in semver */
    v = strtol(s, &end, 10);
    if (v > 99999)
        return NULL;
    *out = (int)v;
    return end;
}

static const char *parse_version_req(const char *text, void *target)
{
    version_req req = {'=', 0, 0, 0};
    const char *s = text;

    if (strcmp(text, "*") == 0 || strcmp(text, "latest") == 0)
    {
        req.op = '*';
        *(version_req *)target = req;
        return NULL;
    }
    if (*s == '^' || *s == '~')
        req.op = *s++;
    else if (s[0] == '>' && s[1] == '=')
    {
        req.op = '>';
        s += 2;
    }

    if (!(s = read_number(s, &req.major)))
        return "expected a version like 1.2.3, ^1.2, ~1.4.0, >=2.0.0 or *";
    if (*s == '.' && !(s = read_number(s + 1, &req.minor)))
        return "each part of a version must be a number without leading zeros";
    if (*s == '.' && !(s = read_number(s + 1, &req.patch)))
        return "each part of a version must be a number without leading zeros";
    if (*s)
        return "expected a version like 1.2.3, ^1.2, ~1.4.0, >=2.0.0 or *";

    *(version_req *)target = req;
    return NULL;
}

/* Also used by help to show the default */
static bool format_version_req(const void *target, char *buf, size_t size)
{
    const version_req *req = (const version_req *)target;
    const char *prefix = req->op == '>' ? ">=" : req->op == '^' ? "^" : req->op == '~' ? "~" : "";
    if (req->op == '*')
        snprintf(buf, size, "latest");
    else
        snprintf(buf, size, "%s%d.%d.%d", prefix, req->major, req->minor, req->patch);
    return true;
}

void print_version_req(FILE *out, const version_req *req)
{
    char buf[48];
    format_version_req(req, buf, sizeof buf);
    fputs(buf, out);
}

const argh_type version_req_type = {"<version>", parse_version_req, format_version_req};

/* ----------------------------------------------------------------------------
 * pkg install
 * ---------------------------------------------------------------------------- */

const argh_opt install_opts[] = {
    ARGH_CUSTOM(0, "version", &opt.install.version, &version_req_type, "Version to install"),
    ARGH_FLAG('D', "dev", &opt.install.dev, "Only needed for development"),
    ARGH_FLAG('O', "optional", &opt.install.optional, "Install when available, skip on failure"),
    ARGH_FLAG('n', "dry-run", &opt.install.dry_run, "Show what would change"),
    ARGH_FLAG(0, "trace-resolve", &opt.install.trace, "Log every resolver step", ARGH_HIDDEN),
    ARGH_REST("packages", &opt.install.packages, "Packages to add; none installs the lockfile"),
    ARGH_EXAMPLE("pkg install", "Install everything in the lockfile"),
    ARGH_EXAMPLE("pkg install left-pad lodash --version '>=4 <5'", "Add two packages, one with a version range"),
    ARGH_END
};

int cmd_install(argh_parser *p, void *user)
{
    pkg_context *ctx = (pkg_context *)user;
    const char *kind = opt.install.dev ? " (dev)" : opt.install.optional ? " (optional)" : "";
    (void)p;

    if (opt.install.packages.count == 0)
    {
        fprintf(ctx->out, "%s packages from the lockfile, source: %s\n",
                opt.install.dry_run ? "Would install" : "Installing", ctx->registry);
        return 0;
    }
    for (int i = 0; i < opt.install.packages.count; i++)
    {
        fprintf(ctx->out, "%s %s ", opt.install.dry_run ? "would add" : "+", opt.install.packages.items[i]);
        print_version_req(ctx->out, &opt.install.version);
        fprintf(ctx->out, "%s\n", kind);
    }
    if (opt.install.trace)
        fprintf(ctx->out, "[resolver] %d roots, source %s\n", opt.install.packages.count, ctx->registry);
    return 0;
}

/* ----------------------------------------------------------------------------
 * pkg remove
 * ---------------------------------------------------------------------------- */

const argh_opt remove_opts[] = {
    ARGH_FLAG(0, "purge", &opt.remove.purge, "Also delete cached downloads"),
    ARGH_REST("packages", &opt.remove.packages, "Packages to remove", ARGH_REQUIRED),
    ARGH_END
};

int cmd_remove(argh_parser *p, void *user)
{
    pkg_context *ctx = (pkg_context *)user;
    (void)p;
    for (int i = 0; i < opt.remove.packages.count; i++)
        fprintf(ctx->out, "- %s%s\n", opt.remove.packages.items[i], opt.remove.purge ? " (and its cache)" : "");
    return 0;
}
