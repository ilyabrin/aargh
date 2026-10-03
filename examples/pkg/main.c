/*
 * pkg: a package manager front end in the style of cargo, npm or git.
 *
 *   $ ./pkg install left-pad --version ^1.3 --dev
 *   $ ./pkg -C ./app --offline install --dry-run
 *   $ ./pkg remote add origin https://pkgs.example.com
 *   $ ./pkg exec node --version
 *   $ ./pkg help remote add
 *
 * A dry run: commands print what they would do.
 *
 * The part that is usually painful to write by hand, and how it looks here:
 *   - nested commands with their own options, spread over several files
 *   - global options that work before and after the command name
 *   - per-command help, "pkg help <command>", suggestions for typos
 *   - a custom type for version requirements with precise error messages
 *   - rules across global and command options, a validator that knows the
 *     selected command
 *   - "pkg exec cmd --its-own-flags" passing arguments through untouched
 *   - dispatch to handlers with an application context
 */

#define ARGH_IMPLEMENTATION
#include "pkg.h"

pkg_options opt = {
    0,             /* verbosity */
    ".",           /* dir */
    NULL,          /* registry */
    false,         /* offline */
    0,             /* color: auto */
    {{NULL, 0, 0}, {'*', 0, 0, 0}, false, false, false, false}, /* install */
    {{NULL, 0, 0}, false},                                      /* remove */
    {NULL, NULL, false},                                        /* remote add */
    {NULL},                                                     /* remote remove */
    {{NULL, 0, 0}},                                             /* exec */
};

static const char *const color_modes[] = {"auto", "always", "never", NULL};

/* Options that work with every command */
static const argh_opt global_opts[] = {
    ARGH_COUNT('v', "verbose", &opt.verbosity, "More output, repeat for more"),
    ARGH_STRING('C', NULL, &opt.dir, "Run as if started in this directory", 0, "<dir>"),
    ARGH_STRING(0, "registry", &opt.registry, "Package registry URL", 0, "<url>"),
    ARGH_ENV(&opt.registry, "PKG_REGISTRY"),
    ARGH_FLAG(0, "offline", &opt.offline, "Use only the local cache"),
    ARGH_ENUM(0, "color", &opt.color, color_modes, "When to use colors"),
    /* Examples for `pkg --help`; commands have their own. Debug builds check
     * that every one of them parses, through commands and rules */
    ARGH_EXAMPLE("pkg install left-pad --version ^1.3 --dev", "Add a development dependency"),
    ARGH_EXAMPLE("pkg -C ./app --offline install", "Install the lockfile from the local cache"),
    ARGH_EXAMPLE("pkg exec node --version", "Run a program with the project's packages"),
    ARGH_END
};

static const argh_cmd commands[] = {
    ARGH_CMD("install", "Add packages to the project", install_opts, cmd_install),
    ARGH_CMD("remove", "Remove packages from the project", remove_opts, cmd_remove),
    ARGH_CMD_GROUP("remote", "Manage package sources", remote_cmds),
    ARGH_CMD("exec", "Run a program with the project's packages", exec_opts, cmd_exec, ARGH_POSIX),
    ARGH_CMD_END
};

/* Rules can mix global options and command options. A rule only applies when
 * its command is selected, so all of them live in one table. */
static const argh_rule rules[] = {
    ARGH_AT_MOST_ONE(&opt.offline, &opt.registry),
    ARGH_AT_MOST_ONE(&opt.install.dev, &opt.install.optional),
    ARGH_RULES_END
};

/* Checks that depend on which command runs */
static bool validate(argh_parser *p, void *ctx)
{
    const argh_cmd *cmd = argh_command(p);
    (void)ctx;
    if (opt.offline && cmd == &remote_cmds[0])
        return argh_fail(p, "'remote add' needs the network; drop --offline");
    if (opt.offline && cmd == &commands[0] && !opt.install.dry_run && opt.install.packages.count > 0)
        return argh_fail(p, "installing new packages needs the network; use --dry-run or drop --offline");
    return true;
}

int main(int argc, char **argv)
{
    pkg_context ctx;

    argh_parser p;
    argh_init(&p, "pkg", "A package manager (dry run: prints what it would do)");
    argh_version(&p, "0.9.0");
    argh_table(&p, global_opts);
    argh_completions(&p); /* pkg --completions bash|zsh|fish */
    argh_commands(&p, commands);
    argh_rules(&p, rules);
    argh_set_validator(&p, validate, NULL);

    if (!argh_parse(&p, argc, argv))
        return argh_exit_code(&p);

    /* Resolve global settings once; handlers only see the result */
    ctx.out = stdout;
    ctx.color = opt.color == 1; /* "auto" would check for a terminal here */
    ctx.registry = opt.offline ? "local cache" : opt.registry ? opt.registry : "https://pkgs.example.com";

    if (opt.verbosity)
        fprintf(ctx.out, "[pkg] in %s, source: %s\n", opt.dir, ctx.registry);

    return argh_run(&p, &ctx);
}
