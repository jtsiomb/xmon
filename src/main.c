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
	UI_NET		= 0x0010,

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
static unsigned int ui_active_widgets;
static unsigned int dirty;

static struct timeval tv0;


static XRectangle minrect;
static XRectangle cpu_rect, mem_rect, load_rect, net_rect;


static void layout(void);
static void draw_window(void);
static int create_window(void);
static void proc_event(XEvent *ev);

long get_msec(void);


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

	if((opt.mon & MON_CPU) && cpu_init() == -1) {
		fprintf(stderr, "disabling CPU usage display\n");
		opt.mon &= ~MON_CPU;
	}
	if((opt.mon & MON_MEM) && mem_init() == -1) {
		fprintf(stderr, "disabling memory usage display\n");
		opt.mon &= ~MON_MEM;
	}
	if((opt.mon & MON_LOAD) && load_init() == -1) {
		fprintf(stderr, "disabling load average display\n");
		opt.mon &= ~MON_LOAD;
	}
	if((opt.mon & MON_NET) && net_init() == -1) {
		fprintf(stderr, "disabling network traffic display\n");
		opt.mon &= ~MON_NET;
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
	if((opt.mon & MON_CPU) && cpumon_init() == -1) {
		return 1;
	}
	if((opt.mon & MON_LOAD) && loadmon_init() == -1) {
		return 1;
	}
	if((opt.mon & MON_NET) && netmon_init() == -1) {
		return 1;
	}

	/* compute bitmask to redraw all enabled widgets */
	if(opt.mon & MON_CPU) ui_active_widgets |= UI_CPU;
	if(opt.mon & MON_MEM) ui_active_widgets |= UI_MEM;
	if(opt.mon & MON_LOAD) ui_active_widgets |= UI_LOAD;
	if(opt.mon & MON_NET) ui_active_widgets |= UI_NET;

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

			if(opt.mon & MON_CPU) {
				cpu_update();
				cpumon_update();
			}
			if(opt.mon & MON_MEM) {
				mem_update();
			}
			if(opt.mon & MON_LOAD) {
				load_update();
			}
			if(opt.mon & MON_NET) {
				net_update();
			}

			dirty |= ui_active_widgets;
			draw_window();
		}
	}

	XCloseDisplay(dpy);
	return 0;
}

#define ALL_X		frm_width
#define ALL_WIDTH	(win_width - frm_width * 2)
#define SEPSZ		(frm_width * 2)

static void layout(void)
{
	int y;

	y = frm_width;

	/* CPU monitor */
	if(opt.mon & MON_CPU) {
		cpu_rect.x = frm_width;
		cpu_rect.y = y;
		cpu_rect.width = ALL_WIDTH;
		cpu_rect.height = cpumon_height(ALL_WIDTH);

		cpumon_move(cpu_rect.x, cpu_rect.y);
		cpumon_resize(cpu_rect.width, cpu_rect.height);

		y += cpu_rect.height + SEPSZ;
	}

	/* load average */
	if(opt.mon & MON_LOAD) {
		load_rect.x = frm_width;
		load_rect.y = y;
		load_rect.width = ALL_WIDTH;
		load_rect.height = loadmon_height(ALL_WIDTH);

		loadmon_move(load_rect.x, load_rect.y);
		loadmon_resize(load_rect.width, load_rect.height);

		y += load_rect.height + SEPSZ;
	}

	/* memory monitor */
	if(opt.mon & MON_MEM) {
		mem_rect.x = ALL_X;
		mem_rect.y = y;
		mem_rect.width = ALL_WIDTH;
		mem_rect.height = memmon_height(ALL_WIDTH);

		memmon_move(mem_rect.x, mem_rect.y);
		memmon_resize(mem_rect.width, mem_rect.height);

		y += mem_rect.height + SEPSZ;
	}

	/* network monitor */
	if(opt.mon & MON_NET) {
		net_rect.x = ALL_X;
		net_rect.y = y;
		net_rect.width = ALL_WIDTH;
		net_rect.height = netmon_height(ALL_WIDTH);

		netmon_move(net_rect.x, net_rect.y);
		netmon_resize(net_rect.width, net_rect.height);

		y += net_rect.height + SEPSZ;
	}

	minrect.width = win_width;
	minrect.height = y - SEPSZ + frm_width;

	dirty = UI_FRAME | ui_active_widgets;
}

#define DRAWSEP(y)	draw_sep(0, (y) - SEPSZ / 2, win_width)

static void draw_window(void)
{
	int n = 0;

	if(dirty & UI_FRAME) {
		XClearWindow(dpy, win);
		if(!opt.vis.decor) {
			draw_frame(0, 0, win_width, win_height, bevel);
		}
	}

	if(dirty & UI_CPU) {
		cpumon_draw();
		n++;
	}

	if(dirty & UI_LOAD) {
		if(n++) DRAWSEP(load_rect.y);
		loadmon_draw();
	}

	if(dirty & UI_MEM) {
		if(n++) DRAWSEP(mem_rect.y);
		memmon_draw();
	}

	if(dirty & UI_NET) {
		if(n++) DRAWSEP(net_rect.y);
		netmon_draw();
	}

	dirty = 0;
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
		dirty = UI_FRAME | ui_active_widgets;
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

long get_msec(void)
{
	struct timeval tv;

	gettimeofday(&tv, 0);
	return (tv.tv_sec - tv0.tv_sec) * 1000 + (tv.tv_usec - tv0.tv_usec) / 1000;
}
