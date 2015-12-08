#include <stdlib.h>
#include <stdio.h>

void *xmalloc(size_t n, const char *file, int line) {
    void *mem = malloc(n);
    if (!mem) {
        perror("malloc");
        fprintf(stderr, "Error allocating memory -> %s:%d\n", file, line);
        exit(EXIT_FAILURE);
    }
    return mem;
}
