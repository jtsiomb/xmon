#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "xmon.h"
#include "options.h"

#if defined(BUILD_WIN32) || defined(__FreeBSD__)
#include <malloc.h>
#else
#include <alloca.h>
#endif

#define GRAD_COLORS	4

#define MIN_COLW		(BEVEL > 8 ? BEVEL : 8)
#define MIN_COL_STEP	(BEVEL * 2 + MIN_COLW)

static int sep_disp;
static unsigned int colw, col_step;

static struct color *rgbcolors;
static unsigned int *colors;
static int rshift, gshift, bshift;
static struct rect rect, view_rect, lb_rect;
static struct image *img;

static int info_idx;

struct color grad[GRAD_COLORS] = {
	{5, 12, 80},
	{128, 8, 4},
	{255, 128, 64},
	{255, 250, 220}
};

static int resize_framebuf(unsigned int width, unsigned int height);
static int mask_to_shift(unsigned int mask);

int cpumon_init(void)
{
	int i;

	if(opt.cpu.ncolors <= 2) {
		return -1;
	}
	if(!(colors = malloc(opt.cpu.ncolors * (sizeof *colors + sizeof *rgbcolors)))) {
		fprintf(stderr, "CPU: failed to allocate array of %d colors\n", opt.cpu.ncolors);
		return -1;
	}
	rgbcolors = (struct color*)(colors + opt.cpu.ncolors);

	for(i=0; i<opt.cpu.ncolors; i++) {
		int seg = i * (GRAD_COLORS - 1) / opt.cpu.ncolors;
		int t = i * (GRAD_COLORS - 1) % opt.cpu.ncolors;
		struct color *c0 = grad + seg;
		struct color *c1 = c0 + 1;

		rgbcolors[i].r = c0->r + (c1->r - c0->r) * t / (opt.cpu.ncolors - 1);
		rgbcolors[i].g = c0->g + (c1->g - c0->g) * t / (opt.cpu.ncolors - 1);
		rgbcolors[i].b = c0->b + (c1->b - c0->b) * t / (opt.cpu.ncolors - 1);
		colors[i] = alloc_color(rgbcolors[i].r, rgbcolors[i].g, rgbcolors[i].b);
	}

	info_idx = -1;
	return 0;
}

void cpumon_destroy(void)
{
	free_image(img);
}

void cpumon_move(int x, int y)
{
	rect.x = x;
	rect.y = y;

	view_rect.x = rect.x + BEVEL;
	view_rect.y = rect.y + BEVEL + font.height;

	lb_rect.x = view_rect.x;
	lb_rect.y = view_rect.y - BEVEL - font.height - 1;
}

void cpumon_resize(int x, int y)
{
	rect.width = x;
	rect.height = y;

	view_rect.width = x - BEVEL * 2;
	view_rect.height = y - BEVEL * 2 - font.height;

	if(opt.cpu.autosplit) {
		sep_disp = view_rect.width >= smon.num_cpus * MIN_COL_STEP;
	}

	lb_rect.width = view_rect.width;
	lb_rect.height = font.height;

	resize_framebuf(view_rect.width, view_rect.height);
}

int cpumon_height(int w)
{
	int h = 14 * w / 16;
	int min_h = font.height + 2 * BEVEL + 8;
	return h < min_h ? min_h : h;
}

void cpumon_update(void)
{
	int *cpucol;
	unsigned char *fb, *row;
	unsigned int *row32;
	unsigned short *row16;
	int col0, roffs, goffs, boffs;
	unsigned int i, j, row_offs, cur, usable_w, pixel;
	struct color *rgb;

	if(!img) return;

	cpucol = alloca(smon.num_cpus * sizeof *cpucol);

	for(i=0; i<smon.num_cpus; i++) {
		int usage = smon.cpu[i];
		if(usage >= 128) usage = 127;
		cpucol[i] = cpulut[usage];
	}

	if(sep_disp) {
		usable_w = rect.width - smon.num_cpus * (BEVEL * 2);
		colw = usable_w / smon.num_cpus / 2;
		if(colw < MIN_COLW) colw = MIN_COLW;
		col_step = rect.width / smon.num_cpus;
		if(colw > rect.height / 6) {
			colw = rect.height / 6;
		}
	}

	fb = (unsigned char*)img->pixels;

	row_offs = (img->height - 1) * img->pitch;

	/* scroll up */
	memmove(fb, fb + img->pitch, row_offs);

	/* draw the bottom line with the current stats */
	row = fb + row_offs;

	switch(img->bpp) {
	case 8:
		if(sep_disp) {
			for(i=0; i<smon.num_cpus; i++) {
				for(j=0; j<colw; j++) {
					row[j] = cpucol[i];
				}
				row += colw;
			}
		} else {
			for(i=0; i<img->width; i++) {
				cur = i * smon.num_cpus / img->width;
				col0 = cpucol[cur];
				*row++ = colors[col0];
			}
		}
		break;

	case 16:
		row16 = (unsigned short*)row;

		if(sep_disp) {
			for(i=0; i<smon.num_cpus; i++) {
				rgb = rgbcolors + cpucol[i];
				pixel = ((rgb->r << rshift) & img->rmask) |
					((rgb->g << gshift) & img->gmask) |
					((rgb->b << bshift) & img->bmask);

				for(j=0; j<colw; j++) {
					row16[j] = pixel;
				}
				row16 += colw;
			}
		} else {
			for(i=0; i<img->width; i++) {
				struct color *rgb;
				cur = i * smon.num_cpus / img->width;
				col0 = cpucol[cur];
				rgb = rgbcolors + col0;

				*row16++ = ((rgb->r << rshift) & img->rmask) |
					((rgb->g << gshift) & img->gmask) |
					((rgb->b << bshift) & img->bmask);
			}
		}
		break;

	case 24:
		roffs = rshift >> 3;
		goffs = gshift >> 3;
		boffs = bshift >> 3;

		if(sep_disp) {
			for(i=0; i<smon.num_cpus; i++) {
				col0 = cpucol[i];
				rgb = rgbcolors + col0;
				for(j=0; j<colw; j++) {
					row[roffs] = rgb->r;
					row[goffs] = rgb->g;
					row[boffs] = rgb->b;
					row += 3;
				}
			}
		} else {
			for(i=0; i<img->width; i++) {
				cur = i * smon.num_cpus / img->width;
				col0 = cpucol[cur];
				rgb = rgbcolors + col0;
				row[roffs] = rgb->r;
				row[goffs] = rgb->g;
				row[boffs] = rgb->b;
				row += 3;
			}
		}
		break;

	case 32:
		row32 = (unsigned int*)row;

		if(sep_disp) {
			for(i=0; i<smon.num_cpus; i++) {
				rgb = rgbcolors + cpucol[i];
				pixel = (rgb->r << rshift) | (rgb->g << gshift) | (rgb->b << bshift);

				for(j=0; j<colw; j++) {
					row32[j] = pixel;
				}
				row32 += colw;
			}
		} else {
			for(i=0; i<img->width; i++) {
				cur = i * smon.num_cpus / img->width;
				col0 = cpucol[cur];
				rgb = rgbcolors + col0;

				*row32++ = (rgb->r << rshift) | (rgb->g << gshift) | (rgb->b << bshift);
			}
		}
		break;

	default:
		break;
	}
}

void cpumon_draw(void)
{
	unsigned int i, total_width, x, y, sx;
	int baseline;
	char buf[128];

	set_color(uicolor[COL_BG]);
	draw_rect(lb_rect.x, lb_rect.y, lb_rect.width, lb_rect.height);

	baseline = lb_rect.y + lb_rect.height - font.descent;
	if(info_idx >= 0) {
		sprintf(buf, "CPU%d: %d%%", info_idx, smon.cpu[info_idx] * 100 >> 7);
		set_color(uicolor[COL_A]);
	} else {
		sprintf(buf, "CPU %d%%", smon.single * 100 >> 7);
		set_color(uicolor[COL_FG]);
	}
	draw_text(lb_rect.x, baseline, buf);

	if(!img) return;

	if(sep_disp) {
		total_width = (smon.num_cpus - 1) * col_step + colw;
		x = (rect.width - total_width) / 2;
		y = view_rect.y - BEVEL;

		sx = 0;
		for(i=0; i<smon.num_cpus; i++) {
			draw_frame(x, y, colw + BEVEL * 2, view_rect.height + BEVEL * 2, -BEVEL);
			blit_subimage(img, x + BEVEL, view_rect.y, sx, 0, colw, img->height);
			x += col_step;
			sx += colw;
		}

	} else {
		draw_frame(view_rect.x - BEVEL, view_rect.y - BEVEL, view_rect.width +
				BEVEL * 2, view_rect.height + BEVEL * 2, -BEVEL);
		blit_image(img, view_rect.x, view_rect.y);
	}
}

int cpumon_info(int show, int x, int y)
{
	int prev = info_idx;

	if(!show) {
		info_idx = -1;
	} else {
		if(!hittest(x, y, &view_rect)) {
			info_idx = -1;
		} else {
			info_idx = (x - view_rect.x) * smon.num_cpus / view_rect.width;
		}
	}

	if(info_idx != prev) {
		redisplay(UI_CPU);
	}
	return info_idx >= 0;
}

static int resize_framebuf(unsigned int width, unsigned int height)
{
	if(img && width == img->width && height == img->height) {
		return 0;
	}

	free_image(img);

	if(width <= 0 || height <= 0) {
		return -1;
	}

	if(!(img = create_image(width, height))) {
		return -1;
	}

	rshift = mask_to_shift(img->rmask);
	gshift = mask_to_shift(img->gmask);
	bshift = mask_to_shift(img->bmask);
	return 0;
}

static int mask_to_shift(unsigned int mask)
{
	int shift = 0;
	while(!(mask & 1)) {
		mask >>= 1;
		shift++;
	}
	while(mask & 1) {
		mask >>= 1;
		shift++;
	}

	return shift ? shift - 8 : 0;
}
