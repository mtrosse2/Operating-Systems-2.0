/*
Implementation of SimpleFS.
Make your change here.
*/

#include "fs.h"
#include "disk.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>

extern struct disk *thedisk;
int free_blocks[2000]; // zero is false, anything else true
// int free_inodes; // just call it 10,000 size throw error when you do a check when you go to mount it
// ************************************
int ld_inode( size_t inumber, struct fs_inode *node);
int fs_save_inode( size_t inumber, struct fs_inode *node);
int scan_bitmap();
int is_mounted = 0;


int ld_inode( size_t inumber, struct fs_inode *node){
		// search for block 
    int inode_block = (int) ((inumber + INODES_PER_BLOCK - 1)/ INODES_PER_BLOCK);

    if (inode_block == 0){
        inode_block = 1;
    }
    
    int iOffset = inumber % INODES_PER_BLOCK;

    // read in inode block
    union fs_block block;
    disk_read(thedisk, inode_block, block.data);

    //node = block.inodes[iOffset];
    // copy to node pointer the inode info
    memcpy(node, &block.inode[iOffset], sizeof(struct fs_inode));
    return 1;
}

int fs_save_inode(size_t inumber, struct fs_inode *node){

    // find blk. offset val
    int inode_block = (int) ((inumber + INODES_PER_BLOCK - 1)/ INODES_PER_BLOCK);
    
    if (inode_block == 0){
        inode_block = 1;
    }
    int iOffset = inumber % INODES_PER_BLOCK;

    // read in block
    union fs_block block;
    disk_read(thedisk, inode_block, block.data);

    // cpy to blk inode info
    memcpy(&block.inode[iOffset], node, sizeof(struct fs_inode));

    // write inode info to disk
    disk_write(thedisk, inode_block, block.data);

    return 1;
}

// search through free_blocks list for a free block to use
int scan_bitmap(/*number of blocks in file system */){

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

// create a dump bitmap helper function

// ************************************


int fs_format()
{
	// check if it already has been mounted
  if (is_mounted == 1) return 0; // use global to determine if already mounted

  // format the superblock 
  union fs_block block = {{0}};
  block.super.magic = FS_MAGIC;
  block.super.nblocks = (int32_t) thedisk->nblocks;
  block.super.ninodeblocks = (int32_t) ((thedisk->nblocks + 9) / 10); // could do this with a function
  block.super.ninodes = block.super.ninodeblocks * INODES_PER_BLOCK;

	// set block data to superblock
  // memcpy(&block.data, &block.super, sizeof(fs_superblock)); // not nec
    
  // Write super block before clearing it
  disk_write(thedisk, 0, block.data);
  // set block data to 0
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
	// check that ninodes > 10,000
	// check if diskblocks is greater than 2000
	// if so throw error and say in readme we had an error check. we interpert it to be this  . . .
	
 	// need global variable set to 1 for "it has been mounted"	
	union fs_block block;
	if(is_mounted == 1){
		printf("FS as already been mounted");
		return -1;
	}
	disk_read(thedisk,0,block.data);
	// check if good magic number
	if(block.super.magic != FS_MAGIC){
		printf("Magic number is invalid");
		return -1;
	}

	// check if right number of blocks
	if(block.super.nblocks != disk_size()){
		printf("Wrong number of blocks");
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
// I never update a bitmap. Should this raise concern

int fs_create() {
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
							// TODO: for loop to make all direct pointer to zero for that specific inode

							int k = 0;
							while ((k*sizeof(inode_block.data)) < inode.size && k < POINTERS_PER_INODE) {
									inode.direct[k] = 0;
									k++;
							}		
              // get inode num 
              inode_num = (i - 1) * INODES_PER_BLOCK + j;
							// free_inodes[inode_num] = 0;
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
		struct fs_inode node;
    if (!ld_inode(inumber, &node)){
        return 0;
    }
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
    
    if (!fs_save_inode(inumber, &node)){
        return 0;
    }

    return 1;
}

int fs_getsize( int inumber )
{
	struct fs_inode inode;

  if (!ld_inode(inumber, &inode)){
      return -1;
  }

  if (inode.isvalid == 0) return -1;

  return inode.size;
}
// make sure you don't go off the bounds of the disk

int fs_read( int inumber, char *data, int length, int offset )
{
	struct fs_inode inode;
    if (!ld_inode( inumber, &inode)){
        return -1;
    }

    if (inode.isvalid == 0){
        return -1;
    }

    if (offset >= inode.size) {
        return -1;
    }

    // get block and new offset based off offset value. initialize read sizes and bytes read
    int nblk = (int) (offset / BLOCK_SIZE);
    int new_off = offset % BLOCK_SIZE;
    int bytes_read = 0;
    int need_cpy = 0;
    int need_read = 0;

    if ((inode.size - offset) < length){
        need_read = inode.size - offset; // max that can be read will be the inode.size - offset
    }
    else{
        need_read = length;
    }

    union fs_block indirect; 
    if (inode.indirect){
        disk_read(thedisk, inode.indirect, indirect.data);
            
        
    }

    // iterate through all pointers            
    for (int i = nblk; i < POINTERS_PER_BLOCK + POINTERS_PER_INODE; i++){
        union fs_block block;
        // read in the block data 
        if (i < POINTERS_PER_INODE){
            if (inode.direct){
                disk_read(thedisk, inode.direct[i], block.data);
            }
        }
        else{
            if (inode.indirect){
                if (indirect.pointers[i - POINTERS_PER_INODE]){
                    disk_read(thedisk, indirect.pointers[i - POINTERS_PER_INODE], block.data);            
                }
            }
        }

        // to prevent overflowing memcpy
        if ((BLOCK_SIZE - new_off) <= (need_read - bytes_read)){
            need_cpy = BLOCK_SIZE - new_off;
        }
        else{
            need_cpy = need_read - bytes_read;
        }
        memcpy(data + bytes_read, block.data + new_off, need_cpy);

        bytes_read += need_cpy;
        new_off = 0;
        if (bytes_read == need_read){
            return bytes_read;
        }
    }
    return bytes_read;
}

int save_inode(size_t inumber, struct fs_inode *node) {

	int inode_block = inumber / INODES_PER_BLOCK + 1;
	int iOffset = inumber % INODES_PER_BLOCK;

    // read in block
    union fs_block block;
    disk_read(thedisk, inode_block, block.data);

    // cpy to blk inode info
    memcpy(&block.inode[iOffset], node, sizeof(struct fs_inode));

    disk_write(thedisk, inode_block, block.data);

    return 1;
}

// try overriding existing file
// try coping in too much stuff
// error checking if you run out of space
// int fs_write( int inumber, const char *data, int length, int offset )
// {
// 	struct fs_inode inode;
//     if (!ld_inode(inumber, &inode)){
// 		printf("ld_inode failed\n");
//         return -1;
//     }

// 	union fs_block block;
// 	int bytes_read = 0, free_data_block;

// 	int i = 0;	// For Direct Blocks
// 	while (i < POINTERS_PER_INODE && bytes_read < length) {
// 		if (inode.direct[i] == 0) {
// 			free_data_block = scan_bitmap();

// 			// if search free list fails we need to do something !!!!

// 			memcpy(&block, &data[bytes_read], DISK_BLOCK_SIZE);

// 			disk_write(thedisk, free_data_block, block.data);
// 			inode.direct[i] = free_data_block;

// 			if (length - offset < DISK_BLOCK_SIZE) {
// 				bytes_read += length - offset;
// 				inode.size += bytes_read;
// 			}
// 			else {
// 				bytes_read += DISK_BLOCK_SIZE;
// 				inode.size += bytes_read;
// 			}
// 		}
// 		i++;
// 	}
	
// 	if (bytes_read < length) {
// 		if (inode.indirect == 0) {
// 			int free_indirect_block = scan_bitmap();

// 			// if free_indirect_block fails error check

// 			inode.indirect = free_indirect_block;
// 		}

// 		int j = 0; // For Indirect Blocks
// 		while (j < POINTERS_PER_BLOCK && bytes_read < length) {
			
// 			free_data_block = scan_bitmap();

// 			free_indirect_block[j] = free_data_block;

// 			disk_read(thedisk, free_indirect_block, &block)
			
			
			
// 			// memcpy(&block, data, DISK_BLOCK_SIZE);
// 			// disk_write(thedisk, free_indirect_block, block.data);

			



// 			memcpy(&block, data, DISK_BLOCK_SIZE);

// 			j++;
// 		}
// 	}


// 	save_inode(inumber, &inode);
// 	return bytes_read;
// }


int fs_write( int inumber, const char *data, int length, int offset )
{
	// check to make sure inode is valid
	struct fs_inode inode;
    if (!ld_inode( inumber, &inode)){
		printf("ld_inode failed\n");
        return 0;
    }

    // initialize values. need new offsets and to get blocks 
    int nblk = (int) (offset / BLOCK_SIZE);
    int new_off = offset % BLOCK_SIZE;
    int byt_wrote = 0;
    int need_cpy = 0;
    uint32_t pnt_to_blk = 0;
   	int inode_update = 0; // keeps track if inode needs to be update

    // create empty block for allocating blocks (must clear them before use)
    union fs_block empt_blk = {{0}};
    union fs_block indirect = {{0}};
    union fs_block block = {{0}};

    // if possible try and get indirect block
    if (inode.indirect){
        disk_read(thedisk, inode.indirect, indirect.data);
    }

    // iterate through all blocks of given inode
    for (int i = nblk; i < POINTERS_PER_BLOCK + POINTERS_PER_INODE; i++){           
        // direct
        if (i < POINTERS_PER_INODE){
            pnt_to_blk = inode.direct[i];
        }
				// indirect
        else{
            // if the indirect does not exist get free block to allocate
            if (!inode.indirect){
                inode.indirect = scan_bitmap();
                if (inode.indirect == -1){
                    break;
                }
                // clear the block wanted
                disk_write(thedisk, inode.indirect, empt_blk.data);
                // read the data into indirect data (empty)
                disk_read(thedisk, inode.indirect, indirect.data);
            }
            // retrieve the pointer from the ind block
            pnt_to_blk = indirect.pointers[i - POINTERS_PER_INODE];
        }

        // allocate block if needed
        if (!pnt_to_blk){
            pnt_to_blk = scan_bitmap();
            if (pnt_to_blk == -1){
                break;
            }   

            // set blk to direct pointer 
            if (i < POINTERS_PER_INODE){
                inode.direct[i] = pnt_to_blk;
            }      
            else{
                // set indirectblk pointer to block pointer as it was empty before
                indirect.pointers[i - POINTERS_PER_INODE] = pnt_to_blk;
            }
            inode_update = 1; // indicate that the inode information has inode_updated
        }

        // read in the data at the blk
        disk_read(thedisk, pnt_to_blk, block.data);

        // to prevent overflowing memcpy
        if ((BLOCK_SIZE - new_off) <= (length - byt_wrote)){
            need_cpy = BLOCK_SIZE - new_off;
        }
        else{
            need_cpy = length - byt_wrote;
        }

        // copy new data to the block
        memcpy(block.data + new_off, data + byt_wrote, need_cpy);

        // write to the block
        disk_write(thedisk, pnt_to_blk, block.data);

        // if the inode has been updated and indirect exists, update indirect block information. Direct has been updated
        if (inode.indirect && inode_update){
            disk_write(thedisk, inode.indirect, indirect.data);
        }

        // update all the counters and values
        byt_wrote += need_cpy;
        new_off = 0;
        inode_update = 0;
        if (byt_wrote == length){
            break; // break if requirement met
        }
    }

    // update and save inode information
    inode.size += byt_wrote;
		save_inode(inumber, &inode);
    return byt_wrote; 
}
