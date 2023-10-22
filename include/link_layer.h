// Link layer header.
// NOTE: This file must not be changed.

#ifndef _LINK_LAYER_H_
#define _LINK_LAYER_H_

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

#define FLAG 0x7E
#define ESC 0x7D
#define A_TRANSMITTER 0x03
#define A_RECEIVER 0x01
#define C_SET 0x03
#define C_UA 0x07
#define C_DISC 0x0B
#define C_RR(Nr) ((Nr << 7) | 0x05)
#define C_REJ(Nr) ((Nr << 7) | 0x01)
#define C_INFO_FRAME(Ns) (Ns << 6)

typedef enum {
    START,
    FLAG_RCV,
    A_RCV,
    C_RCV,
    BCC_OK,
    STOP
} State;

typedef enum
{
    LLTX,
    LLRX,
} LinkLayerRole;

typedef struct
{
    char serialPort[50];
    LinkLayerRole role;
    int baudRate;
    int nRetransmissions;
    int timeout;
} LinkLayer;

// SIZE of maximum acceptable payload.
// Maximum number of bytes that application layer should send to link layer
#define MAX_PAYLOAD_SIZE 1000

// Frame Header Size
#define FH_SIZE 4

// Frame Trailer Size
#define FT_SIZE 2

#define TX_FRAME 0
#define RX_FRAME 1

// MISC
#define FALSE 0
#define TRUE 1

// Open a connection using the "port" parameters defined in struct linkLayer.
// Returns the file descriptor on success or "-1" on error.
int llopen(LinkLayer connectionParameters);

// Send data in buf with size bufSize.
// Return number of chars written, or "-1" on error.
int llwrite(int fd, LinkLayer connectionParameters, const unsigned char *buf, int bufSize);

// Receive data in packet.
// Return number of chars read, or "-1" on error.
int llread(int fd, LinkLayer connectionParameters, unsigned char *packet);

// Close previously opened connection.
// if showStatistics == TRUE, link layer should print statistics in the console on close.
// Return "1" on success or "-1" on error.
int llclose(int fd, LinkLayer connectionParameters, int showStatistics);

// Handles byte stuffing on the data
// Returns the stuffed data and the size of the stuffed data
unsigned char* byteStuffing(unsigned char* buf, int bufSize, int* stuffedBufSize);

// Handles byte destuffing on the data
// Returns the destuffed data and the size of the destuffed data
unsigned char* byteDestuffing(unsigned char* stuffedBuf, int stuffedBufSize, int* destuffedBufSize);

#endif // _LINK_LAYER_H_
