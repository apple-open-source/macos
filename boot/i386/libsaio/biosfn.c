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
 * Copyright 1993 NeXT Computer, Inc.
 * All rights reserved.
 */

#include "libsaio.h"

static biosBuf_t bb;
unsigned char uses_ebios[8] = {0, 0, 0, 0, 0, 0, 0, 0};

int bgetc(void)
{
    bb.intno = 0x16;
    bb.eax.r.h = 0x00;
    bios(&bb);
    return bb.eax.rr;
}

int readKeyboardStatus(void)
{
    bb.intno = 0x16;
    bb.eax.r.h = 0x01;
    bios(&bb);
    if (bb.flags.zf) {
        return 0;
    } else {
        return bb.eax.rr;
    }
}

int readKeyboardShiftFlags(void)
{
    bb.intno = 0x16;
    bb.eax.r.h = 0x02;
    bios(&bb);
    return bb.eax.r.l;
}

unsigned int time18(void)
{
    union {
        struct {
            unsigned int low:16;
            unsigned int high:16;
        } s;
        unsigned int i;
    } time;
    
    bb.intno = 0x1a;
    bb.eax.r.h = 0x00;
    bios(&bb);
    time.s.low = bb.edx.rr;
    time.s.high = bb.ecx.rr;
    return time.i;
}

int memsize(int which)
{
    int size;
        
    if ( which )
    {
        // Get the total system memory discovered by the
        // BIOS in kilobytes.

        get_memsize(&bb);
        size = (bb.edx.rr << 16) | bb.eax.rr;

        // Convert to extended memory size.

        size = ( size > 1024 ) ? size - 1024 : 0;
    }
    else
    {
        // Get amount of conventional memory available.

        bb.intno = 0x12;
        bios(&bb);
        size = bb.eax.rr;
    }
    return size;
}

void video_mode(int mode)
{
    bb.intno = 0x10;
    bb.eax.r.h = 0x00;
    bb.eax.r.l = mode;
    bios(&bb);
}

int biosread(int dev, int cyl, int head, int sec, int num)
{
    int i;

    bb.intno = 0x13;
    sec += 1;  /* sector numbers start at 1 */
    
    for (i=0;;) {
        bb.ecx.r.h = cyl;
        bb.ecx.r.l = ((cyl & 0x300) >> 2) | (sec & 0x3F);
        bb.edx.r.h = head;
        bb.edx.r.l = dev;
        bb.eax.r.l = num;
        bb.ebx.rr  = OFFSET(ptov(BIOS_ADDR));
        bb.es      = SEGMENT(ptov(BIOS_ADDR));

        bb.eax.r.h = 0x02;
        bios(&bb);

        if ((bb.eax.r.h == 0x00) || (i++ >= 5))
            break;

        /* reset disk subsystem and try again */
        bb.eax.r.h = 0x00;
        bios(&bb);
    }
    return bb.eax.r.h;
}

int ebiosread(int dev, long sec, int count)
{
    int i;
    static struct {
        unsigned char  size;
        unsigned char  reserved;
        unsigned char  numblocks;
        unsigned char  reserved2;
        unsigned short bufferOffset;
        unsigned short bufferSegment;
        unsigned long  long startblock;
    } addrpacket = {0};
    addrpacket.size = sizeof(addrpacket);

    for (i=0;;) {
        bb.intno   = 0x13;
        bb.eax.r.h = 0x42;
        bb.edx.r.l = dev;
        bb.esi.rr  = OFFSET((unsigned)&addrpacket);
        bb.ds      = SEGMENT((unsigned)&addrpacket);
        addrpacket.reserved = addrpacket.reserved2 = 0;
        addrpacket.numblocks     = count;
        addrpacket.bufferOffset  = OFFSET(ptov(BIOS_ADDR));
        addrpacket.bufferSegment = SEGMENT(ptov(BIOS_ADDR));
        addrpacket.startblock    = sec;
        bios(&bb);
        if ((bb.eax.r.h == 0x00) || (i++ >= 5))
            break;

        /* reset disk subsystem and try again */
        bb.eax.r.h = 0x00;
        bios(&bb);
    }
    return bb.eax.r.h;
}

void putc(int ch)
{
    bb.intno = 0x10;
    bb.ebx.r.h = 0x00;  /* background black */
    bb.ebx.r.l = 0x0F;  /* foreground white */
    bb.eax.r.h = 0x0e;
    bb.eax.r.l = ch;
    bios(&bb);
}

void putca(int ch, int attr, int repeat)
{
    bb.intno   = 0x10;
    bb.ebx.r.h = 0x00;   /* page number */
    bb.ebx.r.l = attr;   /* attribute   */
    bb.eax.r.h = 0x9;
    bb.eax.r.l = ch;
    bb.ecx.rx  = repeat; /* repeat count */ 
    bios(&bb);
}

unsigned int get_diskinfo(int drive)
{
    static struct {
        unsigned short size;
        unsigned short flags;
        unsigned long cylinders;
        unsigned long heads;
        unsigned long sectors;
        unsigned long long total_sectors;
        unsigned short bps;
        unsigned long params_p;
    } ebios = {0};
    unsigned char useEbios = 0;

    union {
        compact_diskinfo_t di;
        unsigned int ui;
    } ret;
    static int maxhd = 0;

    ret.ui = 0;
    if (maxhd == 0) {
        bb.intno = 0x13;
        bb.eax.r.h = 0x08;
        bb.edx.r.l = 0x80;
        bios(&bb);
        if (bb.flags.cf == 0)
            maxhd = 0x7f + bb.edx.r.l;
    };

    if (drive > maxhd)
        return 0;

    /* Check drive for EBIOS support. */
    bb.intno = 0x13;
    bb.eax.r.h = 0x41;
    bb.edx.r.l = drive;
    bb.ebx.rr = 0x55aa;
    bios(&bb);
    if(bb.ebx.rr == 0xaa55 && bb.flags.cf == 0) {
        /* Get flags for supported operations. */
        useEbios = bb.ecx.r.l;
    }

    if (useEbios & EBIOS_ENHANCED_DRIVE_INFO) {
        /* Get EBIOS drive info. */
        ebios.size = 26;
        bb.intno = 0x13;
        bb.eax.r.h = 0x48;
        bb.edx.r.l = drive;
        bb.esi.rr = OFFSET((unsigned)&ebios);
        bb.ds     = SEGMENT((unsigned)&ebios);
        bios(&bb);
        if(bb.flags.cf != 0) {
            useEbios = 0;
        }
    }

    bb.intno = 0x13;
    bb.eax.r.h = 0x08;
    bb.edx.r.l = drive;
    bios(&bb);
    if (bb.flags.cf == 0 && bb.eax.r.h == 0) {
        unsigned long cyl;
        unsigned long sec;
        unsigned long hds;

        hds = bb.edx.r.h;
        sec = bb.ecx.r.l & 0x3F;
        if(useEbios & EBIOS_ENHANCED_DRIVE_INFO) {
            cyl = (ebios.total_sectors / ((hds + 1) * sec)) - 1;
        }
        else {
            cyl = bb.ecx.r.h | ((bb.ecx.r.l & 0xC0) << 2);
        }
        ret.di.heads = hds;
        ret.di.sectors = sec;
        ret.di.cylinders = cyl;
    }
    if(drive >= 0x80) uses_ebios[drive - 0x80] = useEbios;
    return ret.ui;
}

void setCursorPosition(int x, int y, int page)
{
    bb.intno = 0x10;
    bb.eax.r.h = 0x02;
    bb.ebx.r.h = page;  /* page 0 for graphics */
    bb.edx.r.l = x;
    bb.edx.r.h = y;
    bios(&bb);
}

void setCursorType(int type)
{
    bb.intno   = 0x10;
    bb.eax.r.h = 0x01;
    bb.ecx.rr  = type;
    bios(&bb);
}

void getCursorPositionAndType(int * x, int * y, int * type)
{
    bb.intno   = 0x10;
    bb.eax.r.h = 0x03;
    bios(&bb);
    *x = bb.edx.r.l;
    *y = bb.edx.r.h;
    *type = bb.ecx.rr;
}

void scollPage(int x1, int y1, int x2, int y2, int attr, int rows, int dir)
{
    bb.intno   = 0x10;
    bb.eax.r.h = (dir > 0) ? 0x06 : 0x07;
    bb.eax.r.l = rows;
    bb.ebx.r.h = attr;
    bb.ecx.r.h = y1;
    bb.ecx.r.l = x1;
    bb.edx.r.h = y2;
    bb.edx.r.l = x2;
    bios(&bb);
}

void clearScreenRows( int y1, int y2 )
{
    scollPage( 0, y1, 80 - 1, y2, 0x07, y2 - y1 + 1, 1 );
}

void setActiveDisplayPage( int page )
{
    bb.intno   = 0x10;
    bb.eax.r.h = 5;
    bb.eax.r.l = page;
    bios(&bb);
}

#if DEBUG

int terminateDiskEmulation()
{
    static char cd_spec[0x12];

    bb.intno   = 0x13;
    bb.eax.r.h = 0x4b;
    bb.eax.r.l = 0;     // subfunc: terminate emulation    
    bb.esi.rr  = OFFSET((unsigned)&cd_spec);
    bb.ds      = SEGMENT((unsigned)&cd_spec);
    bios(&bb);
    return bb.eax.r.h;
}

int readDriveParameters(int drive, struct driveParameters *dp)
{
    bb.intno = 0x13;
    bb.edx.r.l = drive;
    bb.eax.r.h = 0x08;
    bios(&bb);
    if (bb.eax.r.h == 0) {
        dp->heads = bb.edx.r.h;
        dp->sectors = bb.ecx.r.l & 0x3F;
        dp->cylinders = bb.ecx.r.h | ((bb.ecx.r.l & 0xC0) << 2);
        dp->totalDrives = bb.edx.r.l;
    } else {
        bzero(dp, sizeof(*dp));
    }
    return bb.eax.r.h;

}
#endif

#define APM_INTNO   0x15
#define APM_INTCODE 0x53

int
APMPresent(void)
{
    KERNBOOTSTRUCT *kbp = kernBootStruct;
    
    bb.intno = APM_INTNO;
    bb.eax.r.h = APM_INTCODE;
    bb.eax.r.l = 0x00;
    bb.ebx.rr = 0x0000;
    bios(&bb);
    if ((bb.flags.cf == 0) &&
        (bb.ebx.r.h == 'P') &&
        (bb.ebx.r.l == 'M')) {
        /* Success */
        kbp->apmConfig.major_vers = bb.eax.r.h;
        kbp->apmConfig.minor_vers = bb.eax.r.l;
        kbp->apmConfig.flags.data = bb.ecx.rr;
        return 1;
    }
    return 0;
}

int
APMConnect32(void)
{
    KERNBOOTSTRUCT *kbp = kernBootStruct;

    bb.intno = APM_INTNO;
    bb.eax.r.h = APM_INTCODE;
    bb.eax.r.l = 0x03;
    bb.ebx.rr = 0x0000;
    bios(&bb);
    if (bb.flags.cf == 0) {
        /* Success */
        kbp->apmConfig.cs32_base = (bb.eax.rr) << 4;
        kbp->apmConfig.entry_offset = bb.ebx.rx;
        kbp->apmConfig.cs16_base = (bb.ecx.rr) << 4;
        kbp->apmConfig.ds_base = (bb.edx.rr) << 4;
        if (kbp->apmConfig.major_vers >= 1 &&
            kbp->apmConfig.minor_vers >= 1) {
            kbp->apmConfig.cs_length = bb.esi.rr;
            kbp->apmConfig.ds_length = bb.edi.rr;
        } else {
            kbp->apmConfig.cs_length = 
                kbp->apmConfig.ds_length = 64 * 1024;
        }
        kbp->apmConfig.connected = 1;
        return 1;
    }
    return 0;
}

#ifdef EISA_SUPPORT
BOOL
eisa_present(
    void
)
{
    static BOOL checked;
    static BOOL isEISA;

    if (!checked) {
        if (strncmp((char *)0xfffd9, "EISA", 4) == 0)
            isEISA = TRUE;

        checked = TRUE;
    }
    
    return (isEISA);
}

int
ReadEISASlotInfo(EISA_slot_info_t *ep, int slot)
{
    union {
        struct {
            unsigned char char2h :2;
            unsigned char char1  :5;
            unsigned char char3  :5;
            unsigned char char2l :3;
            unsigned char d2     :4;
            unsigned char d1     :4;
            unsigned char d4     :4;
            unsigned char d3     :4;
        } s;
        unsigned char data[4];
    } u;
    static char hex[0x10] = "0123456789ABCDEF";

    
    bb.intno = 0x15;
    bb.eax.r.h = 0xd8;
    bb.eax.r.l = 0x00;
    bb.ecx.r.l = slot;
    bios(&bb);
    if (bb.flags.cf)
        return bb.eax.r.h;
    ep->u_ID.d = bb.eax.r.l;
    ep->configMajor = bb.ebx.r.h;
    ep->configMinor = bb.ebx.r.l;
    ep->checksum = bb.ecx.rr;
    ep->numFunctions = bb.edx.r.h;
    ep->u_resources.d = bb.edx.r.l;
    u.data[0] = bb.edi.r.l;
    u.data[1] = bb.edi.r.h;
    u.data[2] = bb.esi.r.l;
    u.data[3] = bb.esi.r.h;
    ep->id[0] = u.s.char1 + ('A' - 1);
    ep->id[1] = (u.s.char2l | (u.s.char2h << 3)) + ('A' - 1);
    ep->id[2] = u.s.char3 + ('A' - 1);
    ep->id[3] = hex[u.s.d1];
    ep->id[4] = hex[u.s.d2];
    ep->id[5] = hex[u.s.d3];
    ep->id[6] = hex[u.s.d4];
    ep->id[7] = 0;
    return 0;
}

/*
 * Note: ep must point to an address below 64k.
 */

int
ReadEISAFuncInfo(EISA_func_info_t *ep, int slot, int function)
{
    bb.intno = 0x15;
    bb.eax.r.h = 0xd8;
    bb.eax.r.l = 0x01;
    bb.ecx.r.l = slot;
    bb.ecx.r.h = function;
    bb.esi.rr = (unsigned int)ep->data;
    bios(&bb);
    if (bb.eax.r.h == 0) {
        ep->slot = slot;
        ep->function = function;
    }
    return bb.eax.r.h;
}
#endif /* EISA_SUPPORT */

#define PCI_SIGNATURE 0x20494350  /* "PCI " */

int
ReadPCIBusInfo(PCI_bus_info_t *pp)
{
    bb.intno = 0x1a;
    bb.eax.r.h = 0xb1;
    bb.eax.r.l = 0x01;
    bios(&bb);
    if ((bb.eax.r.h == 0) && (bb.edx.rx == PCI_SIGNATURE)) {
        pp->BIOSPresent = 1;
        pp->u_bus.d = bb.eax.r.l;
        pp->majorVersion = bb.ebx.r.h;
        pp->minorVersion = bb.ebx.r.l;
        pp->maxBusNum = bb.ecx.r.l;
        return 0;
    }
    return -1;
}
