#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>

#include "pcap-process.h"

// #define MAX_KEY_LEN 100
// #define MAX_VALUE_LEN 100
#define TABLE_SIZE 9973

/* How many packets have we seen? */
uint32_t        gPacketSeenCount;
/* How many total bytes have we seen? */
uint64_t        gPacketSeenBytes;        
/* How many hits have we had? */
uint32_t        gPacketHitCount;
/* How much redundancy have we seen? */
uint64_t        gPacketHitBytes;

struct PacketEntry * TheHash;
pthread_mutex_t HashLock;

void initializeProcessing() {
    // initialize stats
    gPacketSeenCount = 0;
    gPacketSeenBytes = 0;        
    gPacketHitCount  = 0;
    gPacketHitBytes  = 0; 

    TheHash = (struct PacketEntry *) malloc(sizeof(struct PacketEntry) * TABLE_SIZE);
    if(TheHash == NULL)
    {
        printf("* Error: Unable to create the new table\n");
        return;
    }

    // initialize the hash lock
    pthread_mutex_init(&HashLock, 0);

    // initialize the hash table
    for (int j = 0; j < TABLE_SIZE; j++) {
        TheHash[j].ThePacket = NULL;
        TheHash[j].HitCount = 0;
        TheHash[j].RedundantBytes = 0;
    }

    return;
}

void replaceSaveEntry(int index, struct Packet * pPacket) {
    // update globals with entry being replaced
    gPacketHitCount += TheHash[index].HitCount;
    gPacketHitBytes += TheHash[index].RedundantBytes;

    struct Packet * dPacket = TheHash[index].ThePacket;

    // insert new entry in hash
    TheHash[index].ThePacket = pPacket;
    TheHash[index].HitCount = 0;
    TheHash[index].RedundantBytes = 0;

    discardPacket(dPacket);

    return;
}

void tallyProcessing() {

    for (int i = 0; i < TABLE_SIZE; i++) {
        struct PacketEntry * pEntry = &TheHash[i];
        
        if (pEntry->ThePacket != NULL) {
            // update globals with entry being replaced
            gPacketHitCount += pEntry->HitCount;
            gPacketHitBytes += pEntry->RedundantBytes;

            // remove packet
            discardPacket(pEntry->ThePacket);
        }

    }
    free(TheHash);
    return;
}

// https://en.wikipedia.org/wiki/Jenkins_hash_function
uint32_t hashData(uint8_t * key, size_t length) {
    size_t i = 0;
    uint32_t hash = 0;
    while (i != length) {
        hash += key[i++];
        hash += hash << 10;
        hash ^= hash >> 6;
    }
    hash += hash << 3;
    hash ^= hash >> 11;
    hash += hash << 15;
    return (uint32_t) hash % TABLE_SIZE;
}

// return the size of the data payload to be read
uint16_t getNetPayloadSize(struct Packet * pPacket){
	return pPacket->LengthIncluded - pPacket->PayloadOffset;
}

// insert new packet into hash table
void insert(struct Packet * pPacket) {
    
    uint8_t* buffer[getNetPayloadSize(pPacket)];

    // copy the packet data starting form the offset into a buffer
    memcpy(buffer, &pPacket->Data[pPacket->PayloadOffset], getNetPayloadSize(pPacket));

    // hash the data in the buffer to determine the index in the hash
    uint32_t index = hashData((uint8_t *)buffer, getNetPayloadSize(pPacket));


    if (TheHash[index].ThePacket == NULL) { // if there is nothing at the index just insert the new packet
        pthread_mutex_lock(&HashLock);
        TheHash[index].ThePacket = pPacket;
        pthread_mutex_unlock(&HashLock);
    }
    else if (getNetPayloadSize(TheHash[index].ThePacket) == getNetPayloadSize(pPacket) 
                && memcmp(&TheHash[index].ThePacket->Data[pPacket->PayloadOffset], &pPacket->Data[pPacket->PayloadOffset], pPacket->PayloadSize - pPacket->PayloadOffset) == 0){
        
        // if size and data payloads are the same update the hit count and redundant byes
        pthread_mutex_lock(&HashLock);
        TheHash[index].HitCount++;
        TheHash[index].RedundantBytes += pPacket->PayloadSize;
        discardPacket(pPacket);
        pthread_mutex_unlock(&HashLock);
    }
    else {
        // if the sizes and data are not the same evict the existing packet if the hit count is less than 1, otherwise evict the new packet
        if (TheHash[index].HitCount < 1) {
            pthread_mutex_lock(&HashLock);
            replaceSaveEntry(index, pPacket);
            pthread_mutex_unlock(&HashLock);
        }
        else {
            discardPacket(pPacket);
        }
    
    }
    
}

void processPacket(struct Packet * pPacket) {
    uint16_t        PayloadOffset;

    PayloadOffset = 0;

    /* Do a bit of error checking */
    if(pPacket == NULL)
    {
        // printf("* Warning: Packet to assess is null - ignoring\n");
        return;
    }

    if(pPacket->Data == NULL)
    {
        // printf("* Error: The data block is null - ignoring\n");
        return;
    }

    // printf("STARTFUNC: processPacket (Packet Size %d)\n", pPacket->LengthIncluded);

    /* Step 1: Should we process this packet or ignore it? 
     *    We should ignore it if:
     *      The packet is too small
     *      The packet is not an IP packet
     */

    /* Update our statistics in terms of what was in the file */
    gPacketSeenCount++;
    gPacketSeenBytes += pPacket->LengthIncluded;

    /* Is this an IP packet (Layer 2 - Type / Len == 0x0800)? */

    if(pPacket->LengthIncluded <= MIN_PKT_SIZE)
    {
        discardPacket(pPacket);
        return;
    }

    if((pPacket->Data[12] != 0x08) || (pPacket->Data[13] != 0x00))
    {
        // printf("Not IP - ignoring...\n");
        discardPacket(pPacket);
        return;
    }

    /* Adjust the payload offset to skip the Ethernet header 
        Destination MAC (6 bytes), Source MAC (6 bytes), Type/Len (2 bytes) */
    PayloadOffset += 14;

    /* Step 2: Figure out where the payload starts 
         IP Header - Look at the first byte (Version / Length)
         UDP - 8 bytes 
         TCP - Look inside header */

    if(pPacket->Data[PayloadOffset] != 0x45)
    {
        /* Not an IPv4 packet - skip it since it is IPv6 */
        // printf("  Not IPV4 - Ignoring\n");
        discardPacket(pPacket);
        return;
    }
    else
    {
        /* Offset will jump over the IPv4 header eventually (+20 bytes)*/
    }

    /* Is this a UDP packet or a TCP packet? */
    if(pPacket->Data[PayloadOffset + 9] == 6)
    {
        /* TCP */
        uint8_t TCPHdrSize;

        TCPHdrSize = ((uint8_t) pPacket->Data[PayloadOffset+9+12] >> 4) * 4;
        PayloadOffset += 20 + TCPHdrSize;
    }
    else if(pPacket->Data[PayloadOffset+9] == 17)
    {
        /* UDP */

        /* Increment the offset by 28 bytes (20 for IPv4 header, 8 for the UDP header)*/
        PayloadOffset += 28;
    }
    else 
    {
        /* Don't know what this protocol is - probably not helpful */
        discardPacket(pPacket);
        return;
    }

    // printf("  processPacket -> Found an IP packet that is TCP or UDP\n");

    uint16_t    NetPayload;

    NetPayload = pPacket->LengthIncluded - PayloadOffset;

    pPacket->PayloadOffset = PayloadOffset;
    pPacket->PayloadSize = NetPayload;


    // call insert to handle updating the hash data structure with the packet being processed
    insert(pPacket);
    return;
}
