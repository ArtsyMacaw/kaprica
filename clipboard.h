#include <stdbool.h>
#include "wlr-data-control.h"

// No source should be offering more than 25 types hopefully
#define MAX_MIME_TYPES 25
// We accept max 200MB of data
#define MAX_READ_SIZE 209715200
// Give the client 100ms to start writing data
#define WAIT_TIME 100

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
