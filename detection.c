#define _POSIX_C_SOURCE 200112L
#define _XOPEN_SOURCE 700
#include <magic.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <MagickWand/MagickWand.h>
#include "xmalloc.h"
#include "detection.h"
#include "clipboard.h"

static char *find_exact_type(const void *data, size_t length)
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

static bool is_text(const void *data, size_t length)
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
    if (!strncmp("text/_moz_htmlinfo", mime_type, strlen("text/_moz_htmlinfo")))
    {
        return false;
    }
    if (!strncmp("text/", mime_type, strlen("text/")))
    {
        return true;
    }
    return false;
}

static bool is_image(const char *mime_type)
{
    if (!strncmp("image/", mime_type, strlen("image/")))
    {
        return true;
    }
    return false;
}

void guess_mime_types(source_buffer *src)
{
    char *exact_type = find_exact_type(src->data[0], src->len[0]);

    if (is_text(src->data[0], src->len[0]) || is_utf8_text(exact_type) ||
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
        src->types[0] = (mime_type){.type = exact_type, .pos = 0};
        src->num_types++;
    }
}

uint8_t find_write_type(source_buffer *src)
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

/* If there's no text version of the source generate a time
 * stamp and concatenate it with the first mime type for the snippet */
static void generate_stamp(source_buffer *src)
{
    time_t ltime;
    ltime = time(NULL);
    src->snippet = xstrdup(asctime(localtime(&ltime)));
    /* Replace '\n' with a space */
    src->snippet[strlen(src->snippet) - 1] = ' ';

    size_t size = strlen(src->types[0].type) + strlen(src->snippet) + 1;
    src->snippet = xrealloc(src->snippet, (size * sizeof(char)));
    src->snippet = strcat(src->snippet, src->types[0].type);
}

void get_snippet(source_buffer *src)
{
    uint8_t snip_type = find_write_type(src);

    if (!is_utf8_text(src->types[snip_type].type) &&
        !is_explicit_text(src->types[snip_type].type))
    {
        generate_stamp(src);
    }
    else
    {
        int j = 0;
        for (int i = 0; i < src->len[snip_type] && j < (SNIPPET_SIZE - 1); i++)
        {
            /* Replace newline characters with \ so the snippet remains all
             * on one line when shown */
            if (((char *)src->data[snip_type])[i] == '\n')
            {
                src->snippet[j] = '\\';
                j++;
            }
            /* Ignore null characters that come before the end of the string */
            else if (((char *)src->data[snip_type])[i] != '\0')
            {
                src->snippet[j] = ((char *)src->data[snip_type])[i];
                j++;
            }
        }
        src->snippet[j] = '\0';
    }
}

/* Generate a thumbnail of the first image in the source */
void get_thumbnail(source_buffer *src)
{
    int type = -1;
    for (int i = 0; i < src->num_types; i++)
    {
        if (is_image(src->types[i].type))
        {
            type = i;
            break;
        }
    }

    if (type == -1)
    {
        return;
    }

    MagickWand *wand = NewMagickWand();
    MagickBooleanType status;
    size_t length;
    unsigned char *blob;

    status = MagickReadImageBlob(wand, src->data[type], src->len[type]);
    if (status == MagickFalse)
    {
        fprintf(stderr, "Failed to read image\n");
    }

    MagickSetImageFormat(wand, "jpeg");
    MagickThumbnailImage(wand, 320, 100);
    blob = MagickGetImageBlob(wand, &length);
    src->thumbnail = xmalloc(length);
    memcpy(src->thumbnail, blob, length);
    src->thumbnail_len = length;

    DestroyMagickWand(wand);
    MagickWandTerminus();
}
