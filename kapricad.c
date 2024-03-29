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

enum
{
    SIGNAL_EVENT = 1,
    TIMER_EVENT = 2,
    ONE_MINUTE_IN_SECONDS = 60,
    FIVE_MINUTES_IN_SECONDS = 300
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
    clipboard *clip = clip_init();

    /* Block SIGINT and SIGTERM to be able to handle them with signalfd */
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGTERM);
    if (sigprocmask(SIG_BLOCK, &mask, NULL) == -1)
    {
        perror("sigprocmask");
        exit(EXIT_FAILURE);
    }

    /* Handle SIGINT and SIGTERM manually so we can cleanup */
    int watch_signals = signalfd(-1, &mask, 0);

    /* Set up timer to periodically clean up the database */
    int clean_up_entries = timerfd_create(CLOCK_MONOTONIC, 0);
    struct timespec one_minute = {.tv_sec = ONE_MINUTE_IN_SECONDS};
    struct timespec five_minutes = {.tv_sec = FIVE_MINUTES_IN_SECONDS};
    struct itimerspec timer = {.it_interval = five_minutes,
                               .it_value = one_minute};
    timerfd_settime(clean_up_entries, 0, &timer, NULL);

    /* Get the fd of the display for poll */
    int display_fd = wl_display_get_fd(clip->display);

    struct pollfd wait_for_events[] = {
        {.fd = display_fd, .events = POLLIN},
        {.fd = watch_signals, .events = POLLIN},
        {.fd = clean_up_entries, .events = POLLIN}};

    sqlite3 *db = database_init();

    clip_watch(clip);
    wl_display_roundtrip(clip->display);

    /* If selection is set add it to the database; if its unset
     * try to load last source from history, and if all else
     * fails just wait for selection to be set */
    uint32_t num_of_entries = database_get_total_entries(db);
    bool selection_set = false;
    do
    {
        selection_set = clip_get_selection(clip);
        if (selection_set)
        {
            database_insert_entry(db, clip->selection_source);
            break;
        }

        if (num_of_entries > 0)
        {
            int64_t id;
            database_get_latest_entries(db, 1, 0, &id);
            database_get_entry(db, id, clip->selection_source);
            clip_set_selection(clip);
            clip->serving = true;
            break;
        }

        wl_display_dispatch(clip->display);
    } while (!selection_set);

    while (true)
    {
        prepare_read(clip->display);

        // FIXME: This causes wl_display_read_events() to leak memory
        if ((clip->serving && clip->selection_source->expired) ||
            (!clip->serving && clip->selection_offer->expired))
        {
            wl_display_cancel_read(clip->display);

            if (clip_get_selection(clip))
            {
                database_insert_entry(db, clip->selection_source);
                clip->serving = false;
            }
            else
            {
                clip_set_selection(clip);
                clip->serving = true;
            }

            prepare_read(clip->display);
        }

        if (poll(wait_for_events, 3, -1) < 0)
        {
            perror("poll");
            wl_display_cancel_read(clip->display);
            break;
        }

        if (poll(&wait_for_events[SIGNAL_EVENT], 1, 0) > 0)
        {
            printf("\nStopping Kaprica...\n");
            wl_display_cancel_read(clip->display);
            break;
        }

        if (poll(&wait_for_events[TIMER_EVENT], 1, 0) > 0)
        {
            /* Read just to clear the buffer */
            uint64_t tmp;
            read(clean_up_entries, &tmp, sizeof(uint64_t));

            uint32_t entries_removed = database_delete_old_entries(db, -30);
            if (entries_removed)
            {
                printf("Removed %d old entries\n", entries_removed);
            }

            entries_removed = database_delete_duplicate_entries(db);
            if (entries_removed)
            {
                printf("Removed %d duplicate entries\n", entries_removed);
            }
        }

        if (wl_display_read_events(clip->display) == -1)
        {
            perror("wl_display_read_events");
            break;
        }
    }

    /* Cleanup that shouldn't be necessary but helps analyze with valgrind */
    database_destroy(db);
    close(display_fd);
    close(watch_signals);
    close(clean_up_entries);
    clip_destroy(clip);
}
