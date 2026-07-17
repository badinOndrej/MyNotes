#ifndef NOTES_APP_UI_H
#define NOTES_APP_UI_H

#include "app.h"

#include <string>

void draw_menu();
void draw_canvas();
void open_note_for_editing(int id, const std::string &title);
void save_current_note();
void go_to_menu();
void ensure_current_page();
int rows_per_page();
void start_new_note();
void delete_selected_notes();
void handle_menu_tap(int x, int y);
void handle_menu_long_press(int x, int y);
void handle_canvas_toolbar_tap(int x);
void handle_canvas_page_nav(int x, int y);
void canvas_pointer_down(int x, int y);
void canvas_pointer_move(int x, int y);
void canvas_pointer_up(int x, int y);
void destroy_ink_overlay();

#endif // NOTES_APP_UI_H
