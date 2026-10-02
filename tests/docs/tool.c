/* README.md, Commands: the code as shown, with the includes it leaves out */
#include <stdio.h>

#define ARGH_IMPLEMENTATION
#include "argh.h"

static bool verbose, release, force;
static const char *name, *url;

static int build(argh_parser *p, void *app)
{
    printf("building%s\n", release ? " (release)" : "");
    return 0;
}

static const argh_opt build_opts[] = {
    ARGH_FLAG('r', "release", &release, "Optimized build"),
    ARGH_END
};

static const argh_opt add_opts[] = {
    ARGH_FLAG('f', "force", &force, "Overwrite an existing remote"),
    ARGH_POS("name", &name, "Remote name"),
    ARGH_POS("url", &url, "Remote URL"),
    ARGH_END
};

static const argh_cmd remote_cmds[] = {
    ARGH_CMD("add", "Add a remote", add_opts),
    ARGH_CMD("list", "List remotes", NULL),
    ARGH_CMD_END
};

static const argh_cmd commands[] = {
    ARGH_CMD("build", "Build the project", build_opts, build),
    ARGH_CMD_GROUP("remote", "Manage remotes", remote_cmds),
    ARGH_CMD_END
};

int main(int argc, char **argv)
{
    argh_parser p;
    argh_init(&p, "tool", "Builds things");
    argh_flag(&p, 'v', "verbose", &verbose, "Verbose output");   /* a global option */
    argh_commands(&p, commands);

    if (!argh_parse(&p, argc, argv))
        return argh_exit_code(&p);

    if (argh_command(&p) == &remote_cmds[0])
        printf("adding %s -> %s\n", name, url);
    return argh_run(&p, NULL);   /* calls the handler of the selected command, if any */
}
