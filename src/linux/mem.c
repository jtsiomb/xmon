#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "xmon.h"

static int has_memavail;

static int match_field(const char *line, const char *name, long *retval);

int mem_init(void)
{
	char buf[256];
	long val;
	FILE *fp;

	if(!(fp = fopen("/proc/meminfo", "rb"))) {
		fprintf(stderr, "failed to open /proc/meminfo: %s\n", strerror(errno));
		return -1;
	}

	smon.mem_total = 0;
	while(fgets(buf, sizeof buf, fp)) {
		if(match_field(buf, "MemTotal", &val)) {
			smon.mem_total = val;
		} else if(match_field(buf, "MemAvailable", &val)) {
			has_memavail = 1;
		}
	}

	if(smon.mem_total <= 0) {
		fprintf(stderr, "failed to find MemTotal in /proc/meminfo\n");
		fclose(fp);
		return -1;
	}
	fclose(fp);

	return 0;
}

void mem_update(void)
{
	char buf[256];
	long mfree, mcache;
	FILE *fp;

	if(!(fp = fopen("/proc/meminfo", "rb"))) {
		return;
	}

	if(has_memavail) {
		/* linux >= 3.14 has MemAvailable */
		while(fgets(buf, sizeof buf, fp)) {
			if(match_field(buf, "MemAvailable", &smon.mem_free)) {
				break;
			}
		}
	} else {
		/* for older linux kernels, compute by adding MemFree and Cached */
		int num_fields = 2;
		while(num_fields > 0 && fgets(buf, sizeof buf, fp)) {
			if(match_field(buf, "MemFree", &mfree)) {
				num_fields--;
			} else if(match_field(buf, "Cached", &mcache)) {
				num_fields--;
			}
		}
		smon.mem_free = mfree + mcache;
	}

	fclose(fp);
}

static int match_field(const char *line, const char *name, long *retval)
{
	int len;
	long val;
	char *endp;

	len = strlen(name);
	if(memcmp(line, name, len) != 0 || line[len] != ':') {
		return 0;
	}

	val = strtol(line + len + 1, &endp, 0);
	if(endp == line) return 0;

	*retval = val;
	return 1;
}
