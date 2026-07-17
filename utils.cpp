#include "utils.h"

#include <algorithm>
#include <ctime>
#include <cstdio>

bool is_selected(int id)
{
	return std::find(_selected_ids.begin(), _selected_ids.end(), id) != _selected_ids.end();
}

void toggle_selected(int id)
{
	auto it = std::find(_selected_ids.begin(), _selected_ids.end(), id);
	if (it == _selected_ids.end())
		_selected_ids.push_back(id);
	else
		_selected_ids.erase(it);
}

std::string format_date(time_t t)
{
	char buf[64];
	struct tm *tm_info = localtime(&t);
	strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", tm_info);
	return std::string(buf);
}

std::string html_escape(const std::string &in)
{
	std::string out;
	out.reserve(in.size());
	for (size_t i = 0; i < in.size(); i++)
	{
		char c = in[i];
		switch (c)
		{
		case '&': out += "&amp;"; break;
		case '<': out += "&lt;"; break;
		case '>': out += "&gt;"; break;
		case '"': out += "&quot;"; break;
		default: out += c; break;
		}
	}
	return out;
}

void draw_thick_line(int x1, int y1, int x2, int y2, int radius, int color)
{
	if (radius < 1) radius = 1;

	int dx = x2 - x1;
	int dy = y2 - y1;
	if (dx == 0 && dy == 0)
	{
		DrawCircle(x1, y1, radius, color);
		return;
	}

	if (std::abs(dx) >= std::abs(dy))
	{
		for (int off = -radius; off <= radius; off++)
		{
			DrawLineEx(x1, y1 + off, x2, y2 + off, color, 1);
		}
	}
	else
	{
		for (int off = -radius; off <= radius; off++)
		{
			DrawLineEx(x1 + off, y1, x2 + off, y2, color, 1);
		}
	}

	DrawCircle(x1, y1, radius, color);
	DrawCircle(x2, y2, radius, color);
}
