#define _POSIX_C_SOURCE 200112L
#define _XOPEN_SOURCE 700
#include <magic.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include "xmalloc.h"
#include "detection.h"
#include "clipboard.h"

static char *find_exact_type(const void *data, uint32_t length)
{
    magic_t magic = magic_open(MAGIC_MIME_TYPE | MAGIC_RAW);
    if (!magic)
    {
        fprintf(stderr, "Failed to allocate memory\n");
        exit(EXIT_FAILURE);
    }
    if (magic_load(magic, NULL))
    {
        fprintf(stderr, "Failed to load magic database\n");
        exit(EXIT_FAILURE);
    }

    const char *tmp = magic_buffer(magic, data, length);
    if (!tmp)
    {
        fprintf(stderr, "Failed to detect mime type, invalid data\n");
        exit(EXIT_FAILURE);
    }

    char *type = xstrdup(tmp);
    magic_close(magic);

    return type;
}

static bool is_text(const void *data, uint32_t length)
{
    magic_t magic = magic_open(MAGIC_MIME_ENCODING);
    if (!magic)
    {
        fprintf(stderr, "Failed to allocate memory\n");
        exit(EXIT_FAILURE);
    }
    if (magic_load(magic, NULL))
    {
        fprintf(stderr, "Failed to load magic database\n");
        exit(EXIT_FAILURE);
    }

    const char *tmp = magic_buffer(magic, data, length);
    if (!tmp)
    {
        fprintf(stderr, "Failed to detect mime type, invalid data\n");
        exit(EXIT_FAILURE);
    }

    if (strncmp(tmp, "utf-", strlen("utf-")) == 0 ||
        strncmp(tmp, "us-", strlen("us-")) == 0)
    {
        return true;
    }
    magic_close(magic);

    return false;
}

static bool is_utf8_text(const char *mime_type)
{
    if (!strcmp("TEXT", mime_type) || !strcmp("STRING", mime_type) ||
        !strcmp("UTF8_STRING", mime_type) || !strcmp("text/plain", mime_type) ||
        !strcmp("text/plain;charset=utf-8", mime_type))
    {
        return true;
    }
    return false;
}

static bool is_explicit_text(const char *mime_type)
{
    if (!strncmp("text/", mime_type, strlen("text/")))
    {
        return true;
    }
    return false;
}

void guess_mime_types(source_buffer *src)
{
    char *exact_type = find_exact_type(src->data[0], src->len[0]);

    if (is_text(src->data[0], src->len[0]) ||
            is_utf8_text(exact_type) ||
            is_explicit_text(exact_type))
    {
        src->types[0] = (mime_type){.type = "TEXT", .pos = 0};
        src->types[1] = (mime_type){.type = "STRING", .pos = 0};
        src->types[2] = (mime_type){.type = "UTF8_STRING", .pos = 0};
        src->types[3] = (mime_type){.type = "text/plain", .pos = 0};
        src->types[4] =
            (mime_type){.type = "text/plain;charset=utf-8", .pos = 0};
        src->num_types += 5;

        src->data[0] = src->data[0];
        src->data[1] = src->data[0];
        src->data[2] = src->data[0];
        src->data[3] = src->data[0];
        src->data[4] = src->data[0];

        src->len[0] = src->len[0];
        src->len[1] = src->len[0];
        src->len[2] = src->len[0];
        src->len[3] = src->len[0];
        src->len[4] = src->len[0];
    }
    else
    {
        src->types[0] = (mime_type){
            .type = exact_type, .pos = 0};
        src->num_types++;
    }
}

int find_write_type(source_buffer *src)
{
    uint8_t utf8_text = 0, explicit_text = 0, any_text = 0, binary = 0;

    for (int i = 0; i < src->num_types; i++)
    {
        if (is_utf8_text(src->types[i].type))
        {
            utf8_text = i;
        }
        else if (is_explicit_text(src->types[i].type))
        {
            explicit_text = i;
        }
        else if (is_text(src->data[i], src->len[i]))
        {
            any_text = i;
        }
        else
        {
            binary = i;
        }
    }

    if (utf8_text)
    {
        return utf8_text;
    }
    else if (explicit_text)
    {
        return explicit_text;
    }
    else if (any_text)
    {
        return any_text;
    }
    else
    {
        return binary;
    }
}

void get_snippet(source_buffer *src)
{
    int snippet_type = 0;
    bool snippet_type_found = false;

    for (int i = 0; i < src->num_types; i++)
    {
        if (is_utf8_text(src->types[i].type))
        {
            snippet_type = i;
            snippet_type_found = true;
            break;
        }
        else if (is_explicit_text(src->types[i].type))
        {
            snippet_type = i;
            snippet_type_found = true;
        }
        else if (is_text(src->data[i], src->len[i]))
        {
            snippet_type = i;
            snippet_type_found = true;
        }
    }
    if (snippet_type_found)
    {
        for (int i = 0; i < src->len[snippet_type] &&
                i < SNIPPET_SIZE; i++)
        {
            /* Replace newline characters with \ so the snippet remains all
             * on one line when shown */
            if (((char *) src->data[snippet_type])[i] == '\n')
            {
                src->snippet[i] = '\\';
            }
            else
            {
                src->snippet[i] =
                    ((char *) src->data[snippet_type])[i];
            }
        }
        if (strlen(src->snippet) >= SNIPPET_SIZE)
        {
            printf("\"%s...\"\n", src->snippet);
        }
        else
        {
            printf("\"%s\"\n", src->snippet);
        }
    }
    else
    {
    /* If there's no text for us to display show a timestamp
     * and the first MIME type offered */
        time_t ltime;
        struct tm result;
        ltime = time(NULL);
        localtime_r(&ltime, &result);
        asctime_r(&result, src->snippet);
        strcat(src->snippet, src->types[0].type);
        printf("%s\n", src->snippet);
    }
}
