#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "xmalloc.h"

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
