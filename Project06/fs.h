/* fs.h : Definition file for the SimpleFS filesystem 
 *************************************************************
 * Generally, you should not modify this file unless abolutely
 * needed.
 * 
 * You should make all of your changes to fs.c if possible but 
 * changes to this file are not explicitly disallowed.
 *************************************************************
 * Last Updated: 2023-04-23
 */

#ifndef FS_H
#define FS_H

#include <stdio.h>
#include <stdint.h>

#include "disk.h"

/* Definitions for the file itself */

#define FS_MAGIC           0x30341003
#define INODES_PER_BLOCK   128
#define POINTERS_PER_INODE 3
#define POINTERS_PER_BLOCK 1024

struct fs_superblock {
    /* Magic number - should be FS_MAGIC */
	int32_t magic;

    /* Number of blocks on the disk, should be the same 
       as disk_size or disk_nblocks 
     */
	int32_t nblocks;

    /* The number of blocks dedicated to inodes */
	int32_t ninodeblocks;

    /* The total number of possible inodes which should be 
       effectively DISK_BLOCK_SIZE / INODES_PER_BLOCK */
	int32_t ninodes;
};

struct fs_inode {
	int32_t isvalid;
	int32_t size;
	int64_t ctime;
	int32_t direct[POINTERS_PER_INODE];
	int32_t indirect;
};

union fs_block {
	struct fs_superblock super;
	struct fs_inode inode[INODES_PER_BLOCK];
	int pointers[POINTERS_PER_BLOCK];
	unsigned char data[BLOCK_SIZE];
};

int  fs_format();
void fs_debug();
int  fs_mount();

int  fs_create();
int  fs_delete( int inumber );
int  fs_getsize( int inumber );

int  fs_read( int inumber, char *data, int length, int offset );
int  fs_write( int inumber, const char *data, int length, int offset );

#endif
