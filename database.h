#include <sqlite3.h>
#include <stdint.h>
#include "clipboard.h"

#ifndef DATABASE_H
#define DATABASE_H

sqlite3 *database_init(void);
void database_insert_source(sqlite3 *db, copy_src *src);
uint32_t database_get_latest_source_id(sqlite3 *db);
void database_get_source(sqlite3 *db, uint32_t id, copy_src *src);
int database_destroy_old_entries(sqlite3 *db, uint32_t days);
void database_destroy(sqlite3 *db);

#endif