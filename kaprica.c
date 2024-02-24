#define _POSIX_C_SOURCE 200112L
#define _XOPEN_SOURCE 700
#include <sys/types.h>
#include <wayland-client-core.h>
#include <wayland-client.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <getopt.h>
#include "clipboard.h"
#include "database.h"
#include "detection.h"
#include "wlr-data-control.h"
#include "xmalloc.h"

static clipboard *clip;

enum verb
{
    COPY,
    PASTE,
    SEARCH
    // Delete
};

struct config
{
    // Add primary support
    bool newline;
    bool foreground;
    bool listtypes;
    bool clear;
    bool paste_once;
    bool id;
    bool search_by_type;
    char *seat;
    char *type;
    uint16_t limit;
    enum verb action;
};

static struct config options = {.foreground = false,
                                .listtypes = false,
                                .newline = false,
                                .id = false,
                                .search_by_type = false,
                                .clear = false,
                                .paste_once = false,
                                .limit = 10,
                                .seat = NULL,
                                .type = NULL,
                                .action = 0};

static const char help[] =
    "Usage: kapc [command] [options] <data>\n"
    "\n"
    "Commands:\n"
    "    copy  - Copies data to the Wayland clipboard\n"
    "    paste - Retrieves data from either the clipboard or history\n"
    "Options:\n"
    "    -h, --help            Show this help message\n"
    "    -v, --version         Show version number\n"
    "\n"
    "Use '--help' after a [command] to get more detailed options\n";

static const struct option copy[] = {{"help", no_argument, NULL, 'h'},
                                     {"version", no_argument, NULL, 'v'},
                                     {"foreground", no_argument, NULL, 'f'},
                                     {"trim-newline", no_argument, NULL, 'n'},
                                     {"paste-once", no_argument, NULL, 'o'},
                                     {"clear", no_argument, NULL, 'c'},
                                     {"type", required_argument, NULL, 't'},
                                     {"seat", required_argument, NULL, 's'},
                                     {0, 0, 0, 0}};

static const char copy_help[] =
    "Usage:\n"
    "    kapc copy [options] text to copy\n"
    "    kapc copy [options] < ./file-to-copy\n"
    "\n"
    "Options:\n"
    "    -h, --help            Show this help message\n"
    "    -v, --version         Show version number\n"
    "    -f, --foreground      Stay in foreground instead of forking\n"
    "    -n, --trim-newline    Do not copy the trailing newline\n"
    "    -o, --paste-once      Only serve one paste request and then exit\n"
    "    -c, --clear           Instead of copying, clear the clipboard\n"
    "    -s, --seat <seat>     Pick the seat to use\n"
    "    -t, --type <type>     Manually specify MIME type to offer\n";

static const struct option paste[] = {{"help", no_argument, NULL, 'h'},
                                      {"list-types", no_argument, NULL, 'l'},
                                      {"version", no_argument, NULL, 'v'},
                                      {"type", required_argument, NULL, 't'},
                                      {"no-newline", no_argument, NULL, 'n'},
                                      {"seat", required_argument, NULL, 's'},
                                      {0, 0, 0, 0}};

static const char paste_help[] =
    "Usage: kapc paste [options]\n"
    "\n"
    "Options:\n"
    "    -h, --help            Show this help message\n"
    "    -v, --version         Show version number\n"
    "    -l, --list-types      Instead of pasting, list the offered types\n"
    "    -n, --no-newline      Do not add a newline character\n"
    "    -s, --seat <seat>     Pick the seat to use\n"
    "    -t, --type <type>     Manually specify MIME type to paste\n";

static const struct option search[] = {{"help", no_argument, NULL, 'h'},
                                      {"version", no_argument, NULL, 'v'},
                                      {"limit", required_argument, NULL, 'l'},
                                      {"id", no_argument, NULL, 'i'},
                                      {"type", no_argument, NULL, 't'},
                                      {0, 0, 0, 0}};

static const char search_help[] =
    "Usage: kapc search [options]\n"
    "\n"
    "Options:\n"
    "    -h, --help            Show this help message\n"
    "    -v, --version         Show version number\n"
    "    -l, --limit <max>     Limit the number of entries returned from the search\n"
    "    -i, --id              Show only the ids of the entries found\n"
    "    -t, --type            Search by MIME type\n";

static void parse_options(int argc, char *argv[])
{
    if (argc < 2)
    {
        printf("%s", help);
        exit(EXIT_FAILURE);
    }

    /* Manually handle argv[1] */
    void *action;
    char *opt_string;
    if (!strcmp(argv[1], "copy"))
    {
        action = (void *)copy;
        opt_string = "hfnt:s:cov";
        options.action = COPY;
    }
    else if (!strcmp(argv[1], "paste"))
    {
        action = (void *)paste;
        opt_string = "hlt:s:nv";
        options.action = PASTE;
    }
    else if (!strcmp(argv[1], "search"))
    {
        action = (void *)search;
        opt_string = "hvl:it";
        options.action = SEARCH;
    }
    else if (!strcmp(argv[1], "--version") || !strcmp(argv[1], "-v"))
    {
        printf("Kaprica pre-release\n");
        exit(EXIT_SUCCESS);
    }
    else if (!strcmp(argv[1], "--help") || !strcmp(argv[1], "-h"))
    {
        printf("%s", help);
        exit(EXIT_SUCCESS);
    }
    else
    {
        printf("%s", help);
        exit(EXIT_FAILURE);
    }

    int c;
    while (true)
    {
        /* Since we handle argv[1] ourselves, pass a modified argc & argv
         * to 'hide' it from getopt */
        int option_index = 0;
        c = getopt_long((argc - 1), (argv + 1), opt_string,
                        (struct option *)action, &option_index);

        if (c == -1)
        {
            break;
        }

        switch (c)
        {
        case 'v':
            printf("Kaprica pre-release\n");
            exit(EXIT_SUCCESS);
        case 'f':
            options.foreground = true;
            break;
        case 'l':
            if (options.action == SEARCH)
            {
                options.limit = atoi(optarg);
                if (options.limit <= 0)
                {
                    fprintf(stderr,
                            "--limit requires a positive integer as an argument\n");
                    exit(EXIT_FAILURE);
                }
            }
            else
            {
                options.listtypes = true;
            }
            break;
        case 'i':
            options.id = true;
            break;
        case 'n':
            options.newline = true;
            break;
        case 'c':
            options.clear = true;
            break;
        case 'o':
            options.paste_once = true;
            break;
        case 't':
            if (options.action == SEARCH)
            {
                options.search_by_type = true;
            }
            else if (!optarg)
            {
                printf("Missing required argument for --type\n");
                exit(EXIT_FAILURE);
            }
            else
            {
                options.type = xstrdup(optarg);
            }
            break;
        case '?':
        case 'h':
        default:
            if (options.action == COPY)
            {
                printf("%s", copy_help);
            }
            else if (options.action == PASTE)
            {
                printf("%s", paste_help);
            }
            else if (options.action == SEARCH)
            {
                printf("%s", search_help);
            }
            exit(EXIT_FAILURE);
        }
    }
    /* Shift optind back to the correct index */
    optind++;
}

static uint32_t trim_newline(void* data, uint32_t length)
{
    /* Check last two bytes for newline in case data
     * is null ended */
    if (((char *) data)[length - 1] == '\n')
    {
        length -= 1;
    }
    else if (((char *) data)[length - 2] == '\n')
    {
        length -= 2;
    }

    return length;
}

/* Only used if were passed a file via '<' on the cli */
static void get_stdin(source_buffer *input)
{
    input->data[0] = xmalloc(MAX_DATA_SIZE);
    input->len[0] = fread(input->data[0], 1, MAX_DATA_SIZE, stdin);
    input->data[0] = xrealloc(input->data[0], input->len[0]);
    if (!feof(stdin))
    {
        fprintf(stderr, "File is too large to copy\n");
        exit(EXIT_FAILURE);
    }
}

/* Concatenates argv with spaces in between */
static uint32_t concatenate_argv(uint16_t args, char *input[],
        source_buffer *output)
{
    int total = args;
    for (int i = 0; i < args; i++)
    {
        total += strlen(input[i]);
    }
    output->data[0] = xmalloc((sizeof(char) * total) + 1);
    /* strcat requires string to be null-ended */
    ((char *) output->data[0])[0] = '\0';

    for (int i = 0; i < args; i++)
    {
        strcat(output->data[0], input[i]);
        if (i != (args - 1))
        {
            strcat(output->data[0], " ");
        }
    }
    return (strlen(output->data[0]));
}

int main(int argc, char *argv[])
{
    parse_options(argc, argv);

    clip = clip_init();

    source_buffer *src = clip->selection_s;
    offer_buffer *ofr = clip->selection_o;
    if (options.action == COPY)
    {
        if (options.clear)
        {
            clip_clear_selection(clip);
            wl_display_roundtrip(clip->display);
            exit(EXIT_SUCCESS);
        }

        if (argv[optind])
        {
            if ((argc - optind) == 1)
            {
                src->data[0] = xstrdup(argv[optind]);
                src->len[0] = strlen(argv[optind]);
            }
            else
            {
                /* Pass everything not handled by getopt or main */
                src->len[0] = concatenate_argv((argc - optind), (argv + optind), src);
            }
        }
        else
        {
            get_stdin(src);
        }

        if (options.newline)
        {
            src->len[0] = trim_newline(src->data[0], src->len[0]);
        }
        if (options.paste_once)
        {
            src->offer_once = true;
        }

        if (options.type)
        {
            src->types[0].type = options.type;
        }
        else
        {
            guess_mime_types(src);
        }

        clip_set_selection(clip);
        if (!options.foreground)
        {
            pid_t pid = fork();
            if (pid < 0)
            {
                fprintf(stderr, "Failed to fork\n");
                exit(EXIT_FAILURE);
            }
            else if (pid > 0)
            {
                exit(EXIT_SUCCESS);
            }
        }

        while (wl_display_dispatch(clip->display) >= 0);
    }
    else if (options.action == PASTE)
    {
        clip_watch(clip);
        wl_display_roundtrip(clip->display);
        if (!ofr->offer)
        {
            printf("Buffer is unset\n");
            exit(EXIT_SUCCESS);
        }

        while (!ofr->num_types)
        {
            wl_display_dispatch(clip->display);
        }
        clip_get_selection(clip);

        if (options.listtypes)
        {
            for (int i = 0; i < src->num_types; i++)
            {
                printf("%s\n", src->types[i].type);
            }
            exit(EXIT_SUCCESS);
        }

        int type = 0;
        bool type_found = false;

        if (options.type)
        {
            for (int i = 0; i < src->num_types; i++)
            {
                if (!strcmp(src->types[i].type, options.type))
                {
                    type = i;
                    type_found = true;
                }
            }
            if (!type_found)
            {
                fprintf(stderr, "Could not find mime type: %s\n", options.type);
                exit(EXIT_FAILURE);
            }
        }
        else
        {
            type = find_write_type(src);
        }

        write(STDOUT_FILENO, src->data[type], src->len[type]);

        if (!options.newline)
        {
            printf("\n");
        }
    }
    else if (options.action == SEARCH)
    {
        sqlite3 *db = database_init();
        if (argv[optind])
        {
            if ((argc - optind) == 1)
            {
                src->data[0] = xstrdup(argv[optind]);
                src->len[0] = strlen(argv[optind]);
            }
            else
            {
                /* Pass everything not handled by getopt or main */
                src->len[0] = concatenate_argv((argc - optind), (argv + optind), src);
            }
        }

        uint32_t *ids = xmalloc(sizeof(uint32_t) * options.limit);
        uint16_t found =
            database_find_matching_source(db, src->data[0],
                    src->len[0], options.limit, ids, options.search_by_type);

        for (int i = 0; i < found; i++)
        {
            source_clear(src);
            database_get_source(db, ids[i], src);
            printf("ID: %d", ids[i]);
            if (!options.id)
            {
                printf(" \"%s\"...", src->snippet);
            }
            printf("\n");
        }

        free(ids);
        database_destroy(db);
    }

    clip_destroy(clip);
}
