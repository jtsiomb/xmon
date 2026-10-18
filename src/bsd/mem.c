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
#include <stdio.h>
#include <stdlib.h>
#include <sys/sysctl.h>
#include "xmon.h"

static int shift;

static int calc_shift(unsigned int pgsz);

int mem_init(void)
{
	unsigned int i, pgsz, pgtotal, num;
	size_t len;
	static const char *statname[] = {
		"vm.stats.vm.v_active_count",
		"vm.stats.vm.v_inactive_count",
		"vm.stats.vm.v_wire_count",
		"vm.stats.vm.v_cache_count",
		"vm.stats.vm.v_free_count",
		0
	};

	len = sizeof pgsz;
	if(sysctlbyname("vm.stats.vm.v_page_size", &pgsz, &len, 0, 0) == -1 || !pgsz) {
		fprintf(stderr, "failed to get page size\n");
		return -1;
	}
	shift = calc_shift(pgsz);

	pgtotal = 0;
	for(i=0; statname[i]; i++) {
		len = sizeof num;
		if(sysctlbyname(statname[i], &num, &len, 0, 0) == 0) {
			pgtotal += num;
		}
	}
	smon.mem_total = pgtotal << shift;
	return 0;
}

void mem_update(void)
{
	unsigned int mfree, mcache;
	size_t len;

	len = sizeof mfree;
	if(sysctlbyname("vm.stats.vm.v_free_count", &mfree, &len, 0, 0) == -1) {
		return;
	}
	if(sysctlbyname("vm.stats.vm.v_cache_count", &mcache, &len, 0, 0) != -1) {
		mfree += mcache;
	}
	smon.mem_free = mfree << shift;
}

/* calculates the left shift required to go from page count to kb */
static int calc_shift(unsigned int pgsz)
{
	int shift = 0;
	while(pgsz > 1) {
		pgsz >>= 1;
		shift++;
	}
	return shift - 10;
}
