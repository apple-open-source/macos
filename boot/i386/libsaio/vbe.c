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
/*
 * Copyright 1993 NeXT, Inc.
 * All rights reserved.
 */
#include "io_inline.h"
#include "libsaio.h"
#include "vga.h"
#include "vbe.h"
#include "kernBootStruct.h"
#include "appleClut8.h"

/*
 * Graphics mode settings.
 */
BOOL            in_linear_mode;
unsigned char * frame_buffer;
unsigned short  screen_width;
unsigned short  screen_height;
unsigned char   bits_per_pixel;
unsigned short  screen_rowbytes;

/* 
 * Various inline routines for video I/O
 */
static inline void
outi (int port, int index, int val)
{
    outw (port, (val << 8) | index);
}

static inline void
outib (int port, int index, int val)
{
    outb (port, index);
    outb (port + 1, val);
}

static inline int
ini (int port, int index)
{
    outb (port, index);
    return inb (port + 1);
}

static inline void
rmwi (int port, int index, int clear, int set)
{
    outb (port, index);
    outb (port + 1, (inb (port + 1) & ~clear) | set);
}

/*
 * Local Prototypes
 */
void setupPalette(VBEPalette * p, const unsigned char * g);

/*
 * Globals
 */
static biosBuf_t bb;

#if 0
static char *models[] = { "Text", 
			  "CGA", 
			  "Hercules", 
			  "Planar", 
			  "Packed Pixel", 
			  "Non-Chain 4", 
			  "Direct Color", 
			  "YUV" };
#endif

int
set_linear_video_mode(unsigned short mode)
{
    VBEInfoBlock        vinfo;
    VBEModeInfoBlock    minfo;
    int                 err;
    VBEPalette          palette;

    do {
        /*
         * See if VESA is around.
         */
        err = getVBEInfo(&vinfo);
        if (err != errSuccess)
        {
            printf("VESA not available.\n");
            break;
        }

#if 0
        /*
         * See if this mode is supported.
         */
        err = getVBEModeInfo(mode, &minfo);
        if ( !((err == errSuccess) && 
            (minfo.ModeAttributes & maModeIsSupportedBit) &&
            (minfo.ModeAttributes & maGraphicsModeBit) /* &&
            (minfo.ModeAttributes & maLinearFrameBufferAvailBit)*/) )
        {
            printf("Mode %d is not supported.\n", mode);
            err = errFuncNotSupported;
            break;
        }
#endif

        /*
         * Set the mode.
         */
        err = setVBEMode(mode | kLinearFrameBufferBit );
        if ( err != errSuccess )
        {
            if (vinfo.VESAVersion < MIN_VESA_VERSION)
            {
                printf("Video Card is VESA %d.%d. It must be at least VESA %d.%d\n",
                    vinfo.VESAVersion >> 8,
                    vinfo.VESAVersion & 0xff,
                    MIN_VESA_VERSION >> 8,
                    MIN_VESA_VERSION & 0xff);
            }
            else
            {
                printf("Error #%d in set video mode.\n", err);
            }
            break;
        }

        /*
         * Get mode info.
         */
        err = getVBEModeInfo(mode, &minfo);
        if ( err != errSuccess )
        {
            printf("Error #%d in get mode info.\n", err);
            break;
        }

        /*
         * Set the palette.
         */
        if (( vinfo.VESAVersion >= MIN_VESA_VERSION ) &&
            ( minfo.BitsPerPixel == 8 ))
        {
            setupPalette(&palette, appleClut8);
            if ((err = setVBEPalette(palette)) != errSuccess)
            {
                printf("Error #%d in setting palette.\n", err);
                break;
            }
        }

        err             = errSuccess;
        in_linear_mode  = YES;
        screen_width    = minfo.XResolution;
        screen_height   = minfo.YResolution;
        bits_per_pixel  = minfo.BitsPerPixel;
        screen_rowbytes = minfo.BytesPerScanline;

        /* The S3 video card reports 15 bits... the video console driver
         * Can't deal.. set it to 16.
         */
        if (bits_per_pixel > 8 && bits_per_pixel < 16)
            bits_per_pixel = 16;

        frame_buffer = (unsigned char *) ADDRESS(minfo.PhysBasePtr_low, 
                                                 minfo.PhysBasePtr_1, 
                                                 minfo.PhysBasePtr_2, 
                                                 minfo.PhysBasePtr_high);
    }
    while ( 0 );
    
    return err;
}

void setupPalette(VBEPalette * p, const unsigned char * g)
{
    int             i;
    unsigned char * source = (unsigned char *) g;

    for (i = 0; i < 256; i++)
    {
        (*p)[i] = 0;
        (*p)[i] |= ((unsigned long)((*source++) >> 2)) << 16;   // Red
        (*p)[i] |= ((unsigned long)((*source++) >> 2)) << 8;    // Green
        (*p)[i] |= ((unsigned long)((*source++) >> 2));         // Blue
    }
}

int getVBEInfo(void *vinfo_p)
{
    bb.intno = 0x10;
    bb.eax.rr = funcGetControllerInfo;
    bb.es = SEG(vinfo_p);
    bb.edi.rr = OFF(vinfo_p);
    bios(&bb);
    return(bb.eax.r.h);
}

int getVBEModeInfo(int mode, void *minfo_p)
{
    bb.intno = 0x10;
    bb.eax.rr = funcGetModeInfo;
    bb.ecx.rr = mode;
    bb.es = SEG(minfo_p);
    bb.edi.rr = OFF(minfo_p);
    bios(&bb);
    return(bb.eax.r.h);
}

int getVBEDACFormat(unsigned char *format)
{
    bb.intno = 0x10;
    bb.eax.rr = funcGetSetPaletteFormat;
    bb.ebx.r.l = subfuncGet;
    bios(&bb);
    *format = bb.ebx.r.h;
    return(bb.eax.r.h);
}

int setVBEDACFormat(unsigned char format)
{
    bb.intno = 0x10;
    bb.eax.rr = funcGetSetPaletteFormat;
    bb.ebx.r.l = subfuncSet;
    bb.ebx.r.h = format;
    bios(&bb);
    return(bb.eax.r.h);
}

int setVBEMode(unsigned short mode)
{
    bb.intno = 0x10;
    bb.eax.rr = funcSetMode;
    bb.ebx.rr = mode;
    bios(&bb);
    return(bb.eax.r.h);
}

int setVBEPalette(void *palette)
{
    bb.intno = 0x10;
    bb.eax.rr = funcGetSetPaletteData;
    bb.ebx.r.l = subfuncSet;
    bb.ecx.rr = 256;
    bb.edx.rr = 0;
    bb.es = SEG(palette);
    bb.edi.rr = OFF(palette);
    bios(&bb);
    return(bb.eax.r.h);
}

int getVBEPalette(void *palette)
{
    bb.intno = 0x10;
    bb.eax.rr = funcGetSetPaletteData;
    bb.ebx.r.l = subfuncGet;
    bb.ecx.rr = 256;
    bb.edx.rr = 0;
    bb.es = SEG(palette);
    bb.edi.rr = OFF(palette);
    bios(&bb);
    return(bb.eax.r.h);
}

int getVBECurrentMode(unsigned short *mode)
{
    bb.intno = 0x10;
    bb.eax.rr = funcGetCurrentMode;
    bios(&bb);
    *mode = bb.ebx.rr;
    return(bb.eax.r.h);
}
