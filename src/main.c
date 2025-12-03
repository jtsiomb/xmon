#include <X11/X.h>
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
#include "options.h"

/* UI element bits */
enum {
	UI_FRAME	= 1,
	UI_CPU		= 2,

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

static Atom xa_wm_proto, xa_wm_del;
static Atom xa_mwm_hints;

static int win_x, win_y;
static int win_width, win_height;
static int frm_width;	/* total with bevels */
static int bevel;

static struct timeval tv0;


static XRectangle cpumon_rect;


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
	int i;

	init_opt();
	read_config("xmon.conf");
	if(parse_args(argc, argv) == -1) {
		return 1;
	}
	bevel = opt.vis.bevel_thick;
	frm_width = opt.vis.frm_width + bevel;

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
	xa_mwm_hints = XInternAtom(dpy, "_MOTIF_WM_HINTS", False);

	if(create_window() == -1) {
		return 1;
	}

	if(!(gc = XCreateGC(dpy, win, 0, 0))) {
		fprintf(stderr, "failed to allocate GC\n");
		return 1;
	}

	if(!(font = XLoadQueryFont(dpy, opt.vis.font))) {
		fprintf(stderr, "failed to load font: %s\n", opt.vis.font);
		return 1;
	}
	font_height = font->ascent + font->descent;
	XSetFont(dpy, gc, font->fid);

	for(i=0; i<NUM_UICOLORS; i++) {
		XAllocColor(dpy, cmap, opt.vis.uicolor + i);
	}
	XSetWindowBackground(dpy, win, opt.vis.uicolor[COL_BG].pixel);

	if(cpumon_init() == -1) {
		return 1;
	}

	layout();

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
			sysmon_update();

			cpumon_update();

			draw_window(UI_CPU);
		}
	}

	XCloseDisplay(dpy);
	return 0;
}

static void layout(void)
{
	cpumon_rect.x = frm_width;
	cpumon_rect.y = frm_width;
	if(cpumon_rect.y < frm_width) cpumon_rect.y = frm_width;
	cpumon_rect.width = win_width - frm_width * 2;
	cpumon_rect.height = win_height - frm_width * 2;
	if(cpumon_rect.height > cpumon_rect.width) {
		cpumon_rect.height = cpumon_rect.width;
	}

	cpumon_move(cpumon_rect.x, cpumon_rect.y);
	cpumon_resize(cpumon_rect.width, cpumon_rect.height);
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

	XFlush(dpy);
}

static void point(XPoint *p, int x, int y)
{
	p->x = x;
	p->y = y;
}

void draw_frame(int x, int y, int w, int h, int depth)
{
	int bevel;
	XPoint v[4];

	if(depth == 0) return;

	bevel = abs(depth);

	if(bevel == 1) {
		XSetLineAttributes(dpy, gc, bevel, LineSolid, CapButt, JoinBevel);

		point(v, x, y + h - 1);
		point(v + 1, x, y);
		point(v + 2, x + w - 1, y);

		XSetForeground(dpy, gc, opt.vis.uicolor[depth > 0 ? COL_BGHI : COL_BGLO].pixel);
		XDrawLines(dpy, win, gc, v, 3, CoordModeOrigin);

		point(v, x + w - 1, y);
		point(v + 1, x + w - 1, y + h - 1);
		point(v + 2, x, y + h - 1);

		XSetForeground(dpy, gc, opt.vis.uicolor[depth > 0 ? COL_BGLO : COL_BGHI].pixel);
		XDrawLines(dpy, win, gc, v, 3, CoordModeOrigin);
	} else {
		XSetForeground(dpy, gc, opt.vis.uicolor[depth > 0 ? COL_BGHI : COL_BGLO].pixel);

		point(v, x, y);
		point(v + 1, x + bevel, y + bevel);
		point(v + 2, x + bevel, y + h - bevel);
		point(v + 3, x, y + h);
		XFillPolygon(dpy, win, gc, v, 4, Convex, CoordModeOrigin);

		point(v, x, y);
		point(v + 1, x + w, y);
		point(v + 2, x + w - bevel, y + bevel);
		point(v + 3, x + bevel, y + bevel);
		XFillPolygon(dpy, win, gc, v, 4, Convex, CoordModeOrigin);

		XSetForeground(dpy, gc, opt.vis.uicolor[depth > 0 ? COL_BGLO : COL_BGHI].pixel);

		point(v, x + w, y);
		point(v + 1, x + w, y + h);
		point(v + 2, x + w - bevel, y + h - bevel);
		point(v + 3, x + w - bevel, y + bevel);
		XFillPolygon(dpy, win, gc, v, 4, Convex, CoordModeOrigin);

		point(v, x + w, y + h);
		point(v + 1, x, y + h);
		point(v + 2, x + bevel, y + h - bevel);
		point(v + 3, x + w - bevel, y + h - bevel);
		XFillPolygon(dpy, win, gc, v, 4, Convex, CoordModeOrigin);
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
	int i;
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

			case XK_grave:
				printf("CPU: %d%%\n", smon.single);
				for(i=0; i<smon.num_cpus; i++) {
					printf("  %d%%", smon.cpu[i]);
				}
				putchar('\n');
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
