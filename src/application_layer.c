// Application layer protocol implementation

#include "application_layer.h"
#include "link_layer.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h> // strlen

void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename)
{
    // Determination of the Role
    LinkLayerRole roleLink;
    roleLink = (strcmp(role, "tx") == 0) ? LlTx : LlRx;
    
    LinkLayer linkLayer = {
        .role = roleLink,
        .baudRate = baudRate,
        .nRetransmissions = nTries,
        .timeout = timeout
    };
    
    strncpy(linkLayer.serialPort, serialPort, 50);
    linkLayer.serialPort[49] = '\0';

    int correct_Open = llopen(linkLayer);
    
    if (correct_Open != -1) {
        // Connection established

        if (roleLink == LlTx) {
            // TRANSMITTER LOGIC
            const char *test_string = "Ola mundo, Tu gostas de mandioca ?";
            int len = strlen(test_string);

            printf("TX: Sending string of size %d...\n", len);
            
            
            int bytes_written = llwrite((const unsigned char *)test_string, len);

            printf("TX: llwrite finished. Bytes written: %d\n", bytes_written);
            
            // To Finish the connection
            llclose();
        } 
        else {
            // RECEIVER LOGIC
            unsigned char received_data[MAX_PAYLOAD_SIZE];
            
            printf("RX: Waiting for I-Frame...\n");
            
            int bytes_read = llread(received_data);

            if (bytes_read > 0) {
                // Adiciona o terminador de string para que possa ser impressa
                received_data[bytes_read] = '\0'; 
                printf("RX: llread finished. Bytes read: %d\n", bytes_read);
                printf("RX: Received Data: \"%s\"\n", received_data);
            } else {
                printf("RX: llread failed or received 0 bytes.\n");
            }
            
            llclose();
        }
    } else {
        // Ligação não estabelecida
        printf("llopen FAILED for role %s\n", role);
    }
}
