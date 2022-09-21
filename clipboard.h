#include <stdbool.h>
#include "wlr-data-control.h"

#ifndef CLIPBOARD_H
#define CLIPBOARD_H

// No source should be offering more than 25 types hopefully
#define MAX_MIME_TYPES 25
// We accept max 50MB of data
#define MAX_DATA_SIZE 52428800
// Read 64 KiB the capacity of pipes() buffer
#define READ_SIZE 65536
/* By default wait 100ms for the client to start writing data
 for images and other types that may take longer we wait a second
 If we succesfully received data we can safetly wait for a while */
#define WAIT_TIME_SHORT 100
#define WAIT_TIME_LONG 2000
#define WAIT_TIME_LONGEST 8000

typedef enum
{
    UNSET_BUFFER,
    SELECTION,
    PRIMARY
} clipboard_buffer;

typedef struct
{
    void *data[MAX_MIME_TYPES];
    char *mime_types[MAX_MIME_TYPES];
    uint32_t len[MAX_MIME_TYPES];
    bool invalid_data[MAX_MIME_TYPES];
    bool expired;
    int num_mime_types;
    struct zwlr_data_control_offer_v1 *offer;
    clipboard_buffer buf;
} paste_src;

void watch_clipboard(struct zwlr_data_control_device_v1 *control_device, void *data);
void get_selection(paste_src *src, struct wl_display *display);
paste_src *paste_init(void);
void paste_destroy(paste_src *src);
void paste_clear(paste_src *src);

typedef struct
{
    void *data[MAX_MIME_TYPES];
    char *mime_types[MAX_MIME_TYPES];
    uint32_t len[MAX_MIME_TYPES];
    bool expired;
    int num_mime_types;
    struct zwlr_data_control_source_v1 *source;
} copy_src;

int set_selection(void *data,
        struct zwlr_data_control_manager_v1 *control_manager,
        struct zwlr_data_control_device_v1 *device_manager);
int set_primary_selection(void *data,
        struct zwlr_data_control_manager_v1 *control_manager,
        struct zwlr_data_control_device_v1 *device_manager);
copy_src *copy_init(void);
void copy_destroy(copy_src *src);
void copy_clear(copy_src *src);

#endif
