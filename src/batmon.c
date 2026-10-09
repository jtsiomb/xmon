/*
xmon - a minimal lightweight cross-platform system monitoring utility
Copyright (C) 2025-2026  John Tsiombikas <nuclear@mutantstargoat.com>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/
#include <stdio.h>
#include <string.h>
#include "xmon.h"
#include "options.h"

static struct rect rect;

int batmon_init(void)
{
	return 0;
}

void batmon_move(int x, int y)
{
	rect.x = x;
	rect.y = y;
}

void batmon_resize(int x, int y)
{
	rect.width = x;
	rect.height = y;
}

int batmon_height(int w)
{
	return font.height * 2 + bar_height + 2;
}

void batmon_draw(void)
{
	char buf[128];
	int baseline, y;
	unsigned int hr, min;

	baseline = rect.y + font.height - font.descent - 1;

	set_color(uicolor[COL_BG]);
	draw_rect(rect.x, rect.y, rect.width, font.height * 2);

	sprintf(buf, "BAT %u%%", smon.bat);

	set_color(uicolor[COL_FG]);
	draw_text(rect.x, baseline, buf);
	baseline += font.height;

	if(smon.bat_state == BAT_FULL) {
		strcpy(buf, "FULL");
	} else {
		strcpy(buf, smon.bat_state == BAT_CHG ? "CHG" : "DIS");
		if(smon.bat_time == 0xffffffff) {
			strcpy(buf + 3, " --:--");
		} else {
			hr = smon.bat_time / 60;
			min = smon.bat_time % 60;
			sprintf(buf + 3, " %02u:%02u", hr, min);
		}
	}
	draw_text(rect.x, baseline, buf);

	y = baseline + font.descent + 1 + BEVEL;
	draw_bar(rect.x, y, rect.width, smon.bat, 100);
}
