#include <sqlite3.h>
#include <stdint.h>
#include "clipboard.h"

#ifndef DATABASE_H
#define DATABASE_H

// All ids need to be changed from uint32_t to int64_t
sqlite3 *database_init(void);
sqlite3 *database_open(void);
void database_insert_source(sqlite3 *db, source_buffer *src);
uint32_t database_get_total_sources(sqlite3 *db);
uint32_t database_get_latest_sources(sqlite3 *db, uint16_t num_of_entries, uint32_t offset,
                                     uint32_t *list_of_ids);
void database_delete_entry(sqlite3 *db, uint32_t id);
char *database_get_snippet(sqlite3 *db, uint32_t id);
void *database_get_thumbnail(sqlite3 *db, uint32_t id, uint32_t *len);
bool database_get_source(sqlite3 *db, uint32_t id, source_buffer *src);
uint16_t database_find_matching_sources(sqlite3 *db, void *match,
                                       uint32_t length, uint16_t num_of_entries,
                                       uint32_t *list_of_ids, bool mime_type);
uint32_t database_destroy_old_entries(sqlite3 *db, int32_t days);
void database_destroy(sqlite3 *db);

#endif
