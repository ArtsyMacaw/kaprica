#include <stdlib.h>
#include <wayland-client-protocol.h>
#include "clipboard.h"
#include "xmalloc.h"

clipboard *clip_init(void)
{
    clipboard *ret = xmalloc(sizeof(clipboard));
    ret->selection_o = offer_init();
    ret->selection_s = source_init();

    return ret;
}

void clip_destroy(clipboard *clip)
{
    if (clip->selection_s)
    {
        source_clear(clip->selection_s);
    }

    if (clip->selection_o)
    {
        offer_destroy(clip->selection_o);
    }

    zwlr_data_control_device_v1_destroy(clip->dmng);
    zwlr_data_control_manager_v1_destroy(clip->cmng);
    wl_seat_destroy(clip->seat);
    wl_display_disconnect(clip->display);
    free(clip);
}

void clip_sync_buffers(clipboard *clip)
{
    source_buffer *src = clip->selection_s;
    offer_buffer *ofr = clip->selection_o;

    source_clear(src);
    for (int i = 0; i < ofr->num_types; i++)
    {
        if (!ofr->invalid_data[i])
        {
            src->data[src->num_types] = xmalloc(ofr->len[i]);
            memcpy(src->data[src->num_types], ofr->data[i],
                   ofr->len[i]);
            src->len[src->num_types] = ofr->len[i];
            src->types[src->num_types].type =
                xstrdup(ofr->types[i].type);
            src->num_types++;
        }
    }
}
