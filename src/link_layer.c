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
#define C_I1 0x80

// Escape byte for byte stuffing
#define ESC 0x7D

// Global variables
int Ns = 0;
int Nr = 0;
extern int fd;
extern int alarmEnabled;
extern int alarmCount;

// Global variables of the connection and roles
static LinkLayerRole globalRole;
static int globalTimeout;
static int globalNRetransmissions;


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
    
    // Overflow Inspection
    if (dataSize > MAX_PAYLOAD_SIZE) return -1;

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
        int bytesWritten = writeBytesSerialPort(frame, frameSize);
        if (bytesWritten != frameSize) {
            fprintf(stderr, "Erro: falha ao escrever frame (%d/%d bytes)\n", bytesWritten, frameSize);
            return -1;
        }

        /*
        printf("Frame sent. Waiting for response... (retransmissions left: %d)\n", *nRetransmissions);
        */

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

    globalRole = connectionParameters.role;
    globalTimeout = connectionParameters.timeout;
    globalNRetransmissions = connectionParameters.nRetransmissions;
    
    if (connectionParameters.role == LlTx) {
        if (setupAlarm() < 0) {
            perror("setupAlarm");
            return -1;
        }
    }

    if (connectionParameters.role == LlTx) {
        printf("TX: Sending SET frame...\n");
        
        unsigned char setFrame[SUFrame_SIZE];
        buildSUFrame(setFrame, A_TX, C_SET);
        
        int nRetransmissions = connectionParameters.nRetransmissions;
        int timeout = connectionParameters.timeout;
        
        enum State_SU state = START;
        unsigned char byte;
        
        alarmEnabled = FALSE;
        alarmCount = 0;
        
        while (nRetransmissions > 0) {
            writeToSerialPort(setFrame, SUFrame_SIZE, timeout, &nRetransmissions);
            
            state = START;
            
            while (state != STOP && alarmEnabled) {
                if (readByteSerialPort(&byte) > 0) {
                    switch(state) {
                        case START:
                            if (byte == FLAG) state = FLAG_RCV;
                            break;
                        case FLAG_RCV:
                            if (byte == FLAG) state = FLAG_RCV;
                            else if (byte == A_RX) state = A_RCV;
                            else state = START;
                            break;
                        case A_RCV:
                            if (byte == FLAG) state = FLAG_RCV;
                            else if (byte == C_UA) state = C_RCV;
                            else state = START;
                            break;
                        case C_RCV:
                            if (byte == FLAG) state = FLAG_RCV;
                            else if (byte == (A_RX ^ C_UA)) state = BCC1_OK;
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
                printf("TX: Timeout or REJ! Retransmitting...\n");
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
        
        unsigned char uaFrame[SUFrame_SIZE];
        buildSUFrame(uaFrame, A_RX, C_UA);
        
        if (writeBytesSerialPort(uaFrame, SUFrame_SIZE) < 0) {
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
    if (frameSize < 0) {
        fprintf(stderr, "Erro: buildIFrame falhou\n");
        return -1;
    }
    
    int nRetransmissions = globalNRetransmissions;
    int timeout = globalTimeout;

    enum State_SU state = START;
    unsigned char byte;
    unsigned char expectedRR = (Ns == 0) ? C_RR1 : C_RR0;
    
    alarmEnabled = FALSE;
    alarmCount = 0;
    
    bool isREJ = false;

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
                        else if (byte == A_RX) state = A_RCV;
                        else state = START;
                        break;
                    case A_RCV:
                        if (byte == FLAG) state = FLAG_RCV;
                        else if (byte == expectedRR) {
                            isREJ = FALSE;
                            state = C_RCV;
                        }
                        else if (byte == C_REJ0 || byte == C_REJ1) {
                            isREJ = TRUE;
                            state = C_RCV;
                        }
                        else state = START;
                        break;
                    case C_RCV:
                        if (byte == FLAG) state = FLAG_RCV;
                        else if (byte == (A_RX ^ expectedRR)) {
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
                            return bufSize;
                        }
                        else {
                            if (isREJ) {
                                printf("TX: Received REJ — retransmitting frame.\n");
                                // force resending
                                alarm(0);
                                alarmEnabled = FALSE;
                            }
                            state = START;
                        }
                        break;
                    case STOP:
                        break;
                }
            }
        }
        
        if (!alarmEnabled) {
            nRetransmissions--;
            if (isREJ) printf("TX: REJ received — retransmitting frame.\n");
            else printf("TX: Timeout — retransmitting frame.\n");
            isREJ = FALSE;
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
                        if (currentC == 0x00 || currentC == 0x80) {
                            if (currentC != expectedC) {
                                // Duplicated Frame
                                printf("RX: Duplicate frame detected (got %s, expected %s)\n",
                                    currentC == 0x00 ? "I0" : "I1",
                                    expectedC == 0x00 ? "I0" : "I1");
                                
                                // RR sent to confirm what is already expect
                                unsigned char rrFrame[SUFrame_SIZE];
                                unsigned char rrControl = (Nr == 0) ? C_RR1 : C_RR0;
                                buildSUFrame(rrFrame, A_RX, rrControl);
                                int bytesWritten = writeBytesSerialPort(rrFrame, SUFrame_SIZE);
                                if (bytesWritten != SUFrame_SIZE) {
                                    fprintf(stderr, "Erro: falha ao escrever frame (%d/%d bytes)\n", bytesWritten, SUFrame_SIZE);
                                    return -1;
                                }

                                // Discard the duplicated Frame
                                state = START;
                                dataIdx = 0;
                                break;
                            }
                            
                            // Expected Frame
                            state = BCC1_OK;
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
                        
                        if (receivedBCC2 == calculatedBCC2) {
                            // Valid data
                            memcpy(packet, dataBuffer, dataIdx);
                            
                            unsigned char rrFrame[SUFrame_SIZE];
                            unsigned char rrControl = (Nr == 0) ? C_RR1 : C_RR0;
                            
                            buildSUFrame(rrFrame, A_RX, rrControl);
                            int bytesWritten = writeBytesSerialPort(rrFrame, SUFrame_SIZE);
                            if (bytesWritten != SUFrame_SIZE) {
                                fprintf(stderr, "Erro: falha ao escrever frame (%d/%d bytes)\n", bytesWritten, SUFrame_SIZE);
                                return -1;
                            }
                            
                            printf("RX: I-Frame received (Nr=%d). Sent RR%d.\n", Nr, 1 - Nr);
                            Nr = (Nr == 0) ? 1:0;
                            return dataIdx;
                        } else {
                            unsigned char rejFrame[SUFrame_SIZE];
                            unsigned char rejControl = (Nr == 0) ? C_REJ0 : C_REJ1;
                            buildSUFrame(rejFrame, A_RX, rejControl);
                            int bytesWritten = writeBytesSerialPort(rejFrame, SUFrame_SIZE);
                            if (bytesWritten != SUFrame_SIZE) {
                                fprintf(stderr, "Erro: falha ao escrever frame (%d/%d bytes)\n", bytesWritten, SUFrame_SIZE);
                                return -1;
                            }
                            
                            printf("RX: Frame error. Sent REJ%d.\n", Nr);
                            
                            dataIdx = 0;
                            escaped = 0;
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

                            if (dataIdx < MAX_PAYLOAD_SIZE * 2) {
                                dataBuffer[dataIdx++] = byte;
                            } else {
                                // Buffer overflow, Frame discarded
                                printf("RX: Data buffer overflow. Restarting.\n");
                                dataIdx = 0;
                                escaped = 0;
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

/*
    globalRole = connectionParameters.role;
    globalTimeout = connectionParameters.timeout;
    globalNRetransmissions = connectionParameters.nRetransmissions;
*/
int llclose()
{
    if( globalRole == LlTx ){
        printf("Tx: Preparing to send Disc ( SU Frame) to RX\n");
        unsigned char discFrame[SUFrame_SIZE];
        buildSUFrame(discFrame, A_TX, C_DISC);

        int nRetransmissions = globalNRetransmissions;
        int timeout = globalTimeout;

        enum State_SU state = START;
        unsigned char byte;

        alarmEnabled = FALSE;
        alarmCount = 0;
        
        while (nRetransmissions > 0) {
            writeToSerialPort(discFrame, SUFrame_SIZE, timeout, &nRetransmissions);
            printf("Tx: Disc ( SU Frame ) Sent\n");
            
            state = START;
            
            while (state != STOP && alarmEnabled) {
                if (readByteSerialPort(&byte) > 0) {
                    switch(state) {
                        case START:
                            if (byte == FLAG) state = FLAG_RCV;
                            break;
                        case FLAG_RCV:
                            if (byte == FLAG) state = FLAG_RCV;
                            else if (byte == A_RX) state = A_RCV;
                            else state = START;
                            break;
                        case A_RCV:
                            if (byte == FLAG) state = FLAG_RCV;
                            else if (byte == C_DISC) state = C_RCV;
                            else state = START;
                            break;
                        case C_RCV:
                            if (byte == FLAG) state = FLAG_RCV;
                            else if (byte == (A_RX ^ C_DISC)) state = BCC1_OK;
                            else state = START;
                            break;
                        case BCC1_OK:
                            if (byte == FLAG) {
                                state = STOP;
                                alarm(0);
                                alarmEnabled = FALSE;
                                printf("TX: Disc received from RX.\n");

                                printf("TX: Preparring UA ( SU frame ) to finish the connection.\n");

                                unsigned char UAFrame[SUFrame_SIZE];
                                buildSUFrame( UAFrame, A_TX, C_UA);
                                writeBytesSerialPort( UAFrame, SUFrame_SIZE);
                                
                                int isClosed = closeSerialPort();
                                if (isClosed == 0){
                                    printf("Tx: Connection terminated\n");
                                }
                                else{
                                    perror("Error closing SerialPort on Tx");
                                    return -1;
                                }

                                return 0;
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
        
        printf("TX: ERROR - Failed to establish connection after all retries.\n");
        closeSerialPort();
        return -1;
        
    }
    else{
        printf("RX: Waiting for DISC frame...\n");
        
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
                        else if (byte == C_DISC) state = C_RCV;
                        else state = START;
                        break;
                    case C_RCV:
                        if (byte == FLAG) state = FLAG_RCV;
                        else if (byte == (A_TX ^ C_DISC)) state = BCC1_OK;
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
        
        printf("RX: DISC received. Sending DISC...\n");
        
        unsigned char discFrame[SUFrame_SIZE];
        buildSUFrame(discFrame, A_RX, C_DISC);
        
        if (writeBytesSerialPort(discFrame, SUFrame_SIZE) < 0) {
            perror("writeBytesSerialPort - UA");
            return -1;
        }

        printf("RX: Waiting for UA frame...\n");
        
        state = START;
        
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
                        else if (byte == C_UA) state = C_RCV;
                        else state = START;
                        break;
                    case C_RCV:
                        if (byte == FLAG) state = FLAG_RCV;
                        else if (byte == (A_TX ^ C_UA)) state = BCC1_OK;
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

        printf("RX: UA received. Terminating the connection...\n");

        int isClosed = closeSerialPort();
        if (isClosed == 0){
            printf("Tx: Connection terminated\n");
        }
        else{
            perror("Error closing SerialPort on Tx");
            return -1;
        }

        return 0;
    }

}