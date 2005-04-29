/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/via/via_xvpriv.h,v 1.5 2003/08/27 15:16:14 tsi Exp $ */
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

#ifndef _VIA_XVPRIV_H_
#define _VIA_XVPRIV_H_ 1

#include "videodev.h"

#define   XV_PORT_NUM       5
#define   XV_SWOV_PORTID    0
#define   XV_TV0_PORTID     1
#define   XV_TV1_PORTID     2
#define   XV_UTCTRL_PORTID  3
#define   XV_DUMMY_PORTID   4

#define   COMMAND_FOR_SWOV      XV_SWOV_PORTID
#define   COMMAND_FOR_TV0       XV_TV0_PORTID
#define   COMMAND_FOR_TV1       XV_TV1_PORTID
#define   COMMAND_FOR_UTCTRL    XV_UTCTRL_PORTID
#define   COMMAND_FOR_DUMMY     XV_DUMMY_PORTID

typedef struct {
    unsigned char  xv_portnum;
    unsigned char  brightness;
    unsigned char  saturation;
    unsigned char  contrast;
    unsigned char  hue;
    unsigned long  dwEncoding;
    RegionRec      clip;
    CARD32         colorKey;
    Time           offTime;
    Time           freeTime;
    VIACapRec      CapInfo;
    CARD32	   AudioMode;
    int		   Volume;

    /* Surface structure */
    DDSURFACEDESC SurfaceDesc;
    DDLOCK ddLock;

    /* file handle */
    int 			nr;
    struct video_capability     cap;

    /* attributes */
    struct video_picture    	pict;
    struct video_audio          audio;

    int                         *input;
    int                         *norm;
    int                         nenc,cenc;

    /* yuv to offscreen */
    struct video_window		yuv_win;

    /* store old video source & dst data */
    short old_src_w;
    short old_src_h;

    short old_drw_x;
    short old_drw_y;
    short old_drw_w;
    short old_drw_h;

} viaPortPrivRec, *viaPortPrivPtr;

#if 0
__inline void AllocatePortPriv();
__inline void FreePortPriv();
__inline void ClearPortPriv(int);
viaPortPrivPtr GetPortPriv(int);
void SetPortPriv(int nIndex, unsigned long dwAction, unsigned long dwValue);
unsigned long  IdentifyPort(viaPortPrivPtr);
#endif

#endif /* _VIA_XVPRIV_H_ */
