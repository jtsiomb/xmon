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
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include "xmon.h"

struct battery {
	char *path, *file;
	int state;
	unsigned int cur, full, time;
};

#define MAX_BAT	64
static struct battery bat[MAX_BAT];
static unsigned int nbat;

#define SYSPATH_DIR		"/sys/class/power_supply"

static int upd_sysfs(struct battery *bat);
static long read_value(const char *path, long failval);
static void cleanstr(char *s);


int bat_init(void)
{
	int fd, len;
	unsigned int i;
	DIR *dir;
	struct dirent *dent;
	char buf[256], *path;

	if(!(dir = opendir(SYSPATH_DIR))) {
		return -1;
	}

	while((dent = readdir(dir))) {
		if(dent->d_name[0] == '.') continue;

		sprintf(buf, SYSPATH_DIR "/%s/type", dent->d_name);
		if((fd = open(buf, O_RDONLY)) == -1) {
			continue;
		}
		if((len = read(fd, buf, sizeof buf)) <= 0) {
			close(fd);
			continue;
		}
		buf[len] = 0;
		cleanstr(buf);

		if(strcmp(buf, "Battery") == 0) {
			if(!(path = malloc(strlen(SYSPATH_DIR) + strlen(dent->d_name) + 16))) {
				fprintf(stderr, "failed to allocate sysfs path buffer\n");
				close(fd);
				continue;
			}
			len = sprintf(path, SYSPATH_DIR "/%s/", dent->d_name);
			memset(bat + nbat, 0, sizeof *bat);
			bat[nbat].path = path;
			bat[nbat].file = path + len;
			nbat++;
		}
		close(fd);
	}

	for(i=0; i<nbat; i++) {
		strcpy(bat[i].file, "charge_full");
		if((fd = open(bat[i].path, O_RDONLY)) == -1) {
batfail:	free(bat[i].path);
			bat[i] = bat[--nbat];
			continue;
		}
		if((len = read(fd, buf, sizeof buf)) <= 0) {
			close(fd);
			goto batfail;
		}
		buf[len] = 0;
		bat[i].full = atoi(buf);
	}

	return 0;
}

void bat_update(void)
{
	unsigned int i, cur = 0, total = 0, tm = 0;

	if(nbat <= 0) return;

	for(i=0; i<nbat; i++) {
		upd_sysfs(bat + i);
		cur += bat[i].cur;
		total += bat[i].full;
		tm += bat[i].time;
	}

	smon.bat = cur * 100 / total;
	smon.bat_time = 0xffffffff;
	smon.bat_state = bat[0].state;
}

static int upd_sysfs(struct battery *bat)
{
	int fd, len;
	char buf[64];

	strcpy(bat->file, "charge_now");
	bat->cur = read_value(bat->path, 0);

	strcpy(bat->file, "status");
	if((fd = open(bat->path, O_RDONLY)) == -1) {
		return -1;
	}
	if((len = read(fd, buf, sizeof buf)) <= 0) {
		close(fd);
		return -1;
	}
	buf[len] = 0;
	cleanstr(buf);

	if(strcmp(buf, "Full") == 0) {
		bat->state = BAT_FULL;
	} else if(strcmp(buf, "Discharging") == 0) {
		bat->state = BAT_DIS;
	} else if(strcmp(buf, "Charging") == 0) {
		bat->state = BAT_CHG;
	}
	close(fd);

	return 0;
}

static long read_value(const char *path, long failval)
{
	long val;
	int fd, len;
	char buf[64];

	if((fd = open(path, O_RDONLY)) == -1) {
		return failval;
	}

	if((len = read(fd, buf, sizeof buf)) <= 0) {
		close(fd);
		return failval;
	}
	buf[len] = 0;
	val = atoi(buf);
	close(fd);
	return val;
}

static void cleanstr(char *s)
{
	char *ptr = s;
	while(isspace((unsigned char)*ptr)) ptr++;

	if(ptr > s) {
		memmove(s, ptr, strlen(ptr) + 1);
	}
	if(!*s) return;

	ptr = s + strlen(s) - 1;
	while(ptr >= s && isspace((unsigned char)*ptr)) *ptr-- = 0;
}
