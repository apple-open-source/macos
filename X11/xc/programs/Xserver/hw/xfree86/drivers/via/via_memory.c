/*
 * Copyright 2003 Red Hat, Inc. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sub license,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * VIA, S3 GRAPHICS, AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/via/via_memory.c,v 1.5 2004/01/29 03:13:25 dawes Exp $ */

#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86_ansic.h"
#include "xf86fbman.h"

#include "via_compose.h"
#include "via_capture.h"
#include "via.h"
#include "ddmpeg.h"
#include "xf86drm.h"

#include "via_overlay.h"
#include "via_driver.h"
#include "via_regrec.h"
#include "via_priv.h"
#include "via_swov.h"
#include "via_common.h"


/*
 *	Isolate the wonders of X memory allocation, DRI memory allocation
 *	and 4.3 or 4.4 differences in once abstraction
 *
 *	The pool code indicates who provided the memory
 *	0	-	nobody
 *	1	-	xf86 linear (Used in 4.4 only)
 *	2	-	DRM
 *	3	-	Preallocated buffer (Used in 4.3 only)
 */
 
void VIAFreeLinear(VIAMemPtr mem)
{
	VIAPtr pVia;
	DEBUG(ErrorF("Freed %lu (pool %d)\n", mem->base, mem->pool));
	switch(mem->pool)
	{
		case 0:
			return;
		case 1:
			xf86FreeOffscreenLinear(mem->linear);
			mem->linear = NULL;
			mem->pool = 0;
			return;
		case 2:
#ifdef XF86DRI
			if(drmCommandWrite(mem->drm_fd, DRM_VIA_FREEMEM,
					&mem->drm, sizeof(drmViaMem)) < 0)
				ErrorF("DRM module failed free.\n");
			/* Don't close the global drmFD on each memory free! */
			/* drmClose(mem->drm_fd); */
#endif
			mem->pool = 0;
			return;
		case 3:
			mem->pool = 0;
			pVia = mem->pVia;
			pVia->SWOVUsed[mem->slot] = 0;
			return;
	}
}

unsigned long VIAAllocLinear(VIAMemPtr mem, ScrnInfoPtr pScrn, unsigned long size)
{
#if defined(XF86DRI) || !defined(XFREE86_44)
	VIAPtr  pVia = VIAPTR(pScrn);
#endif
	
#ifdef XF86DRI
	if(mem->pool)
		ErrorF("VIA Double Alloc.\n");
		
	if(pVia->graphicInfo.DRMEnabled)
	{
		mem->drm_fd = pVia->drmFD;
		mem->drm.context = 1;
		mem->drm.size = size;
		mem->drm.type = VIDEO;
		
		if(drmCommandWrite(mem->drm_fd, DRM_VIA_ALLOCMEM, 
					&mem->drm, sizeof(drmViaMem)) < 0)
		{
			ErrorF("Cannot allocate memory using DRM.\n");
			return BadAccess;
		}
		
		mem->base = mem->drm.offset;
		mem->pool = 2;
		DEBUG(ErrorF("Fulfilled via DRI at %lu\n", mem->base));
		return 0;
	}
#endif

#ifdef XFREE86_44
	{
		int depth = (pScrn->bitsPerPixel + 7) >> 3;
		/* Make sure we don't truncate requested size */
		mem->linear = xf86AllocateOffscreenLinear(pScrn->pScreen, 
			( size + depth - 1 ) / depth,
			32, NULL, NULL, NULL);
		if(mem->linear == NULL)
		{
			ErrorF("Out of memory for surface.\n");
			return BadAlloc;
		}
		mem->base = mem->linear->offset * depth;
		mem->pool = 1;
		DEBUG(ErrorF("Fulfilled via Linear at %lu\n", mem->base));
		return 0;
	}
#else
	{
		int i;
		if(size > pVia->SWOVSize)
			return BadAccess;
		for(i = 0; i < MEM_BLOCKS; i++)
		{
			if(!pVia->SWOVUsed[i])
			{
				pVia->SWOVUsed[i] = 1;
				mem->pool = 3;
				mem->base = pVia->SWOVPool + pVia->SWOVSize * i;
				mem->pVia = pVia;
				mem->slot = i;
				DEBUG(ErrorF("Fulfilled via pool at %lu\n", mem->base));
				return 0;
			}
		}
	}
	ErrorF("Out of pools.\n");
	return BadAlloc;
#endif	
}

void VIAInitPool(VIAPtr pVia, unsigned long offset, unsigned long size)
{
	DEBUG(ErrorF("VIAInitPool %lu bytes at %lu\n", size, offset));
	
	size /= 4;

	DEBUG(ErrorF("VIAInitPool %d pools of %lu bytes\n", MEM_BLOCKS, size));
	pVia->SWOVPool = offset;
	pVia->SWOVSize = size;
}
