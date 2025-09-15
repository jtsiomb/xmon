#ifndef OPTIONS_H_
#define OPTIONS_H_

#include <X11/Xlib.h>
#include "xmon.h"

enum {
	MON_CPU		= 1,
	MON_MEM		= 2,
	MON_LOAD	= 4
};
#define MON_ALL	0xffff

struct vis_options {
	XColor uicolor[NUM_UICOLORS];
	const char *font;
	int frm_width;
	int decor, bevel_thick;
	/* TODO skin */
};

struct cpu_options {
	int ncolors;
};

struct options {
	int x, y, xsz, ysz;
	int upd_interv;

	unsigned int mon;

	struct vis_options vis;
	struct cpu_options cpu;
};

extern struct options opt;

void init_opt(void);

int parse_args(int argc, char **argv);
int read_config(const char *fname);

#define BEVEL	opt.vis.bevel_thick

#endif	/* OPTIONS_H_ */
