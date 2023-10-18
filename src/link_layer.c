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
#include <signal.h>
#include <time.h>

// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source

struct termios oldtio;
int errorOccurred = FALSE;
int alarmEnabled = FALSE;
int alarmCounter = 0;
int Ns = 0;
int Nr = 1;

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
        errorOccurred = TRUE;
        return -1;
    }

    struct termios newtio;

    if (tcgetattr(fd, &oldtio) == -1)
    { /* save current port settings */
        perror("tcgetattr");
        errorOccurred = TRUE;
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
        errorOccurred = TRUE;
        return -1;
    }

    printf("New termios structure set\n");

    State currState = START;

    unsigned char bufW[5] = {0};
    unsigned char byte;

    if(connectionParameters.role == LLTX){

        bufW[0] = FLAG;
        bufW[1] = A_TRANSMITTER;
        bufW[2] = C_SET;
        bufW[3] = bufW[1] ^ bufW[2];
        bufW[4] = FLAG;

        while (connectionParameters.nRetransmissions > alarmCounter) {
            if (alarmEnabled == FALSE) {
                int resW = write(fd, bufW, 5);
            
                if (resW < 0) {
                    perror("write");
                    errorOccurred = TRUE;
                    return -1;
                }
                printf("Sent SET\n");
                alarm(connectionParameters.timeout);
                alarmEnabled = TRUE;
            }
            if (read(fd, &byte, 1) > 0) {
                switch (currState) {
                    case START:
                        if (byte == FLAG) currState = FLAG_RCV;
                        break;
                    case FLAG_RCV:
                        if (byte == A_RECEIVER) currState = A_RCV;
                        else if (byte == FLAG) currState = FLAG_RCV;
                        else currState = START;
                        break;
                    case A_RCV:
                        if (byte == C_UA) currState = C_RCV;
                        else if (byte == FLAG) currState = FLAG_RCV;
                        else currState = START;
                        break;
                    case C_RCV:
                        if (byte == (C_UA ^ A_RECEIVER)) currState = BCC_OK;
                        else if (byte == FLAG) currState = FLAG_RCV;
                        else currState = START;
                        break;
                    case BCC_OK:
                        if (byte == FLAG) { 
                            currState = STOP; 
                            alarm(0);
                            printf("Received UA\n"); 
                            return fd; 
                        }
                        else currState = START;                    
                        break;
                    default:
                        break;

                }
            }

        }
        errorOccurred = TRUE;
        return -1;

    } else if (connectionParameters.role == LLRX) { // FAZ SENTIDO TER TIMEOUT AQUI?!
        while (currState != STOP) {
            if (read(fd, &byte, 1) > 0) {
                switch (currState) {
                    case START:
                        if (byte == FLAG) currState = FLAG_RCV;
                        break;
                    case FLAG_RCV:
                        if (byte == A_TRANSMITTER) currState = A_RCV;
                        else if (byte != FLAG) currState = START;
                        break;
                    case A_RCV:
                        if (byte == C_SET) currState = C_RCV;
                        else if (byte == FLAG) currState = FLAG_RCV;
                        else currState = START;
                        break;
                    case C_RCV:
                        if (byte == (C_SET ^ A_TRANSMITTER)) currState = BCC_OK;
                        else if (byte == FLAG) currState = FLAG_RCV;
                        else currState = START;
                        break;
                    case BCC_OK:
                        if (byte == FLAG) { 
                            currState = STOP; 
                            printf("Received SET\n"); 
                        }
                        else currState = START;                    
                        break;
                    default:
                        break;

                }
            }
            
        }

        bufW[0] = FLAG;
        bufW[1] = A_RECEIVER;
        bufW[2] = C_UA;
        bufW[3] = bufW[1] ^ bufW[2];
        bufW[4] = FLAG;

        int resW = write(fd, bufW, 5);
        if (resW != 5) {
            perror("write");
            errorOccurred = TRUE;
            return -1;
        }
        printf("Sent UA\n");
        return fd;

    } else {
        printf("Invalid role\n");
        errorOccurred = TRUE;
        return -1;
    }     
    errorOccurred = TRUE;
    return -1;
}

////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int llwrite(int fd, LinkLayer connectionParameters, const unsigned char *buf, int bufSize)
{   
    unsigned char bcc2 = buf[0];
    for (int i = 1; i < bufSize; i++) {
        bcc2 ^= buf[i];
    }

    int stuffedBufSize = 0;
    unsigned char* stuffedBuf = byteStuffing(buf, bufSize, &stuffedBufSize);
    if (stuffedBuf == NULL) {
        errorOccurred = TRUE;
        return -1;
    }
    int frameSize = FH_SIZE + stuffedBufSize + FT_SIZE;
    unsigned char* frame = (unsigned char *)malloc(frameSize * sizeof(unsigned char));

    if (frame == NULL) {
        perror("malloc");
        errorOccurred = TRUE;
        return -1;
    }

    // Construct frame header
    frame[0] = FLAG;
    frame[1] = A_TRANSMITTER;
    frame[2] = C_INFO_FRAME(Ns);
    frame[3] = frame[1] ^ frame[2];

    // Construct frame data
    memcpy(frame + FH_SIZE, stuffedBuf, stuffedBufSize);

    // Construct frame trailer
    frame[FH_SIZE + stuffedBufSize] = bcc2;
    frame[frameSize - 1] = FLAG;

    State currState = START;
    unsigned char byte, receivedC;

    alarmCounter = 0;
    alarmEnabled = FALSE;

    while (connectionParameters.nRetransmissions > alarmCounter && currState != STOP) {
        if (alarmEnabled == FALSE) {
            int resW = write(fd, frame, frameSize);
        
            if (resW != frameSize) {
                perror("write");
                errorOccurred = TRUE;
                return -1;
            }
            printf("Sent frame\n");
            alarm(connectionParameters.timeout);
            alarmEnabled = TRUE;
        }
        if (read(fd, &byte, 1) > 0) {
            switch (currState) {
                case START:
                    if (byte == FLAG) currState = FLAG_RCV;
                    break;
                case FLAG_RCV:
                    if (byte == A_RECEIVER) currState = A_RCV;
                    else if (byte == FLAG) currState = FLAG_RCV;
                    else currState = START;
                    break;
                case A_RCV:
                    if (byte == C_RR(Ns)) currState = C_RCV;
                    else if (byte == C_REJ(Ns)) currState = C_RCV;
                    else if (byte == FLAG) currState = FLAG_RCV;
                    else currState = START;
                    receivedC = byte;
                    break;
                case C_RCV:
                    if (byte == (C_RR(Ns) ^ A_RECEIVER) || byte == (C_REJ(Ns) ^ A_RECEIVER)) currState = BCC_OK;
                    else if (byte == FLAG) currState = FLAG_RCV;
                    else currState = START;
                    break;
                case BCC_OK:
                    if (byte == FLAG) { 
                        if (receivedC == C_RR(Ns)) {
                            printf("Received RR\n"); 
                            currState = STOP; 
                            alarm(0);
                            Ns = (Ns + 1) % 2;
                            Nr = (Nr + 1) % 2;
                            free(stuffedBuf);
                            free(frame);
                            return 1;
                        } else if (receivedC == C_REJ(Ns)) {
                            printf("Received REJ\n");
                            alarm(0);
                            alarmEnabled = FALSE;
                            alarmCounter++;
                            currState = START;

                        }
                    }
                    else currState = START;                    
                    break;
                default:
                    break;
            }
        }
    }
    errorOccurred = TRUE;
    return -1;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(int fd, LinkLayer connectionParameters, unsigned char *packet)
{
    unsigned char byte, receivedC;
    int packetSize = 0;
    unsigned char* stuffedPacket = (unsigned char*)malloc(MAX_PAYLOAD_SIZE * sizeof(unsigned char));

    if (stuffedPacket == NULL) {
        perror("malloc");
        errorOccurred = TRUE;
        return -1;
    }

    State currState = START;
    alarmCounter = 0;
    alarmEnabled = FALSE;

    while (currState != STOP) {
        if (read(fd, &byte, 1) > 0) {
            switch (currState) {
                case START:
                    if (byte == FLAG) currState = FLAG_RCV;
                    break;
                case FLAG_RCV:
                    if (byte == A_TRANSMITTER) currState = A_RCV;
                    else if (byte == FLAG) currState = FLAG_RCV;
                    else currState = START;
                    break;
                case A_RCV:
                    if (byte == C_INFO_FRAME(0) || byte == C_INFO_FRAME(1)) { 
                        currState = C_RCV;
                        receivedC = byte;
                    }
                    else if (byte == FLAG) currState = FLAG_RCV;
                    else currState = START;
                    break;
                case C_RCV:
                    if (byte == (receivedC ^ A_TRANSMITTER)) currState = BCC_OK;
                    else if (byte == FLAG) currState = FLAG_RCV;
                    else currState = START;
                    break;
                case BCC_OK:
                    if (byte == FLAG) { 
                        unsigned char bcc2 = stuffedPacket[packetSize - 1];
                        packetSize--;
                        stuffedPacket[packetSize] = '\0';
                        stuffedPacket = (unsigned char*)realloc(stuffedPacket, packetSize * sizeof(unsigned char));

                        unsigned char* destuffedPacket = byteDestuffing(stuffedPacket, packetSize, &packetSize);
                        if (destuffedPacket == NULL) {
                            errorOccurred = TRUE;
                            return -1;
                        }

                        unsigned char bcc2Check = destuffedPacket[0];
                        for (int i = 1; i < packetSize; i++) {
                            bcc2Check ^= destuffedPacket[i];
                        }

                        unsigned char frame[5] = {0};

                        frame[0] = FLAG;
                        frame[1] = A_RECEIVER;
                        frame[4] = FLAG;

                        if (bcc2Check != bcc2) {
                            printf("BCC2 check failed\n");

                            frame[2] = C_REJ(Nr);
                            frame[3] = frame[1] ^ frame[2];

                            int resW = write(fd, frame, 5);
        
                            if (resW != 5) {
                                perror("write");
                                errorOccurred = TRUE;
                                return -1;
                            }

                            printf("Sent rejection frame\n");
                            free(destuffedPacket);
                            free(stuffedPacket);
                            return packetSize;
                        }
                        else {
                            printf("BCC2 check succeeded\n");

                            frame[2] = C_RR(Nr);
                            frame[3] = frame[1] ^ frame[2];

                            int resW = write(fd, frame, 5);
        
                            if (resW != 5) {
                                perror("write");
                                errorOccurred = TRUE;
                                return -1;
                            }

                            printf("Sent RR frame\n");
                            free(destuffedPacket);
                            free(stuffedPacket);
                            return packetSize;
                        }

                    }
                    else stuffedPacket[packetSize++] = byte;                 
                    break;
                default:
                    break;
            }
            if (currState == C_RCV) {
                stuffedPacket[packetSize++] = byte;
            }
            else if (currState == BCC_OK) {
                stuffedPacket[packetSize++] = byte;
                break;
            }
        }
    }
    return 0;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(int fd, LinkLayer connectionParameters, int showStatistics)
{
    if (errorOccurred == TRUE) {
        printf("Error occurred, closing...\n");
        if (tcsetattr(fd, TCSANOW, &oldtio) == -1) perror("tcsetattr");
        close(fd);
        return -1;
    }

    (void) signal(SIGALRM, alarmHandler);

    if (showStatistics == TRUE) {
        // TODO
    }

    State currState = START;
    unsigned char bufW[5] = {0};
    unsigned char byte;

    if (connectionParameters.role == LLTX) {
        
        bufW[0] = FLAG;
        bufW[1] = A_TRANSMITTER;
        bufW[2] = C_DISC;
        bufW[3] = bufW[1] ^ bufW[2];
        bufW[4] = FLAG;
        
        alarmCounter = 0;
        alarmEnabled = FALSE;

        while (connectionParameters.nRetransmissions > alarmCounter && currState != STOP) {
            if (alarmEnabled == FALSE) {
                int resW = write(fd, bufW, 5);
            
                if (resW != 5) {
                    perror("write");
                    if (tcsetattr(fd, TCSANOW, &oldtio) == -1) perror("tcsetattr");
                    close(fd);
                    return -1;
                }
                printf("Sent DISC\n");
                alarm(connectionParameters.timeout);
                alarmEnabled = TRUE;
            }
            if (read(fd, &byte, 1) > 0) {
                switch (currState) {
                    case START:
                        if (byte == FLAG) currState = FLAG_RCV;
                        break;
                    case FLAG_RCV:
                        if (byte == A_RECEIVER) currState = A_RCV;
                        else if (byte == FLAG) currState = FLAG_RCV;
                        else currState = START;
                        break;
                    case A_RCV:
                        if (byte == C_DISC) currState = C_RCV;
                        else if (byte == FLAG) currState = FLAG_RCV;
                        else currState = START;
                        break;
                    case C_RCV:
                        if (byte == (C_DISC ^ A_RECEIVER)) currState = BCC_OK;
                        else if (byte == FLAG) currState = FLAG_RCV;
                        else currState = START;
                        break;
                    case BCC_OK:
                        if (byte == FLAG) { 
                            currState = STOP; 
                            alarm(0);
                            printf("Received DISC\n"); 
                        }
                        else currState = START;                    
                        break;
                    default:
                        break;

                }
            }
        }
        if (currState == STOP) {
            bufW[0] = FLAG;
            bufW[1] = A_TRANSMITTER;    
            bufW[2] = C_UA;
            bufW[3] = bufW[1] ^ bufW[2];
            bufW[4] = FLAG;

            int resW = write(fd, bufW, 5);
            if (resW != 5) {
                perror("write");
                if (tcsetattr(fd, TCSANOW, &oldtio) == -1) perror("tcsetattr");
                close(fd);
                return -1;
            }

            printf("Sent UA\n");
            if (tcsetattr(fd, TCSANOW, &oldtio) == -1) perror("tcsetattr");
            close(fd);
            return 1;
        }

        if (connectionParameters.nRetransmissions == alarmCounter) {
            printf("Reached maximum number of retransmissions\n");
            if (tcsetattr(fd, TCSANOW, &oldtio) == -1) perror("tcsetattr");
            close(fd);
            return -1;
        }

        if (tcsetattr(fd, TCSANOW, &oldtio) == -1) perror("tcsetattr");
        close(fd);
        return -1;
    }
    else if (connectionParameters.role == LLRX) {
        // for testing purposes
        /*
        while (currState != STOP) {
            if (read(fd, &byte, 1) > 0) {
                switch (currState) {
                    case START:
                        if (byte == FLAG) currState = FLAG_RCV;
                        break;
                    case FLAG_RCV:
                        if (byte == A_TRANSMITTER) currState = A_RCV;
                        else if (byte != FLAG) currState = START;
                        break;
                    case A_RCV:
                        if (byte == C_DISC) currState = C_RCV;
                        else if (byte == FLAG) currState = FLAG_RCV;
                        else currState = START;
                        break;
                    case C_RCV:
                        if (byte == (C_DISC ^ A_TRANSMITTER)) currState = BCC_OK;
                        else if (byte == FLAG) currState = FLAG_RCV;
                        else currState = START;
                        break;
                    case BCC_OK:
                        if (byte == FLAG) { 
                            currState = STOP; 
                            printf("Received DISC\n"); 
                        }
                        else currState = START;                    
                        break;
                    default:
                        break;

                }
            }
            
        }
        */
        bufW[0] = FLAG;
        bufW[1] = A_RECEIVER;
        bufW[2] = C_DISC;
        bufW[3] = bufW[1] ^ bufW[2];
        bufW[4] = FLAG;

        currState = START;
        alarmCounter = 0;
        alarmEnabled = FALSE;

        while (connectionParameters.nRetransmissions > alarmCounter) {
            if (alarmEnabled == FALSE) {
                int resW = write(fd, bufW, 5);
                if (resW != 5) {
                    perror("write");
                    if (tcsetattr(fd, TCSANOW, &oldtio) == -1) perror("tcsetattr");
                    close(fd);
                    return -1;
                }
                printf("Sent DISC\n");
                alarm(connectionParameters.timeout);
                alarmEnabled = TRUE;
            }
            if (read(fd, &byte, 1) > 0) {
                switch (currState) {
                    case START:
                        if (byte == FLAG) currState = FLAG_RCV;
                        break;
                    case FLAG_RCV:
                        if (byte == A_TRANSMITTER) currState = A_RCV;
                        else if (byte == FLAG) currState = FLAG_RCV;
                        else currState = START;
                        break;
                    case A_RCV:
                        if (byte == C_UA) currState = C_RCV;
                        else if (byte == FLAG) currState = FLAG_RCV;
                        else currState = START;
                        break;
                    case C_RCV:
                        if (byte == (C_UA ^ A_TRANSMITTER)) currState = BCC_OK;
                        else if (byte == FLAG) currState = FLAG_RCV;
                        else currState = START;
                        break;
                    case BCC_OK:
                        if (byte == FLAG) { 
                            currState = STOP; 
                            alarm(0);
                            printf("Received UA\n"); 
                            if (tcsetattr(fd, TCSANOW, &oldtio) == -1) perror("tcsetattr");
                            close(fd);
                            return 1;
                        }
                        else currState = START;                    
                        break;
                    default:
                        break;

                }
            }
        }
    }
    else printf("Invalid role\n");
    if (tcsetattr(fd, TCSANOW, &oldtio) == -1) perror("tcsetattr");
    close(fd);
    return -1;
}

unsigned char* byteStuffing(unsigned char* buf, int bufSize, int* stuffedBufSize) {
    *stuffedBufSize = bufSize;

    for (int i = 0; i < bufSize; i++) {
        if (buf[i] == ESC || buf[i] == FLAG) (*stuffedBufSize)++;
    }

    unsigned char *res = (unsigned char *)malloc((*stuffedBufSize) * sizeof(unsigned char));

    if (res == NULL) {
        perror("malloc");
        return NULL;
    }

    int j = 0;
    for (int i = 0; i < bufSize; i++) {
        if (buf[i] == ESC || buf[i] == FLAG) {
            res[j++] = ESC;
            res[j++] = buf[i] ^ 0x20;
            continue;
        }
        res[j++] = buf[i];
    }
    return res;
}

unsigned char* byteDestuffing(unsigned char* stuffedBuf, int stuffedBufSize, int* destuffedBufSize) {
    unsigned char* destuffedBuf = (unsigned char*)malloc(stuffedBufSize * sizeof(unsigned char));

    if (destuffedBuf == NULL) {
        perror("malloc");
        return NULL;
    }

    int i = 0, j = 0;
    while (i < stuffedBufSize) {
        if (stuffedBuf[i] == ESC) {
            i++;
            destuffedBuf[j] = stuffedBuf[i] ^ 0x20;
        } else {
            destuffedBuf[j] = stuffedBuf[i];
        }
        i++;
        j++;
    }

    *destuffedBufSize = j;

    destuffedBuf = (unsigned char*)realloc(destuffedBuf, (*destuffedBufSize) * sizeof(unsigned char));

    return destuffedBuf;
}