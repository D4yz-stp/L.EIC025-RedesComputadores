// Application layer protocol implementation

#include "application_layer.h"
#include "link_layer.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h> // strlen
#include <sys/stat.h>
#include <stdbool.h>

void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename)
{
    printf("The file name needs to have less than 256 characters");

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

    // File Size assignment
    struct stat st;
    if (stat(strcat( "..",filename), &st) != 0){
        perror("Error getting the size of the filename\n");
        return -1;
    }

    long fileSize = st.st_size;
    
    if (correct_Open != -1) {
        // Connection established

        if (roleLink == LlTx) {
            // Control Packet (START)
            unsigned char controlPacket[MAX_PAYLOAD_SIZE];
            int index = 0;

            // Control filed
            controlPacket[index++] = 1;

            // TLV Fileds
            // File Size
            controlPacket[index++] = 0;
            controlPacket[index++] = 8; // 8 bytes
            memcpy(&controlPacket[index], &fileSize, 8);
            index += 8;

            // File Name
            controlPacket[index++] = 1;
            controlPacket[index++] = strlen(filename);
            memcpy(&controlPacket[index], filename, strlen(filename));
            index += strlen(filename);

            printf("TX: Sending the Start Control Packet\n");
            int isWritten = llwrite(controlPacket, index);
            if ( isWritten <= 0){
                return -1;
            }



            printf("TX: Sending string of size %d...\n", len);
            
            
            int bytes_written = llwrite((const unsigned char *)test_string, len);

            printf("TX: llwrite finished. Bytes written: %d\n", bytes_written);
            
            // To Finish the connection
            llclose();
        } 
        else {
            unsigned char packet[MAX_PAYLOAD_SIZE];
            FILE *file = NULL;
            long long int fileSize = 0;
            long long int bytesReceived = 0; // Not kinda needed - Optional
            int sequenceNumber = 0; // Not really needed - Optional
            bool transferComplete = false;  
            bool error = false;           

            /*
                We keep reading packets till the end pakcet or an error
            */
            while (!transferComplete && !error) { 
                
                printf("Rx: Waiting for next packet...\n");
                int bytesRead = llread(packet);
                if (bytesRead < 0) {
                    printf("RX: ERROR -> llread failed\n");
                    error = true;
                    break;
                }
                
                unsigned char C = packet[0];
                printf("Rx: Received packet type: %d\n", C);
                
                switch (C) {
                    case 1:
                        /*
                            Start Control Packet
                        */
                        printf("RX: Start Control Packet recived\n");
                        int index = 1;
                        while (index < bytesRead) {
                            unsigned char T = packet[index++];
                            unsigned char L = packet[index++];
                            
                            if (T == 0) { 
                                /*
                                File Size
                                */
                                memcpy(&fileSize, &packet[index], L);
                                printf("    File size: %lld bytes\n", fileSize);
                            } 
                            else if (T == 1) {
                                /*
                                    File Name
                                */
                                char filename[256] = {0};
                                memcpy(filename, &packet[index], L);
                                filename[L] = '\0';
                                printf("RX: File name is \"%s\"\n", filename);
                                /*
                                    It should create the file with the filename 
                                    or Destroy the existing file with the filename and have a brand file named filename
                                */
                                file = fopen(filename, "wb");
                                if (!file) {
                                    perror("fopen");
                                    error = true;
                                    break;
                                }
                            }
                            /*
                                Advancing L characters that were mentioned above
                            */
                            index += L;
                        }
                        break;

                    case 2:  
                        /*
                            Data Packet
                        */
                        printf("RX: Data packet recived\n");
                        
                        if (!file) {
                            printf("RX: ERROR -> Received DATA before START!\n");
                            error = true;
                            break;
                        }
                        /*
                            Math to get the K octets
                        */
                        int L2 = packet[1];
                        int L1 = packet[2];
                        int K = 256 * L2 + L1;
                        
                        size_t written = fwrite(&packet[3], 1, K, file);
                        if (written != K) {
                            printf("RX: ERROR ->  Failed to write data to file\n");
                            error = true;
                            break;
                        }
                        bytesReceived += K;
                        sequenceNumber++;
                        printf("RX: Data written: \"%d\" bytes\n", K);
                        /*
                            %lld -> long long int -> 1 long long int = GB
                        */
                        printf("RX: Progress: %lld/%lld (%.1f%%)\n", bytesReceived, fileSize, (bytesReceived * 100.0) / fileSize);
                        break;
                
                    case 3:  
                         /*
                            End Control Packet
                        */
                        printf("RX: End control packet recived\n");

                        index = 1;
                        long long int endFileSize = 0;
                        while (index < bytesRead) {
                            unsigned char T = packet[index++];
                            unsigned char L = packet[index++];
                            
                            if (T == 0) {
                                memcpy(&endFileSize, &packet[index], L);
                            }
                            index += L;
                        }
                        /*
                            It should be equal to the Start
                        */
                        if (endFileSize != fileSize) {
                            printf("RX: ERROR -> END file size (%lld) != START file size (%lld)\n", endFileSize, fileSize);
                            error = true;
                        } 
                        else if (bytesReceived != fileSize) {
                            printf("RX: ERROR -> Bytes received (%lld) != expected (%lld)\n", bytesReceived, fileSize);
                            error = true;
                        } 
                        printf("RX: File donwloaded with sucess");
                        printf("RX: Total bytes: %lld\n", bytesReceived);
                        printf("RX: Data packets: %d\n", sequenceNumber);

                        transferComplete = true; 
                        break;
                        
                    default:
                        printf("RX: ERROR -> Unknown packet type: \"%d\"\n", C);
                        error = true;
                        break;
                }
            }
        }
    } else {
        /*
            Connection not estabilished
        */
        printf("llopen FAILED for role %s\n", role);
    }
}

