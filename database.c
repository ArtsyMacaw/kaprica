#define _POSIX_C_SOURCE 200112L
#include <sqlite3.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "database.h"
#include "clipboard.h"
#include "xmalloc.h"

/* Bootstrapping statements */
static sqlite3_stmt *create_main_table, *create_content_table;
/* Pragma statements */
static sqlite3_stmt *pragma_foreign_keys, *pragma_secure_delete,
    *pragma_auto_vacuum, *pragma_optimize;
/* Index statements */
static sqlite3_stmt *create_data_index, *create_mime_index,
    *create_snippet_index, *create_thumbnail_index, *create_timestamp_index,
    *create_hash_index;
/* Insertion statements */
static sqlite3_stmt *insert_entry, *insert_entry_content;
/* Search statements */
static sqlite3_stmt *find_matching_entries, *find_matching_types,
    *find_entry_from_snippet, *find_matching_entries_glob;
/* Retrieval statements */
static sqlite3_stmt *select_latest_entries, *select_entry, *select_snippet,
    *select_thumbnail, *total_entries, *select_size;
/* Deletion statements */
static sqlite3_stmt *delete_entry, *delete_old_entries, *delete_last_entries,
    *delete_duplicate_entries, *delete_large_entries;

#define FIVE_HUNDRED_MS 5
struct timespec one_hundred_ms = {.tv_nsec = 100000000};

enum datatype
{
    BLOB,
    INT,
    TEXT
};

/* Insert into clipboard_history table */
#define SNIPPET_BINDING 1
#define THUMBNAIL_BINDING 2
#define HASH_BINDING 3
/* Insert into content table*/
#define ENTRY_BINDING 1
#define LENGTH_BINDING 2
#define DATA_BINDING 3
#define MIME_TYPE_BINDING 4
/* Search by content */
#define MATCH_BINDING 1
/* Search by id */
#define ID_BINDING 1
/* Delete old entries binding */
#define DATE_BINDING 1

static void prepare_statement(sqlite3 *db, const char *s, sqlite3_stmt **stmt)
{
    int ret = sqlite3_prepare_v2(db, s, -1, stmt, NULL);
    if (ret != SQLITE_OK)
    {
        fprintf(stderr, "Database error: %s\n", sqlite3_errmsg(db));
        exit(EXIT_FAILURE);
    }
}

/* Only statements necessary to create and initialize
 * the database if it did not already exist */
static void prepare_bootstrap_statements(sqlite3 *db)
{
    const char foreign_keys[] = "PRAGMA foreign_keys = ON;";
    prepare_statement(db, foreign_keys, &pragma_foreign_keys);

    const char secure_delete[] = "PRAGMA secure_delete = OFF;";
    prepare_statement(db, secure_delete, &pragma_secure_delete);

    const char auto_vacuum[] = "PRAGMA auto_vacuum = NONE;";
    prepare_statement(db, auto_vacuum, &pragma_auto_vacuum);

    const char optimize[] = "PRAGMA optimize;";
    prepare_statement(db, optimize, &pragma_optimize);

    const char main_table[] =
        "CREATE TABLE IF NOT EXISTS clipboard_history ("
        "    history_id INTEGER PRIMARY KEY,"
        "    timestamp DATETIME NOT NULL DEFAULT (datetime('now')),"
        "    snippet TEXT NOT NULL,"
        "    thumbnail BLOB,"
        "    hash TEXT NOT NULL);";
    prepare_statement(db, main_table, &create_main_table);

    const char content_table[] =
        "CREATE TABLE IF NOT EXISTS content ("
        "    entry INTEGER,"
        "    length INTEGER NOT NULL,"
        "    data BLOB NOT NULL,"
        "    mime_type TEXT NOT NULL,"
        "    FOREIGN KEY (entry) REFERENCES clipboard_history(history_id)"
        "       ON DELETE CASCADE);";
    prepare_statement(db, content_table, &create_content_table);
}

/* Prepare all index statements, should only be needed to be called by
 * kapricad when creating the database */
static void prepare_index_statements(sqlite3 *db)
{
    const char data_index[] = "CREATE INDEX IF NOT EXISTS data_index"
                              "    ON content (data);";
    prepare_statement(db, data_index, &create_data_index);

    const char mime_index[] = "CREATE INDEX IF NOT EXISTS mime_index"
                              "    ON content (mime_type);";
    prepare_statement(db, mime_index, &create_mime_index);

    const char snippet_index[] = "CREATE INDEX IF NOT EXISTS snippet_index"
                                 "    ON clipboard_history (snippet);";
    prepare_statement(db, snippet_index, &create_snippet_index);

    const char thumbnail_index[] = "CREATE INDEX IF NOT EXISTS thumbnail_index"
                                   "    ON clipboard_history (thumbnail);";
    prepare_statement(db, thumbnail_index, &create_thumbnail_index);

    const char timestamp_index[] = "CREATE INDEX IF NOT EXISTS timestamp_index"
                                   "    ON clipboard_history (timestamp);";
    prepare_statement(db, timestamp_index, &create_timestamp_index);

    const char hash_index[] = "CREATE INDEX IF NOT EXISTS hash_index"
                              "    ON clipboard_history (hash);";
    prepare_statement(db, hash_index, &create_hash_index);
}

/* Preparing statements is relatively costly resource wise
 * so we frontload all of them at the start and only
 * finalize them when the program is stopped */
static void prepare_all_statements(sqlite3 *db)
{
    const char insert_entry_history[] =
        "INSERT INTO clipboard_history (snippet, thumbnail, hash)"
        "                       VALUES (?1,      ?2,        ?3);";
    prepare_statement(db, insert_entry_history, &insert_entry);

    const char entry_content[] =
        "INSERT INTO content (entry, length, data, mime_type)"
        "    VALUES          (?1,    ?2,     ?3,   ?4);";
    prepare_statement(db, entry_content, &insert_entry_content);

    const char get_size[] = "SELECT page_count * page_size"
                            "    FROM pragma_page_count,"
                            "         pragma_page_size;";
    prepare_statement(db, get_size, &select_size);

    const char get_latest_entries[] = "SELECT history_id FROM clipboard_history"
                                      "    ORDER BY timestamp DESC"
                                      "    LIMIT ?1 OFFSET ?2;";
    prepare_statement(db, get_latest_entries, &select_latest_entries);

    const char get_entry[] = "SELECT * FROM content"
                             "    WHERE entry = ?1;";
    prepare_statement(db, get_entry, &select_entry);

    const char get_snippet[] = "SELECT snippet FROM clipboard_history"
                               "   WHERE history_id = ?1;";
    prepare_statement(db, get_snippet, &select_snippet);

    const char get_thumbnail[] = "SELECT thumbnail FROM clipboard_history"
                                 "   WHERE history_id = ?1;";
    prepare_statement(db, get_thumbnail, &select_thumbnail);

    const char get_total_entries[] =
        "SELECT COUNT(history_id) FROM clipboard_history;";
    prepare_statement(db, get_total_entries, &total_entries);

    const char find_entry[] = "SELECT DISTINCT entry FROM content"
                              "    WHERE data LIKE '%' || ?1 || '%'"
                              "    ORDER BY entry DESC;";
    prepare_statement(db, find_entry, &find_matching_entries);

    const char find_entry_type[] = "SELECT DISTINCT entry FROM content"
                                   "    WHERE mime_type LIKE '%' || ?1 || '%'"
                                   "    ORDER BY entry DESC;";
    prepare_statement(db, find_entry_type, &find_matching_types);

    const char find_entry_snippet[] = "SELECT history_id FROM clipboard_history"
                                      "    WHERE snippet=?1;";
    prepare_statement(db, find_entry_snippet, &find_entry_from_snippet);

    const char find_entry_glob[] = "SELECT DISTINCT entry FROM content"
                                   "    WHERE data GLOB ?1"
                                   "    ORDER BY entry DESC;";
    prepare_statement(db, find_entry_glob, &find_matching_entries_glob);

    const char remove_entry[] = "DELETE FROM clipboard_history"
                                "    WHERE history_id = ?1;";
    prepare_statement(db, remove_entry, &delete_entry);

    const char remove_old_entry[] =
        "DELETE FROM clipboard_history"
        "    WHERE timestamp < (date('now', ? || ' days'));";
    prepare_statement(db, remove_old_entry, &delete_old_entries);

    const char remove_last_entries[] = "DELETE FROM clipboard_history"
                                       "    WHERE history_id IN("
                                       "        SELECT history_id"
                                       "            FROM clipboard_history"
                                       "            ORDER BY timestamp DESC"
                                       "            LIMIT ?1);";
    prepare_statement(db, remove_last_entries, &delete_last_entries);

    const char remove_duplicates[] = "DELETE FROM clipboard_history"
                                     "    WHERE history_id NOT IN("
                                     "        SELECT MAX(history_id)"
                                     "            FROM clipboard_history"
                                     "            GROUP BY hash"
                                     "        ORDER BY timestamp DESC"
                                     "    );";
    prepare_statement(db, remove_duplicates, &delete_duplicate_entries);

    const char remove_large_entries[] =
        "DELETE FROM clipboard_history"
        "    WHERE history_id IN("
        "        SELECT DISTINCT entry FROM content"
        "            ORDER BY length DESC"
        "            LIMIT ?1);";
    prepare_statement(db, remove_large_entries, &delete_large_entries);
}

static int execute_statement(sqlite3_stmt *stmt)
{
    int ret, time_passed;

    ret = sqlite3_step(stmt);
    while (ret == SQLITE_BUSY)
    {
        time_passed = nanosleep(&one_hundred_ms, NULL);
        ret = sqlite3_step(stmt);
        if (time_passed >= FIVE_HUNDRED_MS)
        {
            fprintf(stderr, "Timed out accessing database\n");
            break;
        }
    }

    if (ret == SQLITE_MISUSE || ret == SQLITE_ERROR)
    {
        fprintf(stderr, "Database Error: %s\n", sqlite3_errstr(ret));
        exit(EXIT_FAILURE);
    }

    return ret;
}

static void bind_statement(sqlite3_stmt *stmt, uint16_t literal, void *data,
                           size_t length, enum datatype type)
{
    int ret;
    switch (type)
    {
    case INT:
        ret = sqlite3_bind_int(stmt, literal, *(int *)data);
        break;
    case BLOB:
        ret = sqlite3_bind_blob64(stmt, literal, data, length, SQLITE_STATIC);
        break;
    case TEXT:
        ret = sqlite3_bind_text(stmt, literal, (char *)data, length,
                                SQLITE_STATIC);
        break;
    }

    if (ret == SQLITE_NOMEM || ret == SQLITE_TOOBIG || ret == SQLITE_RANGE)
    {
        fprintf(stderr, "Database error: %s\n", sqlite3_errstr(ret));
        exit(EXIT_FAILURE);
    }
}

void database_delete_entry(sqlite3 *db, int64_t id)
{
    bind_statement(delete_entry, ID_BINDING, &id, 0, INT);
    execute_statement(delete_entry);

    sqlite3_reset(delete_entry);
    sqlite3_clear_bindings(delete_entry);
}

uint32_t database_delete_duplicate_entries(sqlite3 *db)
{
    execute_statement(delete_duplicate_entries);
    sqlite3_reset(delete_duplicate_entries);

    return sqlite3_changes(db);
}

uint32_t database_delete_old_entries(sqlite3 *db, int32_t days)
{
    bind_statement(delete_old_entries, DATE_BINDING, &days, 0, INT);
    execute_statement(delete_old_entries);
    sqlite3_reset(delete_old_entries);
    sqlite3_clear_bindings(delete_old_entries);

    return sqlite3_changes(db);
}

uint32_t database_delete_last_entries(sqlite3 *db, uint32_t num_of_entries)
{
    bind_statement(delete_last_entries, ENTRY_BINDING, &num_of_entries, 0, INT);
    execute_statement(delete_last_entries);
    sqlite3_reset(delete_last_entries);
    sqlite3_clear_bindings(delete_last_entries);

    return sqlite3_changes(db);
}

uint32_t database_delete_largest_entries(sqlite3 *db, uint32_t num_of_entries)
{
    bind_statement(delete_large_entries, ENTRY_BINDING, &num_of_entries, 0,
                   INT);
    execute_statement(delete_large_entries);
    sqlite3_reset(delete_large_entries);
    sqlite3_clear_bindings(delete_large_entries);

    /* Get number of changes before we vacuum */
    int ret = sqlite3_changes(db);

    sqlite3_exec(db, "VACUUM;", NULL, NULL, NULL);

    return ret;
}

void database_insert_entry(sqlite3 *db, source_buffer *src)
{
    bind_statement(insert_entry, SNIPPET_BINDING, src->snippet,
                   strlen(src->snippet), TEXT);
    bind_statement(insert_entry, THUMBNAIL_BINDING, src->thumbnail,
                   src->thumbnail_len, BLOB);
    bind_statement(insert_entry, HASH_BINDING, src->data_hash,
                   strlen(src->data_hash), TEXT);

    execute_statement(insert_entry);

    sqlite3_reset(insert_entry);
    sqlite3_clear_bindings(insert_entry);

    uint32_t rowid = sqlite3_last_insert_rowid(db);
    for (int i = 0; i < src->num_types; i++)
    {
        bind_statement(insert_entry_content, ENTRY_BINDING, &rowid, 0, INT);
        bind_statement(insert_entry_content, LENGTH_BINDING, &src->len[i], 0,
                       INT);
        bind_statement(insert_entry_content, DATA_BINDING, src->data[i],
                       src->len[i], BLOB);
        bind_statement(insert_entry_content, MIME_TYPE_BINDING, src->types[i],
                       strlen(src->types[i]), TEXT);

        execute_statement(insert_entry_content);

        sqlite3_reset(insert_entry_content);
        sqlite3_clear_bindings(insert_entry_content);
    }

    database_delete_duplicate_entries(db);
}
uint32_t database_find_matching_entries(sqlite3 *db, void *match, size_t length,
                                        uint32_t num_of_entries,
                                        int64_t *list_of_ids,
                                        enum search_type type)
{
    sqlite3_stmt *search;

    if (type == MIME_TYPE)
    {
        search = find_matching_types;
    }
    else if (type == CONTENT)
    {
        search = find_matching_entries;
    }
    else if (type == GLOB)
    {
        search = find_matching_entries_glob;
    }
    else
    {
        fprintf(stderr, "Invalid search type\n");
        exit(EXIT_FAILURE);
    }

    bind_statement(search, MATCH_BINDING, match, length, BLOB);

    int counter = 0;
    while (execute_statement(search) != SQLITE_DONE)
    {
        list_of_ids[counter] = sqlite3_column_int(search, 0);
        counter++;
        if (counter >= num_of_entries)
        {
            break;
        }
    }

    sqlite3_reset(search);
    sqlite3_clear_bindings(search);

    return counter;
}

int64_t database_find_entry_from_snippet(sqlite3 *db, char *snippet,
                                         size_t length)
{
    bind_statement(find_entry_from_snippet, MATCH_BINDING, snippet, length,
                   TEXT);

    int64_t id = 0;
    if (execute_statement(find_entry_from_snippet) != SQLITE_DONE)
    {
        id = sqlite3_column_int(find_entry_from_snippet, 0);
    }

    sqlite3_reset(find_entry_from_snippet);
    sqlite3_clear_bindings(find_entry_from_snippet);

    return id;
}

uint64_t database_get_size(sqlite3 *db)
{
    execute_statement(select_size);
    uint64_t size = sqlite3_column_int(select_size, 0);
    sqlite3_reset(select_size);

    return size;
}

uint32_t database_get_latest_entries(sqlite3 *db, uint32_t num_of_entries,
                                     uint32_t offset, int64_t *list_of_ids)
{
    bind_statement(select_latest_entries, ENTRY_BINDING, &num_of_entries, 0,
                   INT);
    bind_statement(select_latest_entries, LENGTH_BINDING, &offset, 0, INT);

    int counter = 0;
    while (execute_statement(select_latest_entries) != SQLITE_DONE)
    {
        list_of_ids[counter] = sqlite3_column_int(select_latest_entries, 0);
        counter++;
    }

    sqlite3_reset(select_latest_entries);
    sqlite3_clear_bindings(select_latest_entries);

    return counter;
}

uint32_t database_get_total_entries(sqlite3 *db)
{
    int ret = execute_statement(total_entries);
    if (ret == SQLITE_DONE)
    {
        return 0;
    }

    uint32_t total = sqlite3_column_int(total_entries, 0);
    sqlite3_reset(total_entries);

    return total;
}

char *database_get_snippet(sqlite3 *db, int64_t id)
{
    bind_statement(select_snippet, ID_BINDING, &id, 0, INT);
    int ret = execute_statement(select_snippet);
    if (ret == SQLITE_DONE)
    {
        sqlite3_reset(select_snippet);
        sqlite3_clear_bindings(select_snippet);
        return NULL;
    }

    const char *tmp_snippet =
        (char *)sqlite3_column_text(select_snippet, (SNIPPET_BINDING - 1));
    if (!tmp_snippet)
    {
        perror("Failed to allocate memory");
        exit(EXIT_FAILURE);
    }
    char *snippet = xstrdup(tmp_snippet);

    sqlite3_reset(select_snippet);
    sqlite3_clear_bindings(select_snippet);

    return snippet;
}

void *database_get_thumbnail(sqlite3 *db, int64_t id, size_t *len)
{
    bind_statement(select_thumbnail, ID_BINDING, &id, 0, INT);
    execute_statement(select_thumbnail);

    const void *tmp_blob = sqlite3_column_blob(select_thumbnail, 0);
    *len = sqlite3_column_bytes(select_thumbnail, 0);
    void *thumbnail = xmalloc(*len);
    thumbnail = memcpy(thumbnail, tmp_blob, *len);

    sqlite3_reset(select_thumbnail);
    sqlite3_clear_bindings(select_thumbnail);

    return thumbnail;
}

bool database_get_entry(sqlite3 *db, int64_t id, source_buffer *src)
{
    src->snippet = database_get_snippet(db, id);
    if (!src->snippet)
    {
        return false;
    }

    src->thumbnail = database_get_thumbnail(db, id, &src->thumbnail_len);

    bind_statement(select_entry, ID_BINDING, &id, 0, INT);
    while (execute_statement(select_entry) != SQLITE_DONE &&
           src->num_types < MAX_MIME_TYPES)
    {
        src->len[src->num_types] =
            sqlite3_column_int(select_entry, (LENGTH_BINDING - 1));

        const void *tmp_blob =
            sqlite3_column_blob(select_entry, (DATA_BINDING - 1));
        if (!tmp_blob)
        {
            perror("Failed to allocate memory");
            exit(EXIT_FAILURE);
        }
        src->data[src->num_types] = xmalloc(src->len[src->num_types]);
        memcpy(src->data[src->num_types], tmp_blob, src->len[src->num_types]);

        const char *tmp_text =
            (char *)sqlite3_column_text(select_entry, (MIME_TYPE_BINDING - 1));

        if (!tmp_text)
        {
            perror("Failed to allocate memory");
            exit(EXIT_FAILURE);
        }
        src->types[src->num_types] = xstrdup(tmp_text);

        src->num_types++;
    }

    sqlite3_reset(select_entry);
    sqlite3_clear_bindings(select_entry);

    return true;
}

/* Find $XDG_DATA_HOME/kaprica/history.db or
 * $HOME/.local/share/kaprica/history.db */
char *find_database_path()
{
    char *data_path = NULL;
    char *data_home = getenv("XDG_DATA_HOME");
    if (data_home)
    {
        data_path =
            xmalloc(strlen(data_home) + strlen("/kaprica/history.db") + 1);
        strcpy(data_path, data_home);
        strcat(data_path, "/kaprica/history.db");
    }
    else
    {
        data_home = getenv("HOME");
        data_path = xmalloc(strlen(data_home) +
                            strlen("/.local/share/kaprica/history.db") + 1);
        strcpy(data_path, data_home);
        strcat(data_path, "/.local/share/kaprica/history.db");
    }

    return data_path;
}

/* Create a new database if one does not already exist */
sqlite3 *database_init(char *filepath)
{
    filepath = (filepath != NULL) ? filepath : find_database_path();

    sqlite3 *db;
    // Put database in a proper location, probably ~/.kaprica/history.db
    sqlite3_open(filepath, &db);
    if (!db)
    {
        fprintf(stderr, "Failed to create database: %s\n", sqlite3_errmsg(db));
        exit(EXIT_FAILURE);
    }
    free(filepath);

    prepare_bootstrap_statements(db);
    execute_statement(pragma_foreign_keys);
    execute_statement(pragma_auto_vacuum);
    execute_statement(pragma_secure_delete);
    execute_statement(create_main_table);
    execute_statement(create_content_table);

    prepare_index_statements(db);
    execute_statement(create_data_index);
    execute_statement(create_timestamp_index);
    execute_statement(create_mime_index);
    execute_statement(create_snippet_index);
    execute_statement(create_thumbnail_index);
    execute_statement(create_hash_index);

    prepare_all_statements(db);

    return db;
}

/* Open an existing database */
sqlite3 *database_open(char *filepath)
{
    filepath = (filepath != NULL) ? filepath : find_database_path();
    if (access(filepath, F_OK) == -1)
    {
        fprintf(stderr, "Could not find database at %s\n", filepath);
        exit(EXIT_FAILURE);
    }

    sqlite3 *db;
    sqlite3_open(filepath, &db);
    if (!db)
    {
        fprintf(stderr, "Failed to open database: %s\n", sqlite3_errmsg(db));
        exit(EXIT_FAILURE);
    }
    free(filepath);

    prepare_bootstrap_statements(db);

    execute_statement(pragma_foreign_keys);
    execute_statement(pragma_auto_vacuum);
    execute_statement(pragma_secure_delete);
    execute_statement(create_main_table);
    execute_statement(create_content_table);

    prepare_all_statements(db);

    return db;
}

void database_maintenance(sqlite3 *db)
{
    sqlite3_exec(db, "VACUUM;", NULL, NULL, NULL);

    execute_statement(pragma_optimize);
    sqlite3_reset(pragma_optimize);
}

void database_close(sqlite3 *db)
{
    sqlite3_finalize(select_latest_entries);
    sqlite3_finalize(select_entry);
    sqlite3_finalize(delete_old_entries);
    sqlite3_finalize(pragma_foreign_keys);
    sqlite3_finalize(insert_entry_content);
    sqlite3_finalize(insert_entry);
    sqlite3_finalize(create_main_table);
    sqlite3_finalize(create_content_table);
    sqlite3_finalize(find_matching_entries);
    sqlite3_finalize(select_snippet);
    sqlite3_finalize(find_matching_types);
    sqlite3_finalize(delete_entry);
    sqlite3_finalize(total_entries);
    sqlite3_finalize(select_thumbnail);
    sqlite3_finalize(delete_duplicate_entries);
    sqlite3_finalize(delete_last_entries);
    sqlite3_finalize(create_data_index);
    sqlite3_finalize(create_mime_index);
    sqlite3_finalize(create_snippet_index);
    sqlite3_finalize(create_thumbnail_index);
    sqlite3_finalize(create_timestamp_index);
    sqlite3_finalize(create_hash_index);
    sqlite3_finalize(delete_large_entries);
    sqlite3_finalize(find_matching_entries_glob);
    sqlite3_finalize(pragma_secure_delete);
    sqlite3_finalize(pragma_auto_vacuum);
    sqlite3_finalize(pragma_optimize);
    sqlite3_db_release_memory(db);
    sqlite3_close(db);
}
