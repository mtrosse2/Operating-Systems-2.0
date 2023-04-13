#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "pcap-process.h"
#include "pcap-process.c"
#include "packet.h"
#include "packet.c"

#define TABLE_SIZE 100

struct hash_table {
    struct PacketEntry *table[TABLE_SIZE];
};

// https://en.wikipedia.org/wiki/Jenkins_hash_function
uint32_t jenkins(const uint8_t* key, size_t length) {
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
    return hash;
}

int hashData(int netPayload) {
    int hash_value = jenkins(netPayload, sizeof(netPayload));
    // get hash_value from jenkins hash using PacketEntry->ThePacket->Data + PacketEntry->ThePacket->PayloadOffset (ie. netPayload)
    return hash_value % TABLE_SIZE;
}

int getNetPayload(struct PacketEntry * pEntry) {
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
        if (memcpy(existing->key, getNetPayload(pEntry), sizeof(existing->key) == 0)) {
            // this means the netPayloads are the same
            // this is a hit
            existing->HitCount++;
            existing->RedundantBytes += sizeof(getNetPayload(pEntry));

            // discard the one being inserted
            discard(new_node);
        }
    }
    else {
        // if existing hit count is greater than < 1 evict the exising packetentry
        if (existing->HitCount < 1) {
            // discard existing
            discard(existing);
            
            ht->table[index] = new_node;
        }
        else {
            // discard the new one
            discard(new_node);
        }
    }
}

int main (void){
    return 0;
}