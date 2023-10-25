// Application layer protocol header.
// NOTE: This file must not be changed.

#ifndef _APPLICATION_LAYER_H_
#define _APPLICATION_LAYER_H_

// Control packet header size.
#define CP_HEADER_SIZE 3
#define DP_HEADER_SIZE 3

#define CP_START 0x02
#define CP_END 0x03
#define DP_DATA 0x01

#define CP_T_FILE_SIZE 0

// Application layer main function.
// Arguments:
//   serialPort: Serial port name (e.g., /dev/ttyS0).
//   role: Application role {"tx", "rx"}.
//   baudrate: Baudrate of the serial port.
//   nTries: Maximum number of frame retries.
//   timeout: Frame timeout.
//   filename: Name of the file to send / receive.
void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename);

unsigned char* createControlPacket(unsigned char controlField, unsigned int* packetSize);

unsigned char* createDataPacket(unsigned char* data, unsigned int* packetSize);

int parseControlPacket(unsigned char* packet, unsigned int packetSize, unsigned int* fileSize);

int parseDataPacket(unsigned char* packet, unsigned int packetSize, unsigned char* data);

#endif // _APPLICATION_LAYER_H_
