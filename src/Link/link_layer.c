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

enum State_UA{START, FLAG_RCV, A_RCV, C_RCV, BCC1_OK, STOP};

void buildSUFrame(unsigned char *frame, unsigned char address, unsigned char control)
{
    frame[0] = FLAG;
    frame[1] = address;
    frame[2] = control;
    frame[3] = address ^ control; // (BCC1)
    frame[4] = FLAG;
}

void writeToSerialPort(unsigned char *frame ){
    writeBytesSerialPort(frame, SUFrame_SIZE);
    alarm(3);
    alarmEnabled = TRUE;
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
    unsigned char frameToSend[SUFrame_SIZE];
    unsigned char frameToRecive[SUFrame_SIZE];

    if(strcmp(connectionParameters.role == LlTx)) {
        
        buildSUFrame(frameToSend, A_TX, C_SET);
        buildSUFrame(frameToRecive, A_RX, C_UA);

        enum State_UA state = START;

        writeToSerialPort(frameToSend);
        setupAlarm();

        unsigned char byte;
        while(state != STOP){
            int bytes = readByteSerialPort(&byte);
            if( bytes > 0 && alarmEnabled ==TRUE){
                switch(state){
                    case START:
                        if(byte == FLAG) {state = FLAG_RCV;}
                        break;
                    case FLAG_RCV:
                        if(byte == FLAG) {state = FLAG;}
                        else if (byte == A_RX) {state = A_RCV;}
                        else {state = START;}
                    case A_RCV:
                        if(byte == FLAG) {state = FLAG;}
                        else if (byte == C_UA) {state = C_RCV;}
                        else {state = START;}
                    case C_RCV:
                        if(byte == FLAG) {state = FLAG;}
                        else if (byte == (A_RX^C_UA)) {state = BCC1_OK;}
                        else {state = START;}
                    case BCC1_OK:
                        if(byte == FLAG) {state = STOP;}
                        else {state = START;}   
                }
            }
            if( alarmEnabled == FALSE){
                writeToSerialPort(frameToSend); 
            } 
        }
        // Conseguimos estabelecer conexão, agora é dizer isso ao apllication layer que ele chama o write
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
