/*
 * Copyright (c) 2001-2002 Apple Computer, Inc. All rights reserved.
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
/*
 *  DRI: Josh de Cesare
 *
 */

#include <IOKit/IOMessage.h>

#include "AppleRAID.h"
#include "AppleRAIDGlobals.h"
#include "AppleRAIDController.h"


#define super IOService
OSDefineMetaClassAndStructors(AppleRAIDController, IOService);

AppleRAIDController *AppleRAIDController::createAppleRAIDController(void)
{
    AppleRAIDController *raidController;
    
    raidController = new AppleRAIDController;
    if ((raidController != 0) && raidController->init()) {
        raidController->attach(getResourceService());
        raidController->registerService();
    }
    
    return raidController;
}

bool AppleRAIDController::attachToChild(IORegistryEntry *child, const IORegistryPlane *plane)
{
    bool ok = super::attachToChild(child, plane);
    
    if (ok) messageClients(kIOMessageServiceIsSuspended);
    
    return ok;
}

void AppleRAIDController::detachFromChild(IORegistryEntry *child, const IORegistryPlane *plane)
{
    super::detachFromChild(child, plane);
    
    messageClients(kIOMessageServiceIsSuspended);
}

void AppleRAIDController::statusChanged(IORegistryEntry *child)
{
    messageClients(kIOMessageServiceIsSuspended);
}
