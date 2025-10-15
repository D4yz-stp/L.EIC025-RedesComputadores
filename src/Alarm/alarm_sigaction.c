#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "alarm_sigaction.h"

int alarmEnabled = FALSE;
int alarmCount = 0;
struct sigaction act = {0};

// Alarm function handler.
// This function will run whenever the signal SIGALRM is received.
void alarmHandler(int signal)
{
    alarmEnabled = FALSE;
    alarmCount++;

    printf("Alarm #%d received\n", alarmCount);
}

int setupAlarm(){
    // Set alarm function handler.
    // Install the function signal to be automatically invoked when the timer expires,
    // invoking in its turn the user function alarmHandler
    act.sa_handler = &alarmHandler;
    if (sigaction(SIGALRM, &act, NULL) == -1)
    {
        perror("sigaction");
        return -1;
    }

    printf("Alarm configured\n");

}
