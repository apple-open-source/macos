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

AppleRAIDGlobals gAppleRAIDGlobals;

AppleRAIDGlobals::AppleRAIDGlobals()
{
    IOLog1("AppleRAIDGlobals() initing\n");
    raidGlobalLock = IORecursiveLockAlloc();
    raidControllerReferences = 0;
}

AppleRAIDGlobals::~AppleRAIDGlobals()
{
    IOLog1("AppleRAIDGlobals::~AppleRAIDGlobals called.\n");

    assert(raidControllerReferences == 0);
    
    if (raidGlobalLock) {
        IORecursiveLockFree(raidGlobalLock);
        raidGlobalLock = 0;
    }
}

void AppleRAIDGlobals::lock(void)
{
    IORecursiveLockLock(raidGlobalLock);
}

void AppleRAIDGlobals::unlock(void)
{
    IORecursiveLockUnlock(raidGlobalLock);
}

bool AppleRAIDGlobals::islocked(void)
{
    return IORecursiveLockHaveLock(raidGlobalLock);
}

AppleRAID * AppleRAIDGlobals::getController(void)
{
    lock();

    if (!raidController) {

	IOLog1("AppleRAIDGlobals::getController - creating AppleRAID\n");
	assert(raidControllerReferences == 0);

	// XXX - move all this to AppleRAID class, same for releaseController
	raidController = new AppleRAID;
	if (raidController) {
	    
	    raidController->init();

	    const OSSymbol * userClient = OSSymbol::withCStringNoCopy("AppleRAIDUserClient");
	    if (userClient) {
		raidController->setProperty(gIOUserClientClassKey, (OSObject *)userClient);
		userClient->release();
	    }
	    raidController->attach(IOService::getResourceService());
	    raidController->registerService();
	}
    }
    
    if (raidControllerReferences++) raidController->retain();

    unlock();

    return raidController;
}

void AppleRAIDGlobals::releaseController(void)
{
    lock();

    if (raidController) {

	if (--raidControllerReferences == 0) {
	    raidController->detach(IOService::getResourceService());
	    raidController->release();
	    raidController = 0;
	} else {
	    raidController->release();
	}
    }
    
    unlock();
}

