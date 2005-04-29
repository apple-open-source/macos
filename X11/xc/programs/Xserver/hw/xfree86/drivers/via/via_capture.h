/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/via/via_capture.h,v 1.2 2003/08/27 15:16:07 tsi Exp $ */
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

#ifndef _VIA_CAPTURE_H_
#define _VIA_CAPTURE_H_ 1

typedef struct _VIACapRec {
    unsigned long dwDeinterlaceMode;
    unsigned long VideoDecoder;
    unsigned long Tuner;
    unsigned long TVEncoder;
    int Vdec_Slave_Write;
    int Vdec_Slave_Read;
    int Tuner_Slave_Write;
    int Tuner_Slave_Read;
    int TVEncoder_Slave_Write;
    int TVEncoder_Slave_Read;
} VIACapRec;

/* Definition for dwFlags */

#define DDOVER_KEYDEST    0x00000004

/*Definition for  LPVIAGETPORTATTR->attribute*/
#define ATTR_ENCODING         0	/* XV_ENCODING  */
#define ATTR_INIT_AUDIO       ATTR_ENCODING+50	/* XV_MUTE      */
#define ATTR_MUTE_ON          ATTR_ENCODING+51	/* XV_MUTE      */
#define ATTR_MUTE_OFF         ATTR_ENCODING+52	/* XV_MUTE      */
#define ATTR_VOLUME           ATTR_ENCODING+53	/* XV_VOLUME    */
#define ATTR_STEREO           ATTR_ENCODING+54	/* XV_VOLUME    */
#define ATTR_SAP              ATTR_ENCODING+55	/* XV_SAP       */
#define ATTR_TUNER_AUDIO_SWITCH   ATTR_ENCODING+56	/* XV_TUNER_AUDIO */
#define ATTR_AUDIO_CONTROLByAP    ATTR_ENCODING+57	/* XV_AUDIOCTRL  */

#define DEV_TV0   0
#define DEV_TV1   1

typedef struct _CAPDEVICE {
    unsigned char *lpCAPOverlaySurface[3];	/* Max 3 Pointers to CAP Overlay Surface */
    unsigned long dwCAPPhysicalAddr[3];	/*Max 3 Physical address to CAP Overlay Surface */
    unsigned long dwHQVAddr[2];	/*Max 2 Physical address to CAP HQV Overlay Surface */
    CARD32 dwWidth;		/*CAP Source Width, not changed */
    CARD32 dwHeight;		/*CAP Source Height, not changed */
    CARD32 dwPitch;		/*CAP frame buffer pitch */
    unsigned char byDeviceType;	/*Device type. Such as DEV_TV1 and DEV_TV0 */
    CARD32 gdwCAPSrcWidth;	/*CAP Source Width, changed if window is out of screen */
    CARD32 gdwCAPSrcHeight;	/*CAP Source Height, changed if window is out of screen */
    CARD32 gdwCAPDstWidth;	/*CAP Destination Width */
    CARD32 gdwCAPDstHeight;	/*CAP Destination Height */
    CARD32 gdwCAPDstLeft;	/*CAP Position : Left */
    CARD32 gdwCAPDstTop;	/*CAP Position : Top */
    CARD32 dwDeinterlaceMode;	/*BOB / WEAVE */
} CAPDEVICE;

#endif /* _VIA_CAPTURE_H_ */
