#define _XOPEN_SOURCE 700
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef XMALLOC_H
#define XMALLOC_H

void *xmalloc(size_t size);
void *xrealloc(void *ptr, size_t size);
char *xstrdup(const char *s);

#endif
