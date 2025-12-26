#ifndef OPTIONS_H_
#define OPTIONS_H_

#include "xmon.h"

enum {
	MON_CPU		= 0x0001,
	MON_MEM		= 0x0002,
	MON_LOAD	= 0x0004,
	MON_NET		= 0x0008,

	MON_ALL		= MON_CPU | MON_MEM | MON_LOAD | MON_NET
};

struct vis_options {
	struct color uicolor[NUM_UICOLORS];
	const char *font;
	int frm_width;
	int decor, bevel_thick;
	/* TODO skin */
};

struct cpu_options {
	int ncolors;
	int autosplit;
};

struct net_options {
	const char *ifname;
};

struct options {
	int x, y, xsz, ysz;
	int upd_interv;

	unsigned int mon;

	int verbose;

	struct vis_options vis;
	struct cpu_options cpu;
	struct net_options net;
};

extern struct options opt;

void init_opt(void);

int parse_args(int argc, char **argv);
int read_config(void);

#define BEVEL	opt.vis.bevel_thick

#endif	/* OPTIONS_H_ */
