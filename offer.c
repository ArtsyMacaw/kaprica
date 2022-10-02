#define _POSIX_C_SOURCE 200112L
#define _XOPEN_SOURCE 700
#include <wayland-client.h>
#include <errno.h>
#include <poll.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "clipboard.h"
#include "wlr-data-control.h"
#include "xmalloc.h"

static void
data_control_offer_mime_handler(void *data,
                                struct zwlr_data_control_offer_v1 *data_offer,
                                const char *mime_type)
{
    clipboard *clip = (clipboard *)data;
    offer_buffer *ofr = clip->selection_o;

    if (ofr->num_types < (MAX_MIME_TYPES - 1))
    {
        uint8_t index = ofr->num_types;
        ofr->types[index].type = xstrdup(mime_type);
        ofr->types[index].pos = index;
        ofr->num_types++;
    }
    else
    {
        fprintf(stderr, "Failed to copy mime type: %s\n", mime_type);
    }
}

const struct zwlr_data_control_offer_v1_listener
    zwlr_data_control_offer_v1_listener =
{
    .offer = data_control_offer_mime_handler
};

static void data_control_device_selection_handler(
    void *data, struct zwlr_data_control_device_v1 *control_device,
    struct zwlr_data_control_offer_v1 *data_offer)
{
    clipboard *clip = (clipboard *)data;
    clip->selection_o->buf = SELECTION;
}

static void data_control_device_primary_selection_handler(
    void *data, struct zwlr_data_control_device_v1 *control_device,
    struct zwlr_data_control_offer_v1 *data_offer)
{
    clipboard *clip = (clipboard *)data;
    clip->selection_o->buf = PRIMARY;
}

static void data_control_device_finished_handler(
    void *data, struct zwlr_data_control_device_v1 *control_device)
{
    zwlr_data_control_device_v1_destroy(control_device);
}

static void data_control_device_data_offer_handler(
    void *data, struct zwlr_data_control_device_v1 *control_device,
    struct zwlr_data_control_offer_v1 *data_offer)
{
    clipboard *clip = (clipboard *)data;
    offer_clear(clip->selection_o);
    clip->selection_o->offer = data_offer;
    zwlr_data_control_offer_v1_add_listener(
        data_offer, &zwlr_data_control_offer_v1_listener, data);
}

const struct zwlr_data_control_device_v1_listener
    zwlr_data_control_device_v1_listener =
{
        .data_offer = data_control_device_data_offer_handler,
        .selection = data_control_device_selection_handler,
        .primary_selection = data_control_device_primary_selection_handler,
        .finished = data_control_device_finished_handler
};

void clip_watch(clipboard *clip)
{
    zwlr_data_control_device_v1_add_listener(
        clip->dmng, &zwlr_data_control_device_v1_listener, clip);
}

void clip_get_selection(clipboard *clip)
{
    offer_buffer *ofr = clip->selection_o;
    for (int i = 0; i < ofr->num_types; i++)
    {
        int fds[2];
        if (pipe(fds) == -1)
        {
            fprintf(stderr, "Failed to create pipe\n");
            exit(1);
        }

        struct pollfd watch_for_data = {.fd = fds[0], .events = POLLIN};

        /* Events need to be dispatched and flushed so the other client
         * can recieve the fd */
        zwlr_data_control_offer_v1_receive(ofr->offer, ofr->types[i].type,
                                           fds[1]);
        wl_display_dispatch_pending(clip->display);
        wl_display_flush(clip->display);

        /* Allocate max size for simplicity's sake */
        ofr->data[i] = xmalloc(MAX_DATA_SIZE);

        int wait_time;
        if (!strncmp("image/png", ofr->types[i].type, strlen("image/png")) ||
            !strncmp("image/jpeg", ofr->types[i].type, strlen("image/jpeg")))
        {
            wait_time = WAIT_TIME_LONG;
        }
        else
        {
            wait_time = WAIT_TIME_SHORT;
        }

        while (poll(&watch_for_data, 1, wait_time) > 0)
        {
            void *sub_array = ofr->data[i] + ofr->len[i];
            int bytes_read = read(fds[0], sub_array, READ_SIZE);

            /* If we get an error (-1) dont change anything */
            wait_time = (bytes_read > 0) ? WAIT_TIME_LONGEST : 0;
            ofr->len[i] += (bytes_read > 0) ? bytes_read : 0;

            if (ofr->len[i] >= (MAX_DATA_SIZE - (READ_SIZE * 2)))
            {
                fprintf(stderr, "Source is too large to copy\n");
                ofr->invalid_data[i] = true;
                break;
            }
            if (bytes_read < READ_SIZE)
            {
                break;
            }
        }

        close(fds[0]);
        close(fds[1]);

        if (ofr->len[i] == 0)
        {
            ofr->invalid_data[i] = true;
        }
        else
        {
            ofr->data[i] = xrealloc(ofr->data[i], ofr->len[i]);
        }
    }
}

offer_buffer *offer_init(void)
{
    offer_buffer *ofr = xmalloc(sizeof(offer_buffer));
    for (int i = 0; i < MAX_MIME_TYPES; i++)
    {
        ofr->len[i] = 0;
        ofr->data[i] = NULL;
        ofr->invalid_data[i] = false;
    }
    return ofr;
}

void offer_destroy(offer_buffer *ofr)
{
    for (int i = 0; i < ofr->num_types; i++)
    {
        free(ofr->data[i]);
        free(ofr->types[i].type);
    }
    free(ofr);
}

void offer_clear(offer_buffer *ofr)
{
    for (int i = 0; i < ofr->num_types; i++)
    {
        /* src->data isn't guaranteed to exist as get_selection may not have
           been called thus we set it to NULL to be able to tell */
        if (ofr->data[i])
        {
            free(ofr->data[i]);
            ofr->data[i] = NULL;
        }
        free(ofr->types[i].type);
        ofr->types[i].pos = 0;
        ofr->len[i] = 0;
        ofr->invalid_data[i] = false;
    }
    ofr->num_types = 0;
    ofr->offer = NULL;
}
