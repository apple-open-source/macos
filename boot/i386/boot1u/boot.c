/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
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

#include <libsa.h>
#include <libsa/memory.h>
#include <saio_types.h>
#include <saio_internal.h>

#include <fdisk.h>
#include <ufs.h>

#include "boot1u.h"
#include "disk.h"

#include <io_inline.h>

void *gFSLoadAddress;

//#define BOOT_FILE "/foo"
#define BOOT_FILE "/usr/standalone/i386/boot"


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

struct BootVolume bv;

extern char chainbootdev;

void boot(int biosdev, void *partPtr)
{
    struct fdisk_part part;
    int cc;

    zeroBSS();

    //    printf("Hello, world.\n");

    // Enable A20 gate before accessing memory above 1Mb.
    enableA20();

    biosdev = biosdev & 0xFF;
    chainbootdev = biosdev;

    /* Don't believe the passed-in partition table */
    cc = findUFSPartition(biosdev, &part);
    if (cc<0) {
	printf("No UFS partition\n");
	halt();
    }

    initUFSBVRef(&bv, biosdev, &part);

    gFSLoadAddress = (void *)BOOT2_ADDR;
    cc = UFSLoadFile(&bv, BOOT_FILE);
    if (cc < 0) {
	printf("Could not load" BOOT_FILE "\n");
	halt();
    }
    // Return to execute booter
}
