#include <stdio.h>
#include <stdlib.h>
#include <sys/sysctl.h>
#include "xmon.h"

static int shift;

static int calc_shift(unsigned int pgsz);

int mem_init(void)
{
	unsigned int pgsz, num;
	size_t len;

	len = sizeof pgsz;
	if(sysctlbyname("vm.stats.vm.v_page_size", &pgsz, &len, 0, 0) == -1 || !pgsz) {
		fprintf(stderr, "failed to get page size\n");
		return -1;
	}
	shift = calc_shift(pgsz);

	len = sizeof num;
	if(sysctlbyname("vm.stats.vm.v_page_count", &num, &len, 0, 0) == -1) {
		fprintf(stderr, "failed to get total memory size\n");
		return -1;
	}
	smon.mem_total = num << shift;
	return 0;
}

void mem_update(void)
{
	unsigned int mfree;
	size_t len;

	len = sizeof mfree;
	if(sysctlbyname("vm.stats.vm.v_free_count", &mfree, &len, 0, 0) == -1) {
		return;
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
