/***************************************************************************

Copyright 2001 Intel Corporation.  All Rights Reserved.

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sub license, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice (including the
next paragraph) shall be included in all copies or substantial portions
of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
IN NO EVENT SHALL INTEL, AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR
THE USE OR OTHER DEALINGS IN THE SOFTWARE.

**************************************************************************/

/*************************************************************************
** File libI810XvMC.c
**
** Authors:
**      Matt Sottek <matthew.j.sottek@intel.com>
**      Bob Paauwe  <bob.j.paauwe@intel.com>
**
**
***************************************************************************/
/* $XFree86: xc/lib/XvMC/hw/i810/I810XvMC.c,v 1.11 2002/11/19 09:35:49 alanh Exp $ */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <dirent.h>
#include <string.h>

#include <sys/ioctl.h>
#include <X11/Xlibint.h>
#include <fourcc.h>
#include <Xv.h>
#include <Xvlib.h>
#include <XvMC.h>
#include <XvMClib.h>
#include "I810XvMC.h"

static int error_base;
static int event_base;

/***************************************************************************
// Function: i810_get_free_buffer
// Description: Allocates a free dma page using kernel ioctls, then
//   programs the data into the already allocated dma buffer list.
// Arguments: pI810XvMC private data structure from the current context.
// Notes: We faked the drmMapBufs for the i810's security so now we have
//   to insert an allocated page into the correct spot in the faked
//   list to keep up appearences.
//   Concept for this function was taken from Mesa sources.
// Returns: drmBufPtr containing the information about the allocated page.
***************************************************************************/
drmBufPtr i810_get_free_buffer(i810XvMCContext *pI810XvMC) {
   drmI810DMA dma;
   drmBufPtr buf;

   dma.granted = 0;
   dma.request_size = 4096;
   while(!dma.granted) {
     if(GET_BUFFER(pI810XvMC, dma) || !dma.granted)
       FLUSH(pI810XvMC);
   } /* No DMA granted */

   buf = &(pI810XvMC->dmabufs->list[dma.request_idx]);
   buf->idx = dma.request_idx;
   buf->used = 0;
   buf->total = dma.request_size;
   buf->address = (drmAddress)dma.virtual;
   return buf;
}

/***************************************************************************
// Function: free_privContext
// Description: Free's the private context structure if the reference
//  count is 0.
***************************************************************************/
void i810_free_privContext(i810XvMCContext *pI810XvMC) {

  I810_LOCK(pI810XvMC,DRM_LOCK_QUIESCENT);


  pI810XvMC->ref--;
  if(!pI810XvMC->ref) {
    drmUnmapBufs(pI810XvMC->dmabufs);
    drmUnmap(pI810XvMC->overlay.address,pI810XvMC->overlay.size);
    drmUnmap(pI810XvMC->surfaces.address,pI810XvMC->surfaces.size);
    drmClose(pI810XvMC->fd);

    free(pI810XvMC->dmabufs->list);
    free(pI810XvMC);
  }

  I810_UNLOCK(pI810XvMC);
}


/***************************************************************************
// Function: XvMCCreateContext
// Description: Create a XvMC context for the given surface parameters.
// Arguments:
//   display - Connection to the X server.
//   port - XvPortID to use as avertised by the X connection.
//   surface_type_id - Unique identifier for the Surface type.
//   width - Width of the surfaces.
//   height - Height of the surfaces.
//   flags - one or more of the following
//      XVMC_DIRECT - A direct rendered context is requested.
//
// Notes: surface_type_id and width/height parameters must match those
//        returned by XvMCListSurfaceTypes.
// Returns: Status
***************************************************************************/
Status XvMCCreateContext(Display *display, XvPortID port,
			 int surface_type_id, int width, int height, int flags,
			 XvMCContext *context) {  
  i810XvMCContext *pI810XvMC;
  char busIdString[10];
  int priv_count;
  uint *priv_data;
  uint magic;
  Status ret;
  int major, minor;

  /* Verify Obvious things first */
  if(context == NULL) {
    return XvMCBadContext;
  }

  if(!flags & XVMC_DIRECT) {
    /* Indirect */
    printf("Indirect Rendering not supported!\nUsing Direct.");
  }

  /* Limit use to root for now */
  if(geteuid()) {
    printf("Use of XvMC on i810 is currently limited to root\n");
    return BadAccess;
  }

  /* FIXME: Check $DISPLAY for legal values here */

  context->surface_type_id = surface_type_id;
  context->width = (unsigned short)width;
  context->height = (unsigned short)height;
  context->flags = flags;
  context->port = port;
  /* 
     Width, Height, and flags are checked against surface_type_id
     and port for validity inside the X server, no need to check
     here.
  */

  /* Allocate private Context data */
  context->privData = (void *)malloc(sizeof(i810XvMCContext));
  if(!context->privData) {
    printf("Unable to allocate resources for XvMC context.\n");
    return BadAlloc;
  }
  pI810XvMC = (i810XvMCContext *)context->privData;


  /* Verify the XvMC extension exists */
  if(! XvMCQueryExtension(display, &event_base,
			  &error_base)) {
    printf("XvMC Extension is not available!\n");
    return BadAlloc;
  }
  /* Verify XvMC version */
  ret = XvMCQueryVersion(display, &major, &minor);
  if(ret) {
    printf("XvMCQuery Version Failed, unable to determine protocol version\n");
  }
  /* FIXME: Check Major and Minor here */

  /* Check for drm */
  if(! drmAvailable()) {
    printf("Direct Rendering is not avilable on this system!\n");
    return BadAlloc;
  }

  /* 
     Build the Attribute Atoms, and Initialize the ones that exist
     in Xv.
  */
  pI810XvMC->xv_colorkey = XInternAtom(display,"XV_COLORKEY",0);
  if(!pI810XvMC->xv_colorkey) {
    return XvBadPort;
  }
  ret = XvGetPortAttribute(display,port,pI810XvMC->xv_colorkey,
			   &pI810XvMC->colorkey);
  if(ret) {
    return ret;
  }
  pI810XvMC->xv_brightness = XInternAtom(display,"XV_BRIGHTNESS",0);
  pI810XvMC->xv_saturation = XInternAtom(display,"XV_SATURATION",0);
  pI810XvMC->xv_contrast = XInternAtom(display,"XV_CONTRAST",0);
  pI810XvMC->brightness = 0;
  pI810XvMC->saturation = 0x80;  /* 1.0 in 3.7 format */
  pI810XvMC->contrast = 0x40; /* 1.0 in 3.6 format */

  /* Open DRI Device */
  if((pI810XvMC->fd = drmOpen("i810",NULL)) < 0) {
    printf("DRM Device for i810 could not be opened.\n");
    free(busIdString);
    free(pI810XvMC);
    return BadAccess;
  } /* !pI810XvMC->fd */

  /* Get magic number and put it in privData for passing */
  drmGetMagic(pI810XvMC->fd,&magic);
  context->flags = (unsigned long)magic;

  /*
    Pass control to the X server to create a drmContext for us and
    validate the with/height and flags.
  */
  if((ret = _xvmc_create_context(display, context, &priv_count, &priv_data))) {
    printf("Unable to create XvMC Context.\n");
    return ret;
  }

  /* 
     X server returns a structure like this:
     drmContext
     fbBase
     OverlayOffset
     OverlaySize
     SurfacesOffset
     SurfacesSize
     busIdString = 9 char + 1
  */
  if(priv_count != 9) {
    printf("_xvmc_create_context() returned incorrect data size!\n");
    printf("\tExpected 9, got %d\n",priv_count);
    _xvmc_destroy_context(display, context);
    free(pI810XvMC);
    return BadAlloc;
  }
  pI810XvMC->drmcontext = priv_data[0];
  pI810XvMC->fb_base = priv_data[1];
  pI810XvMC->overlay.offset = priv_data[2] + priv_data[1];
  pI810XvMC->overlay.size = priv_data[3];
  pI810XvMC->surfaces.offset = priv_data[4] + priv_data[1];
  pI810XvMC->surfaces.size = priv_data[5];
  strncpy(pI810XvMC->busIdString,(char *)&priv_data[6],9);
  pI810XvMC->busIdString[9] = '\0';

  /* Must free the private data we were passed from X */
  free(priv_data);
  
  /* Initialize private context values */
  pI810XvMC->current = 0;
  pI810XvMC->lock = 0;
  pI810XvMC->last_flip = 0;
  pI810XvMC->dual_prime = 0;

  /* 
     Map dma Buffers: Not really, this would be a drmMapBufs
     but due to the i810 security model we have to just create an
     empty data structure to fake it.
  */
  pI810XvMC->dmabufs = (drmBufMapPtr)malloc(sizeof(drmBufMap));
  if(pI810XvMC->dmabufs == NULL) {
    printf("Dma Bufs could not be mapped.\n");
    _xvmc_destroy_context(display, context);
    free(pI810XvMC);
    return BadAlloc;
  } /* pI810XvMC->dmabufs == NULL */
  memset(pI810XvMC->dmabufs, 0, sizeof(drmBufMap));
  pI810XvMC->dmabufs->list = (drmBufPtr)malloc(sizeof(drmBuf) *
					       I810_DMA_BUF_NR);
  if(pI810XvMC->dmabufs->list == NULL) {
    printf("Dma Bufs could not be mapped.\n");
    _xvmc_destroy_context(display, context);
    free(pI810XvMC);
    return BadAlloc;
  } /* pI810XvMC->dmabufs->list == NULL */
  memset(pI810XvMC->dmabufs->list, 0, sizeof(drmBuf) * I810_DMA_BUF_NR);
  
  /* Map the Overlay memory */
  if(drmMap(pI810XvMC->fd,pI810XvMC->overlay.offset,
	    pI810XvMC->overlay.size,&(pI810XvMC->overlay.address)) < 0) {
    printf("Unable to map Overlay at offset 0x%x and size 0x%x\n",
	   (unsigned int)pI810XvMC->overlay.offset,pI810XvMC->overlay.size);
    _xvmc_destroy_context(display, context);
    free(pI810XvMC->dmabufs->list);
    free(pI810XvMC);
    return BadAlloc;
  } /* drmMap() < 0 */
  
  /* Overlay Regs are offset 1024 into Overlay Map */
  pI810XvMC->oregs = (i810OverlayRec *)
    ((unsigned char *)pI810XvMC->overlay.address + 1024);

  /* Map Surfaces */
  if(drmMap(pI810XvMC->fd,pI810XvMC->surfaces.offset,
	    pI810XvMC->surfaces.size,&(pI810XvMC->surfaces.address)) < 0) {
    printf("Unable to map XvMC Surfaces.\n");
    _xvmc_destroy_context(display, context);
    free(pI810XvMC->dmabufs->list);
    free(pI810XvMC);
    return BadAlloc;
  } /* drmMap() < 0 */

  /*
    There is a tiny chance that someone was using the overlay and
    issued a flip that hasn't finished. To be 100% sure I'll just
    take the lock and sleep for the worst case time for a flip.
  */
  I810_LOCK(pI810XvMC,DRM_LOCK_QUIESCENT);
  usleep(20000);  /* 1/50th Sec for 50hz refresh */

  /* Set up Overlay regs with Initial Values */
  pI810XvMC->oregs->YRGB_VPH = 0;
  pI810XvMC->oregs->UV_VPH = 0;
  pI810XvMC->oregs->HORZ_PH = 0;
  pI810XvMC->oregs->INIT_PH = 0;
  pI810XvMC->oregs->DWINPOS = 0;
  pI810XvMC->oregs->DWINSZ = (I810_XVMC_MAXHEIGHT << 16) |
    I810_XVMC_MAXWIDTH;
  pI810XvMC->oregs->SWID =  I810_XVMC_MAXWIDTH | (I810_XVMC_MAXWIDTH << 15);
  pI810XvMC->oregs->SWIDQW = (I810_XVMC_MAXWIDTH >> 3) |
    (I810_XVMC_MAXWIDTH << 12);
  pI810XvMC->oregs->SHEIGHT = I810_XVMC_MAXHEIGHT |
    (I810_XVMC_MAXHEIGHT << 15);
  pI810XvMC->oregs->YRGBSCALE = 0x80004000; /* scale factor 1 */
  pI810XvMC->oregs->UVSCALE = 0x80004000;   /* scale factor 1 */
  pI810XvMC->oregs->OV0CLRC0 = 0x4000;      /* brightness: 0 contrast: 1.0 */
  pI810XvMC->oregs->OV0CLRC1 = 0x80;        /* saturation: bypass */
  
  /* Destination Colorkey Setup */
  pI810XvMC->oregs->DCLRKV = RGB16ToColorKey(pI810XvMC->colorkey);
  pI810XvMC->oregs->DCLRKM = 0x80070307;

  
  pI810XvMC->oregs->SCLRKVH = 0;
  pI810XvMC->oregs->SCLRKVL = 0;
  pI810XvMC->oregs->SCLRKM = 0; /* source color key disable */
  pI810XvMC->oregs->OV0CONF = 0; /* two 720 pixel line buffers */
  
  pI810XvMC->oregs->OV0CMD = VC_UP_INTERPOLATION | HC_UP_INTERPOLATION | 
    Y_ADJUST | YUV_420;

  pI810XvMC->ref = 1;

  I810_UNLOCK(pI810XvMC);

  return Success;

}

/***************************************************************************
// Function: XvMCDestroyContext
// Description: Destorys the specified context.
//
// Arguments:
//   display - Specifies the connection to the server.
//   context - The context to be destroyed.
//
// Returns: Status
***************************************************************************/
Status XvMCDestroyContext(Display *display, XvMCContext *context) {
  i810XvMCContext *pI810XvMC;

  if(context == NULL) {
    return (error_base + XvMCBadContext);
  }
  if(context->privData == NULL) {
    return (error_base + XvMCBadContext);
  }
  pI810XvMC = (i810XvMCContext *)context->privData;

  /* Turn off the overlay */
  if(pI810XvMC->last_flip) {
    I810_LOCK(pI810XvMC,DRM_LOCK_QUIESCENT);

    /* Make sure last flip is done */
    BLOCK_OVERLAY(pI810XvMC,pI810XvMC->current);

    pI810XvMC->oregs->OV0CMD = VC_UP_INTERPOLATION | HC_UP_INTERPOLATION |
      Y_ADJUST;
    pI810XvMC->current = !pI810XvMC->current;
    if(pI810XvMC->current == 1) {
      pI810XvMC->oregs->OV0CMD |= BUFFER1_FIELD0;
    }
    else {
      pI810XvMC->oregs->OV0CMD |= BUFFER0_FIELD0;
    }
    OVERLAY_FLIP(pI810XvMC);
    pI810XvMC->last_flip++;
  
    /* Wait for the flip */
    BLOCK_OVERLAY(pI810XvMC,pI810XvMC->current);

    I810_UNLOCK(pI810XvMC);
  }

  /* Pass Control to the X server to destroy the drmContext */
  _xvmc_destroy_context(display, context);

  i810_free_privContext(pI810XvMC);
  context->privData = NULL;

  return Success;
}


/***************************************************************************
// Function: XvMCCreateSurface
***************************************************************************/
Status XvMCCreateSurface( Display *display, XvMCContext *context,
			  XvMCSurface *surface) {
  i810XvMCContext *pI810XvMC;
  i810XvMCSurface *pI810Surface;
  int priv_count;
  uint *priv_data;
  Status ret;

  if((surface == NULL) || (context == NULL) || (display == NULL)){
    return BadValue;
  }
  
  pI810XvMC = (i810XvMCContext *)context->privData;
  if(pI810XvMC == NULL) {
    return (error_base + XvMCBadContext);
  }


  surface->privData = (i810XvMCSurface *)malloc(sizeof(i810XvMCSurface));
  if(!surface->privData) {
    return BadAlloc;
  }
  pI810Surface = (i810XvMCSurface *)surface->privData;

  /* Initialize private values */
  pI810Surface->privContext = pI810XvMC;
  pI810Surface->last_render = 0;
  pI810Surface->last_flip = 0;
  pI810Surface->second_field = 0;

  if((ret = _xvmc_create_surface(display, context, surface,
				&priv_count, &priv_data))) {
    free(pI810Surface);
    printf("Unable to create XvMCSurface.\n");
    return ret;
  }

  /*
    _xvmc_create_subpicture returns 2 uints with the offset into
    the DRM map for the Y surface and UV surface.
  */
  if(priv_count != 2) {
    printf("_xvmc_create_surface() return incorrect data size.\n");
    printf("Expected 2 got %d\n",priv_count);
    free(priv_data);
    free(pI810Surface);
    return BadAlloc;
  }
  /* Data == Client Address, offset == Physical address offset */
  pI810Surface->data = pI810XvMC->surfaces.address;
  pI810Surface->offset = pI810XvMC->surfaces.offset;


  /* 
     i810's MC Engine needs surfaces of 2^x (x= 9,10,11,12) pitch
     and the Tiler need 512k aligned surfaces, basically we are
     stuck with fixed memory with pitch 1024 for Y data. UV = 512.
  */
  pI810Surface->pitch = 10;
  if((surface->surface_type_id == FOURCC_UYVY) ||
     (surface->surface_type_id == FOURCC_YUY2)) {
    /* This is not implemented server side. */
    pI810Surface->pitch++;
  }

  /*
    offsets[0,1,2] == Offsets from either data or offset for the Y
    U and V surfaces.
  */
  pI810Surface->offsets[0] = priv_data[0];
  if(((unsigned long)pI810Surface->data + pI810Surface->offsets[0]) & 4095) {
    printf("XvMCCreateSurface: Surface offset 0 is not 4096 aligned\n");
  }

  if((surface->surface_type_id == FOURCC_UYVY) ||
     (surface->surface_type_id == FOURCC_YUY2)) {
    /* Packed surface, not fully implemented */
    pI810Surface->offsets[1] = 0;
    pI810Surface->offsets[2] = 0;
  }
  else {
    /* Planar surface */
    pI810Surface->offsets[1] = priv_data[1];
    if(((unsigned long)pI810Surface->data + pI810Surface->offsets[1]) & 2047) {
      printf("XvMCCreateSurface: Surface offset 1 is not 2048 aligned\n");
    }

    pI810Surface->offsets[2] = ((unsigned long)pI810Surface->offsets[1] +
				(1<<(pI810Surface->pitch - 1)) * 288);
    if(((unsigned long)pI810Surface->data + pI810Surface->offsets[2]) & 2047) {
      printf("XvMCCreateSurface: Surface offset 2 is not 2048 aligned\n");
    }

  }

  /* Free data returned from xvmc_create_surface */
  free(priv_data);

  /* Clear the surface to 0 */
  memset((void *)((unsigned long)pI810Surface->data + (unsigned long)pI810Surface->offsets[0]),
	 0, ((1<<pI810Surface->pitch) * surface->height));

  switch(surface->surface_type_id) {
  case FOURCC_YV12:
  case FOURCC_I420:
    /* Destination buffer info command */
    pI810Surface->dbi1y = ((((unsigned int)pI810Surface->offset +
			     pI810Surface->offsets[0]) & ~0xfc000fff) |
			   (pI810Surface->pitch - 9));
    pI810Surface->dbi1u = ((((unsigned int)pI810Surface->offset +
			     pI810Surface->offsets[1]) & ~0xfc000fff) |
			   (pI810Surface->pitch - 10));
    pI810Surface->dbi1v = ((((unsigned int)pI810Surface->offset +
			     pI810Surface->offsets[2]) & ~0xfc000fff) |
			   (pI810Surface->pitch - 10));

    /* Destination buffer variables command */
    pI810Surface->dbv1 = (0x8<<20) | (0x8<<16);
    /* Map info command */
    pI810Surface->mi1y = (0x1<<24) | (1<<9) | (pI810Surface->pitch - 3);
    pI810Surface->mi1u = (0x1<<24) | (1<<9) | (pI810Surface->pitch - 4);
    pI810Surface->mi1v = (0x1<<24) | (1<<9) | (pI810Surface->pitch - 4);

    pI810Surface->mi2y = (((unsigned int)surface->height - 1)<<16) |
      ((unsigned int)surface->width - 1);
    pI810Surface->mi2u = (((unsigned int)surface->height - 1)<<15) |
      (((unsigned int)surface->width - 1)>>1);
    pI810Surface->mi2v = pI810Surface->mi2u;

    pI810Surface->mi3y = ((unsigned int)pI810Surface->offset +
			  pI810Surface->offsets[0]) & ~0x0000000f;
    pI810Surface->mi3u = ((unsigned int)pI810Surface->offset +
			  pI810Surface->offsets[1]) & ~0x0000000f;
    pI810Surface->mi3v = ((unsigned int)pI810Surface->offset +
			  pI810Surface->offsets[2]) & ~0x0000000f;
    break;
  case FOURCC_UYVY:
  case FOURCC_YUY2:
  default:
    /* Destination buffer info command */
    pI810Surface->dbi1y = ((((unsigned int)pI810Surface->offset +
			     pI810Surface->offsets[0]) & ~0xfc000fff) |
			   (pI810Surface->pitch - 9));
    /* Destination buffer variables command */
    if(surface->surface_type_id == FOURCC_YUY2) {
      pI810Surface->dbv1 = 0x5<<8;
      pI810Surface->mi1y = 0x5<<24 | pI810Surface->pitch | 0x1<<21;
    }
    else {
      pI810Surface->dbv1 = 0x4<<8;
      pI810Surface->mi1y = 0x5<<24 | (pI810Surface->pitch - 3);
    }
    pI810Surface->mi2y = (((unsigned int)surface->width - 1)<<16) |
      ((unsigned int)surface->height - 1);
    pI810Surface->mi3y = ((unsigned int)pI810Surface->offset +
			  pI810Surface->offsets[0]) & ~0xfc000fff;
    break;
  }
  pI810XvMC->ref++;

  return Success;
}


/***************************************************************************
// Function: XvMCDestroySurface
***************************************************************************/
Status XvMCDestroySurface(Display *display, XvMCSurface *surface) {
  i810XvMCSurface *pI810Surface;
  i810XvMCContext *pI810XvMC;

  if((display == NULL) || (surface == NULL)) {
    return BadValue;
  }
  if(surface->privData == NULL) {
    return (error_base + XvMCBadSurface);
  }

  pI810Surface = (i810XvMCSurface *)surface->privData;
  if(pI810Surface->last_flip) {
    XvMCSyncSurface(display,surface);
  }
  pI810XvMC = (i810XvMCContext *)pI810Surface->privContext;

  _xvmc_destroy_surface(display,surface);

  i810_free_privContext(pI810XvMC);

  free(pI810Surface);
  surface->privData = NULL;
  return Success;
}

/***************************************************************************
// Function: XvMCCreateBlocks
***************************************************************************/
Status XvMCCreateBlocks(Display *display, XvMCContext *context,
			unsigned int num_blocks,
			XvMCBlockArray *block) {

  if((display == NULL) || (context == NULL) || (num_blocks == 0)) {
    return BadValue;
  }

  block->blocks = (short *)malloc(num_blocks<<6 * sizeof(short));
  if(block->blocks == NULL) {
    return BadAlloc;
  }

  block->num_blocks = num_blocks;
  block->context_id = context->context_id;

  block->privData = NULL;

  return Success;
}

/***************************************************************************
// Function: XvMCDestroyBlocks
***************************************************************************/
Status XvMCDestroyBlocks(Display *display, XvMCBlockArray *block) {
  if(display == NULL) {
    return BadValue;
  }

  free(block->blocks);
  block->num_blocks = 0;
  block->context_id = 0;
  block->privData = NULL;
  return Success;
}

/***************************************************************************
// Function: XvMCCreateMacroBlocks
***************************************************************************/
Status XvMCCreateMacroBlocks(Display *display, XvMCContext *context,
			     unsigned int num_blocks,
			     XvMCMacroBlockArray *blocks) {

  if((display == NULL) || (context == NULL) || (blocks == NULL) ||
      (num_blocks == 0)) {
    return BadValue;
  }
  memset(blocks,0,sizeof(XvMCMacroBlockArray));
  blocks->context_id = context->context_id;
  blocks->privData = NULL;
  
  blocks->macro_blocks = (XvMCMacroBlock *)
    malloc(num_blocks * sizeof(XvMCMacroBlock));
  if(blocks->macro_blocks == NULL) {
    return BadAlloc;
  }
  blocks->num_blocks = num_blocks;

  return Success;
}

/***************************************************************************
// Function: XvMCDestroyMacroBlocks
***************************************************************************/
Status XvMCDestroyMacroBlocks(Display *display, XvMCMacroBlockArray *block) {
  if((display == NULL) || (block == NULL)) {
    return BadValue;
  }
  if(block->macro_blocks) {
    free(block->macro_blocks);
  }
  block->context_id = 0;
  block->num_blocks = 0;
  block->privData = NULL;

  return Success;
}


/***************************************************************************
// Function: dp (Debug Print)
// Description: This function prints out in hex i * uint32_t at the address
//  supplied. This enables you to print out the dma buffers from
//  within the debugger even though they are not in your address space.
***************************************************************************/
void dp(unsigned int *address, unsigned int i) {
  int j;

  printf("DebugPrint:\n");
  for(j=0; j<i; j++) {
    printf("0x%8.8x ",address[j]);
    if(j && !(j & 7)) { printf("\n");}
  }
}

/***************************************************************************
// Macro: PACK_*
// Description: Packs 16bit signed data from blocks into either 8bit unsigned
//  intra data or 16bit signed correction data, both packed into
//  32 bit integers.
***************************************************************************/
#define PACK_INTRA_DATA(d,b,n)                  \
  do {                                          \
   char *dp = (char *)d;                        \
   char *bp = (char *)b;                        \
   int counter;                                 \
   for(counter = 0; counter < n; counter++) {   \
     *dp++ = *bp;                               \
     bp += 2;                                   \
   }                                            \
  }while(0);

#define PACK_CORR_DATA(d,b,n)          \
            memcpy(d,b,n);             \
            d = (uint *)((unsigned long)d + n);

#define MARK_CORR_DATA(d,n)                          \
            do {                                     \
              uint* q = (uint*)((unsigned long)d - n);        \
              while((unsigned long)q < (unsigned long)d) {             \
               *q++ += 0x00330033;                   \
              }                                      \
	    }while(0);

#define MARK_INTRA_BLOCK(d)         \
           do {                     \
             int q;                 \
             for(q=0; q<16; q++) {  \
               d[q] += 0x33333333;  \
             }                      \
	    }while(0);

/*
  Used for DCT 1 when we need DCT 0. Instead
  of reading from one block we read from two and
  interlace.
*/
#define PACK_CORR_DATA_1to0(d,top,bottom)            \
            do {                                     \
              short *t = top,*b = bottom;            \
              PACK_CORR_DATA(d,t,16);                \
              t = (short *)((unsigned long)t + 16);           \
              PACK_CORR_DATA(d,b,16);                \
              b = (short *)((unsigned long)b + 16);           \
              PACK_CORR_DATA(d,t,16);                \
              t = (short *)((unsigned long)t + 16);           \
              PACK_CORR_DATA(d,b,16);                \
              b = (short *)((unsigned long)b + 16);           \
              PACK_CORR_DATA(d,t,16);                \
              t = (short *)((unsigned long)t + 16);           \
              PACK_CORR_DATA(d,b,16);                \
              b = (short *)((unsigned long)b + 16);           \
              PACK_CORR_DATA(d,t,16);                \
              t = (short *)((unsigned long)t + 16);           \
              PACK_CORR_DATA(d,b,16);                \
              b = (short *)((unsigned long)b + 16);           \
            }while(0);

/* Used for DCT 0 when we need DCT 1. */
#define PACK_CORR_DATA_0to1(d,top,bottom)            \
            do{                                      \
              short *t = top,*b = bottom;            \
              PACK_CORR_DATA(d,t,16);                \
              t = (short *)((unsigned long)t + 32);           \
              PACK_CORR_DATA(d,t,16);                \
              t = (short *)((unsigned long)t + 32);           \
              PACK_CORR_DATA(d,t,16);                \
              t = (short *)((unsigned long)t + 32);           \
              PACK_CORR_DATA(d,t,16);                \
              t = (short *)((unsigned long)t + 32);           \
              PACK_CORR_DATA(d,b,16);                \
              b = (short *)((unsigned long)b + 32);           \
              PACK_CORR_DATA(d,b,16);                \
              b = (short *)((unsigned long)b + 32);           \
              PACK_CORR_DATA(d,b,16);                \
              b = (short *)((unsigned long)b + 32);           \
              PACK_CORR_DATA(d,b,16);                \
              b = (short *)((unsigned long)b + 32);           \
            }while(0);

#define PACK_CORR_DATA_SHORT(d,block)                \
            do {                                     \
              short *b = block;                      \
              PACK_CORR_DATA(d,b,16);                \
              b = (short *)((unsigned long)b + 32);           \
              PACK_CORR_DATA(d,b,16);                \
              b = (short *)((unsigned long)b + 32);           \
              PACK_CORR_DATA(d,b,16);                \
              b = (short *)((unsigned long)b + 32);           \
              PACK_CORR_DATA(d,b,16);                \
              b = (short *)((unsigned long)b + 32);           \
            }while(0);

/* Lookup tables to speed common calculations */
static unsigned int drps_table[] = {2<<6,3<<6};

static unsigned int mvfs_table[] = {
  0x12,
  0x1a,
  0x13,
  0x1b
};

static unsigned int type_table[] = {
  0x1<<12,   /* This is an error so make it Forward motion */
  0x1<<12,
  0x1<<12,
  0x1<<12,
  0x2<<12,
  0x2<<12,
  0x3<<12,
  0x3<<12,
  0x1<<12,  /* Pattern but no Motion, Make motion Forward */
  0x1<<12,
  0x1<<12,
  0x1<<12,
  0x2<<12,
  0x2<<12,
  0x3<<12,
  0x3<<12
};

static unsigned int y_frame_bytes[] = {
  0,0,0,0,128,128,128,128,
  128,128,128,128,256,256,256,256,
  128,128,128,128,256,256,256,256,
  256,256,256,256,384,384,384,384,
  128,128,128,128,256,256,256,256,
  256,256,256,256,384,384,384,384,
  256,256,256,256,384,384,384,384,
  384,384,384,384,512,512,512,512
};

static unsigned int u_frame_bytes[] = {
  0,0,128,128,0,0,128,128,
  0,0,128,128,0,0,128,128,
  0,0,128,128,0,0,128,128,
  0,0,128,128,0,0,128,128,
  0,0,128,128,0,0,128,128,
  0,0,128,128,0,0,128,128,
  0,0,128,128,0,0,128,128,
  0,0,128,128,0,0,128,128
};

static unsigned int v_frame_bytes[] = {
  0,128,0,128,0,128,0,128,
  0,128,0,128,0,128,0,128,
  0,128,0,128,0,128,0,128,
  0,128,0,128,0,128,0,128,
  0,128,0,128,0,128,0,128,
  0,128,0,128,0,128,0,128,
  0,128,0,128,0,128,0,128,
  0,128,0,128,0,128,0,128
};

static unsigned int y_first_field_bytes[] = {
  0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,
  128,128,128,128,128,128,128,128,
  128,128,128,128,128,128,128,128,
  128,128,128,128,128,128,128,128,
  128,128,128,128,128,128,128,128,
  256,256,256,256,256,256,256,256,
  256,256,256,256,256,256,256,256
};

static unsigned int y_second_field_bytes[] = {
  0,0,0,0,128,128,128,128,
  128,128,128,128,256,256,256,256,
  0,0,0,0,128,128,128,128,
  128,128,128,128,256,256,256,256,
  0,0,0,0,128,128,128,128,
  128,128,128,128,256,256,256,256,
  0,0,0,0,128,128,128,128,
  128,128,128,128,256,256,256,256
};

static unsigned int y_dct0_field_bytes[] = {
  0,0,0,0,128,128,128,128,
  128,128,128,128,256,256,256,256,
  128,128,128,128,128,128,128,128,
  256,256,256,256,256,256,256,256,
  128,128,128,128,256,256,256,256,
  128,128,128,128,256,256,256,256,
  256,256,256,256,256,256,256,256,
  256,256,256,256,256,256,256,256
};

static unsigned int y_dct1_frame_bytes[] = {
  0,0,0,0,256,256,256,256,
  256,256,256,256,512,512,512,512,
  256,256,256,256,256,256,256,256,
  512,512,512,512,512,512,512,512,
  256,256,256,256,512,512,512,512,
  256,256,256,256,512,512,512,512,
  512,512,512,512,512,512,512,512,
  512,512,512,512,512,512,512,512
};

static unsigned int u_field_bytes[] = {
  0,0,64,64,0,0,64,64,
  0,0,64,64,0,0,64,64,
  0,0,64,64,0,0,64,64,
  0,0,64,64,0,0,64,64,
  0,0,64,64,0,0,64,64,
  0,0,64,64,0,0,64,64,
  0,0,64,64,0,0,64,64,
  0,0,64,64,0,0,64,64
};

static unsigned int v_field_bytes[] = {
  0,64,0,64,0,64,0,64,
  0,64,0,64,0,64,0,64,
  0,64,0,64,0,64,0,64,
  0,64,0,64,0,64,0,64,
  0,64,0,64,0,64,0,64,
  0,64,0,64,0,64,0,64,
  0,64,0,64,0,64,0,64,
  0,64,0,64,0,64,0,64
};

static short empty_block[] = {
  0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0
};


/***************************************************************************
// Function: dispatchYContext
// Description: Allocate a DMA buffer write the Y MC Context info in it,
//  and dispatch it to hardware.
***************************************************************************/

static __inline__ void dispatchYContext(i810XvMCSurface *privTarget,
					i810XvMCSurface *privPast,
					i810XvMCSurface *privFuture,
					i810XvMCContext *pI810XvMC) {
  uint *data;
  drmBufPtr pDMA;
  drm_i810_mc_t mc;

  pDMA = i810_get_free_buffer(pI810XvMC);
  data = pDMA->address;
  *data++ = CMD_FLUSH;
  *data++ = BOOLEAN_ENA_2;
  *data++ = CMD_FLUSH;
  *data++ = DEST_BUFFER_INFO;
  *data++ = privTarget->dbi1y;
  *data++ = DEST_BUFFER_VAR;
  *data++ = privTarget->dbv1;
  /* Past Surface */
  *data++ = CMD_MAP_INFO;
  *data++ = privPast->mi1y;
  *data++ = privPast->mi2y;
  *data++ = privPast->mi3y;
  /* Future Surface */
  *data++ = CMD_MAP_INFO;
  *data++ = privFuture->mi1y | 0x1<<28;
  *data++ = privFuture->mi2y;
  *data++ = privFuture->mi3y;

  mc.idx = pDMA->idx;
  mc.used = (unsigned long)data - (unsigned long)pDMA->address;
  mc.last_render = ++pI810XvMC->last_render;
  privTarget->last_render = pI810XvMC->last_render;
  I810_MC(pI810XvMC,mc);
}

static __inline__ void renderError(void) {
  printf("Invalid Macroblock Parameters found.\n");
  return;
}

/***************************************************************************
// Function: renderIntrainFrame
// Description: inline function that sets hardware parameters for an Intra
//  encoded macroblock in a Frame picture.
***************************************************************************/
static __inline__ void renderIntrainFrame(uint **datay,uint **datau,
					  uint **datav,
					  XvMCMacroBlock *mb,
					  short *block_ptr) {

  register uint *dy = *datay;
  register uint *du = *datau;
  register uint *dv = *datav;

  /* Y Blocks */
  *dy++ = GFXBLOCK + 68;
  *dy++ = (1<<30) | (3<<28) | (0xf<<24);
  *dy++ = ((uint)mb->x<<20) | ((uint)mb->y<<4);
  *dy++ = (16<<16) | 16;
  *dy++ = 0;
  *dy++ = 0;
  PACK_INTRA_DATA(dy,block_ptr,256);
  dy += 64;
  block_ptr += 256;
  /* End Y Blocks */

  /* U Block */
  *du++ = GFXBLOCK + 20;
  *du++ = (2<<30) | (1<<28) | (1<<23);
  *du++ = (((uint)mb->x)<<19) | (((uint)mb->y)<<3);
  *du++ = (8<<16) | 8;
  *du++ = 0;
  *du++ = 0;
  PACK_INTRA_DATA(du,block_ptr,64);
  du += 16;
  block_ptr += 64;
  
  /* V Block */
  *dv++ = GFXBLOCK + 20;
  *dv++ = (3<<30) | (1<<28) | (1<<22);
  *dv++ = (((uint)mb->x)<<19) | (((uint)mb->y)<<3);
  *dv++ = (8<<16) | 8;
  *dv++ = 0;
  *dv++ = 0;
  PACK_INTRA_DATA(dv,block_ptr,64);
  dv += 16;
  block_ptr += 64;

  *datay = dy;
  *datau = du;
  *datav = dv;
}

/***************************************************************************
// Function: renderIntrainFrameDCT1
// Description: inline function that sets hardware parameters for an Intra
//  encoded macroblock in a Frame picture with DCT type 1.
***************************************************************************/
static __inline__ void renderIntrainFrameDCT1(uint **datay,uint **datau,
					      uint **datav,XvMCMacroBlock *mb,
					      short *block_ptr,uint flags) {

  register uint *dy = *datay;
  register uint *du = *datau;
  register uint *dv = *datav;


  /* Y Blocks */
  *dy++ = GFXBLOCK + 36;
  *dy++ = (1<<30) | (2<<28) | (0x3<<26) | (0x2<<6);
  *dy++ = ((uint)mb->x<<20) | ((uint)mb->y<<3);
  *dy++ = (8<<16) | 16;
  *dy++ = 0;
  *dy++ = 0;
  PACK_INTRA_DATA(dy,block_ptr,128);
  dy += 32;
  block_ptr += 128;

  /* Second Y block */
  *dy++ = GFXBLOCK + 36;
  *dy++ = (1<<30) | (2<<28) | (0x3<<26) | (0x3<<6);
  *dy++ = ((uint)mb->x<<20) | ((uint)mb->y<<3);
  *dy++ = (8<<16) | 16;
  *dy++ = 0;
  *dy++ = 0;
  PACK_INTRA_DATA(dy,block_ptr,128);
  dy += 32;
  block_ptr += 128;
  /* End Y Blocks */

    
  /* U Block */
  *du++ = GFXBLOCK + 20;
  *du++ = (2<<30) | (1<<28) | (1<<23);
  *du++ = (((uint)mb->x)<<19) | (((uint)mb->y)<<3);
  *du++ = (8<<16) | 8;
  *du++ = 0;
  *du++ = 0;
  PACK_INTRA_DATA(du,block_ptr,64);
  du += 16;
  block_ptr += 64;
  
  /* V Block */
  *dv++ = GFXBLOCK + 20;
  *dv++ = (3<<30) | (1<<28) | (1<<22);
  *dv++ = (((uint)mb->x)<<19) | (((uint)mb->y)<<3);
  *dv++ = (8<<16) | 8;
  *dv++ = 0;
  *dv++ = 0;
  PACK_INTRA_DATA(dv,block_ptr,64);
  dv += 16;
  block_ptr += 64;

  *datay = dy;
  *datau = du;
  *datav = dv;
}


/***************************************************************************
// Function: renderIntrainField
// Description: inline function that sets hardware parameters for an Intra
//  encoded macroblock in Field pictures.
***************************************************************************/
static __inline__ void renderIntrainField(uint **datay,uint **datau,
					  uint **datav,
					  XvMCMacroBlock *mb,short *block_ptr,
					  uint ps) {

  register uint *dy = *datay;
  register uint *du = *datau;
  register uint *dv = *datav;

  uint xy = ((uint)mb->x<<20) | ((uint)mb->y<<4);
  uint dw1 = drps_table[~ps & 0x1];

  /* Y Blocks */
  *dy++ = GFXBLOCK + 68;
  *dy++ = (1<<30) | (3<<28) | (0xf<<24) | dw1;
  *dy++ = xy;
  *dy++ = (16<<16) | 16;
  *dy++ = 0;
  *dy++ = 0;
  PACK_INTRA_DATA(dy,block_ptr,256);
  dy += 64;
  block_ptr += 256;
  /* End Y Blocks */

  xy >>= 1;

  /* U Block */
  *du++ = GFXBLOCK + 20;
  *du++ = (2<<30) | (1<<28) | (1<<23) | dw1;
  *du++ = xy;
  *du++ = (8<<16) | 8;
  *du++ = 0;
  *du++ = 0;
  PACK_INTRA_DATA(du,block_ptr,64);
  du += 16;
  block_ptr += 64;
  
  /* V Block */
  *dv++ = GFXBLOCK + 20;
  *dv++ = (3<<30) | (1<<28) | (1<<22) | dw1;
  *dv++ = xy;
  *dv++ = (8<<16) | 8;
  *dv++ = 0;
  *dv++ = 0;
  PACK_INTRA_DATA(dv,block_ptr,64);
  dv += 16;
  block_ptr += 64;

  *datay = dy;
  *datau = du;
  *datav = dv;
}


/***************************************************************************
// Function: renderFieldinField
// Description: inline function that sets hardware parameters for a Field
//  encoded macroblock in a Field Picture.
***************************************************************************/
static __inline__ void renderFieldinField(uint **datay,uint **datau,
					  uint **datav,
					  XvMCMacroBlock *mb,short *block_ptr,
					  uint ps, uint flags) {

  register uint *dy = *datay;
  register uint *du = *datau;
  register uint *dv = *datav;

  /* Motion Vectors */
  short fmv[2];
  short bmv[2];
  /* gfxblock dword 1 */
  uint dw1;

  uint parity = ~ps & XVMC_TOP_FIELD;

  uint ysize = y_frame_bytes[mb->coded_block_pattern];
  uint usize = u_frame_bytes[mb->coded_block_pattern];
  uint vsize = v_frame_bytes[mb->coded_block_pattern];

  uint xy = ((uint)mb->x<<20) | ((uint)mb->y<<4);

  /* i810 Specific flag used to identify the second field in a P frame */
  if(flags & 0x80000000) {
    /* P Frame */
    if((mb->motion_vertical_field_select & XVMC_SELECT_FIRST_FORWARD) ==
      parity) {
      /* Same parity, use reference field (map0) */
      dw1 = 1<<12 | ((0x2 + parity)<<6) | ((0x2 + parity)<<3) |
	(((uint)mb->coded_block_pattern)<<22);
      fmv[0] = mb->PMV[0][0][1];
      fmv[1] = mb->PMV[0][0][0];
      bmv[0] = 0;
      bmv[1] = 0;
    }
    else {
      /*
	Opposite parity, set up as if it were backward
	motion and use map1.
      */
      dw1 = 2<<12 | ((0x2 + parity)<<6) | (0x3 - parity) |
	(((uint)mb->coded_block_pattern)<<22);
      bmv[0] = mb->PMV[0][0][1];
      bmv[1] = mb->PMV[0][0][0];
      fmv[0] = 0;
      fmv[1] = 0;
    }
  }
  else {
    dw1 = type_table[mb->macroblock_type & 0xf] |
      drps_table[~ps & 0x1] |
      mvfs_table[mb->motion_vertical_field_select & 3] |
      (((uint)mb->coded_block_pattern)<<22);
    
    fmv[0] = mb->PMV[0][0][1];
    fmv[1] = mb->PMV[0][0][0];
    
    bmv[0] = mb->PMV[0][1][1];
    bmv[1] = mb->PMV[0][1][0];
  }

  /* Y Block */
  *dy++ = GFXBLOCK + 4 + (ysize>>2);
  *dy++ = (1<<30) | (3<<28) | dw1;
  *dy++ = xy;
  *dy++ = (16<<16) | 16;
  *dy++ = *(uint *)fmv;
  *dy++ = *(uint *)bmv;
  PACK_CORR_DATA(dy,block_ptr,ysize);
  block_ptr = (short *)((unsigned long)block_ptr + ysize);
  /* End Y Blocks */

  fmv[0] /= 2;
  fmv[1] /= 2;
  bmv[0] /= 2;
  bmv[1] /= 2;
  xy >>= 1;

  /* U Block */
  *du++ = GFXBLOCK + 4 + (usize>>2);
  *du++ = (2<<30) | (1<<28) | dw1;
  *du++ = xy;
  *du++ = (8<<16) | 8;
  *du++ = *(uint *)fmv;
  *du++ = *(uint *)bmv;
  PACK_CORR_DATA(du,block_ptr,usize);
  block_ptr = (short *)((unsigned long)block_ptr + usize);

  /* V Block */
  *dv++ = GFXBLOCK + 4 + (vsize>>2);
  *dv++ = (3<<30) | (1<<28) | dw1;
  *dv++ = xy;
  *dv++ = (8<<16) | 8;
  *dv++ = *(uint *)fmv;
  *dv++ = *(uint *)bmv;
  PACK_CORR_DATA(dv,block_ptr,vsize);
  block_ptr = (short *)((unsigned long)block_ptr + vsize);

  *datay = dy;
  *datau = du;
  *datav = dv;
}

/***************************************************************************
// Function: render16x8inField
// Description: inline function that sets hardware parameters for a 16x8
//  encoded macroblock in a field picture.
***************************************************************************/
static __inline__ void render16x8inField(uint **datay,uint **datau,
					 uint **datav,
					 XvMCMacroBlock *mb,short *block_ptr,
					 uint ps, uint flags) {

  register uint *dy = *datay;
  register uint *du = *datau;
  register uint *dv = *datav;

  /* Motion Vectors */
  short fmv[4];
  short bmv[4];
  /* gfxblock dword 1 */
  uint dw1[2];

  uint y1size = y_first_field_bytes[mb->coded_block_pattern];
  uint y2size = y_second_field_bytes[mb->coded_block_pattern];
  uint usize = u_field_bytes[mb->coded_block_pattern];
  uint vsize = v_field_bytes[mb->coded_block_pattern];

  uint parity = ~ps & XVMC_TOP_FIELD;

  uint xy = ((uint)mb->x<<20) | ((uint)mb->y<<4);

  /* i810 Specific flag used to identify the second field in a P frame */
  if(flags & 0x80000000) {
    /* P Frame */
    if((mb->motion_vertical_field_select & XVMC_SELECT_FIRST_FORWARD) ==
      parity) {
      /* Same parity, use reference field (map0) */
      dw1[0] = 1<<12 | ((0x2 + parity)<<6) | ((0x2 + parity)<<3) |
	(((uint)mb->coded_block_pattern)<<22);

      fmv[0] = mb->PMV[0][0][1];
      fmv[1] = mb->PMV[0][0][0];
      bmv[0] = 0;
      bmv[1] = 0;
    }
    else {
      /*
	Opposite parity, set up as if it were backward
	motion and use map1.
      */
      dw1[0] = 2<<12 | ((0x2 + parity)<<6) | (0x3 - parity) |
	(((uint)mb->coded_block_pattern)<<22);

      bmv[0] = mb->PMV[0][0][1];
      bmv[1] = mb->PMV[0][0][0];
      fmv[0] = 0;
      fmv[1] = 0;
    }
    if((mb->motion_vertical_field_select & XVMC_SELECT_SECOND_FORWARD) ==
       (parity<<2)) {
      /* Same parity, use reference field (map0) */
      dw1[1] = 1<<12 | ((0x2 + parity)<<6) | ((0x2 + parity)<<3) |
	((((uint)mb->coded_block_pattern<<22) & (0x3<<22)) |
	 (((uint)mb->coded_block_pattern<<24) & (0x3<<26)));

      fmv[2] = mb->PMV[1][0][1];
      fmv[3] = mb->PMV[1][0][0];
      bmv[2] = 0;
      bmv[3] = 0;
    }
    else {
      /* 
	 Opposite parity, set up as if it were backward
	 motion and use map1.
      */
      dw1[1] = 2<<12 | ((0x2 + parity)<<6) | (0x3 - parity) |
	((((uint)mb->coded_block_pattern<<22) & (0x3<<22)) |
	 (((uint)mb->coded_block_pattern<<24) & (0x3<<26)));

      bmv[2] = mb->PMV[1][0][1];
      bmv[3] = mb->PMV[1][0][0];
      fmv[2] = 0;
      fmv[3] = 0;
    }
  }
  else {
    dw1[0] = type_table[mb->macroblock_type & 0xf] |
      drps_table[~ps & 0x1] |
      mvfs_table[mb->motion_vertical_field_select & 3] |
      (((uint)mb->coded_block_pattern)<<22);

    dw1[1] = type_table[mb->macroblock_type & 0xf] |
      drps_table[~ps & 0x1] |
      mvfs_table[(mb->motion_vertical_field_select>>2) & 0x3] |
      ((((uint)mb->coded_block_pattern<<22) & (0x3<<22)) |
       (((uint)mb->coded_block_pattern<<24) & (0x3<<26)));

    fmv[0] = mb->PMV[0][0][1];
    fmv[1] = mb->PMV[0][0][0];
    fmv[2] = mb->PMV[1][0][1];
    fmv[3] = mb->PMV[1][0][0];
    
    bmv[0] = mb->PMV[0][1][1];
    bmv[1] = mb->PMV[0][1][0];
    bmv[2] = mb->PMV[1][1][1];
    bmv[3] = mb->PMV[1][1][0];
  }

  /* First Y Block */
  *dy++ = GFXBLOCK + 4 + (y1size>>2);
  *dy++ = (1<<30) | (2<<28) | dw1[0];
  *dy++ = xy;
  *dy++ = (8<<16) | 16;
  *dy++ = *(uint *)fmv;
  *dy++ = *(uint *)bmv;
  PACK_CORR_DATA(dy,block_ptr,y1size);
  block_ptr = (short *)((unsigned long)block_ptr + y1size);

  /* Second Y Block */
  *dy++ = GFXBLOCK + 4 + (y2size>>2);
  *dy++ = (1<<30) | (2<<28) | dw1[1];
  *dy++ = (xy + 8);
  *dy++ = (8<<16) | 16;
  *dy++ = *(uint *)&fmv[2];
  *dy++ = *(uint *)&bmv[2];
  PACK_CORR_DATA(dy,block_ptr,y2size);
  block_ptr = (short *)((unsigned long)block_ptr + y2size);
  /* End Y Blocks */

  fmv[0] /= 2;
  fmv[1] /= 2;
  fmv[2] /= 2;
  fmv[3] /= 2;
  
  bmv[0] /= 2;
  bmv[1] /= 2;
  bmv[2] /= 2;
  bmv[3] /= 2;

  xy >>= 1;

  /* U Blocks */
  *du++ = GFXBLOCK + 4 + (usize>>2);
  *du++ = (2<<30) | (1<<28) | dw1[0];
  *du++ = xy;
  *du++ = (4<<16) | 8;
  *du++ = *(uint *)fmv;
  *du++ = *(uint *)bmv;
  PACK_CORR_DATA(du,block_ptr,usize);
  block_ptr = (short *)((unsigned long)block_ptr + usize);

  /* Second U block */
  *du++ = GFXBLOCK + 4 + (usize>>2);
  *du++ = (2<<30) | (1<<28) | dw1[1];
  *du++ = (xy + 4);
  *du++ = (4<<16) | 8;
  *du++ = *(uint *)&fmv[2];
  *du++ = *(uint *)&bmv[2];
  PACK_CORR_DATA(du,block_ptr,usize);
  block_ptr = (short *)((unsigned long)block_ptr + usize);
  /* End U Blocks */

  /* V Blocks */
  *dv++ = GFXBLOCK + 4 + (vsize>>2);
  *dv++ = (3<<30) | (1<<28) | dw1[0];
  *dv++ = xy;
  *dv++ = (4<<16) | 8;
  *dv++ = *(uint *)fmv;
  *dv++ = *(uint *)bmv;
  PACK_CORR_DATA(dv,block_ptr,vsize);
  block_ptr = (short *)((unsigned long)block_ptr + vsize);

  /* Second V Block */
  *dv++ = GFXBLOCK + 4 + (vsize>>2);
  *dv++ = (3<<30) | (1<<28) | dw1[1];
  *dv++ = (xy + 4);
  *dv++ = (4<<16) | 8;
  *dv++ = *(uint *)&fmv[2];
  *dv++ = *(uint *)&bmv[2];
  PACK_CORR_DATA(dv,block_ptr,vsize);
  block_ptr = (short *)((unsigned long)block_ptr + vsize);
  /* End V Blocks */

  *datay = dy;
  *datau = du;
  *datav = dv;
}

/***************************************************************************
// Function: renderDualPrimeinField
// Description: inline function that sets hardware parameters for a Dual
//  prime encoded macroblock in a field picture.
***************************************************************************/
static __inline__ void renderDualPrimeinField(uint **datay,uint **datau,
					      uint **datav,XvMCMacroBlock *mb,
					      short *block_ptr,uint ps,
					      uint flags) {

  register uint *dy = *datay;
  register uint *du = *datau;
  register uint *dv = *datav;

  /* Motion Vectors */
  short fmv[2];
  short bmv[2];
  /* gfxblock dword 1 */
  uint dw1;


  uint ysize = y_frame_bytes[mb->coded_block_pattern];
  uint usize = u_frame_bytes[mb->coded_block_pattern];
  uint vsize = v_frame_bytes[mb->coded_block_pattern];

  uint xy = ((uint)mb->x<<20) | ((uint)mb->y<<4);


  if(ps & XVMC_TOP_FIELD) {
    dw1 = (mb->coded_block_pattern<<22) | 3<<12 | 2<<6 | 2<<3 | 3;
  }
  else {
    dw1 = (mb->coded_block_pattern<<22) | 3<<12 | 3<<6 | 3<<3 | 2;
  }
  fmv[0] = mb->PMV[0][0][1];
  fmv[1] = mb->PMV[0][0][0];
  bmv[0] = mb->PMV[0][1][1];
  bmv[1] = mb->PMV[0][1][0];

  /* Y Block */
  *dy++ = GFXBLOCK + 4 + (ysize>>2);
  *dy++ = (1<<30) | (3<<28) | dw1;
  *dy++ = xy;
  *dy++ = (16<<16) | 16;
  *dy++ = *(uint *)fmv;
  *dy++ = *(uint *)bmv;
  PACK_CORR_DATA(dy,block_ptr,ysize);
  block_ptr = (short *)((unsigned long)block_ptr + ysize);
  /* End Y Blocks */
 
  fmv[0] /= 2;
  fmv[1] /= 2;
  bmv[0] /= 2;
  bmv[1] /= 2;
  xy >>= 1;

  /* U Block */
  *du++ = GFXBLOCK + 4 + (usize>>2);
  *du++ = (2<<30) | (1<<28) | dw1;
  *du++ = xy;
  *du++ = (8<<16) | 8;
  *du++ = *(uint *)fmv;
  *du++ = *(uint *)bmv;
  PACK_CORR_DATA(du,block_ptr,usize);
  block_ptr = (short *)((unsigned long)block_ptr + usize);

  /* V Block */
  *dv++ = GFXBLOCK + 4 + (vsize>>2);
  *dv++ = (3<<30) | (1<<28) | dw1;
  *dv++ = xy;
  *dv++ = (8<<16) | 8;
  *dv++ = *(uint *)fmv;
  *dv++ = *(uint *)bmv;
  PACK_CORR_DATA(dv,block_ptr,vsize);
  block_ptr = (short *)((unsigned long)block_ptr + vsize);

  *datay = dy;
  *datau = du;
  *datav = dv;
}

/***************************************************************************
// Function: renderFieldinFrame
// Description: inline function that sets hardware parameters for a Field
//  encoded macroblock in a frame picture.
***************************************************************************/
static __inline__ void renderFieldinFrame(uint **datay,uint **datau,
					  uint **datav,
					  XvMCMacroBlock *mb,short *block_ptr,
					  uint flags) {

  register uint *dy = *datay;
  register uint *du = *datau;
  register uint *dv = *datav;

  /* Motion Vectors */
  short fmv[4];
  short bmv[4];
  /* gfxblock dword 1 */
  uint dw1[2];

  uint y1size = y_first_field_bytes[mb->coded_block_pattern];
  uint y2size = y_second_field_bytes[mb->coded_block_pattern];
  uint usize = u_field_bytes[mb->coded_block_pattern];
  uint vsize = v_field_bytes[mb->coded_block_pattern];

  uint xy = ((uint)mb->x<<20) | ((uint)mb->y<<3);

  dw1[0] = type_table[mb->macroblock_type & 0xf] | (0x2<<6) |
    mvfs_table[mb->motion_vertical_field_select & 3] |
    (((uint)mb->coded_block_pattern)<<22);
  
  dw1[1] = type_table[mb->macroblock_type & 0xf] | (0x3<<6) |
    mvfs_table[mb->motion_vertical_field_select>>2] |
    (((mb->coded_block_pattern & 0x3) |
      ((mb->coded_block_pattern & 0xc)<<2))<<22);

  fmv[0] = mb->PMV[0][0][1]/2;
  fmv[1] = mb->PMV[0][0][0];
  fmv[2] = mb->PMV[1][0][1]/2;
  fmv[3] = mb->PMV[1][0][0];
  
  bmv[0] = mb->PMV[0][1][1]/2;
  bmv[1] = mb->PMV[0][1][0];
  bmv[2] = mb->PMV[1][1][1]/2;
  bmv[3] = mb->PMV[1][1][0];

  /* First Y Block */
  *dy++ = GFXBLOCK + 4 + (y1size>>2);
  *dy++ = (1<<30) | (2<<28) | dw1[0];
  *dy++ = xy;
  *dy++ = (8<<16) | 16;
  *dy++ = *(uint *)&fmv[0];
  *dy++ = *(uint *)&bmv[0];
  PACK_CORR_DATA(dy,block_ptr,y1size);
  block_ptr = (short *)((unsigned long)block_ptr + y1size);

  /* Second Y Block */
  *dy++ = GFXBLOCK + 4 + (y2size>>2);
  *dy++ = (1<<30) | (2<<28) | dw1[1];
  *dy++ = xy;
  *dy++ = (8<<16) | 16;
  *dy++ = *(uint *)&fmv[2];
  *dy++ = *(uint *)&bmv[2];
  PACK_CORR_DATA(dy,block_ptr,y2size);
  block_ptr = (short *)((unsigned long)block_ptr + y2size);
  /* End Y Blocks */

  fmv[0] /= 2;
  fmv[1] /= 2;
  fmv[2] /= 2;
  fmv[3] /= 2;
  
  bmv[0] /= 2;
  bmv[1] /= 2;
  bmv[2] /= 2;
  bmv[3] /= 2;

  xy >>= 1;

  /* U Blocks */
  *du++ = GFXBLOCK + 4 + (usize>>2);
  *du++ = (2<<30) | (1<<28) | dw1[0];
  *du++ = xy;
  *du++ = (4<<16) | 8;
  *du++ = *(uint *)&fmv[0];
  *du++ = *(uint *)&bmv[0];
  if(usize) {
    PACK_CORR_DATA_SHORT(du,block_ptr);
  }

  /* Second U Block */
  *du++ = GFXBLOCK + 4 + (usize>>2);
  *du++ = (2<<30) | (1<<28) | dw1[1];
  *du++ = xy;
  *du++ = (4<<16) | 8;
  *du++ = *(uint *)&fmv[2];
  *du++ = *(uint *)&bmv[2];
  if(usize) {
    block_ptr = (short *)((unsigned long)block_ptr + 16);
    PACK_CORR_DATA_SHORT(du,block_ptr);
    block_ptr = (short *)((unsigned long)block_ptr + 112);
  }
  /* End U Blocks */

  /* V Blocks */
  *dv++ = GFXBLOCK + 4 + (vsize>>2);
  *dv++ = (3<<30) | (1<<28) | dw1[0];
  *dv++ = xy;
  *dv++ = (4<<16) | 8;
  *dv++ = *(uint *)&fmv[0];
  *dv++ = *(uint *)&bmv[0];
  if(vsize) {
    PACK_CORR_DATA_SHORT(dv,block_ptr);
  }

  /* Second V Block */
  *dv++ = GFXBLOCK + 4 + (vsize>>2);
  *dv++ = (3<<30) | (1<<28) | dw1[1];
  *dv++ = xy;
  *dv++ = (4<<16) | 8;
  *dv++ = *(uint *)&fmv[2];
  *dv++ = *(uint *)&bmv[2];
  if(vsize) {
    block_ptr = (short *)((unsigned long)block_ptr + 16);
    PACK_CORR_DATA_SHORT(dv,block_ptr);
    block_ptr = (short *)((unsigned long)block_ptr + 112);
  }
  /* End V Blocks */

  *datay = dy;
  *datau = du;
  *datav = dv;
}

/***************************************************************************
// Function: renderFieldinFrameDCT0
// Description: inline function that sets hardware parameters for a Field
//  encoded macroblock in a frame picture with DCT0.
***************************************************************************/
static __inline__ void renderFieldinFrameDCT0(uint **datay,uint **datau,
					      uint **datav,XvMCMacroBlock *mb,
					      short *block_ptr,uint flags) {

  register uint *dy = *datay;
  register uint *du = *datau;
  register uint *dv = *datav;

  /* Motion Vectors */
  short fmv[4];
  short bmv[4];
  /* CBP */
  uint cbp = (uint)mb->coded_block_pattern;
  /* gfxblock dword 1 */
  uint dw1[2];

  short * top_left_b = NULL;
  short * top_right_b = NULL;
  short * bottom_left_b = NULL;
  short * bottom_right_b = NULL;

  unsigned int ysize = y_dct0_field_bytes[cbp];
  unsigned int usize = u_field_bytes[cbp];
  unsigned int vsize = v_field_bytes[cbp];

  uint xy = ((uint)mb->x<<20) | ((uint)mb->y<<3);

  dw1[0] = type_table[mb->macroblock_type & 0xf] | (0x2<<6) |
    mvfs_table[mb->motion_vertical_field_select & 3] |
    ((cbp | ((cbp<<2) & 0x30))<<22);
  
  dw1[1] = type_table[mb->macroblock_type & 0xf] | (0x3<<6) |
    mvfs_table[mb->motion_vertical_field_select>>2] |
    ((cbp | ((cbp<<2) & 0x30))<<22);


  fmv[0] = mb->PMV[0][0][1]/2;
  fmv[1] = mb->PMV[0][0][0];
  fmv[2] = mb->PMV[1][0][1]/2;
  fmv[3] = mb->PMV[1][0][0];
  
  bmv[0] = mb->PMV[0][1][1]/2;
  bmv[1] = mb->PMV[0][1][0];
  bmv[2] = mb->PMV[1][1][1]/2;
  bmv[3] = mb->PMV[1][1][0];

  /*
    The i810 cannot use DCT0 directly with field motion, we have to
    interlace the data for it. We use a zero block when the CBP has
    one half of the to-be-interlaced data but not the other half.
  */
  top_left_b = &empty_block[0];
  if(cbp & 0x20) {
    top_left_b = block_ptr;
    block_ptr += 64;
  }

  top_right_b = &empty_block[0];
  if(cbp & 0x10) {
    top_right_b = block_ptr;
    block_ptr += 64;
  }

  bottom_left_b = &empty_block[0];
  if(cbp & 0x8) {
    bottom_left_b = block_ptr;
    block_ptr += 64;
  }

  bottom_right_b = &empty_block[0];
  if(cbp & 0x4) {
    bottom_right_b = block_ptr;
    block_ptr += 64;
  }

  /* First Y Block */
  *dy++ = GFXBLOCK + 4 + (ysize>>2);
  *dy++ = (1<<30) | (2<<28) | dw1[0];
  *dy++ = xy;
  *dy++ = (8<<16) | 16;
  *dy++ = *(uint *)&fmv[0];
  *dy++ = *(uint *)&bmv[0];
  if(dw1[0] & (1<<27)) {
    PACK_CORR_DATA_0to1(dy,top_left_b,bottom_left_b);
  }
  if(dw1[0] & (1<<26)) {
    PACK_CORR_DATA_0to1(dy,top_right_b,bottom_right_b);
  }

  /* Second Y Block */
  *dy++ = GFXBLOCK + 4 + (ysize>>2);
  *dy++ = (1<<30) | (2<<28) | dw1[1];
  *dy++ = xy;
  *dy++ = (8<<16) | 16;
  *dy++ = *(uint *)&fmv[2];
  *dy++ = *(uint *)&bmv[2];
  if(dw1[1] & (1<<27)) {
    top_left_b = (short *)((unsigned long)top_left_b + 16);
    bottom_left_b = (short *)((unsigned long)bottom_left_b + 16);
    PACK_CORR_DATA_0to1(dy,top_left_b,bottom_left_b);
  }
  if(dw1[1] & (1<<26)) {
    top_right_b = (short *)((unsigned long)top_right_b + 16);
    bottom_right_b = (short *)((unsigned long)bottom_right_b + 16);
    PACK_CORR_DATA_0to1(dy,top_right_b,bottom_right_b);
  }
  /* End Y Blocks */

  fmv[0] /= 2;
  fmv[1] /= 2;
  fmv[2] /= 2;
  fmv[3] /= 2;
  
  bmv[0] /= 2;
  bmv[1] /= 2;
  bmv[2] /= 2;
  bmv[3] /= 2;

  xy >>= 1;

  /* U Blocks */
  *du++ = GFXBLOCK + 4 + (usize>>2);
  *du++ = (2<<30) | (1<<28) | dw1[0];
  *du++ = xy;
  *du++ = (4<<16) | 8;
  *du++ = *(uint *)&fmv[0];
  *du++ = *(uint *)&bmv[0];
  if(usize) {
    PACK_CORR_DATA_SHORT(du,block_ptr);
  }

  /* Second U Block */
  *du++ = GFXBLOCK + 4 + (usize>>2);
  *du++ = (2<<30) | (1<<28) | dw1[1];
  *du++ = xy;
  *du++ = (4<<16) | 8;
  *du++ = *(uint *)&fmv[2];
  *du++ = *(uint *)&bmv[2];
  if(usize) {
    block_ptr = (short *)((unsigned long)block_ptr + 16);
    PACK_CORR_DATA_SHORT(du,block_ptr);
    block_ptr = (short *)((unsigned long)block_ptr + 112);
  }
  /* End U Blocks */

  /* V Blocks */
  *dv++ = GFXBLOCK + 4 + (vsize>>2);
  *dv++ = (3<<30) | (1<<28) | dw1[0];
  *dv++ = xy;
  *dv++ = (4<<16) | 8;
  *dv++ = *(uint *)&fmv[0];
  *dv++ = *(uint *)&bmv[0];
  if(vsize) {
    PACK_CORR_DATA_SHORT(dv,block_ptr);
  }

  /* Second V Block */
  *dv++ = GFXBLOCK + 4 + (vsize>>2);
  *dv++ = (3<<30) | (1<<28) | dw1[1];
  *dv++ = xy;
  *dv++ = (4<<16) | 8;
  *dv++ = *(uint *)&fmv[2];
  *dv++ = *(uint *)&bmv[2];
  if(vsize) {
    block_ptr = (short *)((unsigned long)block_ptr + 16);
    PACK_CORR_DATA_SHORT(dv,block_ptr);
    block_ptr = (short *)((unsigned long)block_ptr + 112);
  }
  /* End V Blocks */

  *datay = dy;
  *datau = du;
  *datav = dv;
}

/***************************************************************************
// Function: renderFrameinFrame
// Description: inline function that sets hardware parameters for a Frame
//  encoded macroblock in a frame picture.
***************************************************************************/
static __inline__ void renderFrameinFrame(uint **datay,uint **datau,
					  uint **datav,
					  XvMCMacroBlock *mb,short *block_ptr,
					  uint flags) {

  register uint *dy = *datay;
  register uint *du = *datau;
  register uint *dv = *datav;

  /* Motion Vectors */
  short fmv[2];
  short bmv[2];
  /* gfxblock dword 1 */
  uint dw1;

  unsigned int ysize = y_frame_bytes[mb->coded_block_pattern];
  unsigned int usize = u_frame_bytes[mb->coded_block_pattern];
  unsigned int vsize = v_frame_bytes[mb->coded_block_pattern];

  uint xy = ((uint)mb->x<<20) | ((uint)mb->y<<4);

  dw1 = type_table[mb->macroblock_type & 0xf] |
    (((uint)mb->coded_block_pattern)<<22);


  fmv[0] = mb->PMV[0][0][1];
  fmv[1] = mb->PMV[0][0][0];
  
  bmv[0] = mb->PMV[0][1][1];
  bmv[1] = mb->PMV[0][1][0];

  /* Y Block */
  *dy++ = GFXBLOCK + 4 + (ysize>>2);
  *dy++ = (1<<30) | (3<<28) | dw1;
  *dy++ = xy;
  *dy++ = (16<<16) | 16;
  *dy++ = *(uint *)fmv;
  *dy++ = *(uint *)bmv;
  PACK_CORR_DATA(dy,block_ptr,ysize);
  block_ptr = (short *)((unsigned long)block_ptr + ysize);
  /* End Y Blocks */

  fmv[0] /= 2;
  fmv[1] /= 2;
  
  bmv[0] /= 2;
  bmv[1] /= 2;

  xy >>= 1;

  /* U Block */
  *du++ = GFXBLOCK + 4 + (usize>>2);
  *du++ = (2<<30) | (1<<28) | dw1;
  *du++ = xy;
  *du++ = (8<<16) | 8;
  *du++ = *(uint *)fmv;
  *du++ = *(uint *)bmv;
  PACK_CORR_DATA(du,block_ptr,usize);
  block_ptr = (short *)((unsigned long)block_ptr + usize);
  /* End U Block */

  /* V Block */
  *dv++ = GFXBLOCK + 4 + (vsize>>2);
  *dv++ = (3<<30) | (1<<28) | dw1;
  *dv++ = xy;
  *dv++ = (8<<16) | 8;
  *dv++ = *(uint *)fmv;
  *dv++ = *(uint *)bmv;
  PACK_CORR_DATA(dv,block_ptr,vsize);
  block_ptr = (short *)((unsigned long)block_ptr + vsize);
  /* End V Block */

  *datay = dy;
  *datau = du;
  *datav = dv;
}

/***************************************************************************
// Function: renderFrameinFrameDCT1
// Description: inline function that sets hardware parameters for a Frame
//  encoded macroblock in a frame picture with DCT type 1.
***************************************************************************/
static __inline__ void renderFrameinFrameDCT1(uint **datay,uint **datau,
					      uint **datav,XvMCMacroBlock *mb,
					      short *block_ptr,uint flags) {

  register uint *dy = *datay;
  register uint *du = *datau;
  register uint *dv = *datav;

  /* Motion Vectors */
  short fmv[4];
  short bmv[4];

  short * top_left_b = NULL;
  short * top_right_b = NULL;
  short * bottom_left_b = NULL;
  short * bottom_right_b = NULL;

  uint temp_bp = 0;

  uint ysize = y_dct1_frame_bytes[mb->coded_block_pattern];
  uint usize = u_frame_bytes[mb->coded_block_pattern];
  uint vsize = v_frame_bytes[mb->coded_block_pattern];

  uint xy = ((uint)mb->x<<20) | ((uint)mb->y<<4);

  uint dw1 = type_table[mb->macroblock_type & 0xf] |
    (((uint)mb->coded_block_pattern)<<22);

  fmv[0] = mb->PMV[0][0][1];
  fmv[1] = mb->PMV[0][0][0];
  
  bmv[0] = mb->PMV[0][1][1];
  bmv[1] = mb->PMV[0][1][0];

  /*
    It is easiest to find out what blocks are in need of reading first
    rather than as we go.
  */
  top_left_b = &empty_block[0];
  if(dw1 & (1<<27)) {
    temp_bp |= (1<<25);
    top_left_b = block_ptr;
    block_ptr += 64;
  }

  top_right_b = &empty_block[0];
  if(dw1 & (1<<26)) {
    temp_bp |= (1<<24);
    top_right_b = block_ptr;
    block_ptr += 64;
  }

  bottom_left_b = &empty_block[0];
  if(dw1 & (1<<25)) {
    temp_bp |= (1<<27);
    bottom_left_b = block_ptr;
    block_ptr += 64;
  }

  bottom_right_b = &empty_block[0];
  if(dw1 & (1<<24)) {
    temp_bp |= (1<<26);
    bottom_right_b = block_ptr;
    block_ptr += 64;
  }
  dw1 |= temp_bp;

  /* Y Block */
  *dy++ = GFXBLOCK + 4 + (ysize>>2);
  *dy++ = (1<<30) | (3<<28) | dw1;
  *dy++ = xy;
  *dy++ = (16<<16) | 16;
  *dy++ = *(uint *)fmv;
  *dy++ = *(uint *)bmv;
  if(dw1 & (1<<27)) {
    PACK_CORR_DATA_1to0(dy,top_left_b,bottom_left_b);
    top_left_b = (short *)((unsigned long)top_left_b + 64);
    bottom_left_b = (short *)((unsigned long)bottom_left_b + 64);
  }
  if(dw1 & (1<<26)) {
    PACK_CORR_DATA_1to0(dy,top_right_b,bottom_right_b);
    top_right_b = (short *)((unsigned long)top_right_b + 64);
    bottom_right_b = (short *)((unsigned long)bottom_right_b + 64);
  }
  if(dw1 & (1<<27)) {
    PACK_CORR_DATA_1to0(dy,top_left_b,bottom_left_b);
  }
  if(dw1 & (1<<26)) {
    PACK_CORR_DATA_1to0(dy,top_right_b,bottom_right_b);
  }
  /* End Y Block */

  fmv[0] /= 2;
  fmv[1] /= 2;
  
  bmv[0] /= 2;
  bmv[1] /= 2;

  xy >>= 1;

  /* U Block */
  *du++ = GFXBLOCK + 4 + (usize>>2);
  *du++ = (2<<30) | (1<<28) | dw1;
  *du++ = xy;
  *du++ = (8<<16) | 8;
  *du++ = *(uint *)fmv;
  *du++ = *(uint *)bmv;
  PACK_CORR_DATA(du,block_ptr,usize);
  block_ptr = (short *)((unsigned long)block_ptr + usize);

  /* V Block */
  *dv++ = GFXBLOCK + 4 + (vsize>>2);
  *dv++ = (3<<30) | (1<<28) | dw1;
  *dv++ = xy;
  *dv++ = (8<<16) | 8;
  *dv++ = *(uint *)fmv;
  *dv++ = *(uint *)bmv;
  PACK_CORR_DATA(dv,block_ptr,vsize);
  block_ptr = (short *)((unsigned long)block_ptr + vsize);

  *datay = dy;
  *datau = du;
  *datav = dv;
}

/***************************************************************************
// Function: renderDualPrimeinFrame
// Description: inline function that sets hardware parameters for a Dual
//  Prime encoded macroblock in a frame picture with dct 1.
***************************************************************************/
static __inline__ void renderDualPrimeinFrame(uint **datay,uint **datau,
					      uint **datav,XvMCMacroBlock *mb,
					      short *block_ptr,uint flags) {

  register uint *dy = *datay;
  register uint *du = *datau;
  register uint *dv = *datav;

  /* Motion Vectors */
  short fmv[4];
  short bmv[4];
  /* gfxblock dword 1 */
  uint dw1[2];

  uint y1size = y_first_field_bytes[mb->coded_block_pattern];
  uint y2size = y_second_field_bytes[mb->coded_block_pattern];
  uint usize = u_field_bytes[mb->coded_block_pattern];
  uint vsize = v_field_bytes[mb->coded_block_pattern];

  uint xy = ((uint)mb->x<<20) | ((uint)mb->y<<3);

  /*
    Past Surface (map 0) is used for same parity prediction,
    Future surface (map 1) is used for opposite.
  */
  dw1[0] = (((uint)mb->coded_block_pattern)<<22) |
    3<<12 | 2<<6 | 2<<3 | 3;
  dw1[1] = (((mb->coded_block_pattern & 0x3) |
	     ((mb->coded_block_pattern & 0xc)<<2))<<22) |
    3<<12 | 3<<6 | 3<<3 | 2;
  
  fmv[0] = mb->PMV[0][0][1];
  fmv[1] = mb->PMV[0][0][0];
  bmv[0] = mb->PMV[1][0][1];
  bmv[1] = mb->PMV[1][0][0];
  
  fmv[2] = mb->PMV[0][0][1];
  fmv[3] = mb->PMV[0][0][0];
  bmv[2] = mb->PMV[1][1][1];
  bmv[3] = mb->PMV[1][1][0];

  /* First Y Block */
  *dy++ = GFXBLOCK + 4 + (y1size>>2);
  *dy++ = (1<<30) | (2<<28) | dw1[0];
  *dy++ = xy;
  *dy++ = (8<<16) | 16;
  *dy++ = *(uint *)fmv;
  *dy++ = *(uint *)bmv;;
  PACK_CORR_DATA(dy,block_ptr,y1size);
  block_ptr = (short *)((unsigned long)block_ptr + y1size);

  /* Second Y Block */
  *dy++ = GFXBLOCK + 4 + (y2size>>2);
  *dy++ = (1<<30) | (2<<28) | dw1[1];
  *dy++ = xy;
  *dy++ = (8<<16) | 16;
  *dy++ = *(uint *)&fmv[2];
  *dy++ = *(uint *)&bmv[2];
  PACK_CORR_DATA(dy,block_ptr,y2size);
  block_ptr = (short *)((unsigned long)block_ptr + y2size);

  fmv[0] /= 2;
  fmv[1] /= 2;
  bmv[0] /= 2;
  bmv[1] /= 2;
  
  fmv[2] /= 2;
  fmv[3] /= 2;
  bmv[2] /= 2;
  bmv[3] /= 2;

  xy >>= 1;

  /* U Blocks */
  *du++ = GFXBLOCK + 4 + (usize>>2);
  *du++ = (2<<30) | (1<<28) | dw1[0];
  *du++ = xy;
  *du++ = (4<<16) | 8;
  *du++ = *(uint *)fmv;
  *du++ = *(uint *)bmv;
  if(dw1[0] & (1<<23)) {
    PACK_CORR_DATA_SHORT(du,block_ptr);
  }

  /* Second U Block */
  *du++ = GFXBLOCK + 4 + (usize>>2);
  *du++ = (2<<30) | (1<<28) | dw1[1];
  *du++ = xy;
  *du++ = (4<<16) | 8;
  *du++ = *(uint *)&fmv[2];
  *du++ = *(uint *)&bmv[2];
  if(dw1[1] & (1<<23)) {
    block_ptr = (short *)((unsigned long)block_ptr + 16);
    PACK_CORR_DATA_SHORT(du,block_ptr);
    block_ptr = (short *)((unsigned long)block_ptr + 112);
  }
  /* End U Blocks */

  /* V Blocks */
  *dv++ = GFXBLOCK + 4 + (vsize>>2);
  *dv++ = (3<<30) | (1<<28) | dw1[0];
  *dv++ = xy;
  *dv++ = (4<<16) | 8;
  *dv++ = *(uint *)fmv;
  *dv++ = *(uint *)bmv;
  if(dw1[0] & (1<<22)) {
    PACK_CORR_DATA_SHORT(dv,block_ptr);
  }

  /* Second V Block */
  *dv++ = GFXBLOCK + 4 + (vsize>>2);
  *dv++ = (3<<30) | (1<<28) | dw1[1];
  *dv++ = xy;
  *dv++ = (4<<16) | 8;
  *dv++ = *(uint *)&fmv[2];
  *dv++ = *(uint *)&bmv[2];
  if(dw1[1] & (1<<22)) {
    block_ptr = (short *)((unsigned long)block_ptr + 16);
    PACK_CORR_DATA_SHORT(dv,block_ptr);
    block_ptr = (short *)((unsigned long)block_ptr + 112);
  }
  /* End V Blocks */

  *datay = dy;
  *datau = du;
  *datav = dv;
}

/***************************************************************************
// Function: renderDualPrimeinFrameDCT0
// Description: inline function that sets hardware parameters for a Dual
//  Prime encoded macroblock in a frame picture with dct 0.
***************************************************************************/
static __inline__ void renderDualPrimeinFrameDCT0(uint **datay,uint **datau,
						  uint **datav,
						  XvMCMacroBlock *mb,
						  short *block_ptr,
						  uint flags) {

  register uint *dy = *datay;
  register uint *du = *datau;
  register uint *dv = *datav;

  /* Motion Vectors */
  short fmv[4];
  short bmv[4];
  /* gfxblock dword 1 */
  uint dw1[2];

  short * top_left_b = NULL;
  short * top_right_b = NULL;
  short * bottom_left_b = NULL;
  short * bottom_right_b = NULL;

  uint cbp = (uint)mb->coded_block_pattern;

  uint ysize = y_dct0_field_bytes[cbp];
  uint usize = u_field_bytes[cbp];
  uint vsize = v_field_bytes[cbp];

  uint xy = ((uint)mb->x<<20) | ((uint)mb->y<<3);

  /*
    Past Surface (map 0) is used for same parity prediction,
    Future surface (map 1) is used for opposite.
  */
  dw1[0] = ((cbp | ((cbp<<2) & 0x30))<<22) |
    3<<12 | 2<<6 | 2<<3 | 3;
  dw1[1] = ((cbp | ((cbp<<2) & 0x30))<<22) |
    3<<12 | 3<<6 | 3<<3 | 2;
  
  fmv[0] = mb->PMV[0][0][1];
  fmv[1] = mb->PMV[0][0][0];
  bmv[0] = mb->PMV[1][0][1];
  bmv[1] = mb->PMV[1][0][0];
  
  fmv[2] = mb->PMV[0][0][1];
  fmv[3] = mb->PMV[0][0][0];
  bmv[2] = mb->PMV[1][1][1];
  bmv[3] = mb->PMV[1][1][0];
  
  /*
    The i810 cannot use DCT0 directly with field motion, we have to
    interlace the data for it. We use a zero block when the CBP has
    one half of the to-be-interlaced data but not the other half.
  */
  top_left_b = &empty_block[0];
  if(cbp & 0x20) {
    top_left_b = block_ptr;
    block_ptr += 64;
  }

  top_right_b = &empty_block[0];
  if(cbp & 0x10) {
    top_right_b = block_ptr;
    block_ptr += 64;
  }

  bottom_left_b = &empty_block[0];
  if(cbp & 0x8) {
    bottom_left_b = block_ptr;
    block_ptr += 64;
  }

  bottom_right_b = &empty_block[0];
  if(cbp & 0x4) {
    bottom_right_b = block_ptr;
    block_ptr += 64;
  }

  /* First Y Block */
  *dy++ = GFXBLOCK + 4 + (ysize>>2);
  *dy++ = (1<<30) | (2<<28) | dw1[0];
  *dy++ = xy;
  *dy++ = (8<<16) | 16;
  *dy++ = *(uint *)fmv;
  *dy++ = *(uint *)bmv;
  if(cbp & 0x20) {
    PACK_CORR_DATA_0to1(dy,top_left_b,bottom_left_b);
  }
  if(cbp & 0x10) {
    PACK_CORR_DATA_0to1(dy,top_right_b,bottom_right_b);
  }

  /* Second Y Block */
  *dy++ = GFXBLOCK + 4 + (ysize>>2);
  *dy++ = (1<<30) | (2<<28) | dw1[1];
  *dy++ = xy;
  *dy++ = (8<<16) | 16;
  *dy++ = *(uint *)&fmv[2];
  *dy++ = *(uint *)&bmv[2];
  if(cbp & 0x20) {
    top_left_b = (short *)((unsigned long)top_left_b + 16);
    bottom_left_b = (short *)((unsigned long)bottom_left_b + 16);
    PACK_CORR_DATA_0to1(dy,top_left_b,bottom_left_b);
  }
  if(cbp & 0x10) {
    top_right_b = (short *)((unsigned long)top_right_b + 16);
    bottom_right_b = (short *)((unsigned long)bottom_right_b + 16);
    PACK_CORR_DATA_0to1(dy,top_right_b,bottom_right_b);
  }
  /* End Y Blocks */


  fmv[0] /= 2;
  fmv[1] /= 2;
  bmv[0] /= 2;
  bmv[1] /= 2;
  
  fmv[2] /= 2;
  fmv[3] /= 2;
  bmv[2] /= 2;
  bmv[3] /= 2;

  xy >>= 1;

  /* U Blocks */
  *du++ = GFXBLOCK + 4 + (usize>>2);
  *du++ = (2<<30) | (1<<28) | dw1[0];
  *du++ = xy;
  *du++ = (4<<16) | 8;
  *du++ = *(uint *)fmv;
  *du++ = *(uint *)bmv;
  if(cbp & (1<<23)) {
    PACK_CORR_DATA_SHORT(du,block_ptr);
  }

  /* Second U Block */
  *du++ = GFXBLOCK + 4 + (usize>>2);
  *du++ = (2<<30) | (1<<28) | dw1[1];
  *du++ = xy;
  *du++ = (4<<16) | 8;
  *du++ = *(uint *)&fmv[2];
  *du++ = *(uint *)&bmv[2];
  if(cbp & (1<<23)) {
    block_ptr = (short *)((unsigned long)block_ptr + 16);
    PACK_CORR_DATA_SHORT(du,block_ptr);
    block_ptr = (short *)((unsigned long)block_ptr + 112);
  }
  /* End U Blocks */

  /* V Blocks */
  *dv++ = GFXBLOCK + 4 + (vsize>>2);
  *dv++ = (3<<30) | (1<<28) | dw1[0];
  *dv++ = xy;
  *dv++ = (4<<16) | 8;
  *dv++ = *(uint *)fmv;
  *dv++ = *(uint *)bmv;
  if(cbp & (1<<22)) {
    PACK_CORR_DATA_SHORT(dv,block_ptr);
  }

  /* Second V Block */
  *dv++ = GFXBLOCK + 4 + (vsize>>2);
  *dv++ = (3<<30) | (1<<28) | dw1[1];
  *dv++ = xy;
  *dv++ = (4<<16) | 8;
  *dv++ = *(uint *)&fmv[2];
  *dv++ = *(uint *)&bmv[2];
  if(cbp & (1<<22)) {
    block_ptr = (short *)((unsigned long)block_ptr + 16);
    PACK_CORR_DATA_SHORT(dv,block_ptr);
    block_ptr = (short *)((unsigned long)block_ptr + 112);
  }
  /* End V Blocks */

  *datay = dy;
  *datau = du;
  *datav = dv;
}


/***************************************************************************
// Function: XvMCRenderSurface
// Description: This function does the actual HWMC. Given a list of
//  macroblock structures it dispatched the hardware commands to execute
//  them. DMA buffer containing Y data are dispatched as they fill up
//  U and V DMA buffers are queued until all Y's are done. This minimizes
//  the context flipping and flushing required when switching between Y
//  U and V surfaces.
***************************************************************************/
#define UV_QUEUE 14
Status XvMCRenderSurface(Display *display, XvMCContext *context,
			 unsigned int picture_structure,
			 XvMCSurface *target_surface,
			 XvMCSurface *past_surface,
			 XvMCSurface *future_surface,
			 unsigned int flags,
			 unsigned int num_macroblocks,
			 unsigned int first_macroblock,
			 XvMCMacroBlockArray *macroblock_array,
			 XvMCBlockArray *blocks) {
  /* Dma Data Structures */
  drmBufPtr pDMAy = NULL,pDMAu[UV_QUEUE],pDMAv[UV_QUEUE];
  int u_index = 0,v_index = 0;
  int dirty_context = 1;

  /* Block Pointer */
  short *block_ptr;
  /* Current Macroblock Pointer */
  XvMCMacroBlock *mb;

  drm_i810_mc_t mc;
  int i,j;
  i810XvMCSurface *privTarget;
  i810XvMCSurface *privFuture = NULL;
  i810XvMCSurface *privPast = NULL;
  i810XvMCContext *pI810XvMC;

  /* DMA Pointers set to NULL */
  uint *datay = NULL;
  uint *datau = NULL;
  uint *datav = NULL;


  /* Check Parameters for validity */
  if((target_surface == NULL) || (context == NULL) || (display == NULL)) {
    printf("Error, Invalid Target,Context, or DIsplay!\n");   
    return BadValue;
  }

  if(num_macroblocks == 0) {return Success;}
  if((macroblock_array == NULL) || (blocks == NULL)) {return BadValue;}
  if(context->privData == NULL) {return BadValue;}
  pI810XvMC = (i810XvMCContext *)context->privData;


  if(target_surface->privData == NULL) {
    printf("Error, Invalid Target Surface!\n");
    return BadValue;
  }
  privTarget = (i810XvMCSurface *)target_surface->privData;

  if(macroblock_array->num_blocks < (num_macroblocks + first_macroblock)) {
    printf("Error, Too many macroblocks requested for MB array size.\n");
    return BadValue;
  }

  /* Test For YV12 Surface */
  if(context->surface_type_id != FOURCC_YV12) {
    printf("Error, HWMC only possible on YV12 Surfaces\n");
    return BadValue;
  }

  /* P Frame Test */
  if(past_surface == NULL) {
    /* Just to avoid some ifs later. */
    privPast = privTarget;
  }
  else {
    if(past_surface->privData == NULL) {
      printf("Error, Invalid Past Surface!\n");
      return BadValue;
    }
    privPast = (i810XvMCSurface *)past_surface->privData;
  }


  /* B Frame Test */
  if(future_surface == NULL) {
    privFuture = privTarget;
    if(pI810XvMC->dual_prime) {
      privFuture = privPast;
      /* I810 Specific flag for marking when dual prime is in use. */
      flags |= 0x40000000;
    }

    /*
      References are different for the Second Field Picture. The
      i810 needs to know if it is the second field picture in a
      P picture. We use a Private flag to mark this.
    */
    if(flags & XVMC_SECOND_FIELD) {
      /* I810 Specific flag for marking second fields. */
      flags |= 0x80000000;
    }
  }
  else {
    if((future_surface->privData == NULL) || (past_surface == NULL)) {
      printf("Error, Invalid Future Surface or No Past Surface!\n");
      return BadValue;
    }
    privFuture = (i810XvMCSurface *)future_surface->privData;

    /*
      Undo Second Field flag since the second field in B frames is just like
      the first.
    */
    flags &= ~0x80000000;
  }

  /* Lock For DMA */
  I810_LOCK(pI810XvMC,0);

  for(i=first_macroblock; i<(num_macroblocks + first_macroblock); i++) {
    /* Set up values needed for each macroblock */
    mb = &macroblock_array->macro_blocks[i];
    block_ptr = &(blocks->blocks[mb->index<<6]);

    /* Lockup can happen if the coordinates are too far out of range */
    if(mb->x > target_surface->width>>4) {
      mb->x = 0;
    }
    if(mb->y > target_surface->height>>4) {
      mb->y = 0;
    }

    /* If buffers are almost full dispatch them */
    if(datay) {
      pDMAy->used = (unsigned long)datay - (unsigned long)pDMAy->address;
      if(pDMAy->used  > 3520) {
	if(dirty_context) {
	  dispatchYContext(privTarget,privPast,privFuture,pI810XvMC);
	}
	dirty_context = 0;
	mc.idx = pDMAy->idx;
	mc.used = pDMAy->used;
	datay = NULL;
	mc.last_render = ++pI810XvMC->last_render;
	privTarget->last_render = pI810XvMC->last_render;
	I810_MC(pI810XvMC,mc);
      } /* datay near full */
    } /* if(datay) */
    if(datau) {
      pDMAu[u_index]->used = (unsigned long)datau - (unsigned long)pDMAu[u_index]->address;
      if(pDMAu[u_index]->used > 3904) {
	u_index++;
	datau = NULL;
	if(u_index == UV_QUEUE) {
	  for(j=0; j<UV_QUEUE; j++) {
	    mc.idx = pDMAu[j]->idx;
	    mc.used = pDMAu[j]->used;
	    mc.last_render = ++pI810XvMC->last_render;
	    privTarget->last_render = pI810XvMC->last_render;
	    I810_MC(pI810XvMC,mc);
	  }
	  u_index = 0;
	  dirty_context = 1;
	} /* if(u_index == UV_QUEUE) */
      } /* datau near full */
    } /* if(datau) */
    if(datav) {
      pDMAv[v_index]->used = (unsigned long)datav - (unsigned long)pDMAv[v_index]->address;
      if(pDMAv[v_index]->used > 3904) {
	v_index++;
	datav = NULL;
	if(v_index == UV_QUEUE) {
	  for(j=0; j<UV_QUEUE; j++) {
	    mc.idx = pDMAv[j]->idx;
	    mc.used = pDMAv[j]->used;
	    mc.last_render = ++pI810XvMC->last_render;
	    privTarget->last_render = pI810XvMC->last_render;
	    I810_MC(pI810XvMC,mc);
	  }
	  v_index = 0;
	  dirty_context = 1;
	} /* if(v_index == UV_QUEUE) */
      } /* datav near full */
    } /* if(datav) */

    /* Allocate buffers if this is the first loop,or if we just dispatched */
    if(datay == NULL) {
      pDMAy = i810_get_free_buffer(pI810XvMC);
      datay = pDMAy->address;
    }/* if(datay == NULL) */
    if(datau == NULL) {
      pDMAu[u_index] = i810_get_free_buffer(pI810XvMC);
      datau = pDMAu[u_index]->address;
      if(u_index == 0) {
	*datau++ = CMD_FLUSH;
	*datau++ = BOOLEAN_ENA_2;
	*datau++ = CMD_FLUSH;
	*datau++ = DEST_BUFFER_INFO;
	*datau++ = privTarget->dbi1u;
	*datau++ = DEST_BUFFER_VAR;
	*datau++ = privTarget->dbv1;
	/* Past Surface */
	*datau++ = CMD_MAP_INFO;
	*datau++ = privPast->mi1u;
	*datau++ = privPast->mi2u;
	*datau++ = privPast->mi3u;
	/* Future Surface */
	*datau++ = CMD_MAP_INFO;
	*datau++ = privFuture->mi1u | 0x1<<28;
	*datau++ = privFuture->mi2u;
	*datau++ = privFuture->mi3u;
      }
    } /* if(datau == NULL) */
    if(datav == NULL) {
      pDMAv[v_index] = i810_get_free_buffer(pI810XvMC);
      datav = pDMAv[v_index]->address;
      if(v_index == 0) {
	*datav++ = CMD_FLUSH;
	*datav++ = BOOLEAN_ENA_2;
	*datav++ = CMD_FLUSH;
	*datav++ = DEST_BUFFER_INFO;
	*datav++ = privTarget->dbi1v;
	*datav++ = DEST_BUFFER_VAR;
	*datav++ = privTarget->dbv1;
	/* Past Surface */
	*datav++ = CMD_MAP_INFO;
	*datav++ = privPast->mi1v;
	*datav++ = privPast->mi2v;
	*datav++ = privPast->mi3v;
	/* Future Surface */
	*datav++ = CMD_MAP_INFO;
	*datav++ = privFuture->mi1v | 0x1<<28;
	*datav++ = privFuture->mi2v;
	*datav++ = privFuture->mi3v;
      }
    }/* if(datav == NULL) */

    /* Catch no pattern case */
    if(!(mb->macroblock_type & 0x8)) {
      mb->coded_block_pattern = 0;
    }


    if(mb->motion_type == XVMC_PREDICTION_DUAL_PRIME) {
      /*
	By default the maps will not be set up for dual
	prime. We have to change them.
      */
      if(!pI810XvMC->dual_prime) {
	pI810XvMC->dual_prime = 1;
	privFuture = privPast;
	/* Y */
	*datay++ = CMD_MAP_INFO;
	*datay++ = privFuture->mi1y | 0x1<<28;
	*datay++ = privFuture->mi2y;
	*datay++ = privFuture->mi3y;
	/* U */
	*datau++ = CMD_MAP_INFO;
	*datau++ = privFuture->mi1u | 0x1<<28;
	*datau++ = privFuture->mi2u;
	*datau++ = privFuture->mi3u;
	/* V */
	*datav++ = CMD_MAP_INFO;
	*datav++ = privFuture->mi1v | 0x1<<28;
	*datav++ = privFuture->mi2v;
	*datav++ = privFuture->mi3v;
      }
    }
    if((pI810XvMC->dual_prime) &&
       (mb->motion_type != XVMC_PREDICTION_DUAL_PRIME)) {
      	pI810XvMC->dual_prime = 0;
	privFuture = privTarget;
	/* Y */
	*datay++ = CMD_MAP_INFO;
	*datay++ = privFuture->mi1y | 0x1<<28;
	*datay++ = privFuture->mi2y;
	*datay++ = privFuture->mi3y;
	/* U */
	*datau++ = CMD_MAP_INFO;
	*datau++ = privFuture->mi1u | 0x1<<28;
	*datau++ = privFuture->mi2u;
	*datau++ = privFuture->mi3u;
	/* V */
	*datav++ = CMD_MAP_INFO;
	*datav++ = privFuture->mi1v | 0x1<<28;
	*datav++ = privFuture->mi2v;
	*datav++ = privFuture->mi3v;
    }


    /* Frame Picture */
    if((picture_structure & XVMC_FRAME_PICTURE) == XVMC_FRAME_PICTURE) {
      /* Intra Blocks */
      if(mb->macroblock_type & XVMC_MB_TYPE_INTRA) {
	if(mb->dct_type) {
	  renderIntrainFrameDCT1(&datay,&datau,&datav,mb,block_ptr,flags);
	  continue;
	}
	renderIntrainFrame(&datay,&datau,&datav,mb,block_ptr);
	continue;
      }
      switch((mb->motion_type & 0x3) | (mb->dct_type<<2)) {
      case 0x2:  /* Frame DCT0 */
	renderFrameinFrame(&datay,&datau,&datav,mb,block_ptr,flags);
	continue;
      case 0x5:  /* Field DCT1 */
	renderFieldinFrame(&datay,&datau,&datav,mb,block_ptr,flags);
	continue;
      case 0x6: /* Frame DCT1 */
	renderFrameinFrameDCT1(&datay,&datau,&datav,mb,block_ptr,flags);
	continue;
      case 0x1: /* Field DCT0 */
	renderFieldinFrameDCT0(&datay,&datau,&datav,mb,block_ptr,flags);
	continue;
      case 0x3:  /* Dual Prime DCT0 */
	renderDualPrimeinFrame(&datay,&datau,&datav,mb,block_ptr,flags);
	continue;
      case 0x7:  /* Dual Prime DCT1 */
	renderDualPrimeinFrameDCT0(&datay,&datau,&datav,mb,block_ptr,flags);
	continue;
      default:  /* No Motion Type */
	renderError();
	continue;
      } /* Switch */
    } /* Frame Picture */

    /* Field Pictures */
    if(mb->macroblock_type & XVMC_MB_TYPE_INTRA) {
      renderIntrainField(&datay,&datau,&datav,mb,block_ptr,picture_structure);
      continue;
    }
    switch(mb->motion_type & 0x3) {
    case 0x1: /* Field Motion */
      renderFieldinField(&datay,&datau,&datav,mb,block_ptr,picture_structure,
			 flags);
      continue;
    case 0x2: /* 16x8 Motion */
      render16x8inField(&datay,&datau,&datav,mb,block_ptr,picture_structure,
			flags);
      continue;
    case 0x3: /* Dual Prime */
      renderDualPrimeinField(&datay,&datau,&datav,mb,block_ptr,
			     picture_structure,flags);
      continue;
    default:  /* No Motion Type */
      renderError();
      continue;
    }
    continue;

  } /* for each Macroblock */

  /* Dispatch remaining DMA buffers */
  if(dirty_context) {
    dispatchYContext(privTarget,privPast,privFuture,pI810XvMC);
  }
  mc.idx = pDMAy->idx;
  mc.used = (unsigned long)datay - (unsigned long)pDMAy->address;
  mc.last_render = ++pI810XvMC->last_render;
  privTarget->last_render = pI810XvMC->last_render;
  I810_MC(pI810XvMC,mc);

  pDMAu[u_index]->used = (unsigned long)datau - (unsigned long)pDMAu[u_index]->address;
  for(j=0; j<=u_index; j++) {
    mc.idx = pDMAu[j]->idx;
    mc.used = pDMAu[j]->used;
    mc.last_render = ++pI810XvMC->last_render;
    privTarget->last_render = pI810XvMC->last_render;
    I810_MC(pI810XvMC,mc);
  }
  pDMAv[v_index]->used = (unsigned long)datav - (unsigned long)pDMAv[v_index]->address;
  for(j=0; j<=v_index; j++) {
    mc.idx = pDMAv[j]->idx;
    mc.used = pDMAv[j]->used;
    mc.last_render = ++pI810XvMC->last_render;
    privTarget->last_render = pI810XvMC->last_render;
    I810_MC(pI810XvMC,mc);
  }

  I810_UNLOCK(pI810XvMC);

  return Success;
}

/***************************************************************************
// Function: XvMCPutSurface
// Description:
// Arguments:
//  display: Connection to X server
//  surface: Surface to be displayed
//  draw: X Drawable on which to display the surface
//  srcx: X coordinate of the top left corner of the region to be
//          displayed within the surface.
//  srcy: Y coordinate of the top left corner of the region to be
//          displayed within the surface.
//  srcw: Width of the region to be displayed.
//  srch: Height of the region to be displayed.
//  destx: X cordinate of the top left corner of the destination region
//         in the drawable coordinates.
//  desty: Y cordinate of the top left corner of the destination region
//         in the drawable coordinates.
//  destw: Width of the destination region.
//  desth: Height of the destination region.
//  flags: One or more of the following.
//     XVMC_TOP_FIELD - Display only the Top field of the surface.
//     XVMC_BOTTOM_FIELD - Display only the Bottom Field of the surface.
//     XVMC_FRAME_PICTURE - Display both fields or frame.
//
// Info: Portions of this function derived from i810_video.c (XFree86)
//
//   This function is organized so that we wait as long as possible before
//   touching the overlay registers. Since we don't know that the last
//   flip has happened yet we want to give the overlay as long as
//   possible to catch up before we have to check on its progress. This
//   makes it unlikely that we have to wait on the last flip.
***************************************************************************/
Status XvMCPutSurface(Display *display,XvMCSurface *surface,
		      Drawable draw, short srcx, short srcy,
		      unsigned short srcw, unsigned short srch,
		      short destx, short desty,
		      unsigned short destw, unsigned short desth,
		      int flags) {
  i810XvMCContext *pI810XvMC;
  i810XvMCSurface *pI810Surface;
  i810OverlayRecPtr pORegs;
  unsigned int ysrc_offset,uvsrc_offset;
  Box extents;
  uint window_width,window_height;
  unsigned int xscaleInt = 0,xscaleFract = 0,yscaleInt = 0,yscaleFract = 0;
  unsigned int xscaleFractUV = 0,xscaleIntUV = 0,yscaleFractUV = 0;
  unsigned int yscaleIntUV = 0,yPitch = 0,uvPitch = 0;
  unsigned int ovcmd = 0;
  uint d;
  double xscale,yscale;
  int diff;
  int clipped_srcx, clipped_srcy, clipped_destx, clipped_desty;
  int clipped_srcw, clipped_srch, clipped_destw, clipped_desth;
  uint x1,y1,root_width,root_height;
  int x2 = 0, y2 = 0,unused;
  uint nChilds;
  int stat;
  Window win,root,parent,*pChilds;


  if((display == NULL) || (surface == NULL)) {
    return BadValue;
  }

  if(surface->privData == NULL) {
    return (error_base + XvMCBadSurface);
  }
  pI810Surface = (i810XvMCSurface *)surface->privData;
  pI810XvMC = (i810XvMCContext *)pI810Surface->privContext;
  pORegs = (i810OverlayRecPtr)pI810XvMC->oregs;


  switch(surface->surface_type_id) {
  case FOURCC_YV12:
  case FOURCC_I420:
    yPitch = (srcw + 7) & ~7;
    uvPitch = ((srcw>>1) + 7) & ~7;
    if((flags & XVMC_FRAME_PICTURE) != XVMC_FRAME_PICTURE) {
      srch = srch>>1;
    }
    break;
  case FOURCC_UYVY:
  case FOURCC_YUY2:
  default:
    /* FIXME: Non Planar not fully implemented. */
    return BadValue;
    yPitch = ((srcw + 7) & ~7) << 1;
    break;
  }/* switch(surface->surface_type_id) */

  /*
    FIXME: This should be using the DRI's clip rect but that isn't
    all hooked up yet. This has some latency but we get by.
  */
  win = draw;
  XQueryTree(display,win,&root,&parent,&pChilds,&nChilds);
  if(nChilds) XFree(pChilds);
  XGetGeometry(display,win, &root, &x2, &y2, &window_width,
	       &window_height, &d, &d);
  x1 = x2;
  y1 = y2;
  win = parent;
  do {
    XQueryTree(display,win,&root,&parent,&pChilds,&nChilds);
    if(nChilds) XFree(pChilds);
    XGetGeometry(display,win, &root, &x2, &y2, &d, &d, &d, &d);
    x1 += x2;
    y1 += y2;
    win = parent;
  }while(win != root);
  XGetGeometry(display,root, &root, &unused, &unused,
               &root_width, &root_height, &d, &d);

  /* Left edge of Video window clipped to screen */
  extents.x1 = 0;
  if(x1 > extents.x1) {
    extents.x1 = x1;
  }
  /* Right edge of Video window clipped to screen */
  extents.x2 = root_width;
  if(extents.x2 > (x1 + window_width)) {
    extents.x2 = x1 + window_width;
  }
  /* Top edge of Video window clipped to screen */
  extents.y1 = 0;
  if(y1 > extents.y1) {
    extents.y1 = y1;
  }
  /* Bottom edge of Video window clipped to screen */
  extents.y2 = root_height;
  if(extents.y2 > (y1 + window_height)) {
    extents.y2 = y1 + window_height;
  }

  /*
    Clipping is more difficult than is seems. We need to keep the
    scaling factors even if the destination window needs to be clipped.
    We clip the destination window first then apply a scaled version
    to the source window.
  */

  /* Put destination coords in screen coords */
  destx += x1;
  desty += y1;

  /* Scale factors requested */
  xscale = (double)srcw / (double)destw;
  yscale = (double)srch / (double)desth;

  /*
    If destination window needs to be clipped we actually adjust both
    the src and dest window so as to keep the scaling that was requested
  */
  clipped_srcx = srcx;
  clipped_srcy = srcy;
  clipped_destx = destx;
  clipped_desty = desty;
  clipped_srcw = srcw;
  clipped_srch = srch;
  clipped_destw = destw;
  clipped_desth = desth;

  /* Clip to the source surface boundaries */
  if(clipped_srcx < 0) {
    clipped_destx += (0 - clipped_srcx) / xscale;
    clipped_srcw -= clipped_srcx;
    clipped_destw -= clipped_srcx / xscale;
    clipped_srcx = 0;
  }
  if((clipped_srcw + clipped_srcx) > surface->width) {
    clipped_srcw = surface->width - clipped_srcx;
    clipped_destw -= (clipped_srcw - srcw) / xscale;
  }
  if(clipped_srcy < 0) {
    clipped_desty += (0 - clipped_srcy) / yscale;
    clipped_srch -= clipped_srcy;
    clipped_desth -= clipped_srcy / yscale;
    clipped_srcy = 0;
  }
  if((clipped_srch + clipped_srcy) > surface->height) {
    clipped_srch = surface->height - clipped_srcy;
    clipped_desth -= (clipped_srch - srch) / yscale;
  }

  /* Clip to the extents */
  if(clipped_destx < extents.x1) {
    diff = extents.x1 - clipped_destx;
    clipped_srcx += diff * xscale;
    clipped_srcw -= diff * xscale;
    clipped_destw -= diff;
    clipped_destx = extents.x1;
  }

  diff = (clipped_destx + clipped_destw) - extents.x2;
  if(diff > 0) {
    clipped_destw -= diff;
    clipped_srcw -= diff * xscale;
  }

  if(clipped_desty < extents.y1) {
    diff = extents.y1 - clipped_desty;
    clipped_srcy += diff * yscale;
    clipped_srch -= diff * yscale;
    clipped_desth -= diff;
    clipped_desty = 0;
  }

  diff = (clipped_desty + clipped_desth) - extents.y2;
  if(diff > 0) {
    clipped_desth -= diff;
    clipped_srch -= diff * yscale;
  }

  /* If the whole window is clipped turn off the overlay */
  if((clipped_destx + clipped_destw < extents.x1) ||
     (clipped_desty + clipped_desth < extents.y1) ||
     (clipped_destx > extents.x2) ||
     (clipped_desty > extents.y2)) {
    return XvMCHideSurface(display, surface);
  }

  /*
    Adjust the source offset width and height according to the clipped
    destination window.
  */
  ysrc_offset = ((clipped_srcx + 1) & ~1) +
    ((clipped_srcy + 1) & ~1) * (1<<pI810Surface->pitch);
  uvsrc_offset = (clipped_srcx>>1) +
    (clipped_srcy>>1) * (1<<(pI810Surface->pitch - 1));

  /*
    Initially, YCbCr and Overlay Enable and
    vertical chrominance up interpolation and horozontal chrominance
    up interpolation
  */
  ovcmd = VC_UP_INTERPOLATION | HC_UP_INTERPOLATION |
    Y_ADJUST | OVERLAY_ENABLE;

  if ((clipped_destw != clipped_srcw) ||
      (clipped_desth != clipped_srch)) {
    xscaleInt = (clipped_srcw / clipped_destw) & 0x3;
    xscaleFract = (clipped_srcw << 12) / clipped_destw;
    yscaleInt = (clipped_srch / clipped_desth) & 0x3;
    yscaleFract = (clipped_srch << 12) / clipped_desth;
   
    if (clipped_destw > clipped_srcw) {
      /* horizontal up-scaling */
      ovcmd &= ~HORIZONTAL_CHROMINANCE_FILTER;
      ovcmd &= ~HORIZONTAL_LUMINANCE_FILTER;
      ovcmd |= (HC_UP_INTERPOLATION | HL_UP_INTERPOLATION);
    }

    if (clipped_desth > clipped_srch) {
      /* vertical up-scaling */
      ovcmd &= ~VERTICAL_CHROMINANCE_FILTER;
      ovcmd &= ~VERTICAL_LUMINANCE_FILTER;
      ovcmd |= (VC_UP_INTERPOLATION | VL_UP_INTERPOLATION);
    }

    if (clipped_destw < clipped_srcw) {
      /* horizontal down-scaling */
      ovcmd &= ~HORIZONTAL_CHROMINANCE_FILTER;
      ovcmd &= ~HORIZONTAL_LUMINANCE_FILTER;
      ovcmd |= (HC_DOWN_INTERPOLATION | HL_DOWN_INTERPOLATION);
    }

    if (clipped_desth < clipped_srch) {
      /* vertical down-scaling */
      ovcmd &= ~VERTICAL_CHROMINANCE_FILTER;
      ovcmd &= ~VERTICAL_LUMINANCE_FILTER;
      ovcmd |= (VC_DOWN_INTERPOLATION | VL_DOWN_INTERPOLATION);
    }

    /* now calculate the UV scaling factor */
    if (xscaleFract) {
      xscaleFractUV = xscaleFract >> MINUV_SCALE;
      ovcmd &= ~HC_DOWN_INTERPOLATION;
      ovcmd |= HC_UP_INTERPOLATION;
    }

    if (xscaleInt) {
      xscaleIntUV = xscaleInt >> MINUV_SCALE;
      if (xscaleIntUV) {
	ovcmd &= ~HC_UP_INTERPOLATION;
      }
    }

    if (yscaleFract) {
      yscaleFractUV = yscaleFract >> MINUV_SCALE;
      ovcmd &= ~VC_DOWN_INTERPOLATION;
      ovcmd |= VC_UP_INTERPOLATION;
    }

    if (yscaleInt) {
      yscaleIntUV = yscaleInt >> MINUV_SCALE;
      if (yscaleIntUV) {
	ovcmd &= ~VC_UP_INTERPOLATION;
	ovcmd |= VC_DOWN_INTERPOLATION;
      }
    }

  }/* if((destw != srcw) || (desth != srch)) */

  /* Lock the DRM */
  I810_LOCK(pI810XvMC,0);

  /* Block until rendering on this surface is finished */
  stat = XVMC_RENDERING;
  while(stat & XVMC_RENDERING) {
    XvMCGetSurfaceStatus(display,surface,&stat);
  }
  /* Block until the last flip is finished */
  if(pI810XvMC->last_flip) {
    BLOCK_OVERLAY(pI810XvMC,pI810XvMC->current);
  }

  pI810XvMC->current = !pI810XvMC->current;
  pORegs->OV0CMD = ovcmd;

  if ((clipped_destw != clipped_srcw) ||
      (clipped_desth != clipped_srch)) {
    pORegs->YRGBSCALE = (xscaleInt << 15) |
      ((xscaleFract & 0xFFF) << 3) | (yscaleInt) |
      ((yscaleFract & 0xFFF) << 20);

    pORegs->UVSCALE = yscaleIntUV | ((xscaleFractUV & 0xFFF) << 3) |
      ((yscaleFractUV & 0xFFF) << 20);
  }
  else {
    /* Normal 1:1 scaling */
    pORegs->YRGBSCALE = 0x80004000;
    pORegs->UVSCALE = 0x80004000;
  }

  pORegs->SHEIGHT = clipped_srch | (clipped_srch << 15);
  pORegs->DWINPOS = (clipped_desty << 16) | clipped_destx;
  pORegs->DWINSZ = ((clipped_desth<< 16) | (clipped_destw));

  /* Attributes */
  pORegs->OV0CLRC0 = ((pI810XvMC->contrast & 0x1ff)<<8) |
    (pI810XvMC->brightness & 0xff);
  pORegs->OV0CLRC1 = (pI810XvMC->saturation & 0x3ff);
  
  /* Destination Colorkey Setup */
  pI810XvMC->oregs->DCLRKV = RGB16ToColorKey(pI810XvMC->colorkey);

  /* buffer locations, add the offset from the clipping */
  if(pI810XvMC->current) {
    pORegs->OBUF_1Y = (unsigned long)pI810Surface->offset +
      (unsigned long)pI810Surface->offsets[0] + ysrc_offset;
    pORegs->OBUF_1V = (unsigned long)pI810Surface->offset +
      (unsigned long)pI810Surface->offsets[2] + uvsrc_offset;
    pORegs->OBUF_1U = (unsigned long)pI810Surface->offset +
      (unsigned long)pI810Surface->offsets[1] + uvsrc_offset;
  }
  else {
    pORegs->OBUF_0Y = (unsigned long)pI810Surface->offset +
      (unsigned long)pI810Surface->offsets[0] + ysrc_offset;
    pORegs->OBUF_0V = (unsigned long)pI810Surface->offset +
      (unsigned long)pI810Surface->offsets[2] + uvsrc_offset;
    pORegs->OBUF_0U = (unsigned long)pI810Surface->offset +
      (unsigned long)pI810Surface->offsets[1] + uvsrc_offset;
  }

  switch(surface->surface_type_id) {
  case FOURCC_YV12:
  case FOURCC_I420:
    pORegs->SWID = (uvPitch << 16) | yPitch;
    pORegs->SWIDQW = (uvPitch << 13) | (yPitch >> 3);
    pORegs->OV0STRIDE = (1<<pI810Surface->pitch) | 
      ((1<<pI810Surface->pitch) << 15);
    pORegs->OV0CMD &= ~SOURCE_FORMAT;
    pORegs->OV0CMD |= YUV_420;
    if((flags & XVMC_FRAME_PICTURE) != XVMC_FRAME_PICTURE) {
      /* Top Field Only */
      if(flags & XVMC_TOP_FIELD) {
	if(pI810XvMC->current == 1) {
	  pORegs->OV0CMD |= (VERTICAL_PHASE_BOTH | FLIP_TYPE_FIELD |
			     BUFFER1_FIELD0);
	}
	else {
	  pORegs->OV0CMD |= (VERTICAL_PHASE_BOTH | FLIP_TYPE_FIELD |
			     BUFFER0_FIELD0);
	}
	pORegs->YRGB_VPH = 1<<15 | 1<<31;
	pORegs->UV_VPH = 3<<14 | 3<<30;
	pORegs->INIT_PH = 0x06 | 0x18;
      }
      /* Bottom Field Only */
      else {
	if(pI810XvMC->current == 1) {
	  pORegs->OV0CMD |= (VERTICAL_PHASE_BOTH | FLIP_TYPE_FIELD |
			     BUFFER1_FIELD1);
	}
	else {
	  pORegs->OV0CMD |= (VERTICAL_PHASE_BOTH | FLIP_TYPE_FIELD |
			     BUFFER0_FIELD1);
	}
	pORegs->YRGB_VPH = 0;
	pORegs->UV_VPH = 7<<29 | 7<<13;
	pORegs->INIT_PH = 0x06;
      }
    }
    /* Frame Picture */
    else {
      if(pI810XvMC->current == 1) {
	pORegs->OV0CMD |= BUFFER1_FIELD0;
      }
      else {
	pORegs->OV0CMD |= BUFFER0_FIELD0;
      }
      pORegs->YRGB_VPH = 0;
      pORegs->UV_VPH = 0;
      pORegs->INIT_PH = 0;
    }
    break;
  case FOURCC_UYVY:
  case FOURCC_YUY2:
  default:
    pORegs->SWID = srcw;
    pORegs->SWIDQW = srcw >> 3;
    pORegs->OV0STRIDE = pI810Surface->pitch;
    pORegs->OV0CMD &= ~SOURCE_FORMAT;
    pORegs->OV0CMD |= YUV_422;
    pORegs->OV0CMD &= ~OV_BYTE_ORDER;
    if (surface->surface_type_id == FOURCC_UYVY) {
      pORegs->OV0CMD |= Y_SWAP;
    }

    pORegs->OV0CMD &= ~BUFFER_AND_FIELD;
    if(pI810XvMC->current == 1) {
      pORegs->OV0CMD |= BUFFER1_FIELD0;
    }
    else {
      pORegs->OV0CMD |= BUFFER0_FIELD0;
    }

    break;
  } /* switch(surface->surface_type_id) */
  


  OVERLAY_FLIP(pI810XvMC);

  /*
    The Overlay only flips when it knows you changed
    something. So the first time change stuff while it
    is watching to be sure.
  */
  if(!pI810XvMC->last_flip) {
    pORegs->OV0CMD &= ~0x4;
    if(pI810XvMC->current == 1) {
      pORegs->OV0CMD |= BUFFER1_FIELD0;
    }
    else {
      pORegs->OV0CMD |= BUFFER0_FIELD0;
    }
  }
  pI810Surface->last_flip = ++pI810XvMC->last_flip;
  I810_UNLOCK(pI810XvMC);

  return Success;
}

/***************************************************************************
// Function: XvMCSyncSurface
// Arguments:
//   display - Connection to the X server
//   surface - The surface to synchronize
// Info:
// Returns: Status
***************************************************************************/
Status XvMCSyncSurface(Display *display,XvMCSurface *surface) {
  Status ret;
  int stat=0;
  /*
    FIXME: Perhaps a timer here to prevent lockup?
    FIXME: Perhaps a usleep to not be busy waiting?
  */
  do {
    ret = XvMCGetSurfaceStatus(display,surface,&stat);
  }while(!ret && (stat & XVMC_RENDERING));
  return ret;
}

/***************************************************************************
// Function: XvMCFlushSurface
// Description:
//   This function commits pending rendering requests to ensure that they
//   wll be completed in a finite amount of time.
// Arguments:
//   display - Connection to X server
//   surface - Surface to flush
// Info:
//   This command is a noop for i810 becuase we always dispatch buffers in
//   render. There is little gain to be had with 4k buffers.
// Returns: Status
***************************************************************************/
Status XvMCFlushSurface(Display * display, XvMCSurface *surface) {
  return Success;
}

/***************************************************************************
// Function: XvMCGetSurfaceStatus
// Description:
// Arguments:
//  display: connection to X server
//  surface: The surface to query
//  stat: One of the Following
//    XVMC_RENDERING - The last XvMCRenderSurface command has not
//                     completed.
//    XVMC_DISPLAYING - The surface is currently being displayed or a
//                     display is pending.
***************************************************************************/
Status XvMCGetSurfaceStatus(Display *display, XvMCSurface *surface,
			    int *stat) {
  i810XvMCSurface *privSurface;
  i810XvMCContext *pI810XvMC;
  int temp;

  if((display == NULL) || (surface == NULL) || (stat == NULL)) {
    return BadValue;
  }
  if(surface->privData == NULL) {
    return BadValue;
  }
  *stat = 0;
  privSurface = surface->privData;

  pI810XvMC = privSurface->privContext;
  if(pI810XvMC == NULL) {
    return (error_base + XvMCBadSurface);
  }

  I810_LOCK(pI810XvMC,0);
  if(privSurface->last_flip) {
    /* This can not happen */
    if(pI810XvMC->last_flip < privSurface->last_flip) {
      printf("Error: Context last flip is less than surface last flip.\n");
      return BadValue;
    }
    /*
      If the context has 2 or more flips after this surface it
      cannot be displaying. Don't bother to check.
    */
    if(!(pI810XvMC->last_flip > (privSurface->last_flip + 1))) {
      /*
	If this surface was the last flipped it is either displaying
	or about to be so don't bother checking.
      */
      if(pI810XvMC->last_flip == privSurface->last_flip) {
	*stat |= XVMC_DISPLAYING;
      }
      else {
	/*
	  In this case there has been one more flip since our surface's
	  but we need to check if it is finished or not.
	*/
	temp = GET_FSTATUS(pI810XvMC);
	if(((temp & (1<<20))>>20) != pI810XvMC->current) { 
	  *stat |= XVMC_DISPLAYING;
	}
      }
    }
  }

  if(privSurface->last_render &&
      (privSurface->last_render > GET_RSTATUS(pI810XvMC))) {
    *stat |= XVMC_RENDERING;
  }
  I810_UNLOCK(pI810XvMC);

  return Success;
}

/***************************************************************************
// 
//  Surface manipulation functions
//
***************************************************************************/

/***************************************************************************
// Function: XvMCHideSurface
// Description: Stops the display of a surface.
// Arguments:
//   display - Connection to the X server.
//   surface - surface to be hidden.
//
// Returns: Status
***************************************************************************/
Status XvMCHideSurface(Display *display, XvMCSurface *surface) {
  i810XvMCSurface *pI810Surface;
  i810XvMCContext *pI810XvMC;
  int ss, xx;

  /* Did we get a good display and surface passed into us? */
  if(display == NULL) {
    return BadValue;
  }

  if(surface == NULL) {
    return (error_base + XvMCBadSurface);
  }

  XvMCSyncSurface(display, surface);

  /* Get surface private data pointer */
  if(surface->privData == NULL) {
    return (error_base + XvMCBadSurface);
  }
  pI810Surface = (i810XvMCSurface *)surface->privData;

  /*
    Get the status of the surface, if it is not currently displayed
    we don't need to worry about it.
  */
  if((xx = XvMCGetSurfaceStatus(display, surface, &ss)) != Success) {
    return xx;
  }
  if(! (ss & XVMC_DISPLAYING)) {
    return Success;
  }
 
  /* Get the associated context pointer */
  pI810XvMC = (i810XvMCContext *)pI810Surface->privContext;
  if(pI810XvMC == NULL) {
    return (error_base + XvMCBadSurface);
  }

  if(pI810XvMC->last_flip) {
    I810_LOCK(pI810XvMC,DRM_LOCK_QUIESCENT);
    
    /* Make sure last flip is done */
    BLOCK_OVERLAY(pI810XvMC,pI810XvMC->current);

    /* Set the registers to turn the overlay off */
    pI810XvMC->oregs->OV0CMD = VC_UP_INTERPOLATION | HC_UP_INTERPOLATION |
      Y_ADJUST;
    pI810XvMC->current = !pI810XvMC->current;
    if(pI810XvMC->current == 1) {
      pI810XvMC->oregs->OV0CMD |= BUFFER1_FIELD0;
    }
    else {
      pI810XvMC->oregs->OV0CMD |= BUFFER0_FIELD0;
    }
    OVERLAY_FLIP(pI810XvMC);
    /*
      Increment the context flip but not the surface. This way no
      surface has the last flip #.
    */
    pI810XvMC->last_flip++;


    /* Now wait until the hardware reads the registers and makes the change. */
    BLOCK_OVERLAY(pI810XvMC,pI810XvMC->current)

      I810_UNLOCK(pI810XvMC);
  }

  return Success;
}




/***************************************************************************
//
// Functions that deal with subpictures
//
***************************************************************************/



/***************************************************************************
// Function: XvMCCreateSubpicture
// Description: This creates a subpicture by filling out the XvMCSubpicture
//              structure passed to it and returning Success.
// Arguments:
//   display - Connection to the X server.
//   context - The context to create the subpicture for.
//   subpicture - Pre-allocated XvMCSubpicture structure to be filled in.
//   width - of subpicture
//   height - of subpicture
//   xvimage_id - The id describing the XvImage format.
//
// Returns: Status
***************************************************************************/
Status XvMCCreateSubpicture(Display *display, XvMCContext *context,
                          XvMCSubpicture *subpicture,
                          unsigned short width, unsigned short height,
                          int xvimage_id) {
  i810XvMCContext *pI810XvMC;
  i810XvMCSubpicture *pI810Subpicture;
  int priv_count;
  uint *priv_data;
  Status ret;

  if((subpicture == NULL) || (context == NULL) || (display == NULL)){
    return BadValue;
  }
  
  pI810XvMC = (i810XvMCContext *)context->privData;
  if(pI810XvMC == NULL) {
    return (error_base + XvMCBadContext);
  }


  subpicture->context_id = context->context_id;
  subpicture->xvimage_id = xvimage_id;
  
  /* These need to be checked to make sure they are not too big! */
  subpicture->width = width;
  subpicture->height = height;

  subpicture->privData =
    (i810XvMCSubpicture *)malloc(sizeof(i810XvMCSubpicture));

  if(!subpicture->privData) {
    return BadAlloc;
  }
  pI810Subpicture = (i810XvMCSubpicture *)subpicture->privData;


  if((ret = _xvmc_create_subpicture(display, context, subpicture,
				    &priv_count, &priv_data))) {
    printf("Unable to create XvMCSubpicture.\n");
    return ret;
  }

  if(priv_count != 1) {
    printf("_xvmc_create_subpicture() returned incorrect data size.\n");
    printf("Expected 1 got %d\n",priv_count);
    free(priv_data);
    return BadAlloc;
  }
  /* Data == Client Address, offset == Physical address offset */
  pI810Subpicture->data = pI810XvMC->surfaces.address;
  pI810Subpicture->offset = pI810XvMC->surfaces.offset;

  /* Initialize private values */
  pI810Subpicture->privContext = pI810XvMC;

  pI810Subpicture->last_render = 0;
  pI810Subpicture->last_flip = 0;

  /* Based on the xvimage_id we will need to set the other values */
  subpicture->num_palette_entries = 16;
  subpicture->entry_bytes = 3;
  strcpy(subpicture->component_order,"YUV");

  /*
    i810's MC Engine needs surfaces of 2^x (x= 9,10,11,12) pitch
    and the Tiler need 512k aligned surfaces, basically we are
    stuck with fixed memory with pitch 1024.
  */
  pI810Subpicture->pitch = 10;

  /* 
     offsets[0] == offset into the map described by either
     address (Client memeory address) or offset (physical offset from fb base)
  */
  pI810Subpicture->offsets[0] = priv_data[0];
  if(((unsigned long)pI810Subpicture->data + pI810Subpicture->offsets[0]) & 4095) {
    printf("XvMCCreateSubpicture: Subpicture offset 0 is not 4096 aligned\n");
  }

  /* Free data returned from xvmc_create_surface */
  free(priv_data);

  /* Clear the surface to 0 */
  memset((void *)((unsigned long)pI810Subpicture->data + (unsigned long)pI810Subpicture->offsets[0]),
	 0, ((1<<pI810Subpicture->pitch) * subpicture->height));

  switch(subpicture->xvimage_id) {
  case FOURCC_IA44:
  case FOURCC_AI44:
    /* Destination buffer info command */
    pI810Subpicture->dbi1 = ((((unsigned int)pI810Subpicture->offset +
			       pI810Subpicture->offsets[0]) & ~0xfc000fff) |
			     (pI810Subpicture->pitch - 9));
    
    /* Destination buffer variables command */
    pI810Subpicture->dbv1 = (0x8<<20) | (0x8<<16);

    /* Map info command */
    pI810Subpicture->mi1 = (0x0<<24) | (3<<21) | (1<<9) |
      (pI810Subpicture->pitch - 3);

    pI810Subpicture->mi2 = (((unsigned int)subpicture->height - 1)<<16) |
      ((unsigned int)subpicture->width - 1);

    pI810Subpicture->mi3 = ((unsigned int)pI810Subpicture->offset +
			 pI810Subpicture->offsets[0]) & ~0xfc00000f;
    break;
  default:
    free(subpicture->privData);
    return BadMatch;
  }

  pI810XvMC->ref++;
  return Success;
}



/***************************************************************************
// Function: XvMCClearSubpicture
// Description: Clear the area of the given subpicture to "color".
//              structure passed to it and returning Success.
// Arguments:
//   display - Connection to the X server.
//   subpicture - Subpicture to clear.
//   x, y, width, height - rectangle in the subpicture to clear.
//   color - The data to file the rectangle with.
//
// Returns: Status
***************************************************************************/
Status XvMCClearSubpicture(Display *display, XvMCSubpicture *subpicture,
                          short x, short y,
                          unsigned short width, unsigned short height,
                          unsigned int color) {

  i810XvMCContext *pI810XvMC;
  i810XvMCSubpicture *pI810Subpicture;
  int i;

  if((subpicture == NULL) || (display == NULL)){
    return BadValue;
  }

  if(!subpicture->privData) {
    return (error_base + XvMCBadSubpicture);
  }
  pI810Subpicture = (i810XvMCSubpicture *)subpicture->privData;

  pI810XvMC = (i810XvMCContext *)pI810Subpicture->privContext;
  if(pI810XvMC == NULL) {
    return (error_base + XvMCBadSubpicture);
  }

  if((x < 0) || (x + width > subpicture->width)) {
    return BadValue;
  }

  if((y < 0) || (y + height > subpicture->height)) {
    return BadValue;
  }

  for(i=y; i<y + height; i++) {
    memset((void *)((unsigned long)pI810Subpicture->data +
		    (unsigned long)pI810Subpicture->offsets[0] + x +
		    (1<<pI810Subpicture->pitch) * i),(char)color,width);
  }

  return Success;
}

/***************************************************************************
// Function: XvMCCompositeSubpicture
// Description: Composite the XvImae on the subpicture. This composit uses
//              non-premultiplied alpha. Destination alpha is utilized
//              except for with indexed subpictures. Indexed subpictures
//              use a simple "replace".
// Arguments:
//   display - Connection to the X server.
//   subpicture - Subpicture to clear.
//   image - the XvImage to be used as the source of the composite.
//   srcx, srcy, width, height - The rectangle from the image to be used.
//   dstx, dsty - location in the subpicture to composite the source.
//
// Returns: Status
***************************************************************************/
Status XvMCCompositeSubpicture(Display *display, XvMCSubpicture *subpicture,
                               XvImage *image,
                               short srcx, short srcy,
                               unsigned short width, unsigned short height,
                               short dstx, short dsty) {

  i810XvMCContext *pI810XvMC;
  i810XvMCSubpicture *pI810Subpicture;
  int i;

  if((subpicture == NULL) || (display == NULL)){
    return BadValue;
  }

  if(!subpicture->privData) {
    return (error_base + XvMCBadSubpicture);
  }
  pI810Subpicture = (i810XvMCSubpicture *)subpicture->privData;

  pI810XvMC = (i810XvMCContext *)pI810Subpicture->privContext;
  if(pI810XvMC == NULL) {
    return (error_base + XvMCBadSubpicture);
  }

  if((srcx < 0) || (srcx + width > image->width)) {
    return BadValue;
  }

  if((dstx < 0) || (dstx + width > subpicture->width)) {
    return BadValue;
  }

  if((srcy < 0) || (srcy + height > image->height)) {
    return BadValue;
  }

  if((dsty < 0) || (dsty + height > subpicture->height)) {
    return BadValue;
  }

  for(i=0; i<height; i++) {
    memcpy((void *)((unsigned long)pI810Subpicture->data +
		    (unsigned long)pI810Subpicture->offsets[0] + dstx +
		    (1<<pI810Subpicture->pitch) * (i + dsty)),
	   (void *)((unsigned long)image->data +
		    (unsigned long)image->offsets[0] + srcx +
		    image->pitches[0] * (i + srcy))
	   ,width);
  }

  return Success;

}


/***************************************************************************
// Function: XvMCDestroySubpicture
// Description: Destroys the specified subpicture.
// Arguments:
//   display - Connection to the X server.
//   subpicture - Subpicture to be destroyed.
//
// Returns: Status
***************************************************************************/
Status XvMCDestroySubpicture(Display *display, XvMCSubpicture *subpicture) {

  i810XvMCSubpicture *pI810Subpicture;
  i810XvMCContext *pI810XvMC;

  if((display == NULL) || (subpicture == NULL)) {
    return BadValue;
  }
  if(!subpicture->privData) {
    return (error_base + XvMCBadSubpicture);
  }
  pI810Subpicture = (i810XvMCSubpicture *)subpicture->privData;

  pI810XvMC = (i810XvMCContext *)pI810Subpicture->privContext;
  if(!pI810XvMC) {
    return (error_base + XvMCBadSubpicture);
  }


  if(pI810Subpicture->last_render) {
    XvMCSyncSubpicture(display,subpicture);
  }

  _xvmc_destroy_subpicture(display,subpicture);

  i810_free_privContext(pI810XvMC);

  free(pI810Subpicture);
  subpicture->privData = NULL;
  return Success;
}


/***************************************************************************
// Function: XvMCSetSubpicturePalette
// Description: Set the subpictures palette
// Arguments:
//   display - Connection to the X server.
//   subpicture - Subpiture to set palette for.
//   palette - A pointer to an array holding the palette data. The array
//     is num_palette_entries * entry_bytes in size.
// Returns: Status
***************************************************************************/

Status XvMCSetSubpicturePalette(Display *display, XvMCSubpicture *subpicture,
				unsigned char *palette) {
 i810XvMCSubpicture *privSubpicture;
 int i,j;

 if((display == NULL) || (subpicture == NULL)) {
   return BadValue;
 }
 if(subpicture->privData == NULL) {
   return (error_base + XvMCBadSubpicture);
 }
 privSubpicture = (i810XvMCSubpicture *)subpicture->privData;

 j=0;
 for(i=0; i<16; i++) {
   privSubpicture->palette[0][i] = palette[j++];
   privSubpicture->palette[1][i] = palette[j++];
   privSubpicture->palette[2][i] = palette[j++];
 }
 return Success;
}

/***************************************************************************
// Function: XvMCBlendSubpicture
// Description: 
//    The behavior of this function is different depending on whether
//    or not the XVMC_BACKEND_SUBPICTURE flag is set in the XvMCSurfaceInfo.
//    i810 only support frontend behavior.
//  
//    XVMC_BACKEND_SUBPICTURE not set ("frontend" behavior):
//   
//    XvMCBlendSubpicture is a no-op in this case.
//   
// Arguments:
//   display - Connection to the X server.
//   subpicture - The subpicture to be blended into the video.
//   target_surface - The surface to be displayed with the blended subpic.
//   source_surface - Source surface prior to blending.
//   subx, suby, subw, subh - The rectangle from the subpicture to use.
//   surfx, surfy, surfw, surfh - The rectangle in the surface to blend
//      blend the subpicture rectangle into. Scaling can ocure if 
//      XVMC_SUBPICTURE_INDEPENDENT_SCALING is set.
//
// Returns: Status
***************************************************************************/
Status XvMCBlendSubpicture(Display *display, XvMCSurface *target_surface,
                         XvMCSubpicture *subpicture,
                         short subx, short suby,
                         unsigned short subw, unsigned short subh,
                         short surfx, short surfy,
                         unsigned short surfw, unsigned short surfh) {

  return BadMatch;
}



/***************************************************************************
// Function: XvMCBlendSubpicture2
// Description: 
//    The behavior of this function is different depending on whether
//    or not the XVMC_BACKEND_SUBPICTURE flag is set in the XvMCSurfaceInfo.
//    i810 only supports frontend blending.
//  
//    XVMC_BACKEND_SUBPICTURE not set ("frontend" behavior):
//   
//    XvMCBlendSubpicture2 blends the source_surface and subpicture and
//    puts it in the target_surface.  This does not effect the status of
//    the source surface but will cause the target_surface to query
//    XVMC_RENDERING until the blend is completed.
//   
// Arguments:
//   display - Connection to the X server.
//   subpicture - The subpicture to be blended into the video.
//   target_surface - The surface to be displayed with the blended subpic.
//   source_surface - Source surface prior to blending.
//   subx, suby, subw, subh - The rectangle from the subpicture to use.
//   surfx, surfy, surfw, surfh - The rectangle in the surface to blend
//      blend the subpicture rectangle into. Scaling can ocure if 
//      XVMC_SUBPICTURE_INDEPENDENT_SCALING is set.
//
// Returns: Status
***************************************************************************/
Status XvMCBlendSubpicture2(Display *display, 
                          XvMCSurface *source_surface,
                          XvMCSurface *target_surface,
                          XvMCSubpicture *subpicture,
                          short subx, short suby,
                          unsigned short subw, unsigned short subh,
                          short surfx, short surfy,
                          unsigned short surfw, unsigned short surfh) {
  drmBufPtr pDMA;
  unsigned int *data;
  i810XvMCContext *pI810XvMC;
  i810XvMCSubpicture *privSubpicture;
  i810XvMCSurface *privTarget;
  i810XvMCSurface *privSource;
  drm_i810_mc_t mc;
  int i,j;

  if(display == NULL) {
    return BadValue;
  }

  if(subpicture == NULL) {
    return (error_base + XvMCBadSubpicture);
  }

  if((target_surface == NULL) || (source_surface == NULL)) {
    return (error_base + XvMCBadSurface);
  }

  if((subpicture->xvimage_id != FOURCC_AI44) &&
     (subpicture->xvimage_id != FOURCC_IA44)) {
      return (error_base + XvMCBadSubpicture);
  }

  if(!subpicture->privData) {
    return (error_base + XvMCBadSubpicture);
  }
  privSubpicture = (i810XvMCSubpicture *)subpicture->privData;

  pI810XvMC = (i810XvMCContext *)privSubpicture->privContext;
  if(pI810XvMC == NULL) {
    return (error_base + XvMCBadSubpicture);
  }

  if(!target_surface->privData) {
    return (error_base + XvMCBadSurface);
  }
  privTarget = (i810XvMCSurface *)target_surface->privData;
  
  if(!source_surface->privData) {
    return (error_base + XvMCBadSurface);
  }
  privSource = (i810XvMCSurface *)source_surface->privData;
 

  /* Check that size isn't bigger than subpicture */
  if((subx + subw) > subpicture->width) {
    return BadValue;
  }
  if((suby + subh) > subpicture->height) {
    return BadValue;
  }
  /* Check that dest isn't bigger than surface */
  if((surfx + surfw) > target_surface->width) {
    return BadValue;
  }
  if((surfy + surfh) > target_surface->height) {
    return BadValue;
  }
  /* Make sure surfaces match */
  if(target_surface->width != source_surface->width) {
    return BadValue;
  }
  if(target_surface->height != source_surface->height) {
    return BadValue;
  }

  /* Lock For DMA */
  I810_LOCK(pI810XvMC,0);

  /* Allocate DMA buffer */
  pDMA = i810_get_free_buffer(pI810XvMC);
  data = pDMA->address;

  /* Copy Y data first */
  /* SOURCE_COPY_BLT */
  *data++ = (2<<29) | (0x43<<22) | 0x4;
  *data++ = (0xcc<<16) | (1<<26) | (1<<privTarget->pitch);
  *data++ = (target_surface->height<<16) | target_surface->width;
  *data++ = privTarget->offset + privTarget->offsets[0];
  *data++ = (1<<privSource->pitch);
  *data++ = privSource->offset + privSource->offsets[0];

  /* Select Context 1 for loading */
  *data++ = CMD_FLUSH;
  *data++ = (5<<23) | (1<<17) | (1<<8);
  *data++ = CMD_FLUSH;

  /* Load Palette */
  *data++ = MAP_PALETTE_LOAD;
  /* 16 levels of alpha for each Y */
  switch(subpicture->xvimage_id) {
  case FOURCC_IA44:
    for(i=0; i<16; i++) {
      for(j=0; j<16; j++) {
        *data++ = (j<<12) | (j<<8) | privSubpicture->palette[0][i];
      }
    }
    break;
  case FOURCC_AI44:
    for(i=0; i<16; i++) {
      for(j=0; j<16; j++) {
        *data++ = (i<<12) | (i<<8) | privSubpicture->palette[0][j];
      }
    }
    break;
  }
  /* TARGET */
  /* *data++ = CMD_FLUSH; */
  /* *data++ = BOOLEAN_ENA_2; */
  *data++ = CMD_FLUSH;
  *data++ = DEST_BUFFER_INFO;
  *data++ = privTarget->dbi1y;
  *data++ = DEST_BUFFER_VAR;
  *data++ = privTarget->dbv1;

  /* ALPHA */
  *data++ = CMD_MAP_INFO;
  *data++ = privSubpicture->mi1;
  *data++ = privSubpicture->mi2;
  *data++ = privSubpicture->mi3;

  *data++ = VERTEX_FORMAT | (1<<8) | (3<<1);
  *data++ = BOOLEAN_ENA_1;
  *data++ = SRC_DEST_BLEND_MONO | (0x940);
  /* Map Filter */
  *data++ = (3<<29) | (0x1c<<24) | (2<<19) | (0x224);

  /* Use context 1 */
  *data++ = CMD_FLUSH;
  *data++ = (5<<23) | (1<<16) | 1;
  *data++ = CMD_FLUSH;

  /* Drawing Rect Info */
  *data++ = DRAWING_RECT_INFO;
  *data++ = 0x0;
  *data++ = 0x0;
  *data++ = 0x0;
  *data++ = 0x0;
  *data++ = 0x0;

  /* GFXPRIMITIVE RECTANGLE */
  *data++ = (3<<29) | (0x1f<<24) | (0x7<<18) | 11;
  /* Bottom Right Vertex */
  *(float *)data++ = (float) (surfx + surfw);
  *(float *)data++ = (float) (surfy + surfh);
  *(float *)data++ = (float) (subx + subw);
  *(float *)data++ = (float) (suby + subh);
  /* Bottom Left Vertex */
  *(float *)data++ = (float) surfx;
  *(float *)data++ = (float) (surfy + surfh);
  *(float *)data++ = (float) subx;
  *(float *)data++ = (float) (suby + subh);
  /* Top Left Vertex */
  *(float *)data++ = (float) surfx;
  *(float *)data++ = (float) surfy;
  *(float *)data++ = (float) subx;
  *(float *)data++ = (float) suby;

  /* Load and Use Context 0 */
  *data++ = CMD_FLUSH;
  *data++ = (5<<23) | (1<<17) | (1<<16);
  *data++ = CMD_FLUSH;

  /* U data */
  /* SOURCE_COPY_BLT */
  *data++ = (2<<29) | (0x43<<22) | 0x4;
  *data++ = (0xcc<<16) | (1<<26) | (1<<(privTarget->pitch - 1));
  *data++ = (target_surface->height<<15) | (target_surface->width>>1);
  *data++ = (unsigned long)privTarget->offset + (unsigned long)privTarget->offsets[1];
  *data++ = (1<<(privSource->pitch - 1));
  *data++ = (unsigned long)privSource->offset + (unsigned long)privSource->offsets[1];

  /* Context 1 select */
  *data++ = CMD_FLUSH;
  *data++ = (5<<23) | (1<<17) | (1<<8);
  *data++ = CMD_FLUSH;
  /* ALPHA PALETTE */
  *data++ = MAP_PALETTE_LOAD;
  /* 16 levels of alpha for each Y */
  switch(subpicture->xvimage_id) {
  case FOURCC_IA44:
    for(i=0; i<16; i++) {
      for(j=0; j<16; j++) {
        *data++ = (j<<12) | (j<<8) | privSubpicture->palette[2][i];
      }
    }
    break;
  case FOURCC_AI44:
    for(i=0; i<16; i++) {
      for(j=0; j<16; j++) {
        *data++ = (i<<12) | (i<<8) | privSubpicture->palette[2][j];
      }
    }
    break;
  }
  /* TARGET */
  *data++ = CMD_FLUSH;
  *data++ = BOOLEAN_ENA_2;
  *data++ = CMD_FLUSH;
  *data++ = DEST_BUFFER_INFO;
  *data++ = privTarget->dbi1u;
  *data++ = DEST_BUFFER_VAR;
  *data++ = privTarget->dbv1;

  /* ALPHA */
  *data++ = CMD_MAP_INFO;
  *data++ = privSubpicture->mi1;
  *data++ = privSubpicture->mi2;
  *data++ = privSubpicture->mi3;

  *data++ = VERTEX_FORMAT | (1<<8) | (3<<1);
  *data++ = BOOLEAN_ENA_1;
  *data++ = SRC_DEST_BLEND_MONO | (0x940);
  /* Map Filter */
  *data++ = (3<<29) | (0x1c<<24) | (2<<19) | (1<<16) | (0x224);

  /* Use context 1 */
  *data++ = CMD_FLUSH;
  *data++ = (5<<23) | (1<<16) | 1;
  *data++ = CMD_FLUSH;

  /* Drawing Rect Info */
  *data++ = (3<<29) | (0x1d<<24) | (0x80<<16) | 3;
  *data++ = 0;
  *data++ = 0;
  *data++ = 0;
  *data++ = 0;
  *data++ = 0;

  /* Rectangle */
  *data++ = (3<<29) | (0x1f<<24) | (0x7<<18) | 11;
  /* Bottom Right */
  *(float *)data++ = (float) ((surfx + surfw)>>1);
  *(float *)data++ = (float) ((surfy + surfh)>>1);
  *(float *)data++ = (float) subx + subw;
  *(float *)data++ = (float) suby + subh;
  /* Bottom Left */
  *(float *)data++ = (float) (surfx>>1);
  *(float *)data++ = (float) ((surfy + surfh)>>1);
  *(float *)data++ = (float) subx;
  *(float *)data++ = (float) suby + subh;
  /* Top Left */
  *(float *)data++ = (float) (surfx>>1);
  *(float *)data++ = (float) (surfy>>1);
  *(float *)data++ = (float) subx;
  *(float *)data++ = (float) suby;

  /* Load and Use Context 0 */
  *data++ = CMD_FLUSH;
  *data++ = (5<<23) | (1<<17) | (1<<16);
  *data++ = CMD_FLUSH;

  /* V data */
  /* SOURCE_COPY_BLT */
  *data++ = (2<<29) | (0x43<<22) | 0x4;
  *data++ = (0xcc<<16) | (1<<26) | (1<<(privTarget->pitch - 1));
  *data++ = (target_surface->height<<15) | (target_surface->width>>1);
  *data++ = (unsigned long)privTarget->offset + (unsigned long)privTarget->offsets[2];
  *data++ = (1<<(privSource->pitch - 1));
  *data++ = (unsigned long)privSource->offset + (unsigned long)privSource->offsets[2];

  /* Context 1 select */
  *data++ = CMD_FLUSH;
  *data++ = (5<<23) | (1<<17) | (1<<8);
  *data++ = CMD_FLUSH;

  /* ALPHA PALETTE */
  *data++ = MAP_PALETTE_LOAD;
  /* 16 levels of alpha for each Y */
  switch(subpicture->xvimage_id) {
  case FOURCC_IA44:
    for(i=0; i<16; i++) {
      for(j=0; j<16; j++) {
        *data++ = (j<<12) | (j<<8) | privSubpicture->palette[1][i];
      }
    }
    break;
  case FOURCC_AI44:
    for(i=0; i<16; i++) {
      for(j=0; j<16; j++) {
        *data++ = (i<<12) | (i<<8) | privSubpicture->palette[1][j];
      }
    }
    break;
  }
  /* TARGET */
  *data++ = CMD_FLUSH;
  *data++ = BOOLEAN_ENA_2;
  *data++ = CMD_FLUSH;
  *data++ = DEST_BUFFER_INFO;
  *data++ = privTarget->dbi1v;
  *data++ = DEST_BUFFER_VAR;
  *data++ = privTarget->dbv1;

  /* ALPHA */
  *data++ = CMD_MAP_INFO;
  *data++ = privSubpicture->mi1;
  *data++ = privSubpicture->mi2;
  *data++ = privSubpicture->mi3;

  *data++ = VERTEX_FORMAT | (1<<8) | (3<<1);
  *data++ = BOOLEAN_ENA_1;
  *data++ = SRC_DEST_BLEND_MONO | (0x940);
  /* Map Filter */
  *data++ = (3<<29) | (0x1c<<24) | (2<<19) | (1<<16) | (0x224);

  /* Use context 1 */
  *data++ = CMD_FLUSH;
  *data++ = (5<<23) | (1<<16) | 1;
  *data++ = CMD_FLUSH;

  /* Drawing Rect Info */
  *data++ = (3<<29) | (0x1d<<24) | (0x80<<16) | 3;
  *data++ = 0;
  *data++ = 0;
  *data++ = 0;
  *data++ = 0;
  *data++ = 0;

  /* Rectangle */
  *data++ = (3<<29) | (0x1f<<24) | (0x7<<18) | 11;
  /* Bottom Right */
  *(float *)data++ = (float) ((surfx + surfw)>>1);
  *(float *)data++ = (float) ((surfy + surfh)>>1);
  *(float *)data++ = (float) subx + subw;
  *(float *)data++ = (float) suby + subh;
  /* Bottom Left */
  *(float *)data++ = (float) (surfx>>1);
  *(float *)data++ = (float) ((surfy + surfh)>>1);
  *(float *)data++ = (float) subx;
  *(float *)data++ = (float) suby + subh;
  /* Top Left */
  *(float *)data++ = (float) (surfx>>1);
  *(float *)data++ = (float) (surfy>>1);
  *(float *)data++ = (float) subx;
  *(float *)data++ = (float) suby;

  /* Load and Use Context 0 */
  *data++ = CMD_FLUSH;
  *data++ = (5<<23) | (1<<17) | (1<<16);
  *data++ = CMD_FLUSH;


  /* Dispatch */
  pDMA->used = (unsigned long)data - (unsigned long)pDMA->address;
  mc.idx = pDMA->idx;
  mc.used = pDMA->used;
  mc.last_render = ++pI810XvMC->last_render;
  privTarget->last_render = pI810XvMC->last_render;
  I810_MC(pI810XvMC,mc);

  I810_UNLOCK(pI810XvMC);
  return Success;
}



/***************************************************************************
// Function: XvMCSyncSubpicture
// Description: This function blocks until all composite/clear requests on
//              the subpicture have been complete.
// Arguments:
//   display - Connection to the X server.
//   subpicture - The subpicture to synchronize
//
// Returns: Status
***************************************************************************/
Status XvMCSyncSubpicture(Display *display, XvMCSubpicture *subpicture) {
  Status ret;
  int stat=0;
  do {
    ret = XvMCGetSubpictureStatus(display,subpicture,&stat);
  }while(!ret && (stat & XVMC_RENDERING));
  return ret;
}



/***************************************************************************
// Function: XvMCFlushSubpicture
// Description: This function commits pending composite/clear requests to
//              ensure that they will be completed in a finite amount of
//              time.
// Arguments:
//   display - Connection to the X server.
//   subpicture - The subpicture whos compsiting should be flushed
//
// Returns: Status
// NOTES: i810 always dispatches commands so flush is a no-op
***************************************************************************/
Status XvMCFlushSubpicture(Display *display, XvMCSubpicture *subpicture) {
  if(display == NULL) {
    return BadValue;
  }
  if(subpicture == NULL) {
    return (error_base + XvMCBadSubpicture);
  }

  return Success;
}



/***************************************************************************
// Function: XvMCGetSubpictureStatus
// Description: This function gets the current status of a subpicture
//
// Arguments:
//   display - Connection to the X server.
//   subpicture - The subpicture whos status is being queried
//   stat - The status of the subpicture. It can be any of the following
//          OR'd together:
//          XVMC_RENDERING  - Last composite or clear request not completed
//          XVMC_DISPLAYING - Suppicture currently being displayed.
//
// Returns: Status
// Notes: i810 always blends into a third surface so the subpicture is
//  never actually displaying, only a copy of it is displaying. We only
//  have to worry about the rendering case.
***************************************************************************/
Status XvMCGetSubpictureStatus(Display *display, XvMCSubpicture *subpicture,
                             int *stat) {

  i810XvMCSubpicture *privSubpicture;
  i810XvMCContext *pI810XvMC;

  if((display == NULL) || (stat == NULL)) {
    return BadValue;
  }
  if((subpicture == NULL) || (subpicture->privData == NULL)) {
    return (error_base + XvMCBadSubpicture);
  }
  *stat = 0;
  privSubpicture = (i810XvMCSubpicture *)subpicture->privData;

  pI810XvMC = (i810XvMCContext *)privSubpicture->privContext;
  if(pI810XvMC == NULL) {
    return (error_base + XvMCBadSubpicture);
  }

  I810_LOCK(pI810XvMC,0);

  if(privSubpicture->last_render &&
      (privSubpicture->last_render > GET_RSTATUS(pI810XvMC))) {
    *stat |= XVMC_RENDERING;
  }
  I810_UNLOCK(pI810XvMC);

  return Success;
}

#define NUM_XVMC_ATTRIBUTES 4
static XvAttribute I810_XVMC_ATTRIBUTES[] = {
  {XvGettable | XvSettable, 0, 0xffffff, "XV_COLORKEY"},
  {XvGettable | XvSettable, -127, +127, "XV_BRIGHTNESS"},
  {XvGettable | XvSettable, 0, 0x1ff, "XV_CONTRAST"},
  {XvGettable | XvSettable, 0, 0x3ff, "XV_SATURATION"}
};


/***************************************************************************
// Function: XvMCQueryAttributes
// Description: An array of XvAttributes of size "number" is returned by
//   this function. If there are no attributes, NULL is returned and number
//   is set to 0. The array may be freed with xfree().
//
// Arguments:
//   display - Connection to the X server.
//   context - The context whos attributes we are querying.
//   number - The number of returned atoms.
//
// Returns:
//  An array of XvAttributes.
// Notes:
//   For i810 we support these Attributes:
//    XV_COLORKEY: The colorkey value, initialized from the Xv value at
//                 context creation time.
//    XV_BRIGHTNESS
//    XV_CONTRAST
//    XV_SATURATION
***************************************************************************/
XvAttribute *XvMCQueryAttributes(Display *display, XvMCContext *context,
				 int *number) {
  i810XvMCContext *pI810XvMC;
  XvAttribute *attributes;

  if(number == NULL) {
    return NULL;
  }
  if(display == NULL) {
    *number = 0;
    return NULL;
  }
  if(context == NULL) {
    *number = 0;
    return NULL;
  }
  pI810XvMC = context->privData;
  if(pI810XvMC == NULL) {
    *number = 0;
    return NULL;
  }

  attributes = (XvAttribute *)malloc(NUM_XVMC_ATTRIBUTES *
				     sizeof(XvAttribute));
  if(attributes == NULL) {
    *number = 0;
    return NULL;
  }

  memcpy(attributes,I810_XVMC_ATTRIBUTES,(NUM_XVMC_ATTRIBUTES *
					  sizeof(XvAttribute)));

  *number = NUM_XVMC_ATTRIBUTES;
  return attributes;
}

/***************************************************************************
// Function: XvMCSetAttribute
// Description: This function sets a context-specific attribute.
//
// Arguments:
//   display - Connection to the X server.
//   context - The context whos attributes we are querying.
//   attribute - The X atom of the attribute to be changed.
//   value - The new value for the attribute.
//
// Returns:
//  Status
// Notes:
//   For i810 we support these Attributes:
//    XV_COLORKEY: The colorkey value, initialized from the Xv value at
//                 context creation time.
//    XV_BRIGHTNESS
//    XV_CONTRAST
//    XV_SATURATION
***************************************************************************/
Status XvMCSetAttribute(Display *display, XvMCContext *context,
			Atom attribute, int value) {
  i810XvMCContext *pI810XvMC;

  if(display == NULL) {
    return BadValue;
  }
  if(context == NULL) {
    return (error_base + XvMCBadContext);
  }
  pI810XvMC = context->privData;
  if(pI810XvMC == NULL) {
    return (error_base + XvMCBadContext);
  }

  if(attribute == pI810XvMC->xv_colorkey) {
    if((value < I810_XVMC_ATTRIBUTES[0].min_value) ||
       (value > I810_XVMC_ATTRIBUTES[0].max_value)) {
      return BadValue;
    }
    pI810XvMC->colorkey = value;
    return Success;
  }
  if(attribute == pI810XvMC->xv_brightness) {
    if((value < I810_XVMC_ATTRIBUTES[1].min_value) ||
       (value > I810_XVMC_ATTRIBUTES[1].max_value)) {
      return BadValue;
    }
    pI810XvMC->brightness = value;
    return Success;
  }
  if(attribute == pI810XvMC->xv_saturation) {
    if((value < I810_XVMC_ATTRIBUTES[2].min_value) ||
       (value > I810_XVMC_ATTRIBUTES[2].max_value)) {
      return BadValue;
    }
    pI810XvMC->saturation = value;
    return Success;
  }
  if(attribute == pI810XvMC->xv_contrast) {
    if((value < I810_XVMC_ATTRIBUTES[3].min_value) ||
       (value > I810_XVMC_ATTRIBUTES[3].max_value)) {
      return BadValue;
    }
    pI810XvMC->contrast = value;
    return Success;
  }
  return BadValue;
}

/***************************************************************************
// Function: XvMCGetAttribute
// Description: This function queries a context-specific attribute and
//   returns the value.
//
// Arguments:
//   display - Connection to the X server.
//   context - The context whos attributes we are querying.
//   attribute - The X atom of the attribute to be queried
//   value - The returned attribute value
//
// Returns:
//  Status
// Notes:
//   For i810 we support these Attributes:
//    XV_COLORKEY: The colorkey value, initialized from the Xv value at
//                 context creation time.
//    XV_BRIGHTNESS
//    XV_CONTRAST
//    XV_SATURATION
***************************************************************************/
Status XvMCGetAttribute(Display *display, XvMCContext *context,
			Atom attribute, int *value) {
  i810XvMCContext *pI810XvMC;

  if(display == NULL) {
    return BadValue;
  }
  if(context == NULL) {
    return (error_base + XvMCBadContext);
  }
  pI810XvMC = context->privData;
  if(pI810XvMC == NULL) {
    return (error_base + XvMCBadContext);
  }
  if(value == NULL) {
    return BadValue;
  }

  if(attribute == pI810XvMC->xv_colorkey) {
    *value = pI810XvMC->colorkey;
    return Success;
  }
  if(attribute == pI810XvMC->xv_brightness) {
    *value = pI810XvMC->brightness;
    return Success;
  }
  if(attribute == pI810XvMC->xv_saturation) {
    *value = pI810XvMC->saturation;
    return Success;
  }
  if(attribute == pI810XvMC->xv_contrast) {
    *value = pI810XvMC->contrast;
    return Success;
  }
  return BadValue;
}




