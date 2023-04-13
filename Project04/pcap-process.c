
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "pcap-process.h"

#define MAX_KEY_LEN 100
#define MAX_VALUE_LEN 100



//#######################################################################################################################################################
struct {
	int RedundantBytes;
	int HitCount;
} packet_info;


struct DictionaryNode {
    struct Packet* key; // key of type Packet*
    struct packet_info* value; // value of type packet_info*
    struct DictionaryNode* next; // pointer to the next node
};

// Dictionary
struct Dictionary {
    struct DictionaryNode** buckets; // array of buckets
    int num_buckets; // number of buckets
};

// Function to create a new dictionary
struct Dictionary* create_dictionary(int num_buckets) {
    struct Dictionary* dictionary = (struct Dictionary*)malloc(sizeof(struct Dictionary));
    dictionary->buckets = (struct DictionaryNode**)malloc(num_buckets * sizeof(struct DictionaryNode*));
    dictionary->num_buckets = num_buckets;
    for (int i = 0; i < num_buckets; i++) {
        dictionary->buckets[i] = NULL;
    }
    return dictionary;
}

// Function to compute the hash value for a given key
int hash_function(struct Packet* key, int num_buckets) {
    // Use your own implementation of a hash function for Packet* key
    // Example:
    // return (int)(((uintptr_t)key) % num_buckets);
}

// Function to insert a key-value pair into the dictionary
void insert(struct Dictionary* dictionary, struct Packet* key, struct packet_info* value) {
    int bucket_idx = hash_function(key, dictionary->num_buckets);
    struct DictionaryNode* new_node = (struct DictionaryNode*)malloc(sizeof(struct DictionaryNode));
    new_node->key = key;
    new_node->value = value;
    new_node->next = dictionary->buckets[bucket_idx];
    dictionary->buckets[bucket_idx] = new_node;
}

// Function to lookup a value in the dictionary by key
struct packet_info* lookup(struct Dictionary* dictionary, struct Packet* key) {
    int bucket_idx = hash_function(key, dictionary->num_buckets);
    struct DictionaryNode* current = dictionary->buckets[bucket_idx];
    while (current != NULL) {
        if (current->key == key) {
            return current->value;
        }
        current = current->next;
    }
    return NULL;
}

// Function to remove a key-value pair from the dictionary by key
void remove_entry(struct Dictionary* dictionary, struct Packet* key) {
    int bucket_idx = hash_function(key, dictionary->num_buckets);
    struct DictionaryNode* prev = NULL;
    struct DictionaryNode* current = dictionary->buckets[bucket_idx];
    while (current != NULL) {
        if (current->key == key) {
            if (prev == NULL) {
                dictionary->buckets[bucket_idx] = current->next;
            } else {
                prev->next = current->next;
            }
            free(current);
            return;
        }
        prev = current;
        current = current->next;
    }
}

// Function to free memory used by the dictionary
void free_dictionary(struct Dictionary* dictionary) {
    for (int i = 0; i < dictionary->num_buckets; i++) {
        struct DictionaryNode* current = dictionary->buckets[i];
        while (current != NULL) {
            struct DictionaryNode* next = current->next;
            free(current);
            current = next;
				}
		}
}

// Function to pick a random key from the dictionary
struct Packet* pick_random_key(struct Dictionary* dictionary) {
    // Seed the random number generator
    srand(time(0));

    // Count the number of keys in the dictionary
    int count = 0;
    struct DictionaryNode* current = dictionary->head;
    while (current != NULL) {
        count++;
        current = current->next;
    }

    // Pick a random index
    int random_index = rand() % count;

    // Traverse to the node at the random index
    current = dictionary->head;
    for (int i = 0; i < random_index; i++) {
        current = current->next;
    }

    // Return the key at the random index
    return current->key;
}

int dictionary_size(struct Dictionary* dictionary) {
    int count = 0;
    struct DictionaryNode* current = dictionary->head;
    while (current != NULL) {
        count++;
        current = current->next;
    }
    return count;
}

//#######################################################################################################################################################

/* How many packets have we seen? */
uint32_t        gPacketSeenCount;

/* How many total bytes have we seen? */
uint64_t        gPacketSeenBytes;        

/* How many hits have we had? */
uint32_t        gPacketHitCount;

/* How much redundancy have we seen? */
uint64_t        gPacketHitBytes;

/* Our big table for recalling packets */
// struct Packet *    BigTable; 
int    BigMapSize;
int    BigMapNextToReplace;
struct Dictionary * BigMap = createDictionary();


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
    // BigTable = (struct Packet *) malloc(sizeof(struct Packet) * TableSize);
		BigMap = createDictionary();


    if(BigMap == NULL)
    {
        printf("* Error: Unable to create the new map\n");
        return 0;
    }

		/*
    for(int j=0; j<TableSize; j++)
    {
				insert(BigMap, NULL)
        BigTable[j].ThePacket = NULL;
        BigTable[j].HitCount  = 0;
        BigTable[j].RedundantBytes = 0;
    }

		*/
    BigMapSize = TableSize;
    BigMapNextToReplace = 0;

    return 1;
}

void resetAndSaveEntry (int nEntry)
{
    if(nEntry < 0 || nEntry >= BigMapSize)
    {
        printf("* Warning: Tried to reset an entry in the table - entry out of bounds (%d)\n", nEntry);
        return;
    }

    if(lookup(BigMap, NULL) == NULL) //?????????????????????????????????????????????????????????????????????????????????????????????????????????
    {
        return;
    }

    gPacketHitCount += BigTable[nEntry].HitCount;
    gPacketHitBytes += BigTable[nEntry].RedundantBytes;
    discardPacket(BigTable[nEntry].ThePacket);

    BigTable[nEntry].HitCount = 0;
    BigTable[nEntry].RedundantBytes = 0;
    BigTable[nEntry].ThePacket = NULL;
}

void processPacket (struct Packet * pPacket)
{
    uint16_t        PayloadOffset;

		struct *packet_info temp;
		struct *packet_info idk;
    
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
		if(lookup(BigMap, pPacket) != NULL){
			idk = lookup(BigMap, pPacket);
			idk->HitCount++;
			idk->RedundantBytes += pPacket->PayloadSize;
			discardPacket(pPacket);
			return;
		}
		else
		{
			temp = malloc(sizeof(struct packet_info*));
			temp->HitCount = 0;
			temp->RedundantBytes = 0;
			insert(BigMap, pPacket, temp);
		}

    /* Did we search the entire table and find no matches? */
		int count = dictionary_size(BigMap);
    if(count >= BigMapSize)
    {
				struct Packet* idk2 = pick_random_key(BigMap);
				remove_entry(BigMap, idk2);
        /* Kick out the "oldest" entry by saving its entry to the global counters and 
           free up that packet allocation 
         
        resetAndSaveEntry(BigTableNextToReplace);

         Take ownership of the packet 
        BigTable[BigTableNextToReplace].ThePacket = pPacket;

        Rotate to the next one to replace
        BigTableNextToReplace = (BigTableNextToReplace+1) % BigTableSize;
    		*/
		}

    /* All done */
}

void tallyProcessing ()
{
    for(int j=0; j<BigTableSize; j++)
    {
        resetAndSaveEntry(j);
    }
}





