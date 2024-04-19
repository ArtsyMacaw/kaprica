#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef XMALLOC_H
#define XMALLOC_H

void *xmalloc(size_t size);
void *xrealloc(void *ptr, size_t size);
char *xstrdup(const char *s);
void *reallochugepage(void *ptr, size_t size);

#endif
