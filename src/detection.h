#include <stdint.h>
#include <stdbool.h>
#include "clipboard.h"

#ifndef DETECTION_H
#define DETECTION_H

void guess_mime_types(source_buffer *src);
void get_snippet(source_buffer *src);
void get_thumbnail(source_buffer *src);
uint8_t find_write_type(source_buffer *src);
bool is_minimum_length(source_buffer *src, size_t min_length);

#endif
