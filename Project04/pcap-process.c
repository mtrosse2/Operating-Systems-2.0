
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>

#include "pcap-process.h"

#define MAX_KEY_LEN 100
#define MAX_VALUE_LEN 100
#define TABLE_SIZE 1000

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

void print_all_hitcounts();

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
    // struct PacketEntry existing = TheHash[index];

    // update globals with entry being replaced
		// printf("Updating HitCount by %d\n", TheHash[index].HitCount);
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
uint16_t jenkins_16bit(uint16_t key) {
    uint16_t hash = 0;
    hash += key;
    hash += hash << 10;
    hash ^= hash >> 6;
    hash += hash << 3;
    hash ^= hash >> 11;
    hash += hash << 15;
    return hash;
}

u_int16_t hashData(int netPayload) {
    return (unsigned int) (jenkins_16bit(netPayload) % TABLE_SIZE);
}

uint16_t getNetPayload(struct Packet * pPacket){
    return pPacket->Data[pPacket->PayloadOffset];
		//return pPacket->LengthIncluded - pPacket->PayloadOffset;
}

uint16_t getNetPayloadSize(struct Packet * pPacket){
		return pPacket->LengthIncluded - pPacket->PayloadOffset;
}

void insert(struct Packet * pPacket) {
    u_int16_t index = hashData(getNetPayload(pPacket));
    pthread_mutex_lock(&HashLock);

    // struct PacketEntry existing = TheHash[index];

    if (TheHash[index].ThePacket == NULL) {
        //printf("Inserting new packet entry at index %d\n", index);
        TheHash[index].ThePacket = pPacket;
    }
    else {
				//printf("Conflict at index: %d\n", index);
        //if (TheHash[index].ThePacket->PayloadSize == pPacket->PayloadSize && memcmp(TheHash[index].ThePacket) ) { //&& getNetPayload(TheHash[index].ThePacket) == getNetPayload(pPacket)) {
					if(getNetPayloadSize(TheHash[index].ThePacket) == getNetPayloadSize(pPacket) && getNetPayload(TheHash[index].ThePacket) == getNetPayload(pPacket) ) {
						//printf("Hit\n");
						//printf("Should update hitcount %d\n", existing.HitCount);
            TheHash[index].HitCount++;
						//printf("after: %d\n", TheHash[index].HitCount);
            TheHash[index].RedundantBytes += pPacket->PayloadSize;

            discardPacket(pPacket);
        }
        else {
						//printf("Miss\n");
						//printf("Should not update hitcount\n");
            if (TheHash[index].HitCount < 1) {
                // printf("Replacing packet entry at index %d\n", index);
                replaceSaveEntry(index, pPacket);
            }
            else {
                // printf("Discarding new packet\n");
                discardPacket(pPacket);
            }
        }
    }
    pthread_mutex_unlock(&HashLock);
		// print_all_hitcounts();
}

void print_all_hitcounts(){
	int i;
	printf("--------------------------------------------------------\n");
	for(i = 0; i < TABLE_SIZE; i++){
		if (TheHash[i].ThePacket != NULL){
			printf("%d, ", TheHash[i].HitCount);
		}
	}
	printf("\n--------------------------------------------------------\n");
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

    /* Step 2: Do any packet payloads match up? */


    insert(pPacket);
    return;
}