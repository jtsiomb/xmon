#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/select.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/Xutil.h>
#include "xmon.h"
#include "options.h"

/* UI element bits */
enum {
	UI_FRAME	= 0x0001,
	UI_CPU		= 0x0002,
	UI_MEM		= 0x0004,
	UI_LOAD		= 0x0008,

	UI_ALL		= 0x7fff
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

struct sysmon smon;

Display *dpy;
int scr;
Window win, root;
XVisualInfo *vinf;
Colormap cmap;
GC gc;

#ifndef NO_XSHM
XShmSegmentInfo xshm;
int have_xshm;
#endif

int quit;

static Atom xa_wm_proto, xa_wm_del;
static Atom xa_mwm_hints;

static int win_x, win_y;
static int win_width, win_height;
static int frm_width;	/* total with bevels */
static int bevel;

static struct timeval tv0;


static XRectangle minrect;
static XRectangle cpu_rect, mem_rect, load_rect;


static void layout(void);
static void draw_window(unsigned int draw);
static int create_window(void);
static void proc_event(XEvent *ev);
static long get_msec(void);


int main(int argc, char **argv)
{
	XEvent ev;
	struct timeval tv;
	long prev_upd, msec, dt;

	init_opt();
	read_config("xmon.conf");
	if(parse_args(argc, argv) == -1) {
		return 1;
	}
	bevel = opt.vis.bevel_thick;
	frm_width = opt.vis.frm_width + bevel;

	if(cpu_init() == -1) {
		return 1;
	}
	if(mem_init() == -1) {
		return 1;
	}
	if(load_init() == -1) {
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
	xa_mwm_hints = XInternAtom(dpy, "_MOTIF_WM_HINTS", False);

#ifndef NO_XSHM
	have_xshm = XShmQueryExtension(dpy);
#endif

	if(create_window() == -1) {
		return 1;
	}

	if(!(gc = XCreateGC(dpy, win, 0, 0))) {
		fprintf(stderr, "failed to allocate GC\n");
		return 1;
	}

	if(init_widgets() == -1) {
		return 1;
	}
	if(cpumon_init() == -1) {
		return 1;
	}

	XSetWindowBackground(dpy, win, opt.vis.uicolor[COL_BG].pixel);

	layout();
	XResizeWindow(dpy, win, minrect.width, minrect.height);

	XMapWindow(dpy, win);
	XFlush(dpy);

	prev_upd = -opt.upd_interv;
	gettimeofday(&tv0, 0);

	while(!quit) {
		fd_set rdset;
		int xfd;

		msec = get_msec();
		dt = msec - prev_upd;

		FD_ZERO(&rdset);
		xfd = ConnectionNumber(dpy);
		FD_SET(xfd, &rdset);

		if(dt >= opt.upd_interv) {
			tv.tv_sec = tv.tv_usec = 0;
		} else {
			long delay = opt.upd_interv - dt;
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
		if(msec - prev_upd >= opt.upd_interv) {
			prev_upd = msec;

			cpu_update();
			mem_update();
			load_update();

			cpumon_update();

			draw_window(UI_CPU | UI_MEM | UI_LOAD);
		}
	}

	XCloseDisplay(dpy);
	return 0;
}

static void layout(void)
{
	int y;
	int all_x = frm_width;
	int all_width = win_width - frm_width * 2;

	y = frm_width;

	/* CPU monitor */
	cpu_rect.x = frm_width;
	cpu_rect.y = y;
	cpu_rect.width = all_width;
	cpu_rect.height = cpumon_height(all_width);

	cpumon_move(cpu_rect.x, cpu_rect.y);
	cpumon_resize(cpu_rect.width, cpu_rect.height);

	y += cpu_rect.height + frm_width;

	/* load average */
	load_rect.x = frm_width;
	load_rect.y = y;
	load_rect.width = all_width;
	load_rect.height = loadmon_height(all_width);

	loadmon_move(load_rect.x, load_rect.y);
	loadmon_resize(load_rect.width, load_rect.height);

	y += load_rect.height + frm_width;

	/* memory monitor */
	mem_rect.x = all_x;
	mem_rect.y = y;
	mem_rect.width = all_width;
	mem_rect.height = all_width / 2;

	memmon_move(mem_rect.x, mem_rect.y);
	memmon_resize(mem_rect.width, mem_rect.height);

	y += mem_rect.height + frm_width;

	minrect.width = win_width;
	minrect.height = y;
}

static void draw_window(unsigned int draw)
{
	if(draw & UI_FRAME) {
		XClearWindow(dpy, win);
		if(!opt.vis.decor) {
			draw_frame(0, 0, win_width, win_height, bevel);
		}
	}

	if(draw & UI_CPU) {
		cpumon_draw();
	}

	if(draw & UI_LOAD) {
		loadmon_draw();
	}

	if(draw & UI_MEM) {
		memmon_draw();
	}

	XFlush(dpy);
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
		draw_window(UI_ALL);
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

static long get_msec(void)
{
	struct timeval tv;

	gettimeofday(&tv, 0);
	return (tv.tv_sec - tv0.tv_sec) * 1000 + (tv.tv_usec - tv0.tv_usec) / 1000;
}
