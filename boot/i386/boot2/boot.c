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
#include "drivers.h"
#include "nbp.h"

/*
 * True if using default.table
 */
static BOOL useDefaultConfig;

/*
 * Name of the kernel image file to load.
 * This is specified in the config file, or may be
 * overridden by the user at the boot prompt.
 */
static char gKernelName[BOOT_STRING_LEN];

/*
 * The user asked for boot graphics.
 */
static BOOL gWantBootGraphics = NO;

/*
 * The device that the booter was loaded from.
 */
int gBootDev;

extern char * gFilename;
extern BOOL   sysConfigValid;
extern char   bootPrompt[];    // In prompt.c
extern BOOL   errors;
extern BOOL   verbose_mode;
extern BOOL   gSilentBoot;

#if MULTIPLE_DEFAULTS
char * default_names[] = {
    "$LBL",
};
#define NUM_DEFAULT_NAMES   (sizeof(default_names)/sizeof(char *))
int current_default = 0;
#else
#define DEFAULT_NAME    "$LBL"
#endif

/*
 * Prototypes.
 */
static void getBootString();

/*
 * Message/Error logging macros.
 */
#define PRINT(x)        { printf x }

#ifdef  DEBUG
#define DPRINT(x)       { printf x; }
#define DSPRINT(x)      { printf x; sleep(2); }
#else
#define DPRINT(x)
#define DSPRINT(x)
#endif

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
// execKernel - Load the kernel image file and jump to its entry point.

static int
execKernel(int fd, int installMode)
{
    register KERNBOOTSTRUCT * kbp = kernBootStruct;
    register char *           src = gFilename;
    register char *           dst = kbp->boot_file;
    char *                    val;
    static struct mach_header head;
    entry_t                   kernelEntry;
    int                       ret, size;
#ifdef DISABLED
    char *                    linkerPath;
    int                       loadDrivers;
#endif

    /* Copy the space/tab delimited word pointed by src (gFilename) to
     * kbp->boot_file.
     */ 
    while (*src && (*src != ' ' && *src != '\t'))
        *dst++ = *src++;
    *dst = 0;

    verbose("Loading %s\n", kbp->boot_file);

    /* perform the actual load */
    kbp->kaddr = kbp->ksize = 0;
    ret = loadprog(kbp->kernDev,
                   fd,
                   &head,
                   &kernelEntry,
                   (char **) &kbp->kaddr,
                   &kbp->ksize);
    close(fd);

    if ( ret != 0 )
        return ret;

    /* Clear memory that might be used for loaded drivers
     * because the standalone linker doesn't zero
     * memory that is used later for BSS in the drivers.
     */
    {
        long addr = kbp->kaddr + kbp->ksize;
        bzero((char *)addr, RLD_MEM_ADDR - addr);
    }

    clearActivityIndicator();
    printf("\n");

    if ((getValueForKey("Kernel Flags", &val, &size)) && size) {
        int oldlen, len1;
        char * cp = kbp->bootString;
        oldlen = len1 = strlen(cp);

        // move out the user string
        for(; len1 >= 0; len1--)
            cp[size + len1] = cp[len1 - 1];
        strncpy(cp,val,size);
        if (oldlen) cp[strlen(cp)] = ' ';
    }

    if (errors) {
        printf("Errors encountered while starting up the computer.\n");
        printf("Pausing %d seconds...\n", BOOT_TIMEOUT);
        sleep(BOOT_TIMEOUT);
    }

    message("Starting Darwin Intel", 0);
    
    if (kbp->eisaConfigFunctions)
        kbp->first_addr0 = EISA_CONFIG_ADDR +
            (kbp->eisaConfigFunctions * sizeof(EISA_func_info_t));

    clearActivityIndicator();

    turnOffFloppy();

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
//

static void scanHardware()
{
extern int  ReadPCIBusInfo(PCI_bus_info_t *pp);
extern void PCI_Bus_Init(PCI_bus_info_t *);
    
    KERNBOOTSTRUCT * kbp = KERNSTRUCT_ADDR;

    ReadPCIBusInfo( &kbp->pciInfo );
    PCI_Bus_Init( &kbp->pciInfo );
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
    int      fd, size;
    char *   val;
    int      installMode = 0;

    zeroBSS();

    // Enable A20 gate to be able to access memory above 1 MB.

    enableA20();

    // Remember the device that the booter was loaded from.

    gBootDev = bootdev;

    // Initialize boot info structure.

    initKernBootStruct();

    // Setup VGA text mode.

    setMode(TEXT_MODE);

    // Initialize the malloc area to the top of conventional memory.

    malloc_init( (char *) ZALLOC_ADDR,
                 (kbp->convmem * 1024) - ZALLOC_ADDR,
                 ZALLOC_NODES );

    // Scan hardware configuration.

    scanHardware();

    // Display initial banner.

    printf( bootPrompt, kbp->convmem, kbp->extmem );
    printf( "Darwin Intel will start up in %d seconds, or you can:\n"
            "  Type -v and press Return to start up Darwin Intel with "
              "diagnostic messages\n"
            "  Type ? and press Return to learn about advanced startup "
              "options\n"
            "  Type any other character to stop Darwin Intel from "
              "starting up automatically\n",
            BOOT_TIMEOUT );

    // Parse args, load and start kernel.

    while (1)
    {
        // Initialize globals.

        sysConfigValid   = 0;
        useDefaultConfig = 0;
        errors           = 0;

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

#if 0   // XXX - $LBL
#if MULTIPLE_DEFAULTS
        strcpy(gKernelName, default_names[current_default]);
        if (++current_default == NUM_DEFAULT_NAMES)
            current_default = 0;
#else
        strcpy(gKernelName, DEFAULT_NAME);
#endif
#endif

        // Display boot prompt and get user supplied boot string.

        getBootString();

        if ( bootdev != kBootDevNetwork )
        {
            // To force loading config file off same device as kernel,
            // open kernel file to force device change if necessary.

            fd = open(gKernelName, 0);
            if (fd >= 0)
                close(fd);
        }

        if ( sysConfigValid == 0 )
        {
            val = 0;
            getValueForBootKey(kbp->bootString, 
                               "config", &val, &size);

            DSPRINT(("sys config was not valid trying alt\n"));
            useDefaultConfig = loadSystemConfig(val, size);
            
            if ( sysConfigValid == 0 )
            {
                DSPRINT(("sys config is not valid\n"));
                if (kbp->kernDev == DEV_EN)
                    break;      // return control back to PXE
                else
                    continue;   // keep looping
            }
        }

        // Found and loaded a config file. Proceed with boot.

        gWantBootGraphics = getBoolForKey("Boot Graphics");
        gSilentBoot       = getBoolForKey("Silent Boot");

        message("Loading Darwin Intel", 0);

        if ( (fd = openfile(gKernelName, 0)) >= 0 )
        {
            DSPRINT(("calling exec kernel\n"));
            execKernel(fd, installMode);

            // If execKernel() returns, kernel load failed.
        }
        else
        {
            error("Can't find %s\n", gKernelName);

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

static inline int
containsKernelName(char * cp)
{
    register char c;
    
    skipblanks(&cp);

    // Convert everything to lower case.

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
// to the gKernelName buffer.

static void
getBootString()
{
    char         line[BOOT_STRING_LEN];
    char *       cp;
    char *       val;
    int          count;
    static int   timeout = BOOT_TIMEOUT;

top:
    line[0] = '\0';
    cp      = &line[0];

    /* If there have been problems, don't go on. */
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
        goto top;
    }

    // Did the user specify a kernel file name at the boot prompt?

    if ( containsKernelName(cp) == 0 )
    {
        // User did not type a kernel image file name on the boot prompt.
        // This is fine, read the default kernel file name from the
        // config table.

        printf("\n");

        val = 0;
        getValueForBootKey(cp, "config", &val, &count);

        useDefaultConfig = loadSystemConfig(val, count);

        if ( !sysConfigValid )
            goto top;

        // Get the kernel name from the config table file.

        if ( getValueForKey( "Kernel", &val, &count) )
        {
            strncpy(gKernelName, val, count);
        }
    }
    else
    {
        // Get the kernel name from the user-supplied boot string,
        // and copy the name to the buffer provided.

        char * namep = gKernelName;

        while ( *cp && !(*cp == ' ' || *cp == '\t') )
            *namep++ = *cp++;

        *namep = '\0';
    }

    // Verbose flag specified.

    verbose_mode = getValueForBootKey(cp, "-v", &val, &count);

    // Save the boot string in kernBootStruct.

    strcpy(kernBootStruct->bootString, cp);
}
