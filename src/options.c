#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include "xmon.h"
#include "options.h"

#ifdef BUILD_UNIX
#include <unistd.h>
#include <pwd.h>
#endif

struct options opt;

static struct color def_uicolor[NUM_UICOLORS] = {
	{0, 0, 0},				/* COL_FG */
	{0xb4, 0xb4, 0xb4},		/* COL_BG */
	{0xdf, 0xdf, 0xdf},		/* COL_BGHI */
	{0x62, 0x62, 0x62},		/* COL_BGLO */
	{0x50, 0x50, 0xff},		/* COL_A */
	{0xff, 0x50, 0x50},		/* COL_B */
	{0xc7, 0x50, 0xff}		/* COL_AB */
};

/* these must match the order of the MON_* enums in options.h */
static const char *monstr[] = {"cpu", "mem", "load", "net", 0};

#ifdef BUILD_UNIX
static const char *cfgpath[] = {"~/.config/xmon.conf", "~/.xmon.conf", "/etc/xmon.conf", 0};
#else
static const char *cfgpath[] = {"xmon.conf", 0};
#endif

static int read_config_file(const char *fname, FILE *fp);
static int parse_color(const char *str, struct color *col);
static void calc_bevel_colors(void);

void init_opt(void)
{
#ifdef BUILD_UNIX
	int i;
	char *path, *homedir = 0;
	struct passwd *pw;
#endif

	opt.x = opt.y = -1;
	opt.xsz = 110;
	opt.ysz = 200;
	opt.upd_interv = 250;

	opt.mon = MON_ALL;

	memcpy(opt.vis.uicolor, def_uicolor, sizeof opt.vis.uicolor);
#ifdef BUILD_WIN32
	opt.vis.font = "Arial:12:bold";
#else
	opt.vis.font = "-*-helvetica-bold-r-*-*-12-*";
#endif
	opt.vis.frm_width = 4;
	opt.vis.decor = 0;
	opt.vis.bevel_thick = 2;

	opt.cpu.ncolors = 16;
	opt.cpu.autosplit = 1;

	/* expand cfg paths */
#ifdef BUILD_UNIX
	if((pw = getpwuid(getuid()))) {
		homedir = pw->pw_dir;
	} else {
		homedir = getenv("HOME");
	}

	if(homedir) {
		for(i=0; cfgpath[i]; i++) {
			if(cfgpath[i][0] == '~') {
				if(!(path = malloc(strlen(cfgpath[i]) + strlen(homedir) + 1))) {
					fprintf(stderr, "failed to allocate buffer for search path expansion\n");
					continue;
				}
				strcpy(path, homedir);
				strcat(path, cfgpath[i] + 1);
				cfgpath[i] = path;
			}
		}
	}
#endif
}

static const char *usage_str[] = {
	"Usage: %s [options]\n",
	"Options:\n",
	" -s/-size <width>x<height>: specify window size\n",
	" -pos <X>,<Y>: specify window position\n",
	" -update <interval>: update interval in milliseconds\n",
	" -cpu/-nocpu: enable/disable CPU usage display\n",
	" -load/-noload: enable/disable load average display\n",
	" -mem/-nomem: enable/disable memory usage display\n",
	" -font <font>: specify UI font\n",
	" -frame <pixels>: UI frame width in pixels (not including bevels)\n",
	" -decor/-nodecor: enable/disable window decorations (frame, titlebar)\n",
	" -bevel <pixels>: bevel thickness for the default UI look\n",
	" -textcolor <r,g,b>: specify the text color\n",
	" -bgcolor <r,g,b>: specify background color\n",
	" -v/-verbose: verbose output\n",
	" -cpu-colors <n>: number of colors to use for the CPU usage plot\n",
	" -cpu-nosplit: don't split CPU plot to discrete bars even if they fit\n",
	" -net-if <name>: only collect traffic for a specific named interface\n",
	" -h/-help: print usage and exit\n",
	0
};

int parse_args(int argc, char **argv)
{
	int i, j, x, y, num;
	unsigned int width, height;
	char buf[64];
	char *endp;

	for(i=1; i<argc; i++) {
		if(argv[i][0] == '-') {
			if(strcmp(argv[i], "-size") == 0 || strcmp(argv[i], "-s") == 0) {
				if(!argv[++i] || (num = sscanf(argv[i], "%ux%u", &width, &height)) <= 0) {
					fprintf(stderr, "%s must be followed by WxH\n", argv[i - 1]);
					return -1;
				}
				opt.xsz = width;
				if(num > 1) opt.ysz = height;

			} else if(strcmp(argv[i], "-pos") == 0) {
				if(!argv[++i] || (num = sscanf(argv[i], "%d,%d", &x, &y)) <= 0) {
					fprintf(stderr, "-pos must be followed by X,Y\n");
					return -1;
				}
				opt.x = x;
				if(num > 1) opt.y = y;

			} else if(strcmp(argv[i], "-update") == 0) {
				if(!argv[++i] || (opt.upd_interv = atoi(argv[i])) <= 0) {
					fprintf(stderr, "-update must be followed by an update rate in milliseconds\n");
					return -1;
				}

			} else if(strcmp(argv[i], "-font") == 0){
				if(!argv[++i]) {
					fprintf(stderr, "-font must be followed by a X font descriptor\n");
					return -1;
				}
				opt.vis.font = argv[i];

			} else if(strcmp(argv[i], "-frame") == 0) {
				if(!argv[++i] || (opt.vis.frm_width = strtol(argv[i], &endp, 0),
							(endp == argv[i] || opt.vis.frm_width < 0))) {
					fprintf(stderr, "-frame must be followed by the frame thickness\n");
					return -1;
				}

			} else if(strcmp(argv[i], "-decor") == 0) {
				opt.vis.decor = 1;
			} else if(strcmp(argv[i], "-nodecor") == 0) {
				opt.vis.decor = 0;

			} else if(strcmp(argv[i], "-bevel") == 0) {
				if(!argv[++i] || (opt.vis.bevel_thick = strtol(argv[i], &endp, 0),
							(endp == argv[i] || opt.vis.bevel_thick < 0))) {
					fprintf(stderr, "-bevel must be followed by the bevel thickness\n");
					return -1;
				}

			} else if(strcmp(argv[i], "-textcolor") == 0 || strcmp(argv[i], "-fgcolor") == 0) {
				if(parse_color(argv[++i], opt.vis.uicolor + COL_FG) == -1) {
					fprintf(stderr, "%s must be followed by a color\n", argv[i - 1]);
					return -1;
				}

			} else if(strcmp(argv[i], "-bgcolor") == 0) {
				if(parse_color(argv[++i], opt.vis.uicolor + COL_BG) == -1) {
					fprintf(stderr, "-bgcolor must be followed by a color\n");
					return -1;
				}
				calc_bevel_colors();

			} else if(strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "-verbose") == 0) {
				opt.verbose = 1;

			} else if(strcmp(argv[i], "-net-if") == 0) {
				if(!argv[++i]) {
					fprintf(stderr, "-net-if must be followed by an interface name\n");
					return -1;
				}
				opt.net.ifname = argv[i];

			} else if(strcmp(argv[i], "-cpu-colors") == 0) {
				if(!argv[++i] || (num = atoi(argv[i])) < 3 || num > 128) {
					fprintf(stderr, "-cpu-colors must be followed by a number between 3 and 128\n");
					return -1;
				}
				opt.cpu.ncolors = num;

			} else if(strcmp(argv[i], "-cpu-nosplit") == 0) {
				opt.cpu.autosplit = 0;

			} else if(strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "-help") == 0) {
				for(j=0; usage_str[j]; j++) {
					printf(usage_str[j], argv[0]);
				}
				exit(0);

			} else {
				/* handle -cpu/-nocpu, -mem/-nomem ... etc */
				for(j=0; monstr[j]; j++) {
					sprintf(buf, "-%s", monstr[j]);
					if(strcmp(argv[i], buf) == 0) {
						opt.mon |= 1 << j;
						break;
					}
					sprintf(buf, "-no%s", monstr[j]);
					if(strcmp(argv[i], buf) == 0) {
						opt.mon &= ~(1 << j);
						break;
					}
				}

				if(monstr[j] == 0) {
					/* went through all of the suffixes, it's not one of them */
					fprintf(stderr, "unrecognized option: %s\n", argv[i]);
					return -1;
				}
			}

		} else {
			fprintf(stderr, "unexpected argument: %s\n", argv[i]);
			return -1;
		}
	}

	return 0;
}

int read_config(void)
{
	int i;
	FILE *fp;

	for(i=0; cfgpath[i]; i++) {
		if((fp = fopen(cfgpath[i], "rb"))) {
			return read_config_file(cfgpath[i], fp);
		}
	}
	return -1;
}

static char *clean_line(char *s)
{
	char *endp;
	while(*s && isspace(*s)) s++;
	if(!*s) return 0;

	endp = s;
	while(*endp && *endp != '#') endp++;
	*endp-- = 0;
	while(endp > s && isspace(*endp)) {
		*endp-- = 0;
	}

	return *s ? s : 0;
}

static int read_config_file(const char *fname, FILE *fp)
{
	char buf[256];
	char *name, *valstr;
	int i, val[3], boolval, lineno = 0;
	int num_val;

	if(!(fp = fopen(fname, "rb"))) {
		return -1;
	}

	while(fgets(buf, sizeof buf, fp)) {
		lineno++;
		if(!(valstr = strchr(buf, ':'))) {
			continue;
		}
		*valstr++ = 0;
		if(!(name = clean_line(buf))) {
			continue;
		}
		if(!(valstr = clean_line(valstr))) {
			continue;
		}

		num_val = sscanf(valstr, "%d %d %d", val, val + 1, val + 2);
		if(strcmp(valstr, "true") == 0 || strcmp(valstr, "yes") == 0 || strcmp(valstr, "on") == 0) {
			boolval = 1;
		} else if(strcmp(valstr, "false") == 0 || strcmp(valstr, "no") == 0 || strcmp(valstr, "off") == 0) {
			boolval = 0;
		}

		if(strcmp(name, "size") == 0) {
			if(sscanf(valstr, "%dx%d", val, val + 1) != 2) {
				fprintf(stderr, "%s %d: invalid size, expected <width>x<height>\n",
						fname, lineno);
				continue;
			}
			opt.xsz = val[0];
			opt.ysz = val[1];

		} else if(strcmp(name, "position") == 0) {
			if(sscanf(valstr, "%d %d", val, val + 1) != 2) {
				fprintf(stderr, "%s %d: invalid position, expected <x> <y>\n", fname, lineno);
				continue;
			}
			opt.x = val[0];
			opt.y = val[1];

		} else if(strcmp(name, "update") == 0) {
			if(!(val[0] = atoi(valstr))) {
				fprintf(stderr, "%s %d: invalid update, expected number of milliseconds\n",
						fname, lineno);
				continue;
			}
			opt.upd_interv = val[0];

		} else if(strcmp(name, "enable") == 0) {
			for(i=0; monstr[i]; i++) {
				if(strstr(valstr, monstr[i])) {
					opt.mon |= 1 << i;
				}
			}

		} else if(strcmp(name, "disable") == 0) {
			for(i=0; monstr[i]; i++) {
				if(strstr(valstr, monstr[i])) {
					opt.mon &= ~(1 << i);
				}
			}

		} else if(strcmp(name, "font") == 0) {
			opt.vis.font = strdup(valstr);

		} else if(strcmp(name, "frame") == 0) {
			if(!num_val) {
				fprintf(stderr, "%s %d: invalid frame, expected thickness in pixels\n",
						fname, lineno);
				continue;
			}
			opt.vis.frm_width = val[0];

		} else if(strcmp(name, "decor") == 0) {
			if(boolval < 0) {
				fprintf(stderr, "%s %d: invalid decor, expected boolean\n", fname, lineno);
				continue;
			}
			opt.vis.decor = boolval;

		} else if(strcmp(name, "bevel") == 0) {
			if(!num_val) {
				fprintf(stderr, "%s %d: invalid bevel, expected thickness\n", fname, lineno);
				continue;
			}
			opt.vis.bevel_thick = val[0];

		} else if(strcmp(name, "textcolor") == 0 || strcmp(name, "bgcolor") == 0) {
			struct color col;
			int cidx = name[0] == 't' ? COL_FG : COL_BG;
			if(parse_color(valstr, &col) == -1) {
				fprintf(stderr, "%s %d: invalid %s, expected <r>,<g>,<b>\n", fname,
						lineno, name);
				continue;
			}

			opt.vis.uicolor[cidx] = col;
			calc_bevel_colors();

		} else if(strcmp(name, "verbose") == 0) {
			if(boolval < 0) {
				fprintf(stderr, "%s %d: invalid verbose, expected boolean\n", fname, lineno);
				continue;
			}
			opt.verbose = boolval;

		} else if(strcmp(name, "cpu-colors") == 0) {
			if(!num_val || val[0] < 3 || val[0] > 128) {
				fprintf(stderr, "%s %d: invalid cpu-colors, expected number between 3 and 128\n", fname, lineno);
				continue;
			}
			opt.cpu.ncolors = val[0];

		} else if(strcmp(name, "cpu-autosplit") == 0) {
			if(boolval < 0) {
				fprintf(stderr, "%s %d: invalid cpu-autosplit, expected boolean\n", fname, lineno);
				continue;
			}
			opt.cpu.autosplit = boolval;

		} else if(strcmp(name, "net-if") == 0) {
			opt.net.ifname = strdup(valstr);

		} else {
			fprintf(stderr, "%s %d: ignoring unknown option: %s\n", fname, lineno, name);
			continue;
		}
	}

	fclose(fp);
	return 0;
}

static int parse_color(const char *str, struct color *col)
{
	unsigned int packed;

	if(!str) return -1;

	if(sscanf(str, "%d,%d,%d", &col->r, &col->g, &col->b) == 3) {
		return 0;
	}

	if(sscanf(str, "%x", &packed) == 1) {
		col->r = (packed >> 16) & 0xff;
		col->g = (packed >> 8) & 0xff;
		col->b = packed & 0xff;
		return 0;
	}

	return -1;
}

static void calc_bevel_colors(void)
{
	unsigned int rr, gg, bb;
	unsigned int r = opt.vis.uicolor[COL_BG].r;
	unsigned int g = opt.vis.uicolor[COL_BG].g;
	unsigned int b = opt.vis.uicolor[COL_BG].b;

	rr = r * 5 / 4;
	gg = g * 5 / 4;
	bb = b * 5 / 4;
	opt.vis.uicolor[COL_BGHI].r = rr;
	opt.vis.uicolor[COL_BGHI].g = gg;
	opt.vis.uicolor[COL_BGHI].b = bb;

	rr = r * 3 / 5;
	gg = g * 3 / 5;
	bb = b * 3 / 5;
	opt.vis.uicolor[COL_BGLO].r = rr;
	opt.vis.uicolor[COL_BGLO].g = gg;
	opt.vis.uicolor[COL_BGLO].b = bb;
}
