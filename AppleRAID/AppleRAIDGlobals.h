/*
 * Copyright (c) 2001-2005 Apple Computer, Inc. All rights reserved.
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


#ifndef _APPLERAIDGLOBALS_H
#define _APPLERAIDGLOBALS_H

class AppleRAIDGlobals
{
private:
    IORecursiveLock *     raidGlobalLock;
    AppleRAID *		  raidController;
    UInt32		  raidControllerReferences;
    
public:
    AppleRAIDGlobals();
    ~AppleRAIDGlobals();
    void lock(void);
    void unlock(void);
    bool islocked(void);

    AppleRAID * getController(void);
    void releaseController(void);
    
};

extern AppleRAIDGlobals gAppleRAIDGlobals;

#endif /* ! _APPLERAIDGLOBALS_H */
