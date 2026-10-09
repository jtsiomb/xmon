/*
xmon - a minimal lightweight cross-platform system monitoring utility
Copyright (C) 2025-2026  John Tsiombikas <nuclear@mutantstargoat.com>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/
#include <stdio.h>
#include <stdlib.h>
#include <sys/sysctl.h>
#include "xmon.h"

int bat_init(void)
{
	unsigned int nbat;
	size_t len;

	len = sizeof nbat;
	if(sysctlbyname("hw.acpi.battery.units", &nbat, &len, 0, 0) == -1 || nbat <= 0) {
		return -1;
	}
	bat_update();
	return 0;
}

void bat_update(void)
{
	int tm;
	unsigned int state;
	size_t len;

	len = sizeof smon.bat;
	sysctlbyname("hw.acpi.battery.life", &smon.bat, &len, 0, 0);

	len = sizeof tm;
	if(sysctlbyname("hw.acpi.battery.time", &tm, &len, 0, 0) == -1 || tm == -1) {
		smon.bat_time = 0xffffffff;
	} else {
		smon.bat_time = tm;
	}

	len = sizeof state;
	if(sysctlbyname("hw.acpi.battery.state", &state, &len, 0, 0) != -1) {
		if(state & 2) {
			smon.bat_state = BAT_CHG;
		} else if(state & 1) {
			smon.bat_state = BAT_DIS;
		} else {
			smon.bat_state = BAT_FULL;
		}
	}
}
