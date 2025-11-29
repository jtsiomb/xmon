#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "xmon.h"
#include "sysmon.h"

#define NUM_COLORS	16
#define GRAD_COLORS	4

struct color {
	int r, g, b;
};

static XColor colors[NUM_COLORS];
static unsigned char *fb;
static XImage *ximg;
static int rshift, gshift, bshift;
static int xpos, ypos;

struct color grad[GRAD_COLORS] = {
	{5, 12, 80},
	{128, 8, 4},
	{255, 128, 64},
	{255, 250, 220}
};


static int resize_framebuf(int width, int height);
static int mask_to_shift(unsigned int mask);


int cpumon_init(void)
{
	int i;

	for(i=0; i<GRAD_COLORS; i++) {
		struct color *col = grad + i;
		col->r <<= 8;
		if(col->r & 0x100) col->r |= 0xff;
		col->g <<= 8;
		if(col->g & 0x100) col->g |= 0xff;
		col->b <<= 8;
		if(col->b & 0x100) col->b |= 0xff;
	}

	for(i=0; i<NUM_COLORS; i++) {
		int seg = i * (GRAD_COLORS - 1) / NUM_COLORS;
		int t = i * (GRAD_COLORS - 1) % NUM_COLORS;
		struct color *c0 = grad + seg;
		struct color *c1 = c0 + 1;

		colors[i].red = c0->r + (c1->r - c0->r) * t / (NUM_COLORS - 1);
		colors[i].green = c0->g + (c1->g - c0->g) * t / (NUM_COLORS - 1);
		colors[i].blue = c0->b + (c1->b - c0->b) * t / (NUM_COLORS - 1);
		if(!XAllocColor(dpy, cmap, colors + i)) {
			fprintf(stderr, "failed to allocate color %d\n", i);
		} else {
			/*printf("color %06lx: %3u %3u %3u\n", colors[i].pixel,
					colors[i].red >> 8, colors[i].green >> 8, colors[i].blue >> 8);*/
		}
	}

	return 0;
}

void cpumon_destroy(void)
{
	XDestroyImage(ximg);
	ximg = 0;
}

void cpumon_move(int x, int y)
{
	xpos = x;
	ypos = y;
}

void cpumon_resize(int x, int y)
{
	resize_framebuf(x, y);
}

void cpumon_update(void)
{
	int i, row_offs, cur, col0;
	unsigned char *row;
	unsigned int *row32;

	if(!ximg) return;

	row_offs = (ximg->height - 1) * ximg->bytes_per_line;

	/* scroll up */
	memmove(fb, fb + ximg->bytes_per_line, row_offs);

	/* draw the bottom line with the current stats */
	row = fb + row_offs;

	switch(ximg->bits_per_pixel) {
	case 8:
		for(i=0; i<ximg->width; i++) {
			cur = i * smon.num_cpus / ximg->width;
			col0 = (smon.cpu[cur] * NUM_COLORS) >> 7;
			*row++ = colors[col0].pixel;
		}
		break;

	case 32:
		row32 = (unsigned int*)row;
		for(i=0; i<ximg->width; i++) {
			int r, g, b;
			cur = i * smon.num_cpus / ximg->width;
			col0 = (smon.cpu[cur] * NUM_COLORS) >> 7;

			r = colors[col0].red >> 8;
			g = colors[col0].green >> 8;
			b = colors[col0].blue >> 8;
			*row32++ = (r << rshift) | (g << gshift) | (b << bshift);
		}
		break;

	default:
		break;
	}
}

void cpumon_draw(void)
{
	if(!ximg) return;

	XPutImage(dpy, win, gc, ximg, 0, 0, xpos, ypos, ximg->width, ximg->height);
}



static int resize_framebuf(int width, int height)
{
	int pitch;

	if(!ximg) {
		if(!(ximg = XCreateImage(dpy, vinf->visual, vinf->depth, ZPixmap, 0, 0,
						width, height, 8, 0))) {
			return -1;
		}
		rshift = mask_to_shift(ximg->red_mask);
		gshift = mask_to_shift(ximg->green_mask);
		bshift = mask_to_shift(ximg->blue_mask);
	}

	printf("resizing framebuffer: %dx%d %d bpp\n", width, height, ximg->bits_per_pixel);
	free(fb);

	pitch = width * ximg->bits_per_pixel;
	if(!(fb = calloc(1, height * pitch))) {
		return -1;
	}

	ximg->width = width;
	ximg->height = height;
	ximg->bytes_per_line = pitch;
	ximg->data = (char*)fb;
	return 0;
}

static int mask_to_shift(unsigned int mask)
{
	int i;
	for(i=0; i<32; i++) {
		if(mask & 1) return i;
		mask >>= 1;
	}
	return 0;
}
