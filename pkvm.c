#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

static inline rlim_t middle(rlim_t l, rlim_t r)
{
    /* Compute (l + r) / 2 without overflow: */
    return (
        l / 2 +
        r / 2 +
        (l & r & 1)
    );
}

int main(int argc, char **argv)
{
    if (argc <= 1) {
        fprintf(stderr, "Usage: pkvm <command> [argument...]\n");
        return 1;
    }
    int rc;
    struct rlimit limit;
    rc = getrlimit(RLIMIT_AS, &limit);
    if (rc) {
        perror("pkvm: getrlimit");
        return 1;
    }
    rlim_t l = 0;
    rlim_t r = limit.rlim_max;
    assert(r > l);
    while (l < r) {
        rlim_t m = middle(l, r);
        switch (fork()) {
        case -1:
            perror("pkvm: fork");
            return 1;
        case 0:
            /* child */
            limit.rlim_cur = limit.rlim_max = m;
            rc = setrlimit(RLIMIT_AS, &limit);
            if (rc) {
                perror("pkvm: setrlimit");
                return 1;
            }
            execvp(argv[1], argv + 1);
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
