#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "xmon.h"
#include "options.h"

static struct rect rect;
static unsigned long rx_acc, tx_acc;
static unsigned long rx_rate, tx_rate;
static unsigned int plot_max;
static long last_upd;
static unsigned int plot_width, plot_height;
static unsigned int plot_start, plot_end;		/* ring buffer indices */
static struct { unsigned int rx, tx; } *plot;
static struct rect *rx_bars, *tx_bars, *both_bars;

static unsigned int col_rx, col_tx, col_both;

#define ADV(x) \
	do { \
		if(++(x) >= plot_width) (x) = 0; \
	} while(0)

int netmon_init(void)
{
	plot_max = 1;

	col_rx = uicolor[COL_A];
	col_tx = uicolor[COL_B];
	col_both = uicolor[COL_AB];

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

	if(!plot || x > (int)plot_width) {
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
	return font.height * 3 + plot_height + BEVEL * 2;
}

void netmon_draw(void)
{
	char buf[128], tbuf[64], rbuf[64];
	int x, y, baseline;
	unsigned int rval, tval, bval, idx, bar_x;
	unsigned long msec, interv;
	struct rect *rbar, *tbar, *bbar;

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

	baseline = rect.y + font.height - font.descent - 1;

	set_color(uicolor[COL_BG]);
	draw_rect(rect.x, rect.y, rect.width, font.height * 3);

	set_color(uicolor[COL_FG]);
	if(opt.net.ifname) {
		sprintf(buf, "NET: %s", opt.net.ifname);
		draw_text(rect.x, baseline, buf);
	} else {
		draw_text(rect.x, baseline, "NET");
	}

	baseline += font.height;

	set_color(uicolor[COL_FG]);

	y = baseline - font.ascent;
	draw_text(rect.x + 4, baseline, rbuf);
	baseline += font.height;
	draw_text(rect.x + 4, baseline, tbuf);

	set_color(col_rx);
	draw_rect(rect.x, y + 2, 3, font.ascent - 2);
	y += font.height;
	set_color(col_tx);
	draw_rect(rect.x, y + 2, 3, font.ascent - 2);


	/* plot */

	if(rval > plot_max) plot_max = rval;
	if(tval > plot_max) plot_max = tval;

	x = rect.x;
	y = baseline + font.descent;
	draw_frame(x, y, rect.width, plot_height + BEVEL * 2, -BEVEL);

	x += BEVEL;
	y += BEVEL;
	set_color(uicolor[COL_BG]);
	draw_rect(x, y, plot_width, plot_height);

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
		/*rval = plot[idx].rx * plot_height / plot_max;
		tval = plot[idx].tx * plot_height / plot_max;*/

		rval = (plot[idx].rx << 7) / plot_max;
		if(rval > 127) rval = 127;
		rval = (plotlut[rval] * plot_height) >> 7;
		if(!rval && plot[idx].rx) rval = 1;

		tval = (plot[idx].tx << 7) / plot_max;
		if(tval > 127) tval = 127;
		tval = (plotlut[tval] * plot_height) >> 7;
		if(!tval && plot[idx].tx) tval = 1;

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
		set_color(col_both);
		draw_rects(both_bars, bbar - both_bars);
	}
	if(rbar != rx_bars) {
		set_color(col_rx);
		draw_rects(rx_bars, rbar - rx_bars);
	}
	if(tbar != tx_bars) {
		set_color(col_tx);
		draw_rects(tx_bars, tbar - tx_bars);
	}
}

void netmon_rclick(int x, int y)
{
	plot_max = 1;
	redisplay(UI_NET);
}
