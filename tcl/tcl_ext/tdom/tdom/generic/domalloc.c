/*---------------------------------------------------------------------------
|   Copyright (C) 1999-2000  Jochen C. Loewer (loewerj@hotmail.com)
+----------------------------------------------------------------------------
|
|   $Header: /usr/local/pubcvs/tdom/generic/domalloc.c,v 1.7 2003/03/06 13:58:53 zoran Exp $
|
|
|   A special memory allocator, which uses pre-allocated / bit masked
|   based administration of memory blocks with fixed sizes, like
|   DOM nodes. This will hopefully save some memory.
|
|
|   The contents of this file are subject to the Mozilla Public License
|   Version 1.1 (the "License"); you may not use this file except in
|   compliance with the License. You may obtain a copy of the License at
|   http://www.mozilla.org/MPL/
|
|   Software distributed under the License is distributed on an "AS IS"
|   basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
|   License for the specific language governing rights and limitations
|   under the License.
|
|   The Original Code is tDOM.
|
|   The Initial Developer of the Original Code is Jochen Loewer
|   Portions created by Jochen Loewer are Copyright (C) 1998, 1999
|   Jochen Loewer. All Rights Reserved.
|
|   Contributor(s):
|
|
|   written by Jochen Loewer
|   October, 2000
|
\--------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------
|   Includes
|
\--------------------------------------------------------------------------*/
#include <tcl.h>
#include <stdlib.h>
#include <string.h>
#include <domalloc.h>


/*---------------------------------------------------------------------------
|   Defines
|
\--------------------------------------------------------------------------*/
#define DBG(x)
#define MAX_BINS         256
#define BIN_HASH_SIZE    512
#define BIN_HASH_MASK    0x01FF
#define CACHE_SIZE       4
#define BLOCK_DATA_SIZE  31000
#define BLOCK_SIZE_BITS  16


/*---------------------------------------------------------------------------
|   Typedefs
|
\--------------------------------------------------------------------------*/
typedef struct domAllocBlock {
    struct domAllocBin    * bin;
    void                  * end;
    struct domAllocBlock  * prev;
    struct domAllocBlock  * next;
    int                     hashIndex1;
    struct domAllocBlock  * hashNext1;
    int                     hashIndex2;
    struct domAllocBlock  * hashNext2;
    int                     slots;
    int                     freeSlots;
    int                     bitmaps;
    int                     freePos;
    int                     freeBit;
    unsigned int            freeMask;
} domAllocBlock;


typedef struct domAllocBin {
    int                     size;
    int                     nrSlots;
    int                     freeSlots;
    int                     nrBlocks;
    domAllocBlock         * freeBlocks;
    domAllocBlock         * usedBlocks;
} domAllocBin;


typedef struct domAllocBins {
    struct domAllocBin   * bin[MAX_BINS];
    struct domAllocBlock * hashedBlocks[BIN_HASH_SIZE];
    struct domAllocBlock * blockCache[CACHE_SIZE];
} domAllocBins;


/*---------------------------------------------------------------------------
|   Globals. This is a "single-threaded" allocator.
|
\--------------------------------------------------------------------------*/
static domAllocBins bins;

#ifdef TCL_THREADS
# define TDomThreaded(x) x
  static Tcl_Mutex binMutex;
#else
# define TDomThreaded(x)
#endif

/*---------------------------------------------------------------------------
|   domAllocInit
|
\--------------------------------------------------------------------------*/
void
domAllocInit()
{
    int i;

    DBG(fprintf(stderr, "domAllocInit...\n");)

    for (i=0; i < MAX_BINS;      i++) bins.bin[i]          = NULL;
    for (i=0; i < CACHE_SIZE;    i++) bins.blockCache[i]   = NULL;
    for (i=0; i < BIN_HASH_SIZE; i++) bins.hashedBlocks[i] = NULL;
}


/*--------------------------------------------------------------------------
|   fillHashTable
|
\--------------------------------------------------------------------------*/
static void
fillHashTable (
    domAllocBlock  * block,
    void           * mem
)
{
    domAllocBlock * hashedBlock;
    unsigned int    i;

    i = ( (unsigned int)mem >> BLOCK_SIZE_BITS) & BIN_HASH_MASK;
    hashedBlock = bins.hashedBlocks[i];
    while (hashedBlock != NULL) {
        if (hashedBlock == block) {
            /* all is fine, block is already in hash table */
            return;
        }
        if      (hashedBlock->hashIndex1 == i) hashedBlock = hashedBlock->hashNext1;
        else if (hashedBlock->hashIndex2 == i) hashedBlock = hashedBlock->hashNext2;
        else hashedBlock = NULL;
    }

    /* add block in hash table */
    if (block->hashIndex1 == -1) {
        block->hashIndex1 = i;
        block->hashNext1  = bins.hashedBlocks[i];
    } else
    if (block->hashIndex2 == -1) {
        block->hashIndex2 = i;
        block->hashNext2  = bins.hashedBlocks[i];
    } else {
        DBG(
            fprintf(stderr, "\ntoo many hash entries for %x %x->%d %d,%d!\n",
                    (unsigned int)block,
                    (unsigned int)mem, i, block->hashIndex1, block->hashIndex2);)
    }
    bins.hashedBlocks[i] = block;
}

/*--------------------------------------------------------------------------
|   domAlloc
|
\--------------------------------------------------------------------------*/
void *
domAlloc (
    int  size
)
{
    domAllocBin   * bin;
    domAllocBlock * block;
    domAllocBlock * hashedBlock;
    int             i, j, slots, bitmaps, blockSize;
    unsigned int    mask;
    char          * mem;
    unsigned int  * usedBitmap;


    DBG(fprintf(stderr, "\ndomAlloc %d \n", size);)

    if (size >= MAX_BINS) {
        DBG(fprintf(stderr, "\nSize too large as used for bin!\n");)
        return NULL;
    }

    /*-------------------------------------------------
     |   FIXME
     |
     |   Rewrite with TSD-based bins to avoid mutex
     |   contention. Threads are going to step on
     |   each other toes here which is not what we
     |   would like to have, don't we ?  (zv)
     \------------------------------------------------*/

    TDomThreaded(Tcl_MutexLock(&binMutex);) /* LOCK !*/

    if (bins.bin[size] == NULL) {
        /*-------------------------------------------------
        |   create new bin
        \------------------------------------------------*/
        bin = (domAllocBin *)malloc(sizeof(domAllocBin));
        bin->size        = size;
        bin->nrSlots     = 0;
        bin->freeSlots   = 0;
        bin->nrBlocks    = 0;
        bin->freeBlocks  = NULL;
        bin->usedBlocks  = NULL;

        bins.bin[size] = bin;

    } else {
        bin = bins.bin[size];
    }

    if (bin->freeSlots == 0) {
        DBG(fprintf(stderr, "allocating new block ... \n");)
        /*----------------------------------------------------------------
        |   allocate and initialize a new block
        |
        \---------------------------------------------------------------*/
        bitmaps   = (BLOCK_DATA_SIZE  / size) / 32;
        slots     = bitmaps * 32;
        blockSize = sizeof(domAllocBlock) + bitmaps*4 + slots*size;

        block = (domAllocBlock *)malloc( blockSize );
        block->bin        = bin;
        block->end        = (char*)block + blockSize;
        block->slots      = slots;
        block->freeSlots  = slots;
        block->bitmaps    = bitmaps;
        block->freePos    = 0;
        block->freeBit    = 0;
        block->freeMask   = 0x80000000;
        block->hashIndex1 = -1;
        block->hashNext1  = NULL;
        block->hashIndex2 = -1;
        block->hashNext2  = NULL;

        usedBitmap = (unsigned int *) ((char*)block + sizeof(domAllocBlock));
        memset(usedBitmap, 0, bitmaps * 4);

        bin->nrSlots   += slots;
        bin->freeSlots += slots;
        bin->nrBlocks++;

        block->prev     = NULL;            /* prepend this new block to free list */
        block->next     = bin->freeBlocks;
        bin->freeBlocks = block;

        /*---------------------------------------------------------
        |   enter block in 'hash' table:
        |     first and last memory location could have different
        |     hash entries due to different upper address bits
        \--------------------------------------------------------*/
        mem = (char*)usedBitmap + bitmaps * 4;
        fillHashTable (block, mem);
        mem += (slots-1) * size;
        fillHashTable (block, mem);

    } else {
        block = bin->freeBlocks;
    }

    /*------------------------------------------------------------------------
    |   find free slot in (partial) free block
    |
    \-----------------------------------------------------------------------*/
    usedBitmap = (unsigned int *) ((char*)block + sizeof(domAllocBlock));
    i    = block->freePos;  /* start at old pos to quickly find a free slot */
    j    = block->freeBit;
    mask = block->freeMask;
    do {
        DBG(fprintf(stderr, "looking %d slot i=%d j=%d %x mask %x\n",
                                        size, i, j, usedBitmap[i], mask); )
        if (usedBitmap[i] != 0xFFFFFFFF) {
            do {
                if ((usedBitmap[i] & mask)==0) {
                    DBG(fprintf(stderr, "found free slot i=%d j=%d %x mask %x\n",
                                        i, j, usedBitmap[i], mask); )
                    mem = ((char*)usedBitmap) + (4*block->bitmaps) + ((i*32)+j) * size;
                    usedBitmap[i] |= mask;
                    block->freeSlots--;
                    bin->freeSlots--;
                    if (block->freeSlots == 0) {
                        DBG(fprintf(stderr, "freeSlots == 0\n");)

                        if (block->prev == NULL) {      /* remove block from free list */
                            bin->freeBlocks   = block->next;
                        } else {
                            block->prev->next = block->next;
                        }
                        if (block->next) block->next->prev = block->prev;

                        block->next     = bin->usedBlocks;  /* add block to used list */
                        if (block->next) block->next->prev = block;
                        block->prev     = NULL;
                        bin->usedBlocks = block;

                        /* check consistency */
                        hashedBlock = block->bin->freeBlocks;
                        while (hashedBlock) {
                            if (hashedBlock == block) {
                                DBG(fprintf(stderr, "strange block still in free list \n");)
                            }
                            hashedBlock = hashedBlock->next;
                        }

                    }
                    /* keep found free position for later,
                     * so that next slots can be found quickly
                     */
                    block->freePos  = i;
                    j++; mask = mask >> 1;
                    if (j >= 32) { j = 0; mask = 0x80000000; }
                    block->freeBit  = j;
                    block->freeMask = mask;

                    TDomThreaded(Tcl_MutexUnlock(&binMutex);) /* UNLOCK !*/
                    return mem;
                }
                j++; mask = mask >> 1;
                if (j >= 32) { j = 0; mask = 0x80000000; }
            } while (j != block->freeBit);
        }
        i++;
        if (i >= block->bitmaps) i = 0;
    } while (i != block->freePos);

    /* TDomThreaded(Tcl_MutexUnlock(&binMutex);) */

    DBG(fprintf(stderr, "\ndomAlloc: can't happen! \n");)
    *((char*)0) = 0; /* Use Tcl_Panic() for this ? */
    return NULL;
}


/*---------------------------------------------------------------------------
|   domFree
|
\--------------------------------------------------------------------------*/
void
domFree (
    void  * mem
)
{
    domAllocBlock * block;
    domAllocBlock * hashedBlock;
    domAllocBlock * prevBlock;
    int             slotNr, i, foundInCache;
    unsigned int  * usedBitmap;
    unsigned int    mask;

    DBG(fprintf(stderr, "domFree...\n");)

    if (mem == NULL) return;

    /*-------------------------------------------------
     |   FIXME (see domAlloc comments)
     |
     \------------------------------------------------*/

    TDomThreaded(Tcl_MutexLock(&binMutex);)

    /*-------------------------------------------------------------------
    |   Find the block, which corresponds to the given memory location
    |
    |   - First try to look in the memory range cache.
    |
    \------------------------------------------------------------------*/
    block = NULL;
    foundInCache = 0;
    for (i=0; i < CACHE_SIZE; i++) {
        if ((bins.blockCache[i] != NULL) &&
            (mem > (void*)(bins.blockCache[i])) &&
            (mem < (void*)(bins.blockCache[i]->end))) {
           block = bins.blockCache[i];
           foundInCache = 1;
           break;
        }
    }
    /*-------------------------------------------------------------------
    |   - Otherwise try to lookup corresponding block in hashtable
    |
    \------------------------------------------------------------------*/
    if (!foundInCache) {
        i = ( (unsigned int)mem >> BLOCK_SIZE_BITS) & BIN_HASH_MASK;
        block = bins.hashedBlocks[i];
        while (block != NULL) {
            if ((mem > (void*)block) && (mem < (void*)(block->end))) break;
            if      (block->hashIndex1 == i) block = block->hashNext1;
            else if (block->hashIndex2 == i) block = block->hashNext2;
            else block = NULL;
        }
    }

    if (block == NULL) {
        DBG(fprintf(stderr, "\n unable to free mem %x !\n", (unsigned int)mem);)
        TDomThreaded(Tcl_MutexUnlock(&binMutex);)
        return;
    }

    /*-------------------------------------------------------------------
    |   clear the allocation bit
    \------------------------------------------------------------------*/
    usedBitmap = (unsigned int *) ((char*)block + sizeof(domAllocBlock));
    slotNr = ( (char*)mem - (char*)usedBitmap - block->bitmaps*4 ) / block->bin->size;
    DBG(
    if (slotNr >= block->slots) {
        fprintf(stderr, "assertion failed: slotNr = %d \n", slotNr);
    })
    i = slotNr >> 5 ;  /* slotNr / 32 */
    mask = 0x80000000 >> (slotNr % 32);
    usedBitmap[i] &= ~mask;
    block->freeSlots++;
    block->bin->freeSlots++;

    DBG(
    if ((block->freeSlots < 1) || (block->freeSlots > block->slots)) {
        fprintf(stderr, "assertion failed: freeSlots = %d \n", block->freeSlots);
    })

    /*-------------------------------------------------------------------
    |   update free/used lists
    \------------------------------------------------------------------*/
    if (block->freeSlots == 1) {
        if (block->prev == NULL) {      /* remove block from used list */
            block->bin->usedBlocks = block->next;
        } else {
            block->prev->next = block->next;
        }
        if (block->next) block->next->prev = block->prev;

        block->next            = block->bin->freeBlocks;  /* add block to free list */
        if (block->next) block->next->prev = block;
        block->prev            = NULL;
        block->bin->freeBlocks = block;

        DBG(
        /* check consistency */
        hashedBlock = block->bin->usedBlocks;
        while (hashedBlock) {
            if (hashedBlock == block) {
                fprintf(stderr, "strange block still in used list \n");
            }
            hashedBlock = hashedBlock->next;
        }
        )
    }

    /*-------------------------------------------------------------------
    |   free the whole block, when all slots are freed
    \------------------------------------------------------------------*/
    if (block->freeSlots == block->slots) {

        DBG(fprintf(stderr, "block completely freed %x\n",
                             (unsigned int)block);)

        if (block->prev == NULL) {      /* remove block from free list */
            block->bin->freeBlocks = block->next;
        } else {
            block->prev->next = block->next;
        }
        if (block->next) block->next->prev = block->prev;

        block->bin->nrSlots   -= block->slots;
        block->bin->freeSlots -= block->slots;
        block->bin->nrBlocks--;

        /*--------------------------------------------------------------------
        |   remove block from (two) hash lists
        \-------------------------------------------------------------------*/
        i = block->hashIndex1;
        if (i != -1) {
            DBG(fprintf(stderr, "remove from hash list %d \n", i);)
            prevBlock = NULL;
            hashedBlock = bins.hashedBlocks[i];
            while (hashedBlock) {
                if (hashedBlock == block) break;
                prevBlock = hashedBlock;
                if      (hashedBlock->hashIndex1 == i) hashedBlock = hashedBlock->hashNext1;
                else if (hashedBlock->hashIndex2 == i) hashedBlock = hashedBlock->hashNext2;
                else hashedBlock = NULL;
            }
            if (prevBlock == NULL) {
                bins.hashedBlocks[i] = block->hashNext1;
            } else {
                if      (prevBlock->hashIndex1 == i) prevBlock->hashNext1 = block->hashNext1;
                else if (prevBlock->hashIndex2 == i) prevBlock->hashNext2 = block->hashNext1;
            }
        }
        i = block->hashIndex2;
        if (i != -1) {
            DBG(fprintf(stderr, "remove from hash list %d \n", i);)
            prevBlock = NULL;
            hashedBlock = bins.hashedBlocks[i];
            while (hashedBlock) {
                if (hashedBlock == block) break;
                prevBlock = hashedBlock;
                if      (hashedBlock->hashIndex1 == i) hashedBlock = hashedBlock->hashNext1;
                else if (hashedBlock->hashIndex2 == i) hashedBlock = hashedBlock->hashNext2;
                else hashedBlock = NULL;
            }
            if (prevBlock == NULL) {
                bins.hashedBlocks[i] = block->hashNext2;
            } else {
                if      (prevBlock->hashIndex1 == i) prevBlock->hashNext1 = block->hashNext2;
                else if (prevBlock->hashIndex2 == i) prevBlock->hashNext2 = block->hashNext2;
            }
        }

        /*------------------------------------------------------
        |   remove block from cache, if found
        \-----------------------------------------------------*/
        for (i=0; i < CACHE_SIZE; i++) {
            if (bins.blockCache[i] == block) {
                bins.blockCache[i] = NULL;
            }
        }

        DBG(
        /* check consistency */
        for (i=0; i < block->bitmaps; i++) {
            if (usedBitmap[i] != 0) {
                fprintf(stderr, "strange bitmap %d is %x \n", i, usedBitmap[i]);
            }
        }
        for (i=0; i < BIN_HASH_SIZE; i++) {
            hashedBlock = bins.hashedBlocks[i];
            while (hashedBlock) {
                if (hashedBlock == block) {
                    fprintf(stderr, "strange block %d still in hash table \n", i);
                }
                if      (hashedBlock->hashIndex1 == i) hashedBlock = hashedBlock->hashNext1;
                else if (hashedBlock->hashIndex2 == i) hashedBlock = hashedBlock->hashNext2;
                else hashedBlock = NULL;
            }
        }
        hashedBlock = block->bin->freeBlocks;
        while (hashedBlock) {
            if (hashedBlock == block) {
                fprintf(stderr, "strange block still in free list \n");
            }
            hashedBlock = hashedBlock->next;
        }
        hashedBlock = block->bin->usedBlocks;
        while (hashedBlock) {
            if (hashedBlock == block) {
                fprintf(stderr, "strange block still in used list \n");
            }
            hashedBlock = hashedBlock->next;
        }
        )
        free((char*)block);

    } else {
        /*-----------------------------------------------------------
        |   update cache
        \----------------------------------------------------------*/
        if (!foundInCache) {
            /* remove oldest entry and add this block */
            for (i=1; i < CACHE_SIZE; i++) {
                bins.blockCache[i-1] = bins.blockCache[i];
            }
            bins.blockCache[CACHE_SIZE-1] = block;
        }
    }
    TDomThreaded(Tcl_MutexUnlock(&binMutex);) /* UNLOCK !*/
}

