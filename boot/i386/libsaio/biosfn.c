/*
 * Copyright (c) 1999-2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Portions Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 2.0 (the "License").  You may not use this file
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

#include "bootstruct.h"
#include "libsaio.h"

#define MAX_DRIVES 8

static biosBuf_t bb;

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

unsigned long getMemoryMap( MemoryRange *   rangeArray,
                            unsigned long   maxRangeCount,
                            unsigned long * conMemSizePtr,
                            unsigned long * extMemSizePtr )
{
    #define kMemoryMapSignature  'SMAP'
    #define kDescriptorSizeMin   20

    MemoryRange *        range = rangeArray;
    unsigned long        count = 0;
    unsigned long long   conMemSize = 0;
    unsigned long long   extMemSize = 0;

    // The memory pointed by the rangeArray must reside within the
    // first megabyte.
    //
    // Prepare for the INT15 E820h call. Each call returns a single
    // memory range. A continuation value is returned that must be
    // provided on a subsequent call to fetch the next range.
    //
    // Certain BIOSes (Award 6.00PG) expect the upper word in EAX
    // to be cleared on entry, otherwise only a single range will
    // be reported.
    //
    // Some BIOSes will simply ignore the value of ECX on entry.
    // Probably best to keep its value at 20 to avoid surprises.

    bb.ebx.rx = 0;  // Initial continuation value must be zero.

    while ( count < maxRangeCount )
    {
        bb.intno  = 0x15;
        bb.eax.rx = 0xe820;
        bb.ecx.rx = kDescriptorSizeMin;
        bb.edx.rx = kMemoryMapSignature;
        bb.edi.rr = OFFSET(  (unsigned long) range );
        bb.es     = SEGMENT( (unsigned long) range );
        bios(&bb);

        // Check for errors.

        if ( bb.flags.cf
        ||   bb.eax.rx != kMemoryMapSignature
        ||   bb.ecx.rx != kDescriptorSizeMin ) break;

        // Tally up the conventional/extended memory sizes.

        if ( range->type == kMemoryRangeUsable ||
             range->type == kMemoryRangeACPI   ||
             range->type == kMemoryRangeNVS )
        {
            // Tally the conventional memory ranges.
            if ( range->base + range->length <= 0xa0000 ) {
                conMemSize += range->length;
            } 

            // Record the top of extended memory.
            if ( range->base >= EXTENDED_ADDR ) {
                extMemSize += range->length;
            }
        }

        range++;
        count++;

        // Is this the last address range?

        if ( bb.ebx.rx == 0 ) break;
    }
    *conMemSizePtr = conMemSize / 1024;  // size in KB
    *extMemSizePtr = extMemSize  / 1024;  // size in KB

#if DEBUG
    {
        for (range = rangeArray; range->length != 0; range++) {
        printf("range: type %d, base 0x%x, length 0x%x\n",
               range->type, (unsigned int)range->base, (unsigned int)range->length); getc();

        }
    }
#endif

    return count;
}

unsigned long getExtendedMemorySize()
{
    // Get extended memory size for large configurations. Not used unless
    // the INT15, E820H call (Get System Address Map) failed.
    //
    // Input:
    //
    // AX   Function Code   E801h
    //
    // Outputs:
    //
    // CF   Carry Flag      Carry cleared indicates no error.
    // AX   Extended 1      Number of contiguous KB between 1 and 16 MB,
    //                      maximum 0x3C00 = 15 MB.
    // BX   Extended 2      Number of contiguous 64 KB blocks between
    //                      16 MB and 4 GB.
    // CX   Configured 1    Number of contiguous KB between 1 and 16 MB,
    //                      maximum 0x3C00 = 15 MB.
    // DX   Configured 2    Number of contiguous 64 KB blocks between
    //                      16 MB and 4 GB.

    bb.intno  = 0x15;
    bb.eax.rx = 0xe801;
    bios(&bb);

    // Return the size of memory above 1MB (extended memory) in kilobytes.

    if ( bb.flags.cf == 0 ) return (bb.ebx.rr * 64 + bb.eax.rr);

    // Get Extended memory size. Called on last resort since the return
    // value is limited to 16-bits (a little less than 64MB max). May
    // not be supported by modern BIOSes.
    //
    // Input:
    //
    // AX   Function Code   E801h
    //
    // Outputs:
    //
    // CF   Carry Flag      Carry cleared indicates no error.
    // AX   Memory Count    Number of contiguous KB above 1MB.

    bb.intno  = 0x15;
    bb.eax.rx = 0x88;
    bios(&bb);

    // Return the size of memory above 1MB (extended memory) in kilobytes.

    return bb.flags.cf ? 0 : bb.eax.rr;
}

unsigned long getConventionalMemorySize()
{
    bb.intno = 0x12;
    bios(&bb);
    return bb.eax.rr;  // kilobytes
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
    } addrpacket __attribute__((aligned(16))) = {0};
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

int ebioswrite(int dev, long sec, int count)
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
    } addrpacket __attribute__((aligned(16))) = {0};
    addrpacket.size = sizeof(addrpacket);

    for (i=0;;) {
        bb.intno   = 0x13;
        bb.eax.r.l = 0; /* Don't verify */
        bb.eax.r.h = 0x43;
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

/* Check to see if the passed-in drive is in El Torito no-emulation mode. */
int is_no_emulation(int drive)
{
    struct packet {
	unsigned char packet_size;
	unsigned char media_type;
	unsigned char drive_num;
	unsigned char ctrlr_index;
	unsigned long lba;
	unsigned short device_spec;
	unsigned short buffer_segment;
	unsigned short load_segment;
	unsigned short sector_count;
	unsigned char cyl_count;
	unsigned char sec_count;
	unsigned char head_count;
	unsigned char reseved;
    } __attribute__((packed));
    static struct packet pkt;

    bzero(&pkt, sizeof(pkt));
    pkt.packet_size = 0x13;

    bb.intno   = 0x13;
    bb.eax.r.h = 0x4b;
    bb.eax.r.l = 0x01;     // subfunc: get info
    bb.edx.r.l = drive;
    bb.esi.rr = OFFSET((unsigned)&pkt);
    bb.ds     = SEGMENT((unsigned)&pkt);

    bios(&bb);
#if DEBUG
    printf("el_torito info drive %x\n", drive);

    printf("--> cf %x, eax %x\n", bb.flags.cf, bb.eax.rr);

    printf("pkt_size: %x\n", pkt.packet_size);
    printf("media_type: %x\n", pkt.media_type);
    printf("drive_num: %x\n", pkt.drive_num);
    printf("device_spec: %x\n", pkt.device_spec);
    printf("press a key->\n");getc();
#endif

    /* Some BIOSes erroneously return cf = 1 */
    /* Just check to see if the drive number is the same. */
    if (pkt.drive_num == drive) {
	if ((pkt.media_type & 0x0F) == 0) {
	    /* We are in no-emulation mode. */
	    return 1;
	}
    }
    
    return 0;
}

#if DEBUG
/*
 * BIOS drive information.
 */
void print_drive_info(boot_drive_info_t *dp)
{
    //    printf("buf_size = %x\n", dp->params.buf_size);
    printf("info_flags = %x\n", dp->params.info_flags);
    printf(" phys_cyls = %lx\n", dp->params. phys_cyls);
    printf(" phys_heads = %lx\n", dp->params. phys_heads);
    printf(" phys_spt = %lx\n", dp->params. phys_spt);
    printf("phys_sectors = %lx%lx\n", ((unsigned long *)(&dp->params.phys_sectors))[1],
				      ((unsigned long *)(&dp->params.phys_sectors))[0]);
    printf("phys_nbps = %x\n", dp->params.phys_nbps);
    //    printf("dpte_offset = %x\n", dp->params.dpte_offset);
    //    printf("dpte_segment = %x\n", dp->params.dpte_segment);
    //    printf("key = %x\n", dp->params.key);
    //    printf("path_len = %x\n", dp->params. path_len);
    //    printf("reserved1 = %x\n", dp->params. reserved1);
    //    printf("reserved2 = %x\n", dp->params.reserved2);
    //printf("bus_type[4] = %x\n", dp->params. bus_type[4]);
    //printf("interface_type[8] = %x\n", dp->params. interface_type[8]);
    //printf("interface_path[8] = %x\n", dp->params. interface_path[8]);
    //printf("dev_path[8] = %x\n", dp->params. dev_path[8]);
    //    printf("reserved3 = %x\n", dp->params. reserved3);
    //    printf("checksum = %x\n", dp->params. checksum);

    printf(" io_port_base = %x\n", dp->dpte.io_port_base);
    printf(" control_port_base = %x\n", dp->dpte.control_port_base);
    printf("  head_flags = %x\n", dp->dpte. head_flags);
    printf("  vendor_info = %x\n", dp->dpte. vendor_info);
    printf("  irq = %x\n", dp->dpte. irq);
    //    printf("  irq_unused = %x\n", dp->dpte. irq_unused);
    printf("  block_count = %x\n", dp->dpte. block_count);
    printf("  dma_channe = %x\n", dp->dpte. dma_channel);
    printf("  dma_type = %x\n", dp->dpte. dma_type);
    printf("  pio_type = %x\n", dp->dpte. pio_type);
    printf("  pio_unused = %x\n", dp->dpte. pio_unused);
    printf(" option_flags = %x\n", dp->dpte.option_flags);
    //    printf(" reserved = %x\n", dp->dpte.reserved);
    printf("  revision = %x\n", dp->dpte. revision);
    //    printf("  checksum = %x\n", dp->dpte. checksum);
}

#endif

int get_drive_info(int drive, struct driveInfo *dp)
{
    boot_drive_info_t *di = &dp->di;
    int ret = 0;

#if UNUSED
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
#endif

    bzero(dp, sizeof(struct driveInfo));
    dp->biosdev = drive;

    /* Check for El Torito no-emulation mode. */
    dp->no_emulation = is_no_emulation(drive);

    /* Check drive for EBIOS support. */
    bb.intno = 0x13;
    bb.eax.r.h = 0x41;
    bb.edx.r.l = drive;
    bb.ebx.rr = 0x55aa;
    bios(&bb);
    if((bb.ebx.rr == 0xaa55) && (bb.flags.cf == 0)) {
        /* Get flags for supported operations. */
        dp->uses_ebios = bb.ecx.r.l;
    }

    if (dp->uses_ebios & (EBIOS_ENHANCED_DRIVE_INFO | EBIOS_LOCKING_ACCESS | EBIOS_FIXED_DISK_ACCESS)) {
        /* Get EBIOS drive info. */
	static struct drive_params params;

        params.buf_size = sizeof(params);
        bb.intno = 0x13;
        bb.eax.r.h = 0x48;
        bb.edx.r.l = drive;
        bb.esi.rr = OFFSET((unsigned)&params);
        bb.ds     = SEGMENT((unsigned)&params);
        bios(&bb);
        if(bb.flags.cf != 0 /* || params.phys_sectors < 2097152 */) {
            dp->uses_ebios = 0;
	    di->params.buf_size = 1;
        } else {
	    bcopy(&params, &di->params, sizeof(params));

	    if (drive >= BASE_HD_DRIVE &&
		   (dp->uses_ebios & EBIOS_ENHANCED_DRIVE_INFO) &&
		   di->params.buf_size >= 30 &&
		   !(di->params.dpte_offset == 0xFFFF && di->params.dpte_segment == 0xFFFF)) {
		void *ptr = (void *)(di->params.dpte_offset + ((unsigned int)di->params.dpte_segment << 4));
		bcopy(ptr, &di->dpte, sizeof(di->dpte));
	    }
	}
    }

    if (di->params.phys_heads == 0 || di->params.phys_spt == 0) {
	/* Either it's not EBIOS, or EBIOS didn't tell us. */
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
	    if((dp->uses_ebios & EBIOS_ENHANCED_DRIVE_INFO) && (sec != 0)) {
		cyl = (di->params.phys_sectors / ((hds + 1) * sec)) - 1;
	    }
	    else {
		cyl = bb.ecx.r.h | ((bb.ecx.r.l & 0xC0) << 2);
	    }
	    di->params.phys_heads = hds; 
	    di->params.phys_spt = sec;
	    di->params.phys_cyls = cyl;
	} else {
	    ret = -1;
	}
    }

    if (dp->no_emulation) {
        /* Some BIOSes give us erroneous EBIOS support information.
	 * Assume that if you're on a CD, then you can use
	 * EBIOS disk calls.
	 */
        dp->uses_ebios |= EBIOS_FIXED_DISK_ACCESS;
    }
#if DEBUG
    print_drive_info(di);
    printf("uses_ebios = 0x%x\n", dp->uses_ebios);
    printf("press a key->\n");getc();
#endif

    if (ret == 0) {
	 dp->valid = 1;
    }
    return ret;
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
    static char cd_spec[0x13];

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

#ifdef APM_SUPPORT

#define APM_INTNO   0x15
#define APM_INTCODE 0x53

int
APMPresent(void)
{
    bb.intno = APM_INTNO;
    bb.eax.r.h = APM_INTCODE;
    bb.eax.r.l = 0x00;
    bb.ebx.rr = 0x0000;
    bios(&bb);
    if ((bb.flags.cf == 0) &&
        (bb.ebx.r.h == 'P') &&
        (bb.ebx.r.l == 'M')) {
        /* Success */
        bootArgs->apmConfig.major_vers = bb.eax.r.h;
        bootArgs->apmConfig.minor_vers = bb.eax.r.l;
        bootArgs->apmConfig.flags.data = bb.ecx.rr;
        return 1;
    }
    return 0;
}

int
APMConnect32(void)
{
    bb.intno = APM_INTNO;
    bb.eax.r.h = APM_INTCODE;
    bb.eax.r.l = 0x03;
    bb.ebx.rr = 0x0000;
    bios(&bb);
    if (bb.flags.cf == 0) {
        /* Success */
        bootArgs->apmConfig.cs32_base = (bb.eax.rr) << 4;
        bootArgs->apmConfig.entry_offset = bb.ebx.rx;
        bootArgs->apmConfig.cs16_base = (bb.ecx.rr) << 4;
        bootArgs->apmConfig.ds_base = (bb.edx.rr) << 4;
        if (bootArgs->apmConfig.major_vers >= 1 &&
            bootArgs->apmConfig.minor_vers >= 1) {
            bootArgs->apmConfig.cs_length = bb.esi.rr;
            bootArgs->apmConfig.ds_length = bb.edi.rr;
        } else {
            bootArgs->apmConfig.cs_length = 
                bootArgs->apmConfig.ds_length = 64 * 1024;
        }
        bootArgs->apmConfig.connected = 1;
        return 1;
    }
    return 0;
}

#endif /* APM_SUPPORT */

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

void sleep(int n)
{
    unsigned int endtime = (time18() + 18*n);
    while (time18() < endtime);
}

void delay(int ms)
{
    bb.intno = 0x15;
    bb.eax.r.h = 0x86;
    bb.ecx.rr = ms >> 16;
    bb.edx.rr = ms & 0xFFFF;
    bios(&bb);
}

