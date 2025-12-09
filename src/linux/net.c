#ifndef NET_H_
#define NET_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include "xmon.h"
#include "options.h"

static FILE *fp;
static unsigned long long prev_rx, prev_tx;

int net_init(void)
{
	int found = 0;
	char buf[128], *ptr, *ifname;

	if(!(fp = fopen("/proc/net/dev", "rb"))) {
		fprintf(stderr, "failed to open /proc/net/dev: %s\n", strerror(errno));
		return -1;
	}

	if(opt.net.ifname) {
		while(fgets(buf, sizeof buf, fp)) {
			if(!(ptr = strchr(buf, ':'))) {
				continue;
			}
			*ptr = 0;
			ifname = buf;
			while(*ifname && isspace(*ifname)) ifname++;
			if(strcmp(ifname, opt.net.ifname) == 0) {
				found = 1;
				break;
			}
		}

		if(!found) {
			fprintf(stderr, "failed to find network interface: %s\n", opt.net.ifname);
			return -1;
		}
	}

	return 0;
}

void net_update(void)
{
	int i, ncols;
	char buf[256];
	char *ptr, *ifname, *endp, *col[16];
	unsigned long long cur_rx, cur_tx;

	fseek(fp, 0, SEEK_SET);

	cur_rx = cur_tx = 0;

	while(fgets(buf, sizeof buf, fp)) {
		if(!(ptr = strchr(buf, ':'))) {
			continue;
		}
		*ptr++ = 0;

		if(opt.net.ifname) {
			ifname = buf;
			while(*ifname && isspace(*ifname)) ifname++;
			if(strcmp(ifname, opt.net.ifname) != 0) {
				continue;
			}
		}

		ncols = 0;
		for(i=0; i<16; i++) {
			while(*ptr && isspace(*ptr)) ptr++;		/* skip leading blanks */
			if(!*ptr || !isdigit(*ptr)) break;		/* should be a number */
			col[ncols++] = ptr;
			while(*ptr && !isspace(*ptr)) ptr++;	/* skip number */
		}

		if(ncols < 9) continue;

		cur_rx += strtoull(col[0], &endp, 10);
		if(endp == col[0]) continue;
		cur_tx += strtoull(col[8], &endp, 10);
		if(endp == col[8]) continue;
	}

	smon.net_rx = prev_rx ? cur_rx - prev_rx : 0;
	smon.net_tx = prev_tx ? cur_tx - prev_tx : 0;
	prev_rx = cur_rx;
	prev_tx = cur_tx;
}

#endif	/* NET_H_ */
