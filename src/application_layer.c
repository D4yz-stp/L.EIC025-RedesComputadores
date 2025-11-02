// Application layer protocol implementation

#include "application_layer.h"
#include "link_layer.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h> // strlen
#include <sys/stat.h>
#include <stdbool.h>

// Control packet types
#define C_START 1
#define C_DATA  2
#define C_END   3

#define T_FILE_SIZE 0
#define T_FILE_NAME 1

int buildControlPacket( unsigned char *packet, unsigned char controlType, const char *filename, long long fileSize)
{
    int index = 0;

    packet[index++] = controlType;

    packet[index++] = T_FILE_SIZE;
    packet[index++] = 8;
    memcpy(&packet[index], &fileSize, 8);
    index += 8;

    packet[index++] = T_FILE_NAME;
    packet[index++] = strlen(filename);
    memcpy(&packet[index], filename, strlen(filename));
    index += strlen(filename);

    return index;

}

int buildDataPacket(unsigned char *packet, unsigned char *data, int dataSize) {
    packet[0] = C_DATA;

    packet[1] = dataSize / 256;  // L2 MSB
    packet[2] = dataSize % 256;  // L1 LSB
    /*
        Data
    */
    memcpy(&packet[3], data, dataSize);
    
    return 3 + dataSize; 
}


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
    
    if (correct_Open != -1) {
        // Connection established

        if (roleLink == LlTx) {
            /*
                Opening the File
            */
           FILE *file = fopen(filename, "rb");
            if (!file) {
                perror("fopen");
                return;
            }
            /*
                File Size assignment
            */
            struct stat st;
            if (stat(filename, &st) != 0){
                perror("Error getting the size of the filename\n");
                return;
            }
            long long int fileSize = st.st_size;

            unsigned char packet[MAX_PAYLOAD_SIZE];
            unsigned char fileBuffer[MAX_PAYLOAD_SIZE - 3]; // Without C , L1 , L2
            long long int bytesSum = 0;
            int sequenceNumber = 0; // Not really needed - Optional
            bool error = FALSE;    
            int packetSize;

            /*
                Start Control Packet
            */
            packetSize =  buildControlPacket(packet, C_START, "penguin-received.gif", fileSize);
            int isWriten = llwrite(packet, packetSize);
            if ( isWriten < 0 ){
                printf("TX: Error in the llwrite Start\n");
                if(fclose(file) < 0){
                    perror("TX: Closing File in Start Control Packet");
                }
                return;
            }
            printf("TX: Start Control Packet sent\n");

            /*
                Data packets
            */
            printf("\nTX: Starting file transfer...\n");

            while(!error){
                int bytesRead = fread(fileBuffer, 1, sizeof(fileBuffer), file);
                
                if ( bytesRead <= 0 ){
                    if (feof(file)) {
                        printf("TX: End of file reached\n");
                        break;
                    } else {
                        perror("fread");
                        error = TRUE;
                        break;
                    }
                }

                bytesSum+= bytesRead;
                sequenceNumber++;

                packetSize = buildDataPacket(packet, fileBuffer, bytesRead);

                isWriten = llwrite(packet, packetSize);
                if( isWriten < 0){
                    printf("TX: Error in writing DATA\n");
                    error = TRUE;
                    break;
                }

                printf("TX: Progress: %lld/%lld bytes (%.1f%%)\n", bytesSum, fileSize, (bytesSum * 100.0) / fileSize);
            }

            // Check for errors
            if (error) {
                printf("\nTX: ERROR - File transfer failed\n");
                if(fclose(file) < 0){
                    perror("TX: Closing File in Start Control Packet");
                }
                if(llclose() < 0){
                    printf("TX: Error on llclose in Data Transfer handler error\n");
                }
                return;
            }
            
            // Verify all bytes were sent
            if (bytesSum != fileSize) {
                printf("TX: WARNING -> Bytes sent (%lld) != file size (%lld)\n", bytesSum, fileSize);
            }
            /*
                End Control Packet
            */

            packetSize =  buildControlPacket(packet, C_END, "penguin-received.gif", bytesSum);
            isWriten = llwrite(packet, packetSize);
            if ( isWriten < 0 ){
                printf("TX: Error in the llwrite End\n");
                error = TRUE;
            }
            printf("TX: End Control Packet sent\n");

            fclose(file);
            if (llclose() < 0) {
                printf("ERROR: Failed to close connection\n");
                return;
            }
            printf("TX: Connection closed\n");

        } 
        else {
            unsigned char packet[MAX_PAYLOAD_SIZE];
            FILE *file = NULL;
            long long int fileSize = 0;
            long long int bytesReceived = 0; 
            int sequenceNumber = 0; // Not really needed - Optional
            bool transferComplete = FALSE;  
            bool error = FALSE;    
            char rxfilename[256] = {0};       

            /*
                We keep reading packets till the end pakcet or an error
            */
            while (!transferComplete && !error) { 
                
                printf("Rx: Waiting for next packet...\n");
                int bytesRead = llread(packet);
                if (bytesRead < 0) {
                    printf("RX: ERROR -> llread failed\n");
                    error = TRUE;
                    break;
                }
                
                unsigned char C = packet[0];
                printf("Rx: Received packet type: %d\n", C);
                
                switch (C) {
                    case C_START:
                        
                        /*
                            Start Control Packet
                        */
                        printf("RX: Start Control Packet recived\n");
                        int index = 1;
                        while (index < bytesRead) {
                            unsigned char T = packet[index++];
                            unsigned char L = packet[index++];
                            
                            if (T == T_FILE_SIZE) { 
                                /*
                                File Size
                                */
                                memcpy(&fileSize, &packet[index], L);
                                printf("    File size: %lld bytes\n", fileSize);
                            } 
                            else if (T == T_FILE_NAME) {
                                /*
                                    File Name
                                */
                                memcpy(&rxfilename, &packet[index], L);
                                rxfilename[L] = '\0';
                                printf("RX: File name is \"%s\"\n", rxfilename);
                                /*
                                    It should create the file with the rxfilename 
                                    or Destroy the existing file with the rxfilename and have a brand file named rxfilename
                                */
                                file = fopen(rxfilename, "wb");
                                if (!file) {
                                    perror("fopen");
                                    error = TRUE;
                                    break;
                                }
                            }
                            /*
                                Advancing L characters that were mentioned above
                            */
                            index += L;
                        }
                        break;
                        
                    case C_DATA:  
                        /*
                            Data Packet
                        */
                        printf("RX: Data packet recived\n");
                        
                        if (!file) {
                            printf("RX: ERROR -> Received DATA before START!\n");
                            error = TRUE;
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
                            error = TRUE;
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
                
                    case C_END:  
                         /*
                            End Control Packet
                        */
                        printf("RX: End control packet recived\n");

                        index = 1;
                        long long int endFileSize = 0;
                        char endrxfilename[256] = {0};
                        while (index < bytesRead) {
                            unsigned char T = packet[index++];
                            unsigned char L = packet[index++];
                            
                            if (T == T_FILE_SIZE) {
                                memcpy(&endFileSize, &packet[index], L);
                            }
                            else if (T == T_FILE_NAME) {
                                memcpy(&endrxfilename, &packet[index], L);
                                endrxfilename[L] = '\0';
                            }
                            index += L;
                        }
                        /*
                            It should be equal to the Start
                        */
                        if (endFileSize != fileSize) {
                            printf("RX: ERROR -> END file size (%lld) != START file size (%lld)\n", endFileSize, fileSize);
                            error = TRUE;
                        } 
                        else if(strcmp(endrxfilename, filename) != 0){
                            printf("RX: ERROR -> END filename != filename\n");
                            error = TRUE;   
                        }
                        else if(strcmp(rxfilename, filename) != 0){
                            printf("RX: ERROR -> START filename != filename\n");
                            error = TRUE;   
                        }
                        else if (bytesReceived != fileSize) {
                            printf("RX: ERROR -> Bytes received (%lld) != expected (%lld)\n", bytesReceived, fileSize);
                            error = TRUE;
                        } 
                        printf("\n========================================\n\n");
                        printf("RX: File donwloaded with sucess\n");
                        printf("RX: Total bytes: %lld\n", bytesReceived);
                        printf("RX: Data packets: %d\n", sequenceNumber);
                        printf("\n========================================\n\n");

                        transferComplete = TRUE; 
                        if (llclose() < 0) {
                            printf("ERROR: Failed to close connection\n");
                            return;
                        }
                        printf("RX: Connection closed\n");
                        break;
                        
                    default:
                        printf("RX: ERROR -> Unknown packet type: \"%d\"\n", C);
                        error = TRUE;
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

