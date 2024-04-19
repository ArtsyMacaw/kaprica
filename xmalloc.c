#define _XOPEN_SOURCE 700
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include "xmalloc.h"
#include "clipboard.h"

void *xmalloc(size_t size)
{
    void *ptr = malloc(size);
    if (!ptr)
    {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    return ptr;
}

void *xrealloc(void *ptr, size_t size)
{
    ptr = realloc(ptr, size);
    if (!ptr)
    {
        perror("realloc");
        exit(EXIT_FAILURE);
    }
    return ptr;
}

char *xstrdup(const char *s)
{
    char *duplicate = strdup(s);
    if (!duplicate)
    {
        perror("strdup");
        exit(EXIT_FAILURE);
    }
    return duplicate;
}

/* Asks the kernel to allocate a huge page for the given size */
void *reallochugepage(void *ptr, size_t size)
{
    size_t new_size = size + (TWO_MB - (size % TWO_MB));
    void *new_ptr = aligned_alloc(TWO_MB, new_size);
    if (!new_ptr)
    {
        perror("aligned_alloc");
        free(ptr);
        return NULL;
    }

    madvise(new_ptr, TWO_MB, MADV_HUGEPAGE);
    memset(new_ptr, 0, 1);

    memcpy(new_ptr, ptr, size);
    free(ptr);

    return new_ptr;
}
