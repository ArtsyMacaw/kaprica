#include <stdbool.h>
#include <wayland-client.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "clipboard.h"
#include "wlr-data-control.h"
#include "xmalloc.h"

static void
data_control_source_send_handler(void *data,
                                 struct zwlr_data_control_source_v1 *data_src,
                                 const char *mime_type, int fd)
{
    clipboard *clip = (clipboard *)data;
    source_buffer *src = clip->selection_source;

    for (int i = 0; i < src->num_types; i++)
    {
        if (!strcmp(mime_type, src->types[i]))
        {
            write(fd, src->data[i], src->len[i]);
            close(fd);

            if (src->offer_once)
            {
                clip_clear_selection(clip);
            }
        }
    }
}

static void data_control_source_cancelled_handler(
    void *data, struct zwlr_data_control_source_v1 *data_src)
{
    clipboard *clip = (clipboard *)data;
    clip->selection_source->expired = true;
    zwlr_data_control_source_v1_destroy(data_src);
}

const struct zwlr_data_control_source_v1_listener
    zwlr_data_control_source_v1_listener = {
        .send = data_control_source_send_handler,
        .cancelled = data_control_source_cancelled_handler};

void clip_clear_selection(clipboard *clip)
{
    zwlr_data_control_device_v1_set_selection(clip->dmng, NULL);
}

void clip_set_selection(clipboard *clip)
{
    source_buffer *src = clip->selection_source;
    struct zwlr_data_control_source_v1 *data_src =
        zwlr_data_control_manager_v1_create_data_source(clip->cmng);
    src->source = data_src;

    zwlr_data_control_source_v1_add_listener(
        data_src, &zwlr_data_control_source_v1_listener, clip);

    for (int i = 0; i < src->num_types; i++)
    {
        zwlr_data_control_source_v1_offer(data_src, src->types[i]);
    }

    zwlr_data_control_device_v1_set_selection(clip->dmng, data_src);
}

source_buffer *source_init(void)
{
    source_buffer *src = xmalloc(sizeof(source_buffer));
    src->offer_once = false;
    src->expired = false;
    src->num_types = 0;
    src->thumbnail = NULL;
    src->thumbnail_len = 0;
    src->source = NULL;
    src->snippet = NULL;
    src->data_hash = NULL;
    return src;
}

void source_destroy(source_buffer *src)
{
    /* Source may have been cleared before being destroyed
     * so all the data may have already been freed */
    for (int i = 0; i < src->num_types; i++)
    {
        if (src->data[i])
        {
            free(src->data[i]);
        }
        free(src->types[i]);
    }
    if (src->snippet)
    {
        free(src->snippet);
    }
    if (src->data_hash)
    {
        free(src->data_hash);
    }
    if (src->source)
    {
        zwlr_data_control_source_v1_destroy(src->source);
    }
    if (src->thumbnail)
    {
        free(src->thumbnail);
    }
    free(src);
}

void source_clear(source_buffer *src)
{
    for (int i = 0; i < src->num_types; i++)
    {
        /* The data array may have several pointers to the same memory
         * so we need to check if it has already been freed */
        if (src->data[i])
        {
            free(src->data[i]);
        }
        free(src->types[i]);
        src->len[i] = 0;
    }

    /* Sources retrieved from the database won't have a hash */
    if (src->data_hash)
    {
        free(src->data_hash);
        src->data_hash = NULL;
    }

    free(src->snippet);
    src->snippet = NULL;

    src->num_types = 0;
    src->expired = false;
    src->offer_once = false;

    /* Sources that are not images won't have a thumbnail */
    if (src->thumbnail)
    {
        free(src->thumbnail);
        src->thumbnail = NULL;
        src->thumbnail_len = 0;
    }
}
