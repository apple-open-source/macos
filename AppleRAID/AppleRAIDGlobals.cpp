/*
 * Copyright (c) 2001-2002 Apple Computer, Inc. All rights reserved.
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

#include "AppleRAID.h"
#include "AppleRAIDGlobals.h"


AppleRAIDGlobals gAppleRAIDGlobals;

AppleRAIDGlobals::AppleRAIDGlobals()
{
  _raidSets     = OSDictionary::withCapacity(1);
  _raidSetsLock = IOLockAlloc();
}

AppleRAIDGlobals::~AppleRAIDGlobals()
{
    if (_raidSets) {
        _raidSets->release();
        _raidSets = 0;
    }
    
    if (_raidSetsLock) {
        IOLockFree(_raidSetsLock);
        _raidSetsLock = 0;
    }
}

bool AppleRAIDGlobals::isValid(void)
{
    return _raidSets && _raidSetsLock;
}

void AppleRAIDGlobals::lock(void)
{
    IOLockLock(_raidSetsLock);
}

void AppleRAIDGlobals::unlock(void)
{
    IOLockUnlock(_raidSetsLock);
}

AppleRAID *AppleRAIDGlobals::getAppleRAIDSet(const OSSymbol *raidSetName)
{
    return OSDynamicCast(AppleRAID, _raidSets->getObject(raidSetName));
}

void AppleRAIDGlobals::setAppleRAIDSet(const OSSymbol *raidSetName, AppleRAID *appleRAID)
{
    _raidSets->setObject(raidSetName, appleRAID);
}

void AppleRAIDGlobals::removeAppleRAIDSet(const OSSymbol *raidSetName)
{
    _raidSets->removeObject(raidSetName);
}

AppleRAIDController *AppleRAIDGlobals::getAppleRAIDController(void)
{
    if (_raidController == 0) {
        _raidController = AppleRAIDController::createAppleRAIDController();
        if (_raidController != 0) _raidController->retain();
    }
    
    return _raidController;
}

void AppleRAIDGlobals::removeAppleRAIDController(void)
{
    _raidController->release();
    _raidController = 0;
}
