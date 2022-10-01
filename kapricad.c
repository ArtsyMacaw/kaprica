#define _POSIX_C_SOURCE 200112L
#define _XOPEN_SOURCE 700
#include <stdint.h>
#include <poll.h>
#include <signal.h>
#include <sqlite3.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/signalfd.h>
#include <unistd.h>
#include "clipboard.h"
#include "database.h"
#include "wlr-data-control.h"
#include "xmalloc.h"

static struct zwlr_data_control_manager_v1 *cmng = NULL;
static struct wl_seat *seat = NULL;

static void global_add(void *data, struct wl_registry *registry, uint32_t name,
                       const char *interface, uint32_t version)
{
    if (strcmp(interface, "zwlr_data_control_manager_v1") == 0)
    {
        cmng = wl_registry_bind(registry, name,
                                &zwlr_data_control_manager_v1_interface, 1);
    }
    else if (strcmp(interface, "wl_seat") == 0)
    {
        seat = wl_registry_bind(registry, name, &wl_seat_interface, 3);
    }
}

static void sync_sources(copy_src *copy, paste_src *paste)
{
    for (int i = 0; i < paste->num_mime_types; i++)
    {
        if (paste->invalid_data[i] == false)
        {
            copy->data[copy->num_mime_types] = xmalloc(paste->len[i]);
            memcpy(copy->data[copy->num_mime_types], paste->data[i],
                   paste->len[i]);
            copy->len[copy->num_mime_types] = paste->len[i];
            copy->mime_types[copy->num_mime_types] =
                xstrdup(paste->mime_types[i]);
            copy->num_mime_types++;
        }
    }
}

static void global_remove(void *data, struct wl_registry *registry,
                          uint32_t name)
{
    /* Empty */
}

struct wl_registry_listener registry_listener =
{
    .global = global_add,
    .global_remove = global_remove
};

int main(int argc, char *argv[])
{
    struct wl_display *display = wl_display_connect(NULL);
    if (!display)
    {
        fprintf(stderr, "Failed to create display\n");
        return 1;
    }

    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGTERM);
    if (sigprocmask(SIG_BLOCK, &mask, NULL) == -1)
    {
        fprintf(stderr, "Failed to mask signals\n");
        return 1;
    }

    int watch_signals = signalfd(-1, &mask, 0);
    int display_fd = wl_display_get_fd(display);
    struct pollfd wait_for_events[] = {{.fd = display_fd, .events = POLLIN},
                                       {.fd = watch_signals, .events = POLLIN}};

    struct wl_registry *registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &registry_listener, NULL);

    wl_display_roundtrip(display);
    if (!cmng)
    {
        fprintf(stderr, "wlr-data-control not supported\n");
        return 1;
    }

    struct zwlr_data_control_device_v1 *dmng =
        zwlr_data_control_manager_v1_get_data_device(cmng, seat);

    sqlite3 *db = database_init();
    copy_src *copy = copy_init();
    paste_src *paste = paste_init();

    watch_clipboard(dmng, paste);
    wl_display_roundtrip(display);

    /* If selection is set copy and re-serve it; if its unset
     * try to load last source from history, and if all else
     * fails just wait for selection to be set */
    bool db_is_not_empty = true;
    while (!paste->num_mime_types && !copy->num_mime_types)
    {
        if (!paste->offer && db_is_not_empty)
        {
            printf("Loading from source database\n");
            uint32_t id = database_get_latest_source_id(db);
            if (id)
            {
                database_get_source(db, id, copy);
                break;
            }
            db_is_not_empty = false;
        }
        wl_display_dispatch(display);
    }

    if (!copy->num_mime_types)
    {
        get_selection(paste, display);
        sync_sources(copy, paste);
        database_insert_source(db, copy);
    }
    set_selection(copy, cmng, dmng);

    while (true)
    {
    x:
        while (wl_display_prepare_read(display) != 0)
        {
            wl_display_dispatch_pending(display);
        }
        wl_display_flush(display);

        if (copy->expired)
        {
            wl_display_cancel_read(display);
            get_selection(paste, display);
            copy_clear(copy);
            sync_sources(copy, paste);
            set_selection(copy, cmng, dmng);
            database_insert_source(db, copy);
            goto x;
        }

        // Implement timerfd to clean up old database entries
        if (poll(wait_for_events, 2, -1) < 0)
        {
            fprintf(stderr, "Poll failed\n");
            wl_display_cancel_read(display);
            return 1;
        }

        if (poll(&wait_for_events[1], 1, 0) > 0)
        {
            printf("Stopping Kaprica...\n");
            wl_display_cancel_read(display);
            break;
        }

        wl_display_read_events(display);
    }

    /* Cleanup that shouldn't be necessary but helps analyze with valgrind */
    database_destroy(db);
    close(watch_signals);
    copy_destroy(copy);
    paste_destroy(paste);
    zwlr_data_control_device_v1_destroy(dmng);
    zwlr_data_control_manager_v1_destroy(cmng);
    wl_seat_destroy(seat);
    wl_display_disconnect(display);
}
