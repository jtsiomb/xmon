#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "xmon.h"

struct cpustat {
	unsigned long val[2][7];
};

static FILE *fp;
static struct cpustat *cpustat;
static int cpucount, curupd;
static char *statbuf;
static int sbufsz;

static int calc_usage(unsigned long *cval, unsigned long *pval);
static int parse_cpustat(int cur);

int cpu_init(void)
{
	char buf[256];

	smon.cpu = 0;
	cpustat = 0;
	statbuf = 0;

	if(!(fp = fopen("/proc/stat", "rb"))) {
		fprintf(stderr, "failed to open /proc/stat\n");
		return 1;
	}

	cpucount = 0;
	while(fgets(buf, sizeof buf, fp)) {
		if(memcmp(buf, "cpu", 3) == 0 && isdigit(buf[3])) {
			cpucount++;
		}
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

	sbufsz = (ftell(fp) * 2) & 0xfffff000;
	if(sbufsz < 4096) sbufsz = 4096;

	if(!(statbuf = malloc(sbufsz))) {
		fprintf(stderr, "failed to allocate read buffer\n");
		goto fail;
	}

	fseek(fp, 0, SEEK_SET);
	if(fread(statbuf, 1, sbufsz, fp) <= 0) {
		fprintf(stderr, "unexpected EOF while reading /proc/stat\n");
		goto fail;
	}
	parse_cpustat(1);

	return 0;

fail:
	fclose(fp); fp = 0;
	free(smon.cpu);
	free(cpustat);
	free(statbuf);
	return -1;
}

void cpu_update(void)
{
	int i, nextupd;

	nextupd = curupd ^ 1;

	fseek(fp, 0, SEEK_SET);
	if(fread(statbuf, 1, sbufsz, fp) <= 0) {
		return;
	}
	parse_cpustat(curupd);

	for(i=0; i<cpucount; i++) {
		smon.cpu[i] = calc_usage(cpustat[i].val[curupd], cpustat[i].val[nextupd]);
		if(smon.cpu[i] >= 128) {
			smon.cpu[i] = 127;
		}
	}
	smon.single = calc_usage(cpustat[cpucount].val[curupd], cpustat[cpucount].val[nextupd]);
	if(smon.single >= 128) {
		smon.single = 127;
	}

	curupd = nextupd;
}

static int calc_usage(unsigned long *cval, unsigned long *pval)
{
	unsigned long delta[7], sum = 0;
	int i;

	for(i=0; i<7; i++) {
		delta[i] = cval[i] - pval[i];
		sum += delta[i];
	}

	return sum ? 128 - (delta[3] << 7) / sum : 0;
}

static int parse_cpustat(int cur)
{
	int count;
	char *line, *end;
	struct cpustat *cst;
	unsigned long *val;
	unsigned long cpuidx;

	cst = cpustat;
	count = cpucount;
	line = end = statbuf;

	while(count > 0) {
		while(end < statbuf + sbufsz && *end != '\n') {
			end++;
		}
		if(end - line < 5) return -1;	/* won't even fit "cpuN " */
		*end = 0;

		if(memcmp(line, "cpu", 3) != 0) {
			line = end + 1;
			continue;
		}

		if(isdigit(line[3])) {
			/* specific CPU usage line */
			val = cst->val[cur];
			if(sscanf(line, "cpu%lu %lu %lu %lu %lu %lu %lu %lu", &cpuidx, val,
						val + 1, val + 2, val + 3, val + 4, val + 5, val + 6) < 8) {
				return -1;
			}

			count--;
			cst++;

		} else {
			/* average CPU usage line, we should encounter only one */
			val = cpustat[cpucount].val[cur];
			if(sscanf(line, "cpu %lu %lu %lu %lu %lu %lu %lu", val, val + 1,
						val + 2, val + 3, val + 4, val + 5, val + 6) < 7) {
				return -1;
			}
		}
		line = end + 1;
	}
	return 0;
}
