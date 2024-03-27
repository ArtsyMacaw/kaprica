#include <sqlite3.h>
#include <stdint.h>
#include "clipboard.h"

#ifndef DATABASE_H
#define DATABASE_H

sqlite3 *database_init(void);
sqlite3 *database_open(void);
void database_insert_entry(sqlite3 *db, source_buffer *src);
uint32_t database_get_total_entries(sqlite3 *db);
uint32_t database_get_latest_entries(sqlite3 *db, uint32_t num_of_entries,
                                     uint32_t offset, int64_t *list_of_ids);
void database_delete_entry(sqlite3 *db, int64_t id);
char *database_get_snippet(sqlite3 *db, int64_t id);
void *database_get_thumbnail(sqlite3 *db, int64_t id, size_t *len);
bool database_get_entry(sqlite3 *db, int64_t id, source_buffer *src);
uint32_t database_find_matching_entries(sqlite3 *db, void *match, size_t length,
                                        uint16_t num_of_entries,
                                        int64_t *list_of_ids, bool mime_type);
uint32_t database_destroy_old_entries(sqlite3 *db, uint32_t days);
void database_destroy(sqlite3 *db);

#endif
