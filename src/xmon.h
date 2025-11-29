#ifndef XMON_H_
#define XMON_H_

#include <X11/Xlib.h>
#include <X11/Xutil.h>

Display *dpy;
int scr;
Window win, root;
XVisualInfo *vinf;
Colormap cmap;
GC gc;
XFontStruct *font;
int font_height;

int quit;

int cpumon_init(void);
void cpumon_destroy(void);
void cpumon_move(int x, int y);
void cpumon_resize(int x, int y);
void cpumon_update(void);
void cpumon_draw(void);

#endif	/* XMON_H_ */
