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
 * Mach Operating System
 * Copyright (c) 1990 Carnegie-Mellon University
 * Copyright (c) 1989 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */

/*
 *          INTEL CORPORATION PROPRIETARY INFORMATION
 *
 *  This software is supplied under the terms of a license  agreement or 
 *  nondisclosure agreement with Intel Corporation and may not be copied 
 *  nor disclosed except in accordance with the terms of that agreement.
 *
 *  Copyright 1988, 1989 by Intel Corporation
 */

/*
 * Copyright 1993 NeXT Computer, Inc.
 * All rights reserved.
 */
 
/*
 * Completely reworked by Sam Streeper (sam_s@NeXT.com)
 * Reworked again by Curtis Galloway (galloway@NeXT.com)
 */

#include "boot.h"

/*
 * The user asked for boot graphics.
 */
BOOL gBootGraphics = NO;
int  gBIOSDev;
long gBootMode = kBootModeNormal;

static BOOL gUnloadPXEOnExit = 0;

/*
 * How long to wait (in seconds) to load the
 * kernel after displaying the "boot:" prompt.
 */
#define kBootErrorTimeout 5

//==========================================================================
// Zero the BSS.

static void zeroBSS()
{
    extern char _DATA__bss__begin, _DATA__bss__end;
    extern char _DATA__common__begin, _DATA__common__end;

    bzero( &_DATA__bss__begin,
           (&_DATA__bss__end - &_DATA__bss__begin) );

    bzero( &_DATA__common__begin, 
           (&_DATA__common__end - &_DATA__common__begin) );
}

//==========================================================================
// execKernel - Load the kernel image (mach-o) and jump to its entry point.

static int execKernel(int fd)
{
    register KERNBOOTSTRUCT * kbp = kernBootStruct;
    static struct mach_header head;
    entry_t                   kernelEntry;
    int                       ret;

    verbose("Loading kernel %s\n", kbp->bootFile);

    // Perform the actual load.

    kbp->kaddr = kbp->ksize = 0;

    ret = loadprog( kbp->kernDev, fd,
                    &head,
                    &kernelEntry,
                    (char **) &kbp->kaddr,
                    &kbp->ksize );
    close(fd);
    clearActivityIndicator();

    if ( ret != 0 )
        return ret;

    // Load boot drivers from the specifed root path.

    LoadDrivers("/");
    clearActivityIndicator();

    if (gErrors) {
        printf("Errors encountered while starting up the computer.\n");
        printf("Pausing %d seconds...\n", kBootErrorTimeout);
        sleep(kBootErrorTimeout);
    }

    printf("Starting Darwin/x86");

    turnOffFloppy();

    // Connect to APM BIOS.

    if ( getBoolForKey("APM") )
    {
        if ( APMPresent() ) APMConnect32();
    }

	// Cleanup the PXE base code.

	if ( gUnloadPXEOnExit )
    {
		if ( (ret = nbpUnloadBaseCode()) != nbpStatusSuccess )
        {
        	printf("nbpUnloadBaseCode error %d\n", (int) ret);
            sleep(2);
        }
    }

    // Switch to desired video mode just before starting the kernel.

    setVideoMode( gBootGraphics ? GRAPHICS_MODE : TEXT_MODE );

    // Jump to kernel's entry point. There's no going back now.

    startprog( kernelEntry );

    // Not reached

    return 0;
}

//==========================================================================
// Scan and record the system's hardware information.

static void scanHardware()
{
    extern int  ReadPCIBusInfo(PCI_bus_info_t *);
    extern void PCI_Bus_Init(PCI_bus_info_t *);

    ReadPCIBusInfo( &kernBootStruct->pciInfo );
    
    //
    // Initialize PCI matching support in the booter.
    // Not used, commented out to minimize code size.
    //
    // PCI_Bus_Init( &kernBootStruct->pciInfo );
}

//==========================================================================
// The 'main' function for the booter. Called by boot0 when booting
// from a block device, or by the network booter.
//
// arguments:
//   biosdev - Value passed from boot1/NBP to specify the device
//             that the booter was loaded from.
//
// If biosdev is kBIOSDevNetwork, then this function will return if
// booting was unsuccessful. This allows the PXE firmware to try the
// next boot device on its list.

void boot(int biosdev)
{
    register KERNBOOTSTRUCT * kbp = kernBootStruct;
    int      fd;
    int      status;

    zeroBSS();

    // Enable A20 gate before accessing memory above 1Mb.

    enableA20();

    // Set reminder to unload the PXE base code. Neglect to unload
    // the base code will result in a hang or kernel panic.

    if ( BIOS_DEV_TYPE(biosdev) == kBIOSDevTypeNetwork )
        gUnloadPXEOnExit = 1;

#if 0
    gUnloadPXEOnExit = 1;
    biosdev  = 0x80;
#endif

    // Record the device that the booter was loaded from.

    gBIOSDev = biosdev & kBIOSDevMask;

    // Initialize boot info structure.

    initKernBootStruct( gBIOSDev );

    // Setup VGA text mode.
    // Not sure if it is safe to call setVideoMode() before the
    // config table has been loaded. Call video_mode() instead.

	video_mode( 2 );  // 80x25 mono text mode.

    // Scan hardware configuration.

    scanHardware();

    // Display banner and show hardware info.

    setCursorPosition( 0, 0, 0 );
    printf( bootBanner, kbp->convmem, kbp->extmem );
    printVBEInfo();

    // Parse args, load and start kernel.

    while (1)
    {
        // Initialize globals.

        sysConfigValid = 0;
        gErrors        = 0;

        getBootOptions();
        status = processBootOptions();
        if ( status ==  1 ) break;
        if ( status == -1 ) continue;

        // Found and loaded a config file. Proceed with boot.

        printf("Loading Darwin/x86\n");

        if ( (fd = openfile( kbp->bootFile, 0 )) >= 0 )
        {
            execKernel(fd);  // will not return on success
        }
        else
        {
            error("Can't find %s\n", kbp->bootFile);

            if ( BIOS_DEV_TYPE(gBIOSDev) == kBIOSDevTypeFloppy )
            {
                // floppy in drive, but failed to load kernel.
                gBIOSDev = kBIOSDevTypeHardDrive;
                initKernBootStruct( gBIOSDev );
                printf("Attempt to load from hard drive.");
            }
            else if ( BIOS_DEV_TYPE(gBIOSDev) == kBIOSDevTypeNetwork )
            {
                // Return control back to PXE. Don't unload PXE base code.
                gUnloadPXEOnExit = 0;
                break;
            }
        }
    } /* while(1) */
    
    if (gUnloadPXEOnExit) nbpUnloadBaseCode();
}
