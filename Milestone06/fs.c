/*
Implementation of SimpleFS.
Make your changes here.
*/

#include "fs.h"
#include "disk.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>

extern struct disk *thedisk;
int *free_blocks[2000] = {1}; // zero is false, anything else true
int *free_inodes; 
// ************************************
int  fs_load_inode( size_t inumber, fs_inode *node);
int fs_save_inode( size_t inumber, fs_inode *node);
int search_free_list();


int fs_load_inode( size_t inumber, Inode *node){
		// find blk and offset val
    ssize_t iBlock = (ssize_t) ((inumber + INODES_PER_BLOCK - 1)/ INODES_PER_BLOCK);

    if (iBlock == 0){
        iBlock = 1;
    }
    
    ssize_t iOffset = inumber % INODES_PER_BLOCK;

    // read in inode block
    fs_block block;
    if (disk_read(thedisk, iBlock, block.data) == (-1)){
        return 0;
    }

    //node = block.inodes[iOffset];
    // copy to node pointer the inode info
    memcpy(node, &block.inode[iOffset], sizeof(fs_inode));
    return 1;
}

bool fs_save_inode(size_t inumber, Inode *node){

    // find blk. offset val
    ssize_t iBlock = (ssize_t) ((inumber + INODES_PER_BLOCK - 1)/ INODES_PER_BLOCK);
    
    if (iBlock == 0){
        iBlock = 1;
    }
    ssize_t iOffset = inumber % INODES_PER_BLOCK;

    // read in block
    fs_block block;
    if (disk_read(thedisk, iBlock, block.data) == (-1)){
        return 0;
    }

    // cpy to blk inode info
    memcpy(&block.inode[iOffset], node, sizeof(fs_inode));

    // write inode info to disk
    if (disk_write(thedisk, iBlock, block.data) == (-1)){
        return 0;
    }

    return 1;
}

// ************************************


int fs_format()
{
	// check if it already has been mounted
  if (thedisk) return 0;

  // format the superblock 
  fs_block block = {{0}};
  block.super.magic_number = FS_MAGIC;
  block.super.blocks = thedisk.nblocks;
  block.super.ninodeblocks = (uint32_t) ((thedisk.nblocks + 9) / 10);
  block.super.ninodes = block.super.ninodeblocks * INODES_PER_BLOCK;

	// set block data to superblock
  memcpy(&block.data, &block.super, sizeof(fs_superblock));
    
  // Write super block before clearing it
  if (disk_write(thedisk, 0, block.data) == (-1)){
      return 0;
  }
  // set block data to 0
  memset(block.data, 0, BLOCK_SIZE); // set block data to 0

  // loop through all other blocks and write the 0 data to them
  for (int i=1; i < thedisk.nblocks; i++){
      if (disk_write(disk, i, block.data) == (-1)){
         return 0;
      }
  }

	return 1;
}

void fs_debug()
{
	union fs_block block;

	disk_read(thedisk,0,block.data);

	printf("superblock:\n");
	printf("    0x%08X -> magic number\n", block.super.magic);
	printf("    %d blocks\n",block.super.nblocks);
	printf("    %d inode blocks\n",block.super.ninodeblocks);
	printf("    %d inodes\n",block.super.ninodes);

	int i;
	for (i = 1; i < block.super.ninodeblocks + 1; i++) {
		disk_read(thedisk, i, block.data);

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
					disk_read(thedisk, inode.indirect, indirect_block.data);
					printf("	indirect data blocks: ");
					
					int m = 0;	// print indirect blocks
					while ((k*sizeof(indirect_block.data)) < inode.size && m < POINTERS_PER_BLOCK) {
						int indirect;
						memcpy(&indirect, &indirect_block.data[m*sizeof(indirect)], sizeof(indirect));
						printf("%d ", indirect);
						m++;
						k++;
					}
					// printf("\n");
				}

			}
		}
	}
}

int fs_mount()
    return 0;
}


int fs_create() {
	return 0;
}

int fs_delete( int inumber )
{

    return 0;
}

int fs_getsize( int inumber )
{
  return 0;
}

int fs_read( int inumber, char *data, int length, int offset )
{
    return 0;
}

int fs_write( int inumber, const char *data, int length, int offset )
{
      
    return 0; 
}
