#include <stdlib.h>
#include <wayland-client-protocol.h>
#include "clipboard.h"
#include "xmalloc.h"

static void global_add(void *data, struct wl_registry *registry, uint32_t name,
                       const char *interface, uint32_t version)
{
    clipboard *clip = (clipboard *)data;
    if (strcmp(interface, "zwlr_data_control_manager_v1") == 0)
    {
        clip->cmng = wl_registry_bind(
            registry, name, &zwlr_data_control_manager_v1_interface, 1);
    }
    else if (strcmp(interface, "wl_seat") == 0)
    {
        clip->seat = wl_registry_bind(registry, name, &wl_seat_interface, 3);
    }
}

static void global_remove(void *data, struct wl_registry *registry,
                          uint32_t name)
{
    /* Empty */
}

struct wl_registry_listener registry_listener = {
    .global = global_add, .global_remove = global_remove};

clipboard *clip_init(void)
{
    clipboard *clip = xmalloc(sizeof(clipboard));
    clip->selection_offer = offer_init();
    clip->selection_source = source_init();

    clip->display = wl_display_connect(NULL);
    if (!clip->display)
    {
        fprintf(stderr, "Failed to create display\n");
        exit(EXIT_FAILURE);
    }

    struct wl_registry *registry = wl_display_get_registry(clip->display);
    wl_registry_add_listener(registry, &registry_listener, (void *)clip);

    wl_display_roundtrip(clip->display);
    if (!clip->cmng)
    {
        fprintf(stderr, "The protocol wlr-data-control not supported\n");
        exit(EXIT_FAILURE);
    }

    clip->dmng =
        zwlr_data_control_manager_v1_get_data_device(clip->cmng, clip->seat);

    return clip;
}

void clip_destroy(clipboard *clip)
{
    if (clip->selection_source)
    {
        source_destroy(clip->selection_source);
    }

    if (clip->selection_offer)
    {
        offer_destroy(clip->selection_offer);
    }

    zwlr_data_control_device_v1_destroy(clip->dmng);
    zwlr_data_control_manager_v1_destroy(clip->cmng);
    wl_seat_destroy(clip->seat);
    wl_display_disconnect(clip->display);
    free(clip);
}
