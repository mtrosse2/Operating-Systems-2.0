#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include "pcap-process.h"
// #include "pcap-process.c"
#include "packet.h"
#include "packet.c"

#define TABLE_SIZE 10000

struct hash_table {
    struct PacketEntry *table[TABLE_SIZE];
};

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


int hashData(int netPayload) {
    int hash_value = jenkins_16bit(netPayload);
    // get hash_value from jenkins hash using PacketEntry->ThePacket->Data + PacketEntry->ThePacket->PayloadOffset (ie. netPayload)
    return hash_value % TABLE_SIZE;
}

int getNetPayload(struct PacketEntry * pEntry){
    return pEntry->ThePacket->LengthIncluded - pEntry->ThePacket->PayloadOffset;
}

struct PacketEntry * create_node(struct PacketEntry * pEntry) {
    struct PacketEntry * new_node = malloc(sizeof(struct PacketEntry));
    new_node->key = getNetPayload(pEntry);
    new_node->value = hashData(getNetPayload(pEntry));
    return new_node;
}

void insert(struct hash_table * ht, struct PacketEntry * pEntry ) {
    int index = hashData(getNetPayload(pEntry));
    struct PacketEntry * existing = ht->table[index];

    struct PacketEntry * new_node = create_node(pEntry);

    if (existing == NULL) {
        ht->table[index] = new_node;
    }
    else if (sizeof(existing->key) == sizeof(getNetPayload(pEntry))) {
        if (memcmp((void*) existing->key, (void *) getNetPayload(pEntry), sizeof(existing->key) == 0)) {
            // this means the netPayloads are the same
            // this is a hit
            existing->HitCount++;
            existing->RedundantBytes += sizeof(getNetPayload(pEntry));

            // discard the one being inserted
            discardPacket(new_node->ThePacket);
        }
    }
    else {
        // if existing hit count is greater than < 1 evict the exising packetentry
        if (existing->HitCount < 1) {
            // discard existing
            discardPacket(existing->ThePacket);
            
            ht->table[index] = new_node;
        }
        else {
            // discard the new one
            discardPacket(new_node->ThePacket);
        }
    }
}

