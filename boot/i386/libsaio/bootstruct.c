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
#include "kernBootStruct.h"

// CMOS access ports in I/O space.
//
#define CMOSADDR    0x70
#define CMOSDATA    0x71
#define HDTYPE      0x12

/*==========================================================================
 * Returns the number of active ATA drives since these will increment the
 * bios device numbers of SCSI drives.
 */
static int
countIDEDisks()
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

KERNBOOTSTRUCT * kernBootStruct = (KERNBOOTSTRUCT *) KERNSTRUCT_ADDR;

void
initKernBootStruct()
{
    unsigned char i;

    bzero( (char *) kernBootStruct, sizeof(*kernBootStruct) );    
    
    // Get size of conventional memory.

    kernBootStruct->convmem = memsize(0);

    // Get size of extended memory.

    kernBootStruct->extmem  = memsize(1);    

    // Get number of ATA devices.

    kernBootStruct->numIDEs = countIDEDisks();

    // Get disk info from BIOS.

    for ( i = 0; i < 4; i++ )
    {
        kernBootStruct->diskInfo[i] = get_diskinfo(0x80 + i);
    }

    kernBootStruct->magicCookie  = KERNBOOTMAGIC;
    kernBootStruct->configEnd    = kernBootStruct->config;
    kernBootStruct->graphicsMode = GRAPHICS_MODE;
}
