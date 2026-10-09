/*
xmon - a minimal lightweight cross-platform system monitoring utility
Copyright (C) 2025-2026  John Tsiombikas <nuclear@mutantstargoat.com>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/
#ifndef OPTIONS_H_
#define OPTIONS_H_

#include "xmon.h"

enum {
	MON_CPU		= 0x0001,
	MON_MEM		= 0x0002,
	MON_LOAD	= 0x0004,
	MON_NET		= 0x0008,
	MON_BAT		= 0x0010,

	MON_ALL		= MON_CPU | MON_MEM | MON_LOAD | MON_NET | MON_BAT
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
