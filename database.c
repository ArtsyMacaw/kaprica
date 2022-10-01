#define _POSIX_C_SOURCE 200112L
#include <time.h>
#include <stdint.h>
#include <sqlite3.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "clipboard.h"
#include "database.h"
#include "xmalloc.h"

/* Every statement we intend to use */
static sqlite3_stmt *create_main_table, *create_content_table,
                    *insert_source_entry, *insert_source_content,
                    *delete_old_source_entries, *pragma_foreign_keys,
                    *select_latest_source, *select_source;

#define FIVE_HUNDRED_MS 5
struct timespec one_hundred_ms =
{
    .tv_nsec = 100000000
};

enum datatype {BLOB, INT, TEXT};

/* SQL Parameters && iCol values */
#define ID_BINDING 1
#define DATE_BINDING 1
#define ENTRY_BINDING 1
#define LENGTH_BINDING 2
#define DATA_BINDING 3
#define MIME_TYPE_BINDING 4

void prepare_statement(sqlite3 *db, const char *s, sqlite3_stmt **stmt)
{
    int ret = sqlite3_prepare_v2(db, s, -1, stmt, NULL);
    if (ret != SQLITE_OK)
    {
        fprintf(stderr, "Failed to prepare statement: %d\n", ret);
        exit(1);
    }
}

/* Only statements necessary to create and initialize
 * the database if it did not already exist */
void prepare_bootstrap_statements(sqlite3 *db)
{
    const char foreign_keys[] =
        "PRAGMA foreign_keys = ON;";
    prepare_statement(db, foreign_keys, &pragma_foreign_keys);

    const char main_table[] =
        "CREATE TABLE IF NOT EXISTS clipboard_history ("
        "    history_id INTEGER PRIMARY KEY ASC,"
        "    timestamp DATETIME NOT NULL DEFAULT (datetime('now')));";
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

/* Preparing statements is relatively costly resource wise
 * so we frontload all of them at the start and only
 * finalize them when the program is stopped */
void prepare_all_statements(sqlite3 *db)
{
    const char source_entry[] =
        "INSERT INTO clipboard_history DEFAULT VALUES;";
    prepare_statement(db, source_entry, &insert_source_entry);

    const char source_content[] =
        "INSERT INTO content (entry, length, data, mime_type)"
        "    VALUES          (?1,    ?2,     ?3,   ?4);";
    prepare_statement(db, source_content, &insert_source_content);

    const char remove_old_source_entry[] =
        "DELETE FROM clipboard_history"
        "    WHERE timestamp < (date('now', ?));";
    prepare_statement(db, remove_old_source_entry, &delete_old_source_entries);

    const char get_latest_source[] =
        "SELECT history_id FROM clipboard_history"
        "    ORDER BY timestamp DESC"
        "    LIMIT 1;";
    prepare_statement(db, get_latest_source, &select_latest_source);

    const char get_source[] =
        "SELECT * FROM content"
        "    WHERE entry = ?1;";
    prepare_statement(db, get_source, &select_source);
}

int execute_statement(sqlite3_stmt *stmt)
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
        fprintf(stderr, "Failed to execute statement: %d\n", ret);
        return -1;
    }

    return ret;
}

void bind_statement(sqlite3_stmt *stmt, uint16_t literal,
        void *data, uint16_t length, enum datatype type)
{
    int ret;
    switch (type)
    {
        case INT:
            ret = sqlite3_bind_int(stmt, literal, *(int *) data);
            break;
        case BLOB:
            ret = sqlite3_bind_blob(stmt, literal, data, length, SQLITE_STATIC);
            break;
        case TEXT:
            ret = sqlite3_bind_text(stmt, literal, (char *) data, length, SQLITE_STATIC);
            break;
    }
    
    if (ret == SQLITE_NOMEM)
    {
        fprintf(stderr, "Failed to allocate memory\n");
        exit(1);
    }
    else if (ret == SQLITE_TOOBIG || ret == SQLITE_RANGE)
    {
        fprintf(stderr, "Failed to bind: %d\n", ret);
        exit(1);
    }
}

void database_insert_source(sqlite3 *db, copy_src *src)
{
    execute_statement(insert_source_entry);
    sqlite3_reset(insert_source_entry);

    int rowid = sqlite3_last_insert_rowid(db);
    for (int i = 0; i < src->num_mime_types; i++)
    {
        bind_statement(insert_source_content, ENTRY_BINDING, &rowid, 0, INT);
        bind_statement(insert_source_content, LENGTH_BINDING, &src->len[i], 0, INT);
        bind_statement(insert_source_content, DATA_BINDING, src->data[i], src->len[i], BLOB);
        bind_statement(insert_source_content, MIME_TYPE_BINDING, src->mime_types[i],
                strlen(src->mime_types[i]), TEXT);

        execute_statement(insert_source_content);

        sqlite3_reset(insert_source_content);
        sqlite3_clear_bindings(insert_source_content);
    }
}

uint32_t database_get_latest_source_id(sqlite3 *db)
{
    execute_statement(select_latest_source);
    
    int ret = sqlite3_column_int(select_latest_source, 0);
    sqlite3_reset(select_latest_source);
    return ret;
}

void database_get_source(sqlite3 *db, uint32_t id, copy_src *src)
{
    bind_statement(select_source, ID_BINDING, &id, 0, INT);

    while (execute_statement(select_source) != SQLITE_DONE &&
            src->num_mime_types < MAX_MIME_TYPES)
    {
        src->len[src->num_mime_types] =
            sqlite3_column_int(select_source, (LENGTH_BINDING - 1));

        src->data[src->num_mime_types] =
            xmalloc(src->len[src->num_mime_types]);
        const void *tmp_blob =
            sqlite3_column_blob(select_source, (DATA_BINDING -1 ));
        if (!tmp_blob)
        {
            fprintf(stderr, "Failed to allocate memory\n");
            exit(1);
        }
        memcpy(src->data[src->num_mime_types],
                tmp_blob,
                src->len[src->num_mime_types]);

        const char *tmp_text =
            (char *) sqlite3_column_text(select_source, (MIME_TYPE_BINDING - 1));
        if (!tmp_text)
        {
            fprintf(stderr, "Failed to allocate memory\n");
            exit(1);
        }
        src->mime_types[src->num_mime_types] = xstrdup(tmp_text);

        src->num_mime_types++;
    }

    sqlite3_reset(select_source);
    sqlite3_clear_bindings(select_source);
}

sqlite3 *database_init(void)
{
    sqlite3 *db;
    // Put database in a proper location, probably ~/.kaprica/history.db
    sqlite3_open("./test.db", &db);
    if (!db)
    {
        fprintf(stderr, "Failed to create database\n");
        exit(1);
    }
    printf("Opened database\n");
    prepare_bootstrap_statements(db);

    execute_statement(pragma_foreign_keys);
    execute_statement(create_main_table);
    execute_statement(create_content_table);

    prepare_all_statements(db);

    return db;
}

int database_destroy_old_entries(sqlite3 *db, uint32_t days)
{
    /* Manually create the date string as we can't insert an
     * integer into a string constant */
    char buffer[sizeof(uint32_t) + strlen("- days")];
    snprintf(buffer, sizeof(buffer), "-%d days", days);

    bind_statement(delete_old_source_entries, DATE_BINDING, buffer, strlen(buffer), TEXT);
    execute_statement(delete_old_source_entries);
    sqlite3_reset(delete_old_source_entries);
    sqlite3_clear_bindings(delete_old_source_entries);

    return sqlite3_changes(db);
}

void database_destroy(sqlite3 *db)
{
    sqlite3_finalize(select_latest_source);
    sqlite3_finalize(select_source);
    sqlite3_finalize(delete_old_source_entries);
    sqlite3_finalize(pragma_foreign_keys);
    sqlite3_finalize(insert_source_content);
    sqlite3_finalize(insert_source_entry);
    sqlite3_finalize(create_main_table);
    sqlite3_finalize(create_content_table);
    sqlite3_close(db);
}
