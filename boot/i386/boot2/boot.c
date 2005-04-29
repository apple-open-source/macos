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
#include "libsa.h"


long gBootMode; /* defaults to 0 == kBootModeNormal */
BOOL gOverrideKernel;
static char gBootKernelCacheFile[512];
static char gCacheNameAdler[64 + 256];
char *gPlatformName = gCacheNameAdler;
char gMKextName[512];
BVRef gBootVolume;

static
unsigned long Adler32(unsigned char *buffer, long length);


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
#ifdef APM_SUPPORT
    BOOL                      apm;
#endif /* APM_SUPPORT */
    BOOL                      bootGraphics;

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

#ifdef APM_SUPPORT
    // Connect to APM BIOS.

    if ( getBoolForKey("APM", &apm) && apm == YES )
    {
        if ( APMPresent() ) APMConnect32();
    }
#endif /* APM_SUPPORT */

    // Cleanup the PXE base code.

    if ( (gBootFileType == kNetworkDeviceType) && gUnloadPXEOnExit ) {
		if ( (ret = nbpUnloadBaseCode()) != nbpStatusSuccess )
        {
        	printf("nbpUnloadBaseCode error %d\n", (int) ret);
            sleep(2);
        }
    }

    // Unless Boot Graphics = No, Always switch to graphics mode
    // just before starting the kernel.

    if (!getBoolForKey(kBootGraphicsKey, &bootGraphics)) {
        bootGraphics = YES;
    }

    if (bootGraphics) {
        if (bootArgs->graphicsMode == TEXT_MODE) {
            // If we were in text mode, switch to graphics mode.
            // This will draw the boot graphics.
            setVideoMode( GRAPHICS_MODE );
        } else {
            // If we were already in graphics mode, clear the screen.
            drawBootGraphics();
        }
    } else {
        if (bootArgs->graphicsMode == GRAPHICS_MODE) {
            setVideoMode( TEXT_MODE );
        }
    }

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

#define DLOG(x) outb(0x80, (x))

void boot(int biosdev)
{
    int      status;
    char     *bootFile;
    unsigned long adler32;
    BOOL     quiet;
    BOOL     firstRun = YES;
    BVRef    bvChain;

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

    // First get info for boot volume.
    bvChain = scanBootVolumes(gBIOSDev, 0);

    // Record default boot device.
    gBootVolume = selectBootVolume(bvChain);
    bootArgs->kernDev = MAKEKERNDEV(gBIOSDev,
                                    BIOS_DEV_UNIT(gBootVolume),
                                    gBootVolume->part_no );

    // Load default config file from boot device.
    status = loadSystemConfig(0, 0);

    if ( getBoolForKey( kQuietBootKey, &quiet ) && quiet ) {
        gBootMode |= kBootModeQuiet;
    }

    // Parse args, load and start kernel.

    while (1)
    {
        const char *val;
        int len;
        int trycache;
        long flags, cachetime, kerneltime, exttime;
        int ret = -1;

        // Initialize globals.

        sysConfigValid = 0;
        gErrors        = 0;

        status = getBootOptions(firstRun);
        firstRun = NO;
        if (status == -1) continue;

        status = processBootOptions();
        if ( status ==  1 ) break;
        if ( status == -1 ) continue;

        // Found and loaded a config file. Proceed with boot.


        // Reset cache name.
        bzero(gCacheNameAdler, sizeof(gCacheNameAdler));

        if ( getValueForKey( kRootDeviceKey, &val, &len ) == YES ) {
            if (*val == '*') {
                val++;
                len--;
            }
            strncpy( gCacheNameAdler + 64, val, len );
            sprintf(gCacheNameAdler + 64 + len, ",%s", bootArgs->bootFile);
        } else {
            strcpy(gCacheNameAdler + 64, bootArgs->bootFile);
        }
        adler32 = Adler32(gCacheNameAdler, sizeof(gCacheNameAdler));

        if (getValueForKey(kKernelCacheKey, &val, &len) == YES) {
            strlcpy(gBootKernelCacheFile, val, len+1);
        } else {
            sprintf(gBootKernelCacheFile, "%s.%08lX", kDefaultCachePath, adler32);
        }

        // Check for cache file.


        trycache = (((gBootMode & kBootModeSafe) == 0) &&
                    (gOverrideKernel == NO) &&
                    (gBootFileType == kBlockDeviceType) &&
                    (gMKextName[0] == '\0') &&
                    (gBootKernelCacheFile[0] != '\0'));

        printf("Loading Darwin/x86\n");

        if (trycache) do {
      
            // if we haven't found the kernel yet, don't use the cache
            ret = GetFileInfo(NULL, bootArgs->bootFile, &flags, &kerneltime);
            if ((ret != 0) || ((flags & kFileTypeMask) != kFileTypeFlat)) {
                trycache = 0;
                break;
            }
            ret = GetFileInfo(NULL, gBootKernelCacheFile, &flags, &cachetime);
            if ((ret != 0) || ((flags & kFileTypeMask) != kFileTypeFlat)
                || (cachetime < kerneltime)) {
                trycache = 0;
                break;
            }
            ret = GetFileInfo("/System/Library/", "Extensions", &flags, &exttime);
            if ((ret == 0) && ((flags & kFileTypeMask) == kFileTypeDirectory)
                && (cachetime < exttime)) {
                trycache = 0;
                break;
            }
            if (kerneltime > exttime) {
                exttime = kerneltime;
            }
            if (cachetime != (exttime + 1)) {
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
#if DEBUG
        printf("Pausing...");
        sleep(8);
#endif

        if (ret < 0) {
            error("Can't find %s\n", bootFile);

            if ( gBootFileType == kBIOSDevTypeFloppy )
            {
                // floppy in drive, but failed to load kernel.
                gBIOSDev = kBIOSDevTypeHardDrive;
                initKernBootStruct( gBIOSDev );
                printf("Attempting to load from hard drive...");
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

#define BASE 65521L /* largest prime smaller than 65536 */
#define NMAX 5000
// NMAX (was 5521) the largest n such that 255n(n+1)/2 + (n+1)(BASE-1) <= 2^32-1

#define DO1(buf,i)  {s1 += buf[i]; s2 += s1;}
#define DO2(buf,i)  DO1(buf,i); DO1(buf,i+1);
#define DO4(buf,i)  DO2(buf,i); DO2(buf,i+2);
#define DO8(buf,i)  DO4(buf,i); DO4(buf,i+4);
#define DO16(buf)   DO8(buf,0); DO8(buf,8);

unsigned long Adler32(unsigned char *buf, long len)
{
    unsigned long s1 = 1; // adler & 0xffff;
    unsigned long s2 = 0; // (adler >> 16) & 0xffff;
    unsigned long result;
    int k;

    while (len > 0) {
        k = len < NMAX ? len : NMAX;
        len -= k;
        while (k >= 16) {
            DO16(buf);
            buf += 16;
            k -= 16;
        }
        if (k != 0) do {
            s1 += *buf++;
            s2 += s1;
        } while (--k);
        s1 %= BASE;
        s2 %= BASE;
    }
    result = (s2 << 16) | s1;
    return OSSwapHostToBigInt32(result);
}

