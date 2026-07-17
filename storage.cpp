#include "storage.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static std::string serialize_strokes(const StrokeList &strokes)
{
	std::string out;
	char num[32];
	for (size_t s = 0; s < strokes.size(); s++)
	{
		if (s > 0) out += ";";
		const Stroke &st = strokes[s];
		for (size_t p = 0; p < st.size(); p++)
		{
			if (p > 0) out += "|";
			snprintf(num, sizeof(num), "%d,%d", st[p].x, st[p].y);
			out += num;
		}
	}
	return out;
}

static StrokeList deserialize_strokes(const std::string &data)
{
	StrokeList strokes;
	size_t pos = 0;
	while (pos <= data.size())
	{
		size_t semi = data.find(';', pos);
		std::string stroke_str = (semi == std::string::npos)
			? data.substr(pos)
			: data.substr(pos, semi - pos);

		if (!stroke_str.empty())
		{
			Stroke st;
			size_t ppos = 0;
			while (ppos <= stroke_str.size())
			{
				size_t bar = stroke_str.find('|', ppos);
				std::string pt_str = (bar == std::string::npos)
					? stroke_str.substr(ppos)
					: stroke_str.substr(ppos, bar - ppos);

				if (!pt_str.empty())
				{
					int x = 0, y = 0;
					if (sscanf(pt_str.c_str(), "%d,%d", &x, &y) == 2)
					{
						Point pt;
						pt.x = (short)x;
						pt.y = (short)y;
						st.push_back(pt);
					}
				}

				if (bar == std::string::npos) break;
				ppos = bar + 1;
			}
			if (!st.empty())
				strokes.push_back(st);
		}

		if (semi == std::string::npos) break;
		pos = semi + 1;
	}
	return strokes;
}

static std::string serialize_pages(const PageList &pages)
{
	std::string out;
	for (size_t i = 0; i < pages.size(); i++)
	{
		if (i > 0) out += '\x1f';
		out += serialize_strokes(pages[i]);
	}
	return out;
}

static PageList deserialize_pages(const std::string &data)
{
	PageList pages;
	if (data.empty())
	{
		pages.push_back(StrokeList());
		return pages;
	}

	if (data.find('\x1f') == std::string::npos)
	{
		pages.push_back(deserialize_strokes(data));
		return pages;
	}

	size_t pos = 0;
	while (pos <= data.size())
	{
		size_t sep = data.find('\x1f', pos);
		std::string page_str = (sep == std::string::npos)
			? data.substr(pos)
			: data.substr(pos, sep - pos);
		pages.push_back(deserialize_strokes(page_str));
		if (sep == std::string::npos) break;
		pos = sep + 1;
	}
	return pages;
}

void db_ensure_dirs()
{
	iv_mkdir(FLASHDIR, 0777);
	iv_mkdir(APP_DATA_DIR, 0777);
	iv_mkdir(EXPORT_DIR, 0777);
	iv_buildpath(DB_PATH);
}

bool db_init()
{
	db_ensure_dirs();

	if (iv_access(APP_DATA_DIR, 0) != 0)
	{
		char buf[512];
		snprintf(buf, sizeof(buf),
			 "Data folder is not accessible:\n%s\n(errno %d: %s)",
			 APP_DATA_DIR, errno, strerror(errno));
		Message(ICON_ERROR, "Notes", buf, 5000);
		return false;
	}

	int rc = sqlite3_open(DB_PATH, &_db);
	if (rc != SQLITE_OK)
	{
		char buf[512];
		snprintf(buf, sizeof(buf),
			 "Could not open database (rc=%d):\n%s\npath: %s",
			 rc, _db ? sqlite3_errmsg(_db) : "sqlite3_open returned NULL handle",
			 DB_PATH);
		Message(ICON_ERROR, "Notes", buf, 6000);
		if (_db) { sqlite3_close(_db); _db = NULL; }
		return false;
	}

	const char *sql =
		"CREATE TABLE IF NOT EXISTS notes ("
		"  id INTEGER PRIMARY KEY AUTOINCREMENT,"
		"  title TEXT NOT NULL DEFAULT '',"
		"  created_at INTEGER NOT NULL,"
		"  updated_at INTEGER NOT NULL,"
		"  strokes TEXT NOT NULL DEFAULT ''"
		");";

	char *err = NULL;
	if (sqlite3_exec(_db, sql, NULL, NULL, &err) != SQLITE_OK)
	{
		char buf[512];
		snprintf(buf, sizeof(buf), "Could not create table: %s", err ? err : "?");
		Message(ICON_ERROR, "Notes", buf, 5000);
		sqlite3_free(err);
		sqlite3_close(_db);
		_db = NULL;
		return false;
	}

	return true;
}

void db_close()
{
	if (_db)
	{
		sqlite3_close(_db);
		_db = NULL;
	}
}

void db_load_notes()
{
	_notes.clear();
	if (!_db) return;

	const char *sql = "SELECT id, title, created_at, updated_at FROM notes ORDER BY updated_at DESC;";
	sqlite3_stmt *stmt = NULL;

	if (sqlite3_prepare_v2(_db, sql, -1, &stmt, NULL) != SQLITE_OK)
		return;

	while (sqlite3_step(stmt) == SQLITE_ROW)
	{
		NoteMeta n;
		n.id = sqlite3_column_int(stmt, 0);
		const unsigned char *title = sqlite3_column_text(stmt, 1);
		n.title = title ? (const char *)title : "";
		n.created_at = (time_t)sqlite3_column_int64(stmt, 2);
		n.updated_at = (time_t)sqlite3_column_int64(stmt, 3);
		_notes.push_back(n);
	}

	sqlite3_finalize(stmt);
}

int db_create_note(const std::string &title)
{
	if (!_db) return -1;

	const char *sql = "INSERT INTO notes (title, created_at, updated_at, strokes) VALUES (?, ?, ?, '');";
	sqlite3_stmt *stmt = NULL;
	if (sqlite3_prepare_v2(_db, sql, -1, &stmt, NULL) != SQLITE_OK)
		return -1;

	time_t now = time(NULL);
	sqlite3_bind_text(stmt, 1, title.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_int64(stmt, 2, (sqlite3_int64)now);
	sqlite3_bind_int64(stmt, 3, (sqlite3_int64)now);

	int id = -1;
	if (sqlite3_step(stmt) == SQLITE_DONE)
		id = (int)sqlite3_last_insert_rowid(_db);

	sqlite3_finalize(stmt);
	return id;
}

void db_save_note(int id, const std::string &title, const PageList &pages)
{
	if (!_db || id < 0) return;

	const char *sql = "UPDATE notes SET title = ?, updated_at = ?, strokes = ? WHERE id = ?;";
	sqlite3_stmt *stmt = NULL;
	if (sqlite3_prepare_v2(_db, sql, -1, &stmt, NULL) != SQLITE_OK)
		return;

	std::string data = serialize_pages(pages);

	sqlite3_bind_text(stmt, 1, title.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_int64(stmt, 2, (sqlite3_int64)time(NULL));
	sqlite3_bind_text(stmt, 3, data.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_int(stmt, 4, id);

	sqlite3_step(stmt);
	sqlite3_finalize(stmt);
}

PageList db_load_pages(int id)
{
	PageList pages;
	if (!_db || id < 0) return pages;

	const char *sql = "SELECT strokes FROM notes WHERE id = ?;";
	sqlite3_stmt *stmt = NULL;
	if (sqlite3_prepare_v2(_db, sql, -1, &stmt, NULL) != SQLITE_OK)
		return pages;

	sqlite3_bind_int(stmt, 1, id);

	if (sqlite3_step(stmt) == SQLITE_ROW)
	{
		const unsigned char *data = sqlite3_column_text(stmt, 0);
		if (data)
			pages = deserialize_pages((const char *)data);
	}

	sqlite3_finalize(stmt);
	return pages;
}

void db_delete_note(int id)
{
	if (!_db) return;

	const char *sql = "DELETE FROM notes WHERE id = ?;";
	sqlite3_stmt *stmt = NULL;
	if (sqlite3_prepare_v2(_db, sql, -1, &stmt, NULL) != SQLITE_OK)
		return;

	sqlite3_bind_int(stmt, 1, id);
	sqlite3_step(stmt);
	sqlite3_finalize(stmt);
}
