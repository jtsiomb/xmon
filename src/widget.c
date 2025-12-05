#include <stdio.h>
#include <stdlib.h>
#include "widget.h"
#include "xmon.h"
#include "options.h"

XFontStruct *font;
int font_height;
int bar_height;

static XColor col_bar = {0, 0x5000, 0x5000, 0xffff};

int init_widgets(void)
{
	int i, bar_thick;

	if(!(font = XLoadQueryFont(dpy, opt.vis.font))) {
		fprintf(stderr, "failed to load font: %s\n", opt.vis.font);
		return 1;
	}
	font_height = font->ascent + font->descent;
	XSetFont(dpy, gc, font->fid);

	for(i=0; i<NUM_UICOLORS; i++) {
		XAllocColor(dpy, cmap, opt.vis.uicolor + i);
	}

	XAllocColor(dpy, cmap, &col_bar);

	bar_thick = BEVEL * 2;
	if(bar_thick < 4) bar_thick = 4;
	bar_height = BEVEL * 2 + bar_thick;
	return 0;
}


static void point(XPoint *p, int x, int y)
{
	p->x = x;
	p->y = y;
}

void draw_frame(int x, int y, int w, int h, int depth)
{
	int bevel;
	XPoint v[4];

	if(depth == 0) return;

	bevel = abs(depth);

	if(bevel == 1) {
		XSetLineAttributes(dpy, gc, bevel, LineSolid, CapButt, JoinBevel);

		point(v, x, y + h - 1);
		point(v + 1, x, y);
		point(v + 2, x + w - 1, y);

		XSetForeground(dpy, gc, opt.vis.uicolor[depth > 0 ? COL_BGHI : COL_BGLO].pixel);
		XDrawLines(dpy, win, gc, v, 3, CoordModeOrigin);

		point(v, x + w - 1, y);
		point(v + 1, x + w - 1, y + h - 1);
		point(v + 2, x, y + h - 1);

		XSetForeground(dpy, gc, opt.vis.uicolor[depth > 0 ? COL_BGLO : COL_BGHI].pixel);
		XDrawLines(dpy, win, gc, v, 3, CoordModeOrigin);
	} else {
		XSetForeground(dpy, gc, opt.vis.uicolor[depth > 0 ? COL_BGHI : COL_BGLO].pixel);

		point(v, x, y);
		point(v + 1, x + bevel, y + bevel);
		point(v + 2, x + bevel, y + h - bevel);
		point(v + 3, x, y + h);
		XFillPolygon(dpy, win, gc, v, 4, Convex, CoordModeOrigin);

		point(v, x, y);
		point(v + 1, x + w, y);
		point(v + 2, x + w - bevel, y + bevel);
		point(v + 3, x + bevel, y + bevel);
		XFillPolygon(dpy, win, gc, v, 4, Convex, CoordModeOrigin);

		XSetForeground(dpy, gc, opt.vis.uicolor[depth > 0 ? COL_BGLO : COL_BGHI].pixel);

		point(v, x + w, y);
		point(v + 1, x + w, y + h);
		point(v + 2, x + w - bevel, y + h - bevel);
		point(v + 3, x + w - bevel, y + bevel);
		XFillPolygon(dpy, win, gc, v, 4, Convex, CoordModeOrigin);

		point(v, x + w, y + h);
		point(v + 1, x, y + h);
		point(v + 2, x + bevel, y + h - bevel);
		point(v + 3, x + w - bevel, y + h - bevel);
		XFillPolygon(dpy, win, gc, v, 4, Convex, CoordModeOrigin);
	}
}


void draw_bar(int x, int y, int w, int val, int total)
{
	int bar, max_bar, bar_thick;

	bar_thick = BEVEL * 2;
	if(bar_thick < 4) bar_thick = 4;
	max_bar = w - BEVEL * 2;

	if(val >= total) {
		bar = max_bar;
	} else {
		bar = val * max_bar / total;
	}

	draw_frame(x, y, w, bar_height, -BEVEL);
	y += BEVEL;

	XSetForeground(dpy, gc, col_bar.pixel);
	XFillRectangle(dpy, win, gc, x + BEVEL, y, bar, bar_thick);

	if(BEVEL) {
		/* if we have bevels, the trough is visible just with the background color */
		XSetForeground(dpy, gc, opt.vis.uicolor[COL_BG].pixel);
	} else {
		/* without bevels, let's paint it lighter */
		XSetForeground(dpy, gc, opt.vis.uicolor[COL_BGHI].pixel);
	}
	XFillRectangle(dpy, win, gc, x + BEVEL + bar, y, max_bar - bar, bar_thick);
}
