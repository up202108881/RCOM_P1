// Link layer protocol implementation

#include "link_layer.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>

// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source

int alarmEnabled = FALSE;
int alarmCounter=0;
void alarmHandler()
{
    alarmEnabled = FALSE;
    alarmCounter++;
}

////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
int llopen(LinkLayer connectionParameters)
{
    (void) signal(SIGALRM, alarmHandler);

    int fd = open(connectionParameters.serialPort, O_RDWR | O_NOCTTY);
    if (fd < 0)
    {
        perror(connectionParameters.serialPort);
        return -1;
    }

    struct termios oldtio, newtio;

    if (tcgetattr(fd, &oldtio) == -1)
    { /* save current port settings */
        perror("tcgetattr");
        return -1;
    }

    memset(&newtio, 0, sizeof(newtio));

    newtio.c_cflag = connectionParameters.baudRate | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    /* set input mode (non-canonical, no echo,...) */
    newtio.c_lflag = 0;
    newtio.c_cc[VTIME] = 0; /* inter-character timer unused */
    newtio.c_cc[VMIN] = 0;                            

    tcflush(fd, TCIOFLUSH);

    if (tcsetattr(fd, TCSANOW, &newtio) == -1)
    {
        perror("tcsetattr");
        return -1;
    }

    printf("New termios structure set\n");

    State currState = Start;

    unsigned char bufW[5] = {0};
    unsigned char byte = 0;

    if(connectionParameters.role == LlTx){

        bufW[0] = FLAG;
        bufW[1] = A_TRANSMITTER;
        bufW[2] = C_SET;
        bufW[3] = bufW[1] ^ bufW[2];
        bufW[4] = FLAG;

        while(connectionParameters.nRetransmissions > alarmCounter){
            if(alarmEnabled == FALSE){
                int resW = write(fd, bufW, 5);
            
                if(resW < 0){
                    perror("write");
                    return -1;
                }
                printf("Sent SET\n");
                alarm(connectionParameters.timeout);
                alarmEnabled = TRUE;
            }
            if(read(fd, &byte, 1) > 0){
                switch(currState){
                    case Start:
                        if(byte == FLAG) currState = Flag_RCV;
                        break;
                    case Flag_RCV:
                        if(byte == A_RECEIVER) currState = A_RCV;
                        else if(byte == FLAG) currState = Flag_RCV;
                        else currState = Start;
                        break;
                    case A_RCV:
                        if(byte == C_UA) currState = C_RCV;
                        else if(byte == FLAG) currState = Flag_RCV;
                        else currState = Start;
                        break;
                    case C_RCV:
                        if(byte == (C_UA ^ A_RECEIVER)) currState = BCC_OK;
                        else if(byte == FLAG) currState = Flag_RCV;
                        else currState = Start;
                        break;
                    case BCC_OK:
                        if(byte == FLAG) { currState = Stop; alarm(0); return fd; }
                        else currState = Start;                    
                        break;
                    default:
                        break;

                }
            }

        }
        return -1;

    } else if(connectionParameters.role == LlRx){ // FAZ SENTIDO TER TIMEOUT AQUI?!
        while( currState != Stop){
            if(read(fd, &byte, 1) > 0){
                switch(currState){
                    case Start:
                        if(byte == FLAG) currState = Flag_RCV;
                        break;
                    case Flag_RCV:
                        if(byte == A_TRANSMITTER) currState = A_RCV;
                        else if(byte != FLAG) currState = Start;
                        break;
                    case A_RCV:
                        if(byte == C_SET) currState = C_RCV;
                        else if(byte == FLAG) currState = Flag_RCV;
                        else currState = Start;
                        break;
                    case C_RCV:
                        if(byte == (C_SET ^ A_TRANSMITTER)) currState = BCC_OK;
                        else if(byte == FLAG) currState = Flag_RCV;
                        else currState = Start;
                        break;
                    case BCC_OK:
                        if(byte == FLAG) currState = Stop;
                        else currState = Start;                    
                        break;
                    default:
                        break;

                }
            }
            
        }

        bufW[0]=FLAG;
        bufW[1]=A_RECEIVER;
        bufW[2]=C_UA;
        bufW[3]=bufW[1]^bufW[2];
        bufW[4]=FLAG;

        int resW = write(fd, bufW, 5);
        if(resW != 5){
            perror("write");
            return -1;
        }
        printf("Sent UA\n");
        return fd;

    } else {
        printf("Invalid role\n");
        return -1;
    }     

    return -1;
}

////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int llwrite(int fd, const unsigned char *buf, int bufSize)
{   
    int frameSize = bufSize + FH_SIZE + FT_SIZE;
    unsigned char *frame = (unsigned char *)malloc(frameSize * sizeof(unsigned char));

    for (int i = 0; i < frameSize; i++) {
        frame[i] = 0;
    }
    frame[0] = FLAG;
    frame[1] = A;
    frame[2] = C;
    frame[3] = frame[1] ^ frame[2];
    frame[4] = buf[0];
    frame[frameSize - 1] = FLAG;

    unsigned char bcc2 = buf[0];
    for (int i = 1; i < bufSize; i++) {
        frame[FH_SIZE + i] = buf[i];
        bcc2 ^= buf[i];
    }

    frame[4 + bufSize] = bcc2;

    int response_received = FALSE;
    
    while (response_received == FALSE) {
        int bytes = write(fd, frame, frameSize);

        if (bytes != frameSize) {
            perror("write");
            return -1;
        }

        unsigned char response[5] = {0};

        int bytes_read = read(fd, response, 5);

        if (bytes_read == 5) {
            response_received = TRUE;
            
        }
    }
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(int fd, unsigned char *packet)
{
    // TODO

    return 0;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(int showStatistics)
{
    // TODO

    return 1;
}
