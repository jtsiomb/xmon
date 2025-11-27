#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include "sysmon.h"

struct cpustat {
	unsigned long val[2][7];
};

struct sysmon smon;

static FILE *fp;
static struct cpustat *cpustat;
static int cpucount, curupd;
static char *statbuf;
static int sbufsz;

static int parse_cpustat(int cur);

int sysmon_init(void)
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

	if(!(cpustat = malloc(cpucount * sizeof *cpustat))) {
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

void sysmon_update(void)
{
	int i, j, nextupd;
	unsigned long delta[7], sum;

	nextupd = curupd ^ 1;

	fseek(fp, 0, SEEK_SET);
	if(fread(statbuf, 1, sbufsz, fp) <= 0) {
		return;
	}
	parse_cpustat(curupd);

	for(i=0; i<cpucount; i++) {
		sum = 0;
		for(j=0; j<7; j++) {
			delta[j] = cpustat[i].val[curupd][j] - cpustat[i].val[nextupd][j];
			sum += delta[j];
		}

		if(sum) {
			smon.cpu[i] = 128 - (delta[3] << 7) / sum;
			if(smon.cpu[i] >= 128) {
				smon.cpu[i] = 127;
			}
		}
	}

	curupd = nextupd;
}

static int parse_cpustat(int cur)
{
	int count, cpuidx;
	char *line, *end;
	struct cpustat *cst;

	cst = cpustat;
	count = cpucount;
	line = end = statbuf;

	while(count > 0) {
		while(end < statbuf + sbufsz && *end != '\n') {
			end++;
		}
		if(end - line < 5) return -1;	/* won't even fit "cpuN " */
		*end = 0;

		if(memcmp(line, "cpu", 3) != 0 || !isdigit(line[3])) {
			line = end + 1;
			continue;
		}

		if(sscanf(line, "cpu%d %lu %lu %lu %lu %lu %lu %lu", &cpuidx, cst->val[cur],
					cst->val[cur] + 1, cst->val[cur] + 2, cst->val[cur] + 3, cst->val[cur] + 4,
					cst->val[cur] + 5, cst->val[cur] + 6) < 8) {
			return -1;
		}
		count--;
		cst++;
		line = end + 1;
	}
	return 0;
}
