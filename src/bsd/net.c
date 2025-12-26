#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ifaddrs.h>
#include <net/if.h>
#include "xmon.h"
#include "options.h"

static unsigned long long prev_rx, prev_tx;

int net_init(void)
{
	struct ifaddrs *iflist, *ifa;

	if(getifaddrs(&iflist) == -1) {
		fprintf(stderr, "failed to retreive interface list: %s\n", strerror(errno));
		return -1;
	}

	ifa = iflist;
	while(ifa) {
		if(opt.net.ifname && strcmp(ifa->ifa_name, opt.net.ifname) == 0) {
			break;
		}
		ifa = ifa->ifa_next;
	}
	if(opt.net.ifname && !ifa) {
		fprintf(stderr, "interface %s not found\n", opt.net.ifname);
		freeifaddrs(iflist);
		return -1;
	}

	freeifaddrs(iflist);
	return 0;
}

void net_update(void)
{
	struct ifaddrs *iflist, *ifa;
	struct if_data *ifdata;
	unsigned long tx, rx;

	if(getifaddrs(&iflist) == -1) {
		return;
	}

	ifa = iflist;
	tx = rx = 0;
	while(ifa) {
		if(ifa->ifa_addr->sa_family != AF_LINK) {
			goto next;
		}
		if(opt.net.ifname && strcmp(ifa->ifa_name, opt.net.ifname) != 0) {
			goto next;
		}

		ifdata = ifa->ifa_data;

		rx += ifdata->ifi_ibytes;
		tx += ifdata->ifi_obytes;
next:	ifa = ifa->ifa_next;
	}

	smon.net_rx = prev_rx ? rx - prev_rx : 0;
	smon.net_tx = prev_tx ? tx - prev_tx : 0;

	freeifaddrs(iflist);
}
