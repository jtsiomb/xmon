#include <stdlib.h>
#include "xmon.h"

int load_init(void)
{
	return 0;
}

void load_update(void)
{
	double val[3];

	if(getloadavg(val, 3) == -1) {
		return;
	}

	smon.loadavg[0] = val[0];
	smon.loadavg[1] = val[1];
	smon.loadavg[2] = val[2];
}
