#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/sysmp.h>
#include "xmon.h"

static int fd = -1;
static long avenrun_offs;

int load_init(void)
{
	int uid, euid;

	if((fd = open("/dev/kmem", O_RDONLY)) == -1) {
		fprintf(stderr, "failed to open kmem: %s\n", strerror(errno));
		return -1;
	}

	/* if we're setuid root to access kmem, drop priviledges now */
	if((euid = geteuid()) == 0 && (uid = getuid()) != euid) {
		printf("dropping priviledges uid %d -> %d\n", euid, uid);
		seteuid(uid);
	}

	if((avenrun_offs = sysmp(MP_KERNADDR, MPKA_AVENRUN)) == -1) {
		fprintf(stderr, "failed to retrieve load avg offset in kmem: %s\n",
				strerror(errno));
		close(fd);
		fd = -1;
		return -1;
	}

	return 0;
}

void load_update(void)
{
	int val;

	lseek(fd, avenrun_offs, SEEK_SET);
	if(read(fd, &val, sizeof val) < sizeof val) {
		return;
	}

	smon.loadavg[0] = (float)val / 1000.0f;
}
