#include <stdio.h>
#include <stdlib.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#define NUM_COLORS	32

static Display *dpy;
static int scr;
static Window win, root;
static Atom xa_wm_proto, xa_wm_del;
static Colormap cmap;
static int quit;
static GC gc;
static XColor colors[NUM_COLORS];

static int win_width = 128, win_height = 128;

static int col0[] = {0x8000, 0x800, 0x400};
static int col1[] = {0xffff, 0x8000, 0x400};

static int create_window(const char *title, int width, int height);
static void proc_event(XEvent *ev);

int main(int argc, char **argv)
{
	int i;
	XEvent ev;

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
		colors[i].red = col0[0] + (col1[0] - col0[0]) * i / (NUM_COLORS - 1);
		colors[i].green = col0[1] + (col1[1] - col0[1]) * i / (NUM_COLORS - 1);
		colors[i].blue = col0[2] + (col1[2] - col0[2]) * i / (NUM_COLORS - 1);
		if(!XAllocColor(dpy, cmap, colors + i)) {
			fprintf(stderr, "failed to allocate color %d\n", i);
		}
	}

	if(!(gc = XCreateGC(dpy, win, 0, 0))) {
		fprintf(stderr, "failed to allocate GC\n");
		return 1;
	}

	while(!quit) {
		XNextEvent(dpy, &ev);
		proc_event(&ev);
	}

	XCloseDisplay(dpy);
	return 0;
}


static int create_window(const char *title, int width, int height)
{
	XSetWindowAttributes xattr;
	XTextProperty txprop;

	xattr.background_pixel = BlackPixel(dpy, scr);
	xattr.colormap = cmap = DefaultColormap(dpy, scr);

	if(!(win = XCreateWindow(dpy, root, 0, 0, 128, 128, 0, CopyFromParent, InputOutput,
					CopyFromParent, CWBackPixel | CWColormap, &xattr))) {
		fprintf(stderr, "failed to create window\n");
		return -1;
	}
	XSelectInput(dpy, win, StructureNotifyMask | ExposureMask | KeyPressMask);

	if(XStringListToTextProperty((char**)&title, 1, &txprop)) {
		XSetWMName(dpy, win, &txprop);
		XSetWMIconName(dpy, win, &txprop);
	}
	XSetWMProtocols(dpy, win, &xa_wm_del, 1);

	XMapWindow(dpy, win);

	return 0;
}

static void proc_event(XEvent *ev)
{
	static int mapped;
	int i;
	KeySym sym;

	switch(ev->type) {
	case Expose:
		if(!mapped) break;
		for(i=0; i<win_height; i++) {
			int cidx = i * NUM_COLORS / win_height;
			XSetForeground(dpy, gc, colors[cidx].pixel);
			XDrawRectangle(dpy, win, gc, 0, i, win_width, 1);
		}
		break;

	case MapNotify:
		mapped = 1;
		break;
	case UnmapNotify:
		mapped = 0;
		break;

	case KeyPress:
		if((sym = XLookupKeysym(&ev->xkey, 0)) != NoSymbol) {
			if(sym == XK_Escape) {
				quit = 1;
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
