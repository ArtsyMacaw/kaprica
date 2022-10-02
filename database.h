#include <sqlite3.h>
#include <stdint.h>
#include "clipboard.h"

#ifndef DATABASE_H
#define DATABASE_H

sqlite3 *database_init(void);
void database_insert_source(sqlite3 *db, source_buffer *src);
uint32_t database_get_latest_source_id(sqlite3 *db);
void database_get_source(sqlite3 *db, uint32_t id, source_buffer *src);
uint32_t database_destroy_old_entries(sqlite3 *db, int32_t days);
void database_destroy(sqlite3 *db);

#endif
