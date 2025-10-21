// Application layer protocol implementation

#include "application_layer.h"
#include "link_layer.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h> // Para strlen

void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename)
{
    // A variável 'Text' já não é usada, pode ser removida (evita warning)
    
    // CORREÇÃO: A lógica para determinar o role
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
    
    if (correct_Open == 0) {
        // Ligação estabelecida (Handshake SET/UA concluído)

        if (roleLink == LlTx) {
            // LÓGICA DO TRANSMISSOR: Enviar a string de teste
            const char *test_string = "Ola mundo, esta e a minha primeira string enviada com I-Frame!";
            int len = strlen(test_string);

            printf("TX: Sending string of size %d...\n", len);
            
            

            printf("TX: llwrite finished. Bytes written: %d\n", bytes_written);
            
            // Fechar a ligação
            llclose();
        } 
        else {
            // LÓGICA DO RECETOR: Esperar pelo I-Frame
            unsigned char received_data[MAX_PAYLOAD_SIZE];
            
            printf("RX: Waiting for I-Frame...\n");
            
            // llread irá bloquear até receber um I-Frame (ou falhar)
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
        printf("llopen FAILED for role %s\n", role);
    }
}