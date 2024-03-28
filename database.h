#include <sqlite3.h>
#include <stdint.h>
#include "clipboard.h"

#ifndef DATABASE_H
#define DATABASE_H

enum search_type
{
    CONTENT,
    MIME_TYPE,
    GLOB, //TODO
    TIMESTAMP // TODO
};

sqlite3 *database_init(const char *filepath);
sqlite3 *database_open(const char *filepath);
void database_destroy(sqlite3 *db);

void database_insert_entry(sqlite3 *db, source_buffer *src);

uint32_t database_get_total_entries(sqlite3 *db);
char *database_get_snippet(sqlite3 *db, int64_t id);
void *database_get_thumbnail(sqlite3 *db, int64_t id, size_t *len);
bool database_get_entry(sqlite3 *db, int64_t id, source_buffer *src);
uint32_t database_get_latest_entries(sqlite3 *db, uint32_t num_of_entries,
                                     uint32_t offset, int64_t *list_of_ids);

uint32_t database_find_matching_entries(sqlite3 *db, void *match, size_t length,
                                        uint32_t num_of_entries,
                                        int64_t *list_of_ids,
                                        enum search_type type);

void database_delete_entry(sqlite3 *db, int64_t id);
// TODO: void database_delete_all_entries(sqlite3 *db);
uint32_t database_delete_old_entries(sqlite3 *db, uint32_t days);
uint32_t database_delete_duplicate_entries(sqlite3 *db);

#endif
