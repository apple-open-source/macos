/*
 * Copyright (c) 1998-2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
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
 
// system
#import <libkern/c++/OSIterator.h>
#import <libkern/c++/OSSet.h>
#import <IOKit/IOLib.h>

class IOConfigDirectory;

class IOConfigDirectoryIterator : public OSIterator
{
    OSDeclareDefaultStructors(IOConfigDirectoryIterator)

protected:
    OSSet *	fDirectorySet;
    OSIterator * fDirectoryIterator;
	
    virtual void free();

public:
    virtual IOReturn init(IOConfigDirectory *owner, UInt32 testVal, UInt32 testMask);
    
    virtual void reset();

    virtual bool isValid();

    virtual OSObject *getNextObject();
};
