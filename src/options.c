#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include "options.h"

struct options opt;

static XColor def_uicolor[NUM_UICOLORS] = {
	{0, 0, 0, 0},
	{0, 0xb4b4, 0xb4b4, 0xb4b4},
	{0, 0xdfdf, 0xdfdf, 0xdfdf},
	{0, 0x6262, 0x6262, 0x6262}
};

static int parse_color(const char *str, XColor *col);
static void calc_bevel_colors(void);

void init_opt(void)
{
	opt.x = opt.y = 0;
	opt.xsz = 100;
	opt.ysz = 200;
	opt.upd_interv = 250;

	opt.mon = MON_ALL;

	memcpy(opt.vis.uicolor, def_uicolor, sizeof opt.vis.uicolor);
	opt.vis.font = "-*-helvetica-bold-r-*-*-12-*";
	opt.vis.frm_width = 4;
	opt.vis.decor = 0;
	opt.vis.bevel_thick = 2;

	opt.cpu.ncolors = 16;
}

static const char *usage_str[] = {
	"Usage: %s [options]\n",
	"Options:\n",
	" -geometry <width>x<height>+<x>+<y>: specify window geometry\n",
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
	" -h/-help: print usage and exit\n",
	0
};

int parse_args(int argc, char **argv)
{
	int i, j, x, y;
	unsigned int width, height, flags;
	char buf[64];
	char *endp;

	/* these must match the order of the MON_* enums in options.h */
	static const char *monsuffix[] = {"cpu", "mem", "load", 0};

	for(i=1; i<argc; i++) {
		if(argv[i][0] == '-') {
			if(strcmp(argv[i], "-geometry") == 0) {
				if(!argv[++i] || !(flags = XParseGeometry(argv[i], &x, &y, &width, &height))) {
					fprintf(stderr, "-geometry must be followed by WxH+X+Y\n");
					return -1;
				}
				if(flags & XValue) opt.x = x;
				if(flags & YValue) opt.y = y;
				if(flags & WidthValue) opt.xsz = width;
				if(flags & HeightValue) opt.ysz = height;

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

			} else if(strcmp(argv[i], "-net-if") == 0) {
				if(!argv[++i]) {
					fprintf(stderr, "-net-if must be followed by an interface name\n");
					return -1;
				}
				opt.net.ifname = argv[i];

			} else if(strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "-help") == 0) {
				for(j=0; usage_str[j]; j++) {
					printf(usage_str[j], argv[0]);
				}
				exit(0);

			} else {
				/* handle -cpu/-nocpu, -mem/-nomem ... etc */
				for(j=0; monsuffix[j]; j++) {
					sprintf(buf, "-%s", monsuffix[j]);
					if(strcmp(argv[i], buf) == 0) {
						opt.mon |= 1 << j;
						break;
					}
					sprintf(buf, "-no%s", monsuffix[j]);
					if(strcmp(argv[i], buf) == 0) {
						opt.mon &= ~(1 << j);
						break;
					}
				}

				if(monsuffix[j] == 0) {
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

int read_config(const char *fname)
{
	return -1;
}

static int parse_color(const char *str, XColor *col)
{
	unsigned int packed, r, g, b;

	if(!str) return -1;

	if(sscanf(str, "#%x", &packed) == 1) {
		r = (packed >> 16) & 0xff;
		g = (packed >> 8) & 0xff;
		b = packed & 0xff;

		col->red = r | (r << 8);
		col->green = g | (g << 8);
		col->blue = b | (b << 8);
		return 0;
	}

	if(sscanf(str, "%u,%u,%u", &r, &g, &b) == 3) {
		col->red = r | (r << 8);
		col->green = g | (g << 8);
		col->blue = b | (b << 8);
		return 0;
	}

	return -1;
}

static void calc_bevel_colors(void)
{
	unsigned int rr, gg, bb;
	unsigned int r = opt.vis.uicolor[COL_BG].red;
	unsigned int g = opt.vis.uicolor[COL_BG].green;
	unsigned int b = opt.vis.uicolor[COL_BG].blue;

	rr = r * 5 / 4;
	gg = g * 5 / 4;
	bb = b * 5 / 4;
	opt.vis.uicolor[COL_BGHI].red = rr;
	opt.vis.uicolor[COL_BGHI].green = gg;
	opt.vis.uicolor[COL_BGHI].blue = bb;

	rr = r * 3 / 5;
	gg = g * 3 / 5;
	bb = b * 3 / 5;
	opt.vis.uicolor[COL_BGLO].red = rr;
	opt.vis.uicolor[COL_BGLO].green = gg;
	opt.vis.uicolor[COL_BGLO].blue = bb;
}
