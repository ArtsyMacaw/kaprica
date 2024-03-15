#include <stdint.h>
#include <stdbool.h>
#include "wlr-data-control.h"

#ifndef CLIPBOARD_H
#define CLIPBOARD_H

/* No source should be offering more than 25 types hopefully */
#define MAX_MIME_TYPES 25

/* We accept max 50MB of data */
#define MAX_DATA_SIZE 52428800

/* Read 64 KiB the capacity of pipes() buffer */
#define READ_SIZE 65536 // Assumes 4k page size, fix at some point

/* By default wait 100ms for the client to start writing data
 for images and other types that may take longer we wait a second
 If we succesfully received data we can safely wait for a while */
#define WAIT_TIME_SHORT 10
#define WAIT_TIME_LONG 2000
#define WAIT_TIME_LONGEST 8000

#define SNIPPET_SIZE 80

// Turn these all into enums maybe?

typedef enum
{
    UNSET_BUFFER,
    SELECTION,
    PRIMARY // Not implemented yet
} clipboard_buffer;

/* Mime type and where its data is located */
// Remove this just, fix positioning when syncing buffers
typedef struct
{
    char *type;
    uint16_t pos;
} mime_type;

/* Buffer not managed by us */
typedef struct
{
    void *data[MAX_MIME_TYPES];
    mime_type types[MAX_MIME_TYPES];
    size_t len[MAX_MIME_TYPES];
    bool invalid_data[MAX_MIME_TYPES];
    uint8_t num_types;
    struct zwlr_data_control_offer_v1 *offer;
    clipboard_buffer buf;
} offer_buffer;

/* Buffer managed by us */
typedef struct
{
    void *data[MAX_MIME_TYPES];
    mime_type types[MAX_MIME_TYPES];
    size_t len[MAX_MIME_TYPES];
    char *snippet;
    void *thumbnail;
    size_t thumbnail_len;
    bool offer_once;
    bool expired;
    uint8_t num_types;
    struct zwlr_data_control_source_v1 *source;
} source_buffer;

typedef struct
{
    source_buffer *selection_source;
    offer_buffer *selection_offer;
    struct wl_display *display;
    struct wl_seat *seat;
    struct zwlr_data_control_manager_v1 *cmng;
    struct zwlr_data_control_device_v1 *dmng;
} clipboard;

/* Clipboard functions | clipboard.c */
clipboard *clip_init(void);
void clip_destroy(clipboard *clip);
void clip_watch(clipboard *clip);
void clip_get_selection(clipboard *clip);
void clip_clear_selection(clipboard *clip);
void clip_set_selection(clipboard *clip);

/* Offer buffer functions | offer.c */
offer_buffer *offer_init(void);
void offer_clear(offer_buffer *ofr);
void offer_destroy(offer_buffer *ofr);

/* Source buffer functions | source.c */
source_buffer *source_init(void);
void source_clear(source_buffer *src);
void source_destroy(source_buffer *src);

#endif
