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
#include "bootstruct.h"
#include "sl.h"

/*
 * The user asked for boot graphics.
 */
BOOL gBootGraphics = NO;
long gBootMode = kBootModeNormal;
static char gBootKernelCacheFile[512];

static BOOL gUnloadPXEOnExit = 0;

/*
 * How long to wait (in seconds) to load the
 * kernel after displaying the "boot:" prompt.
 */
#define kBootErrorTimeout 5

/*
 * Default path to kernel cache file
 */
#define kDefaultCachePath "/System/Library/Caches/com.apple.kernelcaches/kernelcache"

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
// Malloc error function

static void malloc_error(char *addr, size_t size)
{
    printf("\nMemory allocation error (0x%x, 0x%x)\n",
           (unsigned)addr, (unsigned)size);
    asm("hlt");
}

//==========================================================================
// execKernel - Load the kernel image (mach-o) and jump to its entry point.

static int ExecKernel(void *binary)
{
    entry_t                   kernelEntry;
    int                       ret;

    bootArgs->kaddr = bootArgs->ksize = 0;

    ret = DecodeKernel(binary,
                       &kernelEntry,
                       (char **) &bootArgs->kaddr,
                       &bootArgs->ksize );

    if ( ret != 0 )
        return ret;

    // Reserve space for boot args
    reserveKernBootStruct();

    // Load boot drivers from the specifed root path.

    if (!gHaveKernelCache) {
          LoadDrivers("/");
    }

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

    if ( (gBootFileType == kNetworkDeviceType) && gUnloadPXEOnExit ) {
		if ( (ret = nbpUnloadBaseCode()) != nbpStatusSuccess )
        {
        	printf("nbpUnloadBaseCode error %d\n", (int) ret);
            sleep(2);
        }
    }

    // Switch to desired video mode just before starting the kernel.

    setVideoMode( gBootGraphics ? GRAPHICS_MODE : TEXT_MODE );

    // Jump to kernel's entry point. There's no going back now.

    startprog( kernelEntry, bootArgs );

    // Not reached

    return 0;
}

//==========================================================================
// Scan and record the system's hardware information.

static void scanHardware()
{
    extern int  ReadPCIBusInfo(PCI_bus_info_t *);
    extern void PCI_Bus_Init(PCI_bus_info_t *);

    ReadPCIBusInfo( &bootArgs->pciInfo );
    
    //
    // Initialize PCI matching support in the booter.
    // Not used, commented out to minimize code size.
    //
    // PCI_Bus_Init( &bootArgs->pciInfo );
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
    int      status;
    char     *bootFile;

    zeroBSS();

    // Initialize malloc

    malloc_init(0, 0, 0, malloc_error);

    // Enable A20 gate before accessing memory above 1Mb.

    enableA20();

    // Set reminder to unload the PXE base code. Neglect to unload
    // the base code will result in a hang or kernel panic.

    gUnloadPXEOnExit = 1;

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
    printf( bootBanner, bootArgs->convmem, bootArgs->extmem );
    printVBEInfo();

    // Parse args, load and start kernel.

    while (1)
    {
        const char *val;
        int len, trycache;
        long flags, cachetime, time;
        int ret = -1;

        // Initialize globals.

        sysConfigValid = 0;
        gErrors        = 0;

        // Reset config space.
        bootArgs->configEnd = bootArgs->config;

        getBootOptions();
        status = processBootOptions();
        if ( status ==  1 ) break;
        if ( status == -1 ) continue;

        // Found and loaded a config file. Proceed with boot.

        // Check for cache file.

        if (getValueForKey(kKernelCacheKey, &val, &len)) {
            strncpy(gBootKernelCacheFile, val, len);
            gBootKernelCacheFile[len] = '\0';
        } else {
            strcpy(gBootKernelCacheFile, kDefaultCachePath);
        }

        trycache = (((gBootMode & kBootModeSafe) == 0) &&
                    (gBootFileType == kBlockDeviceType) &&
                    (gBootKernelCacheFile[0] != '\0'));

        printf("Loading Darwin/x86\n");

        if (trycache) do {
      
            // if we haven't found the kernel yet, don't use the cache
            ret = GetFileInfo(NULL, bootArgs->bootFile, &flags, &time);
            if ((ret != 0) || ((flags & kFileTypeMask) != kFileTypeFlat)) {
                trycache = 0;
                break;
            }
            ret = GetFileInfo(NULL, gBootKernelCacheFile, &flags, &cachetime);
            if ((ret != 0) || ((flags & kFileTypeMask) != kFileTypeFlat)
                || (cachetime < time)) {
                trycache = 0;
                break;
            }
            ret = GetFileInfo("/System/Library/", "Extensions", &flags, &time);
            if ((ret == 0) && ((flags & kFileTypeMask) == kFileTypeDirectory)
                && (cachetime < time)) {
                trycache = 0;
                break;
            }
        } while (0);

        do {
            if (trycache) {
                bootFile = gBootKernelCacheFile;
                verbose("Loading kernel cache %s\n", bootFile);
                ret = LoadFile(bootFile);
                if (ret >= 0) {
                    break;
                }
            }
            bootFile = bootArgs->bootFile;
            verbose("Loading kernel %s\n", bootFile);
            ret = LoadFile(bootFile);
        } while (0);

        clearActivityIndicator();

        if (ret < 0) {
            error("Can't find %s\n", bootFile);

            if ( gBootFileType == kBIOSDevTypeFloppy )
            {
                // floppy in drive, but failed to load kernel.
                gBIOSDev = kBIOSDevTypeHardDrive;
                initKernBootStruct( gBIOSDev );
                printf("Attempt to load from hard drive.");
            }
            else if ( gBootFileType == kNetworkDeviceType )
            {
                // Return control back to PXE. Don't unload PXE base code.
                gUnloadPXEOnExit = 0;
                break;
            }
        } else {
            /* Won't return if successful. */
            ret = ExecKernel((void *)kLoadAddr);
        }

    } /* while(1) */
    
    if ((gBootFileType == kNetworkDeviceType) && gUnloadPXEOnExit) {
	nbpUnloadBaseCode();
    }
}
