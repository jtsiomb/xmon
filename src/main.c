#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/select.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include "sysmon.h"

#define NUM_COLORS	32
#define GRAD_COLORS	4

struct color {
	int r, g, b;
};

static Display *dpy;
static int scr;
static Window win, root;
static XVisualInfo *vinf;
static Atom xa_wm_proto, xa_wm_del;
static Colormap cmap;
static int quit;
static GC gc;
static XColor colors[NUM_COLORS];
static unsigned char *fb;
static XImage *ximg;
static int rshift, gshift, bshift;

static int win_width = 96, win_height = 96;

struct color grad[GRAD_COLORS] = {
	{0, 0x800, 0x4000},
	{0x8000, 0x800, 0x400},
	{0xffff, 0x8000, 0x400},
	{0xffff, 0xffff, 0xf000}
};

static int dbg_cmap;

static void fb_update(void);
static void draw_window(void);
static int create_window(const char *title, int width, int height);
static void proc_event(XEvent *ev);
static int resize_framebuf(int width, int height);
static int mask_to_shift(unsigned int mask);

int main(int argc, char **argv)
{
	int i;
	XEvent ev;

	if(sysmon_init() == -1) {
		return 1;
	}

	if(!(dpy = XOpenDisplay(0))) {
		fprintf(stderr, "failed to connect to the X server\n");
		return 1;
	}
	scr = DefaultScreen(dpy);
	root = RootWindow(dpy, scr);
	xa_wm_proto = XInternAtom(dpy, "WM_PROTOCOLS", False);
	xa_wm_del = XInternAtom(dpy, "WM_DELETE_WINDOW", False);

	if(create_window("xmon", win_width, win_height) == -1) {
		return 1;
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
		}
	}

	if(!(gc = XCreateGC(dpy, win, 0, 0))) {
		fprintf(stderr, "failed to allocate GC\n");
		return 1;
	}

	while(!quit) {
		fd_set rdset;
		int xfd = ConnectionNumber(dpy);
		struct timeval tv = {0, 300000};

		FD_ZERO(&rdset);
		FD_SET(xfd, &rdset);

		if(select(xfd + 1, &rdset, 0, 0, &tv) == -1) {
			if(errno == EINTR) continue;
			fprintf(stderr, "select failed: %s\n", strerror(errno));
			break;
		}

		if(FD_ISSET(xfd, &rdset)) {
			while(XPending(dpy)) {
				XNextEvent(dpy, &ev);
				proc_event(&ev);
			}
		}

		sysmon_update();
		fb_update();
		draw_window();
	}

	XCloseDisplay(dpy);
	return 0;
}

static void fb_update(void)
{
	int i, row_offs, cur, col0;
	unsigned char *row;
	unsigned int *row32;

	if(!ximg || ximg->width != win_width || ximg->height != win_height) {
		if(resize_framebuf(win_width, win_height) == -1) {
			fprintf(stderr, "failed to resize framebuffer to %dx%d\n", win_width, win_height);
			abort();
		}
	}

	row_offs = (win_height - 1) * ximg->bytes_per_line;

	/* scroll up */
	memmove(fb, fb + ximg->bytes_per_line, row_offs);

	/* draw the bottom line with the current stats */
	row = fb + row_offs;

	switch(ximg->bits_per_pixel) {
	case 8:
		for(i=0; i<win_width; i++) {
			cur = i * smon.num_cpus / win_width;
			col0 = (smon.cpu[cur] * NUM_COLORS) >> 7;
			*row++ = colors[col0].pixel;
		}
		break;

	case 32:
		row32 = (unsigned int*)row;
		for(i=0; i<win_width; i++) {
			int r, g, b;
			cur = i * smon.num_cpus / win_width;
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

static void draw_window(void)
{
	if(dbg_cmap) {
		int i, col;
		unsigned char *fbptr = fb;

		for(i=0; i<ximg->height; i++) {
			col = i * NUM_COLORS / ximg->height;
			memset(fbptr, colors[col].pixel, ximg->width);
			fbptr += ximg->width;
		}
	}

	XPutImage(dpy, win, gc, ximg, 0, 0, 0, 0, ximg->width, ximg->height);
}

static int create_window(const char *title, int width, int height)
{
	XSetWindowAttributes xattr;
	XTextProperty txprop;
	XWindowAttributes winattr;
	XVisualInfo vtmpl;
	int num_match;

	xattr.background_pixel = BlackPixel(dpy, scr);
	xattr.colormap = cmap = DefaultColormap(dpy, scr);

	if(!(win = XCreateWindow(dpy, root, 0, 0, width, height, 0, CopyFromParent,
					InputOutput, CopyFromParent, CWBackPixel | CWColormap, &xattr))) {
		fprintf(stderr, "failed to create window\n");
		return -1;
	}
	XSelectInput(dpy, win, StructureNotifyMask | ExposureMask | KeyPressMask);

	if(XStringListToTextProperty((char**)&title, 1, &txprop)) {
		XSetWMName(dpy, win, &txprop);
		XSetWMIconName(dpy, win, &txprop);
	}
	XSetWMProtocols(dpy, win, &xa_wm_del, 1);

	XGetWindowAttributes(dpy, win, &winattr);
	vtmpl.visualid = winattr.visual->visualid;
	vinf = XGetVisualInfo(dpy, VisualIDMask, &vtmpl, &num_match);

	XMapWindow(dpy, win);
	return 0;
}

static void proc_event(XEvent *ev)
{
	static int mapped;
	KeySym sym;

	switch(ev->type) {
	case Expose:
		if(!mapped || ev->xexpose.count > 0) {
			break;
		}
		draw_window();
		break;

	case MapNotify:
		mapped = 1;
		break;
	case UnmapNotify:
		mapped = 0;
		break;

	case KeyPress:
		if((sym = XLookupKeysym(&ev->xkey, 0)) != NoSymbol) {
			switch(sym) {
			case XK_Escape:
				quit = 1;
				break;

			case XK_c:
				dbg_cmap ^= 1;
				break;

			default:
				break;
			}
		}
		break;

	case ClientMessage:
		if(ev->xclient.message_type == xa_wm_proto) {
			if(ev->xclient.data.l[0] == xa_wm_del) {
				quit = 1;
			}
		}
		break;

	case ConfigureNotify:
		win_width = ev->xconfigure.width;
		win_height = ev->xconfigure.height;
		break;

	default:
		break;
	}
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
