// Compile-only check: argh.h works from C++ (tables, builder, implementation).
#define ARGH_IMPLEMENTATION
#include "../argh.h"

static bool verbose;
static int jobs = 4;
static const char *output = "out.txt";
static const char *const modes[] = {"fast", "safe", nullptr};
static int mode;

static const char *parse_level(const char *text, void *target)
{
    *static_cast<int *>(target) = text[0] - '0';
    return nullptr;
}
static const argh_type level_type = {"<level>", parse_level, nullptr};
static int level;

static const argh_opt opts[] = {
    ARGH_FLAG('v', "verbose", &verbose, "Verbose output"),
    ARGH_INT('j', "jobs", &jobs, "Parallel jobs"),
    ARGH_STRING('o', "output", &output, "Output file", ARGH_REQUIRED),
    ARGH_ENUM('m', "mode", &mode, modes, "Mode"),
    ARGH_CUSTOM('l', "level", &level, &level_type, "Level"),
    ARGH_EXAMPLE("cxx --ratio 2 build -o out.txt -j 2", "An example"),
    ARGH_END,
};

static int run_build(argh_parser *, void *) { return 0; }

static const argh_rule rules[] = {
    ARGH_AT_MOST_ONE(&verbose, &level),
    ARGH_REQUIRES(&output, &jobs),
    ARGH_RULES_END,
};

static bool validate(argh_parser *p, void *) { return jobs > 0 || argh_fail(p, "jobs must be positive"); }

static const argh_cmd sub_cmds[] = {
    ARGH_CMD("add", "Add", nullptr),
    ARGH_CMD_END,
};

static const argh_cmd cmds[] = {
    ARGH_CMD("build", "Build", opts, run_build),
    ARGH_CMD_GROUP("remote", "Remotes", sub_cmds),
    ARGH_CMD_END,
};

int main(int argc, char **argv)
{
    double ratio = 1.0;
    argh_parser p;
    argh_init(&p, "cxx", nullptr);
    argh_required(argh_double(&p, 'r', "ratio", &ratio, "Ratio"));
    argh_commands(&p, cmds);
    argh_rules(&p, rules);
    argh_set_validator(&p, validate, nullptr);
    if (!argh_parse(&p, argc, argv))
        return argh_exit_code(&p);
    return argh_run(&p, nullptr);
}
