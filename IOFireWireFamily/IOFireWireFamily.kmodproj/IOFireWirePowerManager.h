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
 
#ifndef _IOKIT_IOFIREWIREPOWERMANAGER_H
#define _IOKIT_IOFIREWIREPOWERMANAGER_H

#include <IOKit/firewire/IOFireWireFamilyCommon.h>

#include <libkern/c++/OSObject.h>
#include <IOKit/IOReturn.h>

class IOFireWireController;

class IOFireWirePowerManager : public OSObject
{
    OSDeclareAbstractStructors(IOFireWirePowerManager);

protected:
    
/*! @struct ExpansionData
    @discussion This structure will be used to expand the capablilties of the class in the future.
    */    
    struct ExpansionData { };

/*! @var reserved
    Reserved for future use.  (Internal use only)  */
    ExpansionData *reserved;

	IOFireWireController *	fControl;
	
	UInt32		fMaximumDeciwatts;
	UInt32		fAllocatedDeciwatts;

public:	
	static IOFireWirePowerManager * createWithController( IOFireWireController * controller );
	
	virtual bool initWithController( IOFireWireController * controller );

	virtual void setMaximumDeciwatts( UInt32 deciwatts );
	virtual IOReturn allocateDeciwatts( UInt32 deciwatts );
	virtual void deallocateDeciwatts( UInt32 deciwatts );
	
private:
    OSMetaClassDeclareReservedUnused(IOFireWirePowerManager, 0);
    OSMetaClassDeclareReservedUnused(IOFireWirePowerManager, 1);
    OSMetaClassDeclareReservedUnused(IOFireWirePowerManager, 2);
    OSMetaClassDeclareReservedUnused(IOFireWirePowerManager, 3);
    OSMetaClassDeclareReservedUnused(IOFireWirePowerManager, 4);
    OSMetaClassDeclareReservedUnused(IOFireWirePowerManager, 5);
    OSMetaClassDeclareReservedUnused(IOFireWirePowerManager, 6);
    OSMetaClassDeclareReservedUnused(IOFireWirePowerManager, 7);
    OSMetaClassDeclareReservedUnused(IOFireWirePowerManager, 8);
    OSMetaClassDeclareReservedUnused(IOFireWirePowerManager, 9);
};

#endif