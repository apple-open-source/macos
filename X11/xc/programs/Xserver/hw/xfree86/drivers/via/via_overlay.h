/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/via/via_overlay.h,v 1.2 2003/08/27 15:16:12 tsi Exp $ */
/*
 * Copyright 1998-2003 VIA Technologies, Inc. All Rights Reserved.
 * Copyright 2001-2003 S3 Graphics, Inc. All Rights Reserved.
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
 
#ifndef _VIA_OVERLAY_H_
#define _VIA_OVERLAY_H_ 1

/*#define   XV_DEBUG      1*/     /* write log msg to /var/log/XFree86.0.log */

#ifdef XV_DEBUG
# define DBG_DD(x) (x)
#else
# define DBG_DD(x)
#endif

#define PLUS_HEIGHT 1 /*V003*/

typedef struct _YCBCRREC  {
  CARD32  dwY ;
  CARD32  dwCB;
  CARD32  dwCR;
} YCBCRREC;

unsigned long viaOverlayHQVGetFormat(LPDDPIXELFORMAT lpDPF, unsigned long dwVidCtl,unsigned long * lpdwHQVCtl );
YCBCRREC viaOverlayGetYCbCrStartAddress(unsigned long dwVideoFlag,unsigned long dwStartAddr, unsigned long dwOffset,unsigned long dwUVoffset,unsigned long dwSrcPitch/*lpGbl->lPitch*/,unsigned long dwSrcHeight/*lpGbl->wHeight*/);
unsigned long viaOverlayGetFetch(unsigned long dwVideoFlag,LPDDPIXELFORMAT lpDPF,unsigned long dwSrcWidth,unsigned long dwDstWidth,unsigned long dwOriSrcWidth,unsigned long * lpHQVsrcFetch);

#endif /* _VIA_OVERLAY_H_ */
