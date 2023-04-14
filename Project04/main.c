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

#define MAX_PCAP_FILES 20
#define MAX_FILE_LEN 40
#define OUR_MAX_PACKETS 0
#define STACK_MAX_SIZE 10

struct FilePcapInfo theInfo[MAX_PCAP_FILES];

pthread_mutex_t StackLock;
pthread_mutex_t FileLock;

struct Packet *stack[STACK_MAX_SIZE];
pthread_cond_t consumer_wait;
pthread_cond_t producer_wait;
int window_size = 1;
int stackSize = 0;
int isdone = 0;
int nThreads = 3;
int nThreadsConsumers = 1;
int nThreadsProducers = 1;
int fileIndex = 0;

int numPcapFiles = 0;

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


void * thread_producer(void *pThreadData){

    while (isdone != numPcapFiles) {

        // Set producer thread to read one file each
        pthread_mutex_lock(&FileLock);

        struct FilePcapInfo * pFileInfo;
        FILE * pTheFile;
	    struct Packet * pPacket;

        if (fileIndex == numPcapFiles) {
            pthread_mutex_unlock(&FileLock);
            break;
        }
        pFileInfo = &theInfo[fileIndex];
        fileIndex++;

        pthread_mutex_unlock(&FileLock);

        /* Default is to not flip due to endian-ness issues */
        pFileInfo->EndianFlip = 0;
        /* Reset the counters */
        pFileInfo->Packets = 0;
        pFileInfo->BytesRead = 0;

        /* Open the file and its respective front matter */
        pTheFile = fopen(pFileInfo->FileName, "r");
        if(pTheFile == NULL){
		    printf("Could not open file\nExiting...\n");
		    exit(-2);
	    }

        /* Read the front matter */
        if(!parsePcapFileStart(pTheFile, pFileInfo))
        {
            printf("* Error: Failed to parse front matter on pcap file %s\n", pFileInfo->FileName);
            return NULL;
        }

        // Continue to read packets from file and add to shared stack
        while(!feof(pTheFile)){
            pPacket = readNextPacket(pTheFile, pFileInfo);
            pthread_mutex_lock(&StackLock);

            // wait while stack is full
            while(stackSize >= STACK_MAX_SIZE){
                pthread_cond_wait(&producer_wait, &StackLock);
            }

            stack[stackSize] = pPacket;
            stackSize++;
            pthread_cond_broadcast(&consumer_wait);
            pthread_mutex_unlock(&StackLock);
        }
        fclose(pTheFile);
        isdone++;
        pthread_cond_broadcast(&consumer_wait);
        pthread_mutex_unlock(&StackLock);
    }
	return NULL;
}


void  *thread_consumer(void *arg){
	while(1){
        // wait while the stack is empty
		pthread_mutex_lock(&StackLock);
		while(stackSize <= 0){
			if (isdone == numPcapFiles) break;
			pthread_cond_wait(&consumer_wait, &StackLock);
		}

        // exit when all producer threads are done reading files
		if(isdone == numPcapFiles && stackSize <= 0){
			pthread_mutex_unlock(&StackLock);
			break;
		}
    
        struct Packet * poc = (struct Packet *) malloc(sizeof(struct Packet));
        poc = stack[stackSize-1];
        stackSize--;

        pthread_cond_broadcast(&producer_wait);
        pthread_mutex_unlock(&StackLock);

        // call process packet to add data to hash
		if(poc->Data != NULL){
			processPacket(poc);
		}
	}
	pthread_cond_broadcast(&consumer_wait);
	return NULL;
}

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
    pthread_mutex_init(&FileLock,0);
   
   char buffer[MAX_FILE_LEN];

	// PARSE COMMAND LINE ARGUMENTS
    int isPcapFile = 0, fileRead = 0;
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
            isPcapFile = 1;
            fileRead = 1;

            numPcapFiles++;
        }
        // handle txt file
        else if (strcmp(ext, "txt") == 0) {
            FILE *fp = fopen(argv[i], "r");
            if (fp == NULL) {
                fprintf(stderr, "Error: Unable to open file '%s'\n", argv[i]);
                exit(-1);
            }
            fileRead = 1;
            while (fgets(buffer, MAX_FILE_LEN, fp) != NULL && numPcapFiles < MAX_PCAP_FILES) {
                if (strcmp(buffer, "\n") == 0) {
                    memset(buffer, 0, sizeof(buffer));
                    continue;
                }
                if (buffer[strlen(buffer)-1] == '\n') {
                    buffer[strlen(buffer)-1] = '\0';
                }
                theInfo[numPcapFiles].FileName = (char *) malloc(sizeof(char) * MAX_FILE_LEN);
                strcpy(theInfo[numPcapFiles].FileName, buffer);
 
                theInfo[numPcapFiles].EndianFlip = 0;
                theInfo[numPcapFiles].BytesRead = 0;
                theInfo[numPcapFiles].Packets = 0;
                theInfo[numPcapFiles].MaxPackets = OUR_MAX_PACKETS;
                numPcapFiles++;
            }
        }
        // handle threads argument
        else if (strcmp(argv[i], "-threads") == 0) {
            //printf("reading nthreads\n");
            i++;
            if (i == argc) {
                fprintf(stderr, "Error: Must enter number of threads!\n");
                exit(-1);
            }
            int n = atoi(argv[i]);
            if (n < 2 || n > 8) {
                fprintf(stderr, "Error: Please enter a valid integer between 2 and 8 to specify the number of threads!\n");
                exit(-1);
            }
            else {
                nThreads = n;
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
            printf("  -threads N       Number of threads to use (2 to 8)\n");
            printf("  -window  W       Window of bytes for partial matching (64 to 512)\n");
            printf("       If not specified, the optimal setting will be used\n");
            return -1;
        }
        i++;
	}
    if (!fileRead) {
        printf("Error: must enter an input file\n");
        exit(-1);
    }

    // Optimally allocate producer and consumer threads based on the number of pcap files read in and the thread argument specified
    if ((numPcapFiles != 1) && (nThreads > numPcapFiles)) {
        nThreadsProducers = numPcapFiles;
        nThreadsConsumers = nThreads - numPcapFiles;
    }
    else if (numPcapFiles == 1) {
        nThreadsProducers = 1;
        nThreadsConsumers = nThreads - 1;
    }
    else {
        nThreadsConsumers = 1;
        nThreadsProducers = nThreads - nThreadsConsumers;
    }
		
    printf("MAIN: Initializing the table for redundancy extraction\n");
    initializeProcessing(DEFAULT_TABLE_SIZE);
    printf("MAIN: Initializing the table for redundancy extraction ... done\n");

    pthread_t * pThreadConsumers;
    pthread_t * pThreadProducers;
    pThreadConsumers = (pthread_t *) malloc(sizeof(pthread_t *) * nThreadsConsumers );
    pThreadProducers = (pthread_t *) malloc(sizeof(pthread_t *) * nThreadsProducers );
    
    // start consumer threads
    for(int k=0; k < nThreadsConsumers; k++){
        pthread_create(pThreadConsumers+k, NULL, &thread_consumer, NULL); 
    }

    // start producer threads
    for (i = 0; i < nThreadsProducers; i++) {
        pthread_create(pThreadProducers+i, NULL, thread_producer, NULL);
    }

    // join consumer threads
    for(int j = 0; j < nThreadsConsumers; j++) {
        pthread_join(pThreadConsumers[j], NULL);
    }

    printf("Summarizing the processed entries\n");
    tallyProcessing();

    if (!isPcapFile) {
        for (i = 0; i < numPcapFiles; i++) {
            free(theInfo[i].FileName);
        }
    }

    /* Output the statistics */
    printf("  Total Packets Parsed:    %d\n", gPacketSeenCount);
    printf("  Total Bytes   Parsed:    %lu\n", (unsigned long) gPacketSeenBytes);
    printf("  Total Packets Duplicate: %d\n", gPacketHitCount);
    printf("  Total Bytes   Duplicate: %lu\n", (unsigned long) gPacketHitBytes);

    float fPct;

    fPct = (float) gPacketHitBytes / (float) gPacketSeenBytes * 100.0;

    printf("  Total Duplicate Percent: %6.2f%%\n", fPct);

    return 0;
}
