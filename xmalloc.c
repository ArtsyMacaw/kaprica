#define _XOPEN_SOURCE 700
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "xmalloc.h"

void *xmalloc(size_t size)
{
    void *ptr = malloc(size);
    if (!ptr)
    {
        fprintf(stderr, "Failed to allocate memory\n");
        exit(1);
    }
    return ptr;
}

void *xrealloc(void *ptr, size_t size)
{
    ptr = realloc(ptr, size);
    if (!ptr)
    {
        fprintf(stderr, "Failed to allocate memory\n");
        exit(1);
    }
    return ptr;
}

char *xstrdup(const char *s)
{
    char *duplicate = strdup(s);
    if (!duplicate)
    {
        fprintf(stderr, "Failed to allocate memory\n");
        exit(1);
    }
    return duplicate;
}
