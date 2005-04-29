/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/sis/sis_dri.h,v 1.12 2004/01/04 18:08:00 twini Exp $ */
/*
 * SiS DRI wrapper
 *
 * Copyright (C) 2001-2004 by Thomas Winischhofer, Vienna, Austria
 *
 * Licensed under the following terms:
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appears in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * and that the name of the copyright holder not be used in advertising
 * or publicity pertaining to distribution of the software without specific,
 * written prior permission. The copyright holder makes no representations
 * about the suitability of this software for any purpose.  It is provided
 * "as is" without expressed or implied warranty.
 *
 * THE COPYRIGHT HOLDER DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO
 * EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 * Authors: 	Can-Ru Yeou, SiS Inc.,
 *		Thomas Winischhofer <thomas@winischhofer.net>,
 *		others.
 *
 * Previously taken and modified from tdfx_dri.h
 */

#ifndef _SIS_DRI_
#define _SIS_DRI_

#include "xf86drm.h"

#define SIS_MAX_DRAWABLES 256
#define SISIOMAPSIZE (64*1024)

typedef struct {
  int CtxOwner;
  int QueueLength;
  unsigned int AGPCmdBufNext;
  unsigned int FrameCount;
#ifdef SIS315DRI
  /* For 315 series */
  unsigned long sharedWPoffset;
#endif
#if 0
  unsigned char *AGPCmdBufBase;
  unsigned long AGPCmdBufAddr;
  unsigned long AGPCmdBufOffset;
  unsigned int  AGPCmdBufSize;
  unsigned long AGPCmdBufNext;
#endif
} SISSAREAPriv, *SISSAREAPrivPtr;

#define AGPVtxBufNext AGPCmdBufNext

#define SIS_FRONT 0
#define SIS_BACK 1
#define SIS_DEPTH 2

typedef struct {
  drmHandle handle;
  drmSize size;
  drmAddress map;
} sisRegion, *sisRegionPtr;

typedef struct {
  sisRegion regs, agp;
  int deviceID;
  int width;
  int height;
  int mem;				/* unused in Mesa 3 DRI */
  int bytesPerPixel;
  int priv1;				/* unused in Mesa 3 DRI */
  int priv2;				/* unused in Mesa 3 DRI */
  int fbOffset;				/* unused in Mesa 3 DRI */
  int backOffset;			/* unused in Mesa 3 DRI */
  int depthOffset;			/* unused in Mesa 3 DRI */
  int textureOffset;			/* unused in Mesa 3 DRI */
  int textureSize;			/* unused in Mesa 3 DRI */
  unsigned int AGPCmdBufOffset;
  unsigned int AGPCmdBufSize;
  int irqEnabled;			/* unused in Mesa 3 DRI */
  unsigned int scrnX, scrnY;		/* unused in Mesa 3 DRI */
} SISDRIRec, *SISDRIPtr;

#define AGPVtxBufOffset AGPCmdBufOffset
#define AGPVtxBufSize AGPCmdBufSize

typedef struct {
  /* Nothing here yet */
  int dummy;
} SISConfigPrivRec, *SISConfigPrivPtr;

typedef struct {
  /* Nothing here yet */
  int dummy;
} SISDRIContextRec, *SISDRIContextPtr;

#ifdef XFree86Server

#include "screenint.h"

Bool SISDRIScreenInit(ScreenPtr pScreen);
void SISDRICloseScreen(ScreenPtr pScreen);
Bool SISDRIFinishScreenInit(ScreenPtr pScreen);

#endif
#endif
