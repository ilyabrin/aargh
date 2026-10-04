/* Parsing in many threads at once, for ThreadSanitizer (`make thread-check`).
 *
 * argh has no global or static state that changes, so parsers in different
 * threads never touch the same memory. Each thread here has its own parser
 * and variables and shares only static const data, the way a program would;
 * it parses values, prints help, reads the environment and generates a
 * completion script. TSan fails the run on any data race, and every thread
 * checks the values it got. */
#include <pthread.h>
#include <stdio.h>

#define ARGH_IMPLEMENTATION
#include "../argh.h"

#define THREADS 8
#define ROUNDS 2000

static const char *const modes[] = {"fast", "safe", NULL};

static void count_output(void *ctx, bool to_stderr, const char *text, size_t len)
{
    size_t *written = (size_t *)ctx;
    (void)to_stderr;
    (void)text;
    *written += len;
}

static void *work(void *arg)
{
    long id = (long)arg;
    long *failures = (long *)calloc(1, sizeof(long));
    int i;
    for (i = 0; i < ROUNDS; i++)
    {
        int jobs = 0, mode = 0;
        bool verbose = false;
        const char *out = NULL;
        size_t written = 0;
        char number[24];
        char *values[] = {"t", "-j", number, "--mode", "safe", "-v", "-o", "x", NULL};
        char *help[] = {"t", "--help", NULL};
        char *script[] = {"t", "--completions", "fish", NULL};
        char **argv = i % 3 == 0 ? values : i % 3 == 1 ? help : script;
        int argc = i % 3 == 0 ? 8 : i % 3 == 1 ? 2 : 3;
        argh_parser p;
        bool ok;

        snprintf(number, sizeof(number), "%ld", id * ROUNDS + i);
        argh_init(&p, "t", "Parsing in threads");
        argh_set_writer(&p, count_output, &written);
        argh_int(&p, 'j', "jobs", &jobs, "Jobs");
        argh_range(&p, &jobs, 0, THREADS * ROUNDS);
        argh_enum(&p, 'm', "mode", &mode, modes, "Mode");
        argh_flag(&p, 'v', "verbose", &verbose, "Verbose");
        argh_string(&p, 'o', "output", &out, "Output");
        argh_env(&p, &jobs, "THREAD_CHECK_JOBS");
        argh_completions(&p);
        ok = argh_parse(&p, argc, argv);

        if (argv == values)
            *failures += !ok || jobs != id * ROUNDS + i || mode != 1 || !verbose || !out;
        else
            *failures += ok || argh_exit_code(&p) != 0 || written == 0;
    }
    return failures;
}

int main(void)
{
    pthread_t threads[THREADS];
    long failures = 0;
    long i;
    for (i = 0; i < THREADS; i++)
        pthread_create(&threads[i], NULL, work, (void *)i);
    for (i = 0; i < THREADS; i++)
    {
        void *result;
        pthread_join(threads[i], &result);
        failures += *(long *)result;
        free(result);
    }
    printf("%d threads x %d parses: %ld wrong\n", THREADS, ROUNDS, failures);
    return failures ? 1 : 0;
}
