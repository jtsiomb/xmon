#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <windows.h>
#include "disp.h"
#include "xmon.h"
#include "options.h"

struct image_data {
	int foo;
};


int quit;

int win_x, win_y, win_width, win_height;

struct font font;

static HWND win;


int init_disp(void)
{
	return 0;
}

void shutdown_disp(void)
{
}


int proc_events(long delay)
{
	return 0;
}

void resize_window(int x, int y)
{
}

void map_window(void)
{
	ShowWindow(win, 1);
}

unsigned int alloc_color(unsigned int r, unsigned int g, unsigned int b)
{
	return 0;
}

void set_color(unsigned int color)
{
}

void set_background(unsigned int color)
{
}

void clear_window(void)
{
}

void draw_rect(int x, int y, int width, int height)
{
}

void draw_rects(struct rect *rects, int count)
{
}

void draw_poly(struct point *v, int count)
{
}

void draw_text(int x, int y, const char *str)
{
}

void end_drawing(void)
{
	ValidateRect(win, 0);
}

struct image *create_image(unsigned int width, unsigned int height)
{
	return 0;
}

void free_image(struct image *img)
{
}

void blit_image(struct image *img, int x, int y)
{
}

void blit_subimage(struct image *img, int dx, int dy, int sx, int sy,
		unsigned int width, unsigned int height)
{
}
