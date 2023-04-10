/* packet.c : Implementation file for packet support functions */

#include <stdlib.h>
#include <stdio.h>

#include "packet.h"


struct Packet * allocatePacket (uint16_t DataSize)
{
    struct Packet * pPacket;

    pPacket = (struct Packet *) malloc(sizeof(struct Packet));

    if(pPacket == NULL)
    {
        printf("Error - malloc failed for a new packet allocation\n");
        return NULL;
    }

    pPacket->Data = (uint8_t *) malloc(sizeof(uint8_t) * DataSize);

    if(pPacket->Data == NULL)
    {
        printf("Error - malloc failed for the buffer within the packet\n");
        free(pPacket);
        return NULL;
    }

    /* Everything is good at this point */

    pPacket->SizeDataMax = DataSize;
    pPacket->LengthIncluded = 0;
    pPacket->LengthOriginal = 0;
    pPacket->TimeCapture.tv_sec = 0;
    pPacket->TimeCapture.tv_usec = 0;

    pPacket->PayloadOffset = 0;
    pPacket->PayloadSize = 0;

    return pPacket;
}

void discardPacket (struct Packet * pPacket)
{
    /* Free up the internal buffer */
    if(pPacket->Data != NULL)
    {
        free(pPacket->Data);
        pPacket->SizeDataMax = 0;
    }

    /* Free up the actual struct itself */
    free(pPacket);
}
