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

    if (!strcmp(role,"tx")) linkLayer.role = LLTX;
    else if (!strcmp(role, "rx")) linkLayer.role = LLRX;
    else {
        printf("Invalid role\n");
        exit(1);
    }

    int fd = llopen(linkLayer);
    if (fd == -1) {
        printf("Connection failed.\n");
        exit(1);
    }
    else printf("Connection established.\n");

    switch (linkLayer.role) {
        case LLTX:
            FILE* file = fopen(filename, "rb");

            if (file == NULL) {
                printf("Error opening file.\n");
                exit(1);
            }
            
            fseek(file, 0, SEEK_END);
            unsigned int fileSize = ftell(file);
            fseek(file, 0, SEEK_SET);

            unsigned int controlPacketSize = fileSize;
            unsigned char* controlPacket = createControlPacket(CP_START, &controlPacketSize);

            if (llwrite(fd, linkLayer, controlPacket, controlPacketSize) == -1) {
                printf("Error occurred!\n");
                break;
            }

            unsigned char* data = (unsigned char*)malloc(fileSize * sizeof(unsigned char));
            fread(data, sizeof(unsigned char), fileSize, file);

            unsigned long long remainingBytes = fileSize;
            int errorOccurred = FALSE;

            while (remainingBytes >= 0) {
                unsigned int packetSize = remainingBytes > MAX_PAYLOAD_SIZE ? MAX_PAYLOAD_SIZE : remainingBytes;
                unsigned int dataSize = packetSize;
                unsigned char* packet = createDataPacket(data, &packetSize);

                if (llwrite(fd, linkLayer, packet, packetSize) == -1) {
                    printf("Error occurred!\n");
                    errorOccurred = TRUE;
                    break;
                }

                remainingBytes -= (long long) dataSize;
                data += dataSize;
                free(packet);
            }

            if (errorOccurred) break;
            
            controlPacketSize = 0;
            unsigned char* endPacket = createControlPacket(CP_END, &controlPacketSize);

            if (llwrite(fd, linkLayer, endPacket, controlPacketSize) == -1) {
                printf("Error occurred!\n");
                break;
            }

            free(data);
            free(controlPacket);
            free(endPacket);
            fclose(file);
            break;
        case LLRX:
            unsigned char* controlPacket = (unsigned char*)malloc(MAX_PAYLOAD_SIZE * sizeof(unsigned char));
            unsigned int controlPacketSize = llread(fd, linkLayer, controlPacket);

            if (controlPacketSize == -1) {
                printf("Error occurred!\n");
                break;
            }

            unsigned long long fileSize = 0;
            controlPacket = parseControlPacket(controlPacket, controlPacketSize, &fileSize);

            if (controlPacket == NULL) {
                printf("Error occurred!\n");
                break;
            }

            FILE* file = fopen(filename, "wb");
            while (TRUE) {
                unsigned char* dataPacket = (unsigned char*)malloc(MAX_PAYLOAD_SIZE * sizeof(unsigned char));
                unsigned int dataPacketSize = llread(fd, linkLayer, dataPacket);

                if (dataPacketSize == -1) {
                    printf("Error occurred!\n");
                    break;
                }

                if (dataPacket[0] == CP_END) break;

                unsigned int dataSize = parseDataPacket(dataPacket, dataPacketSize, dataPacket);

                if (dataPacket == NULL) {
                    printf("Error occurred!\n");
                    break;
                }

                fwrite(dataPacket, sizeof(unsigned char), dataSize, file);
                free(dataPacket);
            }

            free(controlPacket);
            break;
        default:
            printf("Invalid role\n");
            exit(1);
    }

    if (llclose(fd, linkLayer, FALSE) == -1) {
        printf("Error occurred!\n");
        exit(1);
    }
    else printf("Connection finished.\n");
    return 0;
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

            for (unsigned int i = 0; i < fileSizeBytes; i++){
                packet[CP_HEADER_SIZE + i] = ((*packetSize) >> (8 * (fileSizeBytes - i - 1))) & 0xFF;
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

unsigned char* parseControlPacket(unsigned char* packet, unsigned int packetSize, unsigned int* fileSize) {
    if (packet[0] != CP_START && packet[0] != CP_END) {
        printf("Invalid control packet.\n");
        return NULL;
    }

    if (packet[0] == CP_START) {
        if (packet[1] != CP_T_FILE_SIZE) {
            printf("Invalid control packet.\n");
            return NULL;
        }

        unsigned int fileSizeBytes = packet[2];
        *fileSize = 0;

        for (unsigned int i = 0; i < fileSizeBytes; i++) {
            *fileSize |= packet[CP_HEADER_SIZE + i] << (8 * (fileSizeBytes - i - 1));
        }
    }

    return packet;
}

unsigned int parseDataPacket(unsigned char* packet, unsigned int packetSize, unsigned char* data) {
    if (packet[0] != DP_DATA) {
        printf("Invalid data packet.\n");
        return NULL;
    }

    unsigned int dataSize = (packet[1] << 8) | packet[2];

    for (unsigned int i = 0; i < dataSize; i++) {
        data[i] = packet[DP_HEADER_SIZE + i];
    }

    return dataSize;
}