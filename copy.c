#define _XOPEN_SOURCE 700
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "clipboard.h"
#include "wlr-data-control.h"
#include "xmalloc.h"

static void data_control_source_send_handler(void *data,
        struct zwlr_data_control_source_v1 *data_src,
        const char *mime_type,
        int fd)
{
    copy_src *copy = (copy_src *) data;

    for (int i = 0; i < copy->num_mime_types; i++)
    {
        if (!strcmp(mime_type, copy->mime_types[i]))
        {
            write(fd, copy->data[i], copy->len[i]);
            close(fd);
        }
    }
}

static void data_control_source_cancelled_handler(void *data,
        struct zwlr_data_control_source_v1 *data_src)
{
    copy_src *copy = (copy_src *) data;
    copy->expired = true;
    zwlr_data_control_source_v1_destroy(data_src);
}

const struct zwlr_data_control_source_v1_listener zwlr_data_control_source_v1_listener =
{
    .send = data_control_source_send_handler,
    .cancelled = data_control_source_cancelled_handler
};

static void offer_text(struct zwlr_data_control_source_v1 *data_src)
{
    zwlr_data_control_source_v1_offer(data_src, "TEXT");
    zwlr_data_control_source_v1_offer(data_src, "STRING");
    zwlr_data_control_source_v1_offer(data_src, "UTF8_STRING");
    zwlr_data_control_source_v1_offer(data_src, "text/plain");
    zwlr_data_control_source_v1_offer(data_src, "text/plain;charset=utf8");
}

int set_selection(void *data,
        struct zwlr_data_control_manager_v1 *control_manager,
        struct zwlr_data_control_device_v1 *device_manager)
{
    copy_src *copy = (copy_src *) data;
    struct zwlr_data_control_source_v1 *data_src =
        zwlr_data_control_manager_v1_create_data_source(control_manager);
    copy->source = data_src;

    zwlr_data_control_source_v1_add_listener(data_src, &zwlr_data_control_source_v1_listener, data);

    for (int i = 0; i < copy->num_mime_types; i++)
    {
        zwlr_data_control_source_v1_offer(data_src, copy->mime_types[i]);
    }

    zwlr_data_control_device_v1_set_selection(device_manager, data_src);
    return 0;
}

int set_primary_selection(void *data,
        struct zwlr_data_control_manager_v1 *control_manager,
        struct zwlr_data_control_device_v1 *device_manager)
{
    struct zwlr_data_control_source_v1 *data_src =
        zwlr_data_control_manager_v1_create_data_source(control_manager);

    copy_src *copy = (copy_src *) data;

    zwlr_data_control_source_v1_add_listener(data_src, &zwlr_data_control_source_v1_listener, data);
    if (copy->num_mime_types == 0)
    {
        offer_text(data_src);
    }

    zwlr_data_control_device_v1_set_primary_selection(device_manager, data_src);
    return 0;
}

copy_src *copy_init(void)
{
    copy_src *src = xmalloc(sizeof(copy_src));
    src->expired = false;
    src->num_mime_types = 0;
    return src;
}

void copy_destroy(copy_src *src)
{
    for (int i = 0; i < src->num_mime_types; i++)
    {
        free(src->data[i]);
        free(src->mime_types[i]);
    }
    zwlr_data_control_source_v1_destroy(src->source);
    free(src);
}

void copy_clear(copy_src *src)
{
    for (int i = 0; i < src->num_mime_types; i++)
    {
        free(src->data[i]);
        free(src->mime_types[i]);
        src->len[i] = 0;
    }
    src->num_mime_types = 0;
    src->expired = false;
}
