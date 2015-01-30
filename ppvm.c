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
    fprintf(stderr, "Usage: ppvm [-cpv] [-u B|K|M] -- <command> [argument...]\n");
    if (!explicit)
        exit(1);
    fprintf(stderr, "\n"
        "Options:\n"
        "  -c    capture stdin\n"
        "  -p    don't redirect stdout and stderr\n"
        "  -u B  use byte as unit (default)\n"
        "  -u K  use kilobyte as unit\n"
        "  -u M  use megabyte as unit\n"
        "  -v    be verbose\n"
        "  -h    display this help and exit\n"
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

void fatal_child(const char *func)
{
    /* Something went very, very wrong. */
    fprintf(stderr, "ppvm: %s: %s\n", func, strerror(errno));
    kill(getppid(), SIGABRT);
    exit(1);
}

int child(char **argv, rlim_t m, int infd, int outfd)
{
    struct rlimit limit = {m, m};
    int rc = setrlimit(RLIMIT_AS, &limit);
    if (rc)
        fatal_child("setrlimit");
    if (infd >= 0) {
        int fd = dup2(infd, STDIN_FILENO);
        if (fd == -1)
            fatal_child("dup2");
    }
    if (outfd >= 0) {
        int fd;
        fd = dup2(outfd, STDOUT_FILENO);
        if (fd == -1)
            fatal_child("dup2");
        fd = dup2(outfd, STDERR_FILENO);
        if (fd == -1)
            fatal_child("dup2");
    }
    execvp(argv[0], argv);
    perror("ppvm: execvp()");
    return 1;
}

int capture_stdin(void)
{
    const char *tmptemplate = "ppvm.XXXXXX";
    const char *tmpdir = getenv("TMPDIR");
    char *tmppath;
    if (tmpdir == NULL)
        tmpdir = "/tmp";
    tmppath = malloc(
        strlen(tmpdir) +
        1 + /* slash */
        strlen(tmptemplate) +
        1 /* null byte */
    );
    if (tmppath == NULL) {
        perror("ppvm: malloc()");
        exit(1);
    }
    sprintf(tmppath, "%s/%s", tmpdir, tmptemplate);
    int fd = mkstemp(tmppath);
    if (fd == -1) {
        fprintf(stderr, "ppvm: %s: %s\n", tmppath, strerror(errno));
        exit(1);
    }
    char buffer[BUFSIZ];
    ssize_t i;
    while ((i = read(STDIN_FILENO, buffer, sizeof buffer))) {
        if (i == -1) {
            perror("ppvm: /dev/stdin");
            exit(1);
        }
        ssize_t j = write(fd, buffer, i);
        if (j == -1) {
            fprintf(stderr, "ppvm: %s: %s\n", tmppath, strerror(errno));
            exit(1);
        } else if (i != j) {
            assert(j < i);
            fprintf(stderr, "ppvm: %s: short write\n", tmppath);
            exit(1);
        }
    }
    int rc = unlink(tmppath);
    if (rc == -1) {
        fprintf(stderr, "ppvm: %s: %s\n", tmppath, strerror(errno));
        exit(1);
    }
    free(tmppath);
    return fd;
}

rlim_t roundto(rlim_t n, int unit)
{
    assert(n > 0);
    assert(unit > 0);
    return ((n - 1) | (unit - 1)) + 1;
}

int main(int argc, char **argv)
{
    int rc;
    bool opt_verbose = false;
    bool opt_capture_stdin = false;
    bool opt_print = false;
    int opt_unit = 1;
    int nullfd = -1;
    int infd = -1;
    int outfd = -1;
    while (1) {
        int opt = getopt(argc, argv, "hcpu:v");
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
        case 'u':
            {
                char optchar = '?';
                if (strlen(optarg) == 1)
                    optchar = optarg[0];
                opt_unit = 1;
                switch (optchar)
                {
                case 'm':
                case 'M':
                    opt_unit *= 1024;
                case 'k':
                case 'K':
                    opt_unit *= 1024;
                case 'b':
                case 'B':
                    break;
                default:
                    fprintf(stderr, "ppvm: unit must be B, K or M, not %s\n", optarg);
                    return 1;
                }
            }
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
    infd = opt_capture_stdin
        ? capture_stdin()
        : nullfd;
    if (!opt_print)
        outfd = nullfd;
    struct rlimit limit;
    rc = getrlimit(RLIMIT_AS, &limit);
    if (rc) {
        perror("ppvm: getrlimit()");
        return 1;
    }
    rlim_t l = 1;
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
    while (roundto(l, opt_unit) < roundto(r, opt_unit)) {
        rlim_t m = l + (r - l) / 2;
        off_t off = lseek(infd, 0, SEEK_SET);
        if (off == -1) {
            perror("ppvm: captured stdin");
            return 1;
        }
        switch (fork()) {
        case -1:
            perror("ppvm: fork()");
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
                    perror("ppvm: wait()");
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
    printf("%ju\n", (uintmax_t) (roundto(l, opt_unit) / opt_unit));
    flush_stdout();
    return 0;
}

/* vim:set ts=4 sts=4 sw=4 et:*/
