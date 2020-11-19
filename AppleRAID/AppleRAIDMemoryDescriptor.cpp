/*
 * Copyright (c) 2001-2015 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 *
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#include "AppleRAID.h"

#undef super
#define super IOMemoryDescriptor
OSDefineMetaClassAndAbstractStructors(AppleRAIDMemoryDescriptor, IOMemoryDescriptor);

bool AppleRAIDMemoryDescriptor::initWithStorageRequest(AppleRAIDStorageRequest *storageRequest, UInt32 memberIndex)
{
    if (!super::init()) return false;
    
    mdMemoryDescriptorLock = IOLockAlloc();
    if (mdMemoryDescriptorLock == 0) return false;
    
    mdStorageRequest = storageRequest;
    
    mdMemberIndex = memberIndex;
    return true;
}

void AppleRAIDMemoryDescriptor::free(void)
{
    IOLockFree(mdMemoryDescriptorLock);

    super::free();
}

IOReturn AppleRAIDMemoryDescriptor::prepare(IODirection forDirection)
{
    IOReturn result;
    
    IOLockLock(mdMemoryDescriptorLock);
    result = mdMemoryDescriptor->prepare(forDirection);
    IOLockUnlock(mdMemoryDescriptorLock);
    
    return result;
}

IOReturn AppleRAIDMemoryDescriptor::complete(IODirection forDirection)
{
    IOReturn result;
    
    IOLockLock(mdMemoryDescriptorLock);
    result = mdMemoryDescriptor->complete(forDirection);
    IOLockUnlock(mdMemoryDescriptorLock);
    
    return result;
}

uint64_t AppleRAIDMemoryDescriptor::getPreparationID( void )
{
    if ( mdMemoryDescriptor )
    {
        return mdMemoryDescriptor->getPreparationID();
    }

    return super::getPreparationID();
}
