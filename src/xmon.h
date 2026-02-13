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

	UI_ALL		= 0x7fff
};

struct sysmon {
	/* CPU usage range [0, 127] */
	int single;		/* aggregate CPU usage for all CPUs */
	int *cpu;		/* per CPU usage */
	unsigned int num_cpus;

	unsigned int loadavg[3];		/* fixed point .10 */

	unsigned long mem_total, mem_free;	/* in kb */
	unsigned long net_rx, net_tx;		/* in bytes */
};

extern struct sysmon smon;
extern unsigned int ui_active_widgets;

#define LUT_SIZE		128
extern unsigned int cpulut[LUT_SIZE];
extern unsigned int plotlut[LUT_SIZE];

void layout(void);
void draw_window(unsigned int dirty_override);

int cpu_init(void);
int mem_init(void);
int load_init(void);
int net_init(void);

void cpu_update(void);
void mem_update(void);
void load_update(void);
void net_update(void);

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

/* defined in main.c */
void redisplay(unsigned int mask);
void rbutton(int press, int x, int y);
void rdrag(int x, int y);
int hittest(int x, int y, struct rect *r);
long get_msec(void);

/* defined in memmon.c */
int memfmt(char *buf, unsigned long mem, int baseu);

#endif	/* XMON_H_ */
