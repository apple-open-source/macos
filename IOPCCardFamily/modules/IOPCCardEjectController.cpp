/*
 * Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
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

#include <IOKit/pccard/IOPCCard.h>

#undef  super
#define super IOService

OSDefineMetaClassAndStructors(IOPCCardEjectController, IOService);

OSMetaClassDefineReservedUnused(IOPCCardEjectController,  0);
OSMetaClassDefineReservedUnused(IOPCCardEjectController,  1);
OSMetaClassDefineReservedUnused(IOPCCardEjectController,  2);
OSMetaClassDefineReservedUnused(IOPCCardEjectController,  3);
OSMetaClassDefineReservedUnused(IOPCCardEjectController,  4);
OSMetaClassDefineReservedUnused(IOPCCardEjectController,  5);
OSMetaClassDefineReservedUnused(IOPCCardEjectController,  6);
OSMetaClassDefineReservedUnused(IOPCCardEjectController,  7);
OSMetaClassDefineReservedUnused(IOPCCardEjectController,  8);
OSMetaClassDefineReservedUnused(IOPCCardEjectController,  9);
OSMetaClassDefineReservedUnused(IOPCCardEjectController, 10);
OSMetaClassDefineReservedUnused(IOPCCardEjectController, 11);
OSMetaClassDefineReservedUnused(IOPCCardEjectController, 12);
OSMetaClassDefineReservedUnused(IOPCCardEjectController, 13);
OSMetaClassDefineReservedUnused(IOPCCardEjectController, 14);
OSMetaClassDefineReservedUnused(IOPCCardEjectController, 15);

bool
IOPCCardEjectController::start(IOService * provider)
{
    return super::start(provider);
}

void
IOPCCardEjectController::stop(IOService * provider)
{
    super::stop(provider);
}

bool
IOPCCardEjectController::requestCardEjection()
{
    return IOPCCardBridge::requestCardEjection(getProvider()) == 0;
}

IOReturn
IOPCCardEjectController::ejectCard()
{
    return kIOReturnSuccess;
}
