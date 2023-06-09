/*
Implementation of SimpleFS.
Make your change here.
*/

#include "fs.h"
#include "disk.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define BITMAP_SIZE 2000

extern struct disk *thedisk;
int free_blocks[BITMAP_SIZE]; // zero is false, anything else true
void ld_inode( size_t inumber, struct fs_inode *node);
void save_inode( size_t inumber, struct fs_inode *node);
int scan_bitmap();
int is_mounted = 0;

// search through free_blocks list for a free block to use
int scan_bitmap() {

	union fs_block block;
	disk_read(thedisk,0,block.data);

	// printf("%d, %d\n", block.super.ninodeblocks, block.super.nblocks);
    for (int i = block.super.ninodeblocks + 1; i < block.super.nblocks; i++){ // need the equivalent going to be super.nblocks
        if (free_blocks[i] == 0){
            free_blocks[i] = 1;
            return i;
        }
    }

    return -1;
}

int fs_format()
{
	// check if it already has been mounted
  if (is_mounted == 1) return 0; // use global to determine if already mounted

  // format the superblock 
  union fs_block block = {{0}};
  block.super.magic = FS_MAGIC;
  block.super.nblocks = (int) thedisk->nblocks;
  block.super.ninodeblocks = (int) (thedisk->nblocks / 10);
  block.super.ninodes = block.super.ninodeblocks * INODES_PER_BLOCK;
    
  // Write super block before clearing it
  disk_write(thedisk, 0, block.data);

  memset(block.data, 0, BLOCK_SIZE); // set block data to 0

  // loop through all other blocks and write the 0 data to them
  for (int i=1; i < thedisk->nblocks; i++){
      disk_write(thedisk, i, block.data);
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
					 printf("\n");
				}

			}
		}
	}
}

int fs_mount()
{
	union fs_block block;
	if(is_mounted == 1){
		printf("Error: FS has already been mounted");
		return -1;
	}
	disk_read(thedisk,0,block.data);
	// check if good magic number
	if(block.super.magic != FS_MAGIC){
		printf("Error: Magic number is invalid");
		return -1;
	}

	// check if right number of blocks
	if(block.super.nblocks != disk_size()){
		printf("Error: Wrong number of blocks");
		return -1;
	}
	// do debug but instead of print just update free_blocks
	int i;
	for (i = 1; i < block.super.ninodeblocks + 1; i++) {
		disk_read(thedisk, i, block.data);

		int j;	// loop through the inode blocks
		for (j = 0; j < INODES_PER_BLOCK; j++) {
			struct fs_inode inode;
			memcpy(&inode, &block.inode[j], sizeof(struct fs_inode));

			if (inode.isvalid) {	// if an inode block is valid print its information
				int k = 0;
				while ((k*sizeof(block.data)) < inode.size && k < POINTERS_PER_INODE) {
					free_blocks[inode.direct[k]] = 1;
					k++;
				}

				if ((k*sizeof(block.data)) < inode.size) {	// if size is bigger go to indirect blocks
					
					union fs_block indirect_block;
					disk_read(thedisk, inode.indirect, indirect_block.data);
					
					int m = 0;	// print indirect blocks
					while ((k*sizeof(indirect_block.data)) < inode.size && m < POINTERS_PER_BLOCK) {
						int indirect;
						memcpy(&indirect, &indirect_block.data[m*sizeof(indirect)], sizeof(indirect));
						free_blocks[indirect] = 1;
						m++;
						k++;
					}
				}

			}
		}
	}
		is_mounted = 1;
    return 1;

}

int fs_create() {

	if (is_mounted == 0) {
        printf("Error: FS not mounted!\n");
        return -1;
    }

    // get superblock info
    union fs_block super_block;
    disk_read(thedisk, 0, super_block.data);
  
	// loop through inode blocks
    union fs_block inode_block;
    int inode_num = -1;
    for(int i = 1; i < super_block.super.ninodeblocks + 1; i++){                
        disk_read(thedisk, i, inode_block.data);

                // loop through inodes inside inode blocks
        for(int j = 1; j < INODES_PER_BLOCK; j++){
            struct fs_inode inode = inode_block.inode[j];
            if (inode.isvalid == 0){
                // set/clear attributes
                inode.isvalid = 1;
                inode.size = 0;
                
                // clean out direct pointers:
                int k = 0;
                while ((k*sizeof(inode_block.data)) < inode.size && k < POINTERS_PER_INODE) {
                    inode.direct[k] = 0;
                    k++;
                }

                // get inode num 
                inode_num = (i - 1) * INODES_PER_BLOCK + j;

                // save inode to block and write to disk
                memcpy(&inode_block.inode[j], &inode, sizeof(struct fs_inode));
                disk_write(thedisk, i, inode_block.data);
                return inode_num;
            }
        }
    }

  return inode_num; //returns -1 if not found

}

int fs_delete( int inumber )
{
	if (is_mounted == 0) {
        printf("Error: FS not mounted!\n");
        return -1;
    }

    // check that delete inumber exists
    union fs_block block;
    disk_read(thedisk, 0, block.data);
    if (inumber > block.super.ninodeblocks * INODES_PER_BLOCK || inumber == 0) {
        printf("Error: Invalid inode specified. Can not delete inode %d\n", inumber);
        return -1;
    }
    
    struct fs_inode node;
    ld_inode(inumber, &node);
	
    // if its empty return
    if (node.isvalid == 0){  
        return 0;
    }
    // iterate through direct pointer in inode
    for (int l = 0; l < POINTERS_PER_INODE; l++){
        if (node.direct[l] > 0){
            free_blocks[node.direct[l]] = 0; // free direct blks
        }
    }
    // check indirect block existence 
    if (node.indirect > 0){
       	union fs_block indirectBlock;
        // read in indirect blk
        disk_read(thedisk, node.indirect, indirectBlock.data);

        // iterate through pointers of indirect block
        for (int k = 0; k < POINTERS_PER_BLOCK; k++){
            if (indirectBlock.pointers[k] > 0){
            	free_blocks[indirectBlock.pointers[k]] = 0; // set to true to indicate as free
            }
        }
        free_blocks[node.indirect] = 0; // free indirect block itself
    }
    node.isvalid = 0;
    node.size = 0;
    
    save_inode(inumber, &node);

    return 1;
}

int fs_getsize( int inumber )
{
    struct fs_inode inode;

    ld_inode(inumber, &inode);

    if (inode.isvalid == 0) return -1;

    return inode.size;
}

int fs_read( int inumber, char *data, int length, int offset )
{
	if (is_mounted == 0) {
        printf("Error: FS not mounted!\n");
        return -1;
    }

	struct fs_inode inode;
    ld_inode(inumber, &inode);

    if (inode.isvalid == 0){
        return -1;
    }

    // make sure you don't go off the bounds of the inode
    if (offset >= inode.size) {
        return -1;
    }

    // get block and new offset based off offset value. initialize read sizes and bytes read
    int nblk = offset / BLOCK_SIZE;
    int newOffset = offset % BLOCK_SIZE;
    int bytesRead = 0;
    int numToCopy = 0;
    int numToRead = 0;

    if ((inode.size - offset) < length){
        numToRead = inode.size - offset; // max that can be read will be the inode.size - offset
    }
    else{
        numToRead = length;
    }

    union fs_block indirect;

    // iterate through all pointers            
    for (int i = nblk; i < POINTERS_PER_BLOCK + POINTERS_PER_INODE; i++){
        union fs_block block;
        // read in the block data 
        if (i < POINTERS_PER_INODE){
            disk_read(thedisk, inode.direct[i], block.data);
        }
        else{
            if (inode.indirect) {
                disk_read(thedisk, inode.indirect, indirect.data);

                if (indirect.pointers[i - POINTERS_PER_INODE]){
                    disk_read(thedisk, indirect.pointers[i - POINTERS_PER_INODE], block.data);            
                }
            }
        }

        // to prevent overflowing memcpy
        if ((BLOCK_SIZE - newOffset) <= (numToRead - bytesRead)){
            numToCopy = BLOCK_SIZE - newOffset;
        }
        else{
            numToCopy = numToRead - bytesRead;
        }
        memcpy(data + bytesRead, block.data + newOffset, numToCopy);

        bytesRead += numToCopy;
        newOffset = 0;
        if (bytesRead == numToRead){
            return bytesRead;
        }
    }
    return bytesRead;
}

void save_inode(size_t inumber, struct fs_inode *node) {

	int inode_block = inumber / INODES_PER_BLOCK + 1;
	int iOffset = inumber % INODES_PER_BLOCK;

    // read in block
    union fs_block block;
    disk_read(thedisk, inode_block, block.data);

    // cpy to blk inode info
    memcpy(&block.inode[iOffset], node, sizeof(struct fs_inode));

    disk_write(thedisk, inode_block, block.data);
}

void ld_inode(size_t inumber, struct fs_inode *node){

    int inode_block = inumber / INODES_PER_BLOCK + 1;
    int iOffset = inumber % INODES_PER_BLOCK;

    // read in inode block
    union fs_block block;
    disk_read(thedisk, inode_block, block.data);

    // copy to node pointer the inode info
    memcpy(node, &block.inode[iOffset], sizeof(struct fs_inode));
}

int fs_write( int inumber, const char *data, int length, int offset ) {
    
	if (is_mounted == 0) {
        printf("Error: FS not mounted!\n");
        return -1;
    }
    
    struct fs_inode inode;
    ld_inode(inumber, &inode);
    if (!inode.isvalid){
		    printf("Invalid Write: inode %d must be created first\n", inumber);
        return -1;
    }

    int nblk = offset / BLOCK_SIZE;
    int bytesWritten = 0;

    int numToCopy = 0;
    int newOffset = offset % BLOCK_SIZE;

    int pnt2blk = 0;

    union fs_block emptyBlock = {{0}};
    union fs_block indirect = {{0}};
    union fs_block block = {{0}};

    int newDataBlock = 0;

    int i = nblk, usingIndirect = 0;
    while (bytesWritten < length && i < POINTERS_PER_BLOCK + POINTERS_PER_INODE) {

        if (i < POINTERS_PER_INODE) {
            newDataBlock = scan_bitmap();
            if (newDataBlock == -1) {
                break;
            }
            inode.direct[i] = newDataBlock;
            pnt2blk = inode.direct[i];
        }
        else {
            if (!inode.indirect) {
                newDataBlock = scan_bitmap();
                if (newDataBlock == -1) {
                    break;
                }
                inode.indirect = newDataBlock;

                // printf("Writing to disk block: %d\n", inode.indirect);
                disk_write(thedisk, inode.indirect, emptyBlock.data);
                // clear the inode's indirect block
                disk_read(thedisk, inode.indirect, indirect.data);
            }
            else {
                disk_read(thedisk, inode.indirect, indirect.data);
            }
            usingIndirect = 1;
            newDataBlock = scan_bitmap();
            if (newDataBlock == -1) {
                break;
            }
            pnt2blk = newDataBlock;
            indirect.pointers[i - POINTERS_PER_INODE] = pnt2blk;
        }

        // read the data at pnt2blk
        disk_read(thedisk, pnt2blk, block.data);

        if ((BLOCK_SIZE - newOffset) <= (length - bytesWritten)) {
            numToCopy = BLOCK_SIZE - newOffset;
        }
        else {
            numToCopy = length - bytesWritten;
        }

        // copy new data to block
        memcpy(block.data + newOffset, data + bytesWritten, numToCopy);

        // write block to disk
        // printf("Writing to disk block: %d\n", pnt2blk);
        disk_write(thedisk, pnt2blk, block.data);

        // update indirect pointer info
        if (usingIndirect) {
            // printf("Writing to indirect disk block: %d\n", inode.indirect);
            disk_write(thedisk, inode.indirect, indirect.data);
        }

        bytesWritten += numToCopy;
        newOffset = 0;
        usingIndirect = 0;

        if (bytesWritten == length) {
            break;
        }
        i++;
    }

    if (bytesWritten < length) {
        printf("Warning: Disk is full!\n");
    }

    inode.size += bytesWritten;
    save_inode(inumber, &inode);

    return bytesWritten;
}