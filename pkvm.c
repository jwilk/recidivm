/* Copyright © 2015 Jakub Wilk <jwilk@jwilk.net>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the “Software”), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <assert.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

void usage(bool explicit)
{
    fprintf(stderr, "Usage: pkvm [-v] <command> [argument...]\n");
    if (!explicit)
        exit(1);
    fprintf(stderr, "\n"
        "Options:\n"
        "  -h    display this help and exit\n"
        "  -v    be verbose\n"
    );
    exit(0);
}

int main(int argc, char **argv)
{
    int rc;
    bool opt_verbose = false;
    while (1) {
        int opt = getopt(argc, argv, "hv");
        if (opt == -1)
            break;
        switch (opt) {
        case 'h':
            usage(true);
        case 'v':
            opt_verbose = true;
            break;
        case '?':
            usage(false);
        default:
            assert("unexpected getopt(3) return value" == NULL);
        }
    }
    if (optind >= argc)
        usage(false);
    struct rlimit limit;
    rc = getrlimit(RLIMIT_AS, &limit);
    if (rc) {
        perror("pkvm: getrlimit");
        return 1;
    }
    rlim_t l = 0;
    rlim_t r = limit.rlim_max;
#if defined(__i386__) || defined(__x86_64__)
    /* On x86(-64) size of rlim_t can be 64 bits, even though the address space
     * is only 48 bits. */
    if (sizeof (rlim_t) > 6) {
        rlim_t rmax = (rlim_t)1 << 24 << 24;
        if (r > rmax)
            r = rmax;
    }
#endif
    assert(r > l);
    while (l < r) {
        rlim_t m = l + (r - l) / 2;
        switch (fork()) {
        case -1:
            perror("pkvm: fork");
            return 1;
        case 0:
            /* child */
            limit.rlim_cur = limit.rlim_max = m;
            rc = setrlimit(RLIMIT_AS, &limit);
            if (rc) {
                /* Something went very, very wrong. */
                perror("pkvm: setrlimit");
                kill(getppid(), SIGABRT);
                return 1;
            }
            execvp(argv[optind], argv + optind);
            perror("pkvm: execvp");
            return 1;
        default:
            {
                /* parent */
                int status;
                pid_t pid = wait(&status);
                if (pid < 0) {
                    perror("pkvm: wait");
                    return 1;
                }
                if (opt_verbose) {
                    fprintf(stderr, "pkvm: %ju -> ", (uintmax_t) m);
                    if (status == 0)
                        fprintf(stderr, "ok");
                    else if (WIFEXITED(status))
                        fprintf(stderr, "exit code %d", WEXITSTATUS(status));
                    else if (WIFSIGNALED(status)) {
                        int termsig = WTERMSIG(status);
                        fprintf(stderr, "signal %d (%s)", termsig, strsignal(termsig));
                    } else
                        assert("unexpected wait(2) status" == NULL);
                    fprintf(stderr, "\n");
                }
                if (status == 0)
                    r = m;
                else
                    l = m + 1;
                break;
            }
        }
    }
    printf("%ju\n", (uintmax_t) l);
    return 0;
}

/* vim:set ts=4 sts=4 sw=4 et:*/
