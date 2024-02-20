#define _POSIX_C_SOURCE 200112L
#define _XOPEN_SOURCE 700
#include <wayland-client.h>
#include <poll.h>
#include <signal.h>
#include <sqlite3.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/signalfd.h>
#include <sys/timerfd.h>
#include <time.h>
#include <unistd.h>
#include "clipboard.h"
#include "database.h"
#include "wlr-data-control.h"
#include "xmalloc.h"

#define TIMER_EVENT 2
#define SIGNAL_EVENT 1

static clipboard *clip;

static void global_add(void *data, struct wl_registry *registry, uint32_t name,
                       const char *interface, uint32_t version)
{
    if (strcmp(interface, "zwlr_data_control_manager_v1") == 0)
    {
        clip->cmng = wl_registry_bind(registry, name,
                                &zwlr_data_control_manager_v1_interface, 1);
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

struct wl_registry_listener registry_listener =
{
    .global = global_add,
    .global_remove = global_remove
};

static void prepare_read(struct wl_display *display)
{
    while (wl_display_prepare_read(display) != 0)
    {
        wl_display_dispatch_pending(display);
    }
    wl_display_flush(display);
}

int main(int argc, char *argv[])
{
    clip = clip_init();

    clip->display = wl_display_connect(NULL);
    if (!clip->display)
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

    int clean_up_entries = timerfd_create(CLOCK_MONOTONIC, 0);
    struct timespec five_minutes = {.tv_sec = 300};
    struct itimerspec timer = {.it_interval = five_minutes,
                               .it_value = five_minutes};
    timerfd_settime(clean_up_entries, 0, &timer, NULL);

    int watch_signals = signalfd(-1, &mask, 0);
    int display_fd = wl_display_get_fd(clip->display);
    struct pollfd wait_for_events[] = {
        {.fd = display_fd, .events = POLLIN},
        {.fd = watch_signals, .events = POLLIN},
        {.fd = clean_up_entries, .events = POLLIN}};

    struct wl_registry *registry = wl_display_get_registry(clip->display);
    wl_registry_add_listener(registry, &registry_listener, NULL);

    wl_display_roundtrip(clip->display);
    if (!clip->cmng)
    {
        fprintf(stderr, "wlr-data-control not supported\n");
        return 1;
    }

    clip->dmng = zwlr_data_control_manager_v1_get_data_device(clip->cmng, clip->seat);

    sqlite3 *db = database_init();

    clip_watch(clip);
    wl_display_roundtrip(clip->display);

    /* If selection is set copy and re-serve it; if its unset
     * try to load last source from history, and if all else
     * fails just wait for selection to be set */
    bool db_is_not_empty = true;
    while (!clip->selection_o->num_types
            && !clip->selection_s->num_types)
    {
        if (!clip->selection_o->offer && db_is_not_empty)
        {
            printf("Loading from source database\n");
            uint32_t id = database_get_latest_source_id(db);
            if (id)
            {
                database_get_source(db, id, clip->selection_s);
                break;
            }
            db_is_not_empty = false;
        }
        wl_display_dispatch(clip->display);
    }

    if (!clip->selection_s->num_types)
    {
        clip_get_selection(clip);
        clip_sync_buffers(clip);
        database_insert_source(db, clip->selection_s);
    }
    clip_set_selection(clip);

    while (true)
    {
        prepare_read(clip->display);

        if (clip->selection_s->expired)
        {
            wl_display_cancel_read(clip->display);

            clip_get_selection(clip);
            clip_sync_buffers(clip);
            clip_set_selection(clip);

            database_insert_source(db, clip->selection_s);

            prepare_read(clip->display);
        }

        if (poll(wait_for_events, 3, -1) < 0)
        {
            fprintf(stderr, "Poll failed\n");
            wl_display_cancel_read(clip->display);
            return 1;
        }

        if (poll(&wait_for_events[SIGNAL_EVENT], 1, 0) > 0)
        {
            printf("Stopping Kaprica...\n");
            wl_display_cancel_read(clip->display);
            break;
        }

        if (poll(&wait_for_events[TIMER_EVENT], 1, 0) > 0)
        {
            /* Read just to clear the buffer */
            uint64_t tmp;
            read(clean_up_entries, &tmp, sizeof(uint64_t));
            printf("Removed %d old entries\n",
                   database_destroy_old_entries(db, -30));
        }

        wl_display_read_events(clip->display);
    }

    /* Cleanup that shouldn't be necessary but helps analyze with valgrind */
    database_destroy(db);
    close(watch_signals);
    close(clean_up_entries);
    clip_destroy(clip);
}
