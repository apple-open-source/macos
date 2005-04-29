/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/via/ginfo.h,v 1.4 2003/08/31 01:02:04 tsi Exp $ */
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

#ifndef _VIA_GINFO_H_
#define _VIA_GINFO_H_ 1

typedef struct {
    CARD32 dwWidth;          /* On screen Width                  */
    CARD32 dwHeight;         /* On screen Height                 */
    CARD32 dwBPP;            /* Bits Per Pixel                   */
    CARD32 dwPitch;          /* On screen pitch= width x BPP     */
    CARD32 dwRefreshRate;    /* Refresh rate of the mode         */
    CARD32 TotalVRAM;        /* Total Video RAM in unit of byte  */
    CARD32 VideoHeapBase;    /* Physical Start of video heap     */
    CARD32 VideoHeapEnd;     /* Physical End of video heap       */
    CARD32 dwDVIOn;    	    /* Is it DVI simultaneous mode ?    */
    CARD32 dwExpand;    	    /* Is DVI in expand mode ?          */
    CARD32 dwPanelWidth;  	/* Panel physical Width             */
    CARD32 dwPanelHeight; 	/* Panel physical Height            */
    CARD32 Cap0_Deinterlace; /* Capture 0 deintrlace mode        */
    CARD32 Cap1_Deinterlace; /* Capture 1 deintrlace mode        */
    CARD32 Cap0_FieldSwap;   /* Capture 0 input field swap       */
#ifdef XF86DRI
    BOOL  DRMEnabled;               /* kernel module DRM flag           */
#endif
    BOOL  HasSecondary;             /* True: XServer in SAMM mode       */
    BOOL  IsSecondary;              /* True: In SAMM mode 2nd display   */
    BOOL  Screen1IsLeft;            /* True: Screen1 LeftOf Screen0 ; False: Screen1 RightOf Screen0 */
    BOOL  Screen1IsAbove;           /* True: Screen1 Above Screen0 ; False: Screen1 Below Screen0 */
    CARD32 RevisionID;               /* The chip revision ID */
} ViaGraphicRec;

#endif /* _VIA_GINFO_H_ */
