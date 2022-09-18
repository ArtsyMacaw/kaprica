#define _POSIX_C_SOURCE 200112L
#define _XOPEN_SOURCE 700
#include <stdbool.h>
#include <stdio.h>
#include <poll.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>
#include <string.h>
#include "clipboard.h"
#include "wlr-data-control.h"

static void data_control_offer_mime_handler(void *data,
        struct zwlr_data_control_offer_v1 *data_offer,
        const char *mime_type)
{
    paste_src *src = (paste_src *) data;
    if (src->num_mime_types < (MAX_MIME_TYPES - 1))
    {
        src->mime_types[src->num_mime_types] = strdup(mime_type);
        src->num_mime_types++;
    } else {
        fprintf(stderr, "failed to copy all mime types\n");
    }
}

const struct zwlr_data_control_offer_v1_listener zwlr_data_control_offer_v1_listener =
{
    .offer = data_control_offer_mime_handler
};

static void data_control_device_selection_handler(void *data,
        struct zwlr_data_control_device_v1 *control_device,
        struct zwlr_data_control_offer_v1 *data_offer)
{
    paste_src *src = (paste_src *) data;
    src->buf = SELECTION;
}

static void data_control_device_primary_selection_handler(void *data,
        struct zwlr_data_control_device_v1 *control_device,
        struct zwlr_data_control_offer_v1 *data_offer)
{
    paste_src *src = (paste_src *) data;
    src->buf = PRIMARY;
}

static void data_control_device_finished_handler(void *data,
        struct zwlr_data_control_device_v1 *control_device)
{
    zwlr_data_control_device_v1_destroy(control_device);
}

static void data_control_device_data_offer_handler(void *data,
        struct zwlr_data_control_device_v1 *control_device,
        struct zwlr_data_control_offer_v1 *data_offer)
{
    paste_src *src = (paste_src *) data;
    paste_clear(src);
    src->offer = data_offer;
    zwlr_data_control_offer_v1_add_listener(data_offer,
        &zwlr_data_control_offer_v1_listener, data);
}

const struct zwlr_data_control_device_v1_listener zwlr_data_control_device_v1_listener =
{
    .data_offer = data_control_device_data_offer_handler,
    .selection = data_control_device_selection_handler,
    .primary_selection = data_control_device_primary_selection_handler,
    .finished = data_control_device_finished_handler
};

void watch_clipboard(struct zwlr_data_control_device_v1 *control_device, void *data)
{
    zwlr_data_control_device_v1_add_listener(control_device,
            &zwlr_data_control_device_v1_listener, data);
}

void remove_from_list(paste_src *src, int entry)
{
    src->num_mime_types--;
    free(src->data[entry]);
    free(src->mime_types[entry]);
    src->data[entry] = src->data[src->num_mime_types];
    src->mime_types[entry] = src->mime_types[src->num_mime_types];
    src->len[entry] = src->len[src->num_mime_types];
}

void get_selection(paste_src *src, struct wl_display *display)
{
    for (int i = 0; i < src->num_mime_types; i++)
    {
        int fds[2];
        if (pipe(fds) == -1)
        {
            fprintf(stderr, "Failed to create pipe\n");
        }

        struct pollfd watch_for_data =
        {
            .fd = fds[0],
            .events = POLLIN
        };

        // Events need to be dispatched and flushed so the other client can recieve the fd
        zwlr_data_control_offer_v1_receive(src->offer, src->mime_types[i], fds[1]);
        wl_display_dispatch_pending(display);
        wl_display_flush(display);

        // Allocate max size for simplicity's sake
        src->data[i] = malloc(MAX_READ_SIZE);
        if (!src->data[i])
        {
            fprintf(stderr, "Failed to allocate memory\n");
        }

        while (poll(&watch_for_data, 1, WAIT_TIME))
        {
            src->len[i] += read(fds[0], src->data[i], MAX_READ_SIZE);
        }

        if (src->len[i] == 0)
        {
            src->invalid_data[i] = true;
        }

        close(fds[0]);
        close(fds[1]);

        if (src->len[i] >= MAX_READ_SIZE)
        {
            fprintf(stderr, "Source is too large to copy\n");
            src->invalid_data[i] = true;
        }

        // If its invalid its going to get freed shortly, so we dont need to realloc
        if(!src->invalid_data[i])
        {
            src->data[i] = realloc(src->data[i], src->len[i]);
            assert(src->data[i]); // realloc is only ever reducing size
        }
    }

    for (int i = 0; i < src->num_mime_types; i++)
    {
        if (src->invalid_data[i])
        {
            remove_from_list(src, i);
        }
    }
}

paste_src *paste_init(void)
{
    paste_src *src = malloc(sizeof(paste_src));
    for (int i = 0; i <MAX_MIME_TYPES; i++)
    {
        src->len[i] = 0;
        src->data[i] = NULL;
    }
    return src;
}

void paste_destroy(paste_src *src)
{
    for (int i = 0; i < src->num_mime_types; i++)
    {
        free(src->data[i]);
        free(src->mime_types[i]);
    }
    free(src);
}

void paste_clear(paste_src *src)
{
    for (int i = 0; i < src->num_mime_types; i++)
    {
        /* src->data isn't guaranteed to exist as get_selection may not have been called
           thus we set it to NULL to be able to tell */
        if (src->data[i])
        {
            free(src->data[i]);
            src->data[i] = NULL;
        }
        free(src->mime_types[i]);
        src->len[i] = 0;
        src->invalid_data[i] = false;
    }
    src->num_mime_types = 0;
    src->offer = NULL;
}
