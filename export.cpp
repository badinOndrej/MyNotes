#include "export.h"
#include "storage.h"
#include "utils.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>

static int canvas_draw_top()
{
	return TITLEBAR_HEIGHT;
}

static int canvas_draw_bottom()
{
	return _height - TOOLBAR_HEIGHT;
}

static ibitmap *render_note_to_bitmap(const PageList &pages)
{
	int page_h = canvas_draw_bottom() - canvas_draw_top();
	if (page_h < 1) page_h = 1;

	int w = _width;
	int page_count = (int)pages.size();
	if (page_count < 1) page_count = 1;
	int h = page_h * page_count;

	ibitmap *bmp = NewBitmap24(w, h);
	if (!bmp) return NULL;

	icanvas offscreen;
	offscreen.width = bmp->width;
	offscreen.height = bmp->height;
	offscreen.scanline = bmp->scanline;
	offscreen.depth = bmp->depth;
	offscreen.clipx1 = 0;
	offscreen.clipy1 = 0;
	offscreen.clipx2 = bmp->width - 1;
	offscreen.clipy2 = bmp->height - 1;
	offscreen.addr = bmp->data;

	icanvas *prev = GetCanvas();
	SetCanvas(&offscreen);

	int top = canvas_draw_top();
	for (int page_index = 0; page_index < page_count; page_index++)
	{
		int page_offset_y = page_index * page_h;
		FillArea(0, page_offset_y, w, page_h, WHITE);
		if (page_index > 0)
			DrawLine(0, page_offset_y, w, page_offset_y, LGRAY);

		const StrokeList &strokes = pages[page_index];
		for (size_t s = 0; s < strokes.size(); s++)
		{
			const Stroke &st = strokes[s];
			if (st.size() == 1)
			{
				DrawCircle(st[0].x, st[0].y - top + page_offset_y, PEN_WIDTH / 2, BLACK);
			}
			for (size_t p = 1; p < st.size(); p++)
			{
				draw_thick_line(st[p - 1].x, st[p - 1].y - top + page_offset_y,
				                 st[p].x, st[p].y - top + page_offset_y, PEN_WIDTH / 2, BLACK);
			}
		}
	}

	SetCanvas(prev);
	return bmp;
}

static bool read_file(const char *path, unsigned char **out_data, long *out_size)
{
	FILE *f = fopen(path, "rb");
	if (!f) return false;

	fseek(f, 0, SEEK_END);
	long size = ftell(f);
	fseek(f, 0, SEEK_SET);

	if (size <= 0)
	{
		fclose(f);
		return false;
	}

	unsigned char *buf = (unsigned char *)malloc(size);
	if (!buf)
	{
		fclose(f);
		return false;
	}

	size_t read = fread(buf, 1, size, f);
	fclose(f);

	if ((long)read != size)
	{
		free(buf);
		return false;
	}

	*out_data = buf;
	*out_size = size;
	return true;
}

static bool validate_png_file(const char *path)
{
	static const unsigned char png_signature[8] = {137, 80, 78, 71, 13, 10, 26, 10};
	unsigned char header[sizeof(png_signature)];

	FILE *f = fopen(path, "rb");
	if (!f) return false;

	size_t read = fread(header, 1, sizeof(header), f);
	fclose(f);
	if (read != sizeof(header)) return false;
	if (memcmp(header, png_signature, sizeof(png_signature)) != 0)
		return false;

	ibitmap *bmp = LoadPNG(path, 0);
	if (!bmp) return false;
	free(bmp);
	return true;
}

static char *page_to_base64_png(const StrokeList &page, std::string *err_out)
{
	ibitmap *bmp = render_note_to_bitmap(PageList(1, page));
	if (!bmp)
	{
		if (err_out) *err_out = "could not allocate offscreen bitmap";
		return NULL;
	}

	int saved = SavePNG(TMP_PNG_PATH, bmp);
	free(bmp);
	if (saved != 0 && !validate_png_file(TMP_PNG_PATH))
	{
		iv_unlink(TMP_PNG_PATH);
		if (err_out)
		{
			char buf[128];
			snprintf(buf, sizeof(buf), "SavePNG failed (rc=%d) writing %s", saved, TMP_PNG_PATH);
			*err_out = buf;
		}
		return NULL;
	}

	unsigned char *png_data = NULL;
	long png_size = 0;
	if (!read_file(TMP_PNG_PATH, &png_data, &png_size))
	{
		iv_unlink(TMP_PNG_PATH);
		if (err_out)
		{
			char buf[160];
			snprintf(buf, sizeof(buf), "could not read back %s (errno %d: %s)",
			         TMP_PNG_PATH, errno, strerror(errno));
			*err_out = buf;
		}
		return NULL;
	}

	size_t b64_cap = ((png_size + 2) / 3) * 4 + 4;
	char *b64 = (char *)malloc(b64_cap);
	if (!b64)
	{
		free(png_data);
		if (err_out) *err_out = "out of memory encoding base64";
		return NULL;
	}

	int b64_len = base64_encode(png_data, (int)png_size, b64);
	if (b64_len < 0) b64_len = 0;
	b64[b64_len] = '\0';

	free(png_data);
	iv_unlink(TMP_PNG_PATH);

	return b64;
}

void export_notes(const std::vector<int> &ids)
{
	if (ids.empty())
	{
		Message(ICON_WARNING, "Export", "No notes selected.", 2500);
		return;
	}

	db_ensure_dirs();

	char filename[256];
	time_t now = time(NULL);
	struct tm *local = localtime(&now);
	char timestamp[32];
	strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", local);
	snprintf(filename, sizeof(filename), "%s/notes_export_%s.html", EXPORT_DIR, timestamp);

	FILE *f = fopen(filename, "w");
	if (!f)
	{
		Message(ICON_ERROR, "Export", "Could not create export file.", 3000);
		return;
	}

	fprintf(f,
		"<!DOCTYPE html>\n<html><head><meta charset=\"utf-8\">\n"
		"<title>Handwritten Notes Export</title>\n"
		"<style>"
		"body{font-family:sans-serif;background:#f5f5f5;margin:0;padding:20px;}"
		".note{background:#fff;border:1px solid #ccc;border-radius:8px;"
		"padding:16px;margin-bottom:24px;max-width:900px;}"
		".note h2{margin:0 0 4px 0;}"
		".meta{color:#777;font-size:0.85em;margin-bottom:12px;}"
		".note img{max-width:100%%;border:1px solid #ddd;background:#fff;display:block;margin-top:8px;}"
		".page{margin-top:12px;}"
		".page-label{font-size:0.9em;color:#666;margin-bottom:4px;}"
		"</style></head><body>\n"
		"<h1>Handwritten Notes</h1>\n");

	int exported = 0;
	for (size_t i = 0; i < ids.size(); i++)
	{
		const NoteMeta *note = NULL;
		for (size_t j = 0; j < _notes.size(); j++)
		{
			if (_notes[j].id == ids[i]) { note = &_notes[j]; break; }
		}
		if (!note) continue;

		PageList pages = db_load_pages(note->id);
		if (pages.empty())
			pages.push_back(StrokeList());

		fprintf(f, "<div class=\"note\">\n");
		fprintf(f, "  <h2>%s</h2>\n", html_escape(note->title.empty() ? "Untitled" : note->title).c_str());
		fprintf(f, "  <div class=\"meta\">Created: %s &nbsp;|&nbsp; Updated: %s</div>\n",
			format_date(note->created_at).c_str(), format_date(note->updated_at).c_str());

		for (size_t page_index = 0; page_index < pages.size(); page_index++)
		{
			std::string render_err;
			char *b64 = page_to_base64_png(pages[page_index], &render_err);

			fprintf(f, "  <div class=\"page\">\n");
			fprintf(f, "    <div class=\"page-label\">Page %zu</div>\n", page_index + 1);
			if (b64)
			{
				fprintf(f, "    <img src=\"data:image/png;base64,%s\" alt=\"note page %zu\"/>\n", b64, page_index + 1);
				free(b64);
				exported++;
			}
			else
			{
				fprintf(f, "    <p><em>(could not render this page: %s)</em></p>\n",
					html_escape(render_err.empty() ? "unknown error" : render_err).c_str());
			}
			fprintf(f, "  </div>\n");
		}

		fprintf(f, "</div>\n");
	}

	fprintf(f, "</body></html>\n");
	fclose(f);

	char msg[384];
	snprintf(msg, sizeof(msg), "Exported %d page image(s) to:\n%s", exported, filename);
	Message(ICON_INFORMATION, "Export complete", msg, 4000);
}
