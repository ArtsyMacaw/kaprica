#define _POSIX_C_SOURCE 200112L
#define _XOPEN_SOURCE 700
#include <stdbool.h>
#include <stdio.h>
#include <poll.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include "clipboard.h"
#include "wlr-data-control.h"
#include "xmalloc.h"

static void data_control_offer_mime_handler(void *data,
        struct zwlr_data_control_offer_v1 *data_offer,
        const char *mime_type)
{
    paste_src *src = (paste_src *) data;
    if (src->num_mime_types < (MAX_MIME_TYPES - 1))
    {
        src->mime_types[src->num_mime_types] = xstrdup(mime_type);
        src->num_mime_types++;
    } else {
        fprintf(stderr, "Too many mime types to copy\n");
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

void get_selection(paste_src *src, struct wl_display *display)
{
    for (int i = 0; i < src->num_mime_types; i++)
    {
        int fds[2];
        if (pipe(fds) == -1)
        {
            fprintf(stderr, "Failed to create pipe\n");
            exit(1);
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
        src->data[i] = xmalloc(MAX_DATA_SIZE);

        int wait_time;
        if (!strncmp("image/png", src->mime_types[i], strlen("image/png")) ||
                !strncmp("image/jpeg", src->mime_types[i], strlen("image/jpeg")))
        {
            wait_time = WAIT_TIME_LONG;
        } else {
            wait_time = WAIT_TIME_SHORT;
        }

        while (poll(&watch_for_data, 1, wait_time) > 0)
        {
            void *sub_array = src->data[i] + src->len[i];
            int bytes_read = read(fds[0], sub_array, READ_SIZE);
            wait_time = (bytes_read > 0) ? WAIT_TIME_LONGEST : 0;
            // If we get an error (-1) dont change the length
            src->len[i] += (bytes_read > 0) ? bytes_read : 0;
            if (src->len[i] >= (MAX_DATA_SIZE - (READ_SIZE * 2)))
            {
                fprintf(stderr, "Source is too large to copy\n");
                src->invalid_data[i] = true;
                break;
            }
            if (bytes_read < READ_SIZE)
            {
                break;
            }
        }

        close(fds[0]);
        close(fds[1]);

        if (src->len[i] == 0)
        {
            src->invalid_data[i] = true;
        } else {
            src->invalid_data[i] = false;
            src->data[i] = xrealloc(src->data[i], src->len[i]);
        }
    }
}

paste_src *paste_init(void)
{
    paste_src *src = xmalloc(sizeof(paste_src));
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
