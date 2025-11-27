#ifndef SYSMON_H_
#define SYSMON_H_

struct sysmon {
	int single;
	int *cpu;
	int num_cpus;
};

extern struct sysmon smon;

int sysmon_init(void);
void sysmon_update(void);

#endif	/* SYSMON_H_ */
