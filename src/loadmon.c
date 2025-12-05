#include <stdio.h>
#include <string.h>
#include "xmon.h"
#include "options.h"

static XRectangle rect;

void loadmon_move(int x, int y)
{
	rect.x = x;
	rect.y = y;
}

void loadmon_resize(int x, int y)
{
	rect.width = x;
	rect.height = y;
}

int loadmon_height(int w)
{
	return font_height + bar_height + 2;
}

void loadmon_draw(void)
{
	char buf[128];
	int baseline, y, val;

	baseline = rect.y + font_height - font->descent - 1;

	XSetForeground(dpy, gc, opt.vis.uicolor[COL_BG].pixel);
	XFillRectangle(dpy, win, gc, rect.x, rect.y, rect.width, font_height);

	sprintf(buf, "LOAD %.2f", smon.loadavg[0]);

	XSetForeground(dpy, gc, opt.vis.uicolor[COL_FG].pixel);
	XDrawString(dpy, win, gc, rect.x, baseline, buf, strlen(buf));

	y = baseline + font->descent + 1 + BEVEL;
	val = (int)(smon.loadavg[0] * 1024.0);
	draw_bar(rect.x, y, rect.width, val, 0x4000);
}
