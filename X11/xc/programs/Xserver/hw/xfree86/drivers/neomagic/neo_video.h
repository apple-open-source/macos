/**********************************************************************
Copyright 2002 by Shigehiro Nomura.

                        All Rights Reserved

Permission to use, copy, modify, distribute, and sell this software and
its documentation for any purpose is hereby granted without fee,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation, and that the name of Shigehiro Nomura not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.  Shigehiro Nomura
and its suppliers make no representations about the suitability of this
software for any purpose.  It is provided "as is" without express or 
implied warranty.

SHIGEHIRO NOMURA DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
EVENT SHALL SHIGEHIRO NOMURA AND/OR ITS SUPPLIERS BE LIABLE FOR ANY
SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
**********************************************************************/
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/neomagic/neo_video.h,v 1.2 2002/07/01 02:25:58 tsi Exp $ */

#ifndef _NEO_VIDEO_H
#define _NEO_VIDEO_H

#define ACC_MMIO

#include "vgaHW.h"
#include "fourcc.h"
#include "Xv.h"

#define NEO_VIDEO_VIDEO		0
#define NEO_VIDEO_IMAGE		1

#define FOURCC_RV15			0x35315652
#define FOURCC_RV16			0x36315652

#define OFF_DELAY			200		/* milliseconds */
#define FREE_DELAY			60000	/* milliseconds */

#define OFF_TIMER			0x01
#define FREE_TIMER			0x02
#define CLIENT_VIDEO_ON		0x04
#define TIMER_MASK			(OFF_TIMER | FREE_TIMER)

typedef struct
{
    FBLinearPtr	linear;
    RegionRec	clip;
    CARD32	colorKey;
    CARD32	interlace;
    CARD32	brightness;
    CARD32	videoStatus;
    Time	offTime;
    Time	freeTime;
} NEOPortRec, *NEOPortPtr;

typedef struct
{
    FBLinearPtr	linear;
    Bool	isOn;
} NEOOffscreenRec, *NEOOffscreenPtr;

/* I/O Functions */
# define OUTGR(idx,dat) \
   if (nPtr->NeoMMIOBase2) \
     (*(unsigned short *)(nPtr->NeoMMIOBase2+VGA_GRAPH_INDEX)\
                                                     =(idx)|((dat)<<8));\
   else \
      VGAwGR((idx),(dat));

#  define OUTSR(idx,dat) \
if (nPtr->NeoMMIOBase2) \
   (*(unsigned short *)(nPtr->NeoMMIOBase2+VGA_SEQ_INDEX)=(idx)|((dat)<<8));\
else \
   VGAwSR((idx),(dat));

# define VGA_HWP(x)     vgaHWPtr hwp = VGAHWPTR(x)

#endif /* _NEO_VIDEO_H */
