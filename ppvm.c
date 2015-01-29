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
#include <errno.h>
#include <fcntl.h>
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
    fprintf(stderr, "Usage: ppvm [-cpv] <command> [argument...]\n");
    if (!explicit)
        exit(1);
    fprintf(stderr, "\n"
        "Options:\n"
        "  -h    display this help and exit\n"
        "  -c    capture stdin; provide fresh copy of it to every child\n"
        "  -p    don't redirect target program stdout and stderr to /dev/null\n"
        "  -v    be verbose\n"
    );
    exit(0);
}

void flush_stdout(void)
{
    int rc;
    if (ferror(stdout)) {
        fclose(stdout);
        rc = EOF;
        errno = EIO;
    } else
        rc = fclose(stdout);
    if (rc == EOF) {
        perror("ppvm: /dev/stdout");
        exit(1);
    }
}

void fatal_child(void)
{
    /* Something went very, very wrong. */
    perror("ppvm: setrlimit");
    kill(getppid(), SIGABRT);
    exit(1);
}

int child(char **argv, rlim_t m, int infd, int outfd)
{
    struct rlimit limit = {m, m};
    if (infd >= 0) {
        int fd = dup2(infd, STDIN_FILENO);
        if (fd == -1)
            fatal_child();
    }
    if (outfd >= 0) {
        int fd;
        fd = dup2(outfd, STDOUT_FILENO);
        if (fd == -1)
            fatal_child();
        fd = dup2(outfd, STDERR_FILENO);
        if (fd == -1)
            fatal_child();
    }
    int rc = setrlimit(RLIMIT_AS, &limit);
    if (rc)
        fatal_child();
    execvp(argv[0], argv);
    perror("ppvm: execvp");
    return 1;
}

int main(int argc, char **argv)
{
    int rc;
    bool opt_verbose = false;
    bool opt_capture_stdin = false;
    bool opt_print = false;
    int nullfd = -1;
    int infd = -1;
    int outfd = -1;
    while (1) {
        int opt = getopt(argc, argv, "hcpv");
        if (opt == -1)
            break;
        switch (opt) {
        case 'h':
            usage(true);
        case 'c':
            opt_capture_stdin = true;
            break;
        case 'p':
            opt_print = true;
            break;
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
    nullfd = open("/dev/null", O_RDWR);
    if (nullfd == -1) {
        perror("ppvm: /dev/null");
        return 1;
    }
    if (opt_capture_stdin) {
        char buffer[BUFSIZ];
        char path[] = "/tmp/ppvm.XXXXXX";
        infd = mkstemp(path);
        if (infd == -1) {
            perror("ppvm: mkstemp");
            return 1;
        }
        ssize_t i;
        while ((i = read(STDIN_FILENO, buffer, sizeof buffer))) {
            if (i == -1) {
                perror("ppvm: /dev/stdin");
                return 1;
            }
            ssize_t j = write(infd, buffer, i);
            if (j == -1) {
                fprintf(stderr, "ppvm: %s: %s\n", path, strerror(errno));
                return 1;
            } else if (i != j) {
                assert(j < i);
                fprintf(stderr, "ppvm: %s: short write\n", path);
                return 1;
            }
        }
        int rc = unlink(path);
        if (rc == -1) {
            fprintf(stderr, "ppvm: %s: %s\n", path, strerror(errno));
            return 1;
        }
    } else
        infd = nullfd;
    if (!opt_print)
        outfd = nullfd;
    struct rlimit limit;
    rc = getrlimit(RLIMIT_AS, &limit);
    if (rc) {
        perror("ppvm: getrlimit");
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
        off_t off = lseek(infd, 0, SEEK_SET);
        if (off == -1) {
            perror("ppvm: child's stdin");
            return 1;
        }
        switch (fork()) {
        case -1:
            perror("ppvm: fork");
            return 1;
        case 0:
            /* child */
            return child(argv + optind, m, infd, outfd);
        default:
            {
                /* parent */
                int status;
                pid_t pid = wait(&status);
                if (pid < 0) {
                    perror("ppvm: wait");
                    return 1;
                }
                if (opt_verbose) {
                    fprintf(stderr, "ppvm: %ju -> ", (uintmax_t) m);
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
    flush_stdout();
    return 0;
}

/* vim:set ts=4 sts=4 sw=4 et:*/
