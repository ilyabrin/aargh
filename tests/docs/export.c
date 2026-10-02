/* README.md, Rules between options: the rules and the validator as shown,
 * with the options they refer to */
#define ARGH_IMPLEMENTATION
#include "argh.h"

static bool json, yaml, csv, use_stdin;
static const char *input, *tls_key, *tls_cert;
static int min_size, max_size = 100;

static const argh_rule rules[] = {
    ARGH_AT_MOST_ONE(&json, &yaml, &csv),    /* one output format */
    ARGH_EXACTLY_ONE(&input, &use_stdin),    /* one input source */
    ARGH_REQUIRES(&tls_key, &tls_cert),      /* a key needs its certificate */
    ARGH_RULES_END
};

static bool check_sizes(argh_parser *p, void *ctx)
{
    if (min_size > max_size)
        return argh_fail(p, "--min-size must not be greater than --max-size");
    return true;
}

int main(int argc, char **argv)
{
    argh_parser p;
    argh_init(&p, "export", NULL);
    argh_flag(&p, 0, "json", &json, "");
    argh_flag(&p, 0, "yaml", &yaml, "");
    argh_flag(&p, 0, "csv", &csv, "");
    argh_string(&p, 0, "input", &input, "");
    argh_flag(&p, 0, "stdin", &use_stdin, "");
    argh_string(&p, 0, "tls-key", &tls_key, "");
    argh_string(&p, 0, "tls-cert", &tls_cert, "");
    argh_int(&p, 0, "min-size", &min_size, "");
    argh_int(&p, 0, "max-size", &max_size, "");

    argh_rules(&p, rules);

    argh_set_validator(&p, check_sizes, NULL);
    return argh_parse(&p, argc, argv) ? 0 : argh_exit_code(&p);
}
