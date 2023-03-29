/* Definition file for byteblock */

#include <sys/types.h>

#ifndef __BYTEBLOCK_H
#define __BYTEBLOCK_H

/* Hold a block of data */
struct ByteBlock
{
    /* Pointer to the data */
    char * pData;
    /* Size of this block */
    uint32_t nSize;
};

/** Allocate a data block of a particular size 
 * @param nLength Size of the new data block 
 * @returns Non-NULL if valid, NULL if there was an error 
*/
struct ByteBlock * createBlock (int nLength);

#endif
