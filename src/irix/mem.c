#include <stdio.h>
#include <unistd.h>
#include <sys/sysmp.h>
#include "xmon.h"

static int pageshift;

int mem_init(void)
{
	long pagesz;

	pagesz = sysconf(_SC_PAGESIZE);
	printf("page size: %ld ", pagesz);

	pageshift = 0;
	while(pagesz > 1) {
		pagesz >>= 1;
		pageshift++;
	}

	return 0;
}

void mem_update(void)
{
	struct rminfo rm;

	if(sysmp(MP_SAGET, MPSA_RMINFO, &rm, sizeof rm) == -1) {
		fprintf(stderr, "sysmp failed\n");
	}
	smon.mem_total = rm.physmem << (pageshift - 10);
	smon.mem_free = rm.freemem << (pageshift - 10);
}
