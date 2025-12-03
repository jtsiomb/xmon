#include <stdio.h>
#include <stdlib.h>
#include <sys/sysmp.h>
#include <sys/sysinfo.h>
#include "xmon.h"

struct cpustat {
	unsigned long val[2][6];
};

static struct cpustat *cpustat;
static int cpucount, curupd;
static int sinfo_sz;
static struct sysinfo *sinfo;

static int calc_usage(unsigned long *cval, unsigned long *pval);
static int read_cpustat(int cur);


int cpu_init(void)
{
	if((cpucount = sysmp(MP_NPROCS)) == -1) {
		fprintf(stderr, "failed to retreive the number of processors\n");
		return -1;
	}

	smon.num_cpus = cpucount;
	if(!(smon.cpu = calloc(cpucount, sizeof *smon.cpu))) {
		fprintf(stderr, "failed to allocate CPU usage buffer for %d cpus\n", cpucount);
		goto fail;
	}

	if(!(cpustat = malloc((cpucount + 1) * sizeof *cpustat))) {
		fprintf(stderr, "failed to allocate ping-pong buffer for %d cpus\n", cpucount);
		goto fail;
	}

	if((sinfo_sz = sysmp(MP_SASZ, MPSA_SINFO)) == -1) {
		fprintf(stderr, "sysinfo request failed\n");
		goto fail;
	}
	if(!(sinfo = malloc(sinfo_sz))) {
		fprintf(stderr, "failed to allocate sysinfo struct (%d bytes)\n", sinfo_sz);
		goto fail;
	}

	read_cpustat(1);
	return 0;

fail:
	free(smon.cpu);
	free(sinfo);
	return -1;
}

void cpu_update(void)
{
	int i, nextupd;

	nextupd = curupd ^ 1;

	read_cpustat(curupd);

	for(i=0; i<cpucount; i++) {
		smon.cpu[i] = calc_usage(cpustat[i].val[curupd], cpustat[i].val[nextupd]);
		if(smon.cpu[i] >= 128) {
			smon.cpu[i] = 127;
		}
	}
	smon.single = calc_usage(cpustat[i].val[curupd], cpustat[i].val[nextupd]);
	if(smon.single >= 128) {
		smon.single = 127;
	}

	curupd = nextupd;
}

static int calc_usage(unsigned long *cval, unsigned long *pval)
{
	unsigned long delta[7], usage, sum = 0;
	int i;

	for(i=0; i<7; i++) {
		delta[i] = cval[i] - pval[i];
		sum += delta[i];
	}

	usage = delta[CPU_USER] + delta[CPU_KERNEL] + delta[CPU_INTR];

	return sum ? 128 - (usage << 7) / sum : 0;
}

static int read_cpustat(int cur)
{
	int i, j;
	struct sysinfo sinfo;
	unsigned long *val;

	for(i=0; i<cpucount; i++) {
		sysmp(MP_SAGET1, MPSA_SINFO, &sinfo, sinfo_sz, i);

		val = cpustat[i].val[cur];
		for(j=0; j<6; j++) {
			val[j] = sinfo.cpu[j];
		}
	}

	sysmp(MP_SAGET, MPSA_SINFO, &sinfo, sinfo_sz);
	val = cpustat[cpucount].val[cur];
	for(i=0; i<6; i++) {
		val[i] = sinfo.cpu[i];
	}
	return 0;
}
