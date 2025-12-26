#include <stdio.h>
#include <string.h>
#include "xmon.h"
#include "options.h"

/*static XColor col_bar = {0, 0x5000, 0x5000, 0xffff};*/
static struct rect rect;

void memmon_move(int x, int y)
{
	rect.x = x;
	rect.y = y;
}

void memmon_resize(int x, int y)
{
	rect.width = x;
	rect.height = y;
}

int memmon_height(int w)
{
	return font.height * 2 + bar_height + 2;
}

void memmon_draw(void)
{
	char buf[128], *ptr;
	unsigned int used, ratio_used, ratio_total;
	int baseline, y;

	if(smon.mem_total <= 0) return;

	set_color(uicolor[COL_BG]);
	draw_rect(rect.x, rect.y, rect.width, font.height * 2);

	used = smon.mem_total - smon.mem_free;
	if(used & 0xfff00000) {
		ratio_used = used >> 12;
		ratio_total = smon.mem_total >> 12;
	} else {
		ratio_used = used;
		ratio_total = smon.mem_total;
	}
	baseline = rect.y + font.height - font.descent - 1;

	set_color(uicolor[COL_FG]);

	sprintf(buf, "MEM %3u%%", ratio_used * 100 / ratio_total);
	draw_text(rect.x, baseline, buf);
	baseline += font.height;

	ptr = buf;
	ptr += memfmt(ptr, used, 1);
	strcpy(ptr, " / "); ptr += 3;
	ptr += memfmt(ptr, smon.mem_total, 1);
	draw_text(rect.x, baseline, buf);

	y = baseline + font.descent + 1 + BEVEL;
	draw_bar(rect.x, y, rect.width, ratio_used, ratio_total);
}

int memfmt(char *buf, unsigned long mem, int baseunit)
{
	int idx = baseunit;
	int frac = 0;
	static const char *suffix[] = {"B", "K", "M", "G", "T", "P", 0};

	while(mem >= 1024 && suffix[idx + 1]) {
		frac = mem & 1023;
		mem >>= 10;
		idx++;
	}

	frac = (frac * 1000) >> 10;
	while(frac > 10) frac /= 10;

	return sprintf(buf, "%ld.%d %s", mem, frac, suffix[idx]);
}
