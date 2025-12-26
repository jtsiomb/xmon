#include <stdio.h>
#include <stdlib.h>
#include <sys/sysctl.h>
#include <sys/resource.h>
#include "xmon.h"

struct cpustat {
	long times[2][CPUSTATES];
};

static struct cpustat *cpustat;
static int curst;
static long *scbuf;
static size_t scbuf_size;

static int get_cpustat(int cur);


int cpu_init(void)
{
	int num;
	size_t len;

	len = sizeof num;
	if(sysctlbyname("hw.ncpu", &num, &len, 0, 0) == -1) {
		fprintf(stderr, "failed to retrieve number of processors\n");
		return -1;
	}
	smon.num_cpus = num;

	if(!(smon.cpu = calloc(num, sizeof *smon.cpu))) {
		fprintf(stderr, "failed to allocate CPU usage array (%d)\n", num);
		return -1;
	}
	if(!(cpustat = calloc(num + 1, sizeof *cpustat))) {
		fprintf(stderr, "failed to allocate CPU time buffer\n");
		free(smon.cpu);
		return -1;
	}

	scbuf_size = num * CPUSTATES * sizeof *scbuf;
	if(!(scbuf = malloc(scbuf_size))) {
		fprintf(stderr, "failed to allocate sysctl buffer\n");
		free(smon.cpu);
		free(cpustat);
		return -1;
	}

	get_cpustat(1);
	curst = 0;
	return 0;
}

void cpu_update(void)
{
	int i, j, nextst = curst ^ 1;
	unsigned long delta[CPUSTATES], sum;
	struct cpustat *cst;

	get_cpustat(curst);

	for(i=0; i<smon.num_cpus; i++) {
		cst = cpustat + i;
		sum = 0;
		for(j=0; j<CPUSTATES; j++) {
			delta[j] = cst->times[curst][j] - cst->times[nextst][j];
			sum += delta[j];
		}

		smon.cpu[i] = sum ? 128 - (delta[CP_IDLE] << 7) / sum : 0;
	}

	cst = cpustat + smon.num_cpus;
	sum = 0;
	for(j=0; j<CPUSTATES; j++) {
		delta[j] = cst->times[curst][j] - cst->times[nextst][j];
		sum += delta[j];
	}
	smon.single = sum ? 128 - (delta[CP_IDLE] << 7) / sum : 0;

	curst = nextst;
}

static int get_cpustat(int cur)
{
	int i, j;
	struct cpustat *cst;
	long *valptr;
	size_t len;

	len = scbuf_size;
	if(sysctlbyname("kern.cp_times", scbuf, &len, 0, 0) == -1) {
		fprintf(stderr, "insufficient space in sysconf buffer\n");
		free(scbuf);
		if(!(scbuf = malloc(len))) {
			fprintf(stderr, "failed to reallocate sysconf buffer\n");
			return -1;
		}
		scbuf_size = len;
	}

	valptr = scbuf;
	for(i=0; i<smon.num_cpus; i++) {
		cst = cpustat + i;
		for(j=0; j<CPUSTATES; j++) {
			cst->times[cur][j] = valptr[j];
		}
		valptr += CPUSTATES;
	}

	len = scbuf_size;
	if(sysctlbyname("kern.cp_time", scbuf, &len, 0, 0) == -1) {
		fprintf(stderr, "insufficient space in sysconf buffer 2\n");
		return -1;
	}
	cst = cpustat + smon.num_cpus;
	for(i=0; i<CPUSTATES; i++) {
		cst->times[cur][i] = scbuf[i];
	}

	return 0;
}
