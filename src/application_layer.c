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
    if (linkLayer.role == LLTX && llclose(fd, linkLayer, FALSE) == -1) printf("DOES NOT WORK! CLOSE\n");
    else if (linkLayer.role == LLTX) printf("WORKS! CLOSE\n");
}
