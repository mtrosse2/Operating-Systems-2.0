
#include "fs.h"
#include "disk.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

#define FS_MAGIC           0xf0f03410
#define INODES_PER_BLOCK   128
#define POINTERS_PER_INODE 5
#define POINTERS_PER_BLOCK 1024

struct fs_superblock {
	int magic;
	int nblocks;
	int ninodeblocks;
	int ninodes;
};

struct fs_inode {
	int isvalid;
	int size;
	int direct[POINTERS_PER_INODE];
	int indirect;
};

union fs_block {
	struct fs_superblock super;
	struct fs_inode inode[INODES_PER_BLOCK];
	int pointers[POINTERS_PER_BLOCK];
	char data[DISK_BLOCK_SIZE];
};

int fs_format()
{
	return 0;
}

void fs_debug()
{
	union fs_block block;

	disk_read(0,block.data);

	printf("superblock:\n");
	printf("    %d blocks\n",block.super.nblocks);
	printf("    %d inode blocks\n",block.super.ninodeblocks);
	printf("    %d inodes\n",block.super.ninodes);

	int i;
	for (i = 1; i < block.super.ninodeblocks + 1; i++) {
		disk_read(i, block.data);

		int j;	// loop through the inode blocks
		for (j = 0; j < INODES_PER_BLOCK; j++) {
			struct fs_inode inode;
			memcpy(&inode, &block.inode[j], sizeof(struct fs_inode));

			if (inode.isvalid) {	// if an inode block is valid print its information
				printf("inode %d:\n", ((i-1)*INODES_PER_BLOCK) + j);
				printf("	size: %d\n", inode.size);

				printf("	direct blocks: ");	// print direct blocks
				int k = 0;
				while ((k*sizeof(block.data)) < inode.size && k < POINTERS_PER_INODE) {
					printf("%d ", inode.direct[k]);
					k++;
				}
				printf("\n");

				if ((k*sizeof(block.data)) < inode.size) {	// if size is bigger go to indirect blocks
					printf("	indirect block: %d\n", inode.indirect);
					
					union fs_block indirect_block;
					disk_read(inode.indirect, indirect_block.data);
					printf("	indirect data blocks: ");
					
					int m = 0;	// print indirect blocks
					while ((k*sizeof(block.data)) < inode.size && m < POINTERS_PER_BLOCK) {
						int indirect;
						memcpy(&indirect, &block.data[m*sizeof(indirect)], sizeof(indirect));
						printf("%d ", indirect);
						m++;
						k++;
					}
					printf("\n");
				}

			}
		}
	}
}

int fs_mount()
{
	return 0;
}

int fs_create()
{
	return 0;
}

int fs_delete( int inumber )
{
	return 0;
}

int fs_getsize( int inumber )
{
	return -1;
}

int fs_read( int inumber, char *data, int length, int offset )
{
	return 0;
}

int fs_write( int inumber, const char *data, int length, int offset )
{
	return 0;
}
