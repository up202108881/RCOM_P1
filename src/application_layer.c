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
        printf("Invalid role.\n");
        return;
    }
    
    system("clear");
    printf("Establishing connection...\n");
    
    clock_t t;
    
    t = clock();
    
    int fd = llopen(linkLayer);
    
    t = clock() - t;
    
    Statistics stats;
    stats.open_time = ((double)t) / CLOCKS_PER_SEC;
            
    if (fd == -1) {
        printf("Connection failed.\n");
        return;
    }
    else printf("Connection established.\n");
    
    int n = 0;
    double sum = 0, sum_debit = 0;

    switch (linkLayer.role) {
        case LLTX: {
            FILE* file = fopen(filename, "rb");

            if (file == NULL) {
                printf("Error opening file.\n");
                return;
            }
            
            fseek(file, 0, SEEK_END);
            unsigned int fileSize = ftell(file);
            fseek(file, 0, SEEK_SET);
            
            unsigned int controlPacketSize = fileSize;
            unsigned char* controlPacket = createControlPacket(CP_START, &controlPacketSize);

            printf("Sending data...\n");
            
            t = clock();
            
            int check = llwrite(fd, linkLayer, controlPacket, controlPacketSize);
            
            t = clock() - t;
            
            n++;
            sum += ((((double)t)) / CLOCKS_PER_SEC);
            sum_debit += check / ((((double)t)) / CLOCKS_PER_SEC);
                        
            if (check == -1) {
                printf("Error occurred!\n");
                break;
            }

            free(controlPacket);

            unsigned char* data = (unsigned char*)malloc(fileSize * sizeof(unsigned char));
            fread(data, sizeof(unsigned char), fileSize, file);

            unsigned long long remainingBytes = fileSize;
            int errorOccurred = FALSE;

            while (remainingBytes > 0) {
                unsigned int packetSize = remainingBytes > (MAX_PAYLOAD_SIZE - 3) ? (MAX_PAYLOAD_SIZE - 3) : remainingBytes;
                unsigned int dataSize = packetSize;
                unsigned char* packet = createDataPacket(data, &packetSize);
                
                t = clock();
                
                long long bytesWritten = llwrite(fd, linkLayer, packet, packetSize);
            
                t = clock() - t;
            
                n++;
                sum += ((((double)t)) / CLOCKS_PER_SEC);
                sum_debit += bytesWritten / ((((double)t)) / CLOCKS_PER_SEC);
                
                if (bytesWritten == -1) {
                    printf("Error occurred!\n");
                    errorOccurred = TRUE;
                    break;
                }

                printf("Bytes written: %d\n", dataSize);
                printf("Bytes left: %lld\n", remainingBytes);

                remainingBytes -= (long long) dataSize;
                data += dataSize;
                free(packet);
            }

            if (errorOccurred) break;
            
            controlPacketSize = 0;
            unsigned char* endPacket = createControlPacket(CP_END, &controlPacketSize);
            
            t = clock();
            
            check = llwrite(fd, linkLayer, endPacket, controlPacketSize);
            
            t = clock() - t;
            
            if (check == -1) {
                printf("Error occurred!\n");
                break;
            }
         
            n++;
            sum += ((((double)t)) / CLOCKS_PER_SEC);
            sum_debit += check / ((((double)t)) / CLOCKS_PER_SEC);
             
            free(endPacket);
            fclose(file);
            break;
        }
        case LLRX: {
            unsigned char* controlPacket = (unsigned char*)malloc(MAX_PAYLOAD_SIZE * sizeof(unsigned char));
            
            printf("Receiving data...\n");
            
            t = clock();
            
            unsigned int controlPacketSize = llread(fd, linkLayer, controlPacket);
            
            t = clock() - t;
            
            n++;
            sum += ((((double)t)) / CLOCKS_PER_SEC);
            
            unsigned long long fileSize = 0;
            while (parseControlPacket(controlPacket, controlPacketSize, &fileSize) < 0) {
                controlPacketSize = llread(fd, linkLayer, controlPacket);
            }

            FILE* newFile = fopen(filename, "wb");
            while (TRUE) {
                unsigned char* dataPacket = (unsigned char*)malloc(MAX_PAYLOAD_SIZE * sizeof(unsigned char));
                
                t = clock();
                
                unsigned int dataPacketSize = llread(fd, linkLayer, dataPacket);
                
                t = clock() - t;
                
                n++;
                sum += ((((double)t)) / CLOCKS_PER_SEC);
                   
                if (dataPacketSize == -1) {
                    break;
                }

                if (dataPacket[0] == CP_END) break;

                unsigned char *receivedData = (unsigned char*)malloc(MAX_PAYLOAD_SIZE * sizeof(unsigned char));
                int dataSize = parseDataPacket(dataPacket, dataPacketSize, receivedData);

                if (receivedData == NULL) break;

                fwrite(receivedData, sizeof(unsigned char), dataSize, newFile);
                free(dataPacket);
            }

            free(controlPacket);
            fclose(newFile);
            break;
        }
        default:
            printf("Invalid role.\n");
            return;
    }
    
    printf("Disconnecting...\n");
    
    stats.data_time = sum / n;
    stats.debit = sum_debit / n;
    
    if (llclose(fd, linkLayer, TRUE, stats) == -1) {
        printf("Error occurred while disconnecting!\n");
        return;
    }
    
    else printf("Connection finished.\n");
}

unsigned char* createControlPacket(unsigned char controlField, unsigned int* packetSize) {
    switch (controlField) {
        case CP_START: {
            unsigned int fileSize = *packetSize;
            unsigned int fileSizeBytes = 0;

            while (fileSize != 0) {
                fileSizeBytes++;
                fileSize = fileSize >> 8;
            }
            
            unsigned char* packet = (unsigned char*)malloc(CP_HEADER_SIZE + fileSizeBytes);
            packet[0] = controlField;
            packet[1] = CP_T_FILE_SIZE;
            packet[2] = fileSizeBytes;

            for (unsigned int i = 0; i < fileSizeBytes; i++) {
                packet[CP_HEADER_SIZE + i] = ((*packetSize) >> (8 * (fileSizeBytes - i - 1))) & 0xFF;
            }

            *packetSize = CP_HEADER_SIZE + fileSizeBytes;
            return packet;
        }
        case CP_END: {
            unsigned char* packet = (unsigned char*)malloc(1);
            packet[0] = CP_END;
            *packetSize = 1;
            return packet;
        }
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

int parseControlPacket(unsigned char* packet, unsigned int packetSize, unsigned long long* fileSize) {
    if (packet[0] != CP_START && packet[0] != CP_END) return -1;

    if (packet[0] == CP_START) {
        if (packet[1] != CP_T_FILE_SIZE) return -1;

        unsigned int fileSizeBytes = packet[2];
        *fileSize = 0;

        for (unsigned int i = 0; i < fileSizeBytes; i++) {
            *fileSize |= packet[CP_HEADER_SIZE + i] << (8 * (fileSizeBytes - i - 1));
        }
    }
    return 0;
}

int parseDataPacket(unsigned char* packet, unsigned int packetSize, unsigned char* data) {
    if (packet[0] != DP_DATA) return -1;

    unsigned int dataSize = (packet[1] << 8) | packet[2];

    for (unsigned int i = 0; i < dataSize; i++) {
        data[i] = packet[DP_HEADER_SIZE + i];
    }

    return dataSize;
}
