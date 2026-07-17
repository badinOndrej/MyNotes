#ifndef NOTES_APP_STORAGE_H
#define NOTES_APP_STORAGE_H

#include "app.h"

#include <string>

void db_ensure_dirs();
bool db_init();
void db_close();
void db_load_notes();
int db_create_note(const std::string &title);
void db_save_note(int id, const std::string &title, const PageList &pages);
PageList db_load_pages(int id);
void db_delete_note(int id);

#endif // NOTES_APP_STORAGE_H
