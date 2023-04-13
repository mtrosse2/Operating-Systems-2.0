
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>

#include "pcap-process.h"

#define MAX_KEY_LEN 100
#define MAX_VALUE_LEN 100

#define TABLE_SIZE 100

struct hash_table {
    struct PacketEntry *table[TABLE_SIZE];
};


/* How many packets have we seen? */
uint32_t        gPacketSeenCount;

/* How many total bytes have we seen? */
uint64_t        gPacketSeenBytes;        

/* How many hits have we had? */
uint32_t        gPacketHitCount;

/* How much redundancy have we seen? */
uint64_t        gPacketHitBytes;

/* Our big table for recalling packets */
// struct PacketEntry *    BigTable; 
// int    BigTableSize;
// int    BigTableNextToReplace;

struct hash_table TheHash;

// https://en.wikipedia.org/wiki/Jenkins_hash_function
uint32_t jenkins_16bit(uint16_t key) {
    uint32_t hash = 0;
    hash += key;
    hash += hash << 10;
    hash ^= hash >> 6;
    hash += hash << 3;
    hash ^= hash >> 11;
    hash += hash << 15;
    return hash;
}

unsigned int hashData(int netPayload) {
    unsigned int hash_value = jenkins_16bit(netPayload);
    // get hash_value from jenkins hash using PacketEntry->ThePacket->Data + PacketEntry->ThePacket->PayloadOffset (ie. netPayload)
    return hash_value % TABLE_SIZE;
}

int getNetPayload(struct PacketEntry * pEntry){
    return pEntry->ThePacket->LengthIncluded - pEntry->ThePacket->PayloadOffset;
}

// struct PacketEntry * create_node(struct PacketEntry * pEntry) {
//     struct PacketEntry * new_node = malloc(sizeof(struct PacketEntry));
//     new_node->ThePacket = pEntry->ThePacket;
//     new_node->key = getNetPayload(pEntry);
//     new_node->value = hashData(getNetPayload(pEntry));
//     return new_node;
// }

void insert(struct hash_table * ht, struct PacketEntry * pEntry ) {
    unsigned int index = hashData(getNetPayload(pEntry));
    struct PacketEntry * existing = ht->table[index];

    // struct PacketEntry * new_node = create_node(pEntry);
    struct PacketEntry * new_node = pEntry;
    new_node->value = hashData(pEntry->key);

    if (existing == NULL || existing->ThePacket == NULL) {
        printf("Inserting New Node\n");
        ht->table[index] = pEntry;
    }
    // MAYBE PROBLEM HERE
    else if (sizeof(existing->key) == sizeof(new_node->key)) {
        
        // MAYBE PROBLEM HERE
        // if (memcmp((void*) existing->key, (void *) getNetPayload(pEntry), sizeof(existing->key) == 0)) {
        if (memcmp((void*) existing->key, (void *) new_node->key, sizeof(existing->key) == 0)) {
            printf("Payloads Match\n");
            // this means the netPayloads are the same
            // this is a hit
            existing->HitCount++;
            existing->RedundantBytes += sizeof(getNetPayload(pEntry));

            // discard the one being inserted
            discardPacket(new_node->ThePacket);
        }
        else {
            // if existing hit count is greater than < 1 evict the exising packetentry
            if (existing->HitCount < 1) {
                printf("Evicting old1\n");
                // discard existing
                discardPacket(existing->ThePacket);
            
                ht->table[index] = new_node;
            }
                else {
                    // discard the new one
                    printf("Evicting new1\n");
                    discardPacket(new_node->ThePacket);
        }
        }
    }
    // MAYBE PROBLEM HERE
    else {
        // if existing hit count is greater than < 1 evict the exising packetentry
        if (existing->HitCount < 1) {
            printf("Evicting old2\n");
            // discard existing
            discardPacket(existing->ThePacket);
            
            ht->table[index] = new_node;
        }
        else {
            // discard the new one
            printf("Evicting new2\n");
            discardPacket(new_node->ThePacket);
        }
    }
}

void initializeProcessingStats ()
{
    gPacketSeenCount = 0;
    gPacketSeenBytes = 0;        
    gPacketHitCount  = 0;
    gPacketHitBytes  = 0;    
}

char initializeProcessing (int TableSize)
{
    initializeProcessingStats();

    /* Allocate our big table */
    // BigTable = (struct PacketEntry *) malloc(sizeof(struct PacketEntry) * TableSize);

    // if(BigTable == NULL)
    // {
    //     printf("* Error: Unable to create the new table\n");
    //     return 0;
    // }

    for (int j = 0; j < TABLE_SIZE; j++) {
        TheHash.table[j] = (struct PacketEntry *) malloc(sizeof(struct PacketEntry));
        TheHash.table[j]->ThePacket = NULL;
        TheHash.table[j]->HitCount  = 0;
        TheHash.table[j]->RedundantBytes = 0;
    }

    // for(int j=0; j<TableSize; j++)
    // {
    //     BigTable[j].ThePacket = NULL;
    //     BigTable[j].HitCount  = 0;
    //     BigTable[j].RedundantBytes = 0;
    // }

    // BigTableSize = TableSize;
    // BigTableNextToReplace = 0;
    return 1;
}

void resetAndSaveEntry (int nEntry)
{
    // if(nEntry < 0 || nEntry >= BigTableSize)
    // {
    //     printf("* Warning: Tried to reset an entry in the table - entry out of bounds (%d)\n", nEntry);
    //     return;
    // }

    // if(BigTable[nEntry].ThePacket == NULL)
    // {
    //     return;
    // }

    printf("%d\n", nEntry);

    if(nEntry < 0 || nEntry >= TABLE_SIZE)
    {
        printf("* Warning: Tried to reset an entry in the table - entry out of bounds (%d)\n", nEntry);
        return;
    }

    // if (TheHash.table[nEntry] == NULL) {
    //     return;
    // }
    printf("hello\n");

    gPacketHitCount += TheHash.table[nEntry]->HitCount;
    gPacketHitBytes += TheHash.table[nEntry]->RedundantBytes;

    printf("hello\n");

    discardPacket(TheHash.table[nEntry]->ThePacket);

    // TheHash.table[nEntry]->HitCount = 0;
    // TheHash.table[nEntry]->RedundantBytes = 0;
    // TheHash.table[nEntry]->ThePacket = NULL;

    // gPacketHitCount += BigTable[nEntry].HitCount;
    // gPacketHitBytes += BigTable[nEntry].RedundantBytes;
    // discardPacket(BigTable[nEntry].ThePacket);

    // BigTable[nEntry].HitCount = 0;
    // BigTable[nEntry].RedundantBytes = 0;
    // BigTable[nEntry].ThePacket = NULL;
}

void processPacket (struct Packet * pPacket)
{
    uint16_t        PayloadOffset;

    PayloadOffset = 0;

    /* Do a bit of error checking */
    if(pPacket == NULL)
    {
        printf("* Warning: Packet to assess is null - ignoring\n");
        return;
    }

    if(pPacket->Data == NULL)
    {
        printf("* Error: The data block is null - ignoring\n");
        return;
    }

    printf("STARTFUNC: processPacket (Packet Size %d)\n", pPacket->LengthIncluded);

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
        printf("Not IP - ignoring...\n");
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
        printf("  Not IPV4 - Ignoring\n");
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

    printf("  processPacket -> Found an IP packet that is TCP or UDP\n");

    uint16_t    NetPayload;

    NetPayload = pPacket->LengthIncluded - PayloadOffset;

    pPacket->PayloadOffset = PayloadOffset;
    pPacket->PayloadSize = NetPayload;

    /* Step 2: Do any packet payloads match up? */

    struct PacketEntry pEntry;
    pEntry.ThePacket = pPacket;
    pEntry.HitCount = 0;
    pEntry.RedundantBytes = 0;
    pEntry.key = NetPayload;
    pEntry.value = 0;

    insert(&TheHash, &pEntry);

    // int j;

    // for(j=0; j<BigTableSize; j++)
    // {
    //     if(BigTable[j].ThePacket != NULL)
    //     {
    //         int k;

    //         /* Are the sizes the same? */
    //         if(BigTable[j].ThePacket->PayloadSize != pPacket->PayloadSize)
    //         {
    //             continue;
    //         }

    //         /* OK - same size - do the bytes match up? */
    //         for(k=0; k<BigTable[j].ThePacket->PayloadSize; k++)
    //         {
    //             if(BigTable[j].ThePacket->Data[k+PayloadOffset] != pPacket->Data[k+PayloadOffset])
    //             {
    //                 /* Nope - they are not the same */
    //                 break;
    //             }
    //         }

    //         /* Did we break out with a mismatch? */
    //         if(k < BigTable[j].ThePacket->PayloadSize)
    //         {
    //             continue;
    //         }
    //         else 
    //         {
    //             /* Whoot, whoot - the payloads match up */
    //             BigTable[j].HitCount++;
    //             BigTable[j].RedundantBytes += pPacket->PayloadSize;

    //             /* The packets match so get rid of the matching one */
    //             discardPacket(pPacket);
    //             return;
    //         }
    //     }
    //     else 
    //     {
    //         /* We made it to an empty entry without a match */
            
    //         BigTable[j].ThePacket = pPacket;
    //         BigTable[j].HitCount = 0;
    //         BigTable[j].RedundantBytes = 0;
    //         break;
    //     }
    // }

    // /* Did we search the entire table and find no matches? */
    // if(j == BigTableSize)
    // {
    //     /* Kick out the "oldest" entry by saving its entry to the global counters and 
    //        free up that packet allocation 
    //      */
    //     resetAndSaveEntry(BigTableNextToReplace);

    //     /* Take ownership of the packet */
    //     BigTable[BigTableNextToReplace].ThePacket = pPacket;

    //     /* Rotate to the next one to replace */
    //     BigTableNextToReplace = (BigTableNextToReplace+1) % BigTableSize;
    // }

    /* All done */
}

void tallyProcessing ()
{
    // for(int j=0; j<BigTableSize; j++)
    // {
    //     resetAndSaveEntry(j);
    // }
    for(int j=0; j<TABLE_SIZE; j++)
    {
        resetAndSaveEntry(j);
        printf("now here %d %d\n", j, TABLE_SIZE);
    }

    for (int j = 0; j < TABLE_SIZE; j++) {
        free(TheHash.table[j]);
    }

}