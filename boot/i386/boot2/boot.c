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
#include "libsa.h"
#include "memory.h"
#include "saio.h"
#include "libsaio.h"
#include "kernBootStruct.h"
#include "boot.h"
#include "nbp.h"

/*
 * The user asked for boot graphics.
 */
static BOOL gWantBootGraphics = NO;

/*
 * The device that the booter was loaded from.
 */
int    gBootDev;

extern char * gFilename;
extern BOOL   sysConfigValid;
extern char   bootPrompt[];
extern BOOL   errors;
extern BOOL   gVerboseMode;
extern BOOL   gSilentBoot;

/*
 * Prototypes.
 */
static void getBootString();

/*
 * How long to wait (in seconds) to load the
 * kernel after displaying the "boot:" prompt.
 */
#define kBootTimeout  10

//==========================================================================
// Zero the BSS.

static void
zeroBSS()
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

static int
execKernel(int fd)
{
    register KERNBOOTSTRUCT * kbp = kernBootStruct;
    static struct mach_header head;
    entry_t                   kernelEntry;
    int                       ret;

    verbose("Loading kernel %s\n", kbp->bootFile);

    // Perform the actual load.

    kbp->kaddr = kbp->ksize = 0;

    ret = loadprog( kbp->kernDev,
                    fd,
                    &head,
                    &kernelEntry,
                    (char **) &kbp->kaddr,
                    &kbp->ksize );
    close(fd);
    clearActivityIndicator();

    if ( ret != 0 )
        return ret;

    // Load boot drivers from the specifed root.

    LoadDrivers("/");
    clearActivityIndicator();

    if (errors) {
        printf("Errors encountered while starting up the computer.\n");
        printf("Pausing %d seconds...\n", kBootTimeout);
        sleep(kBootTimeout);
    }

    message("Starting Darwin/x86", 0);

    turnOffFloppy();

    // Connect to APM BIOS.

    if ( getBoolForKey("APM") )
    {
        if ( APMPresent() ) APMConnect32();
    }

	// Cleanup the PXE base code.

	if ( gBootDev == kBootDevNetwork )
    {
		if ( (ret = nbpUnloadBaseCode()) != nbpStatusSuccess )
        {
        	printf("nbpUnloadBaseCode error %d\n", (int) ret); sleep(2);
        }
    }

    // Switch to graphics mode just before starting the kernel.

    if ( gWantBootGraphics )
    {
        setMode(GRAPHICS_MODE);
    }

    // Jump to kernel's entry point. There's no going back now.

    startprog(kernelEntry);

    // Not reached

    return 0;
}

//==========================================================================
// Scan and record the system's PCI bus information.

static void scanHardware()
{
extern int  ReadPCIBusInfo(PCI_bus_info_t *);
extern void PCI_Bus_Init(PCI_bus_info_t *);

    ReadPCIBusInfo( &kernBootStruct->pciInfo );
    PCI_Bus_Init( &kernBootStruct->pciInfo );
}

//==========================================================================
// The 'main' function for the booter. This function is called by the
// assembly routine init(), which is in turn called by boot1 or by
// NBP.
//
// arguments:
//   bootdev - Value passed from boot1/NBP to specify the device
//             that the booter was loaded from. See boot.h for the list
//             of allowable values.
//
// If bootdev is kBootDevNetwork, then this function will return if
// booting was unsuccessful. This allows the PXE firmware to try the
// next bootable device on its list. If bootdev is not kBootDevNetwork,
// this function will not return control back to the caller.

void
boot(int bootdev)
{
    register KERNBOOTSTRUCT * kbp = kernBootStruct;
    int      fd;

    zeroBSS();

    // Enable A20 gate before accessing memory above 1Mb.

    enableA20();

    // Remember the device that the booter was loaded from.

    gBootDev = bootdev;

    // Initialize boot info structure.

    initKernBootStruct();

    // Setup VGA text mode.

    setMode(TEXT_MODE);

    // Scan hardware configuration.

    scanHardware();

    // Display boot prompt.

    printf( bootPrompt, kbp->convmem, kbp->extmem, kBootTimeout );

    // Parse args, load and start kernel.

    while (1)
    {
        // Initialize globals.

        sysConfigValid = 0;
        errors         = 0;

        // Make sure we are in VGA text mode.

        setMode(TEXT_MODE);

        // Set up kbp->kernDev to reflect the boot device.

        if ( bootdev == kBootDevHardDisk )
        {
            if (kbp->numIDEs > 0)
            {
                kbp->kernDev = DEV_HD;
            }
            else
            {
                kbp->kernDev = DEV_SD;
            }
        }
        else if ( bootdev == kBootDevFloppyDisk )
        {
            kbp->kernDev = DEV_FLOPPY;
        }
        else
        {
            kbp->kernDev = DEV_EN;
        }
        flushdev();

        // Display boot prompt and get user supplied boot string.

        getBootString();

        if ( bootdev != kBootDevNetwork )
        {
            // To force loading config file off same device as kernel,
            // open kernel file to force device change if necessary.

            fd = open(kbp->bootFile, 0);
            if (fd >= 0) close(fd);
        }

        if ( sysConfigValid == 0 )
        {
            if (kbp->kernDev == DEV_EN)
                break;      // return control back to PXE
            else
                continue;   // keep looping
        }

        // Found and loaded a config file. Proceed with boot.

        gWantBootGraphics = getBoolForKey( kBootGraphicsKey );
        gSilentBoot       = getBoolForKey( kQuietBootKey );

        message("Loading Darwin/x86", 0);

        if ( (fd = openfile(kbp->bootFile, 0)) >= 0 )
        {
            execKernel(fd);  // will not return on success
        }
        else
        {
            error("Can't find %s\n", kbp->bootFile);

            if ( bootdev == kBootDevFloppyDisk )
            {
                // floppy in drive, but failed to load kernel.
                bootdev = kBootDevHardDisk;
                message("Couldn't start up the computer using this "
                        "floppy disk.", 0);
            }
            else if ( bootdev == kBootDevNetwork )
            {
                break;   // Return control back to PXE.
            }
        }
    } /* while(1) */
}

//==========================================================================
// Skip spaces/tabs characters.

static inline void
skipblanks(char ** cp) 
{
    while ( **(cp) == ' ' || **(cp) == '\t' )
        ++(*cp);
}

//==========================================================================
// Load the help file and display the file contents on the screen.

static void showHelp()
{
#define BOOT_DIR_DISK       "/usr/standalone/i386/"
#define BOOT_DIR_NET        ""
#define makeFilePath(x) \
    (gBootDev == kBootDevNetwork) ? BOOT_DIR_NET x : BOOT_DIR_DISK x

    int    fd;
    char * help = makeFilePath("BootHelp.txt");

    if ( (fd = open(help, 0)) >= 0 )
    {
        char * buffer = malloc( file_size(fd) );
        read(fd, buffer, file_size(fd) - 1);
        close(fd);
        printf("%s", buffer);
        free(buffer);
    }
}

//==========================================================================
// Returns 1 if the string pointed by 'cp' contains an user-specified
// kernel image file name. Used by getBootString() function.

static int
containsKernelName(const char * cp)
{
    register char c;

    skipblanks(&cp);

    // Convert char to lower case.

    c = *cp | 0x20;

    // Must start with a letter or a '/'.

    if ( (c < 'a' || c > 'z') && ( c != '/' ) )
        return 0;

    // Keep consuming characters until we hit a separator.

    while ( *cp && (*cp != '=') && (*cp != ' ') && (*cp != '\t') )
        cp++;

    // Only SPACE or TAB separator is accepted.
    // Reject everything else.

    if (*cp == '=')
        return 0;

    return 1;
}

//==========================================================================
// Display the "boot:" prompt and copies the user supplied string to
// kernBootStruct->bootString. The kernel image file name is written
// to kernBootStruct->bootFile.

static void
getBootString()
{
    char         line[BOOT_STRING_LEN];
    char *       cp;
    char *       val;
    int          count;
    static int   timeout = kBootTimeout;

    do {
        line[0] = '\0';
        cp      = &line[0];

        // If there were errors, don't timeout on boot prompt since
        // the same error is likely to occur again.

        if ( errors ) timeout = 0;
        errors = 0;

        // Print the boot prompt and wait a few seconds for user input.

        printf("\n");
        count = Gets(line, sizeof(line), timeout, "boot: ", "");
        flushdev();

        // If something was typed, don't use automatic boot again.
        // The boot: prompt will not timeout and go away until
        // the user hits the return key.

        if ( count ) timeout = 0;

        skipblanks(&cp);

        // If user typed '?', then display the usage message.

        if ( *cp == '?' )
        {
            showHelp();
            continue;
        }

        // Load config table file specified by the user, or fallback
        // to the default one.

        val = 0;
        getValueForBootKey(cp, "config", &val, &count);
        loadSystemConfig(val, count);
        if ( !sysConfigValid )
            continue;
    }
    while ( 0 );

    // Did the user specify a kernel file name at the boot prompt?

    if ( containsKernelName(cp) == 0 )
    {
        // User did not type a kernel image file name on the boot prompt.
        // This is fine, read the default kernel file name from the
        // config table.

        if ( getValueForKey(kKernelNameKey, &val, &count) )
        {
            strncpy(kernBootStruct->bootFile, val, count);
        }
    }
    else
    {
        // Get the kernel name from the user-supplied boot string,
        // and copy the name to the buffer provided.

        char * namep = kernBootStruct->bootFile;

        while ( *cp && !(*cp == ' ' || *cp == '\t') )
            *namep++ = *cp++;

        *namep = '\0';
    }

    // Verbose flag specified.

    gVerboseMode = getValueForBootKey(cp, "-v", &val, &count);

    // Save the boot string in kernBootStruct->bootString.

    if ( getValueForKey(kKernelFlagsKey, &val, &count) && count )
    {
        strncpy( kernBootStruct->bootString, val, count );
    }
    if ( strlen(cp) )
    {
        strcat(kernBootStruct->bootString, " ");
        strcat(kernBootStruct->bootString, cp);
    }
}
