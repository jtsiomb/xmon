#include <stdio.h>
#include <string.h>
#include "xmon.h"
#include "options.h"

static XColor col_bar = {0, 0x5000, 0x5000, 0xffff};
static XRectangle rect;

static int memfmt(char *buf, long mem);


int memmon_init(void)
{
	XAllocColor(dpy, cmap, &col_bar);
	return 0;
}

void memmon_destroy(void)
{
}

void memmon_move(int x, int y)
{
	rect.x = x;
	rect.y = y;
}

void memmon_resize(int x, int y)
{
	rect.width = x;
	rect.height = y;
}

void memmon_draw(void)
{
	char buf[128], *ptr;
	long used, ratio;
	int baseline, y, bar, max_bar, bar_thick;

	if(smon.mem_total <= 0) return;

	XSetForeground(dpy, gc, opt.vis.uicolor[COL_BG].pixel);
	XFillRectangle(dpy, win, gc, rect.x, rect.y, rect.width, font_height * 2);

	used = smon.mem_total - smon.mem_free;
	ratio = used * 100 / smon.mem_total;
	baseline = rect.y + font_height - font->descent - 1;

	XSetForeground(dpy, gc, opt.vis.uicolor[COL_FG].pixel);

	sprintf(buf, "MEM %3ld%%", ratio);
	XDrawString(dpy, win, gc, rect.x, baseline, buf, strlen(buf));
	baseline += font_height;

	ptr = buf;
	ptr += memfmt(ptr, used);
	strcpy(ptr, " / "); ptr += 3;
	ptr += memfmt(ptr, smon.mem_total);
	XDrawString(dpy, win, gc, rect.x, baseline, buf, strlen(buf));

	y = baseline + font->descent + 1 + BEVEL;
	bar_thick = BEVEL * 2;
	if(bar_thick < 4) bar_thick = 4;
	max_bar = rect.width - BEVEL * 2;
	bar = ratio * max_bar / 100;

	draw_frame(rect.x, y, rect.width, bar_thick + 2 * BEVEL, -BEVEL);
	y += BEVEL;

	XSetForeground(dpy, gc, col_bar.pixel);
	XFillRectangle(dpy, win, gc, rect.x + BEVEL, y, bar, bar_thick);

	if(BEVEL) {
		/* if we have bevels, the trough is visible just with the background color */
		XSetForeground(dpy, gc, opt.vis.uicolor[COL_BG].pixel);
	} else {
		/* without bevels, let's paint it lighter */
		XSetForeground(dpy, gc, opt.vis.uicolor[COL_BGHI].pixel);
	}
	XFillRectangle(dpy, win, gc, rect.x + BEVEL + bar, y, max_bar - bar, bar_thick);
}

static int memfmt(char *buf, long mem)
{
	int idx = 0;
	int frac = 0;
	static const char *suffix[] = {"k", "m", "g", "t", "p", 0};

	while(mem >= 1024 && suffix[idx + 1]) {
		frac = mem & 1023;
		mem >>= 10;
		idx++;
	}

	frac = (frac * 1000) >> 10;
	while(frac > 10) frac /= 10;

	return sprintf(buf, "%ld.%d%s", mem, frac, suffix[idx]);
}
