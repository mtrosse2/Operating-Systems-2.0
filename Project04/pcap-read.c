/* pcap-read.c : Parse pcap files to extract packets
 *
 * Adapted from the ScaleBox project of the NetScale lab
 * Code supported in part by the National Science Foundation via
 * CNS-17XXX.
 * 
 * C Port: Adapted in March 2023 for CSE 30341
 */

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/time.h>

#include "packet.h"
#include "pcap-read.h"
#include "pcap-process.h"

#define SHOW_DEBUG	1


char parsePcapFileStart (FILE * pTheFile, struct FilePcapInfo * pFileInfo)
{
	// tcpdump header processing
	//
	//  Reference of file info available at:
	//   http://lists.linux-wlan.com/pipermail/linux-wlan-devel/2003-September/002701.html
	//
	//  Also see:
	//	 http://wiki.wireshark.org/Development/LibpcapFileFormat
		
	int					nMagicNum;
	unsigned short		nMajor;
	unsigned short		nMinor;
	unsigned int		nSnapshotLen;
	unsigned int		nMediumType;
	
	// 32 bit magic number
	// 16 bit Major
	// 16 bit Minor
	// Timezone offset (ignore) - 32 bit
	// Timezone accuracy (ignore) - 32 bit
	// Snapshot length - 32 bit
	// Link layer type - 32 bit
	
	fread((char *) &nMagicNum, 4, 1, pTheFile);
	fread((char *) &nMajor, sizeof(unsigned short), 1, pTheFile);
	fread((char *) &nMinor, sizeof(unsigned short), 1, pTheFile);

	/* Determine the endian-ness of this particular machine
	 *   A union allows us to represent a block of machine in different 
     *   ways.  We will also use this in Project 6. 
	 */
	union {
  		uint32_t a;
  		uint8_t  b[4];
  	} u;

	/* If we set the 32 bit value to 1
	 *   On a big endian machine, it would be 0x00 00 00 01
	 *   On a little endian machine, it is 0x01 00 00 00 
	 * 
	 */
	u.a = 1;
  	char bigEndian = u.b[3];

	if(nMagicNum == 0xa1b2c3d4) {
		pFileInfo->EndianFlip = 0;
	} else if (nMagicNum == 0xd4c3b2a1) {
		pFileInfo->EndianFlip = 1;
		nMajor = endianfixs(nMajor);
		nMinor = endianfixs(nMinor);
	} else {
		printf("Warning: Non-standard magic number at beginning of TCP dump file\n"); 
		printf("   Ignoring processing this file\n");
		return 0;
	}
					
	// Ignore time zone and TZ accuracy
	fseek(pTheFile, 4, SEEK_CUR);
	fseek(pTheFile, 4, SEEK_CUR);
	
	fread((char *) &nSnapshotLen,4,1,pTheFile);
	fread((char *) &nMediumType,4,1,pTheFile);
	
	if(pFileInfo->EndianFlip) 
	{
		/* Normally we can just use ntohol (network to host long) to fix things but since this
		   is a file and not a live protocol, we will need to be intentional about where we 
		   apply endian fixes 
		*/

		nSnapshotLen = endianfixl(nSnapshotLen);
		nMediumType = endianfixl(nMediumType);
	}

	if(SHOW_DEBUG) 
	{
		printf("tcpdump file initial information\n");
		printf("   Major: %d   Minor: %d   Endian Flip: ", nMajor, nMinor);

		if(pFileInfo->EndianFlip)
		{
			printf("Yes\n");
		}
		else
		{
			printf("No\n");
		}

		printf("   Snapshot Len: %d   Medium Type: %d\n", nSnapshotLen, nMediumType);		
	}

	return 1;
}

struct Packet * readNextPacket (FILE * pTheFile, struct FilePcapInfo * pFileInfo)
{
	struct Packet * 	pPacket;

	pPacket = allocatePacket(DEFAULT_READ_BUFFER);

	/* Read the packet from the file
		time_t struct		Seconds, microseconds (each 32 bits)
		Capture Length		32 bits
		Actual Length		32 bits  
	*/

	fread((char *) &(pPacket->TimeCapture.tv_sec), 1, sizeof(uint32_t), pTheFile);
	fread((char *) &(pPacket->TimeCapture.tv_usec), 1, sizeof(uint32_t), pTheFile);
	fread((char *) &(pPacket->LengthIncluded), 1, sizeof(uint32_t), pTheFile);
	fread((char *) &(pPacket->LengthOriginal), 1, sizeof(uint32_t), pTheFile);

	/* Is there an issue with endianness? 
		Do we need to fix it if the file was captured on a big versus small endian machine 
	*/
	if(pFileInfo->EndianFlip) 
	{
		pPacket->TimeCapture.tv_sec = endianfixl(pPacket->TimeCapture.tv_sec);
		pPacket->TimeCapture.tv_usec = endianfixl(pPacket->TimeCapture.tv_usec);

		pPacket->LengthIncluded = endianfixl(pPacket->LengthIncluded);
		pPacket->LengthOriginal = endianfixl(pPacket->LengthOriginal);
	}

	/* Double check that the packet can fit */
	if(pPacket->SizeDataMax < pPacket->LengthIncluded)
	{
		printf("* Warning: Unable to include packet of size %d due it exceeding %d bytes\n", pPacket->LengthIncluded, pPacket->SizeDataMax);
		discardPacket(pPacket);

		/* Skip this packet payload */
		fseek(pTheFile, pPacket->LengthIncluded, SEEK_CUR);
		return NULL;
	}

	/* Read in the actual packet data on a byte-wise basis */
	for(int j=0; j<pPacket->LengthIncluded; j++)
	{
		fread(pPacket->Data+j, sizeof(char), 1, pTheFile);
	}

	pFileInfo->Packets++;
	pFileInfo->BytesRead += pPacket->LengthIncluded;

	if(SHOW_DEBUG)
	{
		printf("Packet %d Info: t=%ld.%08d of %d bytes long (%d on the wire) \n", pFileInfo->Packets-1, (long int) pPacket->TimeCapture.tv_sec, (int) pPacket->TimeCapture.tv_usec, pPacket->LengthIncluded, pPacket->LengthOriginal);
	}

	return pPacket;
}


char readPcapFile (struct FilePcapInfo * pFileInfo)
{
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
		return 0;
	}

	while(!feof(pTheFile))
	{		
		pPacket = readNextPacket(pTheFile, pFileInfo);

		if(pPacket != NULL)
		{
			processPacket(pPacket);
		}

		/* Allow for an early bail out if specified */
		if(pFileInfo->MaxPackets != 0)
		{
			if(pFileInfo->Packets >= pFileInfo->MaxPackets)
			{
				break;
			}
		}
	}

	fclose(pTheFile);

	printf("File processing complete - %s file read containing %d packets with %d bytes of packet data\n", pFileInfo->FileName, pFileInfo->Packets, pFileInfo->BytesRead);
	return 1;
}



