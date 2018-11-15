/* Copyright © 2015-2018 Jakub Wilk <jwilk@jwilk.net>
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

#ifndef RLIMIT_AS
#define RLIMIT_AS RLIMIT_DATA
#endif

static void usage(FILE *fp)
{
    fprintf(fp, "Usage: recidivm [-cpv] [-u B|K|M] -- <command> [argument...]\n");
    if (fp == stderr)
        exit(1);
    fprintf(fp, "\n"
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

static void flush_stdout(void)
{
    int rc;
    if (ferror(stdout)) {
        fclose(stdout);
        rc = EOF;
        errno = EIO;
    } else
        rc = fclose(stdout);
    if (rc == EOF) {
        perror("recidivm: /dev/stdout");
        exit(1);
    }
}

static void fatal_child(const char *func)
{
    /* Something went very, very wrong. */
    fprintf(stderr, "recidivm: %s: %s\n", func, strerror(errno));
    kill(getppid(), SIGABRT);
    exit(1);
}

static int child(char **argv, rlim_t m, int infd, int outfd)
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
    perror("recidivm: execvp()");
    return 1;
}

static int capture_stdin(void)
{
    const char *tmptemplate = "recidivm.XXXXXX";
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
        perror("recidivm: malloc()");
        exit(1);
    }
    sprintf(tmppath, "%s/%s", tmpdir, tmptemplate);
    int fd = mkstemp(tmppath);
    if (fd == -1) {
        fprintf(stderr, "recidivm: %s: %s\n", tmppath, strerror(errno));
        exit(1);
    }
    char buffer[BUFSIZ];
    ssize_t i;
    while ((i = read(STDIN_FILENO, buffer, sizeof buffer))) {
        if (i == -1) {
            perror("recidivm: /dev/stdin");
            exit(1);
        }
        ssize_t j = write(fd, buffer, (size_t) i);
        if (j == -1) {
            fprintf(stderr, "recidivm: %s: %s\n", tmppath, strerror(errno));
            exit(1);
        } else if (i != j) {
            assert(j < i);
            fprintf(stderr, "recidivm: %s: short write\n", tmppath);
            exit(1);
        }
    }
    int rc = unlink(tmppath);
    if (rc == -1) {
        fprintf(stderr, "recidivm: %s: %s\n", tmppath, strerror(errno));
        exit(1);
    }
    free(tmppath);
    return fd;
}

static rlim_t roundto(rlim_t n, rlim_t unit)
{
    assert(n > 0);
    assert(unit > 0);
    rlim_t m = ((n - 1) | (unit - 1)) + 1;
    if (m)
        return m;
    /* Ooops, an integer overflow happened.
     * The result won't be accurate in that case, but oh well. */
    return (rlim_t) -1;
}

static const char * get_signal_name(int sig)
{
    switch (sig) {
#define s(n) case n: return "" # n "";
    /* POSIX.1-1990: */
    s(SIGHUP);
    s(SIGINT);
    s(SIGQUIT);
    s(SIGILL);
    s(SIGABRT);
    s(SIGFPE);
    s(SIGKILL);
    s(SIGSEGV);
    s(SIGPIPE);
    s(SIGALRM);
    s(SIGTERM);
    s(SIGUSR1);
    s(SIGUSR2);
    s(SIGCHLD);
    s(SIGCONT);
    s(SIGSTOP);
    s(SIGTSTP);
    s(SIGTTIN);
    s(SIGTTOU);
    /* SUSv2 and POSIX.1-2001: */
    s(SIGBUS);
#ifdef SIGPOLL
    /* not supported on OpenBSD */
    s(SIGPOLL);
#endif
    s(SIGPROF);
    s(SIGSYS);
    s(SIGTRAP);
    s(SIGURG);
    s(SIGVTALRM);
    s(SIGXCPU);
    s(SIGXFSZ);
#undef s
    default:
        return NULL;
    }
}

int main(int argc, char **argv)
{
    int rc;
    bool opt_verbose = false;
    bool opt_capture_stdin = false;
    bool opt_print = false;
    rlim_t opt_unit = 1;
    int nullfd = -1;
    int infd = -1;
    int outfd = -1;
    long lstep = sysconf(_SC_PAGESIZE);
    if (lstep < 0) {
        perror("recidivm: sysconf(_SC_PAGESIZE)");
        return 1;
    }
    rlim_t step = (rlim_t) lstep;
    while (1) {
        int opt = getopt(argc, argv, "+hcpu:v");
        if (opt == -1)
            break;
        switch (opt) {
        case 'h':
            usage(stdout);
            break;
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
                    opt_unit *= 1024; /* fall through */
                case 'k':
                case 'K':
                    opt_unit *= 1024; /* fall through */
                case 'b':
                case 'B':
                    break;
                default:
                    fprintf(stderr, "recidivm: unit must be B, K or M, not %s\n", optarg);
                    return 1;
                }
            }
            break;
        case 'v':
            opt_verbose = true;
            break;
        case '?':
            usage(stderr);
            break;
        default:
            assert("unexpected getopt(3) return value" == NULL);
        }
    }
    if (optind >= argc)
        usage(stderr);
    nullfd = open("/dev/null", O_RDWR);
    if (nullfd == -1) {
        perror("recidivm: /dev/null");
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
        perror("recidivm: getrlimit()");
        return 1;
    }
    rlim_t l = 1;
    rlim_t r = limit.rlim_max;
    /* Whether or not a limit can be represented as rlim_t is
     * implementation-defined. Hopefully using any number smaller than
     * RLIM_INFINITY and RLIM_SAVED_MAX and RLIM_SAVED_CUR should be okay. */
    if (r > RLIM_INFINITY)
        r = RLIM_INFINITY;
    if (r > RLIM_SAVED_MAX)
        r = RLIM_SAVED_MAX;
    if (r > RLIM_SAVED_CUR)
        r = RLIM_SAVED_CUR;
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
    if (opt_unit > step)
        step = opt_unit;
    while (roundto(l, step) < roundto(r, step)) {
        rlim_t m;
        if (sizeof (rlim_t) > 6 && l == 1 && (r >> 30 >> 16))
            /* slightly above the typical limit for 64-bit ASan-ed programs */
            m = (rlim_t)1 << 30 << 15;
        else
            m = l + (r - l) / 2;
        off_t off = lseek(infd, 0, SEEK_SET);
        if (off == -1) {
            perror("recidivm: captured stdin");
            return 1;
        }
        if (opt_verbose) {
            fprintf(stderr, "recidivm: %ju -> ", (uintmax_t) m);
            fflush(stderr);
        }
        switch (fork()) {
        case -1:
            perror("recidivm: fork()");
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
                    perror("recidivm: wait()");
                    return 1;
                }
                if (opt_verbose) {
                    if (status == 0)
                        fprintf(stderr, "ok");
                    else if (WIFEXITED(status))
                        fprintf(stderr, "exit status %d", WEXITSTATUS(status));
                    else if (WIFSIGNALED(status)) {
                        int termsig = WTERMSIG(status);
                        const char *signame = get_signal_name(termsig);
                        if (signame)
                            fprintf(stderr, "%s", signame);
                        else
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
    printf("%ju\n", (uintmax_t) (roundto(l, step) / opt_unit));
    flush_stdout();
    return 0;
}

/* vim:set ts=4 sts=4 sw=4 et:*/
