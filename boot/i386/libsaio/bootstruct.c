/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * Copyright 1993 NeXT, Inc.
 * All rights reserved.
 */

#include "libsaio.h"
#include "bootstruct.h"

// CMOS access ports in I/O space.
//
#define CMOSADDR    0x70
#define CMOSDATA    0x71
#define HDTYPE      0x12

/*==========================================================================
 * Returns the number of active ATA drives since these will increment the
 * bios device numbers of SCSI drives.
 */
static int countIDEDisks()
{
    int            count = 0;
    unsigned short hdtype;

#if DEBUG
    struct driveParameters param;

    printf("Reading drive parameters...\n");
    readDriveParameters(0x80, &param);
    printf("%d fixed disk drive(s) installed\n", param.totalDrives);
    for (count = 0; count < 256; count++)
    {
        if (readDriveParameters(count + 0x80, &param))
            break;
        else
        {
            printf("Drive %d: %d cyls, %d heads, %d sectors\n",
                   count, param.cylinders, param.heads, param.sectors);
        }
    }
    outb(CMOSADDR, 0x11);
    printf("CMOS addr 0x11 = %x\n",inb(CMOSDATA));
    outb(CMOSADDR, 0x12);
    printf("CMOS addr 0x12 = %x\n",inb(CMOSDATA));
    return count;
#endif

    outb( CMOSADDR, HDTYPE );
    hdtype = (unsigned short) inb( CMOSDATA );

    if (hdtype & 0xF0) count++;
    if (hdtype & 0x0F) count++;
    return count;
}

/*==========================================================================
 * Initialize the 'kernBootStruct'. A structure of parameters passed to
 * the kernel by the booter.
 */

KernelBootArgs_t *bootArgs;

void initKernBootStruct( int biosdev )
{
    static int init_done = 0;

    bootArgs = (KernelBootArgs_t *)KERNSTRUCT_ADDR;

    if ( !init_done )
    {
        bzero(bootArgs, sizeof(KernelBootArgs_t));

        // Get system memory map. Also update the size of the
        // conventional/extended memory for backwards compatibility.

        bootArgs->memoryMapCount =
            getMemoryMap( bootArgs->memoryMap, kMemoryMapCountMax,
                          (unsigned long *) &bootArgs->convmem,
                          (unsigned long *) &bootArgs->extmem );

        if ( bootArgs->memoryMapCount == 0 )
        {
            // BIOS did not provide a memory map, systems with
            // discontiguous memory or unusual memory hole locations
            // may have problems.

            bootArgs->convmem = getConventionalMemorySize();
            bootArgs->extmem  = getExtendedMemorySize();
        }

        bootArgs->magicCookie  = KERNBOOTMAGIC;
        bootArgs->configEnd    = bootArgs->config;
        bootArgs->graphicsMode = TEXT_MODE;
        
	/* New style */
	/* XXX initialize bootArgs here */

        init_done = 1;
    }

    // Get number of ATA devices.

    bootArgs->numDrives = countIDEDisks();

    // Update kernDev from biosdev.

    bootArgs->kernDev = biosdev;
}


/* Copy boot args after kernel and record address. */

void
reserveKernBootStruct(void)
{
    void *oldAddr = bootArgs;
    bootArgs = (KernelBootArgs_t *)AllocateKernelMemory(sizeof(KERNBOOTSTRUCT));
    bcopy(oldAddr, bootArgs, sizeof(KernelBootArgs_t));
}

