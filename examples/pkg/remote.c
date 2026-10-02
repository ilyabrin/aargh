/*
 * pkg remote add / remove / list: a command group with three subcommands.
 */

#include "pkg.h"

static int remote_add(argh_parser *p, void *user)
{
    pkg_context *ctx = (pkg_context *)user;
    (void)p;
    fprintf(ctx->out, "%s remote '%s' -> %s\n", opt.remote_add.force ? "Replaced" : "Added", opt.remote_add.name,
            opt.remote_add.url);
    return 0;
}

static int remote_remove(argh_parser *p, void *user)
{
    pkg_context *ctx = (pkg_context *)user;
    (void)p;
    fprintf(ctx->out, "Removed remote '%s'\n", opt.remote_remove.name);
    return 0;
}

static int remote_list(argh_parser *p, void *user)
{
    pkg_context *ctx = (pkg_context *)user;
    (void)p;
    fprintf(ctx->out, "origin    %s\n", ctx->registry);
    if (opt.verbosity)
        fprintf(ctx->out, "mirror    https://mirror.example.org (read-only)\n");
    return 0;
}

static const argh_opt add_opts[] = {
    ARGH_FLAG('f', "force", &opt.remote_add.force, "Replace a remote with the same name"),
    ARGH_POS("name", &opt.remote_add.name, "Short name, such as origin"),
    ARGH_POS("url", &opt.remote_add.url, "Where the packages come from"),
    ARGH_EXAMPLE("pkg remote add origin https://pkgs.example.com", NULL),
    ARGH_EXAMPLE("pkg remote add -f mirror https://mirror.example.com", "Replace an existing source"),
    ARGH_END
};

static const argh_opt remove_opts_[] = {
    ARGH_POS("name", &opt.remote_remove.name, "Remote to remove"),
    ARGH_END
};

const argh_cmd remote_cmds[] = {
    ARGH_CMD("add", "Add a package source", add_opts, remote_add),
    ARGH_CMD("remove", "Remove a package source", remove_opts_, remote_remove),
    ARGH_CMD("list", "Show package sources", NULL, remote_list),
    ARGH_CMD_END
};
