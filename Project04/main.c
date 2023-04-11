/* main.c : Main file for redextract */

#include <stdio.h>
#include <stdlib.h>

/* for strdup due to C99 */
char * strdup(const char *s);

#include <string.h>

#include "pcap-read.h"
#include "pcap-process.h"

pthread_mutex_t stacklock;
struct Packet *stack[100000]; // need array type and size CHANGE LATER
pthread_cond_t consumer_wait;
pthread_cond_t producer_wait;
int num_threads = 1;
int num_consumers = 1;
int num_producers = 1;



struct ThreadDataProduce
{
    int Iterations;
    int ThreadID;
};

struct ThreadDataConsume
{
    int ThreadID;
    char * SearchString;
};

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


		// PARSE COMMAND LINE ARGUMENTS ./executable filename -threads thead_num
		i = 1
		while(i < argc){
			if(i == 1){
				
			}
			else if(i == 2){
				if( strcmp('-thread', argv[i]) // THIS IS WRONG BUT THE IDEAS RIGHT
			}
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

    struct FilePcapInfo     theInfo;

    theInfo.FileName = strdup(argv[1]);
    theInfo.EndianFlip = 0;
    theInfo.BytesRead = 0;
    theInfo.Packets = 0;
    theInfo.MaxPackets = 5;

    printf("MAIN: Attempting to read in the file named %s\n", theInfo.FileName);
    readPcapFile(&theInfo);

    printf("MAIN: Attempting to read in the file named %s again\n", theInfo.FileName);
    readPcapFile(&theInfo);

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


    return 0;
}
