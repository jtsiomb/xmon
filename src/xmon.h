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
#ifndef XMON_H_
#define XMON_H_

#if defined(unix) || defined(__unix) || defined(__APPLE__)
#define BUILD_UNIX
#endif
#if defined(WIN32) || defined(_WIN32)
#define BUILD_WIN32
#endif

#include "disp.h"
#include "widget.h"


/* UI element bits */
enum {
	UI_FRAME	= 0x0001,
	UI_CPU		= 0x0002,
	UI_MEM		= 0x0004,
	UI_LOAD		= 0x0008,
	UI_NET		= 0x0010,
	UI_BAT		= 0x0020,

	UI_ALL		= 0x7fff
};

/* battery state */
enum { BAT_FULL, BAT_CHG, BAT_DIS };

struct sysmon {
	/* CPU usage range [0, 127] */
	int single;		/* aggregate CPU usage for all CPUs */
	int *cpu;		/* per CPU usage */
	unsigned int num_cpus;

	unsigned int loadavg[3];		/* fixed point .10 */

	unsigned long mem_total, mem_free;	/* in kb */
	unsigned long net_rx, net_tx;		/* in bytes */

	unsigned int bat, bat_time, bat_state;
};

extern struct sysmon smon;
extern unsigned int ui_active_widgets;

#define LUT_SIZE		128
extern unsigned int cpulut[LUT_SIZE];
extern unsigned int plotlut[LUT_SIZE];

void layout(void);
void draw_window(unsigned int dirty_override);

int cpu_init(void);
void cpu_shutdown(void);
int mem_init(void);
int load_init(void);
int net_init(void);
int bat_init(void);

void cpu_update(void);
void mem_update(void);
void load_update(void);
void net_update(void);
void bat_update(void);

int cpumon_init(void);
void cpumon_destroy(void);
void cpumon_move(int x, int y);
void cpumon_resize(int x, int y);
int cpumon_height(int w);
void cpumon_update(void);
void cpumon_draw(void);
int cpumon_info(int show, int x, int y);

void memmon_move(int x, int y);
void memmon_resize(int x, int y);
int memmon_height(int w);
void memmon_draw(void);
void netmon_rclick(int x, int y);

int loadmon_init(void);
void loadmon_move(int x, int y);
void loadmon_resize(int x, int y);
int loadmon_height(int w);
void loadmon_draw(void);

int netmon_init(void);
void netmon_move(int x, int y);
void netmon_resize(int x, int y);
int netmon_height(int w);
void netmon_draw(void);

int batmon_init(void);
void batmon_move(int x, int y);
void batmon_resize(int x, int y);
int batmon_height(int w);
void batmon_draw(void);

/* defined in main.c */
void redisplay(unsigned int mask);
void rbutton(int press, int x, int y);
void rdrag(int x, int y);
int hittest(int x, int y, struct rect *r);
long get_msec(void);

/* defined in memmon.c */
int memfmt(char *buf, unsigned long mem, int baseu);

#endif	/* XMON_H_ */
