// Link layer protocol implementation

#include "link_layer.h"
#include "serial_port.h"
#include "stdbool.h"
#include "../Alarm/alarm_sigaction.h"

// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source

#define SUFrame_SIZE 5

// S/U Frame
#define FLAG 0x7E // (F)
#define A_TX 0x03 // Address that the Tr send (A)
#define A_RX 0x01 // Address that the Rx send (A)

// Control field values (C)
#define C_SET  0x03
#define C_UA   0x07
#define C_DISC 0x0B
#define C_RR0  0xAA
#define C_RR1  0xAB
#define C_REJ0 0x54
#define C_REJ1 0x55

void buildSUFrame(unsigned char *frame, unsigned char address, unsigned char control)
{
    frame[0] = FLAG;
    frame[1] = address;
    frame[2] = control;
    frame[3] = address ^ control; // (BCC1)
    frame[4] = FLAG;
}
////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
int llopen(LinkLayer connectionParameters)
{
    if (openSerialPort(connectionParameters.serialPort, connectionParameters.baudRate) < 0)
    {
        perror("openSerialPort");
        exit(-1);
    }
    unsigned char frame[SUFrame_SIZE];
    bool run = TRUE;

    if(strcmp(connectionParameters.role == LlTx)) {
        
        buildSUFrame(frame, A_TX, C_SET);
        while(run){
            writeBytesSerialPort(frame, SUFrame_SIZE);
        }
    }

    return 0;
}

////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int llwrite(const unsigned char *buf, int bufSize)
{
    // TODO: Implement this function

    return 0;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet)
{
    // TODO: Implement this function

    return 0;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose()
{
    // TODO: Implement this function

    if (closeSerialPort() < 0)
    {
        perror("closeSerialPort");
        exit(-1);
    }
    return 0;
}
