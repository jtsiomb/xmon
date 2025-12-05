#ifndef WIDGET_H_
#define WIDGET_H_

#include <X11/Xlib.h>

/* UI colors */
enum {
	COL_FG,
	COL_BG,
	COL_BGHI,
	COL_BGLO,

	NUM_UICOLORS
};

extern XFontStruct *font;
extern int font_height;
extern int bar_height;

int init_widgets(void);

void draw_frame(int x, int y, int w, int h, int depth);
void draw_bar(int x, int y, int w, int val, int total);

#endif	/* WIDGET_H_ */
