/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.1 (the "License").  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON- INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
// Copyright 1997 by Apple Computer, Inc., all rights reserved.
/* Copyright 1996-1997 NeXT Software, Inc.
 *
 * vesa.h - mode info obtained via int10
 *
 * Revision History
 * ----------------
 * 30 Jul 1996  Doug Mitchell at NeXT
 *  Created.
 */

#ifndef __LIBSAIO_VBE_H
#define __LIBSAIO_VBE_H

/*
 * Graphics mode settings.
 */
extern BOOL            in_linear_mode;
extern unsigned char * frame_buffer;
extern unsigned short  screen_width;
extern unsigned short  screen_height;
extern unsigned char   bits_per_pixel;
extern unsigned short  screen_rowbytes;

#define MIN_VESA_VERSION    0x200

#define SEG(address) \
        ((unsigned short)(((unsigned long)address & 0xffff0000) >> 4))
#define OFF(address) ((unsigned short)((unsigned long)address & 0x0000ffff))
#define RTOV(low, one, two, high) \
        (((unsigned long)high << 12) | ((unsigned long)one << 8) | \
         (unsigned long)low)
#define ADDRESS(low, one, two, high) \
        (((unsigned long)high << 24) | ((unsigned long)two << 16) | \
         ((unsigned long)one << 8) | (unsigned long)low)

/*
 *  Functions
 */
enum {
    funcGetControllerInfo    = 0x4F00,
    funcGetModeInfo          = 0x4F01,
    funcSetMode              = 0x4F02,
    funcGetCurrentMode       = 0x4F03,
    funcSaveRestoreState     = 0x4F04,
    funcWindowControl        = 0x4F05,
    funcGetSetScanLineLength = 0x4F06,
    funcGetSetDisplayStart   = 0x4F07,
    funcGetSetPaletteFormat  = 0x4F08,
    funcGetSetPaletteData    = 0x4F09,
    funcGetProtModeInterdace = 0x4F0A
};

enum {
    subfuncSet          = 0x00,
    subfuncGet          = 0x01,
    subfuncSetSecondary = 0x02,
    subfuncGetSecondary = 0x03
};

/*
 * errors.
 */
enum {
    errSuccess          = 0,
    errFuncFailed       = 1,
    errFuncNotSupported = 2,
    errFuncInvalid      = 3
};

/*
 * Per-controller info, returned in function 4f00.
 */
typedef struct {
    unsigned char   VESASignature[4];
    unsigned short  VESAVersion;
    /*
     * Avoid packing problem...
     */
    unsigned char   OEMStringPtr_low;
    unsigned char   OEMStringPtr_1;
    unsigned char   OEMStringPtr_2;
    unsigned char   OEMStringPtr_high;
    unsigned char   Capabilities_low;
    unsigned char   Capabilities_1;
    unsigned char   Capabilities_2;
    unsigned char   Capabilities_high;
    unsigned char   VideoModePtr_low;
    unsigned char   VideoModePtr_1;
    unsigned char   VideoModePtr_2;
    unsigned char   VideoModePtr_high;
    unsigned short  TotalMemory;
    unsigned char   Reserved[242];
} VBEInfoBlock;

/*
 * Capabilites
 */
enum {
    capDACWidthIsSwitchableBit          = (1 << 0), /* 1 = yes; 0 = no */
    capControllerIsNotVGACompatableBit  = (1 << 1), /* 1 = no; 0 = yes */
    capOldRAMDAC                        = (1 << 2)  /* 1 = yes; 0 = no */
};

/*
 * Per-mode info, returned in function 4f02.
 */
typedef struct {
    unsigned short  ModeAttributes;
    unsigned char   WinAAttributes;
    unsigned char   WinBAttributes;
    unsigned short  WinGranularity;
    unsigned short  WinSize;
    unsigned short  WinASegment;
    unsigned short  WinABegment;
    void *          WinFuncPtr;
    unsigned short  BytesPerScanline;
    unsigned short  XResolution;
    unsigned short  YResolution;
    unsigned char   XCharSize;
    unsigned char   YCharSize;
    unsigned char   NumberOfPlanes;
    unsigned char   BitsPerPixel;
    unsigned char   NumberOfBanks;
    unsigned char   MemoryModel;
    unsigned char   BankSize;
    unsigned char   NumberOfImagePages;
    unsigned char   Reserved;
    unsigned char   RedMaskSize;
    unsigned char   RedFieldPosition;
    unsigned char   GreenMaskSize;
    unsigned char   GreenFieldPosition;
    unsigned char   BlueMaskSize;
    unsigned char   BlueFieldPosition;
    unsigned char   RsvdMaskSize;
    unsigned char   RsvdFieldPosition;
    unsigned char   DirectColorModeInfo;
    unsigned char   PhysBasePtr_low;
    unsigned char   PhysBasePtr_1;
    unsigned char   PhysBasePtr_2;
    unsigned char   PhysBasePtr_high;
    void *          OffScreenMemOffset;
    unsigned short  OffScreenMemSize;
    unsigned char   Reserved1[206];
} VBEModeInfoBlock;

/* 
 * ModeAttributes bits
 */
enum {
    maModeIsSupportedBit        = (1 << 0), /* mode is supported */
    maExtendedInfoAvailableBit  = (1 << 1), /* extended info available */
    maOutputFuncSupportedBit    = (1 << 2), /* output functions supported */
    maColorModeBit              = (1 << 3), /* 1 = color; 0 = mono */
    maGraphicsModeBit           = (1 << 4), /* 1 = graphics; 0 = text */
    maModeIsNotVGACompatableBit = (1 << 5), /* 1 = not compat; 0 = compat */
    maVGAMemoryModeNotAvailBit  = (1 << 6), /* 1 = not avail; 0 = avail */
    maLinearFrameBufferAvailBit = (1 << 7)  /* 1 = avail; 0 = not avail */
};

/*
 *  Modes
 */
enum {
    mode640x400x256   = 0x100,
    mode640x480x256   = 0x101,
    mode800x600x16    = 0x102,
    mode800x600x256   = 0x103,
    mode1024x768x16   = 0x104,
    mode1024x768x256  = 0x105,
    mode1280x1024x16  = 0x106,
    mode1280x1024x256 = 0x107,
    mode80Cx60R       = 0x108,
    mode132Cx25R      = 0x109,
    mode132Cx43R      = 0x10A,
    mode132Cx50R      = 0x10B,
    mode132Cx60R      = 0x10C,
    mode320x200x555   = 0x10D,
    mode320x200x565   = 0x10E,
    mode320x200x888   = 0x10F,
    mode640x480x555   = 0x110,
    mode640x480x565   = 0x111,
    mode640x480x888   = 0x112,
    mode800x600x555   = 0x113,
    mode800x600x565   = 0x114,
    mode800x600x888   = 0x115,
    mode1024x768x555  = 0x116,
    mode1024x768x565  = 0x117,
    mode1024x768x888  = 0x118,
    mode1280x1024x555 = 0x119,
    mode1280x1024x565 = 0x11A,
    mode1280x1024x888 = 0x11B,
    modeSpecial       = 0x81FF
};

/*
 *  Get/Set VBE Mode parameters
 */
enum {
    kLinearFrameBufferBit  =  (1 << 14),
    kPreserveMemoryBit     =  (1 << 15)
};

/*
 * Palette
 */
typedef unsigned long VBEPalette[256];

extern int getVBEInfo(void *vinfo_p);
extern int getVBEModeInfo(int mode, void *minfo_p);
extern int getVBEDACFormat(unsigned char *format);
extern int setVBEDACFormat(unsigned char format);
extern int setVBEPalette(void *palette);
extern int getVBEPalette(void *palette);
extern int setVBEMode(unsigned short mode);
extern int getVBECurrentMode(unsigned short *mode);

#endif /* !__LIBSAIO_VBE_H */
