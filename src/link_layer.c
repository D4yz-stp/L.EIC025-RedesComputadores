// Link layer protocol implementation

#include "link_layer.h"
#include "serial_port.h"
#include "stdbool.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "alarm_sigaction.h"

#define SUFrame_SIZE 5
#define I_HEADER_SIZE 4

// S/U Frame
#define FLAG 0x7E
#define A_TX 0x03
#define A_RX 0x01

// Control field values (C)
#define C_SET  0x03
#define C_UA   0x07
#define C_DISC 0x0B
#define C_RR0  0x05
#define C_RR1  0x85
#define C_REJ0 0x01
#define C_REJ1 0x81

// I-Frame Control Field (C) Values
#define C_I0 0x00
#define C_I1 0x40

// Escape byte for byte stuffing
#define ESC 0x7D

// Global variables
int Ns = 0;
int Nr = 0;
extern int fd;
extern int alarmEnabled;
extern int alarmCount;

// State machine
enum State_SU{START, FLAG_RCV, A_RCV, C_RCV, BCC1_OK, STOP};

//===============================================
// BUILD FRAMES
//===============================================

void buildSUFrame(unsigned char *frame, unsigned char address, unsigned char control)
{
    frame[0] = FLAG;
    frame[1] = address;
    frame[2] = control;
    frame[3] = address ^ control;
    frame[4] = FLAG;
}

int buildIFrame(unsigned char *frame, const unsigned char *data, int dataSize)
{
    int idx = 0;
    unsigned char C_Field = (Ns == 0) ? C_I0 : C_I1;
    
    // Header
    frame[idx++] = FLAG;
    frame[idx++] = A_TX;
    frame[idx++] = C_Field;
    frame[idx++] = A_TX ^ C_Field;
    
    // Calculate BCC2
    unsigned char bcc2 = 0;
    for (int i = 0; i < dataSize; i++) {
        bcc2 ^= data[i];
    }
    
    // Byte stuffing on payload + BCC2
    unsigned char tempBuffer[MAX_PAYLOAD_SIZE + 1];
    memcpy(tempBuffer, data, dataSize);
    tempBuffer[dataSize] = bcc2;
    
    for (int i = 0; i < dataSize + 1; i++) {
        if (tempBuffer[i] == FLAG || tempBuffer[i] == ESC) {
            frame[idx++] = ESC;
            frame[idx++] = tempBuffer[i] ^ 0x20;
        } else {
            frame[idx++] = tempBuffer[i];
        }
    }
    
    frame[idx++] = FLAG;
    
    return idx;
}

//===============================================
// WRITE TO SERIAL PORT WITH ALARM
//===============================================

int writeToSerialPort(unsigned char *frame, int frameSize, int timeout, int *nRetransmissions)
{
    if (*nRetransmissions <= 0) {
        printf("ERROR: Maximum retransmissions reached.\n");
        return -1;
    }
    
    if (!alarmEnabled) {
        writeBytesSerialPort(frame, frameSize);
        printf("Frame sent. Waiting for response... (retransmissions left: %d)\n", *nRetransmissions);
        
        alarm(timeout);
        alarmEnabled = TRUE;
        return 0;
    }
    
    return 0;
}

//===============================================
// LLOPEN
//===============================================

int llopen(LinkLayer connectionParameters)
{
    fd = openSerialPort(connectionParameters.serialPort, connectionParameters.baudRate);
    if (fd < 0) {
        perror("openSerialPort");
        return -1;
    }
    
    if (connectionParameters.role == LlTx) {
        if (setupAlarm() < 0) {
            perror("setupAlarm");
            return -1;
        }
    }

    if (connectionParameters.role == LlTx) {
        printf("TX: Sending SET frame...\n");
        
        unsigned char frameTx[SUFrame_SIZE];
        buildSUFrame(frameTx, A_TX, C_SET);
        
        int nRetransmissions = connectionParameters.nRetransmissions;
        int timeout = connectionParameters.timeout;
        
        enum State_SU state = START;
        unsigned char byte;
        
        alarmEnabled = FALSE;
        alarmCount = 0;
        
        while (nRetransmissions > 0) {
            writeToSerialPort(frameTx, SUFrame_SIZE, timeout, &nRetransmissions);
            
            state = START;
            
            while (state != STOP && alarmEnabled) {
                if (readByteSerialPort(&byte) > 0) {
                    switch(state) {
                        case START:
                            if (byte == FLAG) state = FLAG_RCV;
                            break;
                        case FLAG_RCV:
                            if (byte == FLAG) state = FLAG_RCV;
                            else if (byte == A_TX) state = A_RCV;
                            else state = START;
                            break;
                        case A_RCV:
                            if (byte == FLAG) state = FLAG_RCV;
                            else if (byte == C_UA) state = C_RCV;
                            else state = START;
                            break;
                        case C_RCV:
                            if (byte == FLAG) state = FLAG_RCV;
                            else if (byte == (A_TX ^ C_UA)) state = BCC1_OK;
                            else state = START;
                            break;
                        case BCC1_OK:
                            if (byte == FLAG) {
                                state = STOP;
                                alarm(0);
                                alarmEnabled = FALSE;
                                printf("TX: UA received. Connection established.\n");
                                Ns = 0;
                                printf(" \n fd do tx - >\"%d\" \n",fd);
                                return fd;
                            }
                            else state = START;
                            break;
                        case STOP:
                            break;
                    }
                }
            }
            
            if (!alarmEnabled) {
                nRetransmissions--;
                printf("TX: Timeout! Retrying...\n");
            }
        }
        
        printf("TX: ERROR - Failed to establish connection after all retries.\n");
        closeSerialPort();
        return -1;
        
    } else {
        printf("RX: Waiting for SET frame...\n");
        
        enum State_SU state = START;
        unsigned char byte;
        
        while (state != STOP) {
            if (readByteSerialPort(&byte) > 0) {
                switch(state) {
                    case START:
                        if (byte == FLAG) state = FLAG_RCV;
                        break;
                    case FLAG_RCV:
                        if (byte == FLAG) state = FLAG_RCV;
                        else if (byte == A_TX) state = A_RCV;
                        else state = START;
                        break;
                    case A_RCV:
                        if (byte == FLAG) state = FLAG_RCV;
                        else if (byte == C_SET) state = C_RCV;
                        else state = START;
                        break;
                    case C_RCV:
                        if (byte == FLAG) state = FLAG_RCV;
                        else if (byte == (A_TX ^ C_SET)) state = BCC1_OK;
                        else state = START;
                        break;
                    case BCC1_OK:
                        if (byte == FLAG) state = STOP;
                        else state = START;
                        break;
                    case STOP:
                        break;
                }
            }
        }
        
        printf("RX: SET received. Sending UA...\n");
        
        unsigned char frameTx[SUFrame_SIZE];
        buildSUFrame(frameTx, A_TX, C_UA);
        
        if (writeBytesSerialPort(frameTx, SUFrame_SIZE) < 0) {
            perror("writeBytesSerialPort - UA");
            return -1;
        }
        
        Nr = 0;
        printf(" \n fd do rx - >\"%d\" \n",fd);
        return fd;
    }
}

//===============================================
// LLWRITE
//===============================================

int llwrite(const unsigned char *buf, int bufSize)
{
    unsigned char frameTx[MAX_PAYLOAD_SIZE * 2 + 10];
    int frameSize = buildIFrame(frameTx, buf, bufSize);
    
    int nRetransmissions = 3;
    int timeout = 3;
    
    enum State_SU state = START;
    unsigned char byte;
    unsigned char expectedRR = (Ns == 0) ? C_RR1 : C_RR0;
    
    alarmEnabled = FALSE;
    alarmCount = 0;
    
    while (nRetransmissions > 0) {
        writeToSerialPort(frameTx, frameSize, timeout, &nRetransmissions);
        printf("TX: I-Frame sent (Ns=%d). Waiting for RR... (retries left: %d)\n", Ns, nRetransmissions);
        
        state = START;
        while (state != STOP && alarmEnabled) {
            if (readByteSerialPort(&byte) > 0) {
                switch(state) {
                    case START:
                        if (byte == FLAG) state = FLAG_RCV;
                        break;
                    case FLAG_RCV:
                        if (byte == FLAG) state = FLAG_RCV;
                        else if (byte == A_TX) state = A_RCV;
                        else state = START;
                        break;
                    case A_RCV:
                        if (byte == FLAG) state = FLAG_RCV;
                        else if (byte == expectedRR || byte == C_REJ0 || byte == C_REJ1) {
                            state = C_RCV;
                        }
                        else state = START;
                        break;
                    case C_RCV:
                        if (byte == FLAG) state = FLAG_RCV;
                        else if (byte == (A_TX ^ expectedRR)) {
                            state = BCC1_OK;
                        }
                        else state = START;
                        break;
                    case BCC1_OK:
                        if (byte == FLAG) {
                            state = STOP;
                            alarm(0);
                            alarmEnabled = FALSE;
                            printf("TX: RR received. Frame acknowledged.\n");
                            Ns = 1 - Ns;
                            return frameSize;
                        }
                        else state = START;
                        break;
                    case STOP:
                        break;
                }
            }
        }
        
        if (!alarmEnabled) {
            nRetransmissions--;
            printf("TX: Timeout or REJ! Retransmitting...\n");
        }
    }
    
    printf("TX: ERROR - Failed to send I-Frame after all retries.\n");
    return -1;
}

//===============================================
// LLREAD
//===============================================

int llread(unsigned char *packet)
{
    enum State_SU state = START;
    unsigned char byte;

    // buffer para a data (payload) extraída do I-Frame´
    // Buffer to the payload (data) extracted into the I-Frame
    unsigned char dataBuffer[MAX_PAYLOAD_SIZE * 2];
    int dataIdx = 0;

    // Control variables 
    unsigned char expectedC = (Nr == 0) ? C_I0 : C_I1;
    unsigned char currentC = 0;
    int escaped = 0;
    
    while (state != STOP) {
        int isSuccess = readByteSerialPort(&byte);
        if (isSuccess > 0) {
            switch(state) {
                case START:
                    if (byte == FLAG) state = FLAG_RCV;
                    break;
                case FLAG_RCV:
                    if (byte == FLAG) state = FLAG_RCV;
                    else if (byte == A_TX) state = A_RCV;
                    else state = START;
                    break;
                case A_RCV:
                    if (byte == FLAG) state = FLAG_RCV;
                    else {
                        currentC = byte;
                        state = C_RCV;
                    }
                    break;
                case C_RCV:
                    if(byte == FLAG) {state = FLAG_RCV;}
                    else if (byte == (A_TX ^ currentC)) {
                        // Verification to see if its the awaited I-frame (C_I0 ou C_I1)
                        if (currentC == expectedC || currentC == (1-expectedC)) {
                            state = BCC1_OK;
                            // If its duplicated, we process and send RR
                        } else {
                            state = START;
                        }
                    }
                    else {state = START;}
                    break;
                case BCC1_OK:
                    if (byte == FLAG) {
                        // If we recive a FLAG without data, BCC2 was not recived
                        if (dataIdx == 0) {
                            state = FLAG_RCV;
                            break;
                        }
                        
                        unsigned char receivedBCC2 = dataBuffer[dataIdx - 1];
                        dataIdx--;
                        
                        unsigned char calculatedBCC2 = 0;
                        for (int i = 0; i < dataIdx; i++) {
                            calculatedBCC2 ^= dataBuffer[i];
                        }
                        
                        if (receivedBCC2 == calculatedBCC2 && currentC == expectedC) {
                            // Valid data
                            memcpy(packet, dataBuffer, dataIdx);
                            
                            unsigned char rrFrame[SUFrame_SIZE];
                            unsigned char rrControl = (Nr == 0) ? C_RR1 : C_RR0;
                            
                            buildSUFrame(rrFrame, A_TX, rrControl);
                            writeBytesSerialPort(rrFrame, SUFrame_SIZE);
                            
                            printf("RX: I-Frame received (Nr=%d). Sent RR%d.\n", Nr, 1 - Nr);
                            Nr = 1 - Nr;
                            return dataIdx;
                        } else {
                            unsigned char rejFrame[SUFrame_SIZE];
                            unsigned char rejControl = (Nr == 0) ? C_REJ0 : C_REJ1;
                            buildSUFrame(rejFrame, A_TX, rejControl);
                            writeBytesSerialPort(rejFrame, SUFrame_SIZE);
                            
                            printf("RX: Frame error. Sent REJ%d.\n", Nr);
                            dataIdx = 0;
                            state = START;
                        }
                    }
                    else if (byte == ESC) {
                        escaped = 1;
                    }
                    else {
                        if (escaped) {
                            dataBuffer[dataIdx++] = byte ^ 0x20;
                            escaped = 0;
                        } else {
                            if (dataIdx < MAX_PAYLOAD_SIZE) {
                                dataBuffer[dataIdx++] = byte;
                            } else {
                                // Buffer overflow, Frame discarded
                                printf("RX: Data buffer overflow. Restarting.\n");
                                dataIdx = 0;
                                state = START;
                            }
                        }
                    }
                    break;
                case STOP:
                    break;
            }
        }
    }
    return -1;
}

//===============================================
// LLCLOSE
//===============================================

int llclose()
{
    // TODO: Implement DISC/UA handshake
    
    if (closeSerialPort() < 0) {
        perror("closeSerialPort");
        exit(-1);
    }
    printf("Connection closed.\n");
    return 0;
}