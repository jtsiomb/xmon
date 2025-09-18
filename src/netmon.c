#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "xmon.h"
#include "options.h"

static XRectangle rect;
static unsigned int rx_acc, tx_acc;
static unsigned int rx_rate, tx_rate;
static unsigned int plot_max;
static long last_upd;
static unsigned int plot_width, plot_height;
static unsigned int plot_start, plot_end;		/* ring buffer indices */
static struct { unsigned int rx, tx; } *plot;
static XRectangle *rx_bars, *tx_bars, *both_bars;

static XColor col_rx = {0, 0x5050, 0x5050, 0xffff};
static XColor col_tx = {0, 0xffff, 0x5050, 0x5050};
static XColor col_both = {0, 0xc7c7, 0x5050, 0xffff};

#define ADV(x) \
	do { \
		if(++(x) >= plot_width) (x) = 0; \
	} while(0)

int netmon_init(void)
{
	plot_max = 1;

	XAllocColor(dpy, cmap, &col_rx);
	XAllocColor(dpy, cmap, &col_tx);
	XAllocColor(dpy, cmap, &col_both);

	return 0;
}

void netmon_move(int x, int y)
{
	rect.x = x;
	rect.y = y;
}

void netmon_resize(int x, int y)
{
	int i;

	if(!plot || x > plot_width) {
		free(plot);
		free(rx_bars);
		free(tx_bars);
		plot = calloc(x, sizeof *plot);
		rx_bars = malloc(x * sizeof *rx_bars);
		tx_bars = malloc(x * sizeof *tx_bars);
		both_bars = malloc(x * sizeof *both_bars);

		for(i=0; i<x; i++) {
			rx_bars[i].width = tx_bars[i].width = both_bars[i].width = 1;
		}
	}
	plot_start = plot_end = 0;
	plot_max = 1;

	rect.width = x;
	rect.height = y;

	plot_width = x - BEVEL * 2;
	plot_height = plot_width / 2;
	if(plot_height < 3) plot_height = 3;
}

int netmon_height(int w)
{
	plot_height = w / 2;
	return font_height * 3 + plot_height + BEVEL * 2;
}

void netmon_draw(void)
{
	char buf[128], tbuf[64], rbuf[64];
	int x, y, baseline;
	unsigned int rval, tval, bval, idx, bar_x;
	unsigned long msec, interv;
	XRectangle *rbar, *tbar, *bbar;

	if(!plot) return;

	rx_acc += smon.net_rx;
	tx_acc += smon.net_tx;

	msec = get_msec();
	interv = msec - last_upd;
	if(interv) {
		rx_rate = rx_acc * 1000 / interv;
		tx_rate = tx_acc * 1000 / interv;
	} else {
		rx_rate = tx_rate = 0;
	}
	rx_acc = tx_acc = 0;
	last_upd = msec;

	rval = rx_rate;
	tval = tx_rate;

	strcpy(rbuf, "Rx ");
	memfmt(rbuf + 3, rx_rate, 0);
	strcat(rbuf, "/s");

	strcpy(tbuf, "Tx ");
	memfmt(tbuf + 3, tx_rate, 0);
	strcat(tbuf, "/s");

	plot[plot_end].rx = rval;
	plot[plot_end].tx = tval;
	ADV(plot_end);
	if(plot_end == plot_start) ADV(plot_start);

	baseline = rect.y + font_height - font->descent - 1;

	XSetForeground(dpy, gc, opt.vis.uicolor[COL_BG].pixel);
	XFillRectangle(dpy, win, gc, rect.x, rect.y, rect.width, font_height * 3);

	XSetForeground(dpy, gc, opt.vis.uicolor[COL_FG].pixel);
	if(opt.net.ifname) {
		sprintf(buf, "NET: %s", opt.net.ifname);
		XDrawString(dpy, win, gc, rect.x, baseline, buf, strlen(buf));
	} else {
		XDrawString(dpy, win, gc, rect.x, baseline, "NET", 3);
	}

	baseline += font_height;

	XSetForeground(dpy, gc, opt.vis.uicolor[COL_FG].pixel);

	y = baseline - font->ascent;
	XDrawString(dpy, win, gc, rect.x + 4, baseline, rbuf, strlen(rbuf));
	baseline += font_height;
	XDrawString(dpy, win, gc, rect.x + 4, baseline, tbuf, strlen(tbuf));

	XSetForeground(dpy, gc, col_rx.pixel);
	XFillRectangle(dpy, win, gc, rect.x, y + 2, 3, font->ascent - 2);
	y += font_height;
	XSetForeground(dpy, gc, col_tx.pixel);
	XFillRectangle(dpy, win, gc, rect.x, y + 2, 3, font->ascent - 2);


	/* plot */

	if(rval > plot_max) plot_max = rval;
	if(tval > plot_max) plot_max = tval;

	x = rect.x;
	y = baseline + font->descent;
	draw_frame(x, y, rect.width, plot_height + BEVEL * 2, -BEVEL);

	x += BEVEL;
	y += BEVEL;
	XSetForeground(dpy, gc, opt.vis.uicolor[COL_BG].pixel);
	XFillRectangle(dpy, win, gc, x, y, plot_width, plot_height);

	if(plot_end == plot_start) return;

	baseline = y + plot_height;
	idx = plot_end;
	rbar = rx_bars;
	tbar = tx_bars;
	bbar = both_bars;
	bar_x = x + plot_width;
	do {
		bar_x--;
		idx = idx > 0 ? idx - 1 : plot_width - 1;
		rval = plot[idx].rx * plot_height / plot_max;
		tval = plot[idx].tx * plot_height / plot_max;
		bval = rval < tval ? rval : tval;

		if(bval > 0) {
			bbar->x = bar_x;
			bbar->y = baseline - bval;
			bbar->height = bval;
			bbar++;
		}
		if(rval > tval) {
			rbar->x = bar_x;
			rbar->y = baseline - rval;
			rbar->height = rval - bval;
			rbar++;
		} else {
			tbar->x = bar_x;
			tbar->y = baseline - tval;
			tbar->height = tval - bval;
			tbar++;
		}
	} while(idx != plot_start);

	if(bbar != both_bars) {
		XSetForeground(dpy, gc, col_both.pixel);
		XFillRectangles(dpy, win, gc, both_bars, bbar - both_bars);
	}
	if(rbar != rx_bars) {
		XSetForeground(dpy, gc, col_rx.pixel);
		XFillRectangles(dpy, win, gc, rx_bars, rbar - rx_bars);
	}
	if(tbar != tx_bars) {
		XSetForeground(dpy, gc, col_tx.pixel);
		XFillRectangles(dpy, win, gc, tx_bars, tbar - tx_bars);
	}
}
