#include <windows.h>
#include "xmon.h"

struct memstatex {
	unsigned __int32 length;
	unsigned __int32 mem_load;
	unsigned __int64 total_phys, avail_phys;
	unsigned __int64 total_pgfile, avail_pgfile;
	unsigned __int64 total_virt, avail_virt, avail_ext_virt;
};

typedef BOOL (WINAPI *globmemstatex_func)(struct memstatex*);
static globmemstatex_func gmemstatex;

int mem_init(void)
{
	/* try to find the modern GlobalMemoryStatusEx variant */
	HINSTANCE k32dll = GetModuleHandle("kernel32.dll");
	if(k32dll) {
		gmemstatex = (globmemstatex_func)GetProcAddress(k32dll, "GlobalMemoryStatusEx");
	}

	mem_update();
	return 0;
}

void mem_update(void)
{
	if(gmemstatex) {
		struct memstatex msx;
		gmemstatex(&msx);
		smon.mem_total = (unsigned long)(msx.total_phys >> 10);
		smon.mem_free = (unsigned long)(msx.avail_phys >> 10);
	} else {
		MEMORYSTATUS ms;
		GlobalMemoryStatus(&ms);
		smon.mem_total = ms.dwTotalPhys >> 10;
		smon.mem_free = ms.dwAvailPhys >> 10;
	}
}
