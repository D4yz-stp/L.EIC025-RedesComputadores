// Application layer protocol implementation

#include "application_layer.h"
#include "link_layer.h"
#include <stdio.h>

void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename)
{
    char Text[] = "Ola";
    
    LinkLayerRole roleLink;
    roleLink = (strcmp(role == "tx")) ? LlTx : LlRx;
    
    LinkLayer linkLayer = {
        .role = roleLink,
        .baudRate = baudRate,
        .nRetransmissions = nTries,
        .timeout = timeout
    };
    strncpy(linkLayer.serialPort, serialPort, 50);
    linkLayer.serialPort[49] = '\0';

    int correct_Open = llopen(linkLayer);
}   
