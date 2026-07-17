#ifndef NOTES_APP_UTILS_H
#define NOTES_APP_UTILS_H

#include "app.h"

#include <string>

bool is_selected(int id);
void toggle_selected(int id);
std::string format_date(time_t t);
std::string html_escape(const std::string &in);
void draw_thick_line(int x1, int y1, int x2, int y2, int radius, int color);

#endif // NOTES_APP_UTILS_H
