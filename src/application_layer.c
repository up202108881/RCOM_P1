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
    if(!strcmp(role,"tx")){
        linkLayer.role = LlTx;
    }
    else if(!strcmp(role,"rx")){
        linkLayer.role = LlRx;
    }
    else{
        printf("Invalid role\n");
        exit(1);
    }
    if(llopen(linkLayer) == -1)printf("Ardeu!");
    else printf("NOT ARDEU!");
}
