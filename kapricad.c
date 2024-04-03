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
#include <getopt.h>
#include <ini.h>
#include "clipboard.h"
#include "database.h"
#include "wlr-data-control.h"
#include "xmalloc.h"

enum defaults
{
    SIGNAL_EVENT = 1,
    TIMER_EVENT = 2,
    ONE_MINUTE_IN_SECONDS = 60,
    FIVE_MINUTES_IN_SECONDS = 300,
    THIRTY_DAYS = 30,
    TEN_ENTRIES = 10,
    TEN_THOUSAND_ENTRIES = 10000,
};

struct config
{
    char *seat;
    char *database;
    char *config;
    uint64_t size;
    uint32_t expire;
    uint64_t limit;
};

static struct config options = {
    .seat = NULL,
    .database = NULL,
    .config = NULL,
    /* Defaults to 2GB, can't be stored in a enum due to overflow */
    .size = 2147483648,
    .expire = THIRTY_DAYS,
    .limit = TEN_THOUSAND_ENTRIES};

static const char help[] =
    "Usage: kapd [options]\n"
    "\n"
    "Options:\n"
    "    -h, --help               Show this help message\n"
    "    -v, --version            Show version number\n"
    "    -s, --seat <seat>        Specify the seat to use\n"
    "    -D, --database </path>   Specify the path to the database\n"
    "    -S, --size <(x)KB/MB/GB> Limit the size of the database\n"
    "    -e, --expire <x-days>    Set the time before an entry in the database "
    "is deleted\n"
    "    -l, --limit <0-x>        Limit the number of entries in the database\n"
    "    -c, --config </path>     Specify the path to the configuration file\n"
    "\n";

static const struct option arguments[] = {
    {"help", no_argument, NULL, 'h'},
    {"version", no_argument, NULL, 'v'},
    {"seat", required_argument, NULL, 's'},
    {"database", required_argument, NULL, 'D'},
    {"size", required_argument, NULL, 'S'},
    {"expire", required_argument, NULL, 'e'},
    {"limit", required_argument, NULL, 'l'},
    {"config", required_argument, NULL, 'c'},
    {0, 0, 0, 0}};

static uint64_t parse_size(const char *size)
{
    uint64_t multiplier = 1;
    uint64_t value = strtoull(size, NULL, 10);
    if (strstr(size, "KB"))
    {
        multiplier = 1024;
    }
    else if (strstr(size, "MB"))
    {
        multiplier = 1024 * 1024;
    }
    else if (strstr(size, "GB"))
    {
        multiplier = 1024 * 1024 * 1024;
    }
    else
    {
        fprintf(stderr, "Invalid size format: %s\n", size);
        exit(EXIT_FAILURE);
    }

    return value * multiplier;
}

static void parse_options(int argc, char *argv[])
{
    int c;
    while ((c = getopt_long(argc, argv, "hvs:D:S:e:l:c:", arguments, NULL)) !=
           -1)
    {
        switch (c)
        {
        case 'h':
            printf("%s", help);
            exit(EXIT_SUCCESS);
        case 'v':
            printf("Kaprica pre-release\n");
            exit(EXIT_SUCCESS);
        case 's':
            options.seat = xstrdup(optarg);
            break;
        case 'D':
            options.database = xstrdup(optarg);
            break;
        case 'c':
            options.config = xstrdup(optarg);
            break;
        case 'S':
            options.size = parse_size(optarg);
            break;
        case 'e':
            options.expire = strtoull(optarg, NULL, 10);
            break;
        case 'l':
            options.limit = strtoull(optarg, NULL, 10);
            break;
        default:
            fprintf(stderr, "%s", help);
            exit(EXIT_FAILURE);
        }
    }
}

/* Check $HOME/.config/kaprica/config, /etc/kaprica/config, and
 * $XDG_CONFIG_HOME/kaprica/config for the configuration file */
static char *find_config_file()
{
    if (options.config)
    {
        return options.config;
    }

    char *config_home = getenv("XDG_CONFIG_HOME");
    char *config_path = NULL;
    if (config_home)
    {
        config_path =
            xmalloc(strlen(config_home) + strlen("/kaprica/config") + 1);
        strcpy(config_path, config_home);
        strcat(config_path, "/kaprica/config");
    }
    else
    {
        char *home = getenv("HOME");
        config_path =
            xmalloc(strlen(home) + strlen("/.config/kaprica/config") + 1);
        strcpy(config_path, home);
        strcat(config_path, "/.config/kaprica/config");
    }
    if (access(config_path, F_OK) != -1)
    {
        return config_path;
    }
    free(config_path);

    if (access("/etc/kaprica/config", F_OK) != -1)
    {
        return xstrdup("/etc/kaprica/config");
    }

    return NULL;
}

static void config_handler(void *user, const char *section, const char *name,
                           const char *value)
{
    /* Always check if the option has already been set from the command line
     * so that it doesn't get overwritten */
    if (strcmp(name, "seat") == 0)
    {
        if (options.seat == NULL)
        {
            options.seat = xstrdup(value);
        }
    }
    else if (strcmp(name, "database") == 0)
    {
        if (options.database == NULL)
        {
            options.database = xstrdup(value);
        }
    }
    else if (strcmp(name, "size") == 0)
    {
        if (options.size == 2147483648)
        {
            options.size = parse_size(value);
        }
    }
    else if (strcmp(name, "expire") == 0)
    {
        if (options.expire == THIRTY_DAYS)
        {
            options.expire = strtoull(value, NULL, 10);
        }
    }
    else if (strcmp(name, "limit") == 0)
    {
        if (options.limit == TEN_THOUSAND_ENTRIES)
        {
            options.limit = strtoull(value, NULL, 10);
        }
    }
    else
    {
        fprintf(stderr, "Invalid option: %s\n", name);
        exit(EXIT_FAILURE);
    }
}

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
    parse_options(argc, argv);

    char *config_file = find_config_file();
    if (config_file)
    {
        printf("Loading configuration from %s\n", config_file);
        ini_parse(config_file, (ini_handler)config_handler, NULL);
        free(config_file);
    }

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

    sqlite3 *db = database_init(options.database);

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
            num_of_entries++;
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
                num_of_entries++;
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

            uint32_t entries_removed =
                database_delete_old_entries(db, (options.expire * -1));
            if (entries_removed)
            {
                printf("Removed %u old entries\n", entries_removed);
                num_of_entries -= entries_removed;
            }

            entries_removed = 0;
            while (database_get_size(db) > options.size)
            {
                entries_removed +=
                    database_delete_largest_entries(db, TEN_ENTRIES);
            }
            if (entries_removed)
            {
                printf("Removed %u entries to fit the size limit\n",
                       entries_removed);
                num_of_entries -= entries_removed;
            }

            if (num_of_entries > options.limit)
            {
                entries_removed = database_delete_last_entries(
                    db, num_of_entries - options.limit);
                if (entries_removed)
                {
                    printf("Removed %u entries over the limit\n",
                           entries_removed);
                }
            }
        }

        if (wl_display_read_events(clip->display) == -1)
        {
            perror("wl_display_read_events");
            break;
        }
    }

    /* Cleanup that shouldn't be necessary but helps analyze with valgrind */
    database_close(db);
    close(display_fd);
    close(watch_signals);
    close(clean_up_entries);
    clip_destroy(clip);
}
