#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "alarm_sigaction.h"

// Definição das macros (para garantir que FALSE/TRUE existem)
#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif


// DEFINIÇÃO DAS VARIÁVEIS GLOBAIS.
// Isto resolve o erro do linker "symbol(s) not found".
int alarmEnabled = FALSE;
int alarmCount = 0;
struct sigaction act = {0};


// Alarm function handler.
// This function will run whenever the signal SIGALRM is received.
void alarmHandler(int signal)
{
    alarmEnabled = FALSE;
    alarmCount++;

    printf("Alarm #%d received\\n", alarmCount);
}

int setupAlarm(){
    // Set alarm function handler.
    act.sa_handler = &alarmHandler;
    if (sigaction(SIGALRM, &act, NULL) == -1)
    {
        perror("sigaction");
        return -1;
    }

    printf("Alarm configured\\n");

    return 0; // Adicionado return 0 para corrigir o erro anterior
}