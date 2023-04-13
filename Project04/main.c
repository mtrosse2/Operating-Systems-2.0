/* main.c : Main file for redextract */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>

/* for strdup due to C99 */
char * strdup(const char *s);

#include <string.h>
#include "pcap-read.h"
#include "pcap-process.h"

#define MAX_PCAP_FILES 10
#define MAX_FILE_LEN 20
#define OUR_MAX_PACKETS 5

pthread_mutex_t StackLock;
struct Packet *stack[8]; // need array type and size CHANGE LATER
pthread_cond_t consumer_wait;
pthread_cond_t producer_wait;
int num_threads = 1;
int window_size = 1;
int num_consumers = 1;
int num_producers = 1;
int stackSize = 0;
int isdone = 0;
int nThreadsConsumers = 1;
int nThreadsProducers = 2;

#define STACK_MAX_SIZE 7

struct ThreadDataProduce
{
    struct FilePcapInfo * PcapInfo;
    int ThreadID;
};

struct ThreadDataConsume
{
    int ThreadID;
    char * SearchString;
};

char* extension(char* filename) {
    char* dot = strchr(filename, '.');
    if (dot && dot != filename) {
        return dot + 1;
    }
    return "";
}


// My work start

void * thread_producer(void *pThreadData){
	struct ThreadDataProduce * pData;
	pData = (struct ThreadDataProduce *) pThreadData;
	struct FilePcapInfo * pFileInfo;
	pFileInfo = (struct FilePcapInfo *) pData->PcapInfo;
	
	FILE * pTheFile;
	struct Packet * pPacket;

	/* Default is to not flip due to endian-ness issues */
	pFileInfo->EndianFlip = 0;

	/* Reset the counters */
	pFileInfo->Packets = 0;
	pFileInfo->BytesRead = 0;

	/* Open the file and its respective front matter */
	pTheFile = fopen(pFileInfo->FileName, "r");

	/* Read the front matter */
	if(!parsePcapFileStart(pTheFile, pFileInfo))
	{
		printf("* Error: Failed to parse front matter on pcap file %s\n", pFileInfo->FileName);
		// pthread_mutex_unlock(&StackLock);
		return NULL;
	}
	while(!feof(pTheFile)){
		// printf("Readin Bitch\n");
		pPacket = readNextPacket(pTheFile, pFileInfo);
		pthread_mutex_lock(&StackLock);

		while(stackSize >= STACK_MAX_SIZE && isdone == 0){
			pthread_cond_wait(&producer_wait, &StackLock);
		}

		if(isdone == 1)
			break;

		stack[stackSize] = pPacket;
		stackSize++;
		pthread_cond_broadcast(&consumer_wait);
		pthread_mutex_unlock(&StackLock);
	}
	fclose(pTheFile);
	isdone = 1;
	printf("");
	pthread_cond_broadcast(&consumer_wait);
	pthread_mutex_unlock(&StackLock);
	return NULL;
}

// not too sure what to return
void  *thread_consumer(void *arg){
	while(1){
		// printf("Consumin bitch\n");
		pthread_mutex_lock(&StackLock);
		while(stackSize <= 0 && isdone == 0){	// while stack is empty
			//printf("Wait\n");
			pthread_cond_wait(&consumer_wait, &StackLock);
		}
		if(isdone == 1 && stackSize <= 0){
			pthread_mutex_unlock(&StackLock);
			break;
		}
		struct Packet * poc;
		poc = stack[stackSize-1];
		stackSize--;
		
		if(poc != NULL){
			processPacket(poc);
		}

		/* Allow for an early bail out if specified */
		/*
		if(pFileInfo->MaxPackets != 0)
		{
			if(pFileInfo->Packets >= pFileInfo->MaxPackets)
			{
				break;
			}
		}
		*/
		pthread_cond_broadcast(&producer_wait);
		pthread_mutex_unlock(&StackLock);
		// need to return something
	}
	pthread_cond_broadcast(&consumer_wait);
	printf("Consumer done\n");
	return NULL;
}


// My work end

int main (int argc, char *argv[])
{	 
    if(argc < 2)
    {
        printf("Usage: redextract FileX\n");
        printf("       redextract FileX\n");
        printf("  FileList        List of pcap files to process\n");
        printf("    or\n");
        printf("  FileName        Single file to process (if ending with .pcap)\n");
        printf("\n");
        printf("Optional Arguments:\n");
        /* You should handle this argument but make this a lower priority when 
           writing this code to handle this 
         */
        printf("  -threads N       Number of threads to use (2 to 8)\n");
        /* Note that you do not need to handle this argument in your code */
        printf("  -window  W       Window of bytes for partial matching (64 to 512)\n");
        printf("       If not specified, the optimal setting will be used\n");
        return -1;
    }

	// INITIALIZE VARIABLES
	pthread_mutex_init(&StackLock,0);
	pthread_cond_init(&consumer_wait, NULL);
	pthread_cond_init(&producer_wait, NULL);

   struct FilePcapInfo * theInfo = (struct FilePcapInfo *) malloc(sizeof(struct FilePcapInfo) * MAX_PCAP_FILES);
   int numPcapFiles = 0;
   char buffer[MAX_FILE_LEN];

	// PARSE COMMAND LINE ARGUMENTS
	int i = 1;
	while (i < argc){
        char* ext = extension(argv[i]);
        
        // handle pcap file
        if (strcmp(ext, "pcap") == 0) {
            theInfo[numPcapFiles].FileName = argv[i];
            theInfo[numPcapFiles].EndianFlip = 0;
            theInfo[numPcapFiles].BytesRead = 0;
            theInfo[numPcapFiles].Packets = 0;
            theInfo[numPcapFiles].MaxPackets = OUR_MAX_PACKETS;

            numPcapFiles++;
        }
        // handle txt file
        else if (strcmp(ext, "txt") == 0) {
            FILE *fp = fopen(argv[i], "r");
            if (fp == NULL) {
                fprintf(stderr, "Error: Unable to open file '%s'\n", argv[i]);
                exit(-1);
            }
            while (fgets(buffer, MAX_FILE_LEN, fp) != NULL && numPcapFiles < MAX_PCAP_FILES) {
                buffer[strcspn(buffer, "\n")] = '\0';
                printf("%s\n", buffer);
                ext = extension(buffer);
                if (strcmp(ext, "pcap") == 0) {
                    theInfo[numPcapFiles].FileName = buffer;
                    theInfo[numPcapFiles].EndianFlip = 0;
                    theInfo[numPcapFiles].BytesRead = 0;
                    theInfo[numPcapFiles].Packets = 0;
                    theInfo[numPcapFiles].MaxPackets = OUR_MAX_PACKETS;
                    numPcapFiles++;
                }
                else {
                    fprintf(stderr, "Error: Invalid pcap file specified in file '%s'\n", argv[i]);
                    exit(-1);
                }
            }
        }
        // handle threads argument
        else if (strcmp(argv[i], "-threads") == 0) {
            i++;
            if (i == argc) {
                fprintf(stderr, "Error: Must enter number of threads!\n");
                exit(-1);
            }
            int n = atoi(argv[i]);
            if (n < 1 || n > 8) {
                fprintf(stderr, "Error: Please enter a valid integer between 1 and 8 to specify the number of threads!\n");
                exit(-1);
            }
            else {
                num_threads = n;
            }
        }
        // handle window argument
        else if (strcmp(argv[i], "-window") == 0) {
            i++;
            if (i == argc) {
                fprintf(stderr, "Error: Must enter window size!\n");
                exit(-1);
            }
            int m = atoi(argv[i]);
            if (m < 64 || m > 512) {
                fprintf(stderr, "Error: Please enter a valid integer between 64 and 512 to specify the window size!\n");
                exit(-1);
            }
            else {
                window_size = m;
            }
        }
        else {
            printf("Usage: redextract FileX\n");
            printf("       redextract FileX\n");
            printf("  FileList        List of pcap files to process\n");
            printf("    or\n");
            printf("  FileName        Single file to process (if ending with .pcap)\n");
            printf("\n");
            printf("Optional Arguments:\n");
            /* You should handle this argument but make this a lower priority when 
            writing this code to handle this 
            */
            printf("  -threads N       Number of threads to use (2 to 8)\n");
            /* Note that you do not need to handle this argument in your code */
            printf("  -window  W       Window of bytes for partial matching (64 to 512)\n");
            printf("       If not specified, the optimal setting will be used\n");
            return -1;
        }
        i++;
	}
		
		
    printf("MAIN: Initializing the table for redundancy extraction\n");
    initializeProcessing(DEFAULT_TABLE_SIZE);
    printf("MAIN: Initializing the table for redundancy extraction ... done\n");

    /* Note that the code as provided below does only the following 
     *
     * - Reads in a single file twice
     * - Reads in the file one packet at a time
     * - Process the packets for redundancy extraction one packet at a time
     * - Displays the end results
     */


		
		pthread_t * pThreadConsumers;
		pthread_t * pThreadProducers;
		pThreadConsumers = (pthread_t *) malloc(sizeof(pthread_t *) * nThreadsConsumers ); // Need to change this to number of consumers later
		pThreadProducers = (pthread_t *) malloc(sizeof(pthread_t *) * nThreadsProducers);
		
		struct ThreadDataProduce * pThreadData[nThreadsProducers]; // Change to number of producers
		
		nThreadsConsumers += 1;
		pthread_t PID = 0;
    for (i = 0; i < numPcapFiles; i++) {
				int k = 0;
				for(k=0; k < nThreadsProducers; k++){
					pThreadData[k] = malloc(sizeof(struct ThreadDataProduce));
					pThreadData[k]->ThreadID = PID;
					pThreadData[k]->PcapInfo = &theInfo[i];
					pthread_create(pThreadProducers+k, NULL, thread_producer, (void *) pThreadData[k]);
				 }
				for(k=0; k < nThreadsConsumers; k++){
					pthread_create(pThreadConsumers+k, NULL, &thread_consumer, NULL);
				}
				for(k = 0; k < nThreadsConsumers; k++){
					pthread_join(pThreadConsumers[k], NULL);
				}
				/*
        printf("MAIN: Attempting to read in the file named %s\n", theInfo[i].FileName);
        readPcapFile(&theInfo[i]);

        printf("MAIN: Attempting to read in the file named %s again\n", theInfo[i].FileName);
        readPcapFile(&theInfo[i]);
				*/

        printf("Summarizing the processed entries\n");
        tallyProcessing();

        /* Output the statistics */

        printf("Parsing of file %s complete\n", argv[1]);

        printf("  Total Packets Parsed:    %d\n", gPacketSeenCount);
        printf("  Total Bytes   Parsed:    %lu\n", (unsigned long) gPacketSeenBytes);
        printf("  Total Packets Duplicate: %d\n", gPacketHitCount);
        printf("  Total Bytes   Duplicate: %lu\n", (unsigned long) gPacketHitBytes);

        float fPct;

        fPct = (float) gPacketHitBytes / (float) gPacketSeenBytes * 100.0;

        printf("  Total Duplicate Percent: %6.2f%%\n", fPct);
    }

    return 0;
}
