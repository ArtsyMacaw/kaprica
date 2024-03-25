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

/* Every statement we intend to use */
static sqlite3_stmt *create_main_table, *create_content_table, *insert_entry,
    *insert_entry_content, *delete_old_entries, *pragma_foreign_keys,
    *select_latest_entries, *select_entry, *find_matching_entries,
    *select_snippet, *find_matching_types, *pragma_journal_wal, *delete_entry,
    *total_entries, *select_thumbnail, *create_data_index, *create_mime_index,
    *create_snippet_index, *create_thumbnail_index, *create_timestamp_index,
    *create_hash_index;

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

    const char journal_wal[] = "PRAGMA journal_mode = WAL;";
    prepare_statement(db, journal_wal, &pragma_journal_wal);

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
        "INSERT INTO clipboard_history (snippet, thumbnail, hash) VALUES (?1, ?2, ?3);";
    prepare_statement(db, insert_entry_history, &insert_entry);

    const char entry_content[] =
        "INSERT INTO content (entry, length, data, mime_type)"
        "    VALUES          (?1,    ?2,     ?3,   ?4);";
    prepare_statement(db, entry_content, &insert_entry_content);

    const char remove_old_entry[] =
        "DELETE FROM clipboard_history"
        "    WHERE timestamp < (date('now', ? || ' days'));";
    prepare_statement(db, remove_old_entry, &delete_old_entries);

    const char remove_entry[] = "DELETE FROM clipboard_history"
                                "    WHERE history_id = ?1;";
    prepare_statement(db, remove_entry, &delete_entry);

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

    const char find_entry[] = "SELECT entry FROM content"
                              "    WHERE data LIKE '%' || ?1 || '%';";
    prepare_statement(db, find_entry, &find_matching_entries);

    const char find_entry_type[] = "SELECT entry FROM content"
                                   "    WHERE mime_type LIKE '%' || ?1 || '%';";
    prepare_statement(db, find_entry_type, &find_matching_types);

    const char get_total_entries[] = "SELECT COUNT(*) FROM clipboard_history;";
    prepare_statement(db, get_total_entries, &total_entries);
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

void database_delete_entry(sqlite3 *db, int64_t id)
{
    bind_statement(delete_entry, ID_BINDING, &id, 0, INT);
    execute_statement(delete_entry);

    sqlite3_reset(delete_entry);
    sqlite3_clear_bindings(delete_entry);
}

void database_insert_entry(sqlite3 *db, source_buffer *src)
{
    bind_statement(insert_entry, SNIPPET_BINDING, src->snippet,
                   strlen(src->snippet), TEXT);
    bind_statement(insert_entry, THUMBNAIL_BINDING, src->thumbnail,
                   src->thumbnail_len, BLOB);
    bind_statement(insert_entry, HASH_BINDING, src->data_hash, strlen(src->data_hash),
                   TEXT);

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
}

uint32_t database_get_latest_entries(sqlite3 *db, uint16_t num_of_entries,
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

uint32_t database_find_matching_entries(sqlite3 *db, void *match, size_t length,
                                        uint16_t num_of_entries,
                                        int64_t *list_of_ids, bool mime_type)
{
    sqlite3_stmt *search;
    if (mime_type)
    {
        search = find_matching_types;
    }
    else
    {
        search = find_matching_entries;
    }
    bind_statement(search, MATCH_BINDING, match, length, BLOB);

    int counter = 0;
    while (execute_statement(search) != SQLITE_DONE)
    {
        uint32_t tmp = sqlite3_column_int(search, (ENTRY_BINDING - 1));

        // Ignore repeat matches for the same entry.
        // There's probably a way to do this solely with SQL,
        // but I dont know how.
        if (!counter)
        {
            list_of_ids[counter] = tmp;
            counter++;
        }
        else if (list_of_ids[counter - 1] != tmp)
        {
            list_of_ids[counter] = tmp;
            counter++;
        }
        if (counter == num_of_entries)
        {
            break;
        }
    }

    sqlite3_reset(search);
    sqlite3_clear_bindings(search);

    return counter;
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

/* Create a new database if one does not already exist */
sqlite3 *database_init(void)
{
    sqlite3 *db;
    // Put database in a proper location, probably ~/.kaprica/history.db
    sqlite3_open("/home/haden/.kaprica/history.db", &db);
    if (!db)
    {
        fprintf(stderr, "Failed to create database: %s\n", sqlite3_errmsg(db));
        exit(EXIT_FAILURE);
    }

    prepare_bootstrap_statements(db);
    // execute_statement(pragma_journal_wal);
    execute_statement(pragma_foreign_keys);
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
sqlite3 *database_open(void)
{
    if (access("/home/haden/.kaprica/history.db", F_OK) == -1)
    {
        return NULL;
    }

    sqlite3 *db;
    sqlite3_open("/home/haden/.kaprica/history.db", &db);
    if (!db)
    {
        fprintf(stderr, "Failed to open database: %s\n", sqlite3_errmsg(db));
        exit(EXIT_FAILURE);
    }

    prepare_bootstrap_statements(db);

    execute_statement(pragma_foreign_keys);
    execute_statement(create_main_table);
    execute_statement(create_content_table);

    prepare_all_statements(db);

    return db;
}

uint32_t database_destroy_old_entries(sqlite3 *db, uint32_t days)
{
    bind_statement(delete_old_entries, DATE_BINDING, &days, 0, INT);
    execute_statement(delete_old_entries);
    sqlite3_reset(delete_old_entries);
    sqlite3_clear_bindings(delete_old_entries);

    return sqlite3_changes(db);
}

void database_destroy(sqlite3 *db)
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
    sqlite3_finalize(pragma_journal_wal);
    sqlite3_finalize(select_snippet);
    sqlite3_finalize(find_matching_types);
    sqlite3_finalize(delete_entry);
    sqlite3_finalize(total_entries);
    sqlite3_finalize(select_thumbnail);
    sqlite3_finalize(create_data_index);
    sqlite3_finalize(create_mime_index);
    sqlite3_finalize(create_snippet_index);
    sqlite3_finalize(create_thumbnail_index);
    sqlite3_finalize(create_timestamp_index);
    sqlite3_finalize(create_hash_index);
    sqlite3_db_release_memory(db);
    sqlite3_close(db);
}
