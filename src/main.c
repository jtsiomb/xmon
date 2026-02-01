#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "xmon.h"
#include "options.h"

#ifdef CALC_LUT
#include <math.h>
#else
#include "lut.h"
#endif

#ifdef BUILD_UNIX
#include <sys/time.h>

static struct timeval tv0;
#endif
#ifdef BUILD_WIN32
#include <windows.h>

static long tm0;
#endif

struct sysmon smon;
unsigned int ui_active_widgets;
unsigned int cpulut[LUT_SIZE];
unsigned int plotlut[LUT_SIZE];

static int frm_width;	/* total with bevels */
static int bevel;
static unsigned int dirty;

static struct rect minrect;
static struct rect cpu_rect, mem_rect, load_rect, net_rect;


long get_msec(void);


int main(int argc, char **argv)
{
	int i;
	long prev_upd, msec, dt, delay;
	char *env;

	init_opt();
	read_config();
	if(parse_args(argc, argv) == -1) {
		return 1;
	}
	bevel = opt.vis.bevel_thick;
	frm_width = opt.vis.frm_width + bevel;

	for(i=0; i<LUT_SIZE; i++) {
#ifdef CALC_LUT
		float t = (float)i / (float)(LUT_SIZE - 1);
		float x = 1.0f + (M_E - 1.0f) * t;
		unsigned int val = (unsigned int)(log(x) * 255.0f);
		printf("%u\n", val);
#else
		unsigned int val = lut[i];
#endif
		cpulut[i] = (unsigned int)((val * opt.cpu.ncolors) >> 8);
		plotlut[i] = val >> 1;
	}

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

	if(!opt.mon) {
		fprintf(stderr, "no monitoring widgets are enabled\n");
		return 1;
	}

	if((env = getenv("XMON_DBG_NCPU"))) {
		int n = atoi(env);
		if(n > 0 && n < smon.num_cpus) {
			smon.num_cpus = n;
		}
	}

	if(init_disp() == -1) {
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

	set_background(uicolor[COL_BG]);

	layout();
	resize_window(minrect.width, minrect.height);
	map_window();

	if(opt.x != -1 && opt.y != -1) {
		move_window(opt.x, opt.y);
	}

	prev_upd = -opt.upd_interv;
#ifdef BUILD_UNIX
	gettimeofday(&tv0, 0);
#endif
#ifdef BUILD_WIN32
	tm0 = GetTickCount();
#endif

	while(!quit) {
		msec = get_msec();
		dt = msec - prev_upd;
		delay = opt.upd_interv - dt;

		if(dirty) {
			delay = 0;
		}

		if(proc_events(delay) == -1) break;
		if(quit) break;

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
		}

		if(dirty && win_visible) {
			begin_drawing();
			draw_window(0);
			end_drawing();
		}
	}

	shutdown_disp();
	return 0;
}

#define ALL_X		frm_width
#define ALL_WIDTH	(win_width - frm_width * 2)
#define SEPSZ		(frm_width * 2)

void layout(void)
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

void draw_window(unsigned int dirty_override)
{
	int n = 0;

	dirty |= dirty_override;

	if(dirty & UI_FRAME) {
		clear_window();
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
}

void redisplay(unsigned int mask)
{
	dirty |= mask;
}

int hittest(int x, int y, struct rect *r)
{
	if(x < r->x || y < r->y) return 0;
	if(x >= r->x + r->width) return 0;
	if(y >= r->y + r->height) return 0;
	return 1;
}

static int cpumon_info_state;

void rbutton(int press, int x, int y)
{
	if(cpumon_info_state || hittest(x, y, &cpu_rect)) {
		cpumon_info_state = cpumon_info(press, x, y);
	}
}

void rdrag(int x, int y)
{
	if(hittest(x, y, &cpu_rect)) {
		cpumon_info_state = cpumon_info(1, x, y);
	} else {
		if(cpumon_info_state) {
			cpumon_info_state = cpumon_info(0, x, y);
		}
	}
}

#ifdef BUILD_UNIX
long get_msec(void)
{
	struct timeval tv;

	gettimeofday(&tv, 0);
	return (tv.tv_sec - tv0.tv_sec) * 1000 + (tv.tv_usec - tv0.tv_usec) / 1000;
}
#endif
#ifdef BUILD_WIN32
long get_msec(void)
{
	return GetTickCount() - tm0;
}
#endif
