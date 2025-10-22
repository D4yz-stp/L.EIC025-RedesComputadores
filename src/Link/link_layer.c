// Link layer protocol implementation

#include "link_layer.h"
#include "serial_port.h"
#include "stdbool.h"

// CORREÇÕES de HEADER
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h> // Para memset
#include "alarm_sigaction.h" // Caminho corrigido para o seu layout atual

// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source

#define SUFrame_SIZE 5
#define I_HEADER_SIZE 4 // F | A | C | BCC1

// S/U Frame
#define FLAG 0x7E // (F)
#define A_TX 0x03 // Address that the Tr send (A)
#define A_RX 0x01 // Address that the Rx send (A)

// Control field values (C)
#define C_SET  0x03
#define C_UA   0x07
#define C_DISC 0x0B
#define C_RR0  0x05 // Corrigido de 0xAA (RR) para 0x05 (RR0, bit P/F=0) 
#define C_RR1  0x85 // Corrigido de 0xAB (RR) para 0x85 (RR1, bit P/F=0)
#define C_REJ0 0x01 // Corrigido de 0x54 (REJ) para 0x01 (REJ0, bit P/F=0)
#define C_REJ1 0x81 // Corrigido de 0x55 (REJ) para 0x81 (REJ1, bit P/F=0)

// I-Frame Control Field (C) Values
#define C_I0 0x00 // Ns=0, P/F=0
#define C_I1 0x40 // Ns=1, P/F=0


// Variáveis globais para controlo de sequência (Ns, Nr)
int Ns = 0; // Next sequence number to send (0 or 1)
int Nr = 0; // Next sequence number expected (0 or 1)
extern int fd; // File descriptor da porta serial (Serial port is defined in serial_port.c)

// Máquina de Estados para S/U Frames
enum State_SU{START, FLAG_RCV, A_RCV, C_RCV, BCC1_OK, STOP};


void buildSUFrame(unsigned char *frame, unsigned char address, unsigned char control)
{
    frame[0] = FLAG;
    frame[1] = address;
    frame[2] = control;
    frame[3] = address ^ control; // (BCC1)
    frame[4] = FLAG;
}

// CORREÇÃO de sintaxe e lógica de alarme
int writeToSerialPort(unsigned char *frame , int timeout, int *nRetransmissions){

    writeBytesSerialPort(frame, SUFrame_SIZE);
    alarm(timeout);
    // Isto deve ser alterado quando implementarmos a espera por resposta
    
    return 0; 
}

//Calcula o BCC2 sobre o buffer de dados (Payload)
unsigned char calculateBCC2(const unsigned char *data, int dataSize) {
    unsigned char bcc2 = 0;
    for (int i = 0; i < dataSize; i++) {
        bcc2 ^= data[i];
    }
    return bcc2;
}

////////////////////////////////////////////////
// LLOPEN (M2 - FINALIZADO)
////////////////////////////////////////////////
int llopen(LinkLayer connectionParameters)
{
    // Tenta abrir a porta serial (o FD está em serial_port.c)
    fd = openSerialPort(connectionParameters.serialPort, connectionParameters.baudRate);
    if (fd < 0) {
        perror("openSerialPort");
        return -1; 
    }
    
    // Configurar o alarme apenas para o Transmissor
    if (connectionParameters.role == LlTx) {
        if (setupAlarm() < 0) {
            perror("setupAlarm");
            return -1;
        }
    }


    if(connectionParameters.role == LlTx) {
        // CÓDIGO DO TRANSMISSOR (LlTx) - Enviar SET
        printf("TX: Sending SET frame...\n");

        unsigned char frameTx[SUFrame_SIZE]; // controi a frame SET [F A C BCC1 F] - 5 bytes.
        int nRetransmissions = 0;
        
        buildSUFrame(frameTx, A_TX, C_SET);

        // CORREÇÃO: Aqui deve ter o loop de retransmissão
        //  o llopen_tx retorna após 1 envio/leitura.
        
        // ENVIA
        writeBytesSerialPort(frameTx, SUFrame_SIZE);
        
        // RECEBE (ESPERA PELO UA)
        enum State_SU state = START;
        unsigned char byte;
        
        while (state != STOP) {
             if (readByteSerialPort(&byte) > 0) {
                switch(state) {
                    case START:
                        if(byte == FLAG) {state = FLAG_RCV;}
                        break;
                    case FLAG_RCV:
                        if(byte == FLAG) {state = FLAG_RCV;}
                        else if (byte == A_TX) {state = A_RCV;} // Espera A=0x03
                        else {state = START;}
                        break;
                    case A_RCV:
                        if(byte == FLAG) {state = FLAG_RCV;}
                        else if (byte == C_UA) {state = C_RCV;} // Espera C=UA
                        else {state = START;}
                        break;
                    case C_RCV:
                        if(byte == FLAG) {state = FLAG_RCV;}
                        else if (byte == (A_TX ^ C_UA)) {state = BCC1_OK;} // Espera BCC1
                        else {state = START;}
                        break;
                    case BCC1_OK:
                        if(byte == FLAG) {
                            state = STOP; // UA recebido e validado!
                            printf("TX: Received UA frame. Connection established.\n");
                        }
                        else {state = START;}
                        break;
                    case STOP: 
                        break;
                }
            }
            // Aqui lógica de timeout/retransmissão
        }

        Ns = 0;
        return 0;
    } 
    else { 
        // CÓDIGO DO RECETOR (LlRx) - Receber SET e enviar UA
        
        enum State_SU state = START;
        unsigned char byte;
        
        while(state != STOP) {
            
            int bytes = readByteSerialPort(&byte); 
            
            if(bytes > 0) {
                switch(state) {
                    case START:
                        if(byte == FLAG) {state = FLAG_RCV;}
                        break;
                    case FLAG_RCV:
                        if(byte == FLAG) {state = FLAG_RCV;} 
                        else if (byte == A_TX) {state = A_RCV;} 
                        else {state = START;}
                        break;
                    case A_RCV:
                        if(byte == FLAG) {state = FLAG_RCV;}
                        else if (byte == C_SET) {state = C_RCV;}
                        else {state = START;}
                        break;
                    case C_RCV:
                        if(byte == FLAG) {state = FLAG_RCV;}
                        else if (byte == (A_TX ^ C_SET)) {state = BCC1_OK;}
                        else {state = START;}
                        break;
                    case BCC1_OK:
                        if(byte == FLAG) {
                            state = STOP; // SET recebido e validado!
                        }
                        else {state = START;}   
                        break;
                    case STOP: 
                        break;
                }
            }
        }
        
        printf("RX: Received SET frame. Sending UA...\n");

        // ENVIAR O UA (Unnumbered Acknowledgement)
        unsigned char frameTx[SUFrame_SIZE];
        buildSUFrame(frameTx, A_TX, C_UA); 
        
        if (writeBytesSerialPort(frameTx, SUFrame_SIZE) < 0) {
            perror("writeBytesSerialPort - UA");
            return -1;
        }
        
        Nr = 0; 
        return 0;
    }
}

////////////////////////////////////////////////
// LLWRITE (M3 - ENVIAR I-FRAME)
////////////////////////////////////////////////
int llwrite(const unsigned char *buf, int bufSize)
{
    // I-Frame tem tamanho máximo: I_HEADER_SIZE + bufSize + BCC2_SIZE + 2*FLAG
    // Simplificação: alocar espaço suficiente, sem calcular o stuffing ainda.
    unsigned char frameTx[MAX_PAYLOAD_SIZE * 2 + 10]; 
    int frameTxIdx = 0;
    
    // 1. C-Field (Ns)
    unsigned char C_Field = (Ns == 0) ? C_I0 : C_I1;
    
    // 2. CONSTRUIR O CABEÇALHO DO I-FRAME (F A C BCC1)

    // F (Flag)
    frameTx[frameTxIdx++] = FLAG; 

    // A (Address)
    frameTx[frameTxIdx++] = A_TX;

    // C (Control - Ns)
    frameTx[frameTxIdx++] = C_Field;

    // BCC1 (A XOR C)
    frameTx[frameTxIdx++] = A_TX ^ C_Field;

    // 3. PAYLOAD E CÁLCULO DO BCC2
    unsigned char bcc2 = 0;
    
    // Simplificação: Nenhuma lógica de Byte Stuffing aqui ainda!
    for (int i = 0; i < bufSize; i++) {
        bcc2 ^= buf[i]; // Calcula o BCC2
        frameTx[frameTxIdx++] = buf[i]; // Adiciona o dado
    }
    
    // 4. ADICIONAR BCC2
    frameTx[frameTxIdx++] = bcc2;

    // 5. FLAG DE FECHO
    frameTx[frameTxIdx++] = FLAG;
    
    // LÓGICA DE STOP & WAIT:
    // Aqui deveria estar o loop de retransmissão e a espera por RR/REJ
    
    int bytesWritten = writeBytesSerialPort(frameTx, frameTxIdx);
    
    // Assumindo que a transmissão foi bem-sucedida (Stop & Wait simplificado):
    Ns = 1 - Ns; // Mudar Ns para o próximo frame (Stop & Wait)

    return bytesWritten;
}

////////////////////////////////////////////////
// LLREAD (M3 - RECEBER I-FRAME)
////////////////////////////////////////////////
int llread(unsigned char *packet)
{
    enum State_SU state = START;
    unsigned char byte;
    
    // buffer para a data (payload) extraída do I-Frame
    unsigned char dataBuffer[MAX_PAYLOAD_SIZE];
    int dataIdx = 0;
    
    // Variaveis de controlo
    unsigned char expectedC = (Nr == 0) ? C_I0 : C_I1; // C field esperado
    unsigned char currentC = 0;
    
    // Máquina de Estados para ler o I-Frame
    while (state != STOP) {
        if (readByteSerialPort(&byte) > 0) {
            switch(state) {
                case START:
                    if(byte == FLAG) {state = FLAG_RCV;}
                    break;
                case FLAG_RCV:
                    if(byte == FLAG) {state = FLAG_RCV;} 
                    else if (byte == A_TX) {state = A_RCV;} 
                    else {state = START;}
                    break;
                case A_RCV:
                    if(byte == FLAG) {state = FLAG_RCV;}
                    else {
                        currentC = byte;
                        state = C_RCV;
                    }
                    break;
                case C_RCV:
                    if(byte == FLAG) {state = FLAG_RCV;}
                    else if (byte == (A_TX ^ currentC)) {
                        // Verifica se é o I-Frame que esperamos (C_I0 ou C_I1)
                        if (currentC == expectedC || currentC == (1-expectedC)) {
                            state = BCC1_OK;
                            // Se for duplicado (currentC != expectedC), processa-se e manda-se RR
                        } else {
                            state = START; // C inesperado, reinicia
                        }
                    }
                    else {state = START;}
                    break;
                case BCC1_OK:
                    if(byte == FLAG) {
                        // Se receber FLAG, mas não houver dados, BCC2 não foi recebido!
                        if (dataIdx == 0) {
                            state = FLAG_RCV;
                        } else {
                            // O byte anterior é o BCC2 e este é o FLAG final.
                            unsigned char receivedBCC2 = dataBuffer[dataIdx-1];
                            dataIdx--; // Remove o BCC2 do buffer de dados
                            
                            unsigned char calculatedBCC2 = calculateBCC2(dataBuffer, dataIdx);

                            if (receivedBCC2 == calculatedBCC2) {
                                // Dados VÁLIDOS!
                                // Copiar dados para o buffer final (packet)
                                memcpy(packet, dataBuffer, dataIdx);
                                
                                // Enviar RR (Ready to Receive) - Lógica a ser implementada
                                // Nr = 1 - Nr; // Só altera se não for duplicado
                                
                                state = STOP;
                                return dataIdx;
                            } else {
                                // BCC2 inválido. Enviar REJ (Reject) - Lógica a ser implementada
                                printf("RX: BCC2 Mismatch (Received: 0x%02x, Calculated: 0x%02x). Frame rejected.\n", receivedBCC2, calculatedBCC2);
                                dataIdx = 0;
                                state = START;
                            }
                        }
                    }
                    else {
                        // Adiciona o byte ao buffer de dados (Payload + BCC2)
                        if (dataIdx < MAX_PAYLOAD_SIZE) {
                            dataBuffer[dataIdx++] = byte;
                        } else {
                            // Buffer overflow, descarta o frame
                            printf("RX: Data buffer overflow. Restarting.\n");
                            dataIdx = 0;
                            state = START;
                        }
                    }
                    break;
                case STOP:
                    break;
            }
        }
    }
    return -1; // Erro
}


////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose()
{
    // TODO: Implementar o Handshake DISC/UA aqui

    if (closeSerialPort() < 0)
    {
        perror("closeSerialPort");
        exit(-1);
    }
    printf("Connection closed.\n");
    return 0;
}