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

#include "libsaio.h"
#include "vbe.h"

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
 * Globals
 */
static biosBuf_t bb;

int getVBEInfo( void * infoBlock )
{
    bb.intno  = 0x10;
    bb.eax.rr = funcGetControllerInfo;
    bb.es     = SEG( infoBlock );
    bb.edi.rr = OFF( infoBlock );
    bios( &bb );
    return(bb.eax.r.h);
}

int getVBEModeInfo( int mode, void * minfo_p )
{
    bb.intno  = 0x10;
    bb.eax.rr = funcGetModeInfo;
    bb.ecx.rr = mode;
    bb.es     = SEG(minfo_p);
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
