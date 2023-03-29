/* Code for the byte block */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "byteblock.h"

struct ByteBlock * createBlock (int nLength)
{
    struct ByteBlock * pBlock;

    pBlock = (struct ByteBlock *) malloc(sizeof(struct ByteBlock));

    if(pBlock == NULL)
    {
        return NULL;
    }

    pBlock->pData = (char *) malloc(nLength);

    /* Clean it out just to be nice */
    memset(pBlock->pData, 0, nLength);

    if(pBlock->pData == NULL)
    {
        return NULL;
    }

    pBlock->nSize = nLength;

    return pBlock;
}
