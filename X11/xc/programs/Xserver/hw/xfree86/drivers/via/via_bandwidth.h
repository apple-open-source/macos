/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/via/via_bandwidth.h,v 1.2 2003/08/27 15:16:06 tsi Exp $ */
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

#ifndef _VIA_BANDWIDTH_H_
#define _VIA_BANDWIDTH_H_ 1

typedef struct _VIAPanel3C522Tue
{
    CARD16   wPanelXres;
    CARD16   wPanelYres;
    CARD16   wPanelBpp;
    CARD8    bRamClock;
    CARD8    bTuningValue;
} VIAPanel3C522Tue, *pVIAPanel3C522Tue;

static const VIAPanel3C522Tue Panel_Tuning_Lst[] = {
    {1280, 768,32,0x03,0x3}, {1280,1024,32,0x03,0x4}, {1280,1024,32,0x04,0x3},
    {1600,1200,16,0x03,0x4}, {1600,1200,32,0x04,0x4}, {1024, 768,32,0x03,0xA},
    {1400,1050,16,0x03,0x3}, {1400,1050,32,0x03,0x4}, {1400,1050,32,0x04,0x4},
    { 800, 600,32,0x03,0xA}, {   0,   0, 0,   0,  0}
};

static const VIAPanel3C522Tue Panel_Tuning_LstC0[] = {
    {1280, 768,32,0x03,0x3}, {1280,1024,32,0x03,0x4}, {1280,1024,32,0x04,0x4},
    {1600,1200,32,0x03,0x3}, {1600,1200,32,0x04,0x4}, {1024, 768,32,0x03,0xA},
    {1400,1050,32,0x03,0x4}, {1400,1050,32,0x04,0x4},
    { 800, 600,32,0x03,0xA}, {   0,   0, 0,   0,  0}
};

static const VIAPanel3C522Tue Panel_Tuning_Lst3205[]={
    {1280,1024,32,0x03,0x3}, {1280,1024,32,0x04,0x9}, {1280, 768,32,0x03,0x3},
    {1280, 768,32,0x04,0x9}, {1400,1050,32,0x03,0x3}, {1400,1050,32,0x04,0x9},
    {1600,1200,32,0x03,0x4}, {1600,1200,32,0x04,0xA}, {   0,   0, 0,   0,  0}
};

#endif /* _VIA_BANDWIDTH_H_ */
