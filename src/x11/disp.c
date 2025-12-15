#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/select.h>
#include <sys/time.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#ifndef NO_XSHM
#include <X11/extensions/XShm.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#endif
#include "disp.h"
#include "xmon.h"
#include "options.h"

struct image_data {
	XImage *ximg;
#ifndef NO_XSHM
	XShmSegmentInfo xshm;
#endif
};


typedef struct {
	unsigned long flags;
	unsigned long functions;
	unsigned long decorations;
	long input_mode;
	unsigned long status;
} MotifWmHints;

#define MWM_HINTS_DECORATIONS	0x02
#define MWM_DECOR_BORDER		0x02


int quit;

int win_x, win_y, win_width, win_height;

struct font font;

static Display *dpy;
static int scr;
static Window win, root;
static XVisualInfo *vinf;
static Colormap cmap;
static GC gc;

#ifndef NO_XSHM
static int have_xshm;
#endif

static Atom xa_wm_proto, xa_wm_del;
static Atom xa_mwm_hints;

static long prev_upd;


static int create_window(void);
static void proc_event(XEvent *ev);


int init_disp(void)
{
	XFontStruct *xfont;

	if(!(dpy = XOpenDisplay(0))) {
		fprintf(stderr, "failed to connect to the X server\n");
		return 1;
	}
	scr = DefaultScreen(dpy);
	root = RootWindow(dpy, scr);
	xa_wm_proto = XInternAtom(dpy, "WM_PROTOCOLS", False);
	xa_wm_del = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
	xa_mwm_hints = XInternAtom(dpy, "_MOTIF_WM_HINTS", False);

#ifndef NO_XSHM
	have_xshm = XShmQueryExtension(dpy);
#endif

	if(create_window() == -1) {
		return -1;
	}

	if(!(gc = XCreateGC(dpy, win, 0, 0))) {
		fprintf(stderr, "failed to allocate GC\n");
		return -1;
	}

	if(!(xfont = XLoadQueryFont(dpy, opt.vis.font))) {
		fprintf(stderr, "failed to load font: %s\n", opt.vis.font);
		return -1;
	}

	font.data = xfont;
	font.height = xfont->ascent + xfont->descent;
	font.ascent = xfont->ascent;
	font.descent = xfont->descent;

	XSetFont(dpy, gc, xfont->fid);

	prev_upd = -opt.upd_interv;
	return 0;
}

void shutdown_disp(void)
{
	XCloseDisplay(dpy);
}


int proc_events(long delay)
{
	fd_set rdset;
	int xfd;
	struct timeval tv;
	XEvent ev;

	FD_ZERO(&rdset);
	xfd = ConnectionNumber(dpy);
	FD_SET(xfd, &rdset);

	if(delay <= 0) {
		tv.tv_sec = tv.tv_usec = 0;
	} else {
		tv.tv_sec = delay / 1000;
		tv.tv_usec = (delay % 1000) * 1000;
	}

	if(select(xfd + 1, &rdset, 0, 0, &tv) == -1) {
		if(errno == EINTR) return 0;
		fprintf(stderr, "select failed: %s\n", strerror(errno));
		return -1;
	}

	if(FD_ISSET(xfd, &rdset)) {
		while(XPending(dpy)) {
			XNextEvent(dpy, &ev);
			proc_event(&ev);
		}
	}
	return 0;
}

void resize_window(int x, int y)
{
	XResizeWindow(dpy, win, x, y);
}

void map_window(void)
{
	XMapWindow(dpy, win);
	XFlush(dpy);
}

unsigned int alloc_color(unsigned int r, unsigned int g, unsigned int b)
{
	XColor col;
	col.red = r | (r << 8);
	col.green = g | (g << 8);
	col.blue = b | (b << 8);
	if(XAllocColor(dpy, cmap, &col)) {
		return col.pixel;
	}
	return 0;
}

void set_color(unsigned int color)
{
	XSetForeground(dpy, gc, color);
}

void set_background(unsigned int color)
{
	XSetWindowBackground(dpy, win, color);
}

void clear_window(void)
{
	XClearWindow(dpy, win);
}

void draw_rect(int x, int y, int width, int height)
{
	XFillRectangle(dpy, win, gc, x, y, width, height);
}

void draw_rects(struct rect *rects, int count)
{
	XFillRectangles(dpy, win, gc, (XRectangle*)rects, count);
}

void draw_poly(struct point *v, int count)
{
	XFillPolygon(dpy, win, gc, (XPoint*)v, count, Convex, CoordModeOrigin);
}

void draw_text(int x, int y, const char *str)
{
	XDrawString(dpy, win, gc, x, y, str, strlen(str));
}

void end_drawing(void)
{
	XFlush(dpy);
}

struct image *create_image(unsigned int width, unsigned int height)
{
	struct image *img;
	struct image_data *imgdata;
	XImage *ximg;

	if(!(img = malloc(sizeof *img + sizeof(struct image_data)))) {
		fprintf(stderr, "failed to allocate image\n");
		return 0;
	}
	img->data = img + 1;
	imgdata = img->data;

#ifndef NO_XSHM
	if(have_xshm) {
		XShmSegmentInfo *xshm = &imgdata->xshm;

		if(!(ximg = XShmCreateImage(dpy, vinf->visual, vinf->depth, ZPixmap, 0,
						xshm, width, height))) {
			return 0;
		}
		if((xshm->shmid = shmget(IPC_PRIVATE, ximg->bytes_per_line * ximg->height,
				IPC_CREAT | 0777)) == -1) {
			fprintf(stderr, "failed to create shared memory, fallback to no XShm\n");
			free_image(img);
			goto no_xshm;
		}
		if((xshm->shmaddr = ximg->data = shmat(xshm->shmid, 0, 0)) == (void*)-1) {
			fprintf(stderr, "failed to attach shared memory, fallback to no XShm\n");
			free_image(img);
			goto no_xshm;
		}
		xshm->readOnly = True;
		if(!XShmAttach(dpy, xshm)) {
			fprintf(stderr, "XShmAttach failed\n");
			free_image(img);
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
			return 0;
		}

		if(!(ximg->data = calloc(1, height * ximg->bytes_per_line))) {
			XDestroyImage(ximg);
			ximg = 0;
			return 0;
		}
	}

	img->width = ximg->width;
	img->height = ximg->height;
	img->bpp = ximg->bits_per_pixel;
	img->pitch = ximg->bytes_per_line;
	img->pixels = ximg->data;
	img->rmask = ximg->red_mask;
	img->gmask = ximg->green_mask;
	img->bmask = ximg->blue_mask;
	imgdata->ximg = ximg;
	return img;
}

void free_image(struct image *img)
{
	struct image_data *imgdata;

	if(!img) return;

	imgdata = img->data;

#ifndef NO_XSHM
	if(have_xshm) {
		XShmDetach(dpy, &imgdata->xshm);
		if(imgdata->xshm.shmaddr != (void*)-1) {
			shmdt(imgdata->xshm.shmaddr);
		}
		if(imgdata->xshm.shmid != -1) {
			shmctl(imgdata->xshm.shmid, IPC_RMID, 0);
			imgdata->xshm.shmid = -1;
		}
		XDestroyImage(imgdata->ximg);
	} else
#endif
	{
		XDestroyImage(imgdata->ximg);
	}
}

void blit_image(struct image *img, int x, int y)
{
	struct image_data *imgdata = img->data;
	XImage *ximg = imgdata->ximg;

#ifndef NO_XSHM
	if(have_xshm) {
		XShmPutImage(dpy, win, gc, ximg, 0, 0, x, y, ximg->width, ximg->height, False);
	} else
#endif
	{
		XPutImage(dpy, win, gc, ximg, 0, 0, x, y, ximg->width, ximg->height);
	}
}

void blit_subimage(struct image *img, int dx, int dy, int sx, int sy,
		unsigned int width, unsigned int height)
{
	struct image_data *imgdata = img->data;
	XImage *ximg = imgdata->ximg;

#ifndef NO_XSHM
	if(have_xshm) {
		XShmPutImage(dpy, win, gc, ximg, sx, sy, dx, dy, width, height, False);
	} else
#endif
	{
		XPutImage(dpy, win, gc, ximg, sx, sy, dx, dy, width, height);
	}
}

static int create_window(void)
{
	XSetWindowAttributes xattr;
	XTextProperty txprop;
	XWindowAttributes winattr;
	XVisualInfo vtmpl;
	int num_match;
	MotifWmHints mwmhints = {0};
	static const char *title = "xmon";

	xattr.background_pixel = BlackPixel(dpy, scr);
	xattr.colormap = cmap = DefaultColormap(dpy, scr);

	if(!(win = XCreateWindow(dpy, root, opt.x, opt.y, opt.xsz, opt.ysz, 0,
					CopyFromParent, InputOutput, CopyFromParent,
					CWBackPixel | CWColormap, &xattr))) {
		fprintf(stderr, "failed to create window\n");
		return -1;
	}
	XSelectInput(dpy, win, StructureNotifyMask | ExposureMask | KeyPressMask |
			ButtonPressMask | Button1MotionMask);

	if(XStringListToTextProperty((char**)&title, 1, &txprop)) {
		XSetWMName(dpy, win, &txprop);
		XSetWMIconName(dpy, win, &txprop);
	}
	XSetWMProtocols(dpy, win, &xa_wm_del, 1);

	if(!opt.vis.decor) {
		/* ask for an undecorated window */
		mwmhints.flags = MWM_HINTS_DECORATIONS;
		mwmhints.decorations = 0;
		XChangeProperty(dpy, win, xa_mwm_hints, xa_mwm_hints, 32, PropModeReplace,
				(unsigned char*)&mwmhints, sizeof mwmhints / sizeof(long));
	}

	XGetWindowAttributes(dpy, win, &winattr);
	win_x = winattr.x;
	win_y = winattr.y;
	win_width = winattr.width;
	win_height = winattr.height;

	vtmpl.visualid = winattr.visual->visualid;
	vinf = XGetVisualInfo(dpy, VisualIDMask, &vtmpl, &num_match);
	return 0;
}

static void proc_event(XEvent *ev)
{
	static int mapped, prev_x, prev_y;
	KeySym sym;

	switch(ev->type) {
	case Expose:
		if(!mapped || ev->xexpose.count > 0) {
			break;
		}
		draw_window(0xffff);
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

			default:
				break;
			}
		}
		break;

	case ButtonPress:
		if(ev->xbutton.button == Button1) {
			prev_x = ev->xbutton.x_root;
			prev_y = ev->xbutton.y_root;
		}
		break;

	case MotionNotify:
		if(ev->xmotion.state & Button1MotionMask) {
			int dx = ev->xmotion.x_root - prev_x;
			int dy = ev->xmotion.y_root - prev_y;
			prev_x = ev->xmotion.x_root;
			prev_y = ev->xmotion.y_root;
			win_x += dx;
			win_y += dy;
			XMoveWindow(dpy, win, win_x, win_y);
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
		win_x = ev->xconfigure.x;
		win_y = ev->xconfigure.y;
		if(ev->xconfigure.width != win_width || ev->xconfigure.height != win_height) {
			win_width = ev->xconfigure.width;
			win_height = ev->xconfigure.height;
			layout();
		}
		break;

	default:
		break;
	}
}
