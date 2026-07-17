#include "ui.h"
#include "utils.h"
#include "storage.h"
#include "export.h"

#include <algorithm>
#include <cstdio>

static int canvas_draw_top()
{
    return TITLEBAR_HEIGHT;
}

static int canvas_draw_bottom()
{
    return _height - TOOLBAR_HEIGHT;
}

void destroy_ink_overlay()
{
    if (_ink_overlay)
    {
        free(_ink_overlay);
        _ink_overlay = NULL;
        _ink_overlay_width = 0;
        _ink_overlay_height = 0;
        _ink_overlay_valid = false;
    }
}

static void ensure_ink_overlay()
{
    int page_h = canvas_draw_bottom() - canvas_draw_top();
    if (page_h < 1) page_h = 1;
    if (_ink_overlay && _ink_overlay_width == _width && _ink_overlay_height == page_h)
        return;
    destroy_ink_overlay();
    _ink_overlay = NewBitmap24(_width, page_h);
    if (!_ink_overlay)
    {
        _ink_overlay = NewBitmap(_width, page_h);
        if (!_ink_overlay)
            return;
    }
    _ink_overlay_width = _width;
    _ink_overlay_height = page_h;
    _ink_overlay_valid = false;
}

static void rebuild_ink_overlay()
{
    ensure_ink_overlay();
    if (!_ink_overlay) return;

    icanvas offscreen;
    offscreen.width = _ink_overlay->width;
    offscreen.height = _ink_overlay->height;
    offscreen.scanline = _ink_overlay->scanline;
    offscreen.depth = _ink_overlay->depth;
    offscreen.clipx1 = 0;
    offscreen.clipy1 = 0;
    offscreen.clipx2 = _ink_overlay->width - 1;
    offscreen.clipy2 = _ink_overlay->height - 1;
    offscreen.addr = _ink_overlay->data;

    icanvas *prev = GetCanvas();
    SetCanvas(&offscreen);

    FillArea(0, 0, _ink_overlay_width, _ink_overlay_height, WHITE);
    const StrokeList &current_strokes = _pages[_current_page];
    for (size_t s = 0; s < current_strokes.size(); s++)
    {
        const Stroke &st = current_strokes[s];
        if (st.size() == 1)
            DrawCircle(st[0].x, st[0].y - canvas_draw_top(), PEN_WIDTH / 2, BLACK);
        for (size_t p = 1; p < st.size(); p++)
            draw_thick_line(st[p - 1].x, st[p - 1].y - canvas_draw_top(),
                            st[p].x, st[p].y - canvas_draw_top(),
                            PEN_WIDTH / 2, BLACK);
    }

    SetCanvas(prev);
    _ink_overlay_valid = true;
}

static void blit_ink_overlay_rect(int x1, int y1, int x2, int y2)
{
    if (!_ink_overlay) return;
    int top = canvas_draw_top();
    int bx = std::max(0, x1);
    int by = std::max(0, y1 - top);
    int bw = std::min(_ink_overlay_width, x2) - bx;
    int bh = std::min(_ink_overlay_height, y2 - top) - by;
    if (bw <= 0 || bh <= 0) return;
    DrawBitmapArea(bx, top + by, _ink_overlay, bx, by, bw, bh);
}

static void draw_overlay_circle(int x, int y)
{
    if (!_ink_overlay_valid)
        rebuild_ink_overlay();
    if (!_ink_overlay) return;

    int top = canvas_draw_top();
    icanvas offscreen;
    offscreen.width = _ink_overlay->width;
    offscreen.height = _ink_overlay->height;
    offscreen.scanline = _ink_overlay->scanline;
    offscreen.depth = _ink_overlay->depth;
    offscreen.clipx1 = 0;
    offscreen.clipy1 = 0;
    offscreen.clipx2 = _ink_overlay->width - 1;
    offscreen.clipy2 = _ink_overlay->height - 1;
    offscreen.addr = _ink_overlay->data;

    icanvas *prev = GetCanvas();
    SetCanvas(&offscreen);
    DrawCircle(x, y - top, PEN_WIDTH / 2, BLACK);
    SetCanvas(prev);
}

static void draw_overlay_segment(int x1, int y1, int x2, int y2)
{
    if (!_ink_overlay_valid)
        rebuild_ink_overlay();
    if (!_ink_overlay) return;

    int top = canvas_draw_top();
    icanvas offscreen;
    offscreen.width = _ink_overlay->width;
    offscreen.height = _ink_overlay->height;
    offscreen.scanline = _ink_overlay->scanline;
    offscreen.depth = _ink_overlay->depth;
    offscreen.clipx1 = 0;
    offscreen.clipy1 = 0;
    offscreen.clipx2 = _ink_overlay->width - 1;
    offscreen.clipy2 = _ink_overlay->height - 1;
    offscreen.addr = _ink_overlay->data;

    icanvas *prev = GetCanvas();
    SetCanvas(&offscreen);
    draw_thick_line(x1, y1 - top, x2, y2 - top, PEN_WIDTH / 2, BLACK);
    SetCanvas(prev);
}

int rows_per_page()
{
    int list_h = _height - HEADER_HEIGHT - FOOTER_HEIGHT;
    int n = list_h / ROW_HEIGHT;
    return n < 1 ? 1 : n;
}

static void keyboard_new_note_cb(char *text)
{
    if (!text)
        return;

    std::string title = (strlen(text) > 0) ? std::string(text) : std::string("Untitled");

    int id = db_create_note(title);
    if (id < 0)
    {
        Message(ICON_ERROR, "Notes", "Could not create note.", 2500);
        return;
    }

    open_note_for_editing(id, title);
}

static void open_keyboard_timer_cb()
{
    OpenKeyboard("New note title", _kbd_buffer, sizeof(_kbd_buffer) - 1,
                 KBD_NORMAL | KBD_FIRSTUPPER, keyboard_new_note_cb);
}

void start_new_note()
{
    _kbd_buffer[0] = '\0';
    SetWeakTimer("open_kbd", open_keyboard_timer_cb, 150);
}

void delete_selected_notes()
{
    if (_selected_ids.empty())
    {
        Message(ICON_WARNING, "Delete", "No notes selected.", 2000);
        return;
    }

    char msg[128];
    snprintf(msg, sizeof(msg), "Delete %zu note(s)? This cannot be undone.", _selected_ids.size());

    int result = DialogSynchro(ICON_QUESTION, "Delete notes", msg, "Delete", "Cancel", NULL);
    if (result == 1)
    {
        for (size_t i = 0; i < _selected_ids.size(); i++)
            db_delete_note(_selected_ids[i]);

        _selected_ids.clear();
        _selection_mode = false;
        db_load_notes();
        _page = 0;
    }

    draw_menu();
    FullUpdate();
}

void draw_menu()
{
    FillArea(0, 0, _width, _height, WHITE);

    DrawRect(0, 0, _width, HEADER_HEIGHT, BLACK);
    FillArea(0, 0, _width, HEADER_HEIGHT, BLACK);
    SetFont(_font_regular, WHITE);
    DrawTextRect(0, (HEADER_HEIGHT - FONT_SIZE) / 2, _width, FONT_SIZE,
                 _selection_mode ? "Select notes" : "MyNotes", ALIGN_CENTER);
    SetFont(_font_regular, BLACK);

    int n_per_page = rows_per_page();
    int start = _page * n_per_page;
    int y = HEADER_HEIGHT;

    if (_notes.empty())
    {
        SetFont(_font_regular, DGRAY);
        DrawTextRect(0, y + 20, _width, FONT_SIZE,
                     "No notes yet. Tap \"+ New note\" to create one.", ALIGN_CENTER);
        SetFont(_font_regular, BLACK);
    }

    for (int i = start; i < (int)_notes.size() && i < start + n_per_page; i++)
    {
        const NoteMeta &n = _notes[i];

        DrawLine(0, y + ROW_HEIGHT, _width, y + ROW_HEIGHT, LGRAY);

        int text_x = 20;

        if (_selection_mode)
        {
            int cb_y = y + (ROW_HEIGHT - CHECKBOX_SIZE) / 2;
            DrawRect(20, cb_y, CHECKBOX_SIZE, CHECKBOX_SIZE, BLACK);
            if (is_selected(n.id))
                FillArea(20 + 4, cb_y + 4, CHECKBOX_SIZE - 8, CHECKBOX_SIZE - 8, BLACK);
            text_x = 20 + CHECKBOX_SIZE + 16;
        }

        std::string title = n.title.empty() ? "Untitled" : n.title;
        DrawTextRect(text_x, y + 8, _width - text_x - 20, FONT_SIZE, title.c_str(), ALIGN_LEFT);

        SetFont(_font_small, DGRAY);
        std::string date_line = "Updated " + format_date(n.updated_at);
        DrawTextRect(text_x, y + 8 + FONT_SIZE + 2, _width - text_x - 20, 20, date_line.c_str(), ALIGN_LEFT);
        SetFont(_font_regular, BLACK);

        y += ROW_HEIGHT;
    }

    int total_pages = ((_notes.size() + n_per_page - 1) / n_per_page);
    if (total_pages > 1)
    {
        int py = HEADER_HEIGHT + n_per_page * ROW_HEIGHT;
        int half = _width / 2;
        DrawRect(0, py, half, 40, BLACK);
        DrawRect(half, py, half, 40, BLACK);
        DrawTextRect(0, py + 8, half, 24, "< Prev page", ALIGN_CENTER);
        DrawTextRect(half, py + 8, half, 24, "Next page >", ALIGN_CENTER);
    }

    int fy = _height - FOOTER_HEIGHT;
    DrawLine(0, fy, _width, fy, BLACK);

    if (_selection_mode)
    {
        int w3 = _width / 3;
        DrawRect(0, fy, w3, FOOTER_HEIGHT, BLACK);
        DrawRect(w3, fy, w3, FOOTER_HEIGHT, BLACK);
        DrawRect(w3 * 2, fy, _width - w3 * 2, FOOTER_HEIGHT, BLACK);

        DrawTextRect(0, fy + (FOOTER_HEIGHT - FONT_SIZE) / 2, w3, FONT_SIZE, "Cancel", ALIGN_CENTER);

        char del_label[32];
        snprintf(del_label, sizeof(del_label), "Delete (%zu)", _selected_ids.size());
        DrawTextRect(w3, fy + (FOOTER_HEIGHT - FONT_SIZE) / 2, w3, FONT_SIZE, del_label, ALIGN_CENTER);

        char exp_label[32];
        snprintf(exp_label, sizeof(exp_label), "Export (%zu)", _selected_ids.size());
        DrawTextRect(w3 * 2, fy + (FOOTER_HEIGHT - FONT_SIZE) / 2, _width - w3 * 2, FONT_SIZE, exp_label, ALIGN_CENTER);
    }
    else
    {
        int w2 = _width / 2;
        DrawRect(0, fy, w2, FOOTER_HEIGHT, BLACK);
        DrawRect(w2, fy, _width - w2, FOOTER_HEIGHT, BLACK);

        DrawTextRect(0, fy + (FOOTER_HEIGHT - FONT_SIZE) / 2, w2, FONT_SIZE, "+ New note", ALIGN_CENTER);
        DrawTextRect(w2, fy + (FOOTER_HEIGHT - FONT_SIZE) / 2, _width - w2, FONT_SIZE,
                     "Select / Export", ALIGN_CENTER);
    }
}

static int hit_test_note_row(int x, int y)
{
    int n_per_page = rows_per_page();
    int start = _page * n_per_page;
    int row_y = HEADER_HEIGHT;

    for (int i = start; i < (int)_notes.size() && i < start + n_per_page; i++)
    {
        if (y >= row_y && y < row_y + ROW_HEIGHT)
            return i;
        row_y += ROW_HEIGHT;
    }
    return -1;
}

void handle_menu_tap(int x, int y)
{
    int fy = _height - FOOTER_HEIGHT;

    if (y >= fy)
    {
        if (_selection_mode)
        {
            int w3 = _width / 3;
            if (x < w3)
            {
                _selection_mode = false;
                _selected_ids.clear();
            }
            else if (x < w3 * 2)
            {
                delete_selected_notes();
                return;
            }
            else
            {
                export_notes(_selected_ids);
                _selection_mode = false;
                _selected_ids.clear();
            }
        }
        else
        {
            if (x < _width / 2)
                start_new_note();
            else
                _selection_mode = true;
        }

        draw_menu();
        FullUpdate();
        return;
    }

    int n_per_page = rows_per_page();
    int total_pages = ((_notes.size() + n_per_page - 1) / n_per_page);
    int py = HEADER_HEIGHT + n_per_page * ROW_HEIGHT;
    if (total_pages > 1 && y >= py && y < py + 40)
    {
        if (x < _width / 2)
        {
            if (_page > 0) _page--;
        }
        else
        {
            if (_page < total_pages - 1) _page++;
        }
        draw_menu();
        FullUpdate();
        return;
    }

    int idx = hit_test_note_row(x, y);
    if (idx >= 0)
    {
        const NoteMeta &n = _notes[idx];
        if (_selection_mode)
        {
            toggle_selected(n.id);
            draw_menu();
            PartialUpdate(0, 0, _width, _height);
        }
        else
        {
            open_note_for_editing(n.id, n.title);
        }
    }
}

void handle_menu_long_press(int x, int y)
{
    int idx = hit_test_note_row(x, y);
    if (idx < 0) return;

    const NoteMeta &n = _notes[idx];
    _selection_mode = true;
    if (!is_selected(n.id))
        toggle_selected(n.id);

    _suppress_next_menu_tap = true;

    draw_menu();
    FullUpdate();
}

void go_to_menu()
{
    _state = STATE_MENU;
    db_load_notes();
    _page = 0;
    draw_menu();
    FullUpdate();
}

void ensure_current_page()
{
    if (_current_page < 0)
        _current_page = 0;
    while ((int)_pages.size() <= _current_page)
        _pages.push_back(StrokeList());
}

void draw_canvas()
{
    ensure_current_page();
    FillArea(0, 0, _width, _height, WHITE);

    DrawRect(0, 0, _width, TITLEBAR_HEIGHT, BLACK);
    FillArea(0, 0, _width, TITLEBAR_HEIGHT, BLACK);
    SetFont(_font_regular, WHITE);
    std::string title = _current_title.empty() ? "Untitled" : _current_title;
    DrawTextRect(10, (TITLEBAR_HEIGHT - FONT_SIZE) / 2, _width - 160, FONT_SIZE, title.c_str(), ALIGN_LEFT);

    SetFont(_font_small, WHITE);
    char page_label[32];
    snprintf(page_label, sizeof(page_label), "Page %d/%zu", _current_page + 1, _pages.size());
    DrawTextRect(0, (TITLEBAR_HEIGHT - 18) / 2, _width, 18, page_label, ALIGN_CENTER);

    int page_button_h = TITLEBAR_HEIGHT - 12;
    int page_button_w = 72;
    int page_button_y = (TITLEBAR_HEIGHT - page_button_h) / 2;
    int page_prev_x = _width - page_button_w * 2 - 16;
    int page_next_x = _width - page_button_w - 8;

    FillArea(page_prev_x, page_button_y, page_button_w, page_button_h, WHITE);
    DrawRect(page_prev_x, page_button_y, page_button_w, page_button_h, BLACK);
    SetFont(_font_small, BLACK);
    DrawTextRect(page_prev_x, page_button_y + (page_button_h - 18) / 2, page_button_w, 18, "<", ALIGN_CENTER);

    FillArea(page_next_x, page_button_y, page_button_w, page_button_h, WHITE);
    DrawRect(page_next_x, page_button_y, page_button_w, page_button_h, BLACK);
    DrawTextRect(page_next_x, page_button_y + (page_button_h - 18) / 2, page_button_w, 18, ">", ALIGN_CENTER);
    SetFont(_font_regular, BLACK);

    ensure_ink_overlay();
    if (!_ink_overlay_valid)
        rebuild_ink_overlay();
    if (_ink_overlay)
    {
        DrawBitmap(0, canvas_draw_top(), _ink_overlay);
    }
    else
    {
        StrokeList &current_strokes = _pages[_current_page];
        for (size_t s = 0; s < current_strokes.size(); s++)
        {
            const Stroke &st = current_strokes[s];
            if (st.size() == 1)
            {
                DrawCircle(st[0].x, st[0].y, PEN_WIDTH / 2, BLACK);
            }
            for (size_t p = 1; p < st.size(); p++)
                draw_thick_line(st[p - 1].x, st[p - 1].y, st[p].x, st[p].y, PEN_WIDTH / 2, BLACK);
        }
    }

    int fy = _height - TOOLBAR_HEIGHT;
    DrawLine(0, fy, _width, fy, BLACK);
    int back_w = _width * 35 / 100;
    int undo_w = _width * 35 / 100;
    int clear_w = _width - back_w - undo_w;
    DrawRect(0, fy, back_w, TOOLBAR_HEIGHT, BLACK);
    DrawRect(back_w, fy, undo_w, TOOLBAR_HEIGHT, BLACK);
    DrawRect(back_w + undo_w, fy, clear_w, TOOLBAR_HEIGHT, BLACK);

    DrawTextRect(0, fy + (TOOLBAR_HEIGHT - FONT_SIZE) / 2, back_w, FONT_SIZE, "< Back", ALIGN_CENTER);
    DrawTextRect(back_w, fy + (TOOLBAR_HEIGHT - FONT_SIZE) / 2, undo_w, FONT_SIZE, "Undo", ALIGN_CENTER);
    DrawTextRect(back_w + undo_w, fy + (TOOLBAR_HEIGHT - FONT_SIZE) / 2, clear_w, FONT_SIZE, "Clear", ALIGN_CENTER);
}

void open_note_for_editing(int id, const std::string &title)
{
    _current_id = id;
    _current_title = title;
    _pages = db_load_pages(id);
    if (_pages.empty())
        _pages.push_back(StrokeList());
    _current_page = 0;
    _dirty = false;
    _drawing = false;
    _state = STATE_CANVAS;
    _ink_overlay_valid = false;

    draw_canvas();
    FullUpdate();
}

void save_current_note()
{
    if (_current_id < 0) return;
    if (!_dirty) return;

    db_save_note(_current_id, _current_title, _pages);
    _dirty = false;
}

void handle_canvas_toolbar_tap(int x)
{
    int back_w = _width * 35 / 100;
    int undo_w = _width * 35 / 100;

    if (x < back_w)
    {
        save_current_note();
        go_to_menu();
    }
    else if (x < back_w + undo_w)
    {
        ensure_current_page();
        StrokeList &current_strokes = _pages[_current_page];
        if (!current_strokes.empty())
        {
            current_strokes.pop_back();
            _dirty = true;
            _ink_overlay_valid = false;
            draw_canvas();
            FullUpdate();
        }
    }
    else
    {
        ensure_current_page();
        StrokeList &current_strokes = _pages[_current_page];
        if (!current_strokes.empty())
        {
            int result = DialogSynchro(ICON_QUESTION, "Clear note",
                                       "Erase all strokes on this page?", "Clear", "Cancel", NULL);
            if (result == 1)
            {
                current_strokes.clear();
                _dirty = true;
                _ink_overlay_valid = false;
                draw_canvas();
                FullUpdate();
            }
        }
    }
}

void handle_canvas_page_nav(int x, int y)
{
    ensure_current_page();

    int page_button_y = 6;
    int page_button_h = TITLEBAR_HEIGHT - 12;
    int page_button_w = 72;
    int page_prev_x = _width - page_button_w * 2 - 16;
    int page_next_x = _width - page_button_w - 8;

    if (y >= page_button_y && y < page_button_y + page_button_h)
    {
        if (x >= page_prev_x && x < page_prev_x + page_button_w)
        {
            if (_current_page > 0)
            {
                _current_page--;
                _ink_overlay_valid = false;
                draw_canvas();
                FullUpdate();
            }
            return;
        }

        if (x >= page_next_x && x < page_next_x + page_button_w)
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
        }
    }
}

static void expand_dirty_rect(int x1, int y1, int x2, int y2)
{
    if (!_dirty_rect_valid)
    {
        _dirty_x1 = x1; _dirty_y1 = y1; _dirty_x2 = x2; _dirty_y2 = y2;
        _dirty_rect_valid = true;
    }
    else
    {
        _dirty_x1 = std::min(_dirty_x1, x1);
        _dirty_y1 = std::min(_dirty_y1, y1);
        _dirty_x2 = std::max(_dirty_x2, x2);
        _dirty_y2 = std::max(_dirty_y2, y2);
    }
}

static void flush_dirty_rect()
{
    if (!_dirty_rect_valid) return;
    DynamicUpdateA2(_dirty_x1, _dirty_y1, _dirty_x2 - _dirty_x1, _dirty_y2 - _dirty_y1);
    _dirty_rect_valid = false;
}

static void draw_flush_timer_cb()
{
    flush_dirty_rect();
    if (_drawing)
        SetWeakTimer("draw_flush", draw_flush_timer_cb, DRAW_FLUSH_INTERVAL_MS);
}

static void draw_segment_and_mark_dirty(const Point &a, const Point &b)
{
    if (_ink_overlay)
    {
        draw_overlay_segment(a.x, a.y, b.x, b.y);
        int minx = std::min((int)a.x, (int)b.x) - PEN_WIDTH - 2;
        int miny = std::min((int)a.y, (int)b.y) - PEN_WIDTH - 2;
        int maxx = std::max((int)a.x, (int)b.x) + PEN_WIDTH + 2;
        int maxy = std::max((int)a.y, (int)b.y) + PEN_WIDTH + 2;
        blit_ink_overlay_rect(minx, miny, maxx, maxy);
        expand_dirty_rect(minx, miny, maxx, maxy);
        return;
    }

    draw_thick_line(a.x, a.y, b.x, b.y, PEN_WIDTH / 2, BLACK);
    int minx = std::min((int)a.x, (int)b.x) - PEN_WIDTH - 2;
    int miny = std::min((int)a.y, (int)b.y) - PEN_WIDTH - 2;
    int maxx = std::max((int)a.x, (int)b.x) + PEN_WIDTH + 2;
    int maxy = std::max((int)a.y, (int)b.y) + PEN_WIDTH + 2;
    expand_dirty_rect(minx, miny, maxx, maxy);
}

static void discard_buffered_touch_samples()
{
    unsigned int count = 0;
    while (PopTouchSection(0, &count) != NULL)
        ;
}

void canvas_pointer_down(int x, int y)
{
    int fy = _height - TOOLBAR_HEIGHT;
    if (y < TITLEBAR_HEIGHT)
    {
        handle_canvas_page_nav(x, y);
        return;
    }

    if (y >= fy)
    {
        _toolbar_touch_active = true;
        _toolbar_touch_x = x;
        return;
    }
    _toolbar_touch_active = false;

    if (y < canvas_draw_top()) return;

    ensure_current_page();
    StrokeList &current_strokes = _pages[_current_page];

    discard_buffered_touch_samples();

    _drawing = true;
    Stroke st;
    Point p; p.x = (short)x; p.y = (short)y;
    st.push_back(p);
    current_strokes.push_back(st);

    draw_overlay_circle(x, y);
    blit_ink_overlay_rect(x - PEN_WIDTH - 4, y - PEN_WIDTH - 4, x + PEN_WIDTH + 4, y + PEN_WIDTH + 4);
    expand_dirty_rect(x - PEN_WIDTH - 4, y - PEN_WIDTH - 4, x + PEN_WIDTH + 4, y + PEN_WIDTH + 4);

    SetWeakTimer("draw_flush", draw_flush_timer_cb, DRAW_FLUSH_INTERVAL_MS);
}

void canvas_pointer_move(int x, int y)
{
    ensure_current_page();
    StrokeList &current_strokes = _pages[_current_page];
    if (!_drawing || current_strokes.empty()) return;

    Stroke &st = current_strokes.back();
    int top = canvas_draw_top();
    int bottom = _height - TOOLBAR_HEIGHT;

    if (USE_TOUCH_SECTION_SMOOTHING)
    {
        std::vector<Point> extra;
        unsigned int count = 0;
        iv_mtinfo_section *sec;
        while ((sec = PopTouchSection(0, &count)) != NULL)
        {
            iv_mtinfo &pt = (*sec)[0];
            if (pt.active)
            {
                Point p; p.x = (short)pt.x; p.y = (short)pt.y;
                extra.push_back(p);
            }
        }

        if (!extra.empty())
        {
            std::reverse(extra.begin(), extra.end());
            for (size_t i = 0; i < extra.size(); i++)
            {
                const Point &p = extra[i];
                if (p.y < top || p.y >= bottom) continue;
                if (!st.empty())
                    draw_segment_and_mark_dirty(st.back(), p);
                st.push_back(p);
            }
            return;
        }
    }

    if (y < top || y >= bottom) return;

    Point p; p.x = (short)x; p.y = (short)y;
    if (!st.empty())
        draw_segment_and_mark_dirty(st.back(), p);
    st.push_back(p);
}

void canvas_pointer_up(int, int)
{
    if (_toolbar_touch_active)
    {
        _toolbar_touch_active = false;
        handle_canvas_toolbar_tap(_toolbar_touch_x);
        return;
    }

    if (_drawing)
    {
        _drawing = false;
        _dirty = true;
        flush_dirty_rect();
    }
}
