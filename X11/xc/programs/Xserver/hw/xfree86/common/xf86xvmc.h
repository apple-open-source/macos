/* $XFree86: xc/programs/Xserver/hw/xfree86/common/xf86xvmc.h,v 1.8 2004/02/13 23:58:40 dawes Exp $ */

/*
 * Copyright (c) 2001 by The XFree86 Project, Inc.
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject
 * to the following conditions:
 *
 *   1.  Redistributions of source code must retain the above copyright
 *       notice, this list of conditions, and the following disclaimer.
 *
 *   2.  Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer
 *       in the documentation and/or other materials provided with the
 *       distribution, and in the same place and form as other copyright,
 *       license and disclaimer information.
 *
 *   3.  The end-user documentation included with the redistribution,
 *       if any, must include the following acknowledgment: "This product
 *       includes software developed by The XFree86 Project, Inc
 *       (http://www.xfree86.org/) and its contributors", in the same
 *       place and form as other third-party acknowledgments.  Alternately,
 *       this acknowledgment may appear in the software itself, in the
 *       same form and location as other such third-party acknowledgments.
 *
 *   4.  Except as contained in this notice, the name of The XFree86
 *       Project, Inc shall not be used in advertising or otherwise to
 *       promote the sale, use or other dealings in this Software without
 *       prior written authorization from The XFree86 Project, Inc.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE XFREE86 PROJECT, INC OR ITS CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _XF86XVMC_H
#define _XF86XVMC_H

#include "xvmcext.h"
#include "xf86xv.h"

typedef struct {
  int num_xvimages;
  int *xvimage_ids;  /* reference the subpictures in the XF86MCAdaptorRec */
} XF86MCImageIDList; 

typedef struct {
  int surface_type_id;  /* Driver generated.  Must be unique on the port */
  int chroma_format;
  int color_description;  /* no longer used */
  unsigned short max_width;       
  unsigned short max_height;   
  unsigned short subpicture_max_width;
  unsigned short subpicture_max_height;
  int mc_type;         
  int flags;
  XF86MCImageIDList *compatible_subpictures; /* can be null, if none */
} XF86MCSurfaceInfoRec, *XF86MCSurfaceInfoPtr;


/*
   xf86XvMCCreateContextProc 

   DIX will fill everything out in the context except the driver_priv.
   The port_priv holds the private data specified for the port when
   Xv was initialized by the driver.
   The driver may store whatever it wants in driver_priv and edit
   the width, height and flags.  If the driver wants to return something
   to the client it can allocate space in priv and specify the number
   of 32 bit words in num_priv.  This must be dynamically allocated
   space because DIX will free it after it passes it to the client.
*/
   

typedef int (*xf86XvMCCreateContextProcPtr) (
  ScrnInfoPtr pScrn,
  XvMCContextPtr context,
  int *num_priv,
  CARD32 **priv 
);

typedef void (*xf86XvMCDestroyContextProcPtr) (
  ScrnInfoPtr pScrn,
  XvMCContextPtr context
);

/*
   xf86XvMCCreateSurfaceProc 

   DIX will fill everything out in the surface except the driver_priv.
   The driver may store whatever it wants in driver_priv.  The driver
   may pass data back to the client in the same manner as the
   xf86XvMCCreateContextProc.
*/


typedef int (*xf86XvMCCreateSurfaceProcPtr) (
  ScrnInfoPtr pScrn,
  XvMCSurfacePtr surface,
  int *num_priv,
  CARD32 **priv
);

typedef void (*xf86XvMCDestroySurfaceProcPtr) (
  ScrnInfoPtr pScrn,
  XvMCSurfacePtr surface
);

/*
   xf86XvMCCreateSubpictureProc 

   DIX will fill everything out in the subpicture except the driver_priv,
   num_palette_entries, entry_bytes and component_order.  The driver may
   store whatever it wants in driver_priv and edit the width and height.
   If it is a paletted subpicture the driver needs to fill out the
   num_palette_entries, entry_bytes and component_order.  These are
   not communicated to the client until the time the surface is
   created.

   The driver may pass data back to the client in the same manner as the
   xf86XvMCCreateContextProc.
*/


typedef int (*xf86XvMCCreateSubpictureProcPtr) (
  ScrnInfoPtr pScrn,
  XvMCSubpicturePtr subpicture,
  int *num_priv,
  CARD32 **priv
);

typedef void (*xf86XvMCDestroySubpictureProcPtr) (
  ScrnInfoPtr pScrn,
  XvMCSubpicturePtr subpicture
);


typedef struct {
  char *name;
  int num_surfaces;
  XF86MCSurfaceInfoPtr *surfaces;
  int num_subpictures;
  XF86ImagePtr *subpictures;
  xf86XvMCCreateContextProcPtr 		CreateContext; 
  xf86XvMCDestroyContextProcPtr		DestroyContext; 
  xf86XvMCCreateSurfaceProcPtr 		CreateSurface; 
  xf86XvMCDestroySurfaceProcPtr		DestroySurface; 
  xf86XvMCCreateSubpictureProcPtr	CreateSubpicture; 
  xf86XvMCDestroySubpictureProcPtr	DestroySubpicture; 
} XF86MCAdaptorRec, *XF86MCAdaptorPtr;

/* 
   xf86XvMCScreenInit 

   Unlike Xv, the adaptor data is not copied from this structure.
   This structure's data is used so it must stick around for the
   life of the server.  Note that it's an array of pointers not
   an array of structures.
*/

Bool xf86XvMCScreenInit(
  ScreenPtr pScreen, 
  int num_adaptors,
  XF86MCAdaptorPtr *adaptors
);

XF86MCAdaptorPtr xf86XvMCCreateAdaptorRec (void);
void xf86XvMCDestroyAdaptorRec(XF86MCAdaptorPtr adaptor);

#endif /* _XF86XVMC_H */
