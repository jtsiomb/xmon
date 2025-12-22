#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include "xmon.h"

typedef struct {
	LARGE_INTEGER idle;
	LARGE_INTEGER kernel;
	LARGE_INTEGER user;
	LARGE_INTEGER rsvd1[2];
	unsigned long rsvd2;
} SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION;

enum sys_info_class { SYS_PROC_PERF_INFO = 8 };

typedef int (WINAPI *ntqsysinfo_func)(enum sys_info_class, void*, unsigned long, unsigned long*);

static HINSTANCE ntdll;
static ntqsysinfo_func NtQuerySystemInformation;

static SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION *sppinfo, *prev;
static unsigned long sppinfo_size;

static PERF_DATA_BLOCK *perfbuf;
static int perfbuf_size;


static int init_ntquery(void);
static int init_perfdata(void);


int cpu_init(void)
{
	SYSTEM_INFO si;

	GetSystemInfo(&si);

	smon.num_cpus = si.dwNumberOfProcessors;
	if(!(smon.cpu = calloc(smon.num_cpus, sizeof *smon.cpu))) {
		char buf[256];
		sprintf(buf, "Failed to allocate memory for per-cpu usage (%u)", smon.num_cpus);
		MessageBox(0, buf, "Fatal", MB_OK);
		return -1;
	}

	/* looking for CPU stat gathering methods should be the last thing
	 * in this function because the first available one will return success
	 * immediately, leaving the fallthrough case for failure.
	 */

	/* Try NtQuerySystemInformation, should be available on all NT versions */
	if(init_ntquery() != -1) {
		return 0;
	}

	/* Try querying the performance counters block */
	if(init_perfdata() != -1) {
		return 0;
	}

	/* we found no supported CPU stat gathering method */
	return -1;
}

static int init_ntquery(void)
{
	unsigned int i;
	unsigned long retsz;

	if(!(ntdll = LoadLibrary("ntdll.dll"))) {
		return -1;
	}

	NtQuerySystemInformation = (ntqsysinfo_func)GetProcAddress(ntdll, "NtQuerySystemInformation");
	if(!NtQuerySystemInformation) {
		return -1;
	}
	sppinfo_size = sizeof *sppinfo * smon.num_cpus;
	if(!(sppinfo = malloc(sppinfo_size * 2))) {
		MessageBox(0, "Failed to allocate cpu usage query buffer", "Fatal", MB_OK);
		FreeLibrary(ntdll);
		return -1;
	}
	prev = sppinfo + smon.num_cpus;

	for(;;) {
		NtQuerySystemInformation(SYS_PROC_PERF_INFO, sppinfo, sppinfo_size, &retsz);
		if(retsz <= sppinfo_size) break;

		sppinfo_size <<= 1;
		free(sppinfo);
		if(!(sppinfo = malloc(sppinfo_size * 2))) {
			MessageBox(0, "Failed to allocate cpu usage query buffer", "Fatal", MB_OK);
			FreeLibrary(ntdll);
			return -1;
		}
		prev = sppinfo + smon.num_cpus;
	}

	for(i=0; i<smon.num_cpus; i++) {
		prev[i] = sppinfo[i];
	}

	return 0;
}

static int init_perfdata(void)
{
	long res;

	perfbuf_size = 4096;
	for(;;) {
		if(!(perfbuf = malloc(perfbuf_size))) {
			MessageBox(0, "Failed to allocate performance counter buffer", "Fatal", MB_OK);
			return -1;
		}
		if((res = RegQueryValueEx(HKEY_PERFORMANCE_DATA, 0, 0, 0,
					(unsigned char*)perfbuf, &perfbuf_size)) == 0) {
			break;
		}
		free(perfbuf);
		if(res != ERROR_MORE_DATA) {
			return -1;
		}
		perfbuf_size <<= 1;
	}

	return 0;
}

void cpu_update(void)
{
	unsigned int i;

	if(NtQuerySystemInformation) {
		int status;
		unsigned long retsz;
		unsigned __int64 idle, work, sum, allidle, allsum;

		status = NtQuerySystemInformation(SYS_PROC_PERF_INFO, sppinfo, sppinfo_size, &retsz);

		allidle = allsum = 0;
		for(i=0; i<smon.num_cpus; i++) {
			idle = sppinfo[i].idle.QuadPart - prev[i].idle.QuadPart;
			work = (sppinfo[i].user.QuadPart - prev[i].user.QuadPart) +
					(sppinfo[i].kernel.QuadPart - prev[i].kernel.QuadPart);
			sum = /*idle + */work;
			smon.cpu[i] = 128 - (unsigned int)((idle << 7) / sum);
			prev[i] = sppinfo[i];

			allidle += idle;
			allsum += sum;
		}

		smon.single = 128 - (unsigned int)((allidle << 7) / allsum);
	}
}
