#include <stdint.h>
#include <stdbool.h>
#include "wlr-data-control.h"

#ifndef CLIPBOARD_H
#define CLIPBOARD_H

enum default_sizes
{
    MAX_MIME_TYPES = 25,
    SNIPPET_SIZE = 80,
    /* Read 64 KiB the capacity of pipes() buffer */
    READ_SIZE = 65536,       // FIXME: Assumes 4k page size, fix at some point
    MAX_DATA_SIZE = 52428800 /* 50MB */
};

typedef enum
{
    UNSET_BUFFER,
    SELECTION,
    PRIMARY // TODO
} clipboard_buffer;

/* Buffer not managed by kaprica */
typedef struct
{
    void *data[MAX_MIME_TYPES];
    char *types[MAX_MIME_TYPES];
    size_t len[MAX_MIME_TYPES];
    bool invalid_data[MAX_MIME_TYPES];
    bool expired;
    uint8_t num_types;
    struct zwlr_data_control_offer_v1 *offer;
    clipboard_buffer buf;
} offer_buffer;

/* Buffer managed by kaprica */
typedef struct
{
    void *data[MAX_MIME_TYPES];
    char *types[MAX_MIME_TYPES];
    size_t len[MAX_MIME_TYPES];
    char *snippet;
    char *data_hash;
    void *thumbnail;
    size_t thumbnail_len;
    bool offer_once;
    bool expired;
    uint8_t num_types;
    struct zwlr_data_control_source_v1 *source;
} source_buffer;

/* Clipboard state */
typedef struct
{
    bool serving;
    source_buffer *selection_source;
    offer_buffer *selection_offer;
    struct wl_display *display;
    struct wl_seat *seat;
    struct zwlr_data_control_manager_v1 *cmng;
    struct zwlr_data_control_device_v1 *dmng;
    struct wl_registry *registry;
} clipboard;

/* Clipboard functions | clipboard.c */
clipboard *clip_init(void);
void clip_destroy(clipboard *clip);

/* Offer buffer functions | offer.c */
offer_buffer *offer_init(void);
void offer_clear(offer_buffer *ofr);
void offer_destroy(offer_buffer *ofr);
bool clip_get_selection(clipboard *clip);
void clip_watch(clipboard *clip);

/* Source buffer functions | source.c */
source_buffer *source_init(void);
void source_clear(source_buffer *src);
void source_destroy(source_buffer *src);
void clip_clear_selection(clipboard *clip);
void clip_set_selection(clipboard *clip);

#endif
