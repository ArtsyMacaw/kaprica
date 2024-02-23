#include <stdint.h>
#include <stdbool.h>
#include "clipboard.h"

#ifndef DETECTION_H
#define DETECTION_H

void guess_mime_types(source_buffer *src);
void get_snippet(source_buffer *src);
int find_write_type(source_buffer *src);

#endif
