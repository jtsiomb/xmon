#include <stdio.h>
#include <stdlib.h>
#include <sys/sysctl.h>
#include <sys/resource.h>
#include "xmon.h"

static int shift;

int load_init(void)
{
	struct loadavg la;
	size_t len = sizeof la;

	if(sysctlbyname("vm.loadavg", &la, &len, 0, 0) == -1) {
		fprintf(stderr, "failed to get load average\n");
		return -1;
	}

	shift = 0;
	while(la.fscale > 1) {
		la.fscale >>= 1;
		shift++;
	}
	shift = 10 - shift;	/* we store load in 12.10 fixed point */

	return 0;
}

void load_update(void)
{
	struct loadavg la;
	size_t len = sizeof la;

	sysctlbyname("vm.loadavg", &la, &len, 0, 0);

	if(shift == 0) {
		smon.loadavg[0] = la.ldavg[0];
		smon.loadavg[1] = la.ldavg[1];
		smon.loadavg[2] = la.ldavg[2];
	} else if(shift > 0) {
		smon.loadavg[0] = la.ldavg[0] << shift;
		smon.loadavg[1] = la.ldavg[1] << shift;
		smon.loadavg[2] = la.ldavg[2] << shift;
	} else {
		smon.loadavg[0] = la.ldavg[0] >> -shift;
		smon.loadavg[1] = la.ldavg[1] >> -shift;
		smon.loadavg[2] = la.ldavg[2] >> -shift;
	}
}
