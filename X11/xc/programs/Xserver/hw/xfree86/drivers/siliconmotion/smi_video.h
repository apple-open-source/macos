/* Header:   //Mercury/Projects/archives/XFree86/4.0/smi_video.h.-arc   1.8   27 Nov 2000 15:46:06   Frido  $ */

/*
Copyright (C) 1994-1999 The XFree86 Project, Inc.  All Rights Reserved.
Copyright (C) 2000 Silicon Motion, Inc.  All Rights Reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FIT-
NESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
XFREE86 PROJECT BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the names of the XFree86 Project and
Silicon Motion shall not be used in advertising or otherwise to promote the
sale, use or other dealings in this Software without prior written
authorization from the XFree86 Project and silicon Motion.
*/
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/siliconmotion/smi_video.h,v 1.4 2002/09/16 18:06:01 eich Exp $ */

#ifndef _SMI_VIDEO_H
#define _SMI_VIDEO_H

#include "fourcc.h"

#define SMI_VIDEO_VIDEO		0
#define SMI_VIDEO_IMAGE		1

#define FOURCC_RV15			0x35315652
#define FOURCC_RV16			0x36315652
#define FOURCC_RV24			0x34325652
#define FOURCC_RV32			0x32335652

#define OFF_DELAY			200		/* milliseconds */
#define FREE_DELAY			60000	/* milliseconds */

#define OFF_TIMER			0x01
#define FREE_TIMER			0x02
#define CLIENT_VIDEO_ON		0x04
#define TIMER_MASK			(OFF_TIMER | FREE_TIMER)

#define SAA7110				0x9C
#define SAA7111             0x48

/*
 * Attributes
 */

#define N_ATTRS                 8

#define XV_ENCODING             0
#define XV_BRIGHTNESS           1
#define XV_CAPTURE_BRIGHTNESS	2
#define XV_CONTRAST             3
#define XV_SATURATION           4
#define XV_HUE                  5
#define XV_COLORKEY             6
#define XV_INTERLACED           7

typedef struct
{
	FBAreaPtr	area;
	RegionRec	clip;
    /* Attributes */
    CARD32      Attribute[N_ATTRS];
	CARD32		videoStatus;
	Time		offTime;
	Time		freeTime;
    I2CDevRec   I2CDev;

    /* Encodings */
    XF86VideoEncodingPtr        enc;
    int                         *input;
    int                         *norm;
    int                         *channel;
    int                         nenc,cenc;
} SMI_PortRec, *SMI_PortPtr;

typedef struct
{
	FBAreaPtr	area;
	Bool		isOn;

} SMI_OffscreenRec, *SMI_OffscreenPtr;

typedef struct
{
	CARD8		address;
	CARD8		data;

} SMI_I2CDataRec, *SMI_I2CDataPtr;

#endif /* _SMI_VIDEO_H */
