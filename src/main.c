#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/select.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include "xmon.h"
#include "sysmon.h"

#define UPD_RATE_MS	350
#define FONT_DESC	"-*-helvetica-medium-r-*-*-12-*"

enum {
	COL_FG,
	COL_BG,
	COL_BGHI,
	COL_BGLO,

	NUM_UICOLORS
};

static Atom xa_wm_proto, xa_wm_del;

static int win_width = 96, win_height = 96;
static int frm_width = 8;

static struct timeval tv0;

static XColor uicolor[NUM_UICOLORS] = {
	{0, 0xffff, 0xffff, 0xffff},
	{0, 0x6000, 0x6000, 0x6000},
	{0, 0x8000, 0x8000, 0x8000},
	{0, 0x4000, 0x4000, 0x4000}
};

static XRectangle cpumon_rect;

static void layout(void);
static void draw_window(void);
static void draw_frame(int x, int y, int w, int h, int depth);
static int create_window(const char *title, int width, int height);
static void proc_event(XEvent *ev);
static long get_msec(void);


int main(int argc, char **argv)
{
	XEvent ev;
	struct timeval tv;
	long prev_upd, msec, dt;
	int i;

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

	if(!(gc = XCreateGC(dpy, win, 0, 0))) {
		fprintf(stderr, "failed to allocate GC\n");
		return 1;
	}

	if(!(font = XLoadQueryFont(dpy, FONT_DESC))) {
		fprintf(stderr, "failed to load font: %s\n", FONT_DESC);
		return 1;
	}
	XSetFont(dpy, gc, font->fid);

	for(i=0; i<NUM_UICOLORS; i++) {
		XAllocColor(dpy, cmap, uicolor + i);
		printf("ui color %06lx: %3u %3u %3u\n", uicolor[i].pixel,
				uicolor[i].red >> 8, uicolor[i].green >> 8, uicolor[i].blue >> 8);
	}
	XSetWindowBackground(dpy, win, uicolor[COL_BG].pixel);

	if(cpumon_init() == -1) {
		return 1;
	}

	layout();

	XMapWindow(dpy, win);
	XFlush(dpy);

	gettimeofday(&tv0, 0);

	while(!quit) {
		fd_set rdset;
		int xfd;

		msec = get_msec();
		dt = msec - prev_upd;

		FD_ZERO(&rdset);
		xfd = ConnectionNumber(dpy);
		FD_SET(xfd, &rdset);

		if(dt >= UPD_RATE_MS) {
			tv.tv_sec = tv.tv_usec = 0;
		} else {
			long delay = UPD_RATE_MS - dt;
			tv.tv_sec = delay / 1000;
			tv.tv_usec = (delay % 1000) * 1000;
		}

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

		msec = get_msec();
		if(msec - prev_upd >= UPD_RATE_MS) {
			prev_upd = msec;
			sysmon_update();

			cpumon_update();

			draw_window();
		}
	}

	XCloseDisplay(dpy);
	return 0;
}

static void layout(void)
{
	cpumon_rect.x = frm_width;
	cpumon_rect.y = frm_width;
	cpumon_rect.width = win_width - frm_width * 2;
	if(win_height >= win_width) {
		cpumon_rect.height = cpumon_rect.width;
	} else {
		cpumon_rect.height = win_height - frm_width * 2;
	}

	cpumon_move(cpumon_rect.x, cpumon_rect.y);
	cpumon_resize(cpumon_rect.width, cpumon_rect.height);
}

static void draw_window(void)
{
	int bevel = 1;

	draw_frame(0, 0, win_width, win_height, bevel);
	draw_frame(cpumon_rect.x - bevel, cpumon_rect.y - bevel,
			cpumon_rect.width + bevel * 2, cpumon_rect.height + bevel * 2,
			-bevel);

	cpumon_draw();

	XFlush(dpy);
}

static void draw_frame(int x, int y, int w, int h, int depth)
{
	int bevel;
	XPoint v[3];

	if(depth == 0) return;

	bevel = abs(depth);

	if(bevel == 1) {
		XSetLineAttributes(dpy, gc, bevel, LineSolid, CapButt, JoinBevel);

		v[0].x = x; v[0].y = y + h - 1;
		v[1].x = x; v[1].y = y;
		v[2].x = x + w - 1; v[2].y = y;

		XSetForeground(dpy, gc, uicolor[depth > 0 ? COL_BGHI : COL_BGLO].pixel);
		XDrawLines(dpy, win, gc, v, 3, CoordModeOrigin);

		v[0].x = x + w - 1; v[0].y = y;
		v[1].x = x + w - 1; v[1].y = y + h - 1;
		v[2].x = x; v[2].y = y + h - 1;

		XSetForeground(dpy, gc, uicolor[depth > 0 ? COL_BGLO : COL_BGHI].pixel);
		XDrawLines(dpy, win, gc, v, 3, CoordModeOrigin);
	}
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
		if(ev->xconfigure.width != win_width || ev->xconfigure.height != win_height) {
			printf("configure notify: %dx%d\n", ev->xconfigure.width, ev->xconfigure.height);
			win_width = ev->xconfigure.width;
			win_height = ev->xconfigure.height;
			layout();
		}
		break;

	default:
		break;
	}
}

static long get_msec(void)
{
	struct timeval tv;

	gettimeofday(&tv, 0);
	return (tv.tv_sec - tv0.tv_sec) * 1000 + (tv.tv_usec - tv0.tv_usec) / 1000;
}
