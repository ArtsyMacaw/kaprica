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
    SEARCH,
    DELETE
};

struct config
{
    // TODO: Add primary support
    bool newline;
    bool foreground;
    bool listtypes;
    bool clear;
    bool paste_once;
    bool list;
    bool id;
    bool reverse_search;
    char accept;
    char *db_path;
    bool snippets;
    enum search_type search_type;
    char *type;
    int64_t limit;
    enum verb action;
};

static struct config options = {.foreground = false,
                                .listtypes = false,
                                .newline = false,
                                .list = false,
                                .id = false,
                                .reverse_search = false,
                                .accept = 'n',
                                .db_path = NULL,
                                .snippets = false,
                                .search_type = CONTENT,
                                .clear = false,
                                .paste_once = false,
                                .limit = -1,
                                .type = NULL,
                                .action = 0};

static const char help[] =
    "Usage: kapc [command] [options] <data>\n"
    "Interact with the Wayland clipboard and history database\n"
    "\n"
    "Commands:\n"
    "    copy   - Copies data to the Wayland clipboard\n"
    "    paste  - Retrieves data from either the clipboard or history\n"
    "    search - Searches through history database\n"
    "    delete - Delete entries from the history database\n"
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
                                     {"id", no_argument, NULL, 'i'},
                                     {"type", required_argument, NULL, 't'},
                                     {"reverse-search", no_argument, NULL, 'r'},
                                     {"database", required_argument, NULL, 'D'},
                                     {0, 0, 0, 0}};

static const char copy_help[] =
    "Usage:\n"
    "    kapc copy [options] text to copy\n"
    "    kapc copy [options] < ./file-to-copy\n"
    "Copy contents to the Wayland clipboard\n"
    "\n"
    "Options:\n"
    "    -h, --help             Show this help message\n"
    "    -v, --version          Show version number\n"
    "    -f, --foreground       Stay in foreground instead of forking\n"
    "    -n, --trim-newline     Do not copy the trailing newline\n"
    "    -i, --id               Copy given id to clipboard\n"
    "    -o, --paste-once       Only serve one paste request and then exit\n"
    "    -c, --clear            Instead of copying, clear the clipboard\n"
    "    -t, --type <mime/type> Manually specify MIME type to offer\n"
    "    -D, --database </path> Specify the path to the history database\n"
    "    -r, --reverse-search   Looks for a given snippet and copies the entry "
    "from the history database\n";

static const struct option paste[] = {
    {"help", no_argument, NULL, 'h'},
    {"list-types", no_argument, NULL, 'l'},
    {"version", no_argument, NULL, 'v'},
    {"id", no_argument, NULL, 'i'},
    {"no-newline", no_argument, NULL, 'n'},
    {"type", required_argument, NULL, 't'},
    {"database", required_argument, NULL, 'D'},
    {0, 0, 0, 0}};

static const char paste_help[] =
    "Usage: kapc paste [options]\n"
    "Paste contents from the Wayland clipboard\n"
    "\n"
    "Options:\n"
    "    -h, --help             Show this help message\n"
    "    -v, --version          Show version number\n"
    "    -l, --list-types       Instead of pasting, list the offered types\n"
    "    -n, --no-newline       Do not add a newline character\n"
    "    -i, --id               Paste one or more id's from history\n"
    "    -t, --type <mime/type> Manually specify MIME type to paste\n"
    "    -D, --database </path> Specify the path to the history database\n";

static const struct option search[] = {
    {"help", no_argument, NULL, 'h'},
    {"version", no_argument, NULL, 'v'},
    {"limit", required_argument, NULL, 'l'},
    {"id", no_argument, NULL, 'i'},
    {"snippet", no_argument, NULL, 's'},
    {"list", no_argument, NULL, 'L'},
    {"type", no_argument, NULL, 't'},
    {"glob", no_argument, NULL, 'g'},
    {"database", required_argument, NULL, 'D'},
    {0, 0, 0, 0}};

static const char search_help[] =
    "Usage: kapc search [options] <search-term>\n"
    "Search through the history database\n"
    "\n"
    "Options:\n"
    "    -h, --help             Show this help message\n"
    "    -v, --version          Show version number\n"
    "    -l, --limit <0-x>      Limit the number of entries returned from the "
    "search\n"
    "    -i, --id               Show only the ids of the entries found\n"
    "    -s, --snippet          Show only the snippets of the entries found\n"
    "    -t, --type             Search by MIME type\n"
    "    -g, --glob             Search by glob pattern\n"
    "    -L, --list             Output in machine-readable format\n"
    "    -D, --database </path> Specify the path to the history database\n";

static const struct option delete[] = {
    {"help", no_argument, NULL, 'h'},
    {"version", no_argument, NULL, 'v'},
    {"limit", required_argument, NULL, 'l'},
    {"id", no_argument, NULL, 'i'},
    {"type", no_argument, NULL, 't'},
    {"accept", no_argument, NULL, 'a'},
    {"glob", no_argument, NULL, 'g'},
    {"database", required_argument, NULL, 'D'},
    {0, 0, 0, 0}};

static const char delete_help[] =
    "Usage: kapc delete [options] <search-term>\n"
    "Delete entries from the history database\n"
    "\n"
    "Options:\n"
    "    -h, --help             Show this help message\n"
    "    -v, --version          Show version number\n"
    "    -l, --limit <0-x>      Limit the number of entries deleted\n"
    "    -a, --accept           Don't ask for confirmation when deleting "
    "entries\n"
    "    -g, --glob             Search by glob pattern\n"
    "    -t, --type             Delete by MIME type\n"
    "    -i, --id               Delete one or more id's from history\n"
    "    -D, --database </path> Specify the path to the history database\n";

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
        opt_string = "hfnt:coviD:r";
        options.action = COPY;
    }
    else if (!strcmp(argv[1], "paste"))
    {
        action = (void *)paste;
        opt_string = "hlt:nviD:";
        options.action = PASTE;
    }
    else if (!strcmp(argv[1], "search"))
    {
        action = (void *)search;
        opt_string = "hvl:itLsD:g";
        options.action = SEARCH;
    }
    else if (!strcmp(argv[1], "delete"))
    {
        action = (void *)delete;
        opt_string = "hvl:itaD:g";
        options.action = DELETE;
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
    /* Since we handle argv[1] ourselves, pass a modified argc & argv
     * to 'hide' it from getopt */
    while ((c = getopt_long((argc - 1), (argv + 1), opt_string,
                            (struct option *)action, NULL)) != -1)
    {
        switch (c)
        {
        case 'v':
            printf("Kaprica pre-release\n");
            exit(EXIT_SUCCESS);
        case 'f':
            options.foreground = true;
            break;
        case 'L':
            options.list = true;
            break;
        case 'l':
            if (options.action == SEARCH || options.action == DELETE)
            {
                if (!strcmp(optarg, "all"))
                {
                    options.limit = -1;
                }
                else
                {
                    options.limit = atoi(optarg);
                    if (options.limit <= 0)
                    {
                        fprintf(stderr, "--limit requires a positive integer "
                                        "as an argument\n");
                        exit(EXIT_FAILURE);
                    }
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
        case 'r':
            options.reverse_search = true;
            break;
        case 'a':
            options.accept = 'a';
            break;
        case 'g':
            options.search_type = GLOB;
            break;
        case 'D':
            options.db_path = xstrdup(optarg);
            break;
        case 'n':
            options.newline = true;
            break;
        case 'c':
            options.clear = true;
            break;
        case 's':
            options.snippets = true;
            break;
        case 'o':
            options.paste_once = true;
            break;
        case 't':
            if (options.action == SEARCH || options.action == DELETE)
            {
                options.search_type = MIME_TYPE;
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
            else if (options.action == DELETE)
            {
                printf("%s", delete_help);
            }
            exit(EXIT_FAILURE);
        }
    }
    /* Shift optind back to the correct index */
    optind++;
}

static size_t trim_newline(void *data, size_t length)
{
    /* Check last two bytes for newline in case data
     * is null ended */
    if (((char *)data)[length - 1] == '\n')
    {
        length -= 1;
    }
    else if (((char *)data)[length - 2] == '\n')
    {
        length -= 2;
    }

    return length;
}

/* Only used if were passed a file via '<' on the cli */
static bool read_stdin_fd(source_buffer *input)
{
    input->data[0] = xmalloc(MAX_DATA_SIZE);
    input->len[0] = fread(input->data[0], 1, MAX_DATA_SIZE, stdin);
    input->data[0] = xrealloc(input->data[0], input->len[0]);
    input->types[0] = NULL;
    input->num_types = 1;
    if (!feof(stdin))
    {
        fprintf(stderr, "File is too large to copy\n");
        return false;
    }
    else
    {
        return true;
    }
}

/* Concatenates argv with spaces in between */
static bool concatenate_argv(int args, char *argv[], source_buffer *input)
{
    input->num_types = 1;
    input->types[0] = NULL;

    if (args == 1)
    {
        input->data[0] = xstrdup(argv[0]);
        input->len[0] = strlen(argv[0]);
        return true;
    }

    int total = args;
    for (int i = 0; i < args; i++)
    {
        total += strlen(argv[i]);
    }

    if (total > MAX_DATA_SIZE)
    {
        fprintf(stderr, "Data is too large to copy\n");
        return false;
    }

    input->data[0] = xmalloc((sizeof(char) * total) + 1);
    /* strcat requires string to be null-ended */
    ((char *)input->data[0])[0] = '\0';

    for (int i = 0; i < args; i++)
    {
        strcat(input->data[0], argv[i]);
        if (i != (args - 1))
        {
            strcat(input->data[0], " ");
        }
    }
    input->len[0] = strlen(input->data[0]);

    return true;
}

static bool get_stdin(int args, char *input[], source_buffer *output)
{
    if (isatty(STDIN_FILENO))
    {
        return concatenate_argv(args, input, output);
    }
    else
    {
        return read_stdin_fd(output);
    }
}

/* Processes argv[] into an array of integers */
static int64_t *seperate_argv_into_ids(int args, char *argv[],
                                       uint32_t *num_of_ids)
{
    int64_t *ids = xmalloc(sizeof(int64_t) * args);
    int64_t tmp;
    for (int i = 0; i < args; i++)
    {
        tmp = atoll(argv[i]);
        if (!tmp)
        {
            fprintf(stderr, "Invalid id %s\n", argv[i]);
        }
        else
        {
            ids[*num_of_ids] = tmp;
            (*num_of_ids)++;
        }
    }
    return ids;
}

/* Processes stdin into an array of integers */
static int64_t *seperate_stdin_into_ids(uint32_t *num_of_ids)
{
    size_t array_size = 100;
    int64_t *ids = xmalloc(sizeof(int64_t) * array_size);
    size_t len = 0;
    char *token = NULL;

    while (getline(&token, &len, stdin) != -1)
    {
        if (array_size == *num_of_ids)
        {
            array_size *= 2;
            ids = xrealloc(ids, sizeof(int64_t) * array_size);
        }

        ids[*num_of_ids] = atoll(token);
        if (!ids[*num_of_ids])
        {
            fprintf(stderr, "Invalid id %s\n", token);
        }
        else
        {
            (*num_of_ids)++;
        }
    }
    ids = xrealloc(ids, sizeof(int64_t) * (*num_of_ids));

    return ids;
}

static int64_t *get_ids(int args, char *argv[], uint32_t *num_of_ids)
{
    if (isatty(STDIN_FILENO))
    {
        return seperate_argv_into_ids(args, argv, num_of_ids);
    }
    else
    {
        return seperate_stdin_into_ids(num_of_ids);
    }
}

void write_to_stdout(source_buffer *src)
{
    int type;
    bool type_found = false;
    if (options.type)
    {
        for (int i = 0; i < src->num_types; i++)
        {
            if (!strcmp(src->types[i], options.type))
            {
                type = i;
                type_found = true;
            }
        }
    }
    if (!type_found)
    {
        type = find_write_type(src);
    }
    write(STDOUT_FILENO, src->data[type], src->len[type]);
    if (!options.newline)
    {
        printf("\n");
    }
}

int main(int argc, char *argv[])
{
    parse_options(argc, argv);

    /* Separate already processed arguments from the rest of the argv */
    int num_of_args = argc - optind;
    char **args = argv + optind;

    sqlite3 *db = NULL;
    int64_t *ids = NULL;

    clip = clip_init();
    source_buffer *src = clip->selection_source;

    if (options.action == COPY)
    {
        if (options.clear)
        {
            clip_clear_selection(clip);
            wl_display_roundtrip(clip->display);
            goto cleanup;
        }
        else if (options.id)
        {
            db = database_open(options.db_path);
            uint32_t num_of_ids = 0;
            ids = get_ids(num_of_args, args, &num_of_ids);
            if (num_of_ids != 1)
            {
                fprintf(stderr, "Only one id can be copied at a time\n");
                goto cleanup;
            }
            if (!database_get_entry(db, ids[0], src))
            {
                printf("ID: %ld not found\n", ids[0]);
                goto cleanup;
            }
        }
        else
        {
            if (!get_stdin(num_of_args, args, src))
            {
                goto cleanup;
            }
        }

        /* If were passed a snippet from `kapc search` it will add a newline so
         * it needs to be trimmed */
        if (options.newline || options.reverse_search)
        {
            src->len[0] = trim_newline(src->data[0], src->len[0]);
        }
        if (options.reverse_search)
        {
            db = database_open(options.db_path);
            int64_t id =
                database_find_entry_from_snippet(db, src->data[0], src->len[0]);

            if (id == 0)
            {
                fprintf(stderr, "Snippet not found\n");
                goto cleanup;
            }

            source_clear(src);
            database_get_entry(db, id, src);
        }
        if (options.paste_once)
        {
            src->offer_once = true;
        }

        if (options.id)
        {
            /* Empty */
        }
        else if (options.reverse_search)
        {
            /* Empty */
        }
        else if (options.type)
        {
            src->types[0] = options.type;
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
                perror("fork");
                exit(EXIT_FAILURE);
            }
            else if (pid > 0)
            {
                exit(EXIT_SUCCESS);
            }
        }

        while (wl_display_dispatch(clip->display) >= 0)
            ;
    }
    else if (options.action == PASTE)
    {
        if (options.id)
        {
            db = database_open(options.db_path);
            uint32_t num_of_ids = 0;
            ids = get_ids(num_of_args, args, &num_of_ids);

            for (int i = 0; i < num_of_ids; i++)
            {
                if (database_get_entry(db, ids[i], src))
                {
                    write_to_stdout(src);
                    source_clear(src);
                }
                else
                {
                    fprintf(stderr, "ID: %ld not found\n", ids[i]);
                }
            }
            goto cleanup;
        }

        clip_watch(clip);
        wl_display_roundtrip(clip->display);

        if (!clip_get_selection(clip))
        {
            printf("Buffer is unset\n");
            goto cleanup;
        }

        if (options.listtypes)
        {
            for (int i = 0; i < src->num_types; i++)
            {
                printf("%s\n", src->types[i]);
            }
        }
        else
        {
            write_to_stdout(src);
        }
    }
    else if (options.action == SEARCH)
    {
        db = database_open(options.db_path);
        if (!get_stdin(num_of_args, args, src))
        {
            goto cleanup;
        }

        if (options.limit == -1)
        {
            options.limit = database_get_total_entries(db);
        }
        ids = xmalloc(sizeof(int64_t) * options.limit);

        uint32_t found = database_find_matching_entries(
            db, src->data[0], src->len[0], options.limit, ids,
            options.search_type);

        for (int i = 0; i < found; i++)
        {
            source_clear(src);
            char *snippet = database_get_snippet(db, ids[i]);

            if (!options.snippets)
            {
                if (!options.list)
                {
                    printf("ID: ");
                }
                printf("%ld", ids[i]);
            }
            if (!options.id && !options.snippets)
            {
                printf(" ");
            }
            if (!options.id)
            {
                if (!options.list)
                {
                    printf("\"");
                }
                printf("%s", snippet);
                if (!options.list)
                {
                    printf("...\"");
                }
            }
            printf("\n");

            free(snippet);
        }
    }
    else if (options.action == DELETE)
    {
        db = database_open(options.db_path);
        uint32_t found = 0;
        if (options.id)
        {
            ids = get_ids(num_of_args, args, &found);
        }
        else
        {
            if (!get_stdin(num_of_args, args, src))
            {
                goto cleanup;
            }
            if (options.limit == -1)
            {
                options.limit = database_get_total_entries(db);
            }
            ids = xmalloc(sizeof(int64_t) * options.limit);
            found = database_find_matching_entries(db, src->data[0],
                                                   src->len[0], options.limit,
                                                   ids, options.search_type);
        }

        char *input = NULL;
        char tmp = options.accept;

        for (int i = 0; i < found; i++)
        {
            source_clear(src);
            database_get_entry(db, ids[i], src);
            if (tmp != 'A' && tmp != 'a')
            {
                printf("%s\n", src->snippet);
                printf("Delete? (Yes/No/All): ");

                getline(&input, &(size_t){0}, stdin);
                tmp = input[0];
                free(input);
            }
            if (tmp == 'Y' || tmp == 'y' || tmp == 'A' || tmp == 'a')
            {
                database_delete_entry(db, ids[i]);
            }
        }
    }

cleanup:
    if (db)
    {
        database_close(db);
    }
    if (ids)
    {
        free(ids);
    }
    clip_destroy(clip);
}
