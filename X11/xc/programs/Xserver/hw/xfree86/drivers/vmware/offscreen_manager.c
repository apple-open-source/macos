/* **********************************************************
 * Copyright (C) 1998-2002 VMware, Inc.
 * All Rights Reserved
 * **********************************************************/
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/vmware/offscreen_manager.c,v 1.2 2002/12/11 17:07:58 dawes Exp $ */

#include "vmware.h"

struct _Heap {
    CARD8* ptr;
    CARD32 size;
    CARD32 maxSlots;
    CARD32 startOffset;
    SVGASurface* frontBuffer;
    SVGASurface* slotsStart;
    Bool clear;
};

static SVGASurface* FillInSurface(Heap* heap, SVGASurface* surface,
                                  CARD32 width, CARD32 height,
                                  CARD32 bpp, CARD32 pitch, CARD32 size,
                                  CARD32 sizeUsed);

Heap*
vmwareHeap_Create(CARD8* ptr, CARD32 size, CARD32 maxSlots, CARD32 startOffset,
            CARD32 sWidth, CARD32 sHeight, CARD32 sBPP, CARD32 sPitch,
            CARD32 sFbOffset)
{
    Heap* newHeap = malloc(sizeof (Heap));

    newHeap->ptr = ptr;
    newHeap->size = size - sizeof(SVGASurface); /* leave room for frontbuffer */
    newHeap->maxSlots = maxSlots;
    newHeap->startOffset = startOffset;

    newHeap->frontBuffer = FillInSurface(newHeap,
                                         (SVGASurface*)(ptr + newHeap->size),
                                         sWidth, sHeight, sBPP, sPitch,
                                         sHeight * sPitch, 0);
    newHeap->frontBuffer->dataOffset = sFbOffset;
    newHeap->frontBuffer->numQueued = newHeap->frontBuffer->numDequeued = 0;

    newHeap->slotsStart = (SVGASurface*)(newHeap->ptr + newHeap->size) - 
                                          newHeap->maxSlots;
    newHeap->clear = FALSE;
    vmwareHeap_Clear(newHeap);

    return newHeap;
}

void
vmwareHeap_Destroy(Heap* heap)
{
    free(heap);
}

void
vmwareHeap_Clear(Heap* heap)
{
    if (!heap->clear) {
        memset(heap->slotsStart, 0, heap->maxSlots * sizeof (SVGASurface));
        heap->clear = TRUE;
    }
}

static SVGASurface*
FillInSurface(Heap* heap, SVGASurface* surface, CARD32 width, CARD32 height,
              CARD32 bpp, CARD32 pitch, CARD32 size, CARD32 offset)
{
    surface->size = sizeof (SVGASurface);
    surface->version = SVGA_SURFACE_VERSION_1;
    surface->bpp = bpp;
    surface->width = width;
    surface->height = height;
    surface->pitch = pitch;
    if (surface->userData == 0) {
        /*
         * We allocate exactly what we need the first time we use a slot, so
         * all reuses of this slot will be equal or smaller.
         */
        surface->userData = size;
    }
    surface->dataOffset = offset + heap->startOffset;

    return surface;
}

SVGASurface*
vmwareHeap_GetFrontBuffer(Heap* heap)
{
   return heap->frontBuffer;
}

SVGASurface*
vmwareHeap_AllocSurface(Heap* heap, CARD32 width, CARD32 height,
                  CARD32 pitch, CARD32 bpp)
{
    CARD32 size = pitch * height;
    CARD32 sizeUsed = 0;
    SVGASurface* surface = heap->slotsStart;
    int i;

    /*
     * NOTE: we use SVGASurface::userData to store the largest this slot's
     * size has ever been, since we don't ever compact anything.
     */

    /* find a free slot that's big enough */
    for (i = 0; i < heap->maxSlots; i++) {
        if (surface[i].userData == 0) { /* this surface has never been used */
            if ((CARD8*)heap->slotsStart - heap->ptr - sizeUsed < size) {
                /* no room left for data*/
                return NULL;
            }

            heap->clear = FALSE;
            return FillInSurface(heap, surface + i, width, height, bpp,
                                 pitch, size, sizeUsed);
        }

        if (surface[i].numQueued == surface[i].numDequeued &&
            surface[i].userData >= size) { /* free and big enough, sweet! */
            heap->clear = FALSE;
            return FillInSurface(heap, surface + i, width, height, bpp,
                                 pitch, size, sizeUsed);
        }

        sizeUsed += surface[i].userData;
    }

    return NULL;   
}
