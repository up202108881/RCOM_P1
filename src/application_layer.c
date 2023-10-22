// Application layer protocol implementation

#include "application_layer.h"
#include "link_layer.h"

void applicationLayer(const char* serialPort, const char* role, int baudRate,
                      int nTries, int timeout, const char* filename)
{
    LinkLayer linkLayer;
    strcpy(linkLayer.serialPort, serialPort);
    linkLayer.baudRate = baudRate;
    linkLayer.nRetransmissions = nTries;
    linkLayer.timeout = timeout;
    if (!strcmp(role,"tx")) {
        linkLayer.role = LLTX;
    }
    else if (!strcmp(role, "rx")) {
        linkLayer.role = LLRX;
    }
    else {
        printf("Invalid role\n");
        exit(1);
    }
    int fd = llopen(linkLayer);
    if (fd == -1) printf("DOES NOT WORK! OPEN\n");
    else printf("WORKS! OPEN\n");
    if (llclose(fd, linkLayer, FALSE) == -1) printf("DOES NOT WORK! CLOSE\n");
    else printf("WORKS! CLOSE\n");
}


unsigned char* createControlPacket(unsigned char controlField, unsigned int* packetSize) {
    switch (controlField) {
        case CP_START:
            unsigned int fileSize = *packetSize;
            unsigned int fileSizeBytes = 0;

            while(fileSize != 0){
                fileSizeBytes++;
                fileSize = fileSize >> 8;
            }
            
            unsigned char* packet =  (unsigned char*)malloc(CP_HEADER_SIZE + fileSizeBytes);
            packet[0] = controlField;
            packet[1] = CP_T_FILE_SIZE;
            packet[2] = fileSizeBytes;

            for(unsigned int i = 0; i < fileSizeBytes; i++){
                packet[3 + i] = ((*packetSize) >> (8 * (fileSizeBytes - i - 1))) & 0xFF;
            }

            *packetSize = CP_HEADER_SIZE + fileSizeBytes;
            return packet;
        
        case CP_END:
            unsigned char* packet[1] = {controlField};
            return packet;

        default:
            printf("Invalid role\n");
            return NULL;
    }
}

unsigned char* createDataPacket(unsigned char* data, unsigned int* packetSize) {
    unsigned int dataSize = *packetSize;

    *packetSize += DP_HEADER_SIZE;

    unsigned char* packet = (unsigned char*)malloc((*packetSize) * sizeof(unsigned char));
    packet[0] = DP_DATA;
    packet[1] = (dataSize >> 8) & 0xFF;
    packet[2] = dataSize & 0xFF;
    
    for (int i = 0; i < dataSize; i++) {
        packet[DP_HEADER_SIZE + i] = data[i];
    }

    return packet;
}