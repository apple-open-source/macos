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


#ifndef _APPLERAIDGLOBALS_H
#define _APPLERAIDGLOBALS_H

#include "AppleRAID.h"
#include "AppleRAIDController.h"

class AppleRAIDGlobals
{
private:
    OSDictionary	*_raidSets;
    IOLock		*_raidSetsLock;
    AppleRAIDController	*_raidController;
    
public:
    AppleRAIDGlobals();
    ~AppleRAIDGlobals();
    bool isValid(void);
    void lock(void);
    void unlock(void);
    AppleRAID *getAppleRAIDSet(const OSSymbol *raidSetName);
    void setAppleRAIDSet(const OSSymbol *raidSetName, AppleRAID *appleRAID);
    void removeAppleRAIDSet(const OSSymbol *raidSetName);
    AppleRAIDController *getAppleRAIDController(void);
    void removeAppleRAIDController(void);
};

extern AppleRAIDGlobals gAppleRAIDGlobals;

#endif /* ! _APPLERAIDGLOBALS_H */
