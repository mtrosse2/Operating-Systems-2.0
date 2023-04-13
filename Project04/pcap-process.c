
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>

#include "pcap-process.h"

#define MAX_KEY_LEN 100
#define MAX_VALUE_LEN 100
#define TABLE_SIZE 100

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
    struct PacketEntry existing = TheHash[index];

    // update globals with entry being replaced
    gPacketHitCount += existing.HitCount;
    gPacketHitBytes += existing.RedundantBytes;

    struct Packet * dPacket = existing.ThePacket;

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
        printf("in here\n");

        if (pEntry->ThePacket != NULL) {
            // update globals with entry being replaced
            gPacketHitCount += pEntry->HitCount;
            gPacketHitBytes += pEntry->RedundantBytes;

            // remove packet
            discardPacket(pEntry->ThePacket);
        }

        free(pEntry);
    }

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
    return pPacket->LengthIncluded - pPacket->PayloadOffset;
}

void insert(struct Packet * pPacket) {
    u_int16_t index = hashData(getNetPayload(pPacket));

    struct PacketEntry existing = TheHash[index];

    if (existing.ThePacket == NULL) {
        printf("Inserting new packet entry at index %d\n", index);
        TheHash[index].ThePacket = pPacket;
    }
    else {
        if (existing.ThePacket->PayloadSize == pPacket->PayloadSize && getNetPayload(existing.ThePacket) == getNetPayload(pPacket)) {
            printf("Net payloads match\n");
            existing.HitCount++;
            existing.RedundantBytes += pPacket->PayloadSize;

            discardPacket(pPacket);
        }
        else {
            if (existing.HitCount < 1) {
                printf("Replacing packet entry at index %d\n", index);
                replaceSaveEntry(index, pPacket);
            }
            else {
                printf("Discarding new packet\n");
                discardPacket(pPacket);
            }
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

    /* Step 2: Do any packet payloads match up? */

    pthread_mutex_lock(&HashLock);
    insert(pPacket);
    pthread_mutex_unlock(&HashLock);
    return;
}

// #include <stdlib.h>
// #include <stdio.h>
// #include <sys/types.h>
// #include <stdio.h>
// #include <string.h>
// #include <pthread.h>

// #include "pcap-process.h"

// #define MAX_KEY_LEN 100
// #define MAX_VALUE_LEN 100

// #define TABLE_SIZE 100

// struct hash_table {
//     struct PacketEntry *table[TABLE_SIZE];
// };

// /* How many packets have we seen? */
// uint32_t        gPacketSeenCount;

// /* How many total bytes have we seen? */
// uint64_t        gPacketSeenBytes;        

// /* How many hits have we had? */
// uint32_t        gPacketHitCount;

// /* How much redundancy have we seen? */
// uint64_t        gPacketHitBytes;

// struct hash_table TheHash;
// pthread_mutex_t HashLock;

// void resetAndSaveEntry (int nEntry)
// {
//     if(nEntry < 0 || nEntry >= TABLE_SIZE)
//     {
//         printf("* Warning: Tried to reset an entry in the table - entry out of bounds (%d)\n", nEntry);
//         return;
//     }

//     gPacketHitCount += 5; //TheHash.table[nEntry]->HitCount;
//     gPacketHitBytes += 5; //TheHash.table[nEntry]->RedundantBytes;

//     discardPacket(TheHash.table[nEntry]->ThePacket);

//     // TheHash.table[nEntry]->HitCount = 0;
//     // TheHash.table[nEntry]->RedundantBytes = 0;
//     // TheHash.table[nEntry]->ThePacket = NULL;

//     // gPacketHitCount += BigTable[nEntry].HitCount;
//     // gPacketHitBytes += BigTable[nEntry].RedundantBytes;
//     // discardPacket(BigTable[nEntry].ThePacket);

//     // BigTable[nEntry].HitCount = 0;
//     // BigTable[nEntry].RedundantBytes = 0;
//     // BigTable[nEntry].ThePacket = NULL;
// }

// // https://en.wikipedia.org/wiki/Jenkins_hash_function
// uint32_t jenkins_16bit(uint16_t key) {
//     uint32_t hash = 0;
//     hash += key;
//     hash += hash << 10;
//     hash ^= hash >> 6;
//     hash += hash << 3;
//     hash ^= hash >> 11;
//     hash += hash << 15;
//     return hash;
// }

// unsigned int hashData(int netPayload) {
//     unsigned int hash_value = jenkins_16bit(netPayload);
//     return hash_value % TABLE_SIZE;
// }

// int getNetPayload(struct PacketEntry * pEntry){
//     return pEntry->ThePacket->LengthIncluded - pEntry->ThePacket->PayloadOffset;
// }

// void insert(struct hash_table * ht, struct PacketEntry * pEntry ) {
//     unsigned int index = hashData(getNetPayload(pEntry));
//     struct PacketEntry * existing = ht->table[index];

//     // struct PacketEntry * new_node = create_node(pEntry);
//     struct PacketEntry * new_node = pEntry;
//     new_node->value = hashData(pEntry->key);

//     if (existing == NULL || existing->ThePacket == NULL) {
//         printf("Inserting New Node at index %d\n", index);
//         ht->table[index] = pEntry;
//     }
//     // MAYBE PROBLEM HERE
//     else if (existing->ThePacket->PayloadSize == new_node->ThePacket->PayloadSize) {        
//         // MAYBE PROBLEM HERE
//         // if (memcmp((void*) existing->key, (void *) getNetPayload(pEntry), sizeof(existing->key) == 0)) {
//         if (existing->key == new_node->key) {
//             printf("Payloads Match\n");
//             // this means the netPayloads are the same
//             // this is a hit
//             existing->HitCount++;
//             existing->RedundantBytes += sizeof(getNetPayload(pEntry));

//             // discard the one being inserted
//             discardPacket(new_node->ThePacket);
//         }
//         else {
//             // if existing hit count is greater than < 1 evict the exising packetentry
//             if (existing->HitCount < 1) {
//                 printf("Evicting old1\n");
//                 // discard existing
//                 resetAndSaveEntry(index);
//                 // discardPacket(existing->ThePacket);
            
//                 ht->table[index] = new_node;
//             }
//                 else {
//                     // discard the new one
//                     printf("Evicting new1\n");
//                     discardPacket(new_node->ThePacket);
//         }
//         }
//     }
//     // MAYBE PROBLEM HERE
//     else {
//         // if existing hit count is greater than < 1 evict the exising packetentry
//         if (existing->HitCount < 1) {
//             printf("Evicting old2\n");
//             // discard existing
//             resetAndSaveEntry(index);
//             // discardPacket(existing->ThePacket);
            
//             ht->table[index] = new_node;
//         }
//         else {
//             // discard the new one
//             printf("Evicting new2\n");
//             discardPacket(new_node->ThePacket);
//         }
//     }
// }

// void initializeProcessingStats ()
// {
//     gPacketSeenCount = 0;
//     gPacketSeenBytes = 0;        
//     gPacketHitCount  = 0;
//     gPacketHitBytes  = 0;    
// }

// char initializeProcessing (int TableSize)
// {
//     initializeProcessingStats();

//     pthread_mutex_init(&HashLock, 0);

//     for (int j = 0; j < TABLE_SIZE; j++) {
//         TheHash.table[j] = (struct PacketEntry *) malloc(sizeof(struct PacketEntry));
//         TheHash.table[j]->ThePacket = NULL;
//         TheHash.table[j]->HitCount  = 0;
//         TheHash.table[j]->RedundantBytes = 0;
//     }

//     return 1;
// }

// void processPacket (struct Packet * pPacket)
// {
//     uint16_t        PayloadOffset;

//     PayloadOffset = 0;

//     /* Do a bit of error checking */
//     if(pPacket == NULL)
//     {
//         // printf("* Warning: Packet to assess is null - ignoring\n");
//         return;
//     }

//     if(pPacket->Data == NULL)
//     {
//         // printf("* Error: The data block is null - ignoring\n");
//         return;
//     }

//     // printf("STARTFUNC: processPacket (Packet Size %d)\n", pPacket->LengthIncluded);

//     /* Step 1: Should we process this packet or ignore it? 
//      *    We should ignore it if:
//      *      The packet is too small
//      *      The packet is not an IP packet
//      */

//     /* Update our statistics in terms of what was in the file */
//     gPacketSeenCount++;
//     gPacketSeenBytes += pPacket->LengthIncluded;

//     /* Is this an IP packet (Layer 2 - Type / Len == 0x0800)? */

//     if(pPacket->LengthIncluded <= MIN_PKT_SIZE)
//     {
//         discardPacket(pPacket);
//         return;
//     }

//     if((pPacket->Data[12] != 0x08) || (pPacket->Data[13] != 0x00))
//     {
//         // printf("Not IP - ignoring...\n");
//         discardPacket(pPacket);
//         return;
//     }

//     /* Adjust the payload offset to skip the Ethernet header 
//         Destination MAC (6 bytes), Source MAC (6 bytes), Type/Len (2 bytes) */
//     PayloadOffset += 14;

//     /* Step 2: Figure out where the payload starts 
//          IP Header - Look at the first byte (Version / Length)
//          UDP - 8 bytes 
//          TCP - Look inside header */

//     if(pPacket->Data[PayloadOffset] != 0x45)
//     {
//         /* Not an IPv4 packet - skip it since it is IPv6 */
//         // printf("  Not IPV4 - Ignoring\n");
//         discardPacket(pPacket);
//         return;
//     }
//     else
//     {
//         /* Offset will jump over the IPv4 header eventually (+20 bytes)*/
//     }

//     /* Is this a UDP packet or a TCP packet? */
//     if(pPacket->Data[PayloadOffset + 9] == 6)
//     {
//         /* TCP */
//         uint8_t TCPHdrSize;

//         TCPHdrSize = ((uint8_t) pPacket->Data[PayloadOffset+9+12] >> 4) * 4;
//         PayloadOffset += 20 + TCPHdrSize;
//     }
//     else if(pPacket->Data[PayloadOffset+9] == 17)
//     {
//         /* UDP */

//         /* Increment the offset by 28 bytes (20 for IPv4 header, 8 for the UDP header)*/
//         PayloadOffset += 28;
//     }
//     else 
//     {
//         /* Don't know what this protocol is - probably not helpful */
//         discardPacket(pPacket);
//         return;
//     }

//     // printf("  processPacket -> Found an IP packet that is TCP or UDP\n");

//     uint16_t    NetPayload;

//     NetPayload = pPacket->LengthIncluded - PayloadOffset;

//     pPacket->PayloadOffset = PayloadOffset;
//     pPacket->PayloadSize = NetPayload;

//     /* Step 2: Do any packet payloads match up? */

//     struct PacketEntry pEntry;
//     pEntry.ThePacket = pPacket;
//     pEntry.HitCount = 0;
//     pEntry.RedundantBytes = 0;
//     pEntry.key = NetPayload;
//     pEntry.value = 0;

//     pthread_mutex_lock(&HashLock);
//     insert(&TheHash, &pEntry);
//     pthread_mutex_unlock(&HashLock);
// }

// void tallyProcessing ()
// {
//     for(int j=0; j<TABLE_SIZE; j++)
//     {
//         resetAndSaveEntry(j);
//     }

//     for (int j = 0; j < TABLE_SIZE; j++) {
//         free(TheHash.table[j]);
//     }

// }