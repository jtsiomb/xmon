#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <alloca.h>
#ifndef NO_XSHM
#include <sys/ipc.h>
#include <sys/shm.h>
#endif
#include "xmon.h"
#include "options.h"

#define GRAD_COLORS	4

struct color {
	int r, g, b;
};

static XColor *colors;
static XImage *ximg;
static int rshift, gshift, bshift;
static XRectangle rect, view_rect, lb_rect;

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

	if(opt.cpu.ncolors <= 2) {
		return -1;
	}
	if(!(colors = malloc(opt.cpu.ncolors * sizeof *colors))) {
		fprintf(stderr, "CPU: failed to allocate array of %d colors\n", opt.cpu.ncolors);
		return -1;
	}

	for(i=0; i<GRAD_COLORS; i++) {
		struct color *col = grad + i;
		col->r <<= 8;
		if(col->r & 0x100) col->r |= 0xff;
		col->g <<= 8;
		if(col->g & 0x100) col->g |= 0xff;
		col->b <<= 8;
		if(col->b & 0x100) col->b |= 0xff;
	}

	for(i=0; i<opt.cpu.ncolors; i++) {
		int seg = i * (GRAD_COLORS - 1) / opt.cpu.ncolors;
		int t = i * (GRAD_COLORS - 1) % opt.cpu.ncolors;
		struct color *c0 = grad + seg;
		struct color *c1 = c0 + 1;

		colors[i].red = c0->r + (c1->r - c0->r) * t / (opt.cpu.ncolors - 1);
		colors[i].green = c0->g + (c1->g - c0->g) * t / (opt.cpu.ncolors - 1);
		colors[i].blue = c0->b + (c1->b - c0->b) * t / (opt.cpu.ncolors - 1);
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
	rect.x = x;
	rect.y = y;

	view_rect.x = rect.x + BEVEL;
	view_rect.y = rect.y + BEVEL + font_height;

	lb_rect.x = view_rect.x;
	lb_rect.y = view_rect.y - BEVEL - font_height - 1;
}

void cpumon_resize(int x, int y)
{
	rect.width = x;
	rect.height = y;

	view_rect.width = x - BEVEL * 2;
	view_rect.height = y - BEVEL * 2 - font_height;

	lb_rect.width = view_rect.width;
	lb_rect.height = font_height;

	resize_framebuf(view_rect.width, view_rect.height);
}

int cpumon_height(int w)
{
	int h = w;
	int min_h = font_height + 2 * BEVEL + 8;
	return h < min_h ? min_h : h;
}

void cpumon_update(void)
{
	int i, row_offs, cur, col0;
	unsigned char *fb, *row;
	unsigned int *row32;
	int *cpucol;

	if(!ximg) return;

	fb = (unsigned char*)ximg->data;

	cpucol = alloca(smon.num_cpus * sizeof *cpucol);
	for(i=0; i<smon.num_cpus; i++) {
		int usage = smon.cpu[i];
		if(usage >= 128) usage = 127;
		cpucol[i] = (usage * opt.cpu.ncolors) >> 7;
	}

	row_offs = (ximg->height - 1) * ximg->bytes_per_line;

	/* scroll up */
	memmove(fb, fb + ximg->bytes_per_line, row_offs);

	/* draw the bottom line with the current stats */
	row = fb + row_offs;

	switch(ximg->bits_per_pixel) {
	case 8:
		for(i=0; i<ximg->width; i++) {
			cur = i * smon.num_cpus / ximg->width;
			col0 = cpucol[cur];
			*row++ = colors[col0].pixel;
		}
		break;

	case 32:
		row32 = (unsigned int*)row;
		for(i=0; i<ximg->width; i++) {
			int r, g, b;
			cur = i * smon.num_cpus / ximg->width;
			col0 = cpucol[cur];

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
	int baseline;
	char buf[128];

	draw_frame(view_rect.x - BEVEL, view_rect.y - BEVEL, view_rect.width + BEVEL * 2,
			view_rect.height + BEVEL * 2, -BEVEL);

	XSetForeground(dpy, gc, opt.vis.uicolor[COL_BG].pixel);
	XFillRectangle(dpy, win, gc, lb_rect.x, lb_rect.y, lb_rect.width, lb_rect.height);

	baseline = lb_rect.y + lb_rect.height - font->descent;
	sprintf(buf, "CPU %3d%%", smon.single * 100 >> 7);
	XSetForeground(dpy, gc, opt.vis.uicolor[COL_FG].pixel);
	XSetBackground(dpy, gc, opt.vis.uicolor[COL_BG].pixel);
	XDrawString(dpy, win, gc, lb_rect.x, baseline, buf, strlen(buf));

	if(!ximg) return;

#ifndef NO_XSHM
	if(have_xshm) {
		XShmPutImage(dpy, win, gc, ximg, 0, 0, view_rect.x, view_rect.y,
				ximg->width, ximg->height, False);
	} else
#endif
	{
		XPutImage(dpy, win, gc, ximg, 0, 0, view_rect.x, view_rect.y,
				ximg->width, ximg->height);
	}
}

static void free_framebuf(void)
{
	if(!ximg) return;

#ifndef NO_XSHM
	if(have_xshm) {
		XShmDetach(dpy, &xshm);
		XDestroyImage(ximg);
		if(xshm.shmaddr != (void*)-1) {
			shmdt(xshm.shmaddr);
			xshm.shmaddr = (void*)-1;
		}
		if(xshm.shmid != -1) {
			shmctl(xshm.shmid, IPC_RMID, 0);
			xshm.shmid = -1;
		}
	} else
#endif
	{
		XDestroyImage(ximg);
	}
	ximg = 0;
}

static int resize_framebuf(int width, int height)
{
	if(ximg && width == ximg->width && height == ximg->height) {
		return 0;
	}

	free_framebuf();

	if(width <= 0 || height <= 0) {
		return -1;
	}

#ifndef NO_XSHM
	if(have_xshm) {
		if(!(ximg = XShmCreateImage(dpy, vinf->visual, vinf->depth, ZPixmap, 0,
						&xshm, width, height))) {
			return -1;
		}
		if((xshm.shmid = shmget(IPC_PRIVATE, ximg->bytes_per_line * ximg->height,
				IPC_CREAT | 0777)) == -1) {
			fprintf(stderr, "failed to create shared memory, fallback to no XShm\n");
			free_framebuf();
			goto no_xshm;
		}
		if((xshm.shmaddr = ximg->data = shmat(xshm.shmid, 0, 0)) == (void*)-1) {
			fprintf(stderr, "failed to attach shared memory, fallback to no XShm\n");
			free_framebuf();
			goto no_xshm;
		}
		xshm.readOnly = True;
		if(!XShmAttach(dpy, &xshm)) {
			fprintf(stderr, "XShmAttach failed\n");
			free_framebuf();
			goto no_xshm;
		}
	} else {
no_xshm:
		have_xshm = 0;
#else
	{
#endif
		if(!(ximg = XCreateImage(dpy, vinf->visual, vinf->depth, ZPixmap, 0, 0,
						width, height, 8, 0))) {
			return -1;
		}

		if(!(ximg->data = calloc(1, height * ximg->bytes_per_line))) {
			XDestroyImage(ximg);
			ximg = 0;
			return -1;
		}
	}
	rshift = mask_to_shift(ximg->red_mask);
	gshift = mask_to_shift(ximg->green_mask);
	bshift = mask_to_shift(ximg->blue_mask);
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
