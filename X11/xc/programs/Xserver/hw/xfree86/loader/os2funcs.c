/* $XFree86: xc/programs/Xserver/hw/xfree86/loader/os2funcs.c,v 1.6 2002/05/31 18:46:00 dawes Exp $ */
/*
 * (c) Copyright 1997 by Sebastien Marineau
 *                      <marineau@genie.uottawa.ca>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * SEBASTIEN MARINEAU BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
 * OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Except as contained in this notice, the name of Sebastien Marineau shall not be
 * used in advertising or otherwise to promote the sale, use or other dealings
 * in this Software without prior written authorization from Sebastien Marineau.
 *
 */


/* Implements some OS/2 memory allocation functions to allow 
 * execute permissions for modules. We allocate some mem using DosAllocMem
 * and then use the EMX functions to create a heap from which we allocate
 * the requests. We create a heap of 2 megs, hopefully enough for now.
 */

#define I_NEED_OS2_H
#define INCL_DOSMEMMGR
#include <os2.h>
#include <sys/types.h>
#include <umalloc.h>
#include "os.h"
#include "xf86str.h"

#define RESERVED_BLOCKS 4096  /* reserve 16MB memory for modules */

void *os2ldAddToHeap(Heap_t, size_t *, int *);
void os2ldRemoveFromHeap(Heap_t, void *, size_t);

PVOID os2ldCommitedTop;
PVOID os2ldBase;
Heap_t os2ldHeap;
int os2ldTotalCommitedBlocks;
static BOOL FirstTime = TRUE;

void *os2ldcalloc(size_t num_elem, size_t size_elem) {
	APIRET rc;
	int ret;

	if (FirstTime) {
		if ((rc=DosAllocMem(&os2ldBase,RESERVED_BLOCKS * 4096, 
				PAG_READ | PAG_WRITE | PAG_EXECUTE)) != 0) {
			xf86Msg(X_ERROR,
				"OS2LD: DosAllocMem failed, rc=%d\n",
				rc);
			return NULL;
		}

		/* Now commit the first 128Kb, the rest will 
		 * be done dynamically */
		if ((rc=DosSetMem(os2ldBase,
				  32*4096,
				  PAG_DEFAULT | PAG_COMMIT)) != 0) {
			xf86Msg(X_ERROR,
				"OS2LD: DosSetMem failed, rc=%d\n",rc);
			DosFreeMem(os2ldBase);
			return NULL;
		}
	        os2ldCommitedTop = os2ldBase + 32*4096;
		os2ldTotalCommitedBlocks = 32;
#ifdef DEBUG
		xf86Msg(X_INFO,
			"OS2LD: Initial heap at addr=%p\n",
			os2ldBase);
#endif

		if ((os2ldHeap=_ucreate(os2ldBase,
					32*4096, _BLOCK_CLEAN,
					_HEAP_REGULAR, os2ldAddToHeap, 
					os2ldRemoveFromHeap)) == NULL) {
			xf86Msg(X_ERROR,
				"OS2LD: heap creation failed\n");
			DosFreeMem(os2ldBase);
			return NULL;
		}
	
		if ((ret=_uopen(os2ldHeap)) != 0) {
			xf86Msg(X_ERROR,
				"OS2LD: heap open failed\n");
			ret = _udestroy(os2ldHeap,_FORCE);
			DosFreeMem(os2ldBase);
			return(NULL);
		}

		FirstTime = FALSE;

#ifdef DEBUG
		xf86Msg(X_INFO,"OS2LD: Created module heap at addr=%p\n",
			os2ldHeap);
#endif
	}

	return _ucalloc(os2ldHeap,num_elem,size_elem);
}

void *os2ldAddToHeap(Heap_t H, size_t *new_size, int *PCLEAN)
{
	PVOID NewBase;
	long adjusted_size;
	long blocks;
	APIRET rc;

	if (H != os2ldHeap) {
		xf86Msg(X_ERROR,
			"OS2LD: Heap corruption in GrowHeap, p=%08x\n",H);
		return NULL;
	}
	NewBase = os2ldCommitedTop;
	adjusted_size = (*new_size/65536) * 65536;
	if ((*new_size % 65536) > 0) 
		adjusted_size += 65536;
	blocks = adjusted_size / 4096;

	if ((os2ldTotalCommitedBlocks + blocks) > RESERVED_BLOCKS) {
		xf86Msg(X_ERROR,
			"OS2LD: Out of memory in GrowHeap\n");
		xf86Msg(X_ERROR,
			"OS2LD: Max available memory is of %ld bytes\n",
			RESERVED_BLOCKS * 4096);
		return NULL;
	}
	if ((rc=DosSetMem(NewBase, adjusted_size, PAG_DEFAULT | PAG_COMMIT)) != 0) {
		xf86Msg(X_ERROR,
			"OS2LD: DosSetMem failed in GrowHeap, size req'd=%d, rc=%d\n",
			adjusted_size,
			rc);
		return NULL;
	}

	os2ldCommitedTop += adjusted_size;
	os2ldTotalCommitedBlocks += blocks;
	*PCLEAN = _BLOCK_CLEAN;
	*new_size = adjusted_size;
#ifdef DEBUG
	xf86Msg(X_INFO,"OS2LD: Heap extended by %d bytes, addr=%p\n",
		adjusted_size, NewBase);
#endif
	return NewBase;
}

void os2ldRemoveFromHeap(Heap_t H, void *memory, size_t size)
{
	if (H != os2ldHeap) {
		xf86Msg(X_ERROR,
			"OS2LD: Heap corruption in ShrinkHeap, p=%08x\n",H);
		return;
	}

	/* Currently we do nothing, as we do not keep track of the
	 * commited memory */


	/* Only handle it if it is the base address */
	if (memory == os2ldBase) {
		DosFreeMem(os2ldBase);
#ifdef DEBUG
		xf86Msg(X_INFO,"OS2LD: total heap area deallocated\n");
#endif
		os2ldBase = 0;
		FirstTime = TRUE;
	}
}
