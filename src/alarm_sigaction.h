#ifndef ALARM_H
#define ALARM_H

#include <signal.h>

extern int alarmEnabled;
extern int alarmCount;

void alarmHandler(int signal);
int setupAlarm(void);

#endif
