/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
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

#include "sl.h"
#include "saio_internal.h"
#include "bootstruct.h"

static long  gImageLastKernelAddr;

#define kPageSize     4096
#define RoundPage(x)  ((((unsigned)(x)) + kPageSize - 1) & ~(kPageSize - 1))


long
AllocateMemoryRange(char * rangeName, long start, long length, long type)
{
    if ( bootArgs->numBootDrivers < NDRIVERS )
    {
        int num = bootArgs->numBootDrivers;

        bootArgs->driverConfig[num].address = start;
        bootArgs->driverConfig[num].size    = length;
        bootArgs->driverConfig[num].type    = type;
        bootArgs->numBootDrivers++;
    }
    else
    {
        stop( "AllocateMemoryRange error" );
    }
    return 0;
}

long
AllocateKernelMemory( long inSize )
{
    long addr;

    if (gImageLastKernelAddr == 0) {
        gImageLastKernelAddr = RoundPage( bootArgs->kaddr +
                                          bootArgs->ksize );
    }
    addr = gImageLastKernelAddr;
    gImageLastKernelAddr += RoundPage(inSize);

    if ( gImageLastKernelAddr >= (KERNEL_ADDR + KERNEL_LEN) ) {
        stop ("AllocateKernelMemory error");
    }
        
    bootArgs->ksize = gImageLastKernelAddr - bootArgs->kaddr;

    return addr;
}
