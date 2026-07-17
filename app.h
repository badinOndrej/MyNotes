#ifndef NOTES_APP_APP_H
#define NOTES_APP_APP_H

#include "inkview.h"
#include "sqlite3.h"

#include <string>
#include <vector>
#include <ctime>

#define APP_DATA_DIR   FLASHDIR "/NotesApp"
#define DB_PATH        APP_DATA_DIR "/handwriting_notes.sqlite3"
#define EXPORT_DIR     APP_DATA_DIR "/exports"
#define TMP_PNG_PATH   APP_DATA_DIR "/tmp_export.png"

constexpr int FONT_SIZE = 24;
constexpr int ROW_HEIGHT = 76;
constexpr int HEADER_HEIGHT = 64;
constexpr int FOOTER_HEIGHT = 100;
constexpr int TITLEBAR_HEIGHT = 60;
constexpr int TOOLBAR_HEIGHT = 100;
constexpr int PEN_WIDTH = 3;
constexpr int CHECKBOX_SIZE = 36;
constexpr int DRAW_FLUSH_INTERVAL_MS = 125;
constexpr bool USE_TOUCH_SECTION_SMOOTHING = true;

enum AppState
{
	STATE_MENU,
	STATE_CANVAS
};

struct Point
{
	short x, y;
};

typedef std::vector<Point> Stroke;
typedef std::vector<Stroke> StrokeList;
typedef std::vector<StrokeList> PageList;

struct NoteMeta
{
	int id;
	std::string title;
	time_t created_at;
	time_t updated_at;
};

extern int _width;
extern int _height;
extern ifont *_font_regular;
extern ifont *_font_small;

extern sqlite3 *_db;

extern AppState _state;

extern std::vector<NoteMeta> _notes;
extern int _page;
extern bool _selection_mode;
extern std::vector<int> _selected_ids;
extern bool _suppress_next_menu_tap;

extern int _current_id;
extern std::string _current_title;
extern PageList _pages;
extern int _current_page;
extern bool _drawing;
extern bool _dirty;

extern bool _toolbar_touch_active;
extern int _toolbar_touch_x;

extern bool _dirty_rect_valid;
extern int _dirty_x1;
extern int _dirty_y1;
extern int _dirty_x2;
extern int _dirty_y2;

extern ibitmap *_ink_overlay;
extern int _ink_overlay_width;
extern int _ink_overlay_height;
extern bool _ink_overlay_valid;

extern char _kbd_buffer[128];

void init_app();
int main_handler(int event_type, int param_one, int param_two);

#endif // NOTES_APP_APP_H
