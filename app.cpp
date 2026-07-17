#include "app.h"
#include "ui.h"
#include "storage.h"

int _width = 0;
int _height = 0;
ifont *_font_regular = NULL;
ifont *_font_small = NULL;

sqlite3 *_db = NULL;

AppState _state = STATE_MENU;

std::vector<NoteMeta> _notes;
int _page = 0;
bool _selection_mode = false;
std::vector<int> _selected_ids;
bool _suppress_next_menu_tap = false;

int _current_id = -1;
std::string _current_title;
PageList _pages;
int _current_page = 0;
bool _drawing = false;
bool _dirty = false;

bool _toolbar_touch_active = false;
int _toolbar_touch_x = 0;

bool _dirty_rect_valid = false;
int _dirty_x1 = 0;
int _dirty_y1 = 0;
int _dirty_x2 = 0;
int _dirty_y2 = 0;

ibitmap *_ink_overlay = NULL;
int _ink_overlay_width = 0;
int _ink_overlay_height = 0;
bool _ink_overlay_valid = false;

char _kbd_buffer[128];

void init_app()
{
	_width = ScreenWidth();
	_height = ScreenHeight();

	SetPanelType(PANEL_DISABLED);

	ClearScreen();
	FullUpdate();

	_font_regular = OpenFont("LiberationSans", FONT_SIZE, 1);
	_font_small = OpenFont("LiberationSans", 18, 0);
	SetFont(_font_regular, BLACK);

	db_init();
	go_to_menu();
}

int main_handler(int event_type, int param_one, int param_two)
{
	switch (event_type)
	{
	case EVT_INIT:
		init_app();
		break;

	case EVT_POINTERDOWN:
		if (_state == STATE_MENU)
			handle_menu_tap(param_one, param_two);
		else
			canvas_pointer_down(param_one, param_two);
		break;

	case EVT_POINTERMOVE:
		if (_state == STATE_CANVAS)
			canvas_pointer_move(param_one, param_two);
		break;

	case EVT_POINTERUP:
		if (_state == STATE_CANVAS)
			canvas_pointer_up(param_one, param_two);
		break;

	case EVT_POINTERLONG:
		if (_state == STATE_MENU)
			handle_menu_long_press(param_one, param_two);
		break;

	case EVT_KEYPRESS:
		if (_state == STATE_CANVAS)
		{
			if (param_one == IV_KEY_PREV || param_one == IV_KEY_PREV2)
			{
				if (_current_page > 0)
				{
					_current_page--;
					_ink_overlay_valid = false;
					draw_canvas();
					FullUpdate();
				}
				return 1;
			}

			if (param_one == IV_KEY_NEXT || param_one == IV_KEY_NEXT2)
			{
				if (_current_page + 1 < (int)_pages.size())
				{
					_current_page++;
					_ink_overlay_valid = false;
					draw_canvas();
					FullUpdate();
				}
				else
				{
					_pages.push_back(StrokeList());
					_current_page = (int)_pages.size() - 1;
					_dirty = true;
					_ink_overlay_valid = false;
					draw_canvas();
					FullUpdate();
				}
				return 1;
			}
		}
		else if (_state == STATE_MENU)
		{
			int n_per_page = rows_per_page();
			int total_pages = ((_notes.size() + n_per_page - 1) / n_per_page);
			if (total_pages > 1 && (param_one == IV_KEY_PREV || param_one == IV_KEY_PREV2))
			{
				if (_page > 0) _page--;
				draw_menu();
				FullUpdate();
				return 1;
			}
			if (total_pages > 1 && (param_one == IV_KEY_NEXT || param_one == IV_KEY_NEXT2))
			{
				if (_page < total_pages - 1) _page++;
				draw_menu();
				FullUpdate();
				return 1;
			}
		}

		if (param_one == HWKEY_PREV)
		{
			if (_state == STATE_CANVAS)
			{
				save_current_note();
				go_to_menu();
			}
			else
			{
				CloseApp();
			}
			return 1;
		}
		break;

	case EVT_EXIT:
		if (_state == STATE_CANVAS)
			save_current_note();
		destroy_ink_overlay();
		if (_font_regular) CloseFont(_font_regular);
		if (_font_small) CloseFont(_font_small);
		db_close();
		break;

	default:
		break;
	}

	return 0;
}
