// Application layer protocol implementation

#include "application_layer.h"
#include "link_layer.h"

void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename)
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


unsigned char* createControlPacket(unsigned char controlField, unsigned int * packetSize){
    switch (controlField)
    {
    case 2:
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
    
    case 3:
        unsigned char* packet[1] = {controlField};
        return packet;
    default:
        printf("Invalid role\n");
        return NULL;
    }
    
}

unsigned char* createDataPacket(unsigned char * data, unsigned int * packetSize){
    unsigned int dataSize = *packetSize;
    unsigned int dataSizeBytes = 0;

    while(dataSize != 0){
        dataSizeBytes++;
        dataSize = dataSize >> 8;
    }
    
    unsigned char* packet =  (unsigned char*)malloc(DP_HEADER_SIZE + dataSizeBytes);
    packet[0] = 1;
    packet[1] = dataSizeBytes >> 8;
    packet[2] = dataSizeBytes & 0xFF;
    
    for(unsigned int i = 0; i < dataSizeBytes; i++){
        packet[3 + i] = ((*packetSize) >> (8 * (dataSizeBytes - i - 1))) & 0xFF;
    }
    *packetSize = DP_HEADER_SIZE + dataSizeBytes;
    return packet;
}