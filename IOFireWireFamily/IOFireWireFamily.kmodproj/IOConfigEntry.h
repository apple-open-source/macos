/*
 * Copyright (c) 1998-2002 Apple Computer, Inc. All rights reserved.
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
 
#include <libkern/c++/OSObject.h>
#include <IOKit/IOReturn.h>

#include <IOKit/firewire/IOFireWireFamilyCommon.h>

class IOConfigEntry : public OSObject
{
    OSDeclareDefaultStructors(IOConfigEntry);
	
protected:
    virtual void free();
    
public:
    UInt32 fKey;
    IOConfigKeyType fType;
    OSObject *fData;
    FWAddress fAddr;
    UInt32 fValue;

    unsigned int totalSize();

    static IOConfigEntry* create(UInt32 key, IOConfigKeyType type, OSObject *obj);
    static IOConfigEntry* create(UInt32 key, UInt32 value);
    static IOConfigEntry* create(UInt32 key, FWAddress address);
};