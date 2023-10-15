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

int alarmEnabled = False;
int alarmCounter=0;
void alarmHandler()
{
    alarmEnabled = False;
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
    newtio.c_cc[VTIME] = connectionParameters.timeout; /* inter-character timer unused */
    newtio.c_cc[VMIN] = 0;                            

    tcflush(fd, TCIOFLUSH);

    if (tcsetattr(fd, TCSANOW, &newtio) == -1)
    {
        perror("tcsetattr");
        return -1;
    }

    printf("New termios structure set\n");

    if(connectionParameters.role == LlTx){
        unsigned char bufW[5] = {0};
        unsigned char bufR[5] = {0};

        bufW[0] = FLAG;
        bufW[1] = A_TRANSMITTER;
        bufW[2] = C_SET;
        bufW[3] = buf[1] ^ buf[2];
        bufW[4] = FLAG;

        State currState = Start;
        char currA = 0;
        char currC = 0;

        while(connectionParameters.nRetransmissions > alarmCounter){
            if(alarmEnabled == False){
                int resW = write(fd, bufW, 5);
            
                if(resW < 0){
                    perror("write");
                    return -1;
                }
                printf("Sent SET\n");
                alarm(connectionParameters.timeout);
                alarmEnabled = True;
            }
            int resR = read(fd, bufR, 5);
            if(resR < 0){
                perror("read");
                return -1;
            }
            for(int i = 0; i < 5; i++){
                switch(bufR[i]){
                    case FLAG:
                        if(currState == BCC_OK){
                            return 0;
                        } else {
                            currState = Flag_RCV;
                        }
                        break;
                    case A_TRANSMITTER:
                        if(currState == Flag_RCV){
                            currState = A_RCV;
                            currA = bufR[i];
                        } else {
                            currState = Start;
                        }
                        break;
                    case C_UA:
                        if(currState == A_RCV){
                            currState = C_RCV;
                            currC = bufR[i];
                        } else {
                            currState = Start;
                        }
                        break;
                    case (currA ^ currC):
                        if(currState == C_RCV){
                            currState = BCC_OK;
                        } else {
                            currState = Start;
                        }
                        break;
                    default:
                        currState = Start;
                        break;

                }
            }

        }
        return -1;

    } else if(connectionParameters.role == LlRx){
        newtio.c_cc[VMIN] = 1;
        unsigned char bufW[5] = {0};
        unsigned char bufR[5] = {0};
        int res = read(fd, bufR, 5);
        if(res < 0){
            perror("read");
            return -1;
        }
        bufW[0]=FLAG;
        bufW[1]=A_RECEIVER;
        bufW[2]=C_UA;
        bufW[3]=bufW[1]^bufW[2];
        bufW[4]=FLAG;

        int resW = write(fd, bufW, 5);
        if(resW < 0){
            perror("write");
            return -1;
        }
        printf("Sent UA\n");
        return 0;

    } else {
        printf("Invalid role\n");
        return -1;
    }     

    return 1;
}

////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int llwrite(int fd, const unsigned char *buf, int bufSize)
{
    // TODO

    return 0;
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
